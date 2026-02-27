#include "logging.h"

/**
 * @file logging.c
 * @brief Implementation of formatted logging functions.
 */

/**
 * @brief Print a padded tag to the console.
 * Converts the tag into a fixed-width format and prints in white color.
 *
 * @param tag Module or component name.
 */
void log_print_tag(const char *tag) {
    printf(COLOR_WHITE "<%s>", tag);
}

void log_vprintf(
    const char *color,
    const char *tag,
    const char *level_str,
    const char *fmt,
    va_list args
) {
    printf("%s[%s] <%s> : ", color, level_str, tag);
    vprintf(fmt, args);
    printf(COLOR_RESET "\n");
}
