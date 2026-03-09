#include "memory_manager.h"

#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logging.h"
#include "memory.h"
#include "sdkconfig.h"

#define TAG "MEMORY_MGR"

#if CONFIG_MEMORY_MANAGER_ENABLE

static TaskHandle_t s_task_handle = NULL;
static bool s_started = false;

static void memory_monitor_task(void *arg)
{
    (void)arg;
    const TickType_t period_ticks = pdMS_TO_TICKS(CONFIG_MEMORY_MANAGER_PERIOD_MS);

    while (true) {
        LOG_DEBUG(
            TAG,
            "Queue status: pending=%" PRIu32 " used=%" PRIu32 "B",
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

    BaseType_t ok = xTaskCreate(
        memory_monitor_task,
        "memory_monitor",
        CONFIG_MEMORY_MANAGER_TASK_STACK_SIZE,
        NULL,
        CONFIG_MEMORY_MANAGER_TASK_PRIORITY,
        &s_task_handle
    );
    if (ok != pdPASS) {
        LOG_ERROR(TAG, "Failed to create memory monitor task");
        return ESP_ERR_NO_MEM;
    }

    s_started = true;
    LOG_OK(TAG, "Started: period_ms=%u", CONFIG_MEMORY_MANAGER_PERIOD_MS);
    return ESP_OK;
}

bool memory_manager_is_running(void)
{
    return s_started;
}

#else

esp_err_t memory_manager_start(void)
{
    return memory_init();
}

bool memory_manager_is_running(void)
{
    return false;
}

#endif
