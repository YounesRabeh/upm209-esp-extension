#include "modbus_manager.h"

#include <inttypes.h>
#include <stdint.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logging.h"
#include "memory.h"
#include "modbus_master.h"
#include "sdkconfig.h"

#define TAG "MODBUS_MGR"

#if CONFIG_MODBUS_MANAGER_ENABLE

// Task handle and state variables
static TaskHandle_t s_task_handle = NULL;
static bool s_started = false;

#if CONFIG_MODBUS_MANAGER_SCAN_ON_STARTUP
static void modbus_scan_on_startup(void)
{
    const int addr_start = CONFIG_MODBUS_MANAGER_SCAN_ADDR_START;
    const int addr_end = CONFIG_MODBUS_MANAGER_SCAN_ADDR_END;

    if (addr_end < addr_start) {
        LOG_WARNING(
            TAG,
            "Startup scan skipped: invalid range %d..%d",
            addr_start,
            addr_end
        );
        return;
    }

    const bool configured_in_range =
        (CONFIG_MODBUS_MANAGER_SLAVE_ADDR >= addr_start) &&
        (CONFIG_MODBUS_MANAGER_SLAVE_ADDR <= addr_end);
    bool configured_found = false;
    uint16_t found_count = 0;

    LOG_INFO(
        TAG,
        "Startup scan: probing slaves %d..%d (holding reg=%u)",
        addr_start,
        addr_end,
        CONFIG_MODBUS_MANAGER_SCAN_PROBE_REG
    );

    for (int addr = addr_start; addr <= addr_end; ++addr) {
        esp_err_t err = modbus_probe_holding_register(
            (uint8_t)addr,
            CONFIG_MODBUS_MANAGER_SCAN_PROBE_REG
        );
        if (err == ESP_OK) {
            found_count++;
            LOG_OK(TAG, "Detected Modbus device at slave address %d", addr);
            if (addr == CONFIG_MODBUS_MANAGER_SLAVE_ADDR) {
                configured_found = true;
            }
        }
    }

    if (found_count == 0) {
        LOG_WARNING(
            TAG,
            "Startup scan complete: no devices detected in range %d..%d",
            addr_start,
            addr_end
        );
    } else {
        LOG_OK(
            TAG,
            "Startup scan complete: found %u device(s) in range %d..%d",
            found_count,
            addr_start,
            addr_end
        );
    }

    if (configured_in_range && !configured_found) {
        LOG_WARNING(
            TAG,
            "Configured slave %u did not respond during startup scan",
            CONFIG_MODBUS_MANAGER_SLAVE_ADDR
        );
    }
}
#endif

static void modbus_sampling_task(void *arg)
{
    (void)arg;

    static uint16_t sample_buf[CONFIG_MEMORY_MAX_REGISTERS];
    const TickType_t period_ticks = pdMS_TO_TICKS(CONFIG_MODBUS_MANAGER_POLL_PERIOD_MS);

    while (true) {
        esp_err_t err = modbus_read_holding_registers(
            CONFIG_MODBUS_MANAGER_SLAVE_ADDR,
            CONFIG_MODBUS_MANAGER_START_REG,
            CONFIG_MODBUS_MANAGER_REG_COUNT,
            sample_buf
        );

        if (err == ESP_OK) {
            uint32_t ts = (uint32_t)time(NULL);
            err = memory_enqueue_modbus_sample(
                CONFIG_MODBUS_MANAGER_SLAVE_ADDR,
                CONFIG_MODBUS_MANAGER_START_REG,
                sample_buf,
                CONFIG_MODBUS_MANAGER_REG_COUNT,
                ts
            );
            if (err != ESP_OK) {
                LOG_ERROR(TAG, "Storage enqueue failed: 0x%x", err);
            } else {
                LOG_DEBUG(
                    TAG,
                    "Sample stored: reg_start=%u reg_count=%u pending=%" PRIu32 " used=%" PRIu32 "B",
                    CONFIG_MODBUS_MANAGER_START_REG,
                    CONFIG_MODBUS_MANAGER_REG_COUNT,
                    memory_pending_samples(),
                    memory_used_bytes()
                );
            }
        } else {
            LOG_WARNING(TAG, "Modbus read failed: 0x%x", err);
        }

        vTaskDelay(period_ticks);
    }
}

esp_err_t modbus_manager_start(void)
{
    if (s_started) {
        return ESP_OK;
    }

    if (CONFIG_MODBUS_MANAGER_REG_COUNT == 0 || CONFIG_MODBUS_MANAGER_REG_COUNT > CONFIG_MEMORY_MAX_REGISTERS) {
        LOG_ERROR(
            TAG,
            "Invalid reg count (%u), must be in range 1..%u",
            CONFIG_MODBUS_MANAGER_REG_COUNT,
            CONFIG_MEMORY_MAX_REGISTERS
        );
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = memory_init();
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "memory_init failed: 0x%x", err);
        return err;
    }

    err = modbus_init(
        CONFIG_MODBUS_MANAGER_UART_PORT,
        CONFIG_MODBUS_MANAGER_UART_TX_PIN,
        CONFIG_MODBUS_MANAGER_UART_RX_PIN,
        CONFIG_MODBUS_MANAGER_UART_BAUDRATE
    );
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "modbus_init failed: 0x%x", err);
        return err;
    }

#if CONFIG_MODBUS_MANAGER_SCAN_ON_STARTUP
    modbus_scan_on_startup();
#endif

    BaseType_t ok = xTaskCreate(
        modbus_sampling_task,
        "modbus_sampling",
        CONFIG_MODBUS_MANAGER_TASK_STACK_SIZE,
        NULL,
        CONFIG_MODBUS_MANAGER_TASK_PRIORITY,
        &s_task_handle
    );
    if (ok != pdPASS) {
        LOG_ERROR(TAG, "Failed to create Modbus sampling task");
        return ESP_ERR_NO_MEM;
    }

    s_started = true;
    LOG_OK(
        TAG,
        "Started: slave=%u start_reg=%u reg_count=%u period_ms=%u",
        CONFIG_MODBUS_MANAGER_SLAVE_ADDR,
        CONFIG_MODBUS_MANAGER_START_REG,
        CONFIG_MODBUS_MANAGER_REG_COUNT,
        CONFIG_MODBUS_MANAGER_POLL_PERIOD_MS
    );
    return ESP_OK;
}

bool modbus_manager_is_running(void)
{
    return s_started;
}

#else

esp_err_t modbus_manager_start(void)
{
    return ESP_OK;
}

bool modbus_manager_is_running(void)
{
    return false;
}

#endif
