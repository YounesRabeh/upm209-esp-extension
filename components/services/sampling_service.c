#include "sampling_service.h"

#include <inttypes.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "logging.h"
#include "memory.h"
#include "modbus_manager.h"
#include "sdkconfig.h"

#define TAG "SAMPLE_SVC"

#define SS_QUEUE_LEN               128U
#define SS_WRITER_STACK_SIZE       6144
#define SS_WRITER_PRIORITY         5
#define SS_MONITOR_STACK_SIZE      3072
#define SS_MONITOR_PRIORITY        4
#define SS_MONITOR_PERIOD_MS       5000U
#define SS_PERSIST_RETRY_DELAY_MS  500U
#define SS_STARTUP_CLEAR_PERSISTED 0

typedef struct {
    uint8_t slave_addr;
    uint16_t start_reg;
    uint16_t reg_count;
    uint32_t timestamp_s;
    uint16_t registers[];
} sampling_item_t;

static TaskHandle_t s_writer_task_handle = NULL;
static TaskHandle_t s_monitor_task_handle = NULL;
static QueueHandle_t s_queue = NULL;
static bool s_started = false;

static volatile uint32_t s_queued_total = 0U;
static volatile uint32_t s_persisted_total = 0U;
static volatile uint32_t s_dropped_oldest_total = 0U;
static volatile uint32_t s_persist_retry_total = 0U;

static sampling_item_t *sampling_item_create(
    uint8_t slave_addr,
    uint16_t start_reg,
    const uint16_t *registers,
    uint16_t reg_count,
    uint32_t timestamp_s
)
{
    size_t payload_bytes = (size_t)reg_count * sizeof(uint16_t);
    size_t total_bytes = sizeof(sampling_item_t) + payload_bytes;

    sampling_item_t *item = pvPortMalloc(total_bytes);
    if (item == NULL) {
        return NULL;
    }

    item->slave_addr = slave_addr;
    item->start_reg = start_reg;
    item->reg_count = reg_count;
    item->timestamp_s = timestamp_s;
    memcpy(item->registers, registers, payload_bytes);
    return item;
}

