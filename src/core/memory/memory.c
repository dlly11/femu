/**
 * @file memory.c
 * @brief Generic memory subsystem implementation
 *
 * Manages RAM, ROM, and MMIO regions. Dispatches memory accesses to the
 * appropriate backing store or peripheral. Supports both 32-bit and 64-bit
 * architectures through use of uint64_t addresses.
 */

#include "emu/emu_memory.h"
#include <string.h>
#include <stdlib.h>

/*============================================================================
 * Internal Helpers
 *============================================================================*/

/**
 * Find region containing the given address.
 */
static EmuMemRegion *find_region_internal(EmuMemorySystem *mem, uint64_t addr)
{
    for (int i = 0; i < mem->num_regions; i++) {
        EmuMemRegion *r = &mem->regions[i];
        /* Use subtraction to avoid overflow: addr < base + size  =>  addr - base < size */
        if (addr >= r->base && (addr - r->base) < r->size) {
            return r;
        }
    }
    return NULL;
}

/**
 * Read from RAM/ROM region (little-endian).
 */
static uint64_t read_ram(const EmuMemRegion *r, uint64_t addr, uint8_t size)
{
    uint64_t offset = addr - r->base;
    const uint8_t *data = r->data;

    switch (size) {
        case 1:
            return data[offset];
        case 2:
            return (uint64_t)data[offset] |
                   ((uint64_t)data[offset + 1] << 8);
        case 4:
            return (uint64_t)data[offset] |
                   ((uint64_t)data[offset + 1] << 8) |
                   ((uint64_t)data[offset + 2] << 16) |
                   ((uint64_t)data[offset + 3] << 24);
        case 8:
            return (uint64_t)data[offset] |
                   ((uint64_t)data[offset + 1] << 8) |
                   ((uint64_t)data[offset + 2] << 16) |
                   ((uint64_t)data[offset + 3] << 24) |
                   ((uint64_t)data[offset + 4] << 32) |
                   ((uint64_t)data[offset + 5] << 40) |
                   ((uint64_t)data[offset + 6] << 48) |
                   ((uint64_t)data[offset + 7] << 56);
        default:
            return 0;
    }
}

/**
 * Write to RAM region (little-endian).
 */
static void write_ram(const EmuMemRegion *r, uint64_t addr, uint64_t value, uint8_t size)
{
    uint64_t offset = addr - r->base;
    uint8_t *data = r->data;

    switch (size) {
        case 1:
            data[offset] = (uint8_t)value;
            break;
        case 2:
            data[offset] = (uint8_t)value;
            data[offset + 1] = (uint8_t)(value >> 8);
            break;
        case 4:
            data[offset] = (uint8_t)value;
            data[offset + 1] = (uint8_t)(value >> 8);
            data[offset + 2] = (uint8_t)(value >> 16);
            data[offset + 3] = (uint8_t)(value >> 24);
            break;
        case 8:
            data[offset] = (uint8_t)value;
            data[offset + 1] = (uint8_t)(value >> 8);
            data[offset + 2] = (uint8_t)(value >> 16);
            data[offset + 3] = (uint8_t)(value >> 24);
            data[offset + 4] = (uint8_t)(value >> 32);
            data[offset + 5] = (uint8_t)(value >> 40);
            data[offset + 6] = (uint8_t)(value >> 48);
            data[offset + 7] = (uint8_t)(value >> 56);
            break;
        default:
            break;
    }
}

/**
 * Report a memory fault.
 */
static void report_fault(EmuMemorySystem *mem, uint64_t addr, bool is_write, int fault_type)
{
    if (mem->on_fault) {
        mem->on_fault(mem->fault_ctx, addr, is_write, fault_type);
    }
}

/**
 * Check if access size is valid (1, 2, 4, or 8 bytes).
 */
static bool is_valid_size(uint8_t size)
{
    return size == 1 || size == 2 || size == 4 || size == 8;
}

/**
 * Check if address is properly aligned for the access size.
 * Returns true if aligned, false if misaligned.
 */
static bool is_aligned(uint64_t addr, uint8_t size)
{
    return (addr & (uint64_t)(size - 1)) == 0;
}

