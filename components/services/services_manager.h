#pragma once

#include "esp_err.h"

/**
 * @brief Starts internet service (init + connect) and then starts the rest of services.
 * @returns ESP_OK if all enabled services started successfully, or the first error code.
 */
esp_err_t services_manager_start(void);

/**
 * @brief Services manager component for starting various services after network connection is established.
 * This component provides a function to start services that depend on network connectivity, such as time synchronization and Modbus sampling. The services started by this component can be enabled or disabled via configuration options. The function returns an error code if any of the enabled services fail to start, but it will attempt to start all services regardless of individual failures. This allows for partial functionality even if some services encounter issues during startup.
 * @returns ESP_OK if all enabled services started successfully, or the first encountered error code if any service failed to start.
 */
esp_err_t services_manager_start_post_network(void);
