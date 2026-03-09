#include "memory.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "esp_check.h"
#include "esp_littlefs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "logging.h"
#include "sdkconfig.h"

#define TAG "MEMORY"

#define MEMORY_META_MAGIC   0x4D455441UL
#define MEMORY_META_VERSION 1U
#define RECORD_MAGIC_DATA   0x52454344UL
#define RECORD_MAGIC_WRAP   0x57524150UL

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t timestamp_s;
    uint16_t start_reg;
    uint16_t reg_count;
    uint8_t slave_addr;
    uint8_t flags;
    uint16_t payload_crc16;
} memory_record_header_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint32_t capacity;
    uint32_t head;
    uint32_t tail;
    uint32_t used;
    uint32_t count;
} memory_meta_t;

_Static_assert(sizeof(memory_record_header_t) == 16, "Unexpected header size");

static SemaphoreHandle_t s_lock;
static bool s_ready;
static bool s_mounted;
static memory_meta_t s_meta;
static char s_base_dir[96];
static char s_data_path[128];
static char s_meta_path[128];

static uint16_t crc16_modbus(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            if (crc & 1U) {
                crc = (crc >> 1U) ^ 0xA001U;
            } else {
                crc >>= 1U;
            }
        }
    }
    return crc;
}

static esp_err_t write_meta_locked(void)
{
    FILE *fp = fopen(s_meta_path, "wb");
    ESP_RETURN_ON_FALSE(fp != NULL, ESP_FAIL, TAG, "Failed to open meta file for write");

    size_t wr = fwrite(&s_meta, 1, sizeof(s_meta), fp);
    fclose(fp);
    ESP_RETURN_ON_FALSE(wr == sizeof(s_meta), ESP_FAIL, TAG, "Failed to write meta file");
    return ESP_OK;
}

static esp_err_t read_at_locked(uint32_t offset, void *buf, size_t len)
{
    FILE *fp = fopen(s_data_path, "rb");
    ESP_RETURN_ON_FALSE(fp != NULL, ESP_FAIL, TAG, "Failed to open data file for read");
    if (fseek(fp, (long)offset, SEEK_SET) != 0) {
        LOG_ERROR(TAG, "fseek read failed");
        goto fail;
    }

    size_t rd = fread(buf, 1, len, fp);
    fclose(fp);
    ESP_RETURN_ON_FALSE(rd == len, ESP_FAIL, TAG, "fread failed (offset=%" PRIu32 ", len=%u)", offset, (unsigned)len);
    return ESP_OK;

fail:
    fclose(fp);
    return ESP_FAIL;
}

static esp_err_t write_at_locked(uint32_t offset, const void *buf, size_t len)
{
    FILE *fp = fopen(s_data_path, "rb+");
    ESP_RETURN_ON_FALSE(fp != NULL, ESP_FAIL, TAG, "Failed to open data file for write");
    if (fseek(fp, (long)offset, SEEK_SET) != 0) {
        LOG_ERROR(TAG, "fseek write failed");
        goto fail;
    }

    size_t wr = fwrite(buf, 1, len, fp);
    fflush(fp);
    fclose(fp);
    ESP_RETURN_ON_FALSE(wr == len, ESP_FAIL, TAG, "fwrite failed (offset=%" PRIu32 ", len=%u)", offset, (unsigned)len);
    return ESP_OK;

fail:
    fclose(fp);
    return ESP_FAIL;
}

