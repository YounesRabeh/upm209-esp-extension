#pragma once

#include <stdbool.h>

#include "esp_err.h"

/**
 * @brief Start the processing service task.
 * @return ESP_OK on success, or an appropriate error code on failure.
 */
esp_err_t processing_service_start(void);

/**
 * @brief Check whether the processing service is currently running.
 * @return true if the service is running, false otherwise.
 */
bool processing_service_is_running(void);
