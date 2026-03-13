#include "modbus_manager.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logging.h"
#include "memory.h"
#include "modbus_master.h"
#include "modbus_io.h"
#include "sdkconfig.h"

#define TAG "MODBUS_MGR"

#if CONFIG_MODBUS_MANAGER_ENABLE

// Task handle and state variables
static TaskHandle_t s_task_handle = NULL;
static bool s_started = false;
static const MultimeterRegisterSet *s_target_register_set = NULL;

static esp_err_t modbus_read_by_function(
    uint8_t slave_addr,
    uint8_t function_code,
    uint16_t start_reg,
    uint16_t reg_count,
    uint16_t *dest
)
{
    switch (function_code) {
        case TARGET_REGISTER_FUNC_READ_HOLDING:
            return modbus_read_holding_registers(slave_addr, start_reg, reg_count, dest);
        case TARGET_REGISTER_FUNC_READ_INPUT:
            return modbus_read_input_registers(slave_addr, start_reg, reg_count, dest);
        default:
            return ESP_ERR_INVALID_ARG;
    }
}

static esp_err_t modbus_validate_target_register_set(const MultimeterRegisterSet *target_register_set)
{
    if (target_register_set == NULL || target_register_set->registers == NULL || target_register_set->size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < target_register_set->size; ++i) {
        const MultimeterRegister *target = &target_register_set->registers[i];
        if (target->reg_count == 0U || target->reg_count > CONFIG_MEMORY_MAX_REGISTERS) {
            LOG_ERROR(
                TAG,
                "Invalid reg_count=%u for target[%u] start=0x%04X (range 1..%u)",
                target->reg_count,
                (unsigned)i,
                (unsigned)target->reg_start,
                CONFIG_MEMORY_MAX_REGISTERS
            );
            return ESP_ERR_INVALID_ARG;
        }

        if (target->function_code != TARGET_REGISTER_FUNC_READ_HOLDING &&
            target->function_code != TARGET_REGISTER_FUNC_READ_INPUT) {
            LOG_ERROR(
                TAG,
                "Invalid function_code=0x%02X for target[%u] start=0x%04X",
                target->function_code,
                (unsigned)i,
                (unsigned)target->reg_start
            );
            return ESP_ERR_INVALID_ARG;
        }
    }

    return ESP_OK;
}

static esp_err_t modbus_store_sample(
    uint16_t start_reg,
    uint16_t reg_count,
    const uint16_t *sample_buf
)
{
    uint32_t ts = (uint32_t)time(NULL);
    return memory_enqueue_modbus_sample(
        CONFIG_MODBUS_MANAGER_SLAVE_ADDR,
        start_reg,
        sample_buf,
        reg_count,
        ts
    );
}

