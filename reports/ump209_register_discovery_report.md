# UPM209 Register Discovery Report (One-Shot)

## Scope
- Device: UPM209
- Protocol: Modbus RTU, FC 0x04 (Input Registers)
- Slave: 1
- Scan range: 0x0000..0x063E
- Mode: one-shot (single scan at boot)

## Method
- One request per register address (`reg_count=1`).
- Retries enabled on read failure.
- Registers that respond are logged as:
  - `REG OK: addr=0x.... value=0x.... (...)`
- Contiguous valid regions are summarized as:
  - `REG BLOCK: 0x.... .. 0x.... (N regs)`
- Final summary line:
  - `DISCOVERY REPORT END: scanned=... found=... blocks=...`

## Run Output
Paste here the `REG BLOCK` lines and final `DISCOVERY REPORT END` from device logs.

### Discovered Blocks
- TODO

### Summary
- scanned: TODO
- found: TODO
- blocks: TODO

## Notes
- This discovery mode intentionally ignores `components/devices/ump209/ump209.c` register map.
- Use this report to build/validate the final static register table.
