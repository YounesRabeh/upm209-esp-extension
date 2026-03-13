idf_component_register(
    SRCS 
        "modbus_io.c"
        "modbus_master.c"
        "modbus_manager.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        driver
        devices
        storage
        utils
        esp-modbus
)

set(COMPONENT_KCONFIG "Kconfig")
