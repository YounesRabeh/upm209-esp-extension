#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi_types.h"  // wifi_config_t

/* init driver WiFi */
esp_err_t wifi_init(void);

/* connetti WiFi */
esp_err_t wifi_connect(const wifi_config_t *cfg);

/* connetti WiFi con retry e timeout */
esp_err_t wifi_connect_retry(const wifi_config_t *cfg, int timeout_ms, int retry_count);

/* disconnetti WiFi */
esp_err_t wifi_disconnect(void);

/* stato connessione */
bool wifi_is_connected(void);