static void modbus_sample_configured_window(uint16_t *sample_buf)
{
    esp_err_t err = modbus_read_holding_registers(
        CONFIG_MODBUS_MANAGER_SLAVE_ADDR,
        CONFIG_MODBUS_MANAGER_START_REG,
        CONFIG_MODBUS_MANAGER_REG_COUNT,
        sample_buf
    );

    if (err != ESP_OK) {
        LOG_WARNING(TAG, "Modbus read failed: 0x%x", err);
        return;
    }

    err = modbus_store_sample(
        CONFIG_MODBUS_MANAGER_START_REG,
        CONFIG_MODBUS_MANAGER_REG_COUNT,
        sample_buf
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
}

static void modbus_sample_target_register_set(uint16_t *sample_buf)
{
    uint16_t read_ok = 0;
    uint16_t read_fail = 0;
    uint16_t store_fail = 0;
    esp_err_t first_read_err = ESP_OK;
    uint16_t first_read_fail_reg = 0;
    esp_err_t first_store_err = ESP_OK;
    uint16_t first_store_fail_reg = 0;
    const TickType_t request_delay_ticks = pdMS_TO_TICKS(CONFIG_MODBUS_MANAGER_DELAY_BETWEEN_REQUESTS_MS);

    for (size_t i = 0; i < s_target_register_set->size; ++i) {
        const MultimeterRegister *target = &s_target_register_set->registers[i];

        esp_err_t err = modbus_read_by_function(
            CONFIG_MODBUS_MANAGER_SLAVE_ADDR,
            target->function_code,
            target->reg_start,
            target->reg_count,
            sample_buf
        );
        if (err != ESP_OK) {
            read_fail++;
            if (first_read_err == ESP_OK) {
                first_read_err = err;
                first_read_fail_reg = target->reg_start;
            }
            if (request_delay_ticks > 0) {
                vTaskDelay(request_delay_ticks);
            }
            continue;
        }

        read_ok++;

        err = modbus_store_sample(target->reg_start, target->reg_count, sample_buf);
        if (err != ESP_OK) {
            store_fail++;
            if (first_store_err == ESP_OK) {
                first_store_err = err;
                first_store_fail_reg = target->reg_start;
            }
        }

        if (request_delay_ticks > 0) {
            vTaskDelay(request_delay_ticks);
        }
    }

    if (read_fail == (uint16_t)s_target_register_set->size) {
        LOG_WARNING(
            TAG,
            "No target registers responded: err=0x%x first_reg=0x%04X total=%u",
            first_read_err,
            (unsigned)first_read_fail_reg,
            (unsigned)s_target_register_set->size
        );
    } else if (read_fail > 0U) {
        LOG_WARNING(
            TAG,
            "Not all target registers responded: ok=%u fail=%u first_err=0x%x first_reg=0x%04X total=%u",
            read_ok,
            read_fail,
            first_read_err,
            (unsigned)first_read_fail_reg,
            (unsigned)s_target_register_set->size
        );
    }

    if (store_fail > 0U) {
        LOG_ERROR(
            TAG,
            "Failed storing some samples: fail=%u first_err=0x%x first_reg=0x%04X",
            store_fail,
            first_store_err,
            (unsigned)first_store_fail_reg
        );
    }

    if (read_fail == 0U && store_fail == 0U) {
        LOG_DEBUG(
            TAG,
            "Target-set cycle complete: stored=%u total=%u pending=%" PRIu32 " used=%" PRIu32 "B",
            read_ok,
            (unsigned)s_target_register_set->size,
            memory_pending_samples(),
            memory_used_bytes()
        );
    }
}

static esp_err_t modbus_probe_slave_for_scan(uint8_t slave_addr, uint16_t reg_addr, uint8_t *detected_fc)
{
    esp_err_t err_input = modbus_probe_input_register(slave_addr, reg_addr);
    if (err_input == ESP_OK) {
        if (detected_fc != NULL) {
            *detected_fc = TARGET_REGISTER_FUNC_READ_INPUT;
        }
        return ESP_OK;
    }

    esp_err_t err_holding = modbus_probe_holding_register(slave_addr, reg_addr);
    if (err_holding == ESP_OK) {
        if (detected_fc != NULL) {
            *detected_fc = TARGET_REGISTER_FUNC_READ_HOLDING;
        }
        return ESP_OK;
    }

    if (detected_fc != NULL) {
        *detected_fc = 0U;
    }

    return (err_input != ESP_OK) ? err_input : err_holding;
}

#if CONFIG_MODBUS_MANAGER_SCAN_ON_STARTUP
static bool modbus_scan_on_startup(void)
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
        return false;
    }

    const bool configured_in_range =
        (CONFIG_MODBUS_MANAGER_SLAVE_ADDR >= addr_start) &&
        (CONFIG_MODBUS_MANAGER_SLAVE_ADDR <= addr_end);
    bool configured_found = false;
    uint16_t found_count = 0;
    esp_err_t first_scan_err = ESP_OK;

    LOG_INFO(
        TAG,
        "Startup scan: probing slaves %d..%d (reg=%u, fc=0x04 then 0x03)",
        addr_start,
        addr_end,
        CONFIG_MODBUS_MANAGER_SCAN_PROBE_REG
    );

    for (int addr = addr_start; addr <= addr_end; ++addr) {
        uint8_t detected_fc = 0U;
        esp_err_t err = modbus_probe_slave_for_scan(
            (uint8_t)addr,
            CONFIG_MODBUS_MANAGER_SCAN_PROBE_REG,
            &detected_fc
        );
        if (err == ESP_OK) {
            found_count++;
            LOG_OK(
                TAG,
                "Detected Modbus device at slave address %d (fc=0x%02X)",
                addr,
                (unsigned)detected_fc
            );
            if (addr == CONFIG_MODBUS_MANAGER_SLAVE_ADDR) {
                configured_found = true;
            }
        } else if (first_scan_err == ESP_OK) {
            first_scan_err = err;
        }
    }

    if (found_count == 0) {
        LOG_WARNING(
            TAG,
            "Startup scan complete: no devices detected in range %d..%d (first_err=0x%x)",
            addr_start,
            addr_end,
            first_scan_err
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

    return configured_in_range && configured_found;
}
#endif

static void modbus_sampling_task(void *arg)
{
    (void)arg;

    static uint16_t sample_buf[CONFIG_MEMORY_MAX_REGISTERS];
    const TickType_t period_ticks = pdMS_TO_TICKS(CONFIG_MODBUS_MANAGER_POLL_PERIOD_MS);

    while (true) {
        if (s_target_register_set != NULL) {
            modbus_sample_target_register_set(sample_buf);
        } else {
            modbus_sample_configured_window(sample_buf);
        }
        vTaskDelay(period_ticks);
    }
}

esp_err_t modbus_manager_set_target_register_set(const MultimeterRegisterSet *target_register_set)
{
    if (s_started) {
        return ESP_ERR_INVALID_STATE;
    }

    if (target_register_set == NULL) {
        s_target_register_set = NULL;
        return ESP_OK;
    }

    esp_err_t err = modbus_validate_target_register_set(target_register_set);
    if (err != ESP_OK) {
        return err;
    }

    s_target_register_set = target_register_set;
    return ESP_OK;
}

esp_err_t modbus_manager_start(void)
{
    if (s_started) {
        return ESP_OK;
    }

    if (s_target_register_set != NULL) {
        esp_err_t set_err = modbus_validate_target_register_set(s_target_register_set);
        if (set_err != ESP_OK) {
            LOG_ERROR(TAG, "Invalid target register set");
            return set_err;
        }
    } else {
        if (CONFIG_MODBUS_MANAGER_REG_COUNT == 0 || CONFIG_MODBUS_MANAGER_REG_COUNT > CONFIG_MEMORY_MAX_REGISTERS) {
            LOG_ERROR(
                TAG,
                "Invalid reg count (%u), must be in range 1..%u",
                CONFIG_MODBUS_MANAGER_REG_COUNT,
                CONFIG_MEMORY_MAX_REGISTERS
            );
            return ESP_ERR_INVALID_ARG;
        }
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
        CONFIG_MODBUS_MANAGER_UART_RTS_PIN,
        CONFIG_MODBUS_MANAGER_UART_BAUDRATE,
        MODBUS_IO_LINK_RS485
    );
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "modbus_init failed: 0x%x", err);
        return err;
    }

    bool configured_slave_responded = false;
    esp_err_t configured_probe_err = ESP_OK;
    uint16_t configured_probe_reg = CONFIG_MODBUS_MANAGER_START_REG;

    if (s_target_register_set != NULL && s_target_register_set->size > 0U) {
        configured_probe_reg = s_target_register_set->registers[0].reg_start;
    }

