#include "internet.h"
#include "wifi.h"
#include "lte.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include <stdio.h>
#include <string.h>
#include "esp_http_client.h"
#include "logging.h"

#define TAG "INTERNET_SEND"

// Example: send JSON payload to target server
esp_err_t internet_send_data(const char *json_payload) {
    if (!internet_is_connected()) {
        LOG_WARNING(TAG, "Cannot send data: not connected");
        return ESP_FAIL;
    }

    esp_http_client_config_t config = {
        .url = CONFIG_INTERNET_TARGET_URL,
        .method = HTTP_METHOD_POST,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_payload, strlen(json_payload));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        LOG_INFO(TAG, "Data sent successfully: %s", json_payload);
    } else {
        LOG_ERROR(TAG, "Failed to send data: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}