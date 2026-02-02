/**
 * @file emu_memory.h
 * @brief Generic memory subsystem for multi-architecture support
 *
 * This file provides an architecture-agnostic memory system that supports
 * RAM, ROM, and MMIO regions. Uses 64-bit addresses to support both
 * 32-bit and 64-bit architectures.
 */

#ifndef EMU_MEMORY_H
#define EMU_MEMORY_H

#include "emu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Memory Region
 *============================================================================*/

/**
 * Memory region descriptor.
 */
typedef struct {
    uint64_t base;              /**< Base address */
    uint64_t size;              /**< Size in bytes */
    EmuMemRegionType type;      /**< Region type */

    /* Backing storage (for RAM/ROM) */
    uint8_t *data;              /**< Pointer to backing data (NULL for MMIO) */

    /* MMIO callbacks (for MMIO regions) */
    void *mmio_ctx;             /**< Context for MMIO callbacks */
    uint64_t (*mmio_read)(void *ctx, uint64_t offset, uint8_t size);
    void (*mmio_write)(void *ctx, uint64_t offset, uint64_t value, uint8_t size);
} EmuMemRegion;

/*============================================================================
 * Page Table for Fast Lookup
 *============================================================================*/

#define EMU_PAGE_SHIFT 12               /* 4KB pages */
#define EMU_PAGE_SIZE (1U << EMU_PAGE_SHIFT)
#define EMU_PAGE_MASK (EMU_PAGE_SIZE - 1)

/**
 * Maximum addressable memory for page table (16MB by default).
 * Adjust EMU_PAGE_TABLE_SIZE for larger address spaces.
 */
#define EMU_PAGE_TABLE_MAX_ADDR 0x01000000  /* 16MB */
#define EMU_PAGE_TABLE_SIZE (EMU_PAGE_TABLE_MAX_ADDR >> EMU_PAGE_SHIFT)  /* 4096 entries */

/**
 * Page table entry for fast region lookup.
 */
typedef struct {
    EmuMemRegion *region;   /**< Pointer to region (NULL if unmapped) */
    uint8_t *data_base;     /**< Pre-computed data pointer for fast RAM access */
} EmuPageEntry;

/*============================================================================
 * Memory System Context
 *============================================================================*/

#define EMU_MEM_MAX_REGIONS 32

/**
 * Memory system context.
 */
typedef struct {
    EmuMemRegion regions[EMU_MEM_MAX_REGIONS];
    int num_regions;

    /* Page table for O(1) region lookup */
    EmuPageEntry *page_table;       /**< Page table (NULL if not allocated) */
    uint32_t page_table_size;       /**< Number of entries in page table */
    bool page_table_valid;          /**< True if page table is up-to-date */

    /* MPU/MMU callbacks (optional, NULL if no protection unit) */
    void *mpu_ctx;
    bool (*mpu_check)(void *ctx, uint64_t addr, uint64_t size,
                      bool is_write, bool privileged);

    /* Fault callback (called on access faults) */
    void *fault_ctx;
    void (*on_fault)(void *ctx, uint64_t addr, bool is_write, int fault_type);
} EmuMemorySystem;

/*============================================================================
 * Memory API
 *============================================================================*/

/**
 * Initialize memory system.
 *
 * @param mem       Memory system to initialize
 */
void emu_mem_init(EmuMemorySystem *mem);

/**
 * Add a memory region.
 *
 * @param mem       Memory system
 * @param region    Region descriptor (copied internally)
 * @return          EMU_OK or error code
 */
int emu_mem_add_region(EmuMemorySystem *mem, const EmuMemRegion *region);

/**
 * Add a simple RAM region.
 *
 * @param mem       Memory system
 * @param base      Base address
 * @param size      Size in bytes
 * @param data      Backing storage (must remain valid)
 * @return          EMU_OK or error code
 */
int emu_mem_add_ram(EmuMemorySystem *mem, uint64_t base, uint64_t size, uint8_t *data);

/**
 * Add a ROM region.
 *
 * @param mem       Memory system
 * @param base      Base address
 * @param size      Size in bytes
 * @param data      Backing storage (must remain valid)
 * @return          EMU_OK or error code
 */
