#include "modbus_manager.h"

#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logging.h"
#include "modbus_master.h"
#include "sdkconfig.h"
#include "ump209.h"

#define TAG "MODBUS_MGR"

#if CONFIG_MODBUS_MANAGER_ENABLE

#define MB_PORT_NUM               1
#define MB_BAUD_RATE              19200
#define MB_PARITY_MODE            UART_PARITY_DISABLE
#define MB_SLAVE_ADDR             1U
#define MB_UART_TXD               7
#define MB_UART_RXD               8
#define MB_UART_RTS               4
#define MB_POLL_PERIOD_MS         2000U
#define MB_TASK_STACK_SIZE        4096
#define MB_TASK_PRIORITY          5
#define MB_INIT_DELAY_MS          500U
#define MB_FIRST_POLL_DELAY_MS    500U
#define MB_TEST_REQ_DELAY_MS      25U
#define MB_BLOCK_RETRY_DELAY_MS   20U
#define MB_FALLBACK_CHUNK_WORDS   64U
#define MB_FALLBACK_MIN_CHUNK_WORDS 8U
#define MB_PROBE_REG              0x0000U
#define MB_MAX_REQ_WORDS          125U
#define MB_MAX_CYCLE_WORDS        1200U
#define MB_MAX_FAIL_REPORT_LINES  128U
#define MB_VERBOSE_DEBUG          0

#if MB_VERBOSE_DEBUG
#define MB_LOG_DEBUG(...) LOG_DEBUG(__VA_ARGS__)
#else
#define MB_LOG_DEBUG(...) do { } while (0)
#endif

typedef struct {
    uint8_t fc;
    uint16_t start;
    uint16_t count;
    esp_err_t err;
} modbus_block_fail_entry_t;

static TaskHandle_t s_task_handle = NULL;
static bool s_started = false;
static uint16_t s_read_buf[MB_MAX_REQ_WORDS] = {0};
static uint16_t s_cycle_buf[MB_MAX_CYCLE_WORDS] = {0};
static modbus_block_fail_entry_t s_fail_entries[MB_MAX_FAIL_REPORT_LINES] = {0};
static modbus_sample_sink_t s_sample_sink = NULL;
static void *s_sample_sink_ctx = NULL;

static void modbus_record_fail(
    uint8_t fc,
    uint16_t start,
    uint16_t count,
    esp_err_t err,
    uint32_t *fail_counter
)
{
    if (fail_counter == NULL) {
        return;
    }
    ++(*fail_counter);
    if (*fail_counter <= MB_MAX_FAIL_REPORT_LINES) {
        modbus_block_fail_entry_t *entry = &s_fail_entries[*fail_counter - 1U];
        entry->fc = fc;
        entry->start = start;
        entry->count = count;
        entry->err = err;
    }
}

