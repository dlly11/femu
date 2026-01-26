/**
 * @file memory.c
 * @brief Memory subsystem implementation for ARMv8-M emulator
 *
 * Manages RAM, ROM, and MMIO regions. Dispatches memory accesses to the
 * appropriate backing store or peripheral.
 */

#include "armv8m_memory.h"
#include <string.h>

/*============================================================================
 * Internal Helpers
 *============================================================================*/

/**
 * Find region containing the given address.
 */
static MemRegion *find_region_internal(MemorySystem *mem, uint32_t addr)
{
    for (int i = 0; i < mem->num_regions; i++) {
        MemRegion *r = &mem->regions[i];
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
static uint32_t read_ram(const MemRegion *r, uint32_t addr, uint8_t size)
{
    uint32_t offset = addr - r->base;
    const uint8_t *data = r->data;

    switch (size) {
        case 1:
            return data[offset];
        case 2:
            return (uint32_t)data[offset] |
                   ((uint32_t)data[offset + 1] << 8);
        case 4:
            return (uint32_t)data[offset] |
                   ((uint32_t)data[offset + 1] << 8) |
                   ((uint32_t)data[offset + 2] << 16) |
                   ((uint32_t)data[offset + 3] << 24);
        default:
            return 0;
    }
}

/**
 * Write to RAM region (little-endian).
 */
static void write_ram(const MemRegion *r, uint32_t addr, uint32_t value, uint8_t size)
{
    uint32_t offset = addr - r->base;
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
        default:
            break;
    }
}

/**
 * Report a memory fault.
 */
static void report_fault(MemorySystem *mem, uint32_t addr, bool is_write, int fault_type)
{
    if (mem->on_fault) {
        mem->on_fault(mem->fault_ctx, addr, is_write, fault_type);
    }
}

/**
 * Check if access size is valid (1, 2, or 4 bytes).
 */
static bool is_valid_size(uint8_t size)
{
    return size == 1 || size == 2 || size == 4;
}

/**
 * Check if address is properly aligned for the access size.
 * Returns true if aligned, false if misaligned.
 */
static bool is_aligned(uint32_t addr, uint8_t size)
{
    return (addr & (size - 1)) == 0;
}

/**
 * Check if unaligned access is permitted for the given memory attribute.
 * Normal memory allows unaligned access; Device/Strongly-ordered does not.
 */
static bool allows_unaligned(MemoryAttribute attr)
{
    return attr == MEM_ATTR_NORMAL;
}

/*============================================================================
 * Public API
 *============================================================================*/

void armv8m_mem_init(MemorySystem *mem)
{
    memset(mem, 0, sizeof(*mem));
}

int armv8m_mem_add_region(MemorySystem *mem, const MemRegion *region)
{
    if (mem->num_regions >= MEM_MAX_REGIONS) {
        return ARMV8M_ERR_MEM_FAULT;
    }

    mem->regions[mem->num_regions] = *region;
    mem->num_regions++;

    return ARMV8M_OK;
}

int armv8m_mem_add_ram(MemorySystem *mem, uint32_t base, uint32_t size, uint8_t *data)
{
    MemRegion region = {
        .base = base,
        .size = size,
        .type = MEM_REGION_RAM,
        .attr = MEM_ATTR_NORMAL,
        .data = data,
        .mmio_ctx = NULL,
        .mmio_read = NULL,
        .mmio_write = NULL
    };

    return armv8m_mem_add_region(mem, &region);
}

int armv8m_mem_add_rom(MemorySystem *mem, uint32_t base, uint32_t size, const uint8_t *data)
{
    MemRegion region = {
        .base = base,
        .size = size,
        .type = MEM_REGION_ROM,
        .attr = MEM_ATTR_NORMAL,
        .data = (uint8_t *)data,  /* Cast away const - we won't write to it */
        .mmio_ctx = NULL,
        .mmio_read = NULL,
        .mmio_write = NULL
    };

    return armv8m_mem_add_region(mem, &region);
}

int armv8m_mem_add_mmio(MemorySystem *mem, uint32_t base, uint32_t size,
                        void *ctx,
                        uint32_t (*read_cb)(void *ctx, uint32_t offset, uint8_t size),
                        void (*write_cb)(void *ctx, uint32_t offset, uint32_t value, uint8_t size))
{
    MemRegion region = {
        .base = base,
        .size = size,
        .type = MEM_REGION_MMIO,
        .attr = MEM_ATTR_DEVICE,
        .data = NULL,
        .mmio_ctx = ctx,
        .mmio_read = read_cb,
        .mmio_write = write_cb
    };

    return armv8m_mem_add_region(mem, &region);
}

uint32_t armv8m_mem_read(MemorySystem *mem, uint32_t addr, uint8_t size, bool *fault)
{
    *fault = false;

    /* Validate access size (must be 1, 2, or 4) */
    if (!is_valid_size(size)) {
        *fault = true;
        report_fault(mem, addr, false, ARMV8M_ERR_USAGE_FAULT);
        return 0;
    }

    /* Check MPU if configured */
    if (mem->mpu_check) {
        if (!mem->mpu_check(mem->mpu_ctx, addr, size, false, true)) {
            *fault = true;
            report_fault(mem, addr, false, ARMV8M_ERR_MEM_FAULT);
            return 0;
        }
    }

    /* Find the region */
    MemRegion *r = find_region_internal(mem, addr);
    if (!r) {
        *fault = true;
        report_fault(mem, addr, false, ARMV8M_ERR_BUS_FAULT);
        return 0;
    }

    /* Check alignment - Device/Strongly-ordered memory requires aligned access */
    if (!is_aligned(addr, size) && !allows_unaligned(r->attr)) {
        *fault = true;
        report_fault(mem, addr, false, ARMV8M_ERR_USAGE_FAULT);
        return 0;
    }

    /* Check that the entire access fits within the region */
    /* offset is valid since find_region guarantees addr >= base and addr - base < size */
    uint32_t offset = addr - r->base;
    /* Check: offset + size <= r->size, rearranged to avoid overflow */
    if (r->size - offset < size) {
        *fault = true;
        report_fault(mem, addr, false, ARMV8M_ERR_BUS_FAULT);
        return 0;
    }

    /* Dispatch based on region type */
    switch (r->type) {
        case MEM_REGION_RAM:
        case MEM_REGION_ROM:
            return read_ram(r, addr, size);

        case MEM_REGION_MMIO:
            if (r->mmio_read) {
                return r->mmio_read(r->mmio_ctx, offset, size);
            }
            *fault = true;
            report_fault(mem, addr, false, ARMV8M_ERR_BUS_FAULT);
            return 0;

        case MEM_REGION_UNMAPPED:
        default:
            *fault = true;
            report_fault(mem, addr, false, ARMV8M_ERR_BUS_FAULT);
            return 0;
    }
}

void armv8m_mem_write(MemorySystem *mem, uint32_t addr, uint32_t value, uint8_t size, bool *fault)
{
    *fault = false;

    /* Validate access size (must be 1, 2, or 4) */
    if (!is_valid_size(size)) {
        *fault = true;
        report_fault(mem, addr, true, ARMV8M_ERR_USAGE_FAULT);
        return;
    }

    /* Check MPU if configured */
    if (mem->mpu_check) {
        if (!mem->mpu_check(mem->mpu_ctx, addr, size, true, true)) {
            *fault = true;
            report_fault(mem, addr, true, ARMV8M_ERR_MEM_FAULT);
            return;
        }
    }

    /* Find the region */
    MemRegion *r = find_region_internal(mem, addr);
    if (!r) {
        *fault = true;
        report_fault(mem, addr, true, ARMV8M_ERR_BUS_FAULT);
        return;
    }

    /* Check alignment - Device/Strongly-ordered memory requires aligned access */
    if (!is_aligned(addr, size) && !allows_unaligned(r->attr)) {
        *fault = true;
        report_fault(mem, addr, true, ARMV8M_ERR_USAGE_FAULT);
        return;
    }

    /* Check that the entire access fits within the region */
    uint32_t offset = addr - r->base;
    if (r->size - offset < size) {
        *fault = true;
        report_fault(mem, addr, true, ARMV8M_ERR_BUS_FAULT);
        return;
    }

    /* Dispatch based on region type */
    switch (r->type) {
        case MEM_REGION_RAM:
            write_ram(r, addr, value, size);
            break;

        case MEM_REGION_ROM:
            /* Write to ROM faults */
            *fault = true;
            report_fault(mem, addr, true, ARMV8M_ERR_MEM_FAULT);
            break;

        case MEM_REGION_MMIO:
            if (r->mmio_write) {
                r->mmio_write(r->mmio_ctx, offset, value, size);
            } else {
                *fault = true;
                report_fault(mem, addr, true, ARMV8M_ERR_BUS_FAULT);
            }
            break;

        case MEM_REGION_UNMAPPED:
        default:
            *fault = true;
            report_fault(mem, addr, true, ARMV8M_ERR_BUS_FAULT);
            break;
    }
}

const uint8_t *armv8m_mem_get_ptr(MemorySystem *mem, uint32_t addr, uint32_t size)
{
    MemRegion *r = find_region_internal(mem, addr);
    if (!r) {
        return NULL;
    }

    /* Only RAM and ROM have direct backing storage */
    if (r->type != MEM_REGION_RAM && r->type != MEM_REGION_ROM) {
        return NULL;
    }

    /* Check that requested size fits in region */
    uint32_t offset = addr - r->base;
    if (r->size - offset < size) {
        return NULL;
    }

    return &r->data[offset];
}

int armv8m_mem_load(MemorySystem *mem, uint32_t addr, const uint8_t *data, uint32_t size)
{
    MemRegion *r = find_region_internal(mem, addr);
    if (!r) {
        return ARMV8M_ERR_BUS_FAULT;
    }

    /* Only RAM is writable for load */
    if (r->type != MEM_REGION_RAM) {
        return ARMV8M_ERR_MEM_FAULT;
    }

    /* Check that the entire load fits within the region */
    uint32_t offset = addr - r->base;
    if (r->size - offset < size) {
        return ARMV8M_ERR_BUS_FAULT;
    }

    memcpy(&r->data[offset], data, size);

    return ARMV8M_OK;
}

const MemRegion *armv8m_mem_find_region(const MemorySystem *mem, uint32_t addr)
{
    for (int i = 0; i < mem->num_regions; i++) {
        const MemRegion *r = &mem->regions[i];
        /* Use subtraction to avoid overflow */
        if (addr >= r->base && (addr - r->base) < r->size) {
            return r;
        }
    }
    return NULL;
}
