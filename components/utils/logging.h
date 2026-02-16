#pragma once

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/**
 * @file logging.h
 * @brief Central logging macros and functions for formatted console output.
 *
 * Provides color-coded, padded logging for INFO, DEBUG, SUCCESS, WARNING, and ERROR levels.
 */

/// @name Color codes for console output
///@{
#define COLOR_RESET    "\033[0m"
#define COLOR_WHITE    "\033[0;37m"
#define COLOR_GREEN    "\033[0;32m"
#define COLOR_YELLOW   "\033[0;33m"
#define COLOR_RED      "\033[0;31m"
#define COLOR_PURPLE   "\033[0;35m"
///@}

#define TAG_SIZE "%-12s"

/**
 * @brief Print a padded tag for log messages.
 * @param tag The component or module name to display.
 */
void log_print_tag(const char *tag);

/**
 * @brief Print a formatted log message with a specific color and level.
 * @param color ANSI color code.
 * @param level_str String indicating the log level (e.g., "[INFO]: ").
 * @param fmt printf-style format string.
 * @param args va_list of arguments for the format string.
 */
void log_vprintf(const char *color, const char *level_str, const char *fmt, va_list args);

/**
 * @brief Helper function for LOG macros.
 * @param color ANSI color code.
 * @param level_str Log level string.
 * @param fmt printf-style format string.
 * @param ... variadic arguments matching fmt
 */
static inline void log_printf_helper(const char *color, const char *level_str, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_vprintf(color, level_str, fmt, args);
    va_end(args);
}

/**
 * @name Logging macros
 * These macros provide standardized logging with color, level, and padded tags.
 */
///@{
#define PRINT_TAG(tag) do { log_print_tag(tag); } while(0)
#define LOG_INFO(tag, format, ...)    do { PRINT_TAG(tag); log_printf_helper(COLOR_WHITE, " [INFO]: ", format, ##__VA_ARGS__); } while(0)
#define LOG_DEBUG(tag, format, ...)   do { PRINT_TAG(tag); log_printf_helper(COLOR_PURPLE, " [DEBUG]: ", format, ##__VA_ARGS__); } while(0)
#define LOG_SUCCESS(tag, format, ...) do { PRINT_TAG(tag); log_printf_helper(COLOR_GREEN, " [OK]: ", format, ##__VA_ARGS__); } while(0)
#define LOG_WARNING(tag, format, ...) do { PRINT_TAG(tag); log_printf_helper(COLOR_YELLOW, " [WARN]: ", format, ##__VA_ARGS__); } while(0)
#define LOG_ERROR(tag, format, ...)   do { PRINT_TAG(tag); log_printf_helper(COLOR_RED, " [ERROR]: ", format, ##__VA_ARGS__); } while(0)
///@}