static esp_err_t modbus_read_by_fc(uint8_t fc, uint16_t start_reg, uint16_t reg_count, uint16_t *dest)
{
    if (fc == TARGET_REGISTER_FUNC_READ_INPUT) {
        return modbus_read_input_registers(MB_SLAVE_ADDR, start_reg, reg_count, dest);
    }
    if (fc == TARGET_REGISTER_FUNC_READ_HOLDING) {
        return modbus_read_holding_registers(MB_SLAVE_ADDR, start_reg, reg_count, dest);
    }
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t modbus_read_with_recovery(
    uint8_t fc,
    uint16_t start_reg,
    uint16_t reg_count,
    uint16_t *dest
)
{
    esp_err_t err = modbus_read_by_fc(fc, start_reg, reg_count, dest);
    if (err == ESP_OK) {
        return ESP_OK;
    }

    esp_err_t recover_err = modbus_recover_link();
    if (recover_err != ESP_OK) {
        LOG_WARNING(TAG, "Link recovery failed after read error: recover_err=0x%x", recover_err);
    }

    if (MB_BLOCK_RETRY_DELAY_MS > 0U) {
        vTaskDelay(pdMS_TO_TICKS(MB_BLOCK_RETRY_DELAY_MS));
    }

    esp_err_t retry_err = modbus_read_by_fc(fc, start_reg, reg_count, dest);
    if (retry_err == ESP_OK) {
        MB_LOG_DEBUG(
            TAG,
            "Recovered block after retry: fc=0x%02X start=0x%04X count=%u first_err=0x%x",
            (unsigned)fc,
            (unsigned)start_reg,
            (unsigned)reg_count,
            err
        );
    }
    return retry_err;
}

static esp_err_t modbus_read_block_chunked(
    uint8_t fc,
    uint16_t start_reg,
    uint16_t reg_count,
    uint16_t chunk_words,
    uint16_t *dest
)
{
    if (dest == NULL || reg_count == 0U || chunk_words == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t offset = 0U;
    while (offset < reg_count) {
        uint16_t chunk = (uint16_t)(reg_count - offset);
        if (chunk > chunk_words) {
            chunk = chunk_words;
        }

        esp_err_t err = modbus_read_with_recovery(
            fc,
            (uint16_t)(start_reg + offset),
            chunk,
            &dest[offset]
        );
        if (err != ESP_OK) {
            MB_LOG_DEBUG(
                TAG,
                "Chunk read failed: fc=0x%02X start=0x%04X count=%u err=0x%x",
                (unsigned)fc,
                (unsigned)(start_reg + offset),
                (unsigned)chunk,
                err
            );
            return err;
        }
        offset = (uint16_t)(offset + chunk);
    }

    return ESP_OK;
}

static esp_err_t modbus_read_block_resilient(
    uint8_t fc,
    uint16_t start_reg,
    uint16_t reg_count,
    uint16_t *dest
)
{
    esp_err_t err = modbus_read_with_recovery(fc, start_reg, reg_count, dest);
    if (err == ESP_OK) {
        return ESP_OK;
    }

    uint16_t chunk_words = MB_FALLBACK_CHUNK_WORDS;
    if (chunk_words > reg_count) {
        chunk_words = reg_count;
    }
    if (chunk_words < MB_FALLBACK_MIN_CHUNK_WORDS) {
        chunk_words = MB_FALLBACK_MIN_CHUNK_WORDS;
    }

    esp_err_t last_err = err;
    while (chunk_words >= MB_FALLBACK_MIN_CHUNK_WORDS && chunk_words <= reg_count) {
        MB_LOG_DEBUG(
            TAG,
            "Block fallback to chunked reads: fc=0x%02X start=0x%04X count=%u chunk=%u first_err=0x%x",
            (unsigned)fc,
            (unsigned)start_reg,
            (unsigned)reg_count,
            (unsigned)chunk_words,
            last_err
        );

        esp_err_t chunk_err = modbus_read_block_chunked(fc, start_reg, reg_count, chunk_words, dest);
        if (chunk_err == ESP_OK) {
            MB_LOG_DEBUG(
                TAG,
                "Recovered block with chunked reads: fc=0x%02X start=0x%04X count=%u chunk=%u",
                (unsigned)fc,
                (unsigned)start_reg,
                (unsigned)reg_count,
                (unsigned)chunk_words
            );
            return ESP_OK;
        }

        last_err = chunk_err;
        if (chunk_words == MB_FALLBACK_MIN_CHUNK_WORDS) {
            break;
        }
        chunk_words = (uint16_t)(chunk_words / 2U);
        if (chunk_words < MB_FALLBACK_MIN_CHUNK_WORDS) {
            chunk_words = MB_FALLBACK_MIN_CHUNK_WORDS;
        }
    }

    return last_err;
}

static esp_err_t modbus_sample_and_store_cycle(void)
{
    const MultimeterRegisterSet *set = ump209_get_target_register_set();
    if (set == NULL || set->registers == NULL || set->size == 0U) {
        LOG_ERROR(TAG, "UPM209 register set unavailable");
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t total = (uint32_t)set->size;
    uint32_t block_ok = 0U;
    uint32_t fail = 0U;
    uint32_t skipped = 0U;
    uint32_t requests = 0U;
    uint32_t cycle_words = 0U;
    uint32_t copied_words = 0U;
    bool cycle_overflow = false;
    bool cycle_start_set = false;
    uint16_t cycle_start_reg = 0U;
    esp_err_t first_fail_err = ESP_OK;

    memset(s_fail_entries, 0, sizeof(s_fail_entries));

    LOG_DEBUG(
        TAG,
        "Cycle start: slave=%u entries=%" PRIu32,
        (unsigned)MB_SLAVE_ADDR,
        (uint32_t)set->size
    );

    uint32_t i = 0U;
    while (i < (uint32_t)set->size) {
        const MultimeterRegister *first = &set->registers[i];

        if (first->reg_count == 0U || first->reg_count > MB_MAX_REQ_WORDS) {
            ++skipped;
            LOG_WARNING(
                TAG,
                "SKIP [%" PRIu32 "] invalid reg_count=%u name=%s start=0x%04X",
                i,
                (unsigned)first->reg_count,
                (first->name != NULL) ? first->name : "(null)",
                (unsigned)first->reg_start
            );
            ++i;
            continue;
        }

        uint8_t block_fc = first->function_code;
        uint16_t block_start = first->reg_start;
        uint32_t block_end = (uint32_t)first->reg_start + (uint32_t)first->reg_count;
        uint32_t j = i + 1U;

        while (j < (uint32_t)set->size) {
            const MultimeterRegister *next = &set->registers[j];
            if (next->reg_count == 0U || next->reg_count > MB_MAX_REQ_WORDS) {
                break;
            }
            if (next->function_code != block_fc) {
                break;
            }
            if (next->reg_start > block_end) {
                break;
            }

            uint32_t next_end = (uint32_t)next->reg_start + (uint32_t)next->reg_count;
            uint32_t merged_end = (next_end > block_end) ? next_end : block_end;
            uint32_t merged_count = merged_end - (uint32_t)block_start;
            if (merged_count > MB_MAX_REQ_WORDS) {
                break;
            }

            block_end = merged_end;
            ++j;
        }

        uint16_t block_count = (uint16_t)(block_end - (uint32_t)block_start);
        ++requests;
        MB_LOG_DEBUG(
            TAG,
            "Block request: fc=0x%02X start=0x%04X count=%u entries=%" PRIu32,
            (unsigned)block_fc,
            (unsigned)block_start,
            (unsigned)block_count,
            (uint32_t)(j - i)
        );
        esp_err_t block_err = modbus_read_block_resilient(block_fc, block_start, block_count, s_read_buf);

        if (block_err == ESP_OK) {
            ++block_ok;
            if (!cycle_start_set) {
                cycle_start_reg = block_start;
                cycle_start_set = true;
            }

            uint32_t next_cycle_words = cycle_words + block_count;
            if (next_cycle_words > (uint32_t)UINT16_MAX) {
                if (!cycle_overflow) {
                    LOG_ERROR(
                        TAG,
                        "Merged payload overflow: current=%" PRIu32 " + block=%u exceeds uint16 range",
                        cycle_words,
                        (unsigned)block_count
                    );
                }
                cycle_overflow = true;
            } else {
                cycle_words = next_cycle_words;
                uint32_t next_copied_words = copied_words + block_count;
                if (next_copied_words > (uint32_t)MB_MAX_CYCLE_WORDS) {
                    if (!cycle_overflow) {
                        LOG_ERROR(
                            TAG,
                            "Merged payload overflow: current=%" PRIu32 " + block=%u exceeds max cycle capacity=%u",
                            copied_words,
                            (unsigned)block_count,
                            (unsigned)MB_MAX_CYCLE_WORDS
                        );
                    }
                    cycle_overflow = true;
                } else {
                    memcpy(&s_cycle_buf[copied_words], s_read_buf, (size_t)block_count * sizeof(uint16_t));
                    copied_words = next_copied_words;
                }
            }
        } else {
            modbus_record_fail(block_fc, block_start, block_count, block_err, &fail);
            if (first_fail_err == ESP_OK) {
                first_fail_err = block_err;
            }
        }

        i = j;
        if (MB_TEST_REQ_DELAY_MS > 0U) {
            vTaskDelay(pdMS_TO_TICKS(MB_TEST_REQ_DELAY_MS));
        }
    }

    if (fail == 0U && skipped == 0U) {
        LOG_DEBUG(
            TAG,
            "Cycle summary: total=%" PRIu32 " ok=%" PRIu32 " req=%" PRIu32 " words=%" PRIu32,
            total,
            block_ok,
            requests,
            cycle_words
        );
    } else {
        LOG_WARNING(
            TAG,
            "Cycle summary: total=%" PRIu32 " ok=%" PRIu32 " fail=%" PRIu32 " skipped=%" PRIu32 " req=%" PRIu32 " words=%" PRIu32,
            total,
            block_ok,
            fail,
            skipped,
            requests,
            cycle_words
        );
    }

    const uint32_t printed = (fail < MB_MAX_FAIL_REPORT_LINES) ? fail : MB_MAX_FAIL_REPORT_LINES;
    for (uint32_t i = 0U; i < printed; ++i) {
        const modbus_block_fail_entry_t *e = &s_fail_entries[i];
        LOG_WARNING(
            TAG,
            "FAIL BLOCK [%u] fc=0x%02X start=0x%04X count=%u err=0x%x",
            (unsigned)i,
            (unsigned)e->fc,
            (unsigned)e->start,
            (unsigned)e->count,
            e->err
        );
    }

    if (fail > MB_MAX_FAIL_REPORT_LINES) {
        LOG_WARNING(
            TAG,
            "FAIL report truncated: shown=%u total_fail=%" PRIu32,
            (unsigned)MB_MAX_FAIL_REPORT_LINES,
            fail
        );
    }

    if (fail != 0U || skipped != 0U || !cycle_start_set) {
        LOG_WARNING(TAG, "Cycle not stored: fail=%" PRIu32 " skipped=%" PRIu32, fail, skipped);
        return (first_fail_err != ESP_OK) ? first_fail_err : ESP_FAIL;
    }

    if (cycle_overflow || cycle_words == 0U) {
        LOG_WARNING(
            TAG,
            "Cycle not stored: merged words=%" PRIu32 " exceed configured capacity=%u",
            cycle_words,
            (unsigned)MB_MAX_CYCLE_WORDS
        );
        return ESP_ERR_INVALID_SIZE;
    }

    time_t now = time(NULL);
    uint32_t timestamp_s = (now > 0) ? (uint32_t)now : 0U;

    if (s_sample_sink != NULL) {
        esp_err_t sink_err = s_sample_sink(
            MB_SLAVE_ADDR,
            cycle_start_reg,
            s_cycle_buf,
            (uint16_t)copied_words,
            timestamp_s,
            s_sample_sink_ctx
        );
        if (sink_err != ESP_OK) {
            LOG_WARNING(
                TAG,
                "Cycle publish failed: start=0x%04X words=%" PRIu32 " err=0x%x",
                (unsigned)cycle_start_reg,
                cycle_words,
                sink_err
            );
        }
    } else {
        MB_LOG_DEBUG(TAG, "No sample sink registered: cycle dropped");
    }

    LOG_OK(
        TAG,
        "Cycle sampled: start=0x%04X words=%" PRIu32 " ts=%" PRIu32 " sink=ok",
        (unsigned)cycle_start_reg,
        cycle_words,
        timestamp_s
    );
    return ESP_OK;
}

static void modbus_sampling_task(void *arg)
{
    (void)arg;
    TickType_t period_ticks = pdMS_TO_TICKS(MB_POLL_PERIOD_MS);
    if (period_ticks == 0) {
        period_ticks = 1;
    }

    if (MB_FIRST_POLL_DELAY_MS > 0U) {
        MB_LOG_DEBUG(TAG, "First poll delay: %u ms", (unsigned)MB_FIRST_POLL_DELAY_MS);
        vTaskDelay(pdMS_TO_TICKS(MB_FIRST_POLL_DELAY_MS));
    }

    while (true) {
        esp_err_t err = modbus_sample_and_store_cycle();
        if (err != ESP_OK) {
            LOG_WARNING(TAG, "Cycle completed without persistence: err=0x%x", err);
        }
        vTaskDelay(period_ticks);
    }
}

esp_err_t modbus_manager_set_sample_sink(modbus_sample_sink_t sink, void *ctx)
{
    s_sample_sink = sink;
    s_sample_sink_ctx = ctx;
    return ESP_OK;
}

esp_err_t modbus_manager_start(void)
{
    if (s_started) {
        return ESP_OK;
    }

    if (s_sample_sink == NULL) {
        LOG_WARNING(TAG, "No sample sink configured: Modbus cycles will not be persisted");
    }

    esp_err_t err = modbus_init(
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

    BaseType_t task_ok = xTaskCreate(
        modbus_sampling_task,
        "modbus_sampling",
        MB_TASK_STACK_SIZE,
        NULL,
        MB_TASK_PRIORITY,
        &s_task_handle
    );
    if (task_ok != pdPASS) {
        LOG_ERROR(TAG, "Failed to create Modbus sampling task");
        return ESP_ERR_NO_MEM;
    }

    s_started = true;
    if (probe_ok) {
        LOG_OK(
            TAG,
            "Started: slave=%u mode=periodic-sample-publish period_ms=%u",
            (unsigned)MB_SLAVE_ADDR,
            (unsigned)MB_POLL_PERIOD_MS
        );
    } else {
        LOG_WARNING(
            TAG,
            "Started with warnings: slave=%u mode=periodic-sample-publish period_ms=%u",
            (unsigned)MB_SLAVE_ADDR,
            (unsigned)MB_POLL_PERIOD_MS
        );
    }

    return ESP_OK;
}

bool modbus_manager_is_running(void)
{
    return s_started;
}

#else

esp_err_t modbus_manager_set_sample_sink(modbus_sample_sink_t sink, void *ctx)
{
    (void)sink;
    (void)ctx;
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
