#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    const uint16_t *words;
    uint16_t word_count;
} processing_raw_cycle_t;

typedef struct {
    uint16_t num_reg;
    uint16_t word;
    double value;
    double min;
    double max;
    const char *unit;
    const char *description;
} processing_measurement_t;

typedef struct {
    uint16_t expected_start_reg;
    uint16_t expected_cycle_words;
    size_t measurement_count;
} processing_upm209_layout_info_t;

esp_err_t processing_upm209_get_layout_info(processing_upm209_layout_info_t *out_info);

esp_err_t processing_upm209_compute_window(
    const processing_raw_cycle_t *cycles,
    size_t cycle_count,
    processing_measurement_t *out_measurements,
    size_t out_capacity,
    size_t *out_count
);
