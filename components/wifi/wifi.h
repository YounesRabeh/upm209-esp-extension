#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi_types.h"  // wifi_config_t

/** 
 * @brief Initializes the WiFi subsystem and starts the WiFi driver in station mode.
 * This must be called before any other WiFi functions.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t wifi_init(void);

/** 
 * @brief Connects to a WiFi network using the provided configuration.
 * @param cfg Pointer to a wifi_config_t structure with the desired WiFi settings.
 * @return ESP_OK on successful connection, or an error code on failure.
 */
esp_err_t wifi_connect(const wifi_config_t *cfg);

/** 
 * @brief Attempts to connect to WiFi with retries and timeout.
 * @param cfg Pointer to a wifi_config_t structure with the desired WiFi settings.
 * @param timeout_ms Maximum time to wait for each connection attempt in milliseconds.
 * @param retry_count Number of connection attempts before giving up.
 * @return ESP_OK on successful connection, or an error code on failure after all retries.
 */
esp_err_t wifi_connect_retry(const wifi_config_t *cfg, int timeout_ms, int retry_count);

/**
 * @brief Disconnects from the currently connected WiFi network.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t wifi_disconnect(void);

/** 
 * @return true if  the WiFi is currently connected connected, false otherwise.
 */
bool wifi_is_connected(void);