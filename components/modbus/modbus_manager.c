#include "modbus_manager.h"

#include <inttypes.h>
#include <stdint.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logging.h"
#include "memory.h"
#include "memory_manager.h"
#include "modbus_master.h"
#include "modbus_io.h"
#include "sdkconfig.h"

#define TAG "MODBUS_MGR"

#if CONFIG_MODBUS_MANAGER_ENABLE

#define MB_PORT_NUM            1
#define MB_BAUD_RATE           19200
#define MB_PARITY_MODE         UART_PARITY_DISABLE
#define MB_SLAVE_ADDR          1U
#define MB_UART_TXD            7
#define MB_UART_RXD            8
#define MB_UART_RTS            4
#define MB_INIT_DELAY_MS       1000U
#define MB_FIRST_POLL_DELAY_MS 1000U
#define MB_CHUNK_DELAY_MS      30U
#define MB_CHUNK_RETRIES       2U
#define MB_RETRY_DELAY_MS      200U

#define MB_WINDOW_START_REG    0x0000U
// UPM209 input map is contiguous from 0x0000 to 0x0075 (118 registers).
// Reading beyond this range in one contiguous request can hit invalid addresses.
#define MB_WINDOW_REG_COUNT    118U
#define MB_MAX_READ_REGS       60U
#define MB_PROBE_REG           MB_WINDOW_START_REG

_Static_assert(MB_MAX_READ_REGS > 0U && MB_MAX_READ_REGS <= 125U, "Invalid MB_MAX_READ_REGS");

static TaskHandle_t s_task_handle = NULL;
static bool s_started = false;

static const char *modbus_err_label(esp_err_t err)
{
    switch (err) {
        case ESP_ERR_TIMEOUT:
            return "timeout";
        case ESP_ERR_INVALID_RESPONSE:
            return "invalid_response";
        default:
            return "other";
    }
}

static void modbus_log_sample_values(const uint16_t *sample_buf)
{
    uint32_t checksum = 0U;
    for (uint16_t i = 0; i < MB_WINDOW_REG_COUNT; ++i) {
        checksum += sample_buf[i];
    }

    LOG_DEBUG(
        TAG,
        "Values[0..7]=[%04X %04X %04X %04X %04X %04X %04X %04X] tail=[%04X %04X] checksum=0x%08" PRIX32,
        sample_buf[0], sample_buf[1], sample_buf[2], sample_buf[3],
        sample_buf[4], sample_buf[5], sample_buf[6], sample_buf[7],
        sample_buf[MB_WINDOW_REG_COUNT - 2], sample_buf[MB_WINDOW_REG_COUNT - 1],
        checksum
    );
}

static esp_err_t modbus_store_sample(const uint16_t *sample_buf)
{
    uint32_t ts = (uint32_t)time(NULL);
    return memory_manager_enqueue_modbus_sample(
        MB_SLAVE_ADDR,
        MB_WINDOW_START_REG,
        sample_buf,
        MB_WINDOW_REG_COUNT,
        ts
    );
}

static esp_err_t modbus_read_window_chunked(uint16_t *sample_buf)
{
    uint16_t remaining = MB_WINDOW_REG_COUNT;
    uint16_t offset = 0U;

    while (remaining > 0U) {
        uint16_t chunk_count = (remaining > MB_MAX_READ_REGS) ? MB_MAX_READ_REGS : remaining;
        uint16_t chunk_start = (uint16_t)(MB_WINDOW_START_REG + offset);
        esp_err_t err = ESP_FAIL;

        for (uint8_t attempt = 0U; attempt <= MB_CHUNK_RETRIES; ++attempt) {
            err = modbus_read_input_registers(
                MB_SLAVE_ADDR,
                chunk_start,
                chunk_count,
                &sample_buf[offset]
            );
            if (err == ESP_OK) {
                break;
            }

            LOG_WARNING(
                TAG,
                "Modbus read failed: slave=%u fc=0x04 start=0x%04X count=%u err=0x%x (%s) attempt=%u/%u",
                (unsigned)MB_SLAVE_ADDR,
                (unsigned)chunk_start,
                (unsigned)chunk_count,
                err,
                modbus_err_label(err),
                (unsigned)(attempt + 1U),
                (unsigned)(MB_CHUNK_RETRIES + 1U)
            );

            if (attempt < MB_CHUNK_RETRIES) {
                esp_err_t recover_err = modbus_recover_link();
                if (recover_err != ESP_OK) {
                    LOG_WARNING(TAG, "Modbus link recovery failed: err=0x%x", recover_err);
                }
                if (MB_RETRY_DELAY_MS > 0U) {
                    vTaskDelay(pdMS_TO_TICKS(MB_RETRY_DELAY_MS));
                }
            }
        }

        if (err != ESP_OK) {
            return err;
        }

        LOG_DEBUG(
            TAG,
            "Chunk read OK: slave=%u start=0x%04X count=%u",
            (unsigned)MB_SLAVE_ADDR,
            (unsigned)chunk_start,
            (unsigned)chunk_count
        );

        offset = (uint16_t)(offset + chunk_count);
        remaining = (uint16_t)(remaining - chunk_count);
        if (remaining > 0U && MB_CHUNK_DELAY_MS > 0U) {
            vTaskDelay(pdMS_TO_TICKS(MB_CHUNK_DELAY_MS));
        }
    }

    return ESP_OK;
}

