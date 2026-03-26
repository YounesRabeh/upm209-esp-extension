# Storage Module

## Responsibility
- Persist raw Modbus samples reliably on LittleFS.
- Expose queue-like APIs for enqueue, peek, pop, and clear operations.

## How It Works
- `memory.c` implements a persistent circular queue using:
  - `data.bin` for records
  - `meta.bin` for head/tail/count metadata
- Each stored record includes metadata and CRC to detect corrupted payloads.
- Reads and writes are guarded by a mutex for task safety.
- `memory_manager.c` provides an optional async ingest layer with queue + writer task.

## Main Files
- `memory.h` / `memory.c`
- `memory_manager.h` / `memory_manager.c`
- `Kconfig`
