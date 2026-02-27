#pragma once

#include "esp_err.h"
#include <stdbool.h>

esp_err_t lte_init(void);
esp_err_t lte_connect(void);
bool lte_is_connected(void);