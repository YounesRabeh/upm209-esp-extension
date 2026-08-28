# Security Policy

## Supported versions

This project does not currently publish release versions. Security fixes are currently applied to
the default development branch.

## Reporting a vulnerability

Please report suspected vulnerabilities privately through
[GitHub Security Advisories](https://github.com/YounesRabeh/upm209-esp-extension/security/advisories/new).
Include enough detail to reproduce the issue, the affected component, and any relevant ESP-IDF or
board configuration.

Do not disclose a vulnerability publicly until it has been reviewed. If private reporting is not
available, open a minimal public issue without credentials, exploit details, or other sensitive
information and request a private reporting channel.

## Sensitive information

Never include real values for any of the following in an issue, pull request, log, or reproduction:

- `CONFIG_WIFI_SSID`
- `CONFIG_WIFI_PASSWORD`
- `CONFIG_INTERNET_TARGET_URL`
- Device identifiers, tokens, or other deployment credentials

Replace sensitive values with safe placeholders before sharing configuration or logs.
