# LTE Module

## Responsibility
- Provide a simple LTE connectivity interface to the rest of the system.
- Hide LTE backend details behind a small API (`init`, `connect`, `status`).

## How It Works
- `lte_init()` and `lte_connect()` are currently stub implementations.
- On successful `lte_connect()`, the module stores an internal connected flag.
- `lte_is_connected()` returns that internal flag.

## Main Files
- `lte.h`
- `lte.c`