/*============================================================================
 * Page Table Functions
 *============================================================================*/

/**
 * Build the page table from current regions.
 */
static void build_page_table(EmuMemorySystem *mem)
{
    if (!mem->page_table) {
        return;
    }

    /* Clear the page table */
    memset(mem->page_table, 0, mem->page_table_size * sizeof(EmuPageEntry));

    /* Populate page table entries from regions */
    for (int i = 0; i < mem->num_regions; i++) {
        EmuMemRegion *r = &mem->regions[i];

        /* Calculate page range for this region */
        uint64_t start_page = r->base >> EMU_PAGE_SHIFT;
        uint64_t end_addr = r->base + r->size;
        uint64_t end_page = (end_addr + EMU_PAGE_SIZE - 1) >> EMU_PAGE_SHIFT;

        /* Fill in page table entries */
        for (uint64_t page = start_page; page < end_page && page < mem->page_table_size; page++) {
            mem->page_table[page].region = r;

            /* Pre-compute data base for fast RAM/ROM access */
            if (r->data && (r->type == EMU_MEM_REGION_RAM || r->type == EMU_MEM_REGION_ROM)) {
                /* data_base points so that data_base[addr] gives the right byte */
                mem->page_table[page].data_base = r->data - r->base;
            }
        }
    }

    mem->page_table_valid = true;
}

/**
 * Fast region lookup using page table.
 */
static inline EmuMemRegion *fast_find_region(EmuMemorySystem *mem, uint64_t addr)
{
    /* Build page table on first access if not valid */
    if (mem->page_table && !mem->page_table_valid) {
        build_page_table(mem);
    }

    /* Fast path: use page table */
    if (mem->page_table) {
        uint64_t page = addr >> EMU_PAGE_SHIFT;
        if (page < mem->page_table_size) {
            EmuPageEntry *entry = &mem->page_table[page];
            if (entry->region) {
                /* Verify address is actually in region (handles partial page coverage) */
                EmuMemRegion *r = entry->region;
                if (addr >= r->base && (addr - r->base) < r->size) {
                    return r;
                }
            }
        }
    }

    /* Slow path: linear search */
    return find_region_internal(mem, addr);
}

/**
 * Fast memory read for RAM/ROM using pre-computed data base.
 */
static inline uint64_t fast_read_ram(const EmuPageEntry *entry, uint64_t addr, uint8_t size)
{
    const uint8_t *data = entry->data_base + addr;

    switch (size) {
        case 1:
            return data[0];
        case 2:
            return (uint64_t)data[0] |
                   ((uint64_t)data[1] << 8);
        case 4:
            return (uint64_t)data[0] |
                   ((uint64_t)data[1] << 8) |
                   ((uint64_t)data[2] << 16) |
                   ((uint64_t)data[3] << 24);
        case 8:
            return (uint64_t)data[0] |
                   ((uint64_t)data[1] << 8) |
                   ((uint64_t)data[2] << 16) |
                   ((uint64_t)data[3] << 24) |
                   ((uint64_t)data[4] << 32) |
                   ((uint64_t)data[5] << 40) |
                   ((uint64_t)data[6] << 48) |
                   ((uint64_t)data[7] << 56);
        default:
            return 0;
    }
}

/**
 * Fast memory write for RAM using pre-computed data base.
 */
static inline void fast_write_ram(const EmuPageEntry *entry, uint64_t addr, uint64_t value, uint8_t size)
{
    uint8_t *data = entry->data_base + addr;

    switch (size) {
        case 1:
            data[0] = (uint8_t)value;
            break;
        case 2:
            data[0] = (uint8_t)value;
            data[1] = (uint8_t)(value >> 8);
            break;
        case 4:
            data[0] = (uint8_t)value;
            data[1] = (uint8_t)(value >> 8);
            data[2] = (uint8_t)(value >> 16);
            data[3] = (uint8_t)(value >> 24);
            break;
        case 8:
            data[0] = (uint8_t)value;
            data[1] = (uint8_t)(value >> 8);
            data[2] = (uint8_t)(value >> 16);
            data[3] = (uint8_t)(value >> 24);
            data[4] = (uint8_t)(value >> 32);
            data[5] = (uint8_t)(value >> 40);
            data[6] = (uint8_t)(value >> 48);
            data[7] = (uint8_t)(value >> 56);
            break;
        default:
            break;
    }
}

