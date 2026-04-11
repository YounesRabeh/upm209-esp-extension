# UPM209 ESP Extension

Firmware ESP-IDF per `ESP32-S3` che legge misure elettriche da un contatore UPM209 via Modbus RTU, salva i campioni grezzi in LittleFS, elabora una finestra scorrevole con gestione outlier e invia payload JSON normalizzati a un endpoint HTTP remoto.

## Funzionalita

- Campionamento periodico Modbus RTU della mappa registri UPM209 (default: range completo fino a `0x063E`)
- Buffer dei campioni grezzi in RAM + coda persistente su LittleFS (partizione `storage`)
- Elaborazione a finestra (6 campioni) con filtro outlier basato su IQR
- Generazione payload JSON con metadati dispositivo (device id da MAC, versione firmware, timestamp)
- Upload HTTP POST con logica di riconnessione e retry
- Selezione modalita rete: `WiFi only`, `LTE only` oppure `AUTO (WiFi -> LTE fallback)`
- Avvio centralizzato dei servizi e logging colorato custom

## Pipeline Dati

1. `modbus_manager` legge i blocchi UPM209 ogni 10 secondi.
2. `sampling_service` riceve le word grezze e le mette nella memoria persistente.
3. `processing_service` aspetta 6 campioni, calcola min/avg/max per misura e costruisce il JSON.
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
  - Periodo polling: `10000 ms`
- Finestra processing: `6` campioni
- Capacita coda LittleFS: `262144` byte (configurabile)

## Avvio Rapido

### 1) Prerequisiti

- ESP-IDF installato (consigliato: `v5.4.x`)
- Board/toolchain ESP32-S3 funzionante (`idf.py --version`)
- Contatore UPM209 collegato tramite trasceiver RS485

### 2) Configurazione

Il target base del progetto e `esp32s3` con flash da `8MB`.
Se la scheda in uso non dispone di `8MB`, aggiorna `partitions.csv` e la configurazione flash prima del build.
Dopo il clone, usa sempre `idf.py set-target esp32s3` prima di `idf.py menuconfig` o `idf.py build`.

```bash
idf.py set-target esp32s3
idf.py menuconfig
```

### Come funziona `sdkconfig`

> [!NOTE]
> Il repository include `sdkconfig.defaults`, che contiene il frame condiviso del progetto:
> target `esp32s3`, flash `8MB`, partition table, servizi abilitati e default non sensibili.
> Dopo il clone, ESP-IDF usa questo file come base per generare il tuo `sdkconfig` locale.

> [!WARNING]
> `sdkconfig` e `sdkconfig.old` sono file locali e non vanno pushati.
> Possono contenere dati sensibili in chiaro, ad esempio:
> `CONFIG_INTERNET_TARGET_URL`, `CONFIG_WIFI_SSID`, `CONFIG_WIFI_PASSWORD`.

> [!TIP]
> Dopo il clone, apri `idf.py menuconfig` e imposta almeno questi campi:
> - `Internet Configuration -> Remote Internet target URL`
> - `Internet Configuration -> WiFi SSID`
> - `Internet Configuration -> WiFi password`
> - opzionalmente la modalita rete (`AUTO`, `WiFi only`, `LTE only`)
>
> Se `sdkconfig` non esiste ancora, verra creato automaticamente a partire da `sdkconfig.defaults` durante `menuconfig` o `idf.py build`.

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

### Switch Rapidi (simple/full, dev on/off, ...)

Alcuni switch sono in `menuconfig`, altri sono compile-time nel codice.
Dopo ogni modifica compile-time, ricompila sempre il firmware con `idf.py build`.

#### 1) Register set UPM209: `simple` vs `all registers`

> [!TIP]
> File: `components/devices/upm209/upm209.c`
> - `#define UPM209_SIMPLE_SAMPLING 1U`: modalita `simple` (subset ridotto di registri)
> - `#define UPM209_SIMPLE_SAMPLING 0U`: modalita `all registers` (set completo da `upm209_full_registers.inc`)

#### 2) Dev mode storage: `ON` vs `OFF`

> [!WARNING]
> File: `components/services/sampling_service.c`
> - `#define SS_STARTUP_CLEAR_PERSISTED 1`: Dev `ON`, svuota la coda persistente ad ogni boot
> - `#define SS_STARTUP_CLEAR_PERSISTED 0`: Dev `OFF`, mantiene in coda i campioni non inviati dopo reboot/reset

#### 3) Modbus debug verboso: `ON` vs `OFF`

File: `components/modbus/modbus_manager.c`
- `#define MB_VERBOSE_DEBUG 1`: log dettagliati su fallback/chunk/recovery
- `#define MB_VERBOSE_DEBUG 0`: log ridotti (default consigliato)

#### 4) Switch via `menuconfig` (senza toccare codice)

> [!NOTE]
> Percorsi:
> - Rete: `Internet Configuration -> Preferred network type` (`AUTO`, `WiFi only`, `LTE only`)
> - Servizi: `Services Configuration` (`INTERNET_SERVICE_ENABLE`, `TIME_SERVICE_ENABLE`, `STORAGE_SERVICE_ENABLE`, `MODBUS_SERVICE_ENABLE`)

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
│   ├── devices/upm209/           # Definizioni mappa registri UPM209
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
- Per switch rapidi (`simple/full`, `dev on/off`, debug verboso, rete/servizi) vedi la sezione `Switch Rapidi`.

## Riferimenti Utili

- Schema payload UPM209: `docs/JSON_schema_UPM209.json`
- Report discovery registri: `reports/upm209_register_discovery_report.md`
- Report validazione register-set: `reports/upm209_regset_validation_test_report.md`
