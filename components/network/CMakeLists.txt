idf_component_register(
    SRCS 
        "internet.c"
        "internet_send.c"
    INCLUDE_DIRS 
        "."
    REQUIRES
        utils
    PRIV_REQUIRES 
        lte
        wifi
        esp_wifi
        esp_http_client
        esp-tls
        nvs_flash
)

# Tell ESP-IDF where the Kconfig is
set(COMPONENT_KCONFIG "Kconfig")