/*============================================================================
 * Public API
 *============================================================================*/

void emu_mem_init(EmuMemorySystem *mem)
{
    memset(mem, 0, sizeof(*mem));
}

int emu_mem_add_region(EmuMemorySystem *mem, const EmuMemRegion *region)
{
    if (mem->num_regions >= EMU_MEM_MAX_REGIONS) {
        return EMU_ERR_MEM_FAULT;
    }

    mem->regions[mem->num_regions] = *region;
    mem->num_regions++;

    /* Invalidate page table so it gets rebuilt on next access */
    mem->page_table_valid = false;

    return EMU_OK;
}

int emu_mem_add_ram(EmuMemorySystem *mem, uint64_t base, uint64_t size, uint8_t *data)
{
    EmuMemRegion region = {
        .base = base,
        .size = size,
        .type = EMU_MEM_REGION_RAM,
        .data = data,
        .mmio_ctx = NULL,
        .mmio_read = NULL,
        .mmio_write = NULL
    };

    return emu_mem_add_region(mem, &region);
}

int emu_mem_add_rom(EmuMemorySystem *mem, uint64_t base, uint64_t size, const uint8_t *data)
{
    EmuMemRegion region = {
        .base = base,
        .size = size,
        .type = EMU_MEM_REGION_ROM,
        .data = (uint8_t *)data,  /* Cast away const - we won't write to it */
        .mmio_ctx = NULL,
        .mmio_read = NULL,
        .mmio_write = NULL
    };

    return emu_mem_add_region(mem, &region);
}

int emu_mem_add_mmio(EmuMemorySystem *mem, uint64_t base, uint64_t size,
                     void *ctx,
                     uint64_t (*read_cb)(void *ctx, uint64_t offset, uint8_t size),
                     void (*write_cb)(void *ctx, uint64_t offset, uint64_t value, uint8_t size))
{
    EmuMemRegion region = {
        .base = base,
        .size = size,
        .type = EMU_MEM_REGION_MMIO,
        .data = NULL,
        .mmio_ctx = ctx,
        .mmio_read = read_cb,
        .mmio_write = write_cb
    };

    return emu_mem_add_region(mem, &region);
}

uint64_t emu_mem_read(EmuMemorySystem *mem, uint64_t addr, uint8_t size,
                      bool privileged, bool *fault)
{
    *fault = false;

    /* Validate access size (must be 1, 2, 4, or 8) */
    if (!is_valid_size(size)) {
        *fault = true;
        report_fault(mem, addr, false, EMU_ERR_USAGE_FAULT);
        return 0;
    }

    /* Check MPU if configured */
    if (mem->mpu_check) {
        if (!mem->mpu_check(mem->mpu_ctx, addr, size, false, privileged)) {
            *fault = true;
            report_fault(mem, addr, false, EMU_ERR_MEM_FAULT);
            return 0;
        }
    }

    /* Fast path: use page table for RAM/ROM */
    if (mem->page_table && mem->page_table_valid) {
        uint64_t page = addr >> EMU_PAGE_SHIFT;
        if (page < mem->page_table_size) {
            EmuPageEntry *entry = &mem->page_table[page];
            if (entry->data_base) {
                /* Verify address is in region and access fits */
                EmuMemRegion *r = entry->region;
                uint64_t offset = addr - r->base;
                if (addr >= r->base && r->size - offset >= size) {
                    return fast_read_ram(entry, addr, size);
                }
            }
        }
    }

    /* Slow path: find region */
    EmuMemRegion *r = fast_find_region(mem, addr);
    if (!r) {
        *fault = true;
        report_fault(mem, addr, false, EMU_ERR_BUS_FAULT);
        return 0;
    }

    /* Check alignment for non-RAM/ROM regions */
    if (!is_aligned(addr, size) && r->type == EMU_MEM_REGION_MMIO) {
        *fault = true;
        report_fault(mem, addr, false, EMU_ERR_USAGE_FAULT);
        return 0;
    }

    /* Check that the entire access fits within the region */
    uint64_t offset = addr - r->base;
    if (r->size - offset < size) {
        *fault = true;
        report_fault(mem, addr, false, EMU_ERR_BUS_FAULT);
        return 0;
    }

    /* Dispatch based on region type */
    switch (r->type) {
        case EMU_MEM_REGION_RAM:
        case EMU_MEM_REGION_ROM:
            return read_ram(r, addr, size);

        case EMU_MEM_REGION_MMIO:
            if (r->mmio_read) {
                return r->mmio_read(r->mmio_ctx, offset, size);
            }
            *fault = true;
            report_fault(mem, addr, false, EMU_ERR_BUS_FAULT);
            return 0;

        case EMU_MEM_REGION_UNMAPPED:
        default:
            *fault = true;
            report_fault(mem, addr, false, EMU_ERR_BUS_FAULT);
            return 0;
    }
}

