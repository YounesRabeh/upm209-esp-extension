#pragma once

#include <stdint.h>
#include "esp_err.h"

esp_err_t modbus_init(int uart_num, int tx_pin, int rx_pin, int baudrate);

esp_err_t modbus_read_holding_registers(
    uint8_t slave_addr,
    uint16_t start_reg,
    uint16_t reg_count,
    uint16_t *dest
);

esp_err_t modbus_read_input_registers(
    uint8_t slave_addr,
    uint16_t start_reg,
    uint16_t reg_count,
    uint16_t *dest
);
