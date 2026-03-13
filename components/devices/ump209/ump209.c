#include "ump209.h"

/**
 * @brief Array of all basic registers of the UMP209 multimeter.
 */
static const MultimeterRegister s_target_registers[] = {
    // Instant values
    { "Phase 1-N Voltage",      "V",    0x0000, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 0
    { "Phase 2-N Voltage",      "V",    0x0002, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 1
    { "Phase 3-N Voltage",      "V",    0x0004, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 2
    { "Phase 1-2 Voltage",      "V",    0x0006, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 3
    { "Phase 2-3 Voltage",      "V",    0x0008, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 4
    { "Phase 3-1 Voltage",      "V",    0x000A, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 5
    { "System Voltage",         "V",    0x000C, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 6

    { "Phase 1 Current",        "A",    0x000E, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 7
    { "Phase 2 Current",        "A",    0x0010, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 8
    { "Phase 3 Current",        "A",    0x0012, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 9
    { "Neutral Current",        "A",    0x0014, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 10
    { "System Current",         "A",    0x0016, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 11

    { "Phase 1 Active Power",   "W",    0x0018, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 12
    { "Phase 2 Active Power",   "W",    0x001C, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 13
    { "Phase 3 Active Power",   "W",    0x0020, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 14
    { "System Active Power",    "W",    0x0024, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 15

    { "Phase 1 Apparent Power", "VA",   0x0028, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 16
    { "Phase 2 Apparent Power", "VA",   0x002C, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 17
    { "Phase 3 Apparent Power", "VA",   0x0030, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 18
    { "System Apparent Power",  "VA",   0x0034, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 19

    { "Phase 1 Reactive Power", "var",  0x0038, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 20
    { "Phase 2 Reactive Power", "var",  0x003C, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 21
    { "Phase 3 Reactive Power", "var",  0x0040, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 22
    { "System Reactive Power",  "var",  0x0044, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 23

    { "Phase 1 Power Factor",   "-",    0x0048, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 24
    { "Phase 2 Power Factor",   "-",    0x004A, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 25
    { "Phase 3 Power Factor",   "-",    0x004C, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 26
    { "System Power Factor",    "-",    0x004E, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 27

    { "Phase 1 DPF",            "-",    0x0050, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 28
    { "Phase 2 DPF",            "-",    0x0052, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 29
    { "Phase 3 DPF",            "-",    0x0054, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 30

    { "Phase 1 TAN(phi)",       "-",    0x0056, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 31
    { "Phase 2 TAN(phi)",       "-",    0x0058, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 32
    { "Phase 3 TAN(phi)",       "-",    0x005A, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 33
    { "System TAN(phi)",        "-",    0x005C, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 34

    { "Phase 1 Voltage THD",    "%",    0x005E, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 35
    { "Phase 2 Voltage THD",    "%",    0x0060, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 36
    { "Phase 3 Voltage THD",    "%",    0x0062, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 37
    { "Line 12 Voltage THD",    "%",    0x0064, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 38
    { "Line 23 Voltage THD",    "%",    0x0066, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 39
    { "Line 31 Voltage THD",    "%",    0x0068, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 40

    { "Phase 1 Current THD",    "%",    0x006A, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 41
    { "Phase 2 Current THD",    "%",    0x006C, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 42
    { "Phase 3 Current THD",    "%",    0x006E, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 43
    { "Neutral Current THD",    "%",    0x0070, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 44

    { "Frequency",              "Hz",   0x0072, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 45
    { "Phase Sequence",         "-",    0x0074, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 46

    // Demand values
    { "Phase 1 current DMD",                 "A",   0x010E, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 47
    { "Phase 2 current DMD",                 "A",   0x0110, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 48
    { "Phase 3 current DMD",                 "A",   0x0112, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 49
    { "Neutral current DMD",                 "A",   0x0114, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 50
    { "System current DMD",                  "A",   0x0116, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 51
    { "Phase 1 imported active power DMD",   "W",   0x0118, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 52
    { "Phase 1 exported active power DMD",   "W",   0x011C, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 53
    { "Phase 2 imported active power DMD",   "W",   0x0120, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 54
    { "Phase 2 exported active power DMD",   "W",   0x0124, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 55
    { "Phase 3 imported active power DMD",   "W",   0x0128, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 56
    { "Phase 3 exported active power DMD",   "W",   0x012C, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 57
    { "System imported active power DMD",    "W",   0x0130, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 58
    { "System exported active power DMD",    "W",   0x0134, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 59

    { "Phase 1 imported reactive power DMD", "var", 0x0160, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 60
    { "Phase 1 exported reactive power DMD", "var", 0x0164, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 61
    { "Phase 2 imported reactive power DMD", "var", 0x0168, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 62
    { "Phase 2 exported reactive power DMD", "var", 0x016C, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 63
    { "Phase 3 imported reactive power DMD", "var", 0x0170, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 64
    { "Phase 3 exported reactive power DMD", "var", 0x0174, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 65
    { "System imported reactive power DMD",  "var", 0x0178, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 66
    { "System exported reactive power DMD",  "var", 0x017C, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 67

    // MAX values
    { "Phase 1-N voltage MAX",               "V",   0x0200, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 68
    { "Phase 2-N voltage MAX",               "V",   0x0202, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 69
    { "Phase 3-N voltage MAX",               "V",   0x0204, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 70
    { "Line 12 voltage MAX",                 "V",   0x0206, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 71
    { "Line 23 voltage MAX",                 "V",   0x0208, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 72
    { "Line 31 voltage MAX",                 "V",   0x020A, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 73
    { "System voltage MAX",                  "V",   0x020C, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 74
    { "Phase 1 current MAX",                 "A",   0x020E, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 75
    { "Phase 2 current MAX",                 "A",   0x0210, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 76
    { "Phase 3 current MAX",                 "A",   0x0212, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 77
    { "Neutral current MAX",                 "A",   0x0214, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 78
    { "System current MAX",                  "A",   0x0216, 2, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 79

    { "Phase 1 imported active power DMD MAX",   "W",   0x02B4, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 80
    { "Phase 1 exported active power DMD MAX",   "W",   0x02B8, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 81
    { "Phase 2 imported active power DMD MAX",   "W",   0x02BC, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 82
    { "Phase 2 exported active power DMD MAX",   "W",   0x02C0, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 83
    { "Phase 3 imported active power DMD MAX",   "W",   0x02C4, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 84
    { "Phase 3 exported active power DMD MAX",   "W",   0x02C8, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 85
    { "System imported active power DMD MAX",    "W",   0x02CC, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 86
    { "System exported active power DMD MAX",    "W",   0x02D0, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 87

    { "Phase 1 imported reactive power DMD MAX", "var", 0x02F4, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 88
    { "Phase 1 exported reactive power DMD MAX", "var", 0x02F8, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 89
    { "Phase 2 imported reactive power DMD MAX", "var", 0x02FC, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 90
    { "Phase 2 exported reactive power DMD MAX", "var", 0x0300, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 91
    { "Phase 3 imported reactive power DMD MAX", "var", 0x0304, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 92
    { "Phase 3 exported reactive power DMD MAX", "var", 0x0308, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 93
    { "System imported reactive power DMD MAX",  "var", 0x030C, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 94
    { "System exported reactive power DMD MAX",  "var", 0x0310, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 95

    // MIN values
    { "System Active power MIN",   "W",   0x02D4, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 96
    { "System Apparent power MIN", "VA",  0x02D8, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 97
    { "System Reactive power MIN", "var", 0x02DC, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 98

    // Energy values
    { "Phase 1 imported active energy",            "Wh",   0x0400, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 99
    { "Phase 1 exported active energy",            "Wh",   0x0404, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 100
    { "Phase 2 imported active energy",            "Wh",   0x0408, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 101
    { "Phase 2 exported active energy",            "Wh",   0x040C, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 102
    { "Phase 3 imported active energy",            "Wh",   0x0410, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 103
    { "Phase 3 exported active energy",            "Wh",   0x0414, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 104
    { "System imported active energy",             "Wh",   0x0418, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 105
    { "System exported active energy",             "Wh",   0x041C, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 106
    { "Phase 1 imported active energy",            "Wh",   0x0400, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 107
    { "Phase 1 exported active energy",            "Wh",   0x0404, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 108
    { "Phase 2 imported active energy",            "Wh",   0x0408, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 109
    { "Phase 2 exported active energy",            "Wh",   0x040C, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 110
    { "Phase 3 imported active energy",            "Wh",   0x0410, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 111
    { "Phase 3 exported active energy",            "Wh",   0x0414, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 112
    { "System imported active energy",             "Wh",   0x0418, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 113
    { "System exported active energy",             "Wh",   0x041C, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 114
    { "Balance of system active energy (imp-exp)", "Wh",   0x0420, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 115

    { "Balance of system apparent energy (BAL-C + BAL-L)", "VAh",  0x048C, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 116
    { "Phase 1 imported capacitive reactive energy",       "varh", 0x0490, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 117
    { "Phase 1 exported capacitive reactive energy",       "varh", 0x0494, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 118
    { "Phase 1 imported inductive reactive energy",        "varh", 0x0498, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 119
    { "Phase 1 exported inductive reactive energy",        "varh", 0x049C, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 120
    { "Phase 2 imported capacitive reactive energy",       "varh", 0x04A0, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 121
    { "Phase 2 exported capacitive reactive energy",       "varh", 0x04A4, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 122
    { "Phase 2 imported inductive reactive energy",        "varh", 0x04A8, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 123
    { "Phase 2 exported inductive reactive energy",        "varh", 0x04AC, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 124

    { "Phase 3 imported capacitive reactive energy",       "varh", 0x04B0, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 125
    { "Phase 3 exported capacitive reactive energy",       "varh", 0x04B4, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 126
    { "Phase 3 imported inductive reactive energy",        "varh", 0x04B8, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 127
    { "Phase 3 exported inductive reactive energy",        "varh", 0x04BC, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 128

    { "System imported capacitive reactive energy",        "varh", 0x04C0, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 129
    { "System exported capacitive reactive energy",        "varh", 0x04C4, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 130
    { "System imported inductive reactive energy",         "varh", 0x04C8, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 131
    { "System exported inductive reactive energy",         "varh", 0x04CC, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f }, // 132

    // Experimental registers
    //{ "Balance of system capacitive reactive energy (imp-exp)", "varh", 0x04D0, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f },
    //{ "Balance of system inductive reactive energy (imp-exp)",  "varh", 0x04D4, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f },
    //{ "Balance of system reactive energy (BAL-C + BAL-L)",      "varh", 0x04D8, 4, TARGET_REGISTER_FUNC_READ_INPUT, 0.001f },
};

static const MultimeterRegisterSet s_target_register_set = {
    .registers = s_target_registers,
    .size = sizeof(s_target_registers) / sizeof(s_target_registers[0])
};

const MultimeterRegisterSet *ump209_get_target_register_set(void)
{
    return &s_target_register_set;
}
