#include "modbus_manager.h"

#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logging.h"
#include "modbus_master.h"
#include "modbus_io.h"
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
#define MB_INIT_DELAY_MS          500U
#define MB_FIRST_POLL_DELAY_MS    500U
#define MB_TEST_REQ_DELAY_MS      25U
#define MB_PROBE_REG              0x0000U
#define MB_MAX_REQ_WORDS          125U
#define MB_MAX_FAIL_REPORT_LINES  128U
#define MB_MAX_RAM_ENTRIES        512U
#define MB_MAX_RAM_WORDS_PER_ENTRY 4U

typedef struct {
    uint16_t index;
    uint8_t fc;
    uint16_t start;
    uint16_t count;
    esp_err_t err;
    const char *name;
} modbus_fail_entry_t;

typedef struct {
    bool valid;
    uint8_t fc;
    uint16_t start;
    uint16_t count;
    uint16_t words[MB_MAX_RAM_WORDS_PER_ENTRY];
} modbus_ram_entry_t;

static TaskHandle_t s_task_handle = NULL;
static bool s_started = false;
static uint16_t s_read_buf[MB_MAX_REQ_WORDS] = {0};
static modbus_ram_entry_t s_ram_entries[MB_MAX_RAM_ENTRIES] = {0};
static uint32_t s_ram_entries_count = 0U;
static uint32_t s_ram_entries_dropped = 0U;
static modbus_fail_entry_t s_fail_entries[MB_MAX_FAIL_REPORT_LINES] = {0};

static void modbus_record_fail(
    uint32_t index,
    const MultimeterRegister *reg,
    esp_err_t err,
    uint32_t *fail_counter
)
{
    if (fail_counter == NULL || reg == NULL) {
        return;
    }
    ++(*fail_counter);
    if (*fail_counter <= MB_MAX_FAIL_REPORT_LINES) {
        modbus_fail_entry_t *entry = &s_fail_entries[*fail_counter - 1U];
        entry->index = (uint16_t)index;
        entry->fc = reg->function_code;
        entry->start = reg->reg_start;
        entry->count = reg->reg_count;
        entry->err = err;
        entry->name = reg->name;
    }
}

