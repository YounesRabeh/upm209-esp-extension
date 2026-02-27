idf_component_register(
    SRCS 
        "internet.c"
    INCLUDE_DIRS 
        "."
    REQUIRES
        lte
        wifi
        utils
    PRIV_REQUIRES 
        esp_wifi
        esp_http_client
        nvs_flash
)

# Tell ESP-IDF where the Kconfig is
set(COMPONENT_KCONFIG "Kconfig")
