#include "logging.h"

/**
 * @file logging.c
 * @brief Implementation of formatted logging functions.
 */

/**
 * @brief Print a padded tag to the console.
 *
 * Converts the tag into a fixed-width format and prints in white color.
 *
 * @param tag Module or component name.
 */
void log_print_tag(const char *tag) {
    char tag_buf[32];
    snprintf(tag_buf, sizeof(tag_buf), "<%s>", tag);
    printf(COLOR_WHITE TAG_SIZE, tag_buf);
}

/**
 * @brief Print a formatted log message with color and log level.
 *
 * @param color ANSI color code string.
 * @param level_str Log level string (e.g., "[INFO]: ").
 * @param fmt printf-style format string.
 * @param args va_list of arguments.
 */
void log_vprintf(const char *color, const char *level_str, const char *fmt, va_list args) {
    printf("%s%s", color, level_str);
    vprintf(fmt, args);
    printf(COLOR_RESET "\n");
}
