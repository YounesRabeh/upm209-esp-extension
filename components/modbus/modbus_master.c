#include "modbus_master.h"

#include <stdbool.h>

#include "driver/uart.h"
#include "mbcontroller.h"
#include "logging.h"

#define TAG "MODBUS"
#define MODBUS_CMD_READ_HOLDING_REGISTERS 3U
#define MODBUS_CMD_READ_INPUT_REGISTERS   4U

static bool s_initialized = false;
static uart_port_t s_uart = UART_NUM_1;
static void *s_master_ctx = NULL;

static const mb_parameter_descriptor_t s_min_descriptor = {
    .cid = 0,
    .param_key = "raw_holding",
    .param_units = "",
    .mb_slave_addr = 1,
    .mb_param_type = MB_PARAM_HOLDING,
    .mb_reg_start = 0,
    .mb_size = 1,
    .param_offset = 0,
    .param_type = PARAM_TYPE_U16,
    .param_size = sizeof(uint16_t),
    .param_opts = {{0}},
    .access = PAR_PERMS_READ
};

esp_err_t modbus_init(int uart_num, int tx_pin, int rx_pin, int baudrate)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_uart = (uart_port_t)uart_num;
    mb_communication_info_t comm = {
        .ser_opts.port = s_uart,
        .ser_opts.mode = MB_RTU,
        .ser_opts.baudrate = (uint32_t)baudrate,
        .ser_opts.parity = MB_PARITY_NONE,
        .ser_opts.uid = 0,
        .ser_opts.response_tout_ms = 1000,
        .ser_opts.data_bits = UART_DATA_8_BITS,
        .ser_opts.stop_bits = UART_STOP_BITS_1
    };

    esp_err_t err = mbc_master_create_serial(&comm, &s_master_ctx);
    if (err != ESP_OK || s_master_ctx == NULL) {
        LOG_ERROR(TAG, "mbc_master_create_serial failed: 0x%x", err);
        return err == ESP_OK ? ESP_ERR_INVALID_STATE : err;
    }

    err = uart_set_pin(s_uart, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "uart_set_pin failed: 0x%x", err);
        mbc_master_delete(s_master_ctx);
        s_master_ctx = NULL;
        return err;
    }

    err = mbc_master_set_descriptor(s_master_ctx, &s_min_descriptor, 1);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "mbc_master_set_descriptor failed: 0x%x", err);
        mbc_master_delete(s_master_ctx);
        s_master_ctx = NULL;
        return err;
    }

    err = mbc_master_start(s_master_ctx);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "mbc_master_start failed: 0x%x", err);
        mbc_master_delete(s_master_ctx);
        s_master_ctx = NULL;
        return err;
    }

    err = uart_set_mode(s_uart, UART_MODE_RS485_HALF_DUPLEX);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "uart_set_mode failed: 0x%x", err);
        mbc_master_delete(s_master_ctx);
        s_master_ctx = NULL;
        return err;
    }

    s_initialized = true;
    return ESP_OK;
}

static esp_err_t modbus_read_generic(
    uint8_t slave_addr,
    uint8_t function_code,
    uint16_t start_reg,
    uint16_t reg_count,
    uint16_t *dest
)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_master_ctx == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (dest == NULL || reg_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    mb_param_request_t req = {
        .slave_addr = slave_addr,
        .command = function_code,
        .reg_start = start_reg,
        .reg_size = reg_count
    };

    return mbc_master_send_request(s_master_ctx, &req, dest);
}

esp_err_t modbus_read_holding_registers(
    uint8_t slave_addr,
    uint16_t start_reg,
    uint16_t reg_count,
    uint16_t *dest
)
{
    return modbus_read_generic(
        slave_addr,
        MODBUS_CMD_READ_HOLDING_REGISTERS,
        start_reg,
        reg_count,
        dest
    );
}

esp_err_t modbus_read_input_registers(
    uint8_t slave_addr,
    uint16_t start_reg,
    uint16_t reg_count,
    uint16_t *dest
)
{
    return modbus_read_generic(
        slave_addr,
        MODBUS_CMD_READ_INPUT_REGISTERS,
        start_reg,
        reg_count,
        dest
    );
}

esp_err_t modbus_probe_holding_register(uint8_t slave_addr, uint16_t reg_addr)
{
    uint16_t probe_value = 0;
    return modbus_read_holding_registers(slave_addr, reg_addr, 1, &probe_value);
}
