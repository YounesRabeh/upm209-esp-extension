#include "processing_service.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "internet.h"
#include "logging.h"
#include "memory.h"
#include "outlier_handling.h"
#include "sdkconfig.h"

#define TAG "PROC_SVC"

#define PS_WINDOW_SAMPLES       6U
#define PS_TASK_STACK_SIZE      12288U
#define PS_TASK_PRIORITY        6U
#define PS_START_DELAY_MS       3000U
#define PS_IDLE_DELAY_MS        1000U
#define PS_RETRY_DELAY_MS       3000U
#define PS_PARSE_DELAY_MS       1000U
#define PS_JSON_ALLOC_GUARD     512U
#define PS_JSON_MAX_BYTES       262144U
#define PS_COMPANY_ID           "UNICAM"
#define PS_SCHEMA_ID            "schemaUNICAM"
#define PS_DEVICE_TYPE          "UPM209"
#define PS_LOG_JSON_DEBUG       0U

static TaskHandle_t s_task_handle = NULL;
static bool s_started = false;

static volatile uint32_t s_windows_attempted = 0U;
static volatile uint32_t s_windows_sent = 0U;
static volatile uint32_t s_send_retries = 0U;
static volatile uint32_t s_parse_failures = 0U;
static volatile uint32_t s_samples_consumed = 0U;

static void processing_service_get_device_id(char *out, size_t out_len)
{
    if (out == NULL || out_len == 0U) {
        return;
    }

    uint8_t mac[6] = {0};
    if (esp_efuse_mac_get_default(mac) != ESP_OK) {
        snprintf(out, out_len, "000000000000");
        return;
    }

    snprintf(
        out,
        out_len,
        "%02X%02X%02X%02X%02X%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
    );
}

static const char *processing_service_get_fw_version(void)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();
    if (app_desc == NULL || app_desc->version[0] == '\0') {
        return "unknown";
    }
    return app_desc->version;
}

static bool processing_json_append_char(char **dst, size_t *remaining, char c)
{
    if (dst == NULL || *dst == NULL || remaining == NULL || *remaining <= 1U) {
        return false;
    }
    **dst = c;
    (*dst)++;
    **dst = '\0';
    (*remaining)--;
    return true;
}

static bool processing_json_append_raw(
    char **dst,
    size_t *remaining,
    const char *src,
    size_t src_len
)
{
    if (dst == NULL || *dst == NULL || remaining == NULL || src == NULL) {
        return false;
    }
    if (*remaining <= (src_len + 1U)) {
        return false;
    }
    memcpy(*dst, src, src_len);
    *dst += src_len;
    **dst = '\0';
    *remaining -= src_len;
    return true;
}

static bool processing_json_append_fmt(
    char **dst,
    size_t *remaining,
    const char *fmt,
    ...
)
{
    if (dst == NULL || *dst == NULL || remaining == NULL || fmt == NULL || *remaining == 0U) {
        return false;
    }

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(*dst, *remaining, fmt, args);
    va_end(args);
    if (written < 0 || (size_t)written >= *remaining) {
        return false;
    }

    *dst += (size_t)written;
    *remaining -= (size_t)written;
    return true;
}

