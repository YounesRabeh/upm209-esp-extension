Description

The devices component represents physical devices connected to the system. It translates raw protocol data into meaningful physical measurements.

It acts as a hardware abstraction layer between protocol and application logic.

Goals

Represent real hardware devices

Convert raw registers into physical values

Provide device-specific APIs

Hide protocol details from upper layers

Files
target_reg



Window = 10 campioni (1 secondo)

Pipeline:
1) Validazione
2) Median-of-3
3) Buffer 10 valori
4) Trimmed mean (salva  min e max)
5) Peak confirm ≥2 campioni

-----------

Hampel  - troppo forte