static void modbus_sample_window(uint16_t *sample_buf)
{
    esp_err_t err = modbus_read_window_chunked(sample_buf);
    if (err != ESP_OK) {
        return;
    }

    modbus_log_sample_values(sample_buf);

    err = modbus_store_sample(sample_buf);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "Storage enqueue failed: 0x%x", err);
    } else {
        LOG_DEBUG(
            TAG,
            "Sample stored: start=0x%04X count=%u ingest_pending=%" PRIu32 " persisted_pending=%" PRIu32 " used=%" PRIu32 "B",
            (unsigned)MB_WINDOW_START_REG,
            (unsigned)MB_WINDOW_REG_COUNT,
            memory_manager_ingest_pending(),
            memory_pending_samples(),
            memory_used_bytes()
        );
    }
}

static void modbus_sampling_task(void *arg)
{
    (void)arg;

    static uint16_t sample_buf[MB_WINDOW_REG_COUNT];
    const TickType_t period_ticks = pdMS_TO_TICKS(CONFIG_MODBUS_MANAGER_POLL_PERIOD_MS);

    if (MB_FIRST_POLL_DELAY_MS > 0U) {
        LOG_DEBUG(TAG, "First poll delay: %u ms", (unsigned)MB_FIRST_POLL_DELAY_MS);
        vTaskDelay(pdMS_TO_TICKS(MB_FIRST_POLL_DELAY_MS));
    }

    while (true) {
        modbus_sample_window(sample_buf);
        vTaskDelay(period_ticks);
    }
}

esp_err_t modbus_manager_start(void)
{
    if (s_started) {
        return ESP_OK;
    }

    if (MB_WINDOW_REG_COUNT > CONFIG_MEMORY_MAX_REGISTERS) {
        LOG_WARNING(
            TAG,
            "Fixed reg_count=%u exceeds CONFIG_MEMORY_MAX_REGISTERS=%u",
            (unsigned)MB_WINDOW_REG_COUNT,
            (unsigned)CONFIG_MEMORY_MAX_REGISTERS
        );
        return ESP_ERR_INVALID_SIZE;
    }

    if (!memory_is_ready()) {
        LOG_ERROR(TAG, "memory module is not ready");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ESP_OK;

    err = modbus_init(
        MB_PORT_NUM,
        MB_UART_TXD,
        MB_UART_RXD,
        MB_UART_RTS,
        MB_BAUD_RATE,
        MB_PARITY_MODE,
        MODBUS_IO_LINK_RS485
    );
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "modbus_init failed: 0x%x", err);
        return err;
    }

    if (MB_INIT_DELAY_MS > 0U) {
        vTaskDelay(pdMS_TO_TICKS(MB_INIT_DELAY_MS));
    }

    esp_err_t probe_err = modbus_probe_input_register(MB_SLAVE_ADDR, MB_PROBE_REG);
    bool probe_ok = (probe_err == ESP_OK);

    if (probe_ok) {
        LOG_OK(
            TAG,
            "Startup probe OK: slave=%u fc=0x04 reg=0x%04X",
            (unsigned)MB_SLAVE_ADDR,
            (unsigned)MB_PROBE_REG
        );
    } else {
        LOG_WARNING(
            TAG,
            "Startup probe failed: slave=%u fc=0x04 reg=0x%04X err=0x%x (continuing)",
            (unsigned)MB_SLAVE_ADDR,
            (unsigned)MB_PROBE_REG,
            probe_err
        );
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
    if (probe_ok) {
        LOG_OK(
            TAG,
            "Started: slave=%u fc=0x04 start=0x%04X count=%u period_ms=%u",
            (unsigned)MB_SLAVE_ADDR,
            (unsigned)MB_WINDOW_START_REG,
            (unsigned)MB_WINDOW_REG_COUNT,
            CONFIG_MODBUS_MANAGER_POLL_PERIOD_MS
        );
    } else {
        LOG_WARNING(
            TAG,
            "Started with warnings: slave=%u fc=0x04 start=0x%04X count=%u period_ms=%u",
            (unsigned)MB_SLAVE_ADDR,
            (unsigned)MB_WINDOW_START_REG,
            (unsigned)MB_WINDOW_REG_COUNT,
            CONFIG_MODBUS_MANAGER_POLL_PERIOD_MS
        );
    }

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
