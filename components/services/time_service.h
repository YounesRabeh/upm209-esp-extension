#pragma once

#include <stdbool.h>
#include <time.h>
#include "esp_err.h"

esp_err_t time_service_sync_once(const char *tz, int timeout_ms);
esp_err_t time_service_init(void);
esp_err_t time_service_sync_wait(int timeout_ms);
esp_err_t time_service_set_timezone(const char *tz);
esp_err_t time_service_get_local_time(struct tm *out_tm);
bool time_service_is_synchronized(void);
