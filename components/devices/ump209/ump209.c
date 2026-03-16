#include "ump209.h"

#define WORDS_2 2U
#define WORDS_4 4U

#define SCALE_MILLI 0.001f
#define SCALE_DECI 0.1f
#define SCALE_CENTI_PERCENT 0.01f
#define SCALE_ONE 1.0f
#define UPM209_SIMPLE_SAMPLING 0U
#define REG_U(name, unit, addr, words, scale) \
    { name, unit, addr, words, TARGET_REGISTER_FUNC_READ_INPUT, scale, false }
#define REG_S(name, unit, addr, words, scale) \
    { name, unit, addr, words, TARGET_REGISTER_FUNC_READ_INPUT, scale, true }
#define REG(name, unit, addr, words, scale) REG_U(name, unit, addr, words, scale)

#define HARMONIC16(prefix, base) \
    REG(prefix " component 0 (DC)", "%", (base) + 0x00, WORDS_2, SCALE_CENTI_PERCENT), \
    REG(prefix " component 1st",    "%", (base) + 0x02, WORDS_2, SCALE_CENTI_PERCENT), \
    REG(prefix " component 2nd",    "%", (base) + 0x04, WORDS_2, SCALE_CENTI_PERCENT), \
    REG(prefix " component 3rd",    "%", (base) + 0x06, WORDS_2, SCALE_CENTI_PERCENT), \
    REG(prefix " component 4th",    "%", (base) + 0x08, WORDS_2, SCALE_CENTI_PERCENT), \
    REG(prefix " component 5th",    "%", (base) + 0x0A, WORDS_2, SCALE_CENTI_PERCENT), \
    REG(prefix " component 6th",    "%", (base) + 0x0C, WORDS_2, SCALE_CENTI_PERCENT), \
    REG(prefix " component 7th",    "%", (base) + 0x0E, WORDS_2, SCALE_CENTI_PERCENT), \
    REG(prefix " component 8th",    "%", (base) + 0x10, WORDS_2, SCALE_CENTI_PERCENT), \
    REG(prefix " component 9th",    "%", (base) + 0x12, WORDS_2, SCALE_CENTI_PERCENT), \
    REG(prefix " component 10th",   "%", (base) + 0x14, WORDS_2, SCALE_CENTI_PERCENT), \
    REG(prefix " component 11th",   "%", (base) + 0x16, WORDS_2, SCALE_CENTI_PERCENT), \
    REG(prefix " component 12th",   "%", (base) + 0x18, WORDS_2, SCALE_CENTI_PERCENT), \
    REG(prefix " component 13th",   "%", (base) + 0x1A, WORDS_2, SCALE_CENTI_PERCENT), \
    REG(prefix " component 14th",   "%", (base) + 0x1C, WORDS_2, SCALE_CENTI_PERCENT), \
    REG(prefix " component 15th",   "%", (base) + 0x1E, WORDS_2, SCALE_CENTI_PERCENT)

/**
 * @brief Register map extracted from UPM209 sheet (INTEGER column).
 */
static const MultimeterRegister s_target_registers[] = {
#if UPM209_SIMPLE_SAMPLING
    // AC power-drawn minimal set
    // Keep only what is typically needed for an A/C load:
    // voltage, current, active power, PF, frequency, imported active energy.

    // ===== CHUNK 1: 0x0000 .. 0x0087 (reduced) =====
    REG("Phase 1-N Voltage", "V", 0x0000, WORDS_2, SCALE_MILLI),
    REG_S("Phase 1 Current", "A", 0x000E, WORDS_2, SCALE_MILLI),
    REG_S("Phase 1 Active Power", "W", 0x0018, WORDS_4, SCALE_MILLI),
    REG_S("System Active Power", "W", 0x0024, WORDS_4, SCALE_MILLI),
    REG_S("Phase 1 Power Factor", "-", 0x0048, WORDS_2, SCALE_MILLI),
    REG("Frequency", "Hz", 0x0072, WORDS_2, SCALE_MILLI),

    // ===== CHUNK 4: 0x0400 .. 0x04DB (reduced) =====
    REG("Phase 1 imported active energy", "Wh", 0x0400, WORDS_4, SCALE_DECI),
    REG("System imported active energy", "Wh", 0x0418, WORDS_4, SCALE_DECI),
#else
    #include "ump209_full_registers.inc"
#endif
};

static const MultimeterRegisterSet s_target_register_set = {
    .registers = s_target_registers,
    .size = sizeof(s_target_registers) / sizeof(s_target_registers[0])
};

const MultimeterRegisterSet *ump209_get_target_register_set(void)
{
    return &s_target_register_set;
}
