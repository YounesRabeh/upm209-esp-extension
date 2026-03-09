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