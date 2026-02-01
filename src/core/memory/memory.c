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

    /* Find the region */
    EmuMemRegion *r = find_region_internal(mem, addr);
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

    /* Find the region */
    EmuMemRegion *r = find_region_internal(mem, addr);
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
    EmuMemRegion *r = find_region_internal(mem, addr);
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
