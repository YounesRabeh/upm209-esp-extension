#pragma once

#include <stdbool.h>

#include "esp_err.h"

/**
 * @brief Start the sampling service task.
 * @return ESP_OK on success, or an appropriate error code on failure.
 */
esp_err_t sampling_service_start(void);

/**
 * @brief Check whether the sampling service is currently running.
 * @return true if the service is running, false otherwise.
 */
bool sampling_service_is_running(void);
