#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    /**< Pointer to cycle words (not owned by the caller). */
    const uint16_t *words;
    /**< Number of valid words in words. */
    uint16_t word_count;
} processing_raw_cycle_t;

typedef struct {
    /**< Source register number. */
    uint16_t num_reg;
    /**< Raw source word value used for conversion. */
    uint16_t word;
    /**< Converted engineering value. */
    double value;
    /**< Lower bound observed/derived for the processed window. */
    double min;
    /**< Upper bound observed/derived for the processed window. */
    double max;
    /**< Unit string for value (not owned by the caller). */
    const char *unit;
    /**< Human-readable register description (not owned by the caller). */
    const char *description;
} processing_measurement_t;

typedef struct {
    /**< First register expected in a valid acquisition cycle. */
    uint16_t expected_start_reg;
    /**< Number of words expected per cycle. */
    uint16_t expected_cycle_words;
    /**< Number of measurements produced for one compute call. */
    size_t measurement_count;
} processing_upm209_layout_info_t;

/**
 * @brief Get expected UPM209 register layout information.
 * @param out_info Pointer to destination structure.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if out_info is NULL.
 */
esp_err_t processing_upm209_get_layout_info(processing_upm209_layout_info_t *out_info);

/**
 * @brief Compute filtered measurement values from multiple raw cycles.
 * @param cycles Input cycle array.
 * @param cycle_count Number of entries in cycles.
 * @param out_measurements Output buffer for computed measurements.
 * @param out_capacity Capacity of out_measurements in elements.
 * @param out_count Number of measurements written on success.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for invalid input,
 *         ESP_ERR_NO_MEM if out_capacity is not large enough.
 */
esp_err_t processing_upm209_compute_window(
    const processing_raw_cycle_t *cycles,
    size_t cycle_count,
    processing_measurement_t *out_measurements,
    size_t out_capacity,
    size_t *out_count
);
