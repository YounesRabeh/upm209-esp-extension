#include "internet.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include <stdio.h>
#include <string.h>
#include "esp_http_client.h"
#include "logging.h"

#define TAG "INTERNET_SEND"

static esp_err_t internet_send_post(const char *json_payload, size_t payload_len)
{
    if (json_payload == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_http_client_config_t config = {
        .url = CONFIG_INTERNET_TARGET_URL,
        .method = HTTP_METHOD_POST,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_payload, (int)payload_len);

    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    return err;
}

// Example: send JSON payload to target server
esp_err_t internet_send_data(const char *json_payload) {
    if (json_payload == NULL) {
        LOG_WARNING(TAG, "Cannot send data: payload is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    size_t payload_len = strlen(json_payload);

    if (!internet_is_connected()) {
        LOG_WARNING(TAG, "Not connected before send; attempting reconnect");
        esp_err_t connect_err = internet_connect();
        if (connect_err != ESP_OK) {
            LOG_ERROR(TAG, "Reconnect before send failed: 0x%x", connect_err);
            return connect_err;
        }
    }

    esp_err_t err = internet_send_post(json_payload, payload_len);
    if (err == ESP_OK) {
        LOG_INFO(TAG, "Data sent successfully (%u bytes)", (unsigned)payload_len);
        return ESP_OK;
    }

    LOG_ERROR(TAG, "Failed to send data (%u bytes): %s", (unsigned)payload_len, esp_err_to_name(err));
    LOG_WARNING(TAG, "Attempting reconnect and single resend");

    esp_err_t reconnect_err = internet_connect();
    if (reconnect_err != ESP_OK) {
        LOG_ERROR(TAG, "Reconnect after send failure failed: 0x%x", reconnect_err);
        return err;
    }

    esp_err_t retry_err = internet_send_post(json_payload, payload_len);
    if (retry_err == ESP_OK) {
        LOG_OK(TAG, "Data sent successfully after reconnect (%u bytes)", (unsigned)payload_len);
        return ESP_OK;
    }

    LOG_ERROR(
        TAG,
        "Retry after reconnect failed (%u bytes): %s",
        (unsigned)payload_len,
        esp_err_to_name(retry_err)
    );
    return retry_err;
}
