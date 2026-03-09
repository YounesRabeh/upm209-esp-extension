#pragma once

#include <stdbool.h>
#include "esp_err.h"

/** 
 * @brief Modbus manager component for periodic sampling of Modbus registers and storage in memory.
 * This component initializes the Modbus master, creates a sampling task that periodically reads
 * holding registers from a configured slave device, and stores the samples in memory for later retrieval.
 * The component can be enabled or disabled via the CONFIG_MODBUS_MANAGER_ENABLE configuration option. When enabled,
 * @returns ESP_OK on successful start, or an appropriate error code on failure. The sampling task runs indefinitely until the system is reset.
 */
esp_err_t modbus_manager_start(void);

/**
 * @brief Check if the Modbus manager is currently running.
 * @returns true if the Modbus manager is running, false otherwise.
 */
bool modbus_manager_is_running(void);
