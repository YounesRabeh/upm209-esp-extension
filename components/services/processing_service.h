#pragma once

#include <stdbool.h>

#include "esp_err.h"

esp_err_t processing_service_start(void);

bool processing_service_is_running(void);
