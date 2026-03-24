#pragma once

#include <stdint.h>

#include "driver/uart.h"
#include "esp_err.h"
#include "mbcontroller.h"

/**
 * @brief Physical link type used by the Modbus I/O layer.
 */
typedef enum {
    /**< Deprecated: kept only for backward compatibility. */
    MODBUS_IO_LINK_RS232 = 0,
    /**< RS485 half-duplex link mode. */
    MODBUS_IO_LINK_RS485 = 1
} modbus_io_link_t;

/**
 * @brief UART and link configuration used to initialize Modbus communication.
 */
typedef struct {
    /**< ESP-IDF UART port number. */
    uart_port_t uart_port;
    /**< GPIO number for UART TX. */
    int tx_pin;
    /**< GPIO number for UART RX. */
    int rx_pin;
    /**< GPIO number for UART RTS/DE (used for RS485 direction control). */
    int rts_pin;
    /**< UART baud rate. */
    uint32_t baudrate;
    /**< UART data bits configuration. */
    uart_word_length_t data_bits;
    /**< UART stop bits configuration. */
    uart_stop_bits_t stop_bits;
    /**< UART parity configuration. */
    uart_parity_t parity;
    /**< Link mode for line signaling. */
    modbus_io_link_t link_type;
} modbus_io_config_t;

/**
 * @brief Fill ESP Modbus controller communication info from I/O config.
 * @param io_cfg Input Modbus I/O configuration.
 * @param comm_info Output communication descriptor consumed by the controller.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on invalid input.
 */
esp_err_t modbus_io_fill_comm_info(
    const modbus_io_config_t *io_cfg,
    mb_communication_info_t *comm_info
);

/**
 * @brief Apply configured UART pins to the selected UART port.
 * @param io_cfg Input Modbus I/O configuration.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on invalid input, or driver error code.
 */
esp_err_t modbus_io_apply_pins(const modbus_io_config_t *io_cfg);

/**
 * @brief Apply configured physical link mode (for example RS485 settings).
 * @param io_cfg Input Modbus I/O configuration.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on invalid input, or driver error code.
 */
esp_err_t modbus_io_apply_link_mode(const modbus_io_config_t *io_cfg);
