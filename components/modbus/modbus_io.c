#include "modbus_io.h"

#include "logging.h"

#define TAG "MODBUS_IO"

static esp_err_t modbus_io_get_serial_parity(
    uart_parity_t uart_parity,
    uart_parity_t *serial_parity
)
{
    if (serial_parity == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (uart_parity) {
        case UART_PARITY_DISABLE:
            *serial_parity = UART_PARITY_DISABLE;
            return ESP_OK;
        case UART_PARITY_EVEN:
            *serial_parity = UART_PARITY_EVEN;
            return ESP_OK;
        case UART_PARITY_ODD:
            *serial_parity = UART_PARITY_ODD;
            return ESP_OK;
        default:
            return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t modbus_io_fill_comm_info(
    const modbus_io_config_t *io_cfg,
    mb_communication_info_t *comm_info
)
{
    if (io_cfg == NULL || comm_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uart_parity_t serial_parity = UART_PARITY_DISABLE;
    esp_err_t err = modbus_io_get_serial_parity(io_cfg->parity, &serial_parity);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "Unsupported UART parity: %d", (int)io_cfg->parity);
        return err;
    }

    *comm_info = (mb_communication_info_t){
        .ser_opts.port = io_cfg->uart_port,
        .ser_opts.mode = MB_RTU,
        .ser_opts.baudrate = io_cfg->baudrate,
        .ser_opts.parity = serial_parity,
        .ser_opts.uid = 0,
        .ser_opts.response_tout_ms = 1000,
        .ser_opts.data_bits = io_cfg->data_bits,
        .ser_opts.stop_bits = io_cfg->stop_bits
    };

    return ESP_OK;
}

esp_err_t modbus_io_apply_pins(const modbus_io_config_t *io_cfg)
{
    if (io_cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return uart_set_pin(
        io_cfg->uart_port,
        io_cfg->tx_pin,
        io_cfg->rx_pin,
        io_cfg->rts_pin,
        UART_PIN_NO_CHANGE
    );
}

esp_err_t modbus_io_apply_link_mode(const modbus_io_config_t *io_cfg)
{
    if (io_cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (io_cfg->link_type == MODBUS_IO_LINK_RS232) {
        LOG_WARNING(TAG, "RS232 link is deprecated and no longer supported");
        return ESP_ERR_NOT_SUPPORTED;
    }

    return uart_set_mode(io_cfg->uart_port, UART_MODE_RS485_HALF_DUPLEX);
}
