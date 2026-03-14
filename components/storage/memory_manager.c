#include "memory_manager.h"

#include <inttypes.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "logging.h"
#include "memory.h"
#include "sdkconfig.h"

#define TAG "MEMORY_MGR"

#if defined(CONFIG_MEMORY_MANAGER_ENABLE) && CONFIG_MEMORY_MANAGER_ENABLE

typedef struct {
    uint8_t slave_addr;
    uint16_t start_reg;
    uint16_t reg_count;
    uint32_t timestamp_s;
    uint16_t registers[CONFIG_MEMORY_MAX_REGISTERS];
} memory_ingest_item_t;

static TaskHandle_t s_monitor_task_handle = NULL;
static TaskHandle_t s_writer_task_handle = NULL;
static QueueHandle_t s_ingest_queue = NULL;
static bool s_started = false;

static void memory_writer_task(void *arg)
{
    (void)arg;

    memory_ingest_item_t *item = pvPortMalloc(sizeof(*item));
    if (item == NULL) {
        LOG_ERROR(TAG, "memory_writer: failed to allocate ingest item buffer");
        vTaskDelete(NULL);
        return;
    }

    memset(item, 0, sizeof(*item));
    while (true) {
        if (xQueueReceive(s_ingest_queue, item, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        esp_err_t err = memory_enqueue_modbus_sample(
            item->slave_addr,
            item->start_reg,
            item->registers,
            item->reg_count,
            item->timestamp_s
        );
        if (err != ESP_OK) {
            LOG_ERROR(
                TAG,
                "Flash persist failed: slave=%u start=0x%04X count=%u err=0x%x",
                (unsigned)item->slave_addr,
                (unsigned)item->start_reg,
                (unsigned)item->reg_count,
                err
            );
        }
    }
}

static void memory_monitor_task(void *arg)
{
    (void)arg;
    const TickType_t period_ticks = pdMS_TO_TICKS(CONFIG_MEMORY_MANAGER_PERIOD_MS);

    while (true) {
        const UBaseType_t ingest_pending = (s_ingest_queue != NULL) ? uxQueueMessagesWaiting(s_ingest_queue) : 0U;
        LOG_DEBUG(
            TAG,
            "Queue status: ingest_pending=%u persisted=%" PRIu32 " used=%" PRIu32 "B",
            (unsigned)ingest_pending,
            memory_pending_samples(),
            memory_used_bytes()
        );
        vTaskDelay(period_ticks);
    }
}

esp_err_t memory_manager_start(void)
{
    if (s_started) {
        return ESP_OK;
    }

    esp_err_t err = memory_init();
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "memory_init failed: 0x%x", err);
        return err;
    }

    // Dev mode behavior: always start from a clean queue after reset/boot.
    err = memory_clear();
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "memory_clear failed: 0x%x", err);
        return err;
    }
    LOG_INFO(TAG, "Persisted queue cleared on startup (dev mode)");

    s_ingest_queue = xQueueCreate(
        CONFIG_MEMORY_MANAGER_INGEST_QUEUE_LEN,
        sizeof(memory_ingest_item_t)
    );
    if (s_ingest_queue == NULL) {
        LOG_ERROR(TAG, "Failed to create ingest queue");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ok = xTaskCreate(
        memory_writer_task,
        "memory_writer",
        CONFIG_MEMORY_MANAGER_WRITER_TASK_STACK_SIZE,
        NULL,
        CONFIG_MEMORY_MANAGER_WRITER_TASK_PRIORITY,
        &s_writer_task_handle
    );
    if (ok != pdPASS) {
        LOG_ERROR(TAG, "Failed to create memory writer task");
        return ESP_ERR_NO_MEM;
    }

    ok = xTaskCreate(
        memory_monitor_task,
        "memory_monitor",
        CONFIG_MEMORY_MANAGER_TASK_STACK_SIZE,
        NULL,
        CONFIG_MEMORY_MANAGER_TASK_PRIORITY,
        &s_monitor_task_handle
    );
    if (ok != pdPASS) {
        LOG_ERROR(TAG, "Failed to create memory monitor task");
        return ESP_ERR_NO_MEM;
    }

    s_started = true;
    LOG_OK(
        TAG,
        "Started: ingest_queue=%u writer_prio=%u monitor_period_ms=%u",
        (unsigned)CONFIG_MEMORY_MANAGER_INGEST_QUEUE_LEN,
        (unsigned)CONFIG_MEMORY_MANAGER_WRITER_TASK_PRIORITY,
        (unsigned)CONFIG_MEMORY_MANAGER_PERIOD_MS
    );
    return ESP_OK;
}

esp_err_t memory_manager_enqueue_modbus_sample(
    uint8_t slave_addr,
    uint16_t start_reg,
    const uint16_t *registers,
    uint16_t reg_count,
    uint32_t timestamp_s
)
{
    if (!s_started || s_ingest_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (registers == NULL || reg_count == 0U || reg_count > CONFIG_MEMORY_MAX_REGISTERS) {
        return ESP_ERR_INVALID_ARG;
    }

    memory_ingest_item_t *item = pvPortMalloc(sizeof(*item));
    if (item == NULL) {
        LOG_WARNING(TAG, "Ingest allocation failed: count=%u", (unsigned)reg_count);
        return ESP_ERR_NO_MEM;
    }

    *item = (memory_ingest_item_t){
        .slave_addr = slave_addr,
        .start_reg = start_reg,
        .reg_count = reg_count,
        .timestamp_s = timestamp_s,
    };
    memcpy(item->registers, registers, (size_t)reg_count * sizeof(uint16_t));

    if (xQueueSend(s_ingest_queue, item, 0) != pdTRUE) {
        vPortFree(item);
        LOG_WARNING(
            TAG,
            "Ingest queue full: slave=%u start=0x%04X count=%u",
            (unsigned)slave_addr,
            (unsigned)start_reg,
            (unsigned)reg_count
        );
        return ESP_ERR_NO_MEM;
    }

    vPortFree(item);
    return ESP_OK;
}

bool memory_manager_is_running(void)
{
    return s_started;
}

uint32_t memory_manager_ingest_pending(void)
{
    if (!s_started || s_ingest_queue == NULL) {
        return 0U;
    }
    return (uint32_t)uxQueueMessagesWaiting(s_ingest_queue);
}

#else

esp_err_t memory_manager_start(void)
{
    return memory_init();
}

esp_err_t memory_manager_enqueue_modbus_sample(
    uint8_t slave_addr,
    uint16_t start_reg,
    const uint16_t *registers,
    uint16_t reg_count,
    uint32_t timestamp_s
)
{
    return memory_enqueue_modbus_sample(
        slave_addr,
        start_reg,
        registers,
        reg_count,
        timestamp_s
    );
}

bool memory_manager_is_running(void)
{
    return false;
}

uint32_t memory_manager_ingest_pending(void)
{
    return 0U;
}

#endif