static void modbus_ram_store(uint32_t index, const MultimeterRegister *reg, const uint16_t *data)
{
    if (reg == NULL || data == NULL) {
        return;
    }
    if (index >= MB_MAX_RAM_ENTRIES) {
        ++s_ram_entries_dropped;
        return;
    }

    modbus_ram_entry_t *entry = &s_ram_entries[index];
    memset(entry, 0, sizeof(*entry));
    entry->valid = true;
    entry->fc = reg->function_code;
    entry->start = reg->reg_start;
    entry->count = reg->reg_count;

    uint16_t copy_words = reg->reg_count;
    if (copy_words > MB_MAX_RAM_WORDS_PER_ENTRY) {
        copy_words = MB_MAX_RAM_WORDS_PER_ENTRY;
    }
    memcpy(entry->words, data, (size_t)copy_words * sizeof(uint16_t));
    ++s_ram_entries_count;
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

static void modbus_validate_ump209_register_set(void)
{
    const MultimeterRegisterSet *set = ump209_get_target_register_set();
    if (set == NULL || set->registers == NULL || set->size == 0U) {
        LOG_ERROR(TAG, "UPM209 register set unavailable");
        return;
    }

    uint32_t total = (uint32_t)set->size;
    uint32_t ok = 0U;
    uint32_t fail = 0U;
    uint32_t skipped = 0U;
    uint32_t requests = 0U;
    memset(s_fail_entries, 0, sizeof(s_fail_entries));
    memset(s_ram_entries, 0, sizeof(s_ram_entries));
    s_ram_entries_count = 0U;
    s_ram_entries_dropped = 0U;

    LOG_INFO(
        TAG,
        "TEST REPORT START: register-set validation slave=%u entries=%" PRIu32,
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
        LOG_DEBUG(
            TAG,
            "Block request: fc=0x%02X start=0x%04X count=%u entries=%" PRIu32,
            (unsigned)block_fc,
            (unsigned)block_start,
            (unsigned)block_count,
            (uint32_t)(j - i)
        );
        esp_err_t block_err = modbus_read_by_fc(block_fc, block_start, block_count, s_read_buf);

        if (block_err == ESP_OK) {
            for (uint32_t k = i; k < j; ++k) {
                const MultimeterRegister *reg = &set->registers[k];
                uint16_t offset = (uint16_t)(reg->reg_start - block_start);
                if ((uint16_t)(offset + reg->reg_count) > block_count) {
                    modbus_record_fail(k, reg, ESP_ERR_INVALID_SIZE, &fail);
                    continue;
                }
                ++ok;
                modbus_ram_store(k, reg, &s_read_buf[offset]);
            }
        } else {
            for (uint32_t k = i; k < j; ++k) {
                const MultimeterRegister *reg = &set->registers[k];
                modbus_record_fail(k, reg, block_err, &fail);
            }
        }

        i = j;
        if (MB_TEST_REQ_DELAY_MS > 0U) {
            vTaskDelay(pdMS_TO_TICKS(MB_TEST_REQ_DELAY_MS));
        }
    }

    LOG_INFO(
        TAG,
        "TEST SUMMARY: total=%" PRIu32 " ok=%" PRIu32 " fail=%" PRIu32 " skipped=%" PRIu32 " requests=%" PRIu32,
        total,
        ok,
        fail,
        skipped,
        requests
    );

    const uint32_t printed = (fail < MB_MAX_FAIL_REPORT_LINES) ? fail : MB_MAX_FAIL_REPORT_LINES;
    for (uint32_t i = 0U; i < printed; ++i) {
        const modbus_fail_entry_t *e = &s_fail_entries[i];
        LOG_WARNING(
            TAG,
            "FAIL [%u] fc=0x%02X start=0x%04X count=%u err=0x%x name=%s",
            (unsigned)e->index,
            (unsigned)e->fc,
            (unsigned)e->start,
            (unsigned)e->count,
            e->err,
            (e->name != NULL) ? e->name : "(null)"
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

    if (fail == 0U && skipped == 0U) {
        LOG_OK(TAG, "ALL UPM209 REGISTER-SET ENTRIES RESPONDED");
    }
    LOG_INFO(
        TAG,
        "RAM STORE SUMMARY: stored=%" PRIu32 " dropped=%" PRIu32 " capacity=%u",
        s_ram_entries_count,
        s_ram_entries_dropped,
        (unsigned)MB_MAX_RAM_ENTRIES
    );
    LOG_INFO(TAG, "TEST REPORT END");
}

static void modbus_sampling_task(void *arg)
{
    (void)arg;

    if (MB_FIRST_POLL_DELAY_MS > 0U) {
        LOG_DEBUG(TAG, "First poll delay: %u ms", (unsigned)MB_FIRST_POLL_DELAY_MS);
        vTaskDelay(pdMS_TO_TICKS(MB_FIRST_POLL_DELAY_MS));
    }

    modbus_validate_ump209_register_set();
    LOG_INFO(TAG, "One-shot validation completed, task stopping");
    s_started = false;
    s_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t modbus_manager_start(void)
{
    if (s_started) {
        return ESP_OK;
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
        CONFIG_MODBUS_MANAGER_TASK_STACK_SIZE,
        NULL,
        CONFIG_MODBUS_MANAGER_TASK_PRIORITY,
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
            "Started: slave=%u mode=ump209-regset-validation one-shot",
            (unsigned)MB_SLAVE_ADDR
        );
    } else {
        LOG_WARNING(
            TAG,
            "Started with warnings: slave=%u mode=ump209-regset-validation one-shot",
            (unsigned)MB_SLAVE_ADDR
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
