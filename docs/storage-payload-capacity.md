# Storage Capacity and Payload Modes

[< Back to Documentation Hub](README.md) · [Storage module](../components/storage/README.md) · [Main README](../README.md)

The firmware stores raw Modbus cycles in its LittleFS-backed circular queue. It does not store generated JSON payloads: a payload is built from a window of six raw cycles immediately before an HTTP upload.

## Current queue settings

- Poll interval: **10 seconds**
- Raw-cycle queue capacity: **262,144 bytes (256 KiB)**
- Upload window: **6 cycles**
- Overflow policy: **overwrite the oldest cycle**
- Maximum allocated JSON buffer: **256 KiB**

`UPM209_SIMPLE_SAMPLING` in [`components/devices/upm209/upm209.c`](../components/devices/upm209/upm209.c) selects the register map and, consequently, the payload size. Changing it requires a rebuild and flash.

## Capacity by payload mode

| Item | Full JSON (`UPM209_SIMPLE_SAMPLING=0`) | Small JSON (`UPM209_SIMPLE_SAMPLING=1`) |
| --- | ---: | ---: |
| Measurements in each generated JSON payload | **397** | **8** |
| Modbus words in one stored raw cycle | **1,084** | **24** |
| Stored bytes per cycle | **2,184 B** | **64 B** |
| Raw cycles retained | **120** | **4,096** |
| Time retained at 10 s polling | **20 minutes** | **11 h 22 min 40 s** |
| Samples per upload packet/window | **6 cycles** | **6 cycles** |
| Complete upload windows retained | **20 packets** | **682 packets** (+ 4 cycles) |
| Normal upload interval | **60 seconds** | **60 seconds** |
| JSON saved directly to flash | **No — raw cycles only** | **No — raw cycles only** |

## How the capacity is calculated

Every raw record contains a 16-byte header followed by two bytes for each Modbus word:

```text
stored record bytes = 16 + (Modbus words × 2)
```

For the full map this is `16 + (1,084 × 2) = 2,184 B`; `floor(262,144 / 2,184)` gives 120 cycles. For the small map it is `16 + (24 × 2) = 64 B`; `262,144 / 64` gives 4,096 cycles.

## Persistence note

The current sampling service is configured for development and clears the persisted queue at startup (`SS_STARTUP_CLEAR_PERSISTED` is `1` in [`components/services/sampling_service.c`](../components/services/sampling_service.c)). The capacity above therefore covers a network outage while the device remains powered, but not a reboot or power loss. Set that switch to `0` for release firmware that must preserve queued cycles across restarts.
