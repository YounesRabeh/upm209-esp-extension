#pragma once

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Modbus manager component for periodic raw sampling of fixed UPM209 input registers.
 * This component initializes the Modbus master and starts a sampling task that periodically
 * reads a fixed contiguous window and stores raw samples in memory.
 * @returns ESP_OK on successful start, or an appropriate error code on failure.
 */
esp_err_t modbus_manager_start(void);

/**
 * @brief Check if the Modbus manager is currently running.
 * @returns true if the Modbus manager is running, false otherwise.
 */
bool modbus_manager_is_running(void);
