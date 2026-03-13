#pragma once

#include <stddef.h>
#include <stdint.h>

#define TARGET_REGISTER_FUNC_READ_HOLDING 0x03U
#define TARGET_REGISTER_FUNC_READ_INPUT   0x04U

/**
 * @brief One target register definition for a Modbus device.
 */
typedef struct MultimeterRegister {
    const char *name;
    const char *unit;
    uint16_t reg_start;
    uint16_t reg_count;
    uint8_t function_code;
    float scale;
} MultimeterRegister;

/**
 * @brief Register-set reference used by the Modbus manager.
 */
typedef struct MultimeterRegisterSet {
    const MultimeterRegister *registers;
    size_t size;
} MultimeterRegisterSet;
