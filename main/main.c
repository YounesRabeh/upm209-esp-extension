#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "logging.h"
#include "services_manager.h"

#define TAG "MAIN"

static void log_section(const char *title)
{
    LOG_INFO(TAG, "==================== %s ====================", title);
}

void app_main(void)
{
    // Silence ESP-IDF logs globally; keep only project LOG_* output.
    esp_log_level_set("*", ESP_LOG_NONE);

    log_section("BOOT");
    LOG_OK(TAG, "Starting application");

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    log_section("DEVICE INFO");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    LOG_INFO(
        TAG,
        "Chip: target=%s cores=%d rev=v%u.%u",
        CONFIG_IDF_TARGET,
        chip_info.cores,
        major_rev,
        minor_rev
    );
    LOG_INFO(
        TAG,
        "Features: wifi=%s bt=%s ble=%s ieee802154=%s",
        (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "yes" : "no",
        (chip_info.features & CHIP_FEATURE_BT) ? "yes" : "no",
        (chip_info.features & CHIP_FEATURE_BLE) ? "yes" : "no",
        (chip_info.features & CHIP_FEATURE_IEEE802154) ? "yes" : "no"
    );

    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        LOG_ERROR(TAG, "Failed to read flash size");
        return;
    }

    LOG_INFO(
        TAG,
        "Flash: %" PRIu32 "MB (%s)",
        flash_size / (uint32_t)(1024 * 1024),
        (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external"
    );
    LOG_INFO(TAG, "Heap min free: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());

    log_section("SERVICES STARTUP");
    esp_err_t services_err = services_manager_start();
    if (services_err != ESP_OK) {
        LOG_WARNING(
            TAG,
            "Startup completed with warnings: %s (0x%x)",
            esp_err_to_name(services_err),
            services_err
        );
    } else {
        LOG_OK(TAG, "All startup services initialized");
    }

    log_section("READY");
    LOG_OK(TAG, "Boot sequence completed");
}
