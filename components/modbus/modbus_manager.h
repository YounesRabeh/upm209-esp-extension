#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Callback invoked for each sampled Modbus block.
 * @param slave_addr Modbus slave address used for sampling.
 * @param start_reg Starting register address of the sampled block.
 * @param registers Pointer to sampled register words.
 * @param reg_count Number of valid words in registers.
 * @param timestamp_s Sample timestamp in seconds since Unix epoch.
 * @param ctx User context pointer set via modbus_manager_set_sample_sink.
 * @return ESP_OK on success, or an error code to signal sink-side failure.
 */
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
 * @return ESP_OK on successful start, or an appropriate error code on failure.
 */
esp_err_t modbus_manager_start(void);

/**
 * @brief Registers a sink callback used to consume sampled Modbus cycles.
 * @param sink Callback to receive samples. Pass NULL to detach current sink.
 * @param ctx Opaque user context passed back to sink on each callback.
 * @return ESP_OK on success, or an appropriate error code on failure.
 */
esp_err_t modbus_manager_set_sample_sink(modbus_sample_sink_t sink, void *ctx);

/**
 * @brief Check if the Modbus manager is currently running.
 * @return true if the Modbus manager is running, false otherwise.
 */
bool modbus_manager_is_running(void);
