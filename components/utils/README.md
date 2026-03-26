# Utils Module

## Responsibility
- Provide shared utility primitives used across modules.
- Currently centralize formatted, thread-safe logging.

## How It Works
- `logging.h` exposes colorized `LOG_*` macros (`INFO`, `DEBUG`, `OK`, `WARN`, `ERROR`).
- `logging.c` implements a mutex-protected print path to avoid interleaved logs across tasks.
- If system time is available, logs include a timestamp; otherwise they log without one.

## Main Files
- `logging.h`
- `logging.c`
