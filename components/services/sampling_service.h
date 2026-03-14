#pragma once

#include <stdbool.h>

#include "esp_err.h"

esp_err_t sampling_service_start(void);

bool sampling_service_is_running(void);