#if CONFIG_MODBUS_MANAGER_SCAN_ON_STARTUP
    configured_slave_responded = modbus_scan_on_startup();
#endif

    if (!configured_slave_responded) {
        configured_probe_err = modbus_probe_slave_for_scan(
            CONFIG_MODBUS_MANAGER_SLAVE_ADDR,
            configured_probe_reg,
            NULL
        );
        configured_slave_responded = (configured_probe_err == ESP_OK);
    }

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
    if (s_target_register_set != NULL) {
        if (configured_slave_responded) {
            LOG_OK(
                TAG,
                "Started: slave=%u targets=%u period_ms=%u",
                CONFIG_MODBUS_MANAGER_SLAVE_ADDR,
                (unsigned)s_target_register_set->size,
                CONFIG_MODBUS_MANAGER_POLL_PERIOD_MS
            );
        } else {
            LOG_WARNING(
                TAG,
                "Started with warnings: slave=%u targets=%u period_ms=%u probe_reg=0x%04X err=0x%x",
                CONFIG_MODBUS_MANAGER_SLAVE_ADDR,
                (unsigned)s_target_register_set->size,
                CONFIG_MODBUS_MANAGER_POLL_PERIOD_MS,
                (unsigned)configured_probe_reg,
                configured_probe_err
            );
        }
    } else {
        if (configured_slave_responded) {
            LOG_OK(
                TAG,
                "Started: slave=%u start_reg=%u reg_count=%u period_ms=%u",
                CONFIG_MODBUS_MANAGER_SLAVE_ADDR,
                CONFIG_MODBUS_MANAGER_START_REG,
                CONFIG_MODBUS_MANAGER_REG_COUNT,
                CONFIG_MODBUS_MANAGER_POLL_PERIOD_MS
            );
        } else {
            LOG_WARNING(
                TAG,
                "Started with warnings: slave=%u start_reg=%u reg_count=%u period_ms=%u probe_reg=0x%04X err=0x%x",
                CONFIG_MODBUS_MANAGER_SLAVE_ADDR,
                CONFIG_MODBUS_MANAGER_START_REG,
                CONFIG_MODBUS_MANAGER_REG_COUNT,
                CONFIG_MODBUS_MANAGER_POLL_PERIOD_MS,
                (unsigned)configured_probe_reg,
                configured_probe_err
            );
        }
    }
    return ESP_OK;
}

bool modbus_manager_is_running(void)
{
    return s_started;
}

#else

esp_err_t modbus_manager_set_target_register_set(const MultimeterRegisterSet *target_register_set)
{
    (void)target_register_set;
    return ESP_OK;
}

esp_err_t modbus_manager_start(void)
{
    return ESP_OK;
}

bool modbus_manager_is_running(void)
{
    return false;
}

#endif
