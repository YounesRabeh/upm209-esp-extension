idf_component_register(
    SRCS 
        "time_service.c"
        "services_manager.c"
    INCLUDE_DIRS 
        "."
    REQUIRES
        devices 
        network
        modbus
        storage
        utils
        esp_netif
)

set(COMPONENT_KCONFIG "Kconfig")
