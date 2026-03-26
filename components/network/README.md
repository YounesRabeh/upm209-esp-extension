# Network Module

## Responsibility
- Provide a single internet access layer for the application.
- Select and manage the active interface (WiFi, LTE, or fallback logic).
- Send JSON payloads to the configured HTTP endpoint.

## How It Works
- `internet_init()` initializes NVS, ESP network stack, and enabled interfaces.
- `internet_connect()` follows Kconfig preference:
  - `WIFI_ONLY`
  - `LTE_ONLY`
  - `AUTO` (WiFi first, LTE fallback)
- `internet_send_data()` posts JSON to `CONFIG_INTERNET_TARGET_URL`.
- If sending fails, it reconnects once and retries a single resend.

## Main Files
- `internet.h`
- `internet.c`
- `internet_send.c`
- `Kconfig`
