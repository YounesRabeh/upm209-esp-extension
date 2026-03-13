#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "target_registers.h"

/**
 * @brief Modbus manager component for periodic sampling of Modbus registers and storage in memory.
 * This component initializes the Modbus master and starts a sampling task that can read either:
 * 1) a configured contiguous register window, or
 * 2) a device-specific target register set provided by reference.
 * @returns ESP_OK on successful start, or an appropriate error code on failure.
 */
esp_err_t modbus_manager_start(void);

/**
 * @brief Set the target register set to poll periodically.
 * Call this before modbus_manager_start().
 * @param target_register_set Pointer to a device register set definition.
 * @returns ESP_OK on success, or an error code on failure.
 */
esp_err_t modbus_manager_set_target_register_set(const MultimeterRegisterSet *target_register_set);

/**
 * @brief Check if the Modbus manager is currently running.
 * @returns true if the Modbus manager is running, false otherwise.
 */
bool modbus_manager_is_running(void);
