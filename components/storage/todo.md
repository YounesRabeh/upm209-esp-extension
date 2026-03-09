The storage component manages persistent or temporary data storage. It abstracts memory handling and flash storage mechanisms.

Goals

Store measurements locally

Provide read/write interfaces

Handle buffering when network is unavailable

Abstract ESP-IDF storage mechanisms, LittleFS



134 registri
un salvataggio e 536 B RAW
Storage per 10 minuti
lettura 1 campione ogni 2 secondi [10 minuti = 600 s]
540 × 300 = 162000 B
Scenario

memory	size	use
Flash	8 MB	firmware + filesystem
PSRAM	8 MB	runtime memory
Internal RAM	~512 KB	fast memory

Files

memory.c
memory.h


Il modulo storage è una coda persistente su LittleFS, pensata per buffering offline dei campioni Modbus.

Riferimenti:

API: memory.h
Implementazione: memory.c
Manager periodico: memory_manager.c
Config: Kconfig
Come funziona:

Init filesystem
memory_init() monta LittleFS su CONFIG_MEMORY_LITTLEFS_MOUNT_POINT (default /lfs).
Usa label CONFIG_MEMORY_LITTLEFS_PARTITION_LABEL (default storage), con fallback auto-detect.
Crea directory /lfs/modbus e file:
data.bin (buffer circolare)
meta.bin (stato coda)
Formato dati
Ogni campione è un record binario:
header (magic, timestamp, slave, start_reg, reg_count, CRC payload)
payload: array uint16_t registri Modbus
Così usa meno spazio del JSON.
Coda circolare persistente
In meta.bin mantiene:
head (record più vecchio)
tail (prossima scrittura)
used (byte usati)
count (numero campioni)
capacity (dimensione totale coda)
Write path
memory_enqueue_modbus_sample(...):
valida input
calcola CRC
assicura spazio libero
scrive record in data.bin
aggiorna metadati
Read path
memory_peek_modbus_sample(...): legge il più vecchio senza rimuoverlo
memory_pop_modbus_sample(...): legge e consuma il più vecchio
Controlla CRC e coerenza record.
Overflow policy
Configurabile in Kconfig:
MEMORY_OVERFLOW_OVERWRITE_OLDEST: quando piena sovrascrive i dati più vecchi
MEMORY_OVERFLOW_REJECT_NEWEST: rifiuta nuovi campioni (ESP_ERR_NO_MEM)
Concorrenza e robustezza
Mutex interno (s_lock) su tutte le operazioni
Recovery su metadati corrotti/troncati (reset coda)
Marker di wrap per gestire scritture a fine buffer
Manager storage
memory_manager_start():
chiama memory_init()
opzionalmente avvia task monitor periodico (log di pending/used)