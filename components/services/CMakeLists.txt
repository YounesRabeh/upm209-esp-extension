idf_component_register(
    SRCS 
        "time_service.c"
        "sampling_service.c"
        "processing_service.c"
        "services_manager.c"
    INCLUDE_DIRS 
        "."
    REQUIRES
        network
        modbus
        processing
        storage
        utils
        esp_app_format
        esp_hw_support
        esp_netif
)

set(COMPONENT_KCONFIG "Kconfig")
