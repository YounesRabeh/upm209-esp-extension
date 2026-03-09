#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"


typedef struct {
    uint32_t timestamp_s;
    uint16_t start_reg;
    uint16_t reg_count;
    uint8_t slave_addr;
} memory_sample_meta_t;

esp_err_t memory_init(void);
esp_err_t memory_deinit(void);
bool memory_is_ready(void);

esp_err_t memory_enqueue_modbus_sample(
    uint8_t slave_addr,
    uint16_t start_reg,
    const uint16_t *registers,
    uint16_t reg_count,
    uint32_t timestamp_s
);

esp_err_t memory_peek_modbus_sample(
    memory_sample_meta_t *meta_out,
    uint16_t *registers_out,
    uint16_t max_registers
);

esp_err_t memory_pop_modbus_sample(
    memory_sample_meta_t *meta_out,
    uint16_t *registers_out,
    uint16_t max_registers
);

esp_err_t memory_clear(void);
uint32_t memory_pending_samples(void);
uint32_t memory_used_bytes(void);