static bool processing_json_append_escaped_string(
    char **dst,
    size_t *remaining,
    const char *value
)
{
    const char *in = (value != NULL) ? value : "";
    if (!processing_json_append_char(dst, remaining, '"')) {
        return false;
    }

    while (*in != '\0') {
        unsigned char ch = (unsigned char)*in++;
        switch (ch) {
            case '"':
                if (!processing_json_append_raw(dst, remaining, "\\\"", 2U)) {
                    return false;
                }
                break;
            case '\\':
                if (!processing_json_append_raw(dst, remaining, "\\\\", 2U)) {
                    return false;
                }
                break;
            case '\b':
                if (!processing_json_append_raw(dst, remaining, "\\b", 2U)) {
                    return false;
                }
                break;
            case '\f':
                if (!processing_json_append_raw(dst, remaining, "\\f", 2U)) {
                    return false;
                }
                break;
            case '\n':
                if (!processing_json_append_raw(dst, remaining, "\\n", 2U)) {
                    return false;
                }
                break;
            case '\r':
                if (!processing_json_append_raw(dst, remaining, "\\r", 2U)) {
                    return false;
                }
                break;
            case '\t':
                if (!processing_json_append_raw(dst, remaining, "\\t", 2U)) {
                    return false;
                }
                break;
            default:
                if (ch < 0x20U) {
                    if (!processing_json_append_fmt(dst, remaining, "\\u%04X", (unsigned)ch)) {
                        return false;
                    }
                } else {
                    if (!processing_json_append_char(dst, remaining, (char)ch)) {
                        return false;
                    }
                }
                break;
        }
    }

    return processing_json_append_char(dst, remaining, '"');
}

static bool processing_service_build_json_payload(
    uint32_t timestamp_s,
    const processing_measurement_t *measurements,
    size_t measurement_count,
    char *out_buf,
    size_t out_buf_len
)
{
    if (measurements == NULL || measurement_count == 0U || out_buf == NULL || out_buf_len == 0U) {
        return false;
    }

    out_buf[0] = '\0';
    char *w = out_buf;
    size_t remaining = out_buf_len;

    char device_id[32] = {0};
    processing_service_get_device_id(device_id, sizeof(device_id));
    const char *fw_version = processing_service_get_fw_version();

    if (!processing_json_append_char(&w, &remaining, '{')) {
        return false;
    }

    if (!processing_json_append_raw(&w, &remaining, "\"schemaID\":", 11U) ||
        !processing_json_append_escaped_string(&w, &remaining, PS_SCHEMA_ID) ||
        !processing_json_append_raw(&w, &remaining, ",\"companyID\":", 13U) ||
        !processing_json_append_escaped_string(&w, &remaining, PS_COMPANY_ID) ||
        !processing_json_append_fmt(&w, &remaining, ",\"timestamp\":%" PRIu32, timestamp_s) ||
        !processing_json_append_raw(&w, &remaining, ",\"device_id\":", 13U) ||
        !processing_json_append_escaped_string(&w, &remaining, device_id) ||
        !processing_json_append_raw(&w, &remaining, ",\"firmware_version\":", 20U) ||
        !processing_json_append_escaped_string(&w, &remaining, fw_version) ||
        !processing_json_append_raw(&w, &remaining, ",\"device_type\":", 15U) ||
        !processing_json_append_escaped_string(&w, &remaining, PS_DEVICE_TYPE) ||
        !processing_json_append_raw(&w, &remaining, ",\"measurements\":[", 17U)) {
        return false;
    }

    for (size_t i = 0; i < measurement_count; ++i) {
        const processing_measurement_t *m = &measurements[i];
        if (i > 0U) {
            if (!processing_json_append_char(&w, &remaining, ',')) {
                return false;
            }
        }

        if (!processing_json_append_fmt(
                &w,
                &remaining,
                "{\"num_reg\":%u,\"avg\":%.9g,\"word\":%u,\"min\":%.9g,\"max\":%.9g,\"unit\":",
                (unsigned)m->num_reg,
                m->value,
                (unsigned)m->word,
                m->min,
                m->max) ||
            !processing_json_append_escaped_string(&w, &remaining, m->unit) ||
            !processing_json_append_raw(&w, &remaining, ",\"description\":", 15U) ||
            !processing_json_append_escaped_string(&w, &remaining, m->description) ||
            !processing_json_append_char(&w, &remaining, '}')) {
            return false;
        }
    }

    if (!processing_json_append_raw(&w, &remaining, "]}", 2U)) {
        return false;
    }

    return true;
}

