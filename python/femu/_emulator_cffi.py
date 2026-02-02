"""
CFFI bindings for the emulator shared library.

This module provides low-level CFFI access to libarmv8m_emulator.so.
For high-level usage, see emulator.py.
"""

from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

from cffi import FFI

if TYPE_CHECKING:
    from cffi import FFI as FFI_Type

# Create FFI instance
_ffi = FFI()

# C definitions - using opaque struct style for CFFI dlopen mode
_ffi.cdef(
    """
    /* Emulator state enum */
    typedef enum {
        EMU_STATE_STOPPED,
        EMU_STATE_RUNNING,
        EMU_STATE_HALTED,
        EMU_STATE_BREAKPOINT,
        EMU_STATE_WATCHPOINT,
        EMU_STATE_FAULT,
    } EmulatorState;

    /* EmulatorConfig - we need the full definition to create configs */
    typedef struct {
        bool has_fpu;
        bool has_dsp;
        bool has_trustzone;
        int num_mpu_regions;
        int num_irqs;
        uint32_t default_flash_base;
        uint32_t default_flash_size;
        uint32_t default_ram_base;
        uint32_t default_ram_size;
    } EmulatorConfig;

    /* Emulator - opaque, we only use pointers */
    typedef struct Emulator Emulator;

    /* Lifecycle API */
    void armv8m_emu_default_config(EmulatorConfig *config);
    int armv8m_emu_init(Emulator *emu, const EmulatorConfig *config);
    void armv8m_emu_destroy(Emulator *emu);
    void armv8m_emu_reset(Emulator *emu);

    /* Memory Setup API */
    int armv8m_emu_add_flash(Emulator *emu, uint32_t base, uint32_t size);
    int armv8m_emu_add_ram(Emulator *emu, uint32_t base, uint32_t size);
    int armv8m_emu_load(Emulator *emu, uint32_t addr, const uint8_t *data, uint32_t size);

    /* Execution API */
    int armv8m_emu_step(Emulator *emu);
    int64_t armv8m_emu_run(Emulator *emu, uint64_t max_cycles);
    void armv8m_emu_stop(Emulator *emu);

    /* State Access API */
    uint32_t armv8m_emu_get_reg(const Emulator *emu, int reg);
    void armv8m_emu_set_reg(Emulator *emu, int reg, uint32_t value);
    uint32_t armv8m_emu_get_pc(const Emulator *emu);
    void armv8m_emu_set_pc(Emulator *emu, uint32_t value);
    uint32_t armv8m_emu_get_xpsr(const Emulator *emu);
    void armv8m_emu_set_xpsr(Emulator *emu, uint32_t value);
    uint64_t armv8m_emu_get_cycles(const Emulator *emu);
    EmulatorState armv8m_emu_get_state(const Emulator *emu);
    int armv8m_emu_get_last_error(const Emulator *emu);

    /* Memory Access API */
    uint32_t armv8m_emu_read_mem(const Emulator *emu, uint32_t addr,
                                 uint8_t size, bool *fault);
    void armv8m_emu_write_mem(Emulator *emu, uint32_t addr, uint32_t value,
                              uint8_t size, bool *fault);
    uint32_t armv8m_emu_read_block(const Emulator *emu, uint32_t addr,
                                   uint8_t *data, uint32_t size);
    uint32_t armv8m_emu_write_block(Emulator *emu, uint32_t addr,
                                    const uint8_t *data, uint32_t size);

    /* Breakpoint API */
    int armv8m_emu_add_breakpoint(Emulator *emu, uint32_t addr);
    int armv8m_emu_remove_breakpoint(Emulator *emu, uint32_t addr);
    bool armv8m_emu_has_breakpoint(const Emulator *emu, uint32_t addr);
    void armv8m_emu_clear_breakpoints(Emulator *emu);

    /* Watchpoint types (matches GDB Z packet types) */
    typedef enum {
        WATCHPOINT_WRITE = 2,
        WATCHPOINT_READ = 3,
        WATCHPOINT_ACCESS = 4,
    } WatchpointType;

    /* Watchpoint API */
    int armv8m_emu_add_watchpoint(Emulator *emu, uint32_t addr,
                                  uint32_t size, WatchpointType type);
    int armv8m_emu_remove_watchpoint(Emulator *emu, uint32_t addr,
                                     uint32_t size, WatchpointType type);
    void armv8m_emu_clear_watchpoints(Emulator *emu);
    uint32_t armv8m_emu_get_watchpoint_hit_addr(const Emulator *emu);
    WatchpointType armv8m_emu_get_watchpoint_hit_type(const Emulator *emu);

    /* Special Register Access */
    uint32_t armv8m_emu_get_special_reg(const Emulator *emu, int reg);
    void armv8m_emu_set_special_reg(Emulator *emu, int reg, uint32_t value);
    uint32_t armv8m_emu_get_fpu_reg(const Emulator *emu, int reg);
    void armv8m_emu_set_fpu_reg(Emulator *emu, int reg, uint32_t value);
    uint32_t armv8m_emu_get_fpscr(const Emulator *emu);
    void armv8m_emu_set_fpscr(Emulator *emu, uint32_t value);

    /* ========================================================================
     * Peripheral Support
     * ======================================================================== */

    /* Peripheral callback types */
    typedef uint32_t (*EmuPeriphReadFn)(void *ctx, uint32_t offset, uint8_t size);
    typedef void (*EmuPeriphWriteFn)(void *ctx, uint32_t offset, uint32_t value, uint8_t size);
    typedef void (*EmuPeriphResetFn)(void *ctx);
    typedef void (*EmuPeriphTickFn)(void *ctx, uint64_t cycles);
    typedef void (*EmuPeriphDestroyFn)(void *ctx);

    /* IRQ/DMA callback types */
    typedef void (*EmuPeriphIRQCallback)(void *emu_ctx, int irq, int level);
    typedef void (*EmuPeriphSetIRQFn)(void *ctx, EmuPeriphIRQCallback cb, void *emu_ctx);
    typedef void (*EmuPeriphDMACallback)(void *emu_ctx, int channel, int request);
    typedef void (*EmuPeriphSetDMAFn)(void *ctx, EmuPeriphDMACallback cb, void *emu_ctx);

    /* Debug callback */
    typedef int (*EmuPeriphDebugStateFn)(void *ctx, char *buf, size_t buf_size);

    /* Peripheral virtual function table */
    typedef struct {
        EmuPeriphReadFn read;
        EmuPeriphWriteFn write;
        EmuPeriphResetFn reset;
        EmuPeriphTickFn tick;
        EmuPeriphDestroyFn destroy;
        EmuPeriphSetIRQFn set_irq_callback;
        EmuPeriphSetDMAFn set_dma_callback;
        EmuPeriphDebugStateFn debug_state;
    } EmuPeripheralVTable;

    /* Peripheral instance structure */
    typedef struct {
        const char *name;
        const char *type;
        void *context;
        EmuPeripheralVTable vtable;
        uint64_t base_addr;
        uint64_t size;
        void *emu_ctx;
    } EmuPeripheral;

    /* Peripheral API */
    int armv8m_emu_add_peripheral(Emulator *emu, EmuPeripheral *periph,
                                   uint32_t base, uint32_t size);

    /* ========================================================================
     * Plugin Support
     * ======================================================================== */

    /* Plugin metadata */
    typedef struct {
        int api_version;
        const char *name;
        const char *version;
        const char *author;
        const char *description;
    } EmuPluginInfo;

    /* Peripheral type descriptor (from plugins) */
    typedef struct {
        const char *type_name;
        const char *description;
        EmuPeripheral* (*create)(const char *name, const char *config_json);
        void (*destroy)(EmuPeripheral *periph);
    } EmuPeripheralType;

    /* Plugin init function type */
    typedef EmuPeripheralType* (*EmuPluginInitFn)(const EmuPluginInfo **info_out);

    /* ========================================================================
     * Logging Support
     * ======================================================================== */

    /* Log callback function type */
    typedef void (*EmuLogCallback)(void *ctx, int level, int category,
                                   const char *file, int line, const char *func,
                                   const char *msg);

    /* Logging configuration API */
    void emu_log_set_callback(EmuLogCallback callback, void *ctx);
    void emu_log_set_level(int level);
    void emu_log_set_category_level(int category, int level);
    void emu_log_set_enabled(bool enabled);
    bool emu_log_is_enabled(int level, int category);
"""
)

