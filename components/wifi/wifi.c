#include "wifi.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_eap_client.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "freertos/event_groups.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logging.h"

#define TAG "WIFI"
#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t wifi_event_group;
static esp_netif_t *wifi_netif = NULL;
static bool wifi_initialized = false;

static esp_err_t wifi_configure_enterprise(void) {
#if CONFIG_WIFI_AUTH_WPA2_ENTERPRISE
    const char *username = CONFIG_WIFI_USERNAME;
    const char *password = CONFIG_WIFI_PASSWORD;
    const char *identity = CONFIG_WIFI_EAP_IDENTITY;

    if (strlen(username) == 0) {
        LOG_ERROR(TAG, "WPA2-Enterprise selected but WIFI_USERNAME is empty");
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(password) == 0) {
        LOG_ERROR(TAG, "WPA2-Enterprise selected but WIFI_PASSWORD is empty");
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(identity) == 0) {
        identity = username;
    }

    LOG_INFO(TAG, "Configuring WPA2-Enterprise credentials");
    LOG_DEBUG(TAG, "EAP identity=%s username=%s", identity, username);

    esp_eap_client_clear_identity();
    esp_eap_client_clear_username();
    esp_eap_client_clear_password();

    esp_err_t err = esp_eap_client_set_identity((const unsigned char *)identity, strlen(identity));
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "esp_eap_client_set_identity failed: 0x%x", err);
        return err;
    }
    err = esp_eap_client_set_username((const unsigned char *)username, strlen(username));
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "esp_eap_client_set_username failed: 0x%x", err);
        return err;
    }
    err = esp_eap_client_set_password((const unsigned char *)password, strlen(password));
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "esp_eap_client_set_password failed: 0x%x", err);
        return err;
    }
    err = esp_wifi_sta_enterprise_enable();
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "esp_wifi_sta_enterprise_enable failed: 0x%x", err);
        return err;
    }
    LOG_DEBUG(TAG, "WPA2-Enterprise enabled");
    return ESP_OK;
#else
    esp_err_t err = esp_wifi_sta_enterprise_disable();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        LOG_WARNING(TAG, "esp_wifi_sta_enterprise_disable returned 0x%x", err);
        return err;
    }
    return ESP_OK;
#endif
}

static esp_err_t wifi_target_ssid_found(const uint8_t *ssid, bool *found) {
    if (!ssid || !found || strlen((const char *)ssid) == 0) return ESP_ERR_INVALID_ARG;

    wifi_scan_config_t scan_cfg = {0};
    uint16_t ap_count = 0;

    scan_cfg.ssid = (uint8_t *)ssid;

    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);
    if (err != ESP_OK) {
        LOG_WARNING(TAG, "Scan start failed: 0x%x", err);
        return err;
    }

    err = esp_wifi_scan_get_ap_num(&ap_count);
    if (err != ESP_OK) {
        LOG_WARNING(TAG, "Scan get AP num failed: 0x%x", err);
        return err;
    }

    *found = ap_count > 0;
    LOG_DEBUG(TAG, "Target SSID \"%s\" %s", (const char *)ssid, *found ? "found" : "not found");
    return ESP_OK;
}

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            LOG_DEBUG(TAG, "STA started");
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)event_data;
            xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
            if (disc) {
                LOG_WARNING(TAG, "STA disconnected (reason=%u)", (unsigned)disc->reason);
            } else {
                LOG_WARNING(TAG, "STA disconnected");
            }
        }
    } else if (event_base == IP_EVENT &&
               event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *got_ip = (ip_event_got_ip_t *)event_data;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        if (got_ip) {
            LOG_OK(TAG, "Got IP: " IPSTR, IP2STR(&got_ip->ip_info.ip));
        } else {
            LOG_OK(TAG, "Got IP event");
        }
    }
}

esp_err_t wifi_init(void) {
    if (wifi_initialized) return ESP_OK;

    wifi_event_group = xEventGroupCreate();
    if (!wifi_event_group) return ESP_ERR_NO_MEM;

    wifi_netif = esp_netif_create_default_wifi_sta();
    if (!wifi_netif) return ESP_FAIL;

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    esp_log_level_set("esp_netif_handlers", ESP_LOG_ERROR);
    esp_log_level_set("net80211", ESP_LOG_ERROR);
    esp_log_level_set("pp", ESP_LOG_ERROR);
    esp_log_level_set("phy_init", ESP_LOG_ERROR);

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_initialized = true;
    return ESP_OK;
}

