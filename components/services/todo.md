The services component is the system orchestration layer. It coordinates device acquisition, data processing, storage, and communication.

This is the central control layer of the application.

Goals

Control acquisition timing

Coordinate data flow between components

Trigger processing operations

Send data to network

Store data when required

Files

services_manager.c
time_service.c

Periodic logic moved to:
- components/modbus/modbus_manager.c
- components/storage/memory_manager.c
