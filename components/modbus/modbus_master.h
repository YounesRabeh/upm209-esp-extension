#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "modbus_io.h"

/**
 * @brief Modbus master component for reading registers from Modbus slave devices.
 * @param uart_num UART port number used by the Modbus stack.
 * @param tx_pin GPIO number for UART TX.
 * @param rx_pin GPIO number for UART RX.
 * @param rts_pin GPIO number for UART RTS/DE (used for RS485 direction control).
 * @param baudrate UART baud rate.
 * @param parity UART parity mode.
 * @param link_type Physical link mode (RS232 legacy or RS485).
 * @return ESP_OK on success, or an appropriate error code on failure.
 */
esp_err_t modbus_init(
    int uart_num,
    int tx_pin,
    int rx_pin,
    int rts_pin,
    int baudrate,
    uart_parity_t parity,
    modbus_io_link_t link_type
);

/** 
 * @brief Read holding registers from a Modbus slave device.
 * @param slave_addr Modbus slave address
 * @param start_reg Starting register address to read from
 * @param reg_count Number of registers to read
 * @param dest Output buffer for register values (must contain at least reg_count words)
 * @return ESP_OK on success, or an appropriate error code on failure.
 *         Returns ESP_ERR_INVALID_STATE if the Modbus master is not initialized.
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
 * @param dest Output buffer for register values (must contain at least reg_count words)
 * @return ESP_OK on success, or an appropriate error code on failure.
 *         Returns ESP_ERR_INVALID_STATE if the Modbus master is not initialized.
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
 * @return ESP_OK if the slave responds successfully, otherwise an error code.
 */
esp_err_t modbus_probe_holding_register(uint8_t slave_addr, uint16_t reg_addr);

/**
 * @brief Probe a slave by attempting to read a single input register.
 * @param slave_addr Modbus slave address to probe
 * @param reg_addr Register address used for the probe read
 * @return ESP_OK if the slave responds successfully, otherwise an error code.
 */
esp_err_t modbus_probe_input_register(uint8_t slave_addr, uint16_t reg_addr);

/**
 * @brief Recover Modbus UART link after framing/timeouts.
 * @return ESP_OK on success or an error code on failure.
 */
esp_err_t modbus_recover_link(void);
