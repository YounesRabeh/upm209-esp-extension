# UPM209 ESP Extension

Firmware ESP-IDF per `ESP32-S3` che legge misure elettriche da un contatore UPM209 via Modbus RTU, salva i campioni grezzi in LittleFS, elabora una finestra scorrevole con gestione outlier e invia payload JSON normalizzati a un endpoint HTTP remoto.

## Funzionalita

- Campionamento periodico Modbus RTU della mappa registri UPM209 (default: range completo fino a `0x063E`)
- Buffer dei campioni grezzi in RAM + coda persistente su LittleFS (partizione `storage`)
- Elaborazione a finestra (3 campioni) con filtro outlier basato su IQR
- Generazione payload JSON con metadati dispositivo (device id da MAC, versione firmware, timestamp)
- Upload HTTP POST con logica di riconnessione e retry
- Selezione modalita rete: `WiFi only`, `LTE only` oppure `AUTO (WiFi -> LTE fallback)`
- Avvio centralizzato dei servizi e logging colorato custom

## Pipeline Dati

1. `modbus_manager` legge i blocchi UPM209 ogni 2 secondi.
2. `sampling_service` riceve le word grezze e le mette nella memoria persistente.
3. `processing_service` aspetta 3 campioni, calcola min/avg/max per misura e costruisce il JSON.
4. `internet_send` invia il payload a `CONFIG_INTERNET_TARGET_URL`.
5. Se l'invio va a buon fine, i campioni consumati vengono rimossi dalla coda.

## Default Runtime Attuali

- Target: `esp32s3`
- ESP-IDF nel lockfile: `5.4.1`
- Modbus (hardcoded in `components/modbus/modbus_manager.c`):
  - Porta UART: `1`
  - TX: `GPIO7`
  - RX: `GPIO8`
  - RTS/DE: `GPIO4`
  - Baud: `19200`
  - Parita: `none`
  - Indirizzo slave: `1`
  - Periodo polling: `2000 ms`
- Finestra processing: `3` campioni
- Capacita coda LittleFS: `262144` byte (configurabile)
- URL upload di default:
  - `https://blockboxchain-api.beesoft.it/saveData`

## Avvio Rapido

### 1) Prerequisiti

- ESP-IDF installato (consigliato: `v5.4.x`)
- Board/toolchain ESP32-S3 funzionante (`idf.py --version`)
- Contatore UPM209 collegato tramite trasceiver RS485

### 2) Configurazione

```bash
idf.py set-target esp32s3
idf.py menuconfig
```

Sezioni menu principali:

- `Internet Configuration`
  - `INTERNET_TARGET_URL`
  - Modalita rete (`AUTO`, `WIFI_ONLY`, `LTE_ONLY`)
  - Autenticazione WiFi e credenziali
- `Services Configuration`
  - Abilita/disabilita internet, time, storage e modbus
- `Storage Configuration`
  - Dimensione coda, max registri, policy overflow
- `Modbus-module Configuration`
  - Abilita/disabilita Modbus manager

### 3) Build e flash

```bash
idf.py build
idf.py -p <PORT> flash monitor
```

## Struttura Progetto

```
.
├── main/                         # app_main e startup
├── components/
│   ├── devices/ump209/           # Definizioni mappa registri UPM209
│   ├── modbus/                   # Manager Modbus RTU e I/O
│   ├── storage/                  # Coda persistente su LittleFS
│   ├── processing/               # Calcolo finestra + gestione outlier
│   ├── network/                  # Init/connect/send internet
│   ├── wifi/                     # Gestione connessione/autenticazione WiFi
│   ├── lte/                      # Astrazione LTE (attualmente stub)
│   ├── services/                 # Orchestrazione servizi e task
│   └── utils/                    # Utility di logging
├── docs/                         # Schema payload e documenti di riferimento
└── partitions.csv                # Include partizione LittleFS "storage" da 4MB
```

## Formato Payload

Schema di riferimento: `docs/JSON_schema_UPM209.json`.

Esempio:

```json
{
  "schemaID": "schemaUNICAM",
  "companyID": "UNICAM",
  "timestamp": 1710000000,
  "device_id": "A1B2C3D4E5F6",
  "firmware_version": "1",
  "device_type": "UPM209",
  "measurements": [
    {
      "num_reg": 24,
      "avg": 1234.5,
      "word": 4,
      "min": 1229.1,
      "max": 1238.0,
      "unit": "W",
      "description": "System Active Power"
    }
  ]
}
```

## Note e Limitazioni

- LTE e attualmente una implementazione stub (`components/lte/lte.c`) e non controlla ancora un modem reale.
- I log di default ESP-IDF sono silenziati in `app_main`; usa i log `LOG_*` del progetto per la diagnostica.
- La scelta del register-set e compile-time in `components/devices/ump209/ump209.c`:
  - `UPM209_SIMPLE_SAMPLING = 0` -> set completo (`ump209_full_registers.inc`)
  - `UPM209_SIMPLE_SAMPLING = 1` -> subset ridotto focalizzato su carichi AC

## Riferimenti Utili

- Schema payload UPM209: `docs/JSON_schema_UPM209.json`
- Report discovery registri: `reports/ump209_register_discovery_report.md`
- Report validazione register-set: `reports/ump209_regset_validation_test_report.md`
