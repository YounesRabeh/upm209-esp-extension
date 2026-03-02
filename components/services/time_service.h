#pragma once

#include <stdbool.h>
#include <time.h>
#include "esp_err.h"

/** 
 * @brief Synchronizes the system time with an NTP server once.
 * @param tz Timezone string (e.g., "UTC", "CST-8") to set before synchronization.
 * @param timeout_ms Maximum time to wait for synchronization in milliseconds.
 * @return ESP_OK on success, or an error code on failure/timeout.
 */
esp_err_t time_service_sync_once(const char *tz, int timeout_ms);

/** 
 * @brief Initializes the time service and starts SNTP.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t time_service_init(void);

/** 
 * @brief Waits for time synchronization to complete, with a timeout.
 * @param timeout_ms Maximum time to wait in milliseconds.
 * @return ESP_OK if time is synchronized, or an error code on failure/timeout.
 */
esp_err_t time_service_sync_wait(int timeout_ms);

/**
 * @brief Sets the system timezone.
 * @param tz Timezone string (e.g., "UTC", "CST-8").
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t time_service_set_timezone(const char *tz);

/**
 * @brief Checks if the time has been synchronized.
 * @return true if time is synchronized, false otherwise.
 */
esp_err_t time_service_get_local_time(struct tm *out_tm);

/** 
 * @brief Checks if the time has been synchronized.
 * @return true if time is synchronized, false otherwise.
 */
bool time_service_is_synchronized(void);
