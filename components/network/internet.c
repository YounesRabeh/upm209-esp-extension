#include "lte.h"
#include "wifi.h"
#include "internet.h"
#include "nvs_flash.h"
#include "logging.h"
#include "sdkconfig.h"
#include "esp_netif.h"
#include "esp_event.h"
#include <string.h>
#include <stdbool.h>

#define TAG "INTERNET"

#ifndef CONFIG_WIFI_CONNECT_TIMEOUT_MS
#define CONFIG_WIFI_CONNECT_TIMEOUT_MS 5000
#endif

#ifndef CONFIG_WIFI_CONNECT_RETRIES
#define CONFIG_WIFI_CONNECT_RETRIES 3
#endif

#ifndef CONFIG_WIFI_CONNECT_RETRY_DELAY_MS
#define CONFIG_WIFI_CONNECT_RETRY_DELAY_MS 500
#endif

static bool internet_initialized = false;
static internet_if_t active_if = INTERNET_IF_NONE;

static const char *configured_network_preference(void) {
#if CONFIG_INTERNET_NETWORK_WIFI_ONLY
    return "WIFI_ONLY";
#elif CONFIG_INTERNET_NETWORK_LTE_ONLY
    return "LTE_ONLY";
#else
    return "AUTO";
#endif
}

static esp_err_t connect_wifi_from_config(void) {
    wifi_config_t wifi_cfg = {0};
    const int timeout_ms = CONFIG_WIFI_CONNECT_TIMEOUT_MS;
    const int retries = CONFIG_WIFI_CONNECT_RETRIES;
    const int retry_delay_ms = CONFIG_WIFI_CONNECT_RETRY_DELAY_MS;

    if (strlen(CONFIG_WIFI_SSID) == 0) {
        LOG_ERROR(TAG, "WiFi SSID not configured");
        return ESP_ERR_INVALID_ARG;
    }

    strncpy((char *)wifi_cfg.sta.ssid, CONFIG_WIFI_SSID, sizeof(wifi_cfg.sta.ssid) - 1);
#if CONFIG_WIFI_AUTH_WPA2_PSK
    strncpy((char *)wifi_cfg.sta.password, CONFIG_WIFI_PASSWORD, sizeof(wifi_cfg.sta.password) - 1);
#endif

#if CONFIG_WIFI_AUTH_OPEN
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
#elif CONFIG_WIFI_AUTH_WPA2_PSK
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
#elif CONFIG_WIFI_AUTH_WPA2_ENTERPRISE
    // WPA2-Enterprise uses EAP credentials configured in wifi.c. A non-empty
    // placeholder avoids driver warnings about empty password with strict auth threshold.
    strncpy((char *)wifi_cfg.sta.password, "x", sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_ENTERPRISE;
#endif

    LOG_DEBUG(TAG, "WiFi target SSID: %s", (const char *)wifi_cfg.sta.ssid);
    LOG_DEBUG(TAG, "WiFi retries=%d timeout_ms=%d retry_delay_ms=%d", retries, timeout_ms, retry_delay_ms);
    return wifi_connect_retry(&wifi_cfg, timeout_ms, retries, retry_delay_ms);
}



esp_err_t internet_init(void) {
    if (internet_initialized) return ESP_OK;

    // --- Initialize NVS (needed for WiFi) ---
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // --- Initialize network stack ---
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    LOG_DEBUG(TAG, "Configured target URL: %s", CONFIG_INTERNET_TARGET_URL);
    LOG_DEBUG(TAG, "Configured network preference: %s", configured_network_preference());

#if CONFIG_INTERNET_NETWORK_WIFI_ONLY
    ESP_ERROR_CHECK(wifi_init());
#elif CONFIG_INTERNET_NETWORK_LTE_ONLY
    ESP_ERROR_CHECK(lte_init());
#else
    ESP_ERROR_CHECK(wifi_init());
    ESP_ERROR_CHECK(lte_init());
#endif

    internet_initialized = true;
    return ESP_OK;
}


esp_err_t internet_connect(void) {
    esp_err_t err = ESP_FAIL;
    active_if = INTERNET_IF_NONE;

#if CONFIG_INTERNET_NETWORK_WIFI_ONLY
    LOG_INFO(TAG, "Connecting with mode WIFI_ONLY");
    err = connect_wifi_from_config();
    if (err == ESP_OK) {
        active_if = INTERNET_IF_WIFI;
        LOG_OK(TAG, "Connected via WiFi");
    } else {
        LOG_ERROR(TAG, "WiFi connection failed: 0x%x", err);
    }
    return err;
#elif CONFIG_INTERNET_NETWORK_LTE_ONLY
    LOG_INFO(TAG, "Connecting with mode LTE_ONLY");
    err = lte_connect();
    if (err == ESP_OK) {
        active_if = INTERNET_IF_LTE;
        LOG_OK(TAG, "Connected via LTE");
    } else {
        LOG_ERROR(TAG, "LTE connection failed: 0x%x", err);
    }
    return err;
#else
    LOG_INFO(TAG, "Connecting with mode AUTO (WiFi -> LTE fallback)");
    err = connect_wifi_from_config();
    if (err == ESP_OK) {
        active_if = INTERNET_IF_WIFI;
        LOG_OK(TAG, "Connected via WiFi");
        return ESP_OK;
    }
    LOG_WARNING(TAG, "WiFi connection failed (0x%x), trying LTE fallback", err);

    err = lte_connect();
    if (err == ESP_OK) {
        active_if = INTERNET_IF_LTE;
        LOG_OK(TAG, "Connected via LTE");
        return ESP_OK;
    }
    LOG_ERROR(TAG, "LTE fallback failed: 0x%x", err);
    return err;
#endif
}


bool internet_is_connected(void) {
    if (wifi_is_connected()) return true;
    if (lte_is_connected()) return true;
    return false;
}

internet_if_t internet_active_interface(void) {
    return active_if;
}
