#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t modbus_manager_start(void);
bool modbus_manager_is_running(void);
