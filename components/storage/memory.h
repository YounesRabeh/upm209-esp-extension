#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Structure representing metadata for a stored Modbus sample.
 */
typedef struct {
    uint32_t timestamp_s;
    uint16_t start_reg;
    uint16_t reg_count;
    uint8_t slave_addr;
} memory_sample_meta_t;

/**
 * @brief Initialize the memory component. This must be called before any other memory functions.
 * @return ESP_OK on success, or an error code on failure.
 * This function sets up the necessary resources for storing Modbus samples in memory. It should be called once during system initialization before any samples are enqueued or retrieved. If the memory component is already initialized, this function will return ESP_OK without reinitializing.
 */
esp_err_t memory_init(void);

/**
 * @brief Deinitialize the memory component and free any allocated resources. After this call, the memory component must be reinitialized before use.
 * @return ESP_OK on success, or an error code on failure.
 * This function cleans up any resources used by the memory component. It should be called when the
 */
esp_err_t memory_deinit(void);

/**
 * @brief Check if the memory component is initialized and ready for use.
 * @return true if the memory component is ready, false otherwise.
 */
bool memory_is_ready(void);

/**
 * @brief Enqueue a Modbus sample into memory for later retrieval. The sample consists of the slave address, starting register, register values, and a timestamp.
 * @param slave_addr Modbus slave address associated with the sample
 * @param start_reg Starting register address of the sample
 * @param registers Pointer to an array of register values to store
 * @param reg_count Number of registers in the sample
 * @param timestamp_s Timestamp of the sample in seconds since the Unix epoch
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t memory_enqueue_modbus_sample(
    uint8_t slave_addr,
    uint16_t start_reg,
    const uint16_t *registers,
    uint16_t reg_count,
    uint32_t timestamp_s
);

/** 
 * @brief Retrieve the next Modbus sample from memory without removing it. The sample metadata and register values are copied to the provided output parameters.
 * @param meta_out Pointer to a memory_sample_meta_t structure where the sample metadata will be 
 * @param registers_out Pointer to an array where the register values will be copied
 * @param max_registers Maximum number of registers that can be copied to registers_out
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t memory_peek_modbus_sample(
    memory_sample_meta_t *meta_out,
    uint16_t *registers_out,
    uint16_t max_registers
);

/**
 * @brief Retrieve and remove the next Modbus sample from memory. The sample metadata and register values are copied to the provided output parameters.
 * @param meta_out Pointer to a memory_sample_meta_t structure where the sample metadata will be
 * @param registers_out Pointer to an array where the register values will be copied
 * @param max_registers Maximum number of registers that can be copied to registers_out
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t memory_pop_modbus_sample(
    memory_sample_meta_t *meta_out,
    uint16_t *registers_out,
    uint16_t max_registers
);

/**
 * @brief Clear all stored Modbus samples from memory. After this call, the memory will be empty and pending_samples() will return 0.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t memory_clear(void);

/** 
 * @brief Get the number of pending Modbus samples currently stored in memory. This indicates how many samples are waiting to be retrieved.
 * @return The number of pending samples, or 0 if the memory component is not ready
 */
uint32_t memory_pending_samples(void);

/**
 * @brief Get the total amount of memory currently used to store Modbus samples. This can be used for monitoring memory usage and ensuring it does not exceed limits.
 * @return The number of bytes currently used for storing samples, or 0 if the memory component is not ready
 */
uint32_t memory_used_bytes(void);
