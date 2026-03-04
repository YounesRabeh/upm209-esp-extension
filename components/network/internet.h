#pragma once
#include <stdbool.h>
#include "esp_err.h"

/** 
 * @brief Defines the available network interfaces for internet connectivity 
 */
typedef enum {
    INTERNET_IF_NONE = -1,
    INTERNET_IF_WIFI,
    INTERNET_IF_LTE
} internet_if_t;

/**
 * @brief Initializes the internet module, including WiFi and LTE components as configured.
 * This must be called before any other internet functions.
 */
esp_err_t internet_init(void);

/**
 * @brief Attempts to connect to the internet using the configured network preference.
 * In AUTO mode, it will try WiFi first and fallback to LTE if WiFi fails.
 * @return ESP_OK on successful connection, or an error code on failure.
 */
esp_err_t internet_connect(void);


/**
 * @return true if the internet is connected, false otherwise
 */
bool internet_is_connected(void);

/**
 * @return the currently active network interface used for internet connectivity.
 * INTERNET_IF_WIFI, INTERNET_IF_LTE, o INTERNET_IF_NONE se non connesso
 */
internet_if_t internet_active_interface(void);

/**
 * @brief Sends a JSON payload through the active internet connection.
 * @param json_payload Null-terminated JSON payload to send.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t internet_send_data(const char *json_payload);
