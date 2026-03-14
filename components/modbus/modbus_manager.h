#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef esp_err_t (*modbus_sample_sink_t)(
    uint8_t slave_addr,
    uint16_t start_reg,
    const uint16_t *registers,
    uint16_t reg_count,
    uint32_t timestamp_s,
    void *ctx
);

/**
 * @brief Modbus manager component for periodic raw sampling of UPM209 register-set blocks.
 * This component initializes the Modbus master and starts a sampling task that periodically reads
 * merged Modbus blocks and publishes one raw sample per cycle to a registered sink.
 * @returns ESP_OK on successful start, or an appropriate error code on failure.
 */
esp_err_t modbus_manager_start(void);

/**
 * @brief Registers a sink callback used to consume sampled Modbus cycles.
 * Pass NULL sink to detach.
 */
esp_err_t modbus_manager_set_sample_sink(modbus_sample_sink_t sink, void *ctx);

/**
 * @brief Check if the Modbus manager is currently running.
 * @returns true if the Modbus manager is running, false otherwise.
 */
bool modbus_manager_is_running(void);
