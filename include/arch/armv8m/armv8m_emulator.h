/**
 * @file armv8m_emulator.h
 * @brief Emulator glue layer - unified API for ARMv8-M emulation
 *
 * AI INSTRUCTIONS:
 * - This header provides a high-level emulator API that integrates all modules
 * - Implementation goes in src/core/emulator/
 * - Depends on: armv8m_executor.h, armv8m_memory.h, armv8m_nvic.h, armv8m_mpu.h
 */

#ifndef ARMV8M_EMULATOR_H
#define ARMV8M_EMULATOR_H

#include "arch/armv8m/armv8m_types.h"
#include "arch/armv8m/armv8m_executor.h"
#include "emu/emu_memory.h"
#include "arch/armv8m/armv8m_nvic.h"
#include "arch/armv8m/armv8m_mpu.h"
#include "emu/emu_peripheral.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Emulator State
 *============================================================================*/

#include "emu/emu_types.h"

/*============================================================================
 * Emulator Configuration
 *============================================================================*/

/**
 * Emulator configuration.
 */
typedef struct {
    /* Feature flags */
    bool has_fpu;               /**< Enable FPU */
    bool has_dsp;               /**< Enable DSP extension */
    bool has_trustzone;         /**< Enable TrustZone */

    /* MPU configuration */
    int num_mpu_regions;        /**< Number of MPU regions (0 = no MPU) */

    /* NVIC configuration */
    int num_irqs;               /**< Number of external IRQs (max 240) */

    /* Default memory configuration (can be overridden) */
    uint32_t default_flash_base; /**< Default flash base address */
    uint32_t default_flash_size; /**< Default flash size */
    uint32_t default_ram_base;   /**< Default RAM base address */
    uint32_t default_ram_size;   /**< Default RAM size */
} EmulatorConfig;

/*============================================================================
 * Emulator Context
 *============================================================================*/

#define EMU_MAX_PERIPHERALS 32
#define EMU_MAX_BREAKPOINTS 64
#define EMU_MAX_WATCHPOINTS 32

/*============================================================================
 * Watchpoint Types
 *============================================================================*/

/**
 * Watchpoint type - matches GDB Z packet types.
 */
typedef enum {
    WATCHPOINT_WRITE = 2,       /**< Write watchpoint (Z2) */
    WATCHPOINT_READ = 3,        /**< Read watchpoint (Z3) */
    WATCHPOINT_ACCESS = 4,      /**< Access watchpoint - read or write (Z4) */
} WatchpointType;

/**
 * Watchpoint descriptor.
 */
typedef struct {
    uint32_t addr;              /**< Watched address */
    uint32_t size;              /**< Size of watched region */
    WatchpointType type;        /**< Type of watchpoint */
    bool active;                /**< Whether this slot is in use */
} Watchpoint;

/**
 * Emulator context - integrates all modules.
 */
typedef struct {
    /* Core modules */
    Executor exec;              /**< CPU executor (includes CPUState) */
    EmuMemorySystem mem;           /**< Memory subsystem */
    NVIC nvic;                  /**< Interrupt controller */
    MPU mpu;                    /**< Memory protection unit */

    /* Peripherals */
    EmuPeripheral *peripherals[EMU_MAX_PERIPHERALS];
    int num_peripherals;

    /* Emulator state */
    EmuState state;        /**< Current execution state */
    int last_error;             /**< Last error code */

    /* Memory backing storage */
    uint8_t *flash_data;        /**< Flash backing memory (allocated) */
    uint32_t flash_base;        /**< Flash base address */
    uint32_t flash_size;        /**< Flash size */
    uint8_t *ram_data;          /**< RAM backing memory (allocated) */
    uint32_t ram_base;          /**< RAM base address */
    uint32_t ram_size;          /**< RAM size */

    /* Breakpoints */
    uint32_t breakpoints[EMU_MAX_BREAKPOINTS];
    int num_breakpoints;

    /* Watchpoints */
    Watchpoint watchpoints[EMU_MAX_WATCHPOINTS];
    int num_watchpoints;
    uint32_t watchpoint_hit_addr;   /**< Address that triggered watchpoint */
    WatchpointType watchpoint_hit_type; /**< Type of access that triggered */

    /* Stop request flag (for external stop) */
    volatile bool stop_requested;
} Emulator;

/*============================================================================
 * Lifecycle API
 *============================================================================*/

/**
 * Initialize config with default values.
 *
 * @param config    Config to initialize
 */
void armv8m_emu_default_config(EmulatorConfig *config);

/**
 * Initialize emulator with configuration.
 *
 * @param emu       Emulator to initialize
 * @param config    Configuration (NULL for defaults)
 * @return          ARMV8M_OK or error code
 */
int armv8m_emu_init(Emulator *emu, const EmulatorConfig *config);

/**
 * Destroy emulator and free resources.
 *
 * @param emu       Emulator to destroy
 */
void armv8m_emu_destroy(Emulator *emu);

