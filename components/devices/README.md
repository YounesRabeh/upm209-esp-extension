# Devices Module

## Responsibility
- Define device-specific register metadata used by acquisition and processing layers.
- Expose a stable register-set API for the target multimeter model.

## How It Works
- `target_registers.h` defines generic register descriptors (`MultimeterRegister`, `MultimeterRegisterSet`).
- `ump209/ump209.c` builds the UPM209 register table (full map or reduced map via compile-time flag).
- `ump209_get_target_register_set()` returns a constant register set used by `modbus` and `processing`.

## Main Files
- `target_registers.h`
- `ump209/ump209.h`
- `ump209/ump209.c`
- `ump209/ump209_full_registers.inc`
