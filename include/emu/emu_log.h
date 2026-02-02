/**
 * @file emu_log.h
 * @brief Unified logging system for FEMU
 *
 * Provides a logging interface that routes C logs through a callback to Python's
 * logging module, enabling unified output with configurable verbosity.
 *
 * Log Levels:
 *   - TRACE (0): Instruction-level detail with register changes
 *   - DEBUG (1): Internal state, function entry/exit
 *   - INFO (2): Normal operation milestones
 *   - WARNING (3): Recoverable issues
 *   - ERROR (4): Errors and failures
 *
 * Usage:
 *   EMU_LOG_INFO(EMU_LOG_CAT_EMULATOR, "Emulator initialized");
 *   EMU_LOG_TRACE(EMU_LOG_CAT_EXECUTOR, "0x%08X: ADD R%d, R%d, R%d ; R%d: 0x%X -> 0x%X",
 *                 pc, rd, rn, rm, rd, old_val, new_val);
 */

#ifndef EMU_LOG_H
#define EMU_LOG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Log Levels
 *============================================================================*/

/**
 * Log severity levels.
 */
typedef enum {
    EMU_LOG_TRACE = 0,    /**< Instruction-level detail with register changes */
    EMU_LOG_DEBUG = 1,    /**< Internal state, function entry/exit */
    EMU_LOG_INFO = 2,     /**< Normal operation milestones */
    EMU_LOG_WARNING = 3,  /**< Recoverable issues */
    EMU_LOG_ERROR = 4,    /**< Errors and failures */
    EMU_LOG_LEVEL_COUNT = 5
} EmuLogLevel;

/*============================================================================
 * Log Categories
 *============================================================================*/

/**
 * Log categories for different subsystems.
 */
typedef enum {
    EMU_LOG_CAT_DECODER = 0,    /**< Instruction decoding (femu.decoder) */
    EMU_LOG_CAT_EXECUTOR = 1,   /**< Instruction execution (femu.executor) */
    EMU_LOG_CAT_MEMORY = 2,     /**< Memory reads/writes (femu.memory) */
    EMU_LOG_CAT_NVIC = 3,       /**< Interrupt handling (femu.nvic) */
    EMU_LOG_CAT_MPU = 4,        /**< Memory protection (femu.mpu) */
    EMU_LOG_CAT_PERIPHERAL = 5, /**< Peripheral I/O (femu.peripheral) */
    EMU_LOG_CAT_GDB = 6,        /**< GDB server (femu.gdb) */
    EMU_LOG_CAT_EMULATOR = 7,   /**< Top-level operations (femu.emulator) */
    EMU_LOG_CAT_COUNT = 8
} EmuLogCategory;

/*============================================================================
 * Log Callback
 *============================================================================*/

/**
 * Log callback function type.
 *
 * @param ctx       User context pointer (passed to emu_log_set_callback)
 * @param level     Log level (EmuLogLevel value)
 * @param category  Log category (EmuLogCategory value)
 * @param file      Source file name (__FILE__)
 * @param line      Source line number (__LINE__)
 * @param func      Function name (__func__)
 * @param msg       Formatted log message
 */
typedef void (*EmuLogCallback)(void *ctx, int level, int category,
                               const char *file, int line, const char *func,
                               const char *msg);

/*============================================================================
 * Configuration API
 *============================================================================*/

/**
 * Set the log callback function.
 *
 * The callback receives all log messages that pass the level/category filters.
 * If no callback is set, logs are discarded.
 *
 * @param callback  Callback function (NULL to disable)
 * @param ctx       User context pointer passed to callback
 */
void emu_log_set_callback(EmuLogCallback callback, void *ctx);

/**
 * Set the global minimum log level.
 *
 * Messages below this level are filtered out. Default is EMU_LOG_INFO.
 *
 * @param level  Minimum log level (EmuLogLevel value)
 */
void emu_log_set_level(int level);

/**
 * Set the minimum log level for a specific category.
 *
 * This overrides the global level for the given category.
 * Set to -1 to use the global level.
 *
 * @param category  Log category (EmuLogCategory value)
 * @param level     Minimum log level, or -1 to use global
 */