void emu_mem_write(EmuMemorySystem *mem, uint64_t addr, uint64_t value, uint8_t size,
                   bool privileged, bool *fault)
{
    *fault = false;

    /* Validate access size (must be 1, 2, 4, or 8) */
    if (!is_valid_size(size)) {
        *fault = true;
        report_fault(mem, addr, true, EMU_ERR_USAGE_FAULT);
        return;
    }

    /* Check MPU if configured */
    if (mem->mpu_check) {
        if (!mem->mpu_check(mem->mpu_ctx, addr, size, true, privileged)) {
            *fault = true;
            report_fault(mem, addr, true, EMU_ERR_MEM_FAULT);
            return;
        }
    }

    /* Fast path: use page table for RAM */
    if (mem->page_table && mem->page_table_valid) {
        uint64_t page = addr >> EMU_PAGE_SHIFT;
        if (page < mem->page_table_size) {
            EmuPageEntry *entry = &mem->page_table[page];
            if (entry->data_base && entry->region->type == EMU_MEM_REGION_RAM) {
                /* Verify address is in region and access fits */
                EmuMemRegion *r = entry->region;
                uint64_t offset = addr - r->base;
                if (addr >= r->base && r->size - offset >= size) {
                    fast_write_ram(entry, addr, value, size);
                    return;
                }
            }
        }
    }

    /* Slow path: find region */
    EmuMemRegion *r = fast_find_region(mem, addr);
    if (!r) {
        *fault = true;
        report_fault(mem, addr, true, EMU_ERR_BUS_FAULT);
        return;
    }

    /* Check alignment for non-RAM regions */
    if (!is_aligned(addr, size) && r->type == EMU_MEM_REGION_MMIO) {
        *fault = true;
        report_fault(mem, addr, true, EMU_ERR_USAGE_FAULT);
        return;
    }

    /* Check that the entire access fits within the region */
    uint64_t offset = addr - r->base;
    if (r->size - offset < size) {
        *fault = true;
        report_fault(mem, addr, true, EMU_ERR_BUS_FAULT);
        return;
    }

    /* Dispatch based on region type */
    switch (r->type) {
        case EMU_MEM_REGION_RAM:
            write_ram(r, addr, value, size);
            break;

        case EMU_MEM_REGION_ROM:
            /* Write to ROM faults */
            *fault = true;
            report_fault(mem, addr, true, EMU_ERR_MEM_FAULT);
            break;

        case EMU_MEM_REGION_MMIO:
            if (r->mmio_write) {
                r->mmio_write(r->mmio_ctx, offset, value, size);
            } else {
                *fault = true;
                report_fault(mem, addr, true, EMU_ERR_BUS_FAULT);
            }
            break;

        case EMU_MEM_REGION_UNMAPPED:
        default:
            *fault = true;
            report_fault(mem, addr, true, EMU_ERR_BUS_FAULT);
            break;
    }
}

