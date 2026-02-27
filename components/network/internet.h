#pragma once
#include <stdbool.h>
#include "esp_err.h"

/* Tipo di interfaccia */
typedef enum {
    INTERNET_IF_NONE = -1,
    INTERNET_IF_WIFI,
    INTERNET_IF_LTE
} internet_if_t;

/**
 * @brief Inizializza lo stack di rete
 */
esp_err_t internet_init(void);

/**
 * @brief Connette l’interfaccia specificata (WiFi o LTE)
 */
esp_err_t internet_connect(void);


/**
 * @brief Restituisce true se la rete è connessa
 */
bool internet_is_connected(void);

/**
 * @brief Restituisce l'interfaccia attiva dopo internet_connect()
 */
internet_if_t internet_active_interface(void);
