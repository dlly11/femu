/**
 * @file armv8m_memory.h
 * @brief Memory subsystem for ARMv8-M emulator
 *
 * AI INSTRUCTIONS:
 * - This header defines the COMPLETE interface for the memory module
 * - Implementation goes in src/core/memory/
 * - Depends on: armv8m_types.h
 * - See src/core/memory/README.md for implementation guidance
 */

#ifndef ARMV8M_MEMORY_H
#define ARMV8M_MEMORY_H

#include "armv8m_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Memory Region Types
 *============================================================================*/

typedef enum {
    MEM_REGION_RAM,         /**< Read/Write memory */
    MEM_REGION_ROM,         /**< Read-only memory */
    MEM_REGION_MMIO,        /**< Memory-mapped I/O */
    MEM_REGION_UNMAPPED,    /**< Unmapped (faults on access) */
} MemRegionType;

typedef enum {
    MEM_ATTR_NORMAL = 0,        /**< Normal memory */
    MEM_ATTR_DEVICE = 1,        /**< Device memory (no speculation) */
    MEM_ATTR_STRONGLY_ORDERED = 2,  /**< Strongly ordered */
} MemoryAttribute;

/*============================================================================
 * Memory Region
 *============================================================================*/

/**
 * Memory region descriptor.
 */
typedef struct {
    uint32_t base;          /**< Base address */
    uint32_t size;          /**< Size in bytes */
    MemRegionType type;     /**< Region type */
    MemoryAttribute attr;   /**< Memory attributes */

    /* Backing storage (for RAM/ROM) */
    uint8_t *data;          /**< Pointer to backing data (NULL for MMIO) */

    /* MMIO callbacks (for MMIO regions) */
    void *mmio_ctx;         /**< Context for MMIO callbacks */
    uint32_t (*mmio_read)(void *ctx, uint32_t offset, uint8_t size);
    void (*mmio_write)(void *ctx, uint32_t offset, uint32_t value, uint8_t size);
} MemRegion;

/*============================================================================
 * Memory System Context
 *============================================================================*/

#define MEM_MAX_REGIONS 32

/**
 * Memory system context.
 */
typedef struct {
    MemRegion regions[MEM_MAX_REGIONS];
    int num_regions;

    /* MPU callbacks (optional, NULL if no MPU) */
    void *mpu_ctx;
    bool (*mpu_check)(void *ctx, uint32_t addr, uint32_t size,
                      bool is_write, bool privileged, bool in_hardfault_nmi);

    /* Fault callback (called on access faults) */
    void *fault_ctx;
    void (*on_fault)(void *ctx, uint32_t addr, bool is_write, int fault_type);
} MemorySystem;

/*============================================================================
 * Memory API
 *============================================================================*/

/**
 * Initialize memory system.
 *
 * @param mem       Memory system to initialize
 */
void armv8m_mem_init(MemorySystem *mem);

/**
 * Add a memory region.
 *
 * @param mem       Memory system
 * @param region    Region descriptor (copied internally)
 * @return          ARMV8M_OK or error code
 */
int armv8m_mem_add_region(MemorySystem *mem, const MemRegion *region);

/**
 * Add a simple RAM region.
 *
 * @param mem       Memory system
 * @param base      Base address
 * @param size      Size in bytes
 * @param data      Backing storage (must remain valid)
 * @return          ARMV8M_OK or error code
 */
int armv8m_mem_add_ram(MemorySystem *mem, uint32_t base, uint32_t size, uint8_t *data);

/**
 * Add a ROM region.
 *
 * @param mem       Memory system
 * @param base      Base address
 * @param size      Size in bytes
 * @param data      Backing storage (must remain valid)
 * @return          ARMV8M_OK or error code
 */
int armv8m_mem_add_rom(MemorySystem *mem, uint32_t base, uint32_t size, const uint8_t *data);

/**
 * Add an MMIO region.
 *
 * @param mem       Memory system
 * @param base      Base address
 * @param size      Size in bytes
 * @param ctx       Context for callbacks
 * @param read_cb   Read callback
 * @param write_cb  Write callback
 * @return          ARMV8M_OK or error code
 */
int armv8m_mem_add_mmio(MemorySystem *mem, uint32_t base, uint32_t size,
                        void *ctx,
                        uint32_t (*read_cb)(void *ctx, uint32_t offset, uint8_t size),
                        void (*write_cb)(void *ctx, uint32_t offset, uint32_t value, uint8_t size));

/**
 * Read from memory.
 *
 * @param mem               Memory system
 * @param addr              Address to read from
 * @param size              Access size (1, 2, or 4 bytes)
 * @param privileged        True if privileged access
 * @param in_hardfault_nmi  True if executing in HardFault or NMI handler
 * @param fault             Set to true if fault occurred
 * @return                  Value read (undefined if fault)
 */
uint32_t armv8m_mem_read(MemorySystem *mem, uint32_t addr, uint8_t size,
                         bool privileged, bool in_hardfault_nmi, bool *fault);

/**
 * Write to memory.
 *
 * @param mem               Memory system
 * @param addr              Address to write to
 * @param value             Value to write
 * @param size              Access size (1, 2, or 4 bytes)
 * @param privileged        True if privileged access
 * @param in_hardfault_nmi  True if executing in HardFault or NMI handler
 * @param fault             Set to true if fault occurred
 */
void armv8m_mem_write(MemorySystem *mem, uint32_t addr, uint32_t value, uint8_t size,
                      bool privileged, bool in_hardfault_nmi, bool *fault);

/**
 * Get direct pointer to memory (for instruction fetch).
 * Returns NULL if address is not backed by contiguous RAM/ROM.
 *
 * @param mem       Memory system
 * @param addr      Address
 * @param size      Required size
 * @return          Pointer to memory or NULL
 */
const uint8_t *armv8m_mem_get_ptr(MemorySystem *mem, uint32_t addr, uint32_t size);

/**
 * Load data into memory.
 *
 * @param mem       Memory system
 * @param addr      Destination address
 * @param data      Data to load
 * @param size      Size in bytes
 * @return          ARMV8M_OK or error code
 */
int armv8m_mem_load(MemorySystem *mem, uint32_t addr, const uint8_t *data, uint32_t size);

/**
 * Find region containing address.
 *
 * @param mem       Memory system
 * @param addr      Address to find
 * @return          Region pointer or NULL if not found
 */
const MemRegion *armv8m_mem_find_region(const MemorySystem *mem, uint32_t addr);

#ifdef __cplusplus
}
#endif

#endif /* ARMV8M_MEMORY_H */
