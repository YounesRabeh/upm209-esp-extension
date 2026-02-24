#pragma once

#include <stdio.h>
#include <stdarg.h>

/**
 * @file logging.h
 * @brief Central logging macros and functions for formatted console output.
 */

#define COLOR_RESET    "\033[0m"
#define COLOR_WHITE    "\033[0;37m"
#define COLOR_GREEN    "\033[0;32m"
#define COLOR_YELLOW   "\033[0;33m"
#define COLOR_RED      "\033[0;31m"
#define COLOR_PURPLE   "\033[0;35m"

/**
 * @brief Print a formatted log message with color, tag and level.
 *
 * @param color ANSI color code.
 * @param tag Component/module name.
 * @param level_str Log level string (e.g. "[INFO]: ").
 * @param fmt printf-style format string.
 * @param args va_list arguments.
 */
void log_vprintf(
    const char *color,
    const char *tag,
    const char *level_str,
    const char *fmt,
    va_list args
);

static inline void log_printf_helper(
    const char *color,
    const char *tag,
    const char *level_str,
    const char *fmt, ...
) {
    va_list args;
    va_start(args, fmt);
    log_vprintf(color, tag, level_str, fmt, args);
    va_end(args);
}

/* Logging macros */
#define LOG_INFO(tag, format, ...)    \
    do { log_printf_helper(COLOR_WHITE,  tag, " [INFO]: ",  format, ##__VA_ARGS__); } while(0)

#define LOG_DEBUG(tag, format, ...)   \
    do { log_printf_helper(COLOR_PURPLE, tag, " [DEBUG]: ", format, ##__VA_ARGS__); } while(0)

#define LOG_SUCCESS(tag, format, ...) \
    do { log_printf_helper(COLOR_GREEN,  tag, " [OK]: ",    format, ##__VA_ARGS__); } while(0)

#define LOG_WARNING(tag, format, ...) \
    do { log_printf_helper(COLOR_YELLOW, tag, " [WARN]: ",  format, ##__VA_ARGS__); } while(0)

#define LOG_ERROR(tag, format, ...)   \
    do { log_printf_helper(COLOR_RED,    tag, " [ERROR]: ", format, ##__VA_ARGS__); } while(0)