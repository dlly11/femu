/**
 * @file emu_plugin.h
 * @brief Plugin interface for dynamically loadable peripherals
 *
 * This header defines the C ABI for peripheral plugins. Plugins are shared
 * libraries (.so/.dll/.dylib) that export peripheral factories.
 *
 * To create a plugin:
 * 1. Include this header
 * 2. Implement peripheral read/write/reset/tick functions
 * 3. Create factory function(s) that return EmuPeripheral*
 * 4. Export emu_plugin_init() that returns array of EmuPeripheralType
 *
 * Example:
 *   // my_plugin.c
 *   #include "emu/emu_plugin.h"
 *
 *   static EmuPeripheral* my_periph_create(const char *name, const char *config) {
 *       // Create and return peripheral
 *   }
 *
 *   static EmuPeripheralType types[] = {
 *       { "my_periph", "My custom peripheral", my_periph_create, NULL },
 *       { NULL }  // Terminator
 *   };
 *
 *   static EmuPluginInfo info = {
 *       .api_version = EMU_PLUGIN_API_VERSION,
 *       .name = "My Plugin",
 *       .version = "1.0.0",
 *       .author = "Author Name",
 *       .description = "Plugin description"
 *   };
 *
 *   EMU_PLUGIN_EXPORT
 *   EmuPeripheralType* emu_plugin_init(const EmuPluginInfo **info_out) {
 *       if (info_out) *info_out = &info;
 *       return types;
 *   }
 */

#ifndef EMU_PLUGIN_H
#define EMU_PLUGIN_H

#include "emu_peripheral.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Plugin API Version
 *============================================================================*/

/**
 * Current plugin API version.
 * Plugins must set this in their EmuPluginInfo.
 */
#define EMU_PLUGIN_API_VERSION 1

/*============================================================================
 * Platform-specific Export Macro
 *============================================================================*/

#if defined(_WIN32) || defined(__CYGWIN__)
    #define EMU_PLUGIN_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define EMU_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
    #define EMU_PLUGIN_EXPORT
#endif

/*============================================================================
 * Plugin Metadata
 *============================================================================*/

/**
 * Plugin information structure.
 *
 * Returned by emu_plugin_init() to identify the plugin.
 */
typedef struct {
    int api_version;            /**< Must be EMU_PLUGIN_API_VERSION */
    const char *name;           /**< Plugin name (e.g., "STM32 Peripherals") */
    const char *version;        /**< Version string (e.g., "1.0.0") */
    const char *author;         /**< Author or vendor name */
    const char *description;    /**< Brief description of plugin contents */
} EmuPluginInfo;

/*============================================================================
 * Peripheral Type Descriptor
 *============================================================================*/

/**
 * Peripheral type descriptor.
 *
 * Each plugin exports an array of these, terminated by a NULL type_name.
 */
typedef struct {
    /**
     * Type name used for registration.
     * This is how the peripheral is referenced in YAML configs.
     * Must be unique across all loaded plugins.
     */
    const char *type_name;

    /**
     * Human-readable description.
     * Shown in peripheral listings and help text.
     */
    const char *description;

    /**
     * Factory function to create peripheral instances.
     *
     * @param name        Instance name (e.g., "USART1")
     * @param config_json Configuration as JSON string (may be NULL or "{}")
     * @return            New peripheral instance, or NULL on error
     *
     * The returned EmuPeripheral should have:
     * - name and type fields set
     * - context pointing to peripheral state
     * - vtable with at least read/write functions
     */
    EmuPeripheral* (*create)(const char *name, const char *config_json);

    /**
     * Destroy function for peripherals created by this factory.
     * Called when the emulator is destroyed or peripheral is removed.
     * May be NULL if create() returns static/pooled peripherals.
     *
     * @param periph      Peripheral to destroy
     */
    void (*destroy)(EmuPeripheral *periph);

} EmuPeripheralType;

/*============================================================================
 * Plugin Entry Point
 *============================================================================*/

/**
 * Plugin initialization function type.
 *
 * Every plugin must export a function with this signature named "emu_plugin_init".
 *
 * @param info_out  If non-NULL, set to point to plugin's EmuPluginInfo
 * @return          NULL-terminated array of EmuPeripheralType descriptors
 */
typedef EmuPeripheralType* (*EmuPluginInitFn)(const EmuPluginInfo **info_out);

/**
 * Expected symbol name for the plugin entry point.
 */
#define EMU_PLUGIN_INIT_SYMBOL "emu_plugin_init"

/*============================================================================
 * Helper Macros for Plugin Authors
 *============================================================================*/

/**
 * Declare the plugin entry point with proper export attributes.
 *
 * Usage:
 *   EMU_PLUGIN_DECLARE_INIT(my_init_function)
 */
#define EMU_PLUGIN_DECLARE_INIT(fn) \
    EMU_PLUGIN_EXPORT EmuPeripheralType* fn(const EmuPluginInfo **info_out)

/**
 * Define a simple peripheral with read/write callbacks.
 *
 * Usage:
 *   EMU_PLUGIN_SIMPLE_PERIPH(my_periph, "my_type", read_fn, write_fn)
 */
#define EMU_PLUGIN_SIMPLE_PERIPH(name, type_str, read_fn, write_fn) \
    static EmuPeripheral name = { \
        .name = #name, \
        .type = type_str, \
        .vtable = { .read = read_fn, .write = write_fn } \
    }

#ifdef __cplusplus
}
#endif

#endif /* EMU_PLUGIN_H */
