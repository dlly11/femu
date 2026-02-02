/**
 * @file emu_log.c
 * @brief Unified logging system implementation
 *
 * Implements the logging infrastructure that routes C logs through a callback
 * to Python's logging module.
 */

#include "emu/emu_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*============================================================================
 * Configuration State
 *============================================================================*/

/**
 * Global logging configuration.
 */
typedef struct {
    EmuLogCallback callback;           /**< User callback function */
    void *callback_ctx;                /**< User context for callback */
    int global_level;                  /**< Global minimum log level */
    int category_levels[EMU_LOG_CAT_COUNT]; /**< Per-category levels (-1 = use global) */
    bool enabled;                      /**< Master enable switch */
} EmuLogConfig;

/**
 * Default configuration.
 */
static EmuLogConfig g_log_config = {
    .callback = NULL,
    .callback_ctx = NULL,
    .global_level = EMU_LOG_INFO,
    .category_levels = {-1, -1, -1, -1, -1, -1, -1, -1},
    .enabled = true
};

/*============================================================================
 * Configuration API
 *============================================================================*/

void emu_log_set_callback(EmuLogCallback callback, void *ctx)
{
    g_log_config.callback = callback;
    g_log_config.callback_ctx = ctx;
}

void emu_log_set_level(int level)
{
    if (level >= 0 && level < EMU_LOG_LEVEL_COUNT) {
        g_log_config.global_level = level;
    }
}

void emu_log_set_category_level(int category, int level)
{
    if (category >= 0 && category < EMU_LOG_CAT_COUNT) {
        /* Allow -1 to reset to global, otherwise validate level */
        if (level == -1 || (level >= 0 && level < EMU_LOG_LEVEL_COUNT)) {
            g_log_config.category_levels[category] = level;
        }
    }
}

void emu_log_set_enabled(bool enabled)
{
    g_log_config.enabled = enabled;
}

bool emu_log_is_enabled(int level, int category)
{
    /* Fast path: check master enable */
    if (!g_log_config.enabled) {
        return false;
    }

    /* Fast path: no callback means no logging */
    if (!g_log_config.callback) {
        return false;
    }

    /* Determine effective level for this category */
    int effective_level = g_log_config.global_level;
    if (category >= 0 && category < EMU_LOG_CAT_COUNT) {
        int cat_level = g_log_config.category_levels[category];
        if (cat_level >= 0) {
            effective_level = cat_level;
        }
    }

    /* Check if the message level meets the threshold */
    return level >= effective_level;
}

/*============================================================================
 * Logging Implementation
 *============================================================================*/

/** Maximum log message length */
#define EMU_LOG_MAX_MSG_LEN 1024

void emu_log_impl(int level, int category, const char *file, int line,
                  const char *func, const char *fmt, ...)
{
    /* Double-check that logging is enabled (should have been checked by macro) */
    if (!g_log_config.callback) {
        return;
    }

    /* Format the message */
    char msg[EMU_LOG_MAX_MSG_LEN];
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    /* Handle truncation */
    if (written >= (int)sizeof(msg)) {
        /* Message was truncated, add ellipsis */
        msg[sizeof(msg) - 4] = '.';
        msg[sizeof(msg) - 3] = '.';
        msg[sizeof(msg) - 2] = '.';
        msg[sizeof(msg) - 1] = '\0';
    }

    /* Extract just the filename from the path */
    const char *filename = file;
    const char *p = file;
    while (*p) {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
        p++;
    }

    /* Call the user callback */
    g_log_config.callback(g_log_config.callback_ctx, level, category,
                           filename, line, func, msg);
}
