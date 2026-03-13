#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "modbus_io.h"

/**
 * @brief Modbus master component for reading registers from Modbus slave devices.
 * This component provides functions to initialize the Modbus master and read holding/input registers from slave devices. It is used by the Modbus manager component for periodic sampling of Modbus data
 */
esp_err_t modbus_init(
    int uart_num,
    int tx_pin,
    int rx_pin,
    int rts_pin,
    int baudrate,
    modbus_io_link_t link_type
);

/** 
 * @brief Read holding registers from a Modbus slave device.
 * @param slave_addr Modbus slave address
 * @param start_reg Starting register address to read from
 * @param reg_count Number of registers to read
 * @param dest Pointer to a buffer where the read register values will be stored. The buffer
 * must be large enough to hold reg_count uint16_t values.
 * @returns ESP_OK on success, or an appropriate error code on failure.
 * Note: The Modbus master must be initialized before calling this function. The function will return ESP_ERR_INVALID_STATE if the master is not initialized.
 */
esp_err_t modbus_read_holding_registers(
    uint8_t slave_addr,
    uint16_t start_reg,
    uint16_t reg_count,
    uint16_t *dest
);

/** 
 * @brief Read input registers from a Modbus slave device.
 * @param slave_addr Modbus slave address
 * @param start_reg Starting register address to read from
 * @param reg_count Number of registers to read
 * @param dest Pointer to a buffer where the read register values will be stored. The buffer
 * must be large enough to hold reg_count uint16_t values.
 * @returns ESP_OK on success, or an appropriate error code on failure.
 * Note: The Modbus master must be initialized before calling this function. The function will return
 */
esp_err_t modbus_read_input_registers(
    uint8_t slave_addr,
    uint16_t start_reg,
    uint16_t reg_count,
    uint16_t *dest
);

/**
 * @brief Probe a slave by attempting to read a single holding register.
 * @param slave_addr Modbus slave address to probe
 * @param reg_addr Register address used for the probe read
 * @returns ESP_OK if the slave responds successfully, otherwise an error code.
 */
esp_err_t modbus_probe_holding_register(uint8_t slave_addr, uint16_t reg_addr);

/**
 * @brief Probe a slave by attempting to read a single input register.
 * @param slave_addr Modbus slave address to probe
 * @param reg_addr Register address used for the probe read
 * @returns ESP_OK if the slave responds successfully, otherwise an error code.
 */
esp_err_t modbus_probe_input_register(uint8_t slave_addr, uint16_t reg_addr);
