#include "logging.h"

#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t s_log_mutex = NULL;

static inline void log_lock(void)
{
    if (s_log_mutex == NULL) {
        s_log_mutex = xSemaphoreCreateMutex();
    }
    if (s_log_mutex != NULL) {
        xSemaphoreTake(s_log_mutex, portMAX_DELAY);
    }
}

static inline void log_unlock(void)
{
    if (s_log_mutex != NULL) {
        xSemaphoreGive(s_log_mutex);
    }
}

static bool log_get_timestamp(char *buf, size_t buf_len)
{
    if (!buf || buf_len == 0) {
        return false;
    }

    time_t now = time(NULL);
    // If SNTP has not synchronized yet, epoch is usually near 1970.
    if (now < 1700000000) {
        return false;
    }

    struct tm tm_now = {0};
    if (!localtime_r(&now, &tm_now)) {
        return false;
    }

    if (strftime(buf, buf_len, "%Y-%m-%d %H:%M:%S", &tm_now) == 0) {
        return false;
    }

    return true;
}


/**
 * @brief Print a padded tag to the console.
 * Converts the tag into a fixed-width format and prints in white color.
 *
 * @param tag Module or component name.
 */
void log_print_tag(const char *tag) {
    printf(COLOR_WHITE "<%s>", tag);
}

/**
 * @brief Print a formatted log message with color coding.
 * The message is prefixed with the log level and tag, and colored according to the log
 */
void log_vprintf(
    const char *color,
    const char *tag,
    const char *level_str,
    const char *fmt,
    va_list args
) {
    log_lock();

    const int level_width = 5;
    int level_len = (int)strlen(level_str);
    int level_pad = level_width - level_len;
    if (level_pad < 0) {
        level_pad = 0;
    }

    char ts[32] = {0};
    if (log_get_timestamp(ts, sizeof(ts))) {
        printf("%s[%s] [%s]%*s <%s> : ", color, ts, level_str, level_pad, "", tag);
    } else {
        printf("%s[%s]%*s <%s> : ", color, level_str, level_pad, "", tag);
    }
    vprintf(fmt, args);
    printf(COLOR_RESET "\n");

    log_unlock();
}
