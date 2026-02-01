/**
 * @file emu_types.h
 * @brief Architecture-agnostic type definitions for emulator framework
 *
 * This file defines common types, error codes, and enumerations used
 * across all architecture implementations.
 */

#ifndef EMU_TYPES_H
#define EMU_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Architecture Identifiers
 *============================================================================*/

/**
 * Supported architecture types.
 */
typedef enum {
    EMU_ARCH_UNKNOWN = 0,
    EMU_ARCH_ARMV8M,        /**< ARMv8-M Mainline (Cortex-M33, etc.) */
    EMU_ARCH_ARMV7M,        /**< ARMv7-M (Cortex-M3/M4/M7) */
    EMU_ARCH_RISCV32,       /**< RISC-V 32-bit */
    EMU_ARCH_RISCV64,       /**< RISC-V 64-bit */
} EmuArchType;

/*============================================================================
 * Error Codes
 *============================================================================*/

/**
 * Generic error codes used across all architectures.
 * Architecture-specific error codes should be defined in arch headers.
 */
typedef enum {
    EMU_OK = 0,                     /**< Success */
    EMU_ERR_INVALID_PARAM = -1,     /**< Invalid parameter */
    EMU_ERR_OUT_OF_MEMORY = -2,     /**< Memory allocation failed */
    EMU_ERR_NOT_SUPPORTED = -3,     /**< Operation not supported */
    EMU_ERR_NOT_INITIALIZED = -4,   /**< Component not initialized */

    /* Execution errors */
    EMU_ERR_UNDEFINED_INSN = -10,   /**< Undefined instruction */
    EMU_ERR_UNPREDICTABLE = -11,    /**< Unpredictable behavior */
    EMU_ERR_BREAKPOINT = -12,       /**< Hit breakpoint */
    EMU_ERR_HALTED = -13,           /**< CPU is halted */

    /* Fault errors */
    EMU_ERR_BUS_FAULT = -20,        /**< Bus fault */
    EMU_ERR_MEM_FAULT = -21,        /**< Memory fault */
    EMU_ERR_USAGE_FAULT = -22,      /**< Usage fault */
    EMU_ERR_HARD_FAULT = -23,       /**< Hard fault */
    EMU_ERR_SECURITY_FAULT = -24,   /**< Security fault (TrustZone) */
} EmuError;

/*============================================================================
 * Access Size
 *============================================================================*/

/**
 * Memory access sizes.
 */
typedef enum {
    EMU_ACCESS_BYTE = 1,        /**< 8-bit access */
    EMU_ACCESS_HALF = 2,        /**< 16-bit access */
    EMU_ACCESS_WORD = 4,        /**< 32-bit access */
    EMU_ACCESS_DWORD = 8,       /**< 64-bit access */
} EmuAccessSize;

/*============================================================================
 * Emulator State
 *============================================================================*/

/**
 * Emulator execution state.
 */
typedef enum {
    EMU_STATE_STOPPED = 0,      /**< Not running */
    EMU_STATE_RUNNING,          /**< Currently executing */
    EMU_STATE_HALTED,           /**< Halted (WFI/WFE/debug halt) */
    EMU_STATE_BREAKPOINT,       /**< Hit a breakpoint */
    EMU_STATE_FAULT,            /**< Unrecoverable fault */
} EmuState;

/*============================================================================
 * Memory Region Types
 *============================================================================*/

/**
 * Memory region types.
 */
typedef enum {
    EMU_MEM_REGION_RAM,         /**< Read/Write memory */
    EMU_MEM_REGION_ROM,         /**< Read-only memory */
    EMU_MEM_REGION_MMIO,        /**< Memory-mapped I/O */
    EMU_MEM_REGION_UNMAPPED,    /**< Unmapped (faults on access) */
} EmuMemRegionType;

/*============================================================================
 * Register Description
 *============================================================================*/

/**
 * Register descriptor for architecture-agnostic register enumeration.
 */
typedef struct {
    const char *name;           /**< Register name (e.g., "r0", "sp", "pc") */
    int id;                     /**< Architecture-specific register ID */
    int size;                   /**< Register size in bytes */
    int gdb_regnum;             /**< GDB register number (-1 if not exposed) */
} EmuRegisterDesc;

/*============================================================================
 * CPU Information
 *============================================================================*/

/**
 * Architecture-agnostic CPU information.
 */
typedef struct {
    EmuArchType arch;           /**< Architecture type */
    const char *arch_name;      /**< Architecture name string */
    int num_gp_regs;            /**< Number of general purpose registers */
    int addr_bits;              /**< Address bus width (32 or 64) */
    int reg_bits;               /**< Register width (32 or 64) */
    int num_regs;               /**< Total number of registers in regs array */
    const EmuRegisterDesc *regs; /**< Register descriptors */
} EmuCPUInfo;

/*============================================================================
 * Callback Types
 *============================================================================*/

/**
 * Generic memory read callback.
 *
 * @param ctx       Opaque context
 * @param addr      Address to read
 * @param size      Access size (1, 2, 4, or 8)
 * @param fault     Set to true if fault occurred
 * @return          Value read (zero-extended)
 */
typedef uint64_t (*EmuMemReadCallback)(void *ctx, uint64_t addr, uint8_t size, bool *fault);

/**
 * Generic memory write callback.
 *
 * @param ctx       Opaque context
 * @param addr      Address to write
 * @param value     Value to write
 * @param size      Access size (1, 2, 4, or 8)
 * @param fault     Set to true if fault occurred
 */
typedef void (*EmuMemWriteCallback)(void *ctx, uint64_t addr, uint64_t value, uint8_t size, bool *fault);

/**
 * IRQ callback - peripheral calls this to assert/deassert an interrupt.
 *
 * @param ctx       Emulator context
 * @param irq       IRQ number
 * @param level     1 = assert, 0 = deassert
 */
typedef void (*EmuIRQCallback)(void *ctx, int irq, int level);

#ifdef __cplusplus
}
#endif

#endif /* EMU_TYPES_H */