/**
 * Reset emulator to initial state.
 *
 * @param emu       Emulator to reset
 */
void armv8m_emu_reset(Emulator *emu);

/*============================================================================
 * Memory Setup API
 *============================================================================*/

/**
 * Add flash memory region.
 *
 * @param emu       Emulator
 * @param base      Base address
 * @param size      Size in bytes
 * @return          ARMV8M_OK or error code
 */
int armv8m_emu_add_flash(Emulator *emu, uint32_t base, uint32_t size);

/**
 * Add RAM region.
 *
 * @param emu       Emulator
 * @param base      Base address
 * @param size      Size in bytes
 * @return          ARMV8M_OK or error code
 */
int armv8m_emu_add_ram(Emulator *emu, uint32_t base, uint32_t size);

/**
 * Load data into memory.
 *
 * @param emu       Emulator
 * @param addr      Destination address
 * @param data      Data to load
 * @param size      Size in bytes
 * @return          ARMV8M_OK or error code
 */
int armv8m_emu_load(Emulator *emu, uint32_t addr, const uint8_t *data, uint32_t size);

/*============================================================================
 * Execution API
 *============================================================================*/

/**
 * Execute a single instruction.
 *
 * @param emu       Emulator
 * @return          ARMV8M_OK, ARMV8M_ERR_BREAKPOINT, or error code
 */
int armv8m_emu_step(Emulator *emu);

/**
 * Run until stopped, breakpoint, or max cycles reached.
 *
 * @param emu       Emulator
 * @param max_cycles Maximum cycles (0 = unlimited)
 * @return          Number of cycles executed, or negative on error
 */
int64_t armv8m_emu_run(Emulator *emu, uint64_t max_cycles);

/**
 * Request emulator to stop (thread-safe).
 *
 * @param emu       Emulator
 */
void armv8m_emu_stop(Emulator *emu);

/*============================================================================
 * State Access API
 *============================================================================*/

/**
 * Get general purpose register.
 *
 * @param emu       Emulator
 * @param reg       Register number (0-15)
 * @return          Register value
 */
uint32_t armv8m_emu_get_reg(const Emulator *emu, int reg);

/**
 * Set general purpose register.
 *
 * @param emu       Emulator
 * @param reg       Register number (0-15)
 * @param value     New value
 */
void armv8m_emu_set_reg(Emulator *emu, int reg, uint32_t value);

/**
 * Get program counter.
 *
 * @param emu       Emulator
 * @return          PC value
 */
uint32_t armv8m_emu_get_pc(const Emulator *emu);

/**
 * Set program counter.
 *
 * @param emu       Emulator
 * @param value     New PC value
 */
void armv8m_emu_set_pc(Emulator *emu, uint32_t value);

/**
 * Get xPSR register.
 *
 * @param emu       Emulator
 * @return          xPSR value
 */
uint32_t armv8m_emu_get_xpsr(const Emulator *emu);

/**
 * Set xPSR register.
 *
 * @param emu       Emulator
 * @param value     New xPSR value
 */
void armv8m_emu_set_xpsr(Emulator *emu, uint32_t value);

/**
 * Get total cycles executed.
 *
 * @param emu       Emulator
 * @return          Cycle count
 */
uint64_t armv8m_emu_get_cycles(const Emulator *emu);

/**
 * Get emulator state.
 *
 * @param emu       Emulator
 * @return          Current state
 */
EmuState armv8m_emu_get_state(const Emulator *emu);

/**
 * Get last error code.
 *
 * @param emu       Emulator
 * @return          Last error code
 */
int armv8m_emu_get_last_error(const Emulator *emu);

/*============================================================================
 * Memory Access API
 *============================================================================*/

/**
 * Read from memory (bypasses MPU for debugging).
 *
 * @param emu       Emulator
 * @param addr      Address
 * @param size      Access size (1, 2, or 4)
 * @param fault     Set to true if fault occurred
 * @return          Value read
 */
uint32_t armv8m_emu_read_mem(const Emulator *emu, uint32_t addr, uint8_t size, bool *fault);

/**
 * Write to memory (bypasses MPU for debugging).
 *
 * @param emu       Emulator
 * @param addr      Address
 * @param value     Value to write
 * @param size      Access size (1, 2, or 4)
 * @param fault     Set to true if fault occurred
 */
void armv8m_emu_write_mem(Emulator *emu, uint32_t addr, uint32_t value, uint8_t size, bool *fault);

/**
 * Read a block of memory.
 *
 * @param emu       Emulator
 * @param addr      Start address
 * @param data      Output buffer
 * @param size      Number of bytes to read
 * @return          Number of bytes read successfully
 */
uint32_t armv8m_emu_read_block(const Emulator *emu, uint32_t addr, uint8_t *data, uint32_t size);

/**
 * Write a block of memory.
 *
 * @param emu       Emulator
 * @param addr      Start address
 * @param data      Input buffer
 * @param size      Number of bytes to write
 * @return          Number of bytes written successfully
 */
