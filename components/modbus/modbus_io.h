#pragma once

#include <stdint.h>

#include "driver/uart.h"
#include "esp_err.h"
#include "mbcontroller.h"

typedef enum {
    // Deprecated: kept only for backward compatibility.
    MODBUS_IO_LINK_RS232 = 0,
    MODBUS_IO_LINK_RS485 = 1
} modbus_io_link_t;

typedef struct {
    uart_port_t uart_port;
    int tx_pin;
    int rx_pin;
    int rts_pin;
    uint32_t baudrate;
    uart_word_length_t data_bits;
    uart_stop_bits_t stop_bits;
    uart_parity_t parity;
    modbus_io_link_t link_type;
} modbus_io_config_t;

esp_err_t modbus_io_fill_comm_info(
    const modbus_io_config_t *io_cfg,
    mb_communication_info_t *comm_info
);

esp_err_t modbus_io_apply_pins(const modbus_io_config_t *io_cfg);

esp_err_t modbus_io_apply_link_mode(const modbus_io_config_t *io_cfg);
