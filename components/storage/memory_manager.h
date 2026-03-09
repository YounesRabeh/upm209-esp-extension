#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t memory_manager_start(void);
bool memory_manager_is_running(void);
