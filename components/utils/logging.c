#include "logging.h"


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
    printf("%s[%s] <%s> : ", color, level_str, tag);
    vprintf(fmt, args);
    printf(COLOR_RESET "\n");
}
