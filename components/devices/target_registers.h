#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Modbus function code for Read Holding Registers (0x03).
 */
#define TARGET_REGISTER_FUNC_READ_HOLDING 0x03U
/**
 * @brief Modbus function code for Read Input Registers (0x04).
 */
#define TARGET_REGISTER_FUNC_READ_INPUT   0x04U

/**
 * @brief One target register definition for a Modbus device.
 *
 * Register arrays are expected to be ordered by function code and ascending
 * register address so acquisition code can merge adjacent reads efficiently.
 */
typedef struct MultimeterRegister {
    /**< Human-readable label of the measurement (not owned by the caller). */
    const char *name;
    /**< Unit string associated with the scaled value (not owned by the caller). */
    const char *unit;
    /**< First Modbus register address for this value. */
    uint16_t reg_start;
    /**< Number of contiguous 16-bit words used by this value (currently 1..4). */
    uint16_t reg_count;
    /**< Modbus function code used to read this register block. */
    uint8_t function_code;
    /**< Scale factor applied to the decoded raw integer value. */
    float scale;
    /**< Signedness flag for raw decoding (true = signed, false = unsigned). */
    bool is_signed;
} MultimeterRegister;

/**
 * @brief Register-set reference used by the Modbus manager and processing code.
 */
typedef struct MultimeterRegisterSet {
    /**< Pointer to the first register definition in a contiguous array. */
    const MultimeterRegister *registers;
    /**< Number of valid entries in registers. */
    size_t size;
} MultimeterRegisterSet;