uint32_t armv8m_emu_write_block(Emulator *emu, uint32_t addr, const uint8_t *data, uint32_t size);

/*============================================================================
 * Breakpoint API
 *============================================================================*/

/**
 * Add a breakpoint.
 *
 * @param emu       Emulator
 * @param addr      Breakpoint address
 * @return          ARMV8M_OK or error code
 */
int armv8m_emu_add_breakpoint(Emulator *emu, uint32_t addr);

/**
 * Remove a breakpoint.
 *
 * @param emu       Emulator
 * @param addr      Breakpoint address
 * @return          ARMV8M_OK or error code
 */
int armv8m_emu_remove_breakpoint(Emulator *emu, uint32_t addr);

/**
 * Check if address is a breakpoint.
 *
 * @param emu       Emulator
 * @param addr      Address to check
 * @return          true if breakpoint exists
 */
bool armv8m_emu_has_breakpoint(const Emulator *emu, uint32_t addr);

/**
 * Clear all breakpoints.
 *
 * @param emu       Emulator
 */
void armv8m_emu_clear_breakpoints(Emulator *emu);

/*============================================================================
 * Watchpoint API
 *============================================================================*/

/**
 * Add a watchpoint.
 *
 * @param emu       Emulator
 * @param addr      Watch address
 * @param size      Size of watched region (1, 2, or 4 bytes)
 * @param type      Watchpoint type (WATCHPOINT_WRITE, WATCHPOINT_READ, WATCHPOINT_ACCESS)
 * @return          ARMV8M_OK or error code
 */
int armv8m_emu_add_watchpoint(Emulator *emu, uint32_t addr, uint32_t size, WatchpointType type);

/**
 * Remove a watchpoint.
 *
 * @param emu       Emulator
 * @param addr      Watch address
 * @param size      Size of watched region
 * @param type      Watchpoint type
 * @return          ARMV8M_OK or error code
 */
int armv8m_emu_remove_watchpoint(Emulator *emu, uint32_t addr, uint32_t size, WatchpointType type);

/**
 * Check if address has a watchpoint of given type.
 *
 * @param emu       Emulator
 * @param addr      Address to check
 * @param size      Access size
 * @param is_write  True if checking for write access
 * @return          Pointer to matching watchpoint, or NULL
 */
const Watchpoint *armv8m_emu_check_watchpoint(const Emulator *emu, uint32_t addr, uint32_t size, bool is_write);

/**
 * Clear all watchpoints.
 *
 * @param emu       Emulator
 */
void armv8m_emu_clear_watchpoints(Emulator *emu);

/**
 * Get the address that triggered the last watchpoint hit.
 *
 * @param emu       Emulator
 * @return          Address that triggered watchpoint
 */
uint32_t armv8m_emu_get_watchpoint_hit_addr(const Emulator *emu);

/**
 * Get the type of access that triggered the last watchpoint hit.
 *
 * @param emu       Emulator
 * @return          Type of access (WATCHPOINT_READ or WATCHPOINT_WRITE)
 */
WatchpointType armv8m_emu_get_watchpoint_hit_type(const Emulator *emu);

/*============================================================================
 * Peripheral API
 *============================================================================*/

/**
 * Register a peripheral.
 *
 * @param emu       Emulator
 * @param periph    Peripheral to register
 * @param base      Base address
 * @param size      Address space size
 * @return          ARMV8M_OK or error code
 */
int armv8m_emu_add_peripheral(Emulator *emu, EmuPeripheral *periph, uint32_t base, uint32_t size);

/*============================================================================
 * Special Register Access (for GDB)
 *============================================================================*/

/**
 * Get special register value.
 *
 * @param emu       Emulator
 * @param reg       Special register ID (see armv8m_types.h)
 * @return          Register value
 */
uint32_t armv8m_emu_get_special_reg(const Emulator *emu, int reg);

/**
 * Set special register value.
 *
 * @param emu       Emulator
 * @param reg       Special register ID
 * @param value     New value
 */
void armv8m_emu_set_special_reg(Emulator *emu, int reg, uint32_t value);

/**
 * Get FPU register (S0-S31).
 *
 * @param emu       Emulator
 * @param reg       FPU register number (0-31)
 * @return          Register value (as uint32_t bits)
 */
uint32_t armv8m_emu_get_fpu_reg(const Emulator *emu, int reg);

/**
 * Set FPU register.
 *
 * @param emu       Emulator
 * @param reg       FPU register number (0-31)
 * @param value     New value (as uint32_t bits)
 */
void armv8m_emu_set_fpu_reg(Emulator *emu, int reg, uint32_t value);

/**
 * Get FPSCR register.
 *
 * @param emu       Emulator
 * @return          FPSCR value
 */
uint32_t armv8m_emu_get_fpscr(const Emulator *emu);

/**
 * Set FPSCR register.
 *
 * @param emu       Emulator
 * @param value     New FPSCR value
 */
void armv8m_emu_set_fpscr(Emulator *emu, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif /* ARMV8M_EMULATOR_H */