# Error codes (must match armv8m_types.h)
ARMV8M_OK = 0
ARMV8M_ERR_UNDEFINED_INSN = -1
ARMV8M_ERR_UNPREDICTABLE = -2
ARMV8M_ERR_BUS_FAULT = -3
ARMV8M_ERR_MEM_FAULT = -4
ARMV8M_ERR_USAGE_FAULT = -5
ARMV8M_ERR_HARD_FAULT = -6
ARMV8M_ERR_BREAKPOINT = -7
ARMV8M_ERR_HALTED = -8
ARMV8M_ERR_SECURE_FAULT = -9
ARMV8M_ERR_INVALID_PARAM = -10
ARMV8M_ERR_WATCHPOINT = -11

# Register indices
ARMV8M_REG_SP = 13
ARMV8M_REG_LR = 14
ARMV8M_REG_PC = 15

# Special register IDs (must match armv8m_types.h)
ARMV8M_SYSREG_PRIMASK = 0
ARMV8M_SYSREG_BASEPRI = 1
ARMV8M_SYSREG_FAULTMASK = 2
ARMV8M_SYSREG_CONTROL = 3
ARMV8M_SYSREG_MSP = 4
ARMV8M_SYSREG_PSP = 5
ARMV8M_SYSREG_MSPLIM = 6
ARMV8M_SYSREG_PSPLIM = 7

