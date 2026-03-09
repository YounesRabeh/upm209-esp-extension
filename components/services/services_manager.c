#include "services_manager.h"

#include "time_service.h"
#include "modbus_manager.h"
#include "memory_manager.h"
#include "logging.h"
#include "sdkconfig.h"

#define TAG "SERVICES"

esp_err_t services_manager_start_post_network(void)
{
    esp_err_t first_err = ESP_OK;

#if CONFIG_TIME_SERVICE_ENABLE
    esp_err_t err = time_service_sync_once(
        CONFIG_TIME_SERVICE_TZ,
        CONFIG_TIME_SERVICE_SYNC_TIMEOUT_MS
    );
    if (err != ESP_OK) {
        LOG_WARNING(TAG, "Time sync failed: 0x%x (continuing)", err);
        first_err = err;
    }
#else
    LOG_INFO(TAG, "Time service disabled by config");
#endif

    esp_err_t memory_err = memory_manager_start();
    if (memory_err != ESP_OK) {
        LOG_ERROR(TAG, "Failed to start Memory manager: 0x%x", memory_err);
        if (first_err == ESP_OK) {
            first_err = memory_err;
        }
    }

#if CONFIG_MODBUS_MANAGER_ENABLE
    esp_err_t modbus_err = modbus_manager_start();
    if (modbus_err != ESP_OK) {
        LOG_ERROR(TAG, "Failed to start Modbus manager: 0x%x", modbus_err);
        if (first_err == ESP_OK) {
            first_err = modbus_err;
        }
    }
#else
    LOG_INFO(TAG, "Modbus manager disabled by config");
#endif

    LOG_INFO(TAG, "Post-network services started");
    return first_err;
}
