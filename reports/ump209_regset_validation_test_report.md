# UPM209 Register-Set Validation Test Report

## Scope
- Device: UPM209
- Protocol: Modbus RTU
- Slave: 1
- Validation target: `components/devices/ump209/ump209.c`
- Test mode: one-shot register-set validation

## Firmware Behavior
At boot, Modbus manager now validates each register entry in the UPM209 register set:
- one Modbus request per entry (`fc/start/count` from table)
- collects pass/fail
- prints final summary and failure list

Key log markers:
- `TEST REPORT START`
- `TEST SUMMARY: total=... ok=... fail=... skipped=...`
- `FAIL [...] ...` (only on failures)
- `ALL UPM209 REGISTER-SET ENTRIES RESPONDED` (all-pass case)
- `TEST REPORT END`

## Execution Result
Status: Pending hardware run

Paste runtime summary here:
- total: 
- ok: 
- fail: 
- skipped: 

If failures exist, paste failure lines:
- 

## Conclusion
- PASS condition: `fail=0` and `skipped=0`
- FAIL condition: any non-zero `fail` or `skipped`

## Notes
- This environment cannot execute hardware Modbus I/O, so on-device run is required for final verdict.
