Description

The modbus component implements the Modbus protocol communication layer. It handles low-level communication with Modbus devices including frame construction, register access, and error handling.

It must remain independent from device logic.

Goals

Implement Modbus RTU communication

Provide register read/write APIs

Handle protocol errors and retries

Abstract UART communication details

Files

modbus_master.c

modbus_master.h