const uint8_t *emu_mem_get_ptr(EmuMemorySystem *mem, uint64_t addr, uint64_t size)
{
    /* Fast path: use page table */
    if (mem->page_table && mem->page_table_valid) {
        uint64_t page = addr >> EMU_PAGE_SHIFT;
        if (page < mem->page_table_size) {
            EmuPageEntry *entry = &mem->page_table[page];
            if (entry->data_base && entry->region) {
                EmuMemRegion *r = entry->region;
                if (addr >= r->base && r->size - (addr - r->base) >= size) {
                    return entry->data_base + addr;
                }
            }
        }
    }

    /* Slow path */
    EmuMemRegion *r = fast_find_region(mem, addr);
    if (!r) {
        return NULL;
    }

    /* Only RAM and ROM have direct backing storage */
    if (r->type != EMU_MEM_REGION_RAM && r->type != EMU_MEM_REGION_ROM) {
        return NULL;
    }

    /* Check that requested size fits in region */
    uint64_t offset = addr - r->base;
    if (r->size - offset < size) {
        return NULL;
    }

    return &r->data[offset];
}

int emu_mem_load(EmuMemorySystem *mem, uint64_t addr, const uint8_t *data, uint64_t size)
{
    EmuMemRegion *r = find_region_internal(mem, addr);
    if (!r) {
        return EMU_ERR_BUS_FAULT;
    }

    /* Only RAM is writable for load */
    if (r->type != EMU_MEM_REGION_RAM) {
        return EMU_ERR_MEM_FAULT;
    }

    /* Check that the entire load fits within the region */
    uint64_t offset = addr - r->base;
    if (r->size - offset < size) {
        return EMU_ERR_BUS_FAULT;
    }

    memcpy(&r->data[offset], data, (size_t)size);

    return EMU_OK;
}

const EmuMemRegion *emu_mem_find_region(const EmuMemorySystem *mem, uint64_t addr)
{
    for (int i = 0; i < mem->num_regions; i++) {
        const EmuMemRegion *r = &mem->regions[i];
        /* Use subtraction to avoid overflow */
        if (addr >= r->base && (addr - r->base) < r->size) {
            return r;
        }
    }
    return NULL;
}

void emu_mem_set_mpu(EmuMemorySystem *mem, void *ctx,
                     bool (*check_cb)(void *ctx, uint64_t addr, uint64_t size,
                                      bool is_write, bool privileged))
{
    mem->mpu_ctx = ctx;
    mem->mpu_check = check_cb;
}

void emu_mem_set_fault_callback(EmuMemorySystem *mem, void *ctx,
                                void (*fault_cb)(void *ctx, uint64_t addr,
                                                 bool is_write, int fault_type))
{
    mem->fault_ctx = ctx;
    mem->on_fault = fault_cb;
}

/*============================================================================
 * Page Table Management
 *============================================================================*/

int emu_mem_init_page_table(EmuMemorySystem *mem, uint64_t max_addr)
{
    /* Free existing page table if any */
    if (mem->page_table) {
        free(mem->page_table);
        mem->page_table = NULL;
    }

    /* Use default if not specified */
    if (max_addr == 0) {
        max_addr = EMU_PAGE_TABLE_MAX_ADDR;
    }

    /* Calculate number of pages needed */
    uint32_t num_pages = (uint32_t)((max_addr + EMU_PAGE_SIZE - 1) >> EMU_PAGE_SHIFT);

    /* Allocate page table */
    mem->page_table = (EmuPageEntry *)calloc(num_pages, sizeof(EmuPageEntry));
    if (!mem->page_table) {
        return EMU_ERR_OUT_OF_MEMORY;
    }

    mem->page_table_size = num_pages;
    mem->page_table_valid = false;  /* Will be built on first access */

    return EMU_OK;
}

void emu_mem_free_page_table(EmuMemorySystem *mem)
{
    if (mem->page_table) {
        free(mem->page_table);
        mem->page_table = NULL;
        mem->page_table_size = 0;
        mem->page_table_valid = false;
    }
}

void emu_mem_invalidate_page_table(EmuMemorySystem *mem)
{
    mem->page_table_valid = false;
}