static size_t processing_json_escaped_len(const char *value)
{
    size_t len = 0U;
    const unsigned char *in = (const unsigned char *)((value != NULL) ? value : "");
    while (*in != '\0') {
        unsigned char ch = *in++;
        switch (ch) {
            case '"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                len += 2U;
                break;
            default:
                if (ch < 0x20U) {
                    len += 6U; // \u00XX
                } else {
                    len += 1U;
                }
                break;
        }
    }
    return len;
}

static size_t processing_json_measurement_len(const processing_measurement_t *m)
{
    if (m == NULL) {
        return 0U;
    }

    size_t len = 0U;
    int fixed = snprintf(
        NULL,
        0,
        "{\"num_reg\":%u,\"avg\":%.9g,\"word\":%u,\"min\":%.9g,\"max\":%.9g,\"unit\":",
        (unsigned)m->num_reg,
        m->value,
        (unsigned)m->word,
        m->min,
        m->max
    );
    if (fixed < 0) {
        return 0U;
    }
    len += (size_t)fixed;
    len += 2U + processing_json_escaped_len(m->unit); // "unit"
    len += 15U; // ,"description":
    len += 2U + processing_json_escaped_len(m->description); // "description"
    len += 1U; // }
    return len;
}

static size_t processing_service_estimate_json_len(
    uint32_t timestamp_s,
    const processing_measurement_t *measurements,
    size_t measurement_count
)
{
    char device_id[32] = {0};
    processing_service_get_device_id(device_id, sizeof(device_id));
    const char *fw_version = processing_service_get_fw_version();

    size_t len = 0U;
    len += 1U; // {

    len += 11U + 2U + processing_json_escaped_len(PS_SCHEMA_ID);  // "schemaID":
    len += 13U + 2U + processing_json_escaped_len(PS_COMPANY_ID); // ,"companyID":
    int ts_len = snprintf(NULL, 0, ",\"timestamp\":%" PRIu32, timestamp_s);
    if (ts_len < 0) {
        return 0U;
    }
    len += (size_t)ts_len;
    len += 13U + 2U + processing_json_escaped_len(device_id);     // ,"device_id":
    len += 20U + 2U + processing_json_escaped_len(fw_version);    // ,"firmware_version":
    len += 15U + 2U + processing_json_escaped_len(PS_DEVICE_TYPE);// ,"device_type":
    len += 17U; // ,"measurements":[

    for (size_t i = 0; i < measurement_count; ++i) {
        if (i > 0U) {
            len += 1U; // comma
        }
        size_t ml = processing_json_measurement_len(&measurements[i]);
        if (ml == 0U) {
            return 0U;
        }
        len += ml;
    }

    len += 2U; // ]}
    len += 1U; // NUL
    return len;
}

static char *processing_service_alloc_json_buf(size_t needed_bytes)
{
    if (needed_bytes == 0U || needed_bytes > PS_JSON_MAX_BYTES) {
        return NULL;
    }

    char *buf = heap_caps_malloc(needed_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf == NULL) {
        buf = heap_caps_malloc(needed_bytes, MALLOC_CAP_8BIT);
    }
    return buf;
}

