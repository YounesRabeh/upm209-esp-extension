#pragma once

#include <stdbool.h>
#include "esp_err.h"

/** 
 * @brief Memory manager component for handling storage of Modbus samples in memory. This component provides functions to initialize the memory system, enqueue Modbus samples, and check the status of the memory. It is used by the Modbus manager component to store sampled Modbus data for later retrieval and transmission. The memory manager abstracts the underlying storage mechanism and provides a simple interface for managing Modbus samples in memory.
 * @returns ESP_OK on successful start, or an appropriate error code on failure. The memory manager must be started before it can be used to store samples, and its status can be checked using the provided function.
 */
esp_err_t memory_manager_start(void);

/** 
 * @brief Check if the memory manager is currently running and ready to store samples.
 * @returns true if the memory manager is running, false otherwise. This function can be used to verify that the memory manager has been successfully started and is ready for use before attempting to enqueue samples
 */
bool memory_manager_is_running(void);
