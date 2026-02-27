#include "time_service.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "esp_netif_sntp.h"
#include "sdkconfig.h"
#include "internet.h"
#include "logging.h"

#define TAG "TIME"
#define NTP_SERVER_1 CONFIG_TIME_SERVICE_NTP_SERVER_1
#if CONFIG_LWIP_SNTP_MAX_SERVERS > 1
#define NTP_SERVER_2 CONFIG_TIME_SERVICE_NTP_SERVER_2
#endif

static bool s_time_inited = false;
static bool s_time_synced = false;

esp_err_t time_service_sync_once(const char *tz, int timeout_ms)
{
    esp_err_t err = time_service_init();
    if (err != ESP_OK) return err;

    err = time_service_set_timezone(tz);
    if (err != ESP_OK) return err;

    err = time_service_sync_wait(timeout_ms);
    if (err != ESP_OK) return err;

    struct tm now_tm = {0};
    if (time_service_get_local_time(&now_tm) == ESP_OK) {
        char time_buf[64] = {0};
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S %Z", &now_tm);
        LOG_OK(TAG, "Current local time: %s", time_buf);
    } else {
        LOG_WARNING(TAG, "Time synchronized but local time read failed");
    }
    return ESP_OK;
}

esp_err_t time_service_init(void)
{
    if (s_time_inited) return ESP_OK;
    if (!internet_is_connected()) return ESP_ERR_INVALID_STATE;

#if CONFIG_LWIP_SNTP_MAX_SERVERS > 1
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(
        2,
        ESP_SNTP_SERVER_LIST(NTP_SERVER_1, NTP_SERVER_2)
    );
#else
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER_1);
#endif
    config.start = true;
    config.wait_for_sync = true;
    config.renew_servers_after_new_IP = true;

    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "SNTP init failed: 0x%x", err);
        return err;
    }

    s_time_inited = true;
#if CONFIG_LWIP_SNTP_MAX_SERVERS > 1
    LOG_INFO(TAG, "SNTP started with servers: %s, %s", NTP_SERVER_1, NTP_SERVER_2);
#else
    LOG_INFO(TAG, "SNTP started with server: %s", NTP_SERVER_1);
#endif
    return ESP_OK;
}

esp_err_t time_service_sync_wait(int timeout_ms)
{
    if (!s_time_inited) return ESP_ERR_INVALID_STATE;
    if (timeout_ms <= 0) return ESP_ERR_INVALID_ARG;

    esp_err_t err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(timeout_ms));
    if (err != ESP_OK) {
        LOG_WARNING(TAG, "SNTP sync timeout/error: 0x%x", err);
        return err;
    }

    s_time_synced = true;
    LOG_OK(TAG, "Time synchronized from NTP");
    return ESP_OK;
}

esp_err_t time_service_set_timezone(const char *tz)
{
    if (!tz || strlen(tz) == 0) return ESP_ERR_INVALID_ARG;
    if (setenv("TZ", tz, 1) != 0) return ESP_FAIL;
    tzset();
    return ESP_OK;
}

esp_err_t time_service_get_local_time(struct tm *out_tm)
{
    if (!out_tm) return ESP_ERR_INVALID_ARG;
    if (!s_time_synced) return ESP_ERR_INVALID_STATE;

    time_t now = 0;
    time(&now);
    if (now < 100000) return ESP_ERR_INVALID_STATE;

    if (!localtime_r(&now, out_tm)) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

bool time_service_is_synchronized(void)
{
    return s_time_synced;
}
