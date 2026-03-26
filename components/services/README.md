# Services Module

## Responsibility
- Orchestrate runtime services and background tasks.
- Coordinate data flow from acquisition to processing to upload.

## How It Works
- `services_manager` starts services in order, based on Kconfig enables.
- `time_service` performs SNTP-based clock sync.
- `sampling_service` registers as Modbus sink, buffers incoming samples in RAM queue, then persists to storage.
- `processing_service`:
  - Reads windows of stored samples.
  - Validates expected register layout.
  - Runs processing/window aggregation.
  - Builds JSON payloads and uploads through `network`.
  - Removes consumed samples after successful send.

## Main Files
- `services_manager.h` / `services_manager.c`
- `time_service.h` / `time_service.c`
- `sampling_service.h` / `sampling_service.c`
- `processing_service.h` / `processing_service.c`
- `Kconfig`