static esp_err_t processing_service_pop_oldest(uint16_t *scratch_words)
{
    if (scratch_words == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = memory_pop_modbus_sample(NULL, scratch_words, CONFIG_MEMORY_MAX_REGISTERS);
    if (err == ESP_ERR_INVALID_CRC) {
        LOG_WARNING(TAG, "Dropped corrupted oldest sample");
        return ESP_OK;
    }
    return err;
}

static void processing_service_task(void *arg)
{
    (void)arg;

    processing_upm209_layout_info_t layout = {0};
    esp_err_t err = processing_upm209_get_layout_info(&layout);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "Failed to load UPM209 layout: 0x%x", err);
        vTaskDelete(NULL);
        return;
    }
    if (layout.expected_cycle_words > CONFIG_MEMORY_MAX_REGISTERS) {
        LOG_ERROR(
            TAG,
            "Layout too large: expected_words=%u max=%u",
            (unsigned)layout.expected_cycle_words,
            (unsigned)CONFIG_MEMORY_MAX_REGISTERS
        );
        vTaskDelete(NULL);
        return;
    }

    processing_measurement_t *measurements = pvPortMalloc(
        layout.measurement_count * sizeof(processing_measurement_t)
    );
    if (measurements == NULL) {
        LOG_ERROR(TAG, "Failed to allocate measurements buffer");
        vTaskDelete(NULL);
        return;
    }

    uint16_t *window_words[PS_WINDOW_SAMPLES] = {0};
    for (uint32_t i = 0U; i < PS_WINDOW_SAMPLES; ++i) {
        window_words[i] = pvPortMalloc((size_t)CONFIG_MEMORY_MAX_REGISTERS * sizeof(uint16_t));
        if (window_words[i] == NULL) {
            LOG_ERROR(TAG, "Failed to allocate cycle buffer[%u]", (unsigned)i);
            for (uint32_t j = 0U; j < i; ++j) {
                vPortFree(window_words[j]);
            }
            vPortFree(measurements);
            vTaskDelete(NULL);
            return;
        }
    }

    memory_sample_meta_t metas[PS_WINDOW_SAMPLES] = {0};
    processing_raw_cycle_t cycles[PS_WINDOW_SAMPLES] = {0};

    if (PS_START_DELAY_MS > 0U) {
        LOG_INFO(TAG, "Startup delay: %u ms", (unsigned)PS_START_DELAY_MS);
        vTaskDelay(pdMS_TO_TICKS(PS_START_DELAY_MS));
    }

    while (true) {
        uint32_t pending = memory_pending_samples();
        if (pending < PS_WINDOW_SAMPLES) {
            vTaskDelay(pdMS_TO_TICKS(PS_IDLE_DELAY_MS));
            continue;
        }

        bool window_ready = true;
        bool window_valid = true;
        uint32_t newest_timestamp_s = 0U;

        for (uint32_t i = 0U; i < PS_WINDOW_SAMPLES; ++i) {
            err = memory_peek_modbus_sample_at(
                i,
                &metas[i],
                window_words[i],
                CONFIG_MEMORY_MAX_REGISTERS
            );
            if (err == ESP_ERR_NOT_FOUND) {
                window_ready = false;
                break;
            }
            if (err != ESP_OK) {
                LOG_WARNING(TAG, "Window peek failed at index=%u err=0x%x", (unsigned)i, err);
                window_valid = false;
                break;
            }

            if (metas[i].reg_count != layout.expected_cycle_words ||
                metas[i].start_reg != layout.expected_start_reg) {
                LOG_WARNING(
                    TAG,
                    "Invalid sample shape at index=%u start=0x%04X count=%u expected_start=0x%04X expected_count=%u",
                    (unsigned)i,
                    (unsigned)metas[i].start_reg,
                    (unsigned)metas[i].reg_count,
                    (unsigned)layout.expected_start_reg,
                    (unsigned)layout.expected_cycle_words
                );
                window_valid = false;
                break;
            }

            cycles[i].words = window_words[i];
            cycles[i].word_count = metas[i].reg_count;
            if (metas[i].timestamp_s > newest_timestamp_s) {
                newest_timestamp_s = metas[i].timestamp_s;
            }
        }

        if (!window_ready) {
            vTaskDelay(pdMS_TO_TICKS(PS_IDLE_DELAY_MS));
            continue;
        }

        if (!window_valid) {
            ++s_parse_failures;
            esp_err_t pop_err = processing_service_pop_oldest(window_words[0]);
            if (pop_err == ESP_OK) {
                ++s_samples_consumed;
            } else if (pop_err != ESP_ERR_NOT_FOUND) {
                LOG_WARNING(TAG, "Failed to drop oldest sample: err=0x%x", pop_err);
            }
            vTaskDelay(pdMS_TO_TICKS(PS_PARSE_DELAY_MS));
            continue;
        }

        ++s_windows_attempted;
        size_t measurement_count = 0U;
        err = processing_upm209_compute_window(
            cycles,
            PS_WINDOW_SAMPLES,
            measurements,
            layout.measurement_count,
            &measurement_count
        );
        if (err != ESP_OK) {
            ++s_parse_failures;
            LOG_WARNING(TAG, "Window processing failed: err=0x%x", err);
            esp_err_t pop_err = processing_service_pop_oldest(window_words[0]);
            if (pop_err == ESP_OK) {
                ++s_samples_consumed;
            } else if (pop_err != ESP_ERR_NOT_FOUND) {
                LOG_WARNING(TAG, "Failed to drop oldest sample: err=0x%x", pop_err);
            }
            vTaskDelay(pdMS_TO_TICKS(PS_PARSE_DELAY_MS));
            continue;
        }

        size_t estimated_len = processing_service_estimate_json_len(
            newest_timestamp_s,
            measurements,
            measurement_count
        );
        if (estimated_len == 0U || estimated_len > PS_JSON_MAX_BYTES) {
            ++s_send_retries;
            LOG_WARNING(
                TAG,
                "Invalid JSON estimate: measurements=%u estimate=%u",
                (unsigned)measurement_count,
                (unsigned)estimated_len
            );
            vTaskDelay(pdMS_TO_TICKS(PS_RETRY_DELAY_MS));
            continue;
        }

        size_t alloc_len = estimated_len + PS_JSON_ALLOC_GUARD;
        if (alloc_len > PS_JSON_MAX_BYTES) {
            alloc_len = PS_JSON_MAX_BYTES;
        }
        char *json_payload_buf = processing_service_alloc_json_buf(alloc_len);
        if (json_payload_buf == NULL) {
            ++s_send_retries;
            LOG_WARNING(
                TAG,
                "Failed to allocate JSON buffer (%u bytes)",
                (unsigned)alloc_len
            );
            vTaskDelay(pdMS_TO_TICKS(PS_RETRY_DELAY_MS));
            continue;
        }

        bool payload_ok = processing_service_build_json_payload(
            newest_timestamp_s,
            measurements,
            measurement_count,
            json_payload_buf,
            alloc_len
        );
        if (!payload_ok) {
            heap_caps_free(json_payload_buf);

            alloc_len *= 2U;
            if (alloc_len > PS_JSON_MAX_BYTES) {
                alloc_len = PS_JSON_MAX_BYTES;
            }
            json_payload_buf = processing_service_alloc_json_buf(alloc_len);
            if (json_payload_buf == NULL) {
                ++s_send_retries;
                LOG_WARNING(
                    TAG,
                    "Failed to allocate JSON retry buffer (%u bytes)",
                    (unsigned)alloc_len
                );
                vTaskDelay(pdMS_TO_TICKS(PS_RETRY_DELAY_MS));
                continue;
            }

            payload_ok = processing_service_build_json_payload(
                newest_timestamp_s,
                measurements,
                measurement_count,
                json_payload_buf,
                alloc_len
            );
        }

        if (!payload_ok) {
            heap_caps_free(json_payload_buf);
            ++s_send_retries;
            LOG_WARNING(
                TAG,
                "Failed to build JSON payload: measurements=%u estimate=%u alloc=%u",
                (unsigned)measurement_count,
                (unsigned)estimated_len,
                (unsigned)alloc_len
            );
            vTaskDelay(pdMS_TO_TICKS(PS_RETRY_DELAY_MS));
            continue;
        }

#if PS_LOG_JSON_DEBUG
        LOG_DEBUG(
            TAG,
            "JSON payload (%u bytes): %s",
            (unsigned)strlen(json_payload_buf),
            json_payload_buf
        );
#endif
        err = internet_send_data(json_payload_buf);
        heap_caps_free(json_payload_buf);
        if (err != ESP_OK) {
            ++s_send_retries;
            LOG_WARNING(
                TAG,
                "Upload failed: err=0x%x retry_in_ms=%u queue_pending=%" PRIu32,
                err,
                (unsigned)PS_RETRY_DELAY_MS,
                memory_pending_samples()
            );
            vTaskDelay(pdMS_TO_TICKS(PS_RETRY_DELAY_MS));
            continue;
        }

        uint32_t popped = 0U;
        for (uint32_t i = 0U; i < PS_WINDOW_SAMPLES; ++i) {
            err = memory_pop_modbus_sample(NULL, window_words[0], CONFIG_MEMORY_MAX_REGISTERS);
            if (err == ESP_ERR_INVALID_CRC) {
                LOG_WARNING(TAG, "Dropped corrupted sample during post-send consume");
                ++popped;
                continue;
            }
            if (err != ESP_OK) {
                LOG_WARNING(
                    TAG,
                    "Pop after send failed at step=%u err=0x%x",
                    (unsigned)i,
                    err
                );
                break;
            }
            ++popped;
        }

        s_samples_consumed += popped;
        if (popped == PS_WINDOW_SAMPLES) {
            ++s_windows_sent;
            uint32_t pending_after_send = memory_pending_samples();
            LOG_OK(
                TAG,
                "Upload OK: window=%u measurements=%u ts=%" PRIu32 " sent=%" PRIu32 " retries=%" PRIu32 " parse_fail=%" PRIu32 " consumed=%" PRIu32 " queue_pending=%" PRIu32 " used=%" PRIu32 "B",
                (unsigned)PS_WINDOW_SAMPLES,
                (unsigned)measurement_count,
                newest_timestamp_s,
                s_windows_sent,
                s_send_retries,
                s_parse_failures,
                s_samples_consumed,
                pending_after_send,
                memory_used_bytes()
            );
        } else {
            LOG_WARNING(
                TAG,
                "Partial pop after send: popped=%u expected=%u",
                (unsigned)popped,
                (unsigned)PS_WINDOW_SAMPLES
            );
        }
    }
}

