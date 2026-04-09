# Processing Module

## Responsibility
- Convert raw Modbus cycle words into engineering measurements.
- Filter noisy/outlier values across a window of sampled cycles.

## How It Works
- Builds a deterministic layout from the target register map (`upm209_get_target_register_set()`).
- Decodes each register value from raw words using scale and signedness metadata.
- Computes window statistics per measurement using IQR-based filtering.
- Returns processed output with average, min, max, unit, and description.

## Main Files
- `outlier_handling.h`
- `outlier_handling.c`
