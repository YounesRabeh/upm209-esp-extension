# WiFi Module

## Responsibility
- Manage WiFi station initialization, connection, retry behavior, and status.
- Support configured authentication mode (open, WPA2-PSK, WPA2-Enterprise).

## How It Works
- `wifi_init()` creates STA netif, starts WiFi driver, and registers event handlers.
- Event handlers maintain connection state in an event group (`WIFI_CONNECTED_BIT`).
- `wifi_connect_retry()`:
  - Checks whether target SSID is visible.
  - Applies enterprise auth setup when enabled.
  - Attempts connection with timeout/retry/delay policy.
- `wifi_disconnect()` and `wifi_is_connected()` expose control/state helpers.

## Main Files
- `wifi.h`
- `wifi.c`
