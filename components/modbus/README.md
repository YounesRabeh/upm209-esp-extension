# Modbus Module

## Responsibility
- Manage Modbus RTU communication with the target meter over UART/RS485.
- Perform periodic sampling cycles and publish raw cycle data to a sink callback.

## How It Works
- `modbus_io.*` translates local UART/link settings into ESP Modbus controller settings.
- `modbus_master.*` initializes ESP Modbus master and exposes low-level register read/probe/recover APIs.
- `modbus_manager.*`:
  - Initializes the master.
  - Periodically reads register blocks defined by the UPM209 register set.
  - Applies retry and chunked fallback on read failures.
  - Publishes one sampled cycle through `modbus_manager_set_sample_sink(...)`.

## Main Files
- `modbus_io.h` / `modbus_io.c`
- `modbus_master.h` / `modbus_master.c`
- `modbus_manager.h` / `modbus_manager.c`
- `Kconfig`
