#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Inizializza il modem LTE
 */
esp_err_t lte_init(void);

/**
 * @brief Connette alla rete LTE
 */
esp_err_t lte_connect(void);

/**
 * @brief Restituisce true se la rete LTE è connessa
 */
bool lte_is_connected(void);