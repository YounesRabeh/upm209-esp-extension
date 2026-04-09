# Devices Module

## Responsibility
- Define device-specific register metadata used by acquisition and processing layers.
- Expose a stable register-set API for the target multimeter model.

## How It Works
- `target_registers.h` defines generic register descriptors (`MultimeterRegister`, `MultimeterRegisterSet`).
- `upm209/upm209.c` builds the UPM209 register table (full map or reduced map via compile-time flag).
- `upm209_get_target_register_set()` returns a constant register set used by `modbus` and `processing`.

## Main Files
- `target_registers.h`
- `upm209/upm209.h`
- `upm209/upm209.c`
- `upm209/upm209_full_registers.inc`