esp_err_t processing_service_start(void)
{
    if (s_started) {
        return ESP_OK;
    }

    esp_err_t err = memory_init();
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "memory_init failed: 0x%x", err);
        return err;
    }

    processing_upm209_layout_info_t layout = {0};
    err = processing_upm209_get_layout_info(&layout);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "Failed to initialize processing layout: 0x%x", err);
        return err;
    }
    if (layout.expected_cycle_words > CONFIG_MEMORY_MAX_REGISTERS) {
        LOG_ERROR(
            TAG,
            "Layout exceeds memory buffer: expected_words=%u max=%u",
            (unsigned)layout.expected_cycle_words,
            (unsigned)CONFIG_MEMORY_MAX_REGISTERS
        );
        return ESP_ERR_INVALID_SIZE;
    }

    BaseType_t ok = xTaskCreate(
        processing_service_task,
        "processing_service",
        PS_TASK_STACK_SIZE,
        NULL,
        PS_TASK_PRIORITY,
        &s_task_handle
    );
    if (ok != pdPASS) {
        LOG_ERROR(TAG, "Failed to create processing task");
        return ESP_ERR_NO_MEM;
    }

    s_windows_attempted = 0U;
    s_windows_sent = 0U;
    s_send_retries = 0U;
    s_parse_failures = 0U;
    s_samples_consumed = 0U;

    s_started = true;
    LOG_OK(
        TAG,
        "Started: prio=%u window=%u expected_words=%u measurement_count=%u",
        (unsigned)PS_TASK_PRIORITY,
        (unsigned)PS_WINDOW_SAMPLES,
        (unsigned)layout.expected_cycle_words,
        (unsigned)layout.measurement_count
    );
    return ESP_OK;
}

bool processing_service_is_running(void)
{
    return s_started;
}