void emu_log_set_category_level(int category, int level);

/**
 * Enable or disable all logging.
 *
 * When disabled, all log macros short-circuit immediately without
 * checking levels or formatting messages.
 *
 * @param enabled  true to enable, false to disable
 */
void emu_log_set_enabled(bool enabled);

/**
 * Check if logging is enabled for a given level and category.
 *
 * This is used by the log macros for fast short-circuit evaluation.
 *
 * @param level     Log level
 * @param category  Log category
 * @return          true if a message would be logged
 */
bool emu_log_is_enabled(int level, int category);

/*============================================================================
 * Logging Implementation
 *============================================================================*/

/**
 * Log a message (internal implementation).
 *
 * Do not call directly - use the EMU_LOG_* macros instead.
 *
 * @param level     Log level
 * @param category  Log category
 * @param file      Source file name
 * @param line      Source line number
 * @param func      Function name
 * @param fmt       printf-style format string
 * @param ...       Format arguments
 */
void emu_log_impl(int level, int category, const char *file, int line,
                  const char *func, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 6, 7)))
#endif
    ;

/*============================================================================
 * Logging Macros
 *============================================================================*/

#ifndef EMU_LOG_DISABLE_ALL

/**
 * Log at TRACE level.
 *
 * Use for instruction-level detail with register changes.
 * Example: EMU_LOG_TRACE(EMU_LOG_CAT_EXECUTOR, "ADD R0, R1, R2 ; R0: 0x5 -> 0xA");
 */
#define EMU_LOG_TRACE(cat, ...) \
    do { \
        if (emu_log_is_enabled(EMU_LOG_TRACE, (cat))) \
            emu_log_impl(EMU_LOG_TRACE, (cat), __FILE__, __LINE__, __func__, __VA_ARGS__); \
    } while (0)

/**
 * Log at DEBUG level.
 *
 * Use for internal state and function entry/exit.
 */
#define EMU_LOG_DEBUG(cat, ...) \
    do { \
        if (emu_log_is_enabled(EMU_LOG_DEBUG, (cat))) \
            emu_log_impl(EMU_LOG_DEBUG, (cat), __FILE__, __LINE__, __func__, __VA_ARGS__); \
    } while (0)

/**
 * Log at INFO level.
 *
 * Use for normal operation milestones.
 */
#define EMU_LOG_INFO(cat, ...) \
    do { \
        if (emu_log_is_enabled(EMU_LOG_INFO, (cat))) \
            emu_log_impl(EMU_LOG_INFO, (cat), __FILE__, __LINE__, __func__, __VA_ARGS__); \
    } while (0)

/**
 * Log at WARNING level.
 *
 * Use for recoverable issues.
 */
#define EMU_LOG_WARNING(cat, ...) \
    do { \
        if (emu_log_is_enabled(EMU_LOG_WARNING, (cat))) \
            emu_log_impl(EMU_LOG_WARNING, (cat), __FILE__, __LINE__, __func__, __VA_ARGS__); \
    } while (0)

/**
 * Log at ERROR level.
 *
 * Use for errors and failures.
 */
#define EMU_LOG_ERROR(cat, ...) \
    do { \
        if (emu_log_is_enabled(EMU_LOG_ERROR, (cat))) \
            emu_log_impl(EMU_LOG_ERROR, (cat), __FILE__, __LINE__, __func__, __VA_ARGS__); \
    } while (0)

#else /* EMU_LOG_DISABLE_ALL */

/* Compile out all logging for release builds */
#define EMU_LOG_TRACE(cat, ...) ((void)0)
#define EMU_LOG_DEBUG(cat, ...) ((void)0)
#define EMU_LOG_INFO(cat, ...) ((void)0)
#define EMU_LOG_WARNING(cat, ...) ((void)0)
#define EMU_LOG_ERROR(cat, ...) ((void)0)

#endif /* EMU_LOG_DISABLE_ALL */

#ifdef __cplusplus
}
#endif

#endif /* EMU_LOG_H */