esp_err_t wifi_connect(const wifi_config_t *cfg) {
    if (!wifi_initialized) return ESP_ERR_INVALID_STATE;
    if (!cfg) return ESP_ERR_INVALID_ARG;

    LOG_DEBUG(TAG, "Trying to connect to SSID: %s", (const char *)cfg->sta.ssid);
    esp_err_t err;
    wifi_config_t sta_cfg = *cfg;
    err = esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "esp_wifi_set_config failed: 0x%x", err);
        return err;
    }

    err = esp_wifi_connect();
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "esp_wifi_connect failed: 0x%x", err);
    }
    return err;
}

/**
 * Connect WiFi with retry and timeout
 *
 * @param cfg WiFi configuration
 * @param timeout_ms maximum time to wait for connection
 * @param retry_count number of times to retry
 * @param retry_delay_ms delay between retries in milliseconds
 * @return ESP_OK if connected, else ESP_FAIL
 */
esp_err_t wifi_connect_retry(const wifi_config_t *cfg, int timeout_ms, int retry_count, int retry_delay_ms) {
    esp_err_t err = ESP_FAIL;
    bool ssid_found = false;

    if (!cfg) return ESP_ERR_INVALID_ARG;
    if (retry_delay_ms < 0) return ESP_ERR_INVALID_ARG;

    LOG_DEBUG(TAG, "Retry connect config: timeout_ms=%d retries=%d retry_delay_ms=%d",
              timeout_ms, retry_count, retry_delay_ms);

    err = wifi_target_ssid_found(cfg->sta.ssid, &ssid_found);
    if (err != ESP_OK) {
        LOG_WARNING(TAG, "Target SSID scan failed: 0x%x", err);
        return err;
    }
    if (!ssid_found) {
        LOG_WARNING(TAG, "Target SSID \"%s\" is not visible", (const char *)cfg->sta.ssid);
        return ESP_ERR_NOT_FOUND;
    }

    // Prepare auth environment once before connect attempts.
    err = wifi_configure_enterprise();
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "Auth environment setup failed: 0x%x", err);
        return err;
    }

    for (int i = 0; i < retry_count; i++) {
        LOG_DEBUG(TAG, "WiFi connect attempt %d/%d", i + 1, retry_count);
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);

        // Ensure STA state is clean before starting a new attempt.
        esp_err_t disc_err = esp_wifi_disconnect();
        if (disc_err != ESP_OK && disc_err != ESP_ERR_WIFI_NOT_CONNECT) {
            LOG_DEBUG(TAG, "esp_wifi_disconnect before retry returned 0x%x", disc_err);
        }

        err = wifi_connect(cfg);

        if (err != ESP_OK) {
            LOG_WARNING(TAG, "esp_wifi_connect() returned 0x%x, retrying...", err);
        } else {
            // Wait for connection 'evento'
            int wait_ticks = pdMS_TO_TICKS(timeout_ms);
            int bits = xEventGroupWaitBits(
                wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, wait_ticks
            );
            if (bits & WIFI_CONNECTED_BIT) {
                LOG_DEBUG(TAG, "WiFi connected successfully on attempt %d", i + 1);
                return ESP_OK;
            } else {
                LOG_WARNING(TAG, "WiFi connection timeout on attempt %d", i + 1);
                err = ESP_ERR_TIMEOUT;
                // Abort pending connection to avoid "sta is connecting" on next retry.
                disc_err = esp_wifi_disconnect();
                if (disc_err != ESP_OK && disc_err != ESP_ERR_WIFI_NOT_CONNECT) {
                    LOG_DEBUG(TAG, "esp_wifi_disconnect after timeout returned 0x%x", disc_err);
                }
            }
        }

        if (i + 1 < retry_count && retry_delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
        }
    }

    return err; // failed after all retries
}




esp_err_t wifi_disconnect(void) {
    if (!wifi_initialized) return ESP_ERR_INVALID_STATE;
    return esp_wifi_disconnect();
}

bool wifi_is_connected(void) {
    if (!wifi_initialized) return false;
    return (xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT) != 0;
}