static esp_err_t sampling_service_sink(
    uint8_t slave_addr,
    uint16_t start_reg,
    const uint16_t *registers,
    uint16_t reg_count,
    uint32_t timestamp_s,
    void *ctx
)
{
    (void)ctx;

    if (!s_started || s_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (registers == NULL || reg_count == 0U || reg_count > CONFIG_MEMORY_MAX_REGISTERS) {
        return ESP_ERR_INVALID_ARG;
    }

    sampling_item_t *item = sampling_item_create(
        slave_addr,
        start_reg,
        registers,
        reg_count,
        timestamp_s
    );
    if (item == NULL) {
        LOG_WARNING(TAG, "Queue item allocation failed: reg_count=%u", (unsigned)reg_count);
        return ESP_ERR_NO_MEM;
    }

    if (xQueueSend(s_queue, &item, 0) == pdTRUE) {
        ++s_queued_total;
        return ESP_OK;
    }

    sampling_item_t *oldest = NULL;
    if (xQueueReceive(s_queue, &oldest, 0) == pdTRUE) {
        if (oldest != NULL) {
            vPortFree(oldest);
        }
        ++s_dropped_oldest_total;
    }

    if (xQueueSend(s_queue, &item, 0) != pdTRUE) {
        vPortFree(item);
        LOG_WARNING(TAG, "Queue saturated after drop-oldest; sample lost");
        return ESP_ERR_NO_MEM;
    }

    ++s_queued_total;
    return ESP_OK;
}

static void sampling_writer_task(void *arg)
{
    (void)arg;
    const TickType_t retry_ticks = pdMS_TO_TICKS(SS_PERSIST_RETRY_DELAY_MS);

    while (true) {
        sampling_item_t *item = NULL;
        if (xQueueReceive(s_queue, &item, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (item == NULL) {
            continue;
        }

        uint32_t attempts = 0U;
        while (true) {
            esp_err_t err = memory_enqueue_modbus_sample(
                item->slave_addr,
                item->start_reg,
                item->registers,
                item->reg_count,
                item->timestamp_s
            );
            if (err == ESP_OK) {
                ++s_persisted_total;
                vPortFree(item);
                break;
            }

            ++attempts;
            ++s_persist_retry_total;
            if ((attempts % 10U) == 1U) {
                LOG_WARNING(
                    TAG,
                    "Persist retry: start=0x%04X count=%u attempt=%" PRIu32 " err=0x%x",
                    (unsigned)item->start_reg,
                    (unsigned)item->reg_count,
                    attempts,
                    err
                );
            }
            vTaskDelay(retry_ticks);
        }
    }
}

static void sampling_monitor_task(void *arg)
{
    (void)arg;
    TickType_t period_ticks = pdMS_TO_TICKS(SS_MONITOR_PERIOD_MS);
    if (period_ticks == 0) {
        period_ticks = 1;
    }

    while (true) {
        UBaseType_t pending = (s_queue != NULL) ? uxQueueMessagesWaiting(s_queue) : 0U;
        LOG_DEBUG(
            TAG,
            "Queue status: pending=%u queued=%" PRIu32 " persisted=%" PRIu32 " dropped_oldest=%" PRIu32 " retries=%" PRIu32 " flash_pending=%" PRIu32 " used=%" PRIu32 "B",
            (unsigned)pending,
            s_queued_total,
            s_persisted_total,
            s_dropped_oldest_total,
            s_persist_retry_total,
            memory_pending_samples(),
            memory_used_bytes()
        );
        vTaskDelay(period_ticks);
    }
}

esp_err_t sampling_service_start(void)
{
    if (s_started) {
        return ESP_OK;
    }

    s_queued_total = 0U;
    s_persisted_total = 0U;
    s_dropped_oldest_total = 0U;
    s_persist_retry_total = 0U;

    esp_err_t err = memory_init();
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "memory_init failed: 0x%x", err);
        return err;
    }

#if SS_STARTUP_CLEAR_PERSISTED
    err = memory_clear();
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "memory_clear failed: 0x%x", err);
        return err;
    }
    LOG_INFO(TAG, "Persisted queue cleared on startup (dev mode)");
#endif

    s_queue = xQueueCreate(SS_QUEUE_LEN, sizeof(sampling_item_t *));
    if (s_queue == NULL) {
        LOG_ERROR(TAG, "Failed to create sampling queue");
        return ESP_ERR_NO_MEM;
    }

    err = modbus_manager_set_sample_sink(sampling_service_sink, NULL);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "Failed to register sample sink: 0x%x", err);
        vQueueDelete(s_queue);
        s_queue = NULL;
        return err;
    }

    BaseType_t ok = xTaskCreate(
        sampling_writer_task,
        "sampling_writer",
        SS_WRITER_STACK_SIZE,
        NULL,
        SS_WRITER_PRIORITY,
        &s_writer_task_handle
    );
    if (ok != pdPASS) {
        LOG_ERROR(TAG, "Failed to create sampling writer task");
        modbus_manager_set_sample_sink(NULL, NULL);
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    ok = xTaskCreate(
        sampling_monitor_task,
        "sampling_monitor",
        SS_MONITOR_STACK_SIZE,
        NULL,
        SS_MONITOR_PRIORITY,
        &s_monitor_task_handle
    );
    if (ok != pdPASS) {
        s_monitor_task_handle = NULL;
        LOG_WARNING(TAG, "Failed to create sampling monitor task (continuing without monitor)");
    }

    s_started = true;
    LOG_OK(
        TAG,
        "Started: queue_len=%u writer_prio=%u retry_ms=%u",
        (unsigned)SS_QUEUE_LEN,
        (unsigned)SS_WRITER_PRIORITY,
        (unsigned)SS_PERSIST_RETRY_DELAY_MS
    );
    return ESP_OK;
}

bool sampling_service_is_running(void)
{
    return s_started;
}
