#include "outlier_handling.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "logging.h"
#include "target_registers.h"
#include "ump209.h"

#define TAG "PROCESSING"

#define PROCESSING_MAX_LAYOUT_ENTRIES 512U
#define PROCESSING_MAX_WINDOW_SAMPLES 64U
#define PROCESSING_MAX_REQUEST_WORDS  125U
#define PROCESSING_IQR_MIN_SAMPLES    4U
#define PROCESSING_IQR_FACTOR         1.5
#define PROCESSING_SIGNED_SIGN_BIT_MODE 1U

typedef struct {
    const MultimeterRegister *reg;
    uint16_t word_offset;
} processing_layout_entry_t;

static processing_layout_entry_t s_layout[PROCESSING_MAX_LAYOUT_ENTRIES] = {0};
static processing_upm209_layout_info_t s_layout_info = {0};
static bool s_layout_ready = false;

static int processing_double_compare(const void *a, const void *b)
{
    const double da = *(const double *)a;
    const double db = *(const double *)b;
    if (da < db) {
        return -1;
    }
    if (da > db) {
        return 1;
    }
    return 0;
}

static double processing_percentile_sorted(const double *sorted, size_t n, double p)
{
    if (n == 0U) {
        return 0.0;
    }
    if (n == 1U) {
        return sorted[0];
    }

    double pos = p * (double)(n - 1U);
    size_t lo = (size_t)pos;
    size_t hi = (size_t)ceil(pos);
    if (hi >= n) {
        hi = n - 1U;
    }
    double frac = pos - (double)lo;
    return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

static void processing_compute_iqr_stats(
    const double *values,
    size_t n,
    double *out_mean,
    double *out_min,
    double *out_max
)
{
    double sorted[PROCESSING_MAX_WINDOW_SAMPLES] = {0.0};
    for (size_t i = 0; i < n; ++i) {
        sorted[i] = values[i];
    }
    qsort(sorted, n, sizeof(sorted[0]), processing_double_compare);

    double low = -INFINITY;
    double high = INFINITY;
    bool use_filter = (n >= PROCESSING_IQR_MIN_SAMPLES);

    if (use_filter) {
        const double q1 = processing_percentile_sorted(sorted, n, 0.25);
        const double q3 = processing_percentile_sorted(sorted, n, 0.75);
        const double iqr = q3 - q1;
        low = q1 - (PROCESSING_IQR_FACTOR * iqr);
        high = q3 + (PROCESSING_IQR_FACTOR * iqr);
    }

    double sum = 0.0;
    double min_v = INFINITY;
    double max_v = -INFINITY;
    size_t kept = 0U;

    for (size_t i = 0; i < n; ++i) {
        const double v = values[i];
        if (use_filter && (v < low || v > high)) {
            continue;
        }
        sum += v;
        if (v < min_v) {
            min_v = v;
        }
        if (v > max_v) {
            max_v = v;
        }
        ++kept;
    }

    if (kept == 0U) {
        sum = 0.0;
        min_v = INFINITY;
        max_v = -INFINITY;
        for (size_t i = 0; i < n; ++i) {
            const double v = values[i];
            sum += v;
            if (v < min_v) {
                min_v = v;
            }
            if (v > max_v) {
                max_v = v;
            }
        }
        kept = n;
    }

    if (kept == 0U) {
        *out_mean = 0.0;
        *out_min = 0.0;
        *out_max = 0.0;
        return;
    }

    *out_mean = sum / (double)kept;
    *out_min = min_v;
    *out_max = max_v;
}

static esp_err_t processing_decode_scaled(
    const uint16_t *words,
    uint16_t word_count,
    float scale,
    bool is_signed,
    double *out_value
)
{
    if (words == NULL || out_value == NULL || word_count == 0U || word_count > 4U) {
        return ESP_ERR_INVALID_ARG;
    }

    uint64_t raw = 0U;
    for (uint16_t i = 0; i < word_count; ++i) {
        raw = (raw << 16U) | (uint64_t)words[i];
    }

    if (!is_signed) {
        *out_value = (double)raw * (double)scale;
        return ESP_OK;
    }

#if PROCESSING_SIGNED_SIGN_BIT_MODE
    // UPM209 "Sign" fields: MSB is sign bit (0=+, 1=-), magnitude in remaining bits.
    const uint32_t total_bits = (uint32_t)word_count * 16U;
    const uint64_t sign_mask = 1ULL << (total_bits - 1U);
    const uint64_t magnitude_mask = sign_mask - 1ULL;
    const bool is_negative = (raw & sign_mask) != 0ULL;
    const int64_t magnitude = (int64_t)(raw & magnitude_mask);
    const int64_t signed_value = is_negative ? -magnitude : magnitude;
    *out_value = (double)signed_value * (double)scale;
#else
    // Alternative model: standard two's-complement signed integer.
    if (word_count == 1U) {
        int16_t raw_s = (int16_t)words[0];
        *out_value = (double)raw_s * (double)scale;
        return ESP_OK;
    }

    if (word_count == 2U) {
        int32_t raw_s = (int32_t)(((uint32_t)words[0] << 16U) | (uint32_t)words[1]);
        *out_value = (double)raw_s * (double)scale;
        return ESP_OK;
    }

    if (word_count == 3U) {
        uint64_t raw24 = raw & 0x0000FFFFFFFFFFFFULL;
        if ((raw24 & 0x0000800000000000ULL) != 0ULL) {
            raw24 |= 0xFFFF000000000000ULL;
        }
        int64_t raw_s = (int64_t)raw24;
        *out_value = (double)raw_s * (double)scale;
        return ESP_OK;
    }

    int64_t raw_s = (int64_t)raw;
    *out_value = (double)raw_s * (double)scale;
#endif
    return ESP_OK;
}

static esp_err_t processing_build_layout(void)
{
    const MultimeterRegisterSet *set = ump209_get_target_register_set();
    if (set == NULL || set->registers == NULL || set->size == 0U) {
        LOG_ERROR(TAG, "UPM209 register set unavailable");
        return ESP_ERR_INVALID_STATE;
    }
    if (set->size > PROCESSING_MAX_LAYOUT_ENTRIES) {
        LOG_ERROR(TAG, "Register set too large: size=%u", (unsigned)set->size);
        return ESP_ERR_INVALID_SIZE;
    }

    uint32_t cycle_words = 0U;
    size_t i = 0U;
    while (i < set->size) {
        const MultimeterRegister *first = &set->registers[i];
        if (first->reg_count == 0U || first->reg_count > PROCESSING_MAX_REQUEST_WORDS) {
            LOG_ERROR(TAG, "Invalid register width at index=%u", (unsigned)i);
            return ESP_ERR_INVALID_SIZE;
        }

        uint8_t block_fc = first->function_code;
        uint16_t block_start = first->reg_start;
        uint32_t block_end = (uint32_t)first->reg_start + (uint32_t)first->reg_count;
        size_t j = i + 1U;

        while (j < set->size) {
            const MultimeterRegister *next = &set->registers[j];
            if (next->reg_count == 0U || next->reg_count > PROCESSING_MAX_REQUEST_WORDS) {
                break;
            }
            if (next->function_code != block_fc) {
                break;
            }
            if (next->reg_start > block_end) {
                break;
            }

            uint32_t next_end = (uint32_t)next->reg_start + (uint32_t)next->reg_count;
            uint32_t merged_end = (next_end > block_end) ? next_end : block_end;
            uint32_t merged_count = merged_end - (uint32_t)block_start;
            if (merged_count > PROCESSING_MAX_REQUEST_WORDS) {
                break;
            }
            block_end = merged_end;
            ++j;
        }

        uint32_t block_count = block_end - (uint32_t)block_start;
        if ((cycle_words + block_count) > UINT16_MAX) {
            LOG_ERROR(TAG, "Cycle layout overflow");
            return ESP_ERR_INVALID_SIZE;
        }

        for (size_t k = i; k < j; ++k) {
            const MultimeterRegister *reg = &set->registers[k];
            uint32_t reg_offset_in_block = (uint32_t)reg->reg_start - (uint32_t)block_start;
            if ((reg_offset_in_block + reg->reg_count) > block_count) {
                LOG_ERROR(TAG, "Invalid register mapping at index=%u", (unsigned)k);
                return ESP_ERR_INVALID_SIZE;
            }

            uint32_t absolute_offset = cycle_words + reg_offset_in_block;
            if (absolute_offset > UINT16_MAX) {
                return ESP_ERR_INVALID_SIZE;
            }

            s_layout[k].reg = reg;
            s_layout[k].word_offset = (uint16_t)absolute_offset;
        }

        cycle_words += block_count;
        i = j;
    }

    s_layout_info.expected_start_reg = set->registers[0].reg_start;
    s_layout_info.expected_cycle_words = (uint16_t)cycle_words;
    s_layout_info.measurement_count = set->size;
    s_layout_ready = true;
    return ESP_OK;
}

static esp_err_t processing_ensure_layout(void)
{
    if (s_layout_ready) {
        return ESP_OK;
    }
    return processing_build_layout();
}

esp_err_t processing_upm209_get_layout_info(processing_upm209_layout_info_t *out_info)
{
    if (out_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = processing_ensure_layout();
    if (err != ESP_OK) {
        return err;
    }

    *out_info = s_layout_info;
    return ESP_OK;
}

esp_err_t processing_upm209_compute_window(
    const processing_raw_cycle_t *cycles,
    size_t cycle_count,
    processing_measurement_t *out_measurements,
    size_t out_capacity,
    size_t *out_count
)
{
    if (cycles == NULL || out_measurements == NULL || out_count == NULL || cycle_count == 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    if (cycle_count > PROCESSING_MAX_WINDOW_SAMPLES) {
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t err = processing_ensure_layout();
    if (err != ESP_OK) {
        return err;
    }
    if (out_capacity < s_layout_info.measurement_count) {
        return ESP_ERR_INVALID_SIZE;
    }

    for (size_t i = 0; i < cycle_count; ++i) {
        if (cycles[i].words == NULL || cycles[i].word_count < s_layout_info.expected_cycle_words) {
            return ESP_ERR_INVALID_SIZE;
        }
    }

    double values[PROCESSING_MAX_WINDOW_SAMPLES] = {0.0};
    for (size_t m = 0; m < s_layout_info.measurement_count; ++m) {
        const processing_layout_entry_t *entry = &s_layout[m];
        const MultimeterRegister *reg = entry->reg;
        if (reg == NULL) {
            return ESP_ERR_INVALID_STATE;
        }

        uint32_t end_offset = (uint32_t)entry->word_offset + (uint32_t)reg->reg_count;
        if (end_offset > s_layout_info.expected_cycle_words) {
            return ESP_ERR_INVALID_SIZE;
        }

        for (size_t s = 0; s < cycle_count; ++s) {
            err = processing_decode_scaled(
                &cycles[s].words[entry->word_offset],
                reg->reg_count,
                reg->scale,
                reg->is_signed,
                &values[s]
            );
            if (err != ESP_OK) {
                return err;
            }
        }

        double mean_v = 0.0;
        double min_v = 0.0;
        double max_v = 0.0;
        processing_compute_iqr_stats(values, cycle_count, &mean_v, &min_v, &max_v);

        out_measurements[m] = (processing_measurement_t){
            .num_reg = reg->reg_start,
            .word = reg->reg_count,
            .value = mean_v,
            .min = min_v,
            .max = max_v,
            .unit = (reg->unit != NULL) ? reg->unit : "",
            .description = (reg->name != NULL) ? reg->name : ""
        };
    }

    *out_count = s_layout_info.measurement_count;
    return ESP_OK;
}