static esp_err_t ensure_directory(const char *path)
{
    struct stat st = {0};
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return ESP_OK;
        }
        LOG_ERROR(TAG, "%s exists and is not a directory", path);
        return ESP_FAIL;
    }

    if (mkdir(path, 0775) != 0 && errno != EEXIST) {
        LOG_ERROR(TAG, "mkdir(%s) failed: errno=%d", path, errno);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t ensure_data_file_locked(void)
{
    struct stat st = {0};
    const uint32_t cap = CONFIG_MEMORY_SAMPLE_QUEUE_BYTES;

    if (stat(s_data_path, &st) == 0) {
        if ((uint32_t)st.st_size == cap) {
            return ESP_OK;
        }
        LOG_WARNING(TAG, "Data file size mismatch (%ld != %" PRIu32 "), recreating", (long)st.st_size, cap);
    }

    FILE *fp = fopen(s_data_path, "wb");
    ESP_RETURN_ON_FALSE(fp != NULL, ESP_FAIL, TAG, "Failed to create data file");
    if (fseek(fp, (long)(cap - 1U), SEEK_SET) != 0) {
        LOG_ERROR(TAG, "Failed to set data file size");
        goto fail;
    }
    if (fputc(0, fp) == EOF) {
        LOG_ERROR(TAG, "Failed to finalize data file size");
        goto fail;
    }
    fclose(fp);
    return ESP_OK;

fail:
    fclose(fp);
    return ESP_FAIL;
}

static esp_err_t reset_meta_locked(void)
{
    memset(&s_meta, 0, sizeof(s_meta));
    s_meta.magic = MEMORY_META_MAGIC;
    s_meta.version = MEMORY_META_VERSION;
    s_meta.capacity = CONFIG_MEMORY_SAMPLE_QUEUE_BYTES;
    return write_meta_locked();
}

static esp_err_t load_or_create_meta_locked(void)
{
    FILE *fp = fopen(s_meta_path, "rb");
    if (fp == NULL) {
        return reset_meta_locked();
    }

    memory_meta_t on_disk = {0};
    size_t rd = fread(&on_disk, 1, sizeof(on_disk), fp);
    fclose(fp);
    if (rd != sizeof(on_disk)) {
        LOG_WARNING(TAG, "Meta file truncated, resetting");
        return reset_meta_locked();
    }

    if (on_disk.magic != MEMORY_META_MAGIC ||
        on_disk.version != MEMORY_META_VERSION ||
        on_disk.capacity != CONFIG_MEMORY_SAMPLE_QUEUE_BYTES ||
        on_disk.head >= on_disk.capacity ||
        on_disk.tail >= on_disk.capacity ||
        on_disk.used > on_disk.capacity) {
        LOG_WARNING(TAG, "Meta file invalid, resetting");
        return reset_meta_locked();
    }

    s_meta = on_disk;
    return ESP_OK;
}

static esp_err_t read_header_locked(uint32_t offset, memory_record_header_t *hdr)
{
    if ((offset + sizeof(*hdr)) > s_meta.capacity) {
        return ESP_ERR_INVALID_SIZE;
    }
    return read_at_locked(offset, hdr, sizeof(*hdr));
}

static uint32_t record_size_bytes(uint16_t reg_count)
{
    return (uint32_t)sizeof(memory_record_header_t) + ((uint32_t)reg_count * sizeof(uint16_t));
}

static esp_err_t pop_oldest_locked(void)
{
    while (s_meta.count > 0U && s_meta.used > 0U) {
        if (s_meta.head > s_meta.tail) {
            uint32_t to_end = s_meta.capacity - s_meta.head;
            if (to_end < sizeof(memory_record_header_t)) {
                s_meta.head = 0U;
                ESP_RETURN_ON_ERROR(write_meta_locked(), TAG, "Failed to persist implicit wrap");
                continue;
            }
        }

        memory_record_header_t hdr = {0};
        ESP_RETURN_ON_ERROR(read_header_locked(s_meta.head, &hdr), TAG, "Failed to read oldest header");

        if (hdr.magic == RECORD_MAGIC_WRAP) {
            s_meta.head = (s_meta.head + sizeof(memory_record_header_t)) % s_meta.capacity;
            if (s_meta.used >= sizeof(memory_record_header_t)) {
                s_meta.used -= sizeof(memory_record_header_t);
            } else {
                s_meta.used = 0U;
            }
            ESP_RETURN_ON_ERROR(write_meta_locked(), TAG, "Failed to persist wrap pop");
            continue;
        }

        if (hdr.magic != RECORD_MAGIC_DATA) {
            if (s_meta.head > s_meta.tail) {
                s_meta.head = 0U;
                ESP_RETURN_ON_ERROR(write_meta_locked(), TAG, "Failed to persist head wrap");
                continue;
            }
            LOG_ERROR(TAG, "Corrupted queue at offset=%" PRIu32 ", clearing", s_meta.head);
            return reset_meta_locked();
        }

        uint32_t rec_size = record_size_bytes(hdr.reg_count);
        if (rec_size > s_meta.capacity || rec_size > s_meta.used) {
            LOG_ERROR(TAG, "Invalid record size=%" PRIu32 ", clearing", rec_size);
            return reset_meta_locked();
        }

        s_meta.head = (s_meta.head + rec_size) % s_meta.capacity;
        s_meta.used -= rec_size;
        s_meta.count--;
        return write_meta_locked();
    }

    return ESP_ERR_NOT_FOUND;
}

static esp_err_t ensure_free_space_locked(uint32_t required)
{
    ESP_RETURN_ON_FALSE(required <= s_meta.capacity, ESP_ERR_INVALID_SIZE, TAG, "Record too large for queue");

    while ((s_meta.capacity - s_meta.used) < required) {
        if (s_meta.count == 0U) {
            ESP_RETURN_ON_ERROR(reset_meta_locked(), TAG, "Failed queue reset on inconsistent state");
            break;
        }
#if CONFIG_MEMORY_OVERFLOW_OVERWRITE_OLDEST
        esp_err_t err = pop_oldest_locked();
        if (err != ESP_OK) {
            return err;
        }
#else
        return ESP_ERR_NO_MEM;
#endif
    }

    return ESP_OK;
}

static esp_err_t place_wrap_marker_if_needed_locked(uint32_t record_size)
{
    uint32_t remaining = s_meta.capacity - s_meta.tail;
    if (record_size <= remaining) {
        return ESP_OK;
    }

    if (remaining >= sizeof(memory_record_header_t)) {
        uint32_t required = record_size + (uint32_t)sizeof(memory_record_header_t);
        ESP_RETURN_ON_ERROR(ensure_free_space_locked(required), TAG, "No space for wrap marker");

        memory_record_header_t wrap = {
            .magic = RECORD_MAGIC_WRAP,
        };
        ESP_RETURN_ON_ERROR(write_at_locked(s_meta.tail, &wrap, sizeof(wrap)), TAG, "Failed writing wrap marker");
        s_meta.tail += sizeof(wrap);
        s_meta.used += sizeof(wrap);
    }

    s_meta.tail = 0U;
    return write_meta_locked();
}

static esp_err_t read_sample_locked(
    bool consume,
    memory_sample_meta_t *meta_out,
    uint16_t *registers_out,
    uint16_t max_registers
)
{
    while (s_meta.count > 0U && s_meta.used > 0U) {
        if (s_meta.head > s_meta.tail) {
            uint32_t to_end = s_meta.capacity - s_meta.head;
            if (to_end < sizeof(memory_record_header_t)) {
                s_meta.head = 0U;
                ESP_RETURN_ON_ERROR(write_meta_locked(), TAG, "Failed to persist implicit wrap");
                continue;
            }
        }

        memory_record_header_t hdr = {0};
        ESP_RETURN_ON_ERROR(read_header_locked(s_meta.head, &hdr), TAG, "Failed to read sample header");

        if (hdr.magic == RECORD_MAGIC_WRAP) {
            s_meta.head = (s_meta.head + sizeof(memory_record_header_t)) % s_meta.capacity;
            if (s_meta.used >= sizeof(memory_record_header_t)) {
                s_meta.used -= sizeof(memory_record_header_t);
            } else {
                s_meta.used = 0U;
            }
            ESP_RETURN_ON_ERROR(write_meta_locked(), TAG, "Failed to persist wrap consume");
            continue;
        }

        if (hdr.magic != RECORD_MAGIC_DATA) {
            if (s_meta.head > s_meta.tail) {
                s_meta.head = 0U;
                ESP_RETURN_ON_ERROR(write_meta_locked(), TAG, "Failed to persist head wrap");
                continue;
            }
            LOG_ERROR(TAG, "Corrupted record magic=0x%08" PRIx32 ", clearing queue", hdr.magic);
            ESP_RETURN_ON_ERROR(reset_meta_locked(), TAG, "Failed clearing corrupted queue");
            return ESP_ERR_INVALID_CRC;
        }

        if (hdr.reg_count > CONFIG_MEMORY_MAX_REGISTERS) {
            LOG_ERROR(TAG, "Invalid reg_count=%u, clearing queue", hdr.reg_count);
            ESP_RETURN_ON_ERROR(reset_meta_locked(), TAG, "Failed clearing invalid queue");
            return ESP_ERR_INVALID_SIZE;
        }

        uint32_t payload_size = (uint32_t)hdr.reg_count * sizeof(uint16_t);
        uint32_t rec_size = record_size_bytes(hdr.reg_count);
        if (rec_size > s_meta.capacity || rec_size > s_meta.used) {
            LOG_ERROR(TAG, "Record size invalid (%" PRIu32 "), clearing queue", rec_size);
            ESP_RETURN_ON_ERROR(reset_meta_locked(), TAG, "Failed clearing invalid queue");
            return ESP_ERR_INVALID_SIZE;
        }

        ESP_RETURN_ON_FALSE(max_registers >= hdr.reg_count, ESP_ERR_INVALID_SIZE, TAG, "Output buffer too small");
        if (payload_size > 0U) {
            uint32_t payload_offset = s_meta.head + sizeof(memory_record_header_t);
            ESP_RETURN_ON_ERROR(read_at_locked(payload_offset, registers_out, payload_size), TAG, "Failed reading payload");

            uint16_t crc = crc16_modbus((const uint8_t *)registers_out, payload_size);
            ESP_RETURN_ON_FALSE(crc == hdr.payload_crc16, ESP_ERR_INVALID_CRC, TAG, "CRC mismatch");
        }

        if (meta_out != NULL) {
            meta_out->timestamp_s = hdr.timestamp_s;
            meta_out->start_reg = hdr.start_reg;
            meta_out->reg_count = hdr.reg_count;
            meta_out->slave_addr = hdr.slave_addr;
        }

        if (consume) {
            s_meta.head = (s_meta.head + rec_size) % s_meta.capacity;
            s_meta.used -= rec_size;
            s_meta.count--;
            ESP_RETURN_ON_ERROR(write_meta_locked(), TAG, "Failed to persist queue pop");
        }

        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t memory_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }

    if (s_lock == NULL) {
        s_lock = xSemaphoreCreateMutex();
        ESP_RETURN_ON_FALSE(s_lock != NULL, ESP_ERR_NO_MEM, TAG, "Failed creating mutex");
    }

    const char *partition_label = strlen(CONFIG_MEMORY_LITTLEFS_PARTITION_LABEL) > 0 ? CONFIG_MEMORY_LITTLEFS_PARTITION_LABEL : NULL;

    esp_vfs_littlefs_conf_t conf = {
        .base_path = CONFIG_MEMORY_LITTLEFS_MOUNT_POINT,
        .partition_label = partition_label,
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err == ESP_ERR_NOT_FOUND && partition_label != NULL) {
        LOG_WARNING(TAG, "LittleFS partition label '%s' not found, retrying with auto-detect", partition_label);
        conf.partition_label = NULL;
        err = esp_vfs_littlefs_register(&conf);
    }
    ESP_RETURN_ON_ERROR(err, TAG, "LittleFS mount failed");
    s_mounted = true;

    snprintf(s_base_dir, sizeof(s_base_dir), "%s/modbus", CONFIG_MEMORY_LITTLEFS_MOUNT_POINT);
    snprintf(s_data_path, sizeof(s_data_path), "%s/data.bin", s_base_dir);
    snprintf(s_meta_path, sizeof(s_meta_path), "%s/meta.bin", s_base_dir);

    err = ensure_directory(s_base_dir);
    if (err != ESP_OK) {
        goto fail;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    err = ensure_data_file_locked();
    if (err == ESP_OK) {
        err = load_or_create_meta_locked();
    }
    xSemaphoreGive(s_lock);
    if (err != ESP_OK) {
        goto fail;
    }

    s_ready = true;
    LOG_OK(TAG, "Memory queue ready at %s (capacity=%" PRIu32 " bytes)", s_base_dir, s_meta.capacity);
    return ESP_OK;

fail:
    if (s_mounted) {
        esp_vfs_littlefs_unregister(strlen(CONFIG_MEMORY_LITTLEFS_PARTITION_LABEL) > 0 ? CONFIG_MEMORY_LITTLEFS_PARTITION_LABEL : NULL);
        s_mounted = false;
    }
    return err;
}

esp_err_t memory_deinit(void)
{
    if (!s_ready) {
        return ESP_OK;
    }

    s_ready = false;
    if (s_mounted) {
        esp_vfs_littlefs_unregister(strlen(CONFIG_MEMORY_LITTLEFS_PARTITION_LABEL) > 0 ? CONFIG_MEMORY_LITTLEFS_PARTITION_LABEL : NULL);
        s_mounted = false;
    }

    return ESP_OK;
}

bool memory_is_ready(void)
{
    return s_ready;
}

esp_err_t memory_enqueue_modbus_sample(
    uint8_t slave_addr,
    uint16_t start_reg,
    const uint16_t *registers,
    uint16_t reg_count,
    uint32_t timestamp_s
)
{
    ESP_RETURN_ON_FALSE(s_ready, ESP_ERR_INVALID_STATE, TAG, "Memory not initialized");
    ESP_RETURN_ON_FALSE(registers != NULL, ESP_ERR_INVALID_ARG, TAG, "registers is NULL");
    ESP_RETURN_ON_FALSE(reg_count > 0U && reg_count <= CONFIG_MEMORY_MAX_REGISTERS, ESP_ERR_INVALID_ARG, TAG, "Invalid reg_count");

    memory_record_header_t hdr = {
        .magic = RECORD_MAGIC_DATA,
        .timestamp_s = timestamp_s,
        .start_reg = start_reg,
        .reg_count = reg_count,
        .slave_addr = slave_addr,
        .flags = 0U,
        .payload_crc16 = crc16_modbus((const uint8_t *)registers, (size_t)reg_count * sizeof(uint16_t)),
    };

    uint32_t rec_size = record_size_bytes(reg_count);
    xSemaphoreTake(s_lock, portMAX_DELAY);

    esp_err_t err = ensure_free_space_locked(rec_size);
    if (err == ESP_OK) {
        err = place_wrap_marker_if_needed_locked(rec_size);
    }
    if (err == ESP_OK) {
        err = ensure_free_space_locked(rec_size);
    }
    if (err == ESP_OK) {
        err = write_at_locked(s_meta.tail, &hdr, sizeof(hdr));
    }
    if (err == ESP_OK) {
        err = write_at_locked(s_meta.tail + sizeof(hdr), registers, (size_t)reg_count * sizeof(uint16_t));
    }
    if (err == ESP_OK) {
        s_meta.tail = (s_meta.tail + rec_size) % s_meta.capacity;
        s_meta.used += rec_size;
        s_meta.count++;
        err = write_meta_locked();
    }

    xSemaphoreGive(s_lock);
    return err;
}

esp_err_t memory_peek_modbus_sample(
    memory_sample_meta_t *meta_out,
    uint16_t *registers_out,
    uint16_t max_registers
)
{
    ESP_RETURN_ON_FALSE(s_ready, ESP_ERR_INVALID_STATE, TAG, "Memory not initialized");
    ESP_RETURN_ON_FALSE(registers_out != NULL, ESP_ERR_INVALID_ARG, TAG, "registers_out is NULL");

    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = read_sample_locked(false, meta_out, registers_out, max_registers);
    xSemaphoreGive(s_lock);
    return err;
}

esp_err_t memory_pop_modbus_sample(
    memory_sample_meta_t *meta_out,
    uint16_t *registers_out,
    uint16_t max_registers
)
{
    ESP_RETURN_ON_FALSE(s_ready, ESP_ERR_INVALID_STATE, TAG, "Memory not initialized");
    ESP_RETURN_ON_FALSE(registers_out != NULL, ESP_ERR_INVALID_ARG, TAG, "registers_out is NULL");

    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = read_sample_locked(true, meta_out, registers_out, max_registers);
    xSemaphoreGive(s_lock);
    return err;
}

esp_err_t memory_clear(void)
{
    ESP_RETURN_ON_FALSE(s_ready, ESP_ERR_INVALID_STATE, TAG, "Memory not initialized");

    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = reset_meta_locked();
    xSemaphoreGive(s_lock);
    return err;
}

uint32_t memory_pending_samples(void)
{
    if (!s_ready) {
        return 0U;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint32_t count = s_meta.count;
    xSemaphoreGive(s_lock);
    return count;
}

uint32_t memory_used_bytes(void)
{
    if (!s_ready) {
        return 0U;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint32_t used = s_meta.used;
    xSemaphoreGive(s_lock);
    return used;
}
