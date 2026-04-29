# Processing Module

## Responsibility

The processing module turns stored raw Modbus acquisition cycles from the UPM209
meter into engineering measurements ready to be sent by the processing service.
It also smooths short windows of samples by rejecting statistical outliers before
computing the reported average, minimum, and maximum.

The module is intentionally device-aware: it uses the UPM209 target register map
from `upm209_get_target_register_set()` to know where each value lives in the raw
cycle, how many 16-bit words it uses, whether it is signed, what scale factor to
apply, and which unit/description should be attached to the result.

## Main Files

- `outlier_handling.h`: public data types and APIs.
- `outlier_handling.c`: layout generation, raw value decoding, and window
  filtering/statistics.

## Public API

- `processing_upm209_get_layout_info()`
  - Builds or reuses the cached register layout.
  - Returns the first expected register, the expected number of words in one raw
    acquisition cycle, and the number of measurements that will be produced.

- `processing_upm209_compute_window()`
  - Accepts an array of raw acquisition cycles.
  - Validates that every cycle has at least the expected number of words.
  - Decodes every configured UPM209 measurement from every cycle.
  - Filters outliers per measurement across the window.
  - Writes one `processing_measurement_t` per register-map entry.

## Data Flow

1. The Modbus acquisition path reads the UPM209 register map and stores each
   complete cycle as a flat array of 16-bit words.
2. The processing service waits until enough cycles are available for its window.
3. Each stored cycle is checked against the layout returned by
   `processing_upm209_get_layout_info()`.
4. `processing_upm209_compute_window()` converts raw words into scaled values.
5. For each measurement, the module computes filtered window statistics:
   average, minimum, and maximum.
6. The processing service serializes the output as JSON using the register
   number, average, word count, min, max, unit, and description.

## Layout Algorithm

The layout is built once and cached in static memory.

The algorithm walks the ordered UPM209 register map and groups adjacent or
overlapping registers into read blocks when they use the same Modbus function
code. A block can grow only while its merged size remains within the Modbus
request limit of 125 words.

For every register entry, the module stores:

- A pointer to the register metadata.
- The word offset of that register inside the flattened acquisition cycle.

The resulting layout tells the processing code how to map a raw cycle like:

```text
cycle words: [block 0 words][block 1 words][block 2 words]...
```

back to individual measurements from the register map.

Important limits enforced by the layout builder:

- Up to 512 register-map entries.
- Each register value must use 1 to 125 words, though decoding currently accepts
  values up to 4 words.
- A merged Modbus request block cannot exceed 125 words.
- The final flattened cycle length must fit in a `uint16_t`.

## Decoding Algorithm

Each measurement is decoded from one to four 16-bit Modbus words.

Words are combined in big-endian order:

```text
raw = word[0] << 16*(n - 1) | word[1] << 16*(n - 2) | ...
```

Unsigned values are converted directly:

```text
value = raw * scale
```

Signed UPM209 values use sign-bit plus magnitude encoding:

```text
sign bit = most significant bit of the combined raw value
magnitude = remaining bits
signed_value = -magnitude when sign bit is set, otherwise magnitude
value = signed_value * scale
```

The scale, signedness flag, unit, and human-readable description all come from
the `MultimeterRegister` entry in the UPM209 register table.

## Outlier Filtering Algorithm

Filtering is done independently for each measurement across the sample window.
For example, all decoded "Phase 1 Current" values from the window are filtered
together, while voltage, power, frequency, and other measurements are filtered in
their own independent groups.

The module uses the interquartile range (IQR) rule:

1. Copy the decoded values for one measurement.
2. Sort the copy in ascending order with `qsort()`.
3. If there are at least 4 samples, compute:
   - `Q1`: 25th percentile.
   - `Q3`: 75th percentile.
   - `IQR = Q3 - Q1`.
4. Build the accepted range:
   - `low = Q1 - 1.5 * IQR`.
   - `high = Q3 + 1.5 * IQR`.
5. Keep only samples inside `[low, high]`.
6. Compute the mean, min, and max from the kept samples.

Percentiles are computed with linear interpolation between sorted samples. For a
percentile `p`, the position is:

```text
pos = p * (sample_count - 1)
```

The lower and upper neighboring samples are blended according to the fractional
part of `pos`.

For windows with fewer than 4 samples, the module skips IQR filtering and uses
all samples. If every sample would be rejected by the IQR bounds, it falls back
to all samples so the caller still receives a measurement instead of an empty
statistic.

## Output Semantics

Each `processing_measurement_t` contains:

- `num_reg`: source Modbus register address.
- `word`: number of 16-bit words used by the raw register value.
- `value`: filtered average over the window.
- `min`: minimum kept value after filtering.
- `max`: maximum kept value after filtering.
- `unit`: unit string from the register map, or an empty string.
- `description`: register name from the register map, or an empty string.

The processing module does not allocate or own the unit/description strings; it
returns pointers to the static register metadata.

## Error Handling

The module returns ESP-IDF error codes:

- `ESP_ERR_INVALID_ARG` for null pointers, empty windows, or invalid decode
  arguments.
- `ESP_ERR_INVALID_SIZE` for unsupported window sizes, insufficient output
  capacity, invalid cycle shape, or layout sizes that exceed internal limits.
- `ESP_ERR_INVALID_STATE` when the UPM209 register set or cached layout is not
  usable.

The maximum processing window is 64 samples. The caller chooses the actual
window size, but it must be non-zero and no larger than that limit.
