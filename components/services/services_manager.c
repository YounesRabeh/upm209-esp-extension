#include "services_manager.h"

#include "internet.h"
#include "time_service.h"
#include "modbus_manager.h"
#include "memory_manager.h"
#include "ump209.h"
#include "logging.h"
#include "sdkconfig.h"

#define TAG "SERVICES"

esp_err_t services_manager_start(void)
{
    esp_err_t first_err = ESP_OK;

#if CONFIG_INTERNET_SERVICE_ENABLE
    esp_err_t err = internet_init();
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "Failed to initialize internet service: 0x%x", err);
        first_err = err;
    } else {
        err = internet_connect();
        if (err != ESP_OK) {
            LOG_WARNING(TAG, "Internet connection failed: 0x%x (continuing offline)", err);
            if (first_err == ESP_OK) {
                first_err = err;
            }
        } else {
            LOG_OK(TAG, "Internet connected successfully");
        }
    }
#else
    LOG_INFO(TAG, "Internet service disabled by config");
#endif

    esp_err_t post_err = services_manager_start_post_network();
    if (post_err != ESP_OK && first_err == ESP_OK) {
        first_err = post_err;
    }

    return first_err;
}

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

    #if CONFIG_STORAGE_SERVICE_ENABLE
    esp_err_t memory_err = memory_manager_start();
    if (memory_err != ESP_OK) {
        LOG_ERROR(TAG, "Failed to start Memory manager: 0x%x", memory_err);
        if (first_err == ESP_OK) {
            first_err = memory_err;
        }
    }
    #else
    LOG_INFO(TAG, "Storage service disabled by config");
    #endif

#if CONFIG_MODBUS_SERVICE_ENABLE && CONFIG_MODBUS_MANAGER_ENABLE
    if (!modbus_manager_is_running()) {
        esp_err_t target_set_err = modbus_manager_set_target_register_set(ump209_get_target_register_set());
        if (target_set_err != ESP_OK) {
            LOG_WARNING(
                TAG,
                "Failed to set UMP209 target register set: 0x%x (fallback to configured window)",
                target_set_err
            );
            if (first_err == ESP_OK) {
                first_err = target_set_err;
            }
        }
    }

    esp_err_t modbus_err = modbus_manager_start();
    if (modbus_err != ESP_OK) {
        LOG_ERROR(TAG, "Failed to start Modbus manager: 0x%x", modbus_err);
        if (first_err == ESP_OK) {
            first_err = modbus_err;
        }
    }
#else
    LOG_INFO(TAG, "Modbus service or manager disabled by config");
#endif

    LOG_INFO(TAG, "Post-network services started");
    return first_err;
}
