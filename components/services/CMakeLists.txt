idf_component_register(
    SRCS 
        "modbus_service.c"
        "time_service.c"
        "services_manager.c"
    INCLUDE_DIRS 
        "."
    REQUIRES
        devices 
        network
        storage
        utils
        esp_netif
)

set(COMPONENT_KCONFIG "Kconfig")
