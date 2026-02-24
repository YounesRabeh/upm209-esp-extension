#include "modbus_master.h"
#include "modbus_crc.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>

#define BUF_SIZE 256
static const char *TAG = "MODBUS";

static int s_uart = UART_NUM_1;

esp_err_t modbus_init(int uart_num, int tx_pin, int rx_pin, int baudrate)
{
    s_uart = uart_num;

    uart_config_t uart_config = {
        .baud_rate = baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_driver_install(s_uart, BUF_SIZE, 0, 0, NULL, 0);
    uart_param_config(s_uart, &uart_config);
    uart_set_pin(s_uart, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    return ESP_OK;
}

static esp_err_t modbus_read_generic(
    uint8_t slave_addr,
    uint8_t function,
    uint16_t start_reg,
    uint16_t reg_count,
    uint16_t *dest
)
{
    uint8_t request[8];

    request[0] = slave_addr;
    request[1] = function;
    request[2] = start_reg >> 8;
    request[3] = start_reg & 0xFF;
    request[4] = reg_count >> 8;
    request[5] = reg_count & 0xFF;

    uint16_t crc = modbus_crc16(request, 6);
    request[6] = crc & 0xFF;
    request[7] = crc >> 8;

    uart_write_bytes(s_uart, (const char*)request, 8);

    uint8_t response[BUF_SIZE];
    int len = uart_read_bytes(s_uart, response, BUF_SIZE, pdMS_TO_TICKS(200));

    if (len < 5) {
        return ESP_FAIL;
    }

    uint16_t resp_crc = (response[len-1] << 8) | response[len-2];
    uint16_t calc_crc = modbus_crc16(response, len - 2);

    if (resp_crc != calc_crc) {
        return ESP_ERR_INVALID_CRC;
    }

    for (int i = 0; i < reg_count; i++) {
        dest[i] = (response[3 + i*2] << 8) | response[4 + i*2];
    }

    return ESP_OK;
}

esp_err_t modbus_read_holding_registers(
    uint8_t slave_addr,
    uint16_t start_reg,
    uint16_t reg_count,
    uint16_t *dest
)
{
    return modbus_read_generic(slave_addr, 0x03, start_reg, reg_count, dest);
}

esp_err_t modbus_read_input_registers(
    uint8_t slave_addr,
    uint16_t start_reg,
    uint16_t reg_count,
    uint16_t *dest
)
{
    return modbus_read_generic(slave_addr, 0x04, start_reg, reg_count, dest);
}