int emu_mem_add_rom(EmuMemorySystem *mem, uint64_t base, uint64_t size, const uint8_t *data);

/**
 * Add an MMIO region.
 *
 * @param mem       Memory system
 * @param base      Base address
 * @param size      Size in bytes
 * @param ctx       Context for callbacks
 * @param read_cb   Read callback
 * @param write_cb  Write callback
 * @return          EMU_OK or error code
 */
int emu_mem_add_mmio(EmuMemorySystem *mem, uint64_t base, uint64_t size,
                     void *ctx,
                     uint64_t (*read_cb)(void *ctx, uint64_t offset, uint8_t size),
                     void (*write_cb)(void *ctx, uint64_t offset, uint64_t value, uint8_t size));

/**
 * Read from memory.
 *
 * @param mem       Memory system
 * @param addr      Address to read from
 * @param size      Access size (1, 2, 4, or 8 bytes)
 * @param privileged True if privileged access
 * @param fault     Set to true if fault occurred
 * @return          Value read (undefined if fault)
 */
uint64_t emu_mem_read(EmuMemorySystem *mem, uint64_t addr, uint8_t size,
                      bool privileged, bool *fault);

/**
 * Write to memory.
 *
 * @param mem       Memory system
 * @param addr      Address to write to
 * @param value     Value to write
 * @param size      Access size (1, 2, 4, or 8 bytes)
 * @param privileged True if privileged access
 * @param fault     Set to true if fault occurred
 */
void emu_mem_write(EmuMemorySystem *mem, uint64_t addr, uint64_t value, uint8_t size,
                   bool privileged, bool *fault);

/**
 * Get direct pointer to memory (for instruction fetch).
 * Returns NULL if address is not backed by contiguous RAM/ROM.
 *
 * @param mem       Memory system
 * @param addr      Address
 * @param size      Required size
 * @return          Pointer to memory or NULL
 */
const uint8_t *emu_mem_get_ptr(EmuMemorySystem *mem, uint64_t addr, uint64_t size);

/**
 * Load data into memory.
 *
 * @param mem       Memory system
 * @param addr      Destination address
 * @param data      Data to load
 * @param size      Size in bytes
 * @return          EMU_OK or error code
 */
int emu_mem_load(EmuMemorySystem *mem, uint64_t addr, const uint8_t *data, uint64_t size);

/**
 * Find region containing address.
 *
 * @param mem       Memory system
 * @param addr      Address to find
 * @return          Region pointer or NULL if not found
 */
const EmuMemRegion *emu_mem_find_region(const EmuMemorySystem *mem, uint64_t addr);

/**
 * Set MPU/MMU check callback.
 *
 * @param mem       Memory system
 * @param ctx       Context for callback
 * @param check_cb  Protection check callback
 */
void emu_mem_set_mpu(EmuMemorySystem *mem, void *ctx,
                     bool (*check_cb)(void *ctx, uint64_t addr, uint64_t size,
                                      bool is_write, bool privileged));

/**
 * Set fault callback.
 *
 * @param mem       Memory system
 * @param ctx       Context for callback
 * @param fault_cb  Fault callback
 */
void emu_mem_set_fault_callback(EmuMemorySystem *mem, void *ctx,
                                void (*fault_cb)(void *ctx, uint64_t addr,
                                                 bool is_write, int fault_type));

/**
 * Initialize page table for fast memory lookup.
 * Call this after adding all memory regions for best performance.
 *
 * @param mem       Memory system
 * @param max_addr  Maximum address to cover (0 for default 16MB)
 * @return          EMU_OK or error code
 */
int emu_mem_init_page_table(EmuMemorySystem *mem, uint64_t max_addr);

/**
 * Free page table resources.
 *
 * @param mem       Memory system
 */
void emu_mem_free_page_table(EmuMemorySystem *mem);

/**
 * Invalidate page table (call when regions change).
 *
 * @param mem       Memory system
 */
void emu_mem_invalidate_page_table(EmuMemorySystem *mem);

#ifdef __cplusplus
}
#endif

#endif /* EMU_MEMORY_H */