# Emulator state values
EMU_STATE_STOPPED = 0
EMU_STATE_RUNNING = 1
EMU_STATE_HALTED = 2
EMU_STATE_BREAKPOINT = 3
EMU_STATE_WATCHPOINT = 4
EMU_STATE_FAULT = 5

# Watchpoint types (matches GDB Z packet types)
WATCHPOINT_WRITE = 2
WATCHPOINT_READ = 3
WATCHPOINT_ACCESS = 4

# Log levels (must match EmuLogLevel in emu_log.h)
EMU_LOG_TRACE = 0
EMU_LOG_DEBUG = 1
EMU_LOG_INFO = 2
EMU_LOG_WARNING = 3
EMU_LOG_ERROR = 4

# Log categories (must match EmuLogCategory in emu_log.h)
EMU_LOG_CAT_DECODER = 0
EMU_LOG_CAT_EXECUTOR = 1
EMU_LOG_CAT_MEMORY = 2
EMU_LOG_CAT_NVIC = 3
EMU_LOG_CAT_MPU = 4
EMU_LOG_CAT_PERIPHERAL = 5
EMU_LOG_CAT_GDB = 6
EMU_LOG_CAT_EMULATOR = 7

# Cache for library instance
_lib = None


def _find_library() -> Path:
    """Find the shared library in standard locations."""
    # Get project root (femu/python/femu -> femu)
    project_root = Path(__file__).parent.parent.parent

    # Platform-specific library name
    import sys

    if sys.platform == "darwin":
        lib_name = "libarmv8m_emulator.dylib"
    elif sys.platform == "win32":
        lib_name = "armv8m_emulator.dll"
    else:
        lib_name = "libarmv8m_emulator.so"

    # Possible library locations
    search_paths = [
        # Package-bundled library (for pip-installed package)
        Path(__file__).parent / "_lib" / lib_name,
        # New arch-specific location (after restructuring)
        project_root / "build" / "src" / "arch" / "armv8m" / lib_name,
        # Debug build
        project_root / "build" / "Debug" / "src" / "arch" / "armv8m" / lib_name,
        # Release build
        project_root / "build" / "Release" / "src" / "arch" / "armv8m" / lib_name,
        # Legacy locations (for backward compatibility)
        project_root / "build" / "src" / "core" / "emulator" / lib_name,
        # System path (if installed)
        Path("/usr/local/lib") / lib_name,
    ]

    for path in search_paths:
        if path.exists():
            return path

    # If not found, return the most likely path for error message
    return project_root / "build" / "src" / "arch" / "armv8m" / "libarmv8m_emulator.so"


def get_ffi() -> FFI_Type:
    """Get the FFI instance."""
    return _ffi


def get_lib():
    """
    Get the loaded library instance.

    Returns:
        The CFFI library object with all emulator functions.

    Raises:
        OSError: If the library cannot be found or loaded.
    """
    global _lib

    if _lib is None:
        lib_path = _find_library()
        if not lib_path.exists():
            raise OSError(
                f"Emulator library not found at {lib_path}. "
                "Please build the project first with 'femu build all'."
            )
        _lib = _ffi.dlopen(str(lib_path))

    return _lib


def get_emulator_size() -> int:
    """
    Get the size of the Emulator struct.

    This is needed to allocate memory for the emulator.
    We use a generous estimate since the actual struct is complex.
    """
    # Emulator struct contains:
    # - Executor (~700 bytes)
    # - MemorySystem (~2KB)
    # - NVIC (~1KB)
    # - MPU (~512 bytes)
    # - Plus peripherals, breakpoints, memory pointers
    # Total estimate: 8KB should be plenty
    return 8192


def create_emulator():
    """
    Create a new emulator instance.

    Returns:
        A tuple of (emulator pointer, cleanup function)
    """
    # Allocate memory for emulator struct
    emu = _ffi.new("char[]", get_emulator_size())
    emu_ptr = _ffi.cast("Emulator *", emu)

    def cleanup():
        lib = get_lib()
        lib.armv8m_emu_destroy(emu_ptr)

    return emu_ptr, cleanup, emu  # Return emu to prevent GC


def create_config() -> tuple:
    """
    Create a new EmulatorConfig.

    Returns:
        A tuple of (config pointer, config buffer to keep alive)
    """
    config = _ffi.new("EmulatorConfig *")
    get_lib().armv8m_emu_default_config(config)
    return config, config


def create_peripheral() -> tuple:
    """
    Create a new EmuPeripheral structure.

    Returns:
        A tuple of (peripheral pointer, buffer to keep alive)
    """
    periph = _ffi.new("EmuPeripheral *")
    return periph, periph


def load_plugin_library(path: str):
    """
    Load a plugin shared library.

    Args:
        path: Path to the .so/.dll/.dylib file

    Returns:
        The loaded CFFI library object

    Raises:
        OSError: If the library cannot be loaded
    """
    return _ffi.dlopen(path)


# Plugin API version constant
EMU_PLUGIN_API_VERSION = 1
