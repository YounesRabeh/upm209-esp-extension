#include "services_manager.h"

#include "time_service.h"
#include "logging.h"
#include "sdkconfig.h"

#define TAG "SERVICES"

esp_err_t services_manager_start_post_network(void)
{
#if CONFIG_TIME_SERVICE_ENABLE
    esp_err_t err = time_service_sync_once(
        CONFIG_TIME_SERVICE_TZ,
        CONFIG_TIME_SERVICE_SYNC_TIMEOUT_MS
    );
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "Failed to sync time service: 0x%x", err);
        return err;
    }
#else
    LOG_INFO(TAG, "Time service disabled by config");
#endif

    LOG_INFO(TAG, "Post-network services started");
    return ESP_OK;
}
