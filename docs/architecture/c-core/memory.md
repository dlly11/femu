# Memory Module

The memory module provides the memory subsystem for the emulator.

## Purpose

- Manage memory regions (flash, RAM, peripherals)
- Handle memory reads and writes
- Enforce access permissions
- Route peripheral accesses

## API

```c
// include/emu/emu_memory.h

/**
 * Create memory subsystem.
 */
EmuMemory* emu_memory_create(void);

/**
 * Destroy memory subsystem.
 */
void emu_memory_destroy(EmuMemory *mem);

/**
 * Add a memory region.
 */
int emu_memory_add_region(EmuMemory *mem, uint32_t base, uint32_t size,
                          EmuMemoryType type, uint8_t *data);

/**
 * Read from memory.
 */
int emu_memory_read(EmuMemory *mem, uint32_t addr, uint32_t *value, uint8_t size);

/**
 * Write to memory.
 */
int emu_memory_write(EmuMemory *mem, uint32_t addr, uint32_t value, uint8_t size);

/**
 * Add a peripheral.
 */
int emu_memory_add_peripheral(EmuMemory *mem, uint32_t base, uint32_t size,
                               EmuPeripheral *periph);
```

## Key Data Structures

### EmuMemory

```c
typedef struct EmuMemory {
    EmuMemoryRegion *regions;
    size_t num_regions;
    size_t capacity;

    EmuPeripheralEntry *peripherals;
    size_t num_peripherals;
} EmuMemory;
```

### EmuMemoryRegion

```c
typedef struct EmuMemoryRegion {
    uint32_t base;
    uint32_t size;
    EmuMemoryType type;
    uint8_t *data;          // NULL for peripheral regions
    bool read_only;
} EmuMemoryRegion;

typedef enum EmuMemoryType {
    EMU_MEM_RAM,
    EMU_MEM_FLASH,
    EMU_MEM_PERIPHERAL,
} EmuMemoryType;
```

## Implementation Structure

```text
src/core/memory/
├── memory.c           # Main implementation
└── tests/
    └── test_memory.cpp
```

## Memory Map

Default ARMv8-M memory map:

| Region              | Start        | End          | Type       |
| ------------------- | ------------ | ------------ | ---------- |
| Code                | 0x00000000   | 0x1FFFFFFF   | Flash/RAM  |
| SRAM                | 0x20000000   | 0x3FFFFFFF   | RAM        |
| Peripheral          | 0x40000000   | 0x5FFFFFFF   | MMIO       |
| External RAM        | 0x60000000   | 0x9FFFFFFF   | RAM        |
| External Device     | 0xA0000000   | 0xDFFFFFFF   | MMIO       |
| Private Peripheral  | 0xE0000000   | 0xE00FFFFF   | PPB        |
| Vendor Specific     | 0xE0100000   | 0xFFFFFFFF   | Device     |

## Region Lookup

Fast path for common case (sorted regions, binary search):

```c
static EmuMemoryRegion* find_region(EmuMemory *mem, uint32_t addr) {
    // Binary search for region containing addr
    size_t lo = 0, hi = mem->num_regions;

    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        EmuMemoryRegion *r = &mem->regions[mid];

        if (addr < r->base) {
            hi = mid;
        } else if (addr >= r->base + r->size) {
            lo = mid + 1;
        } else {
            return r;
        }
    }

    return NULL;  // No region found
}
```

## Read/Write Operations

### Aligned Access

```c
int emu_memory_read(EmuMemory *mem, uint32_t addr, uint32_t *value, uint8_t size) {
    // Check alignment
    if (addr & (size - 1)) {
        return EMU_ERROR_ALIGNMENT;
    }

    // Find region
    EmuMemoryRegion *region = find_region(mem, addr);
    if (!region) {
        return EMU_ERROR_BUS_FAULT;
    }

    // Check peripheral
    if (region->type == EMU_MEM_PERIPHERAL) {
        EmuPeripheral *periph = find_peripheral(mem, addr);
        if (periph) {
            uint32_t offset = addr - periph->base_addr;
            *value = periph->vtable.read(periph->context, offset, size);
            return 0;
        }
    }

    // Read from backing store
    uint32_t offset = addr - region->base;
    switch (size) {
        case 1: *value = region->data[offset]; break;
        case 2: *value = *(uint16_t*)(region->data + offset); break;
        case 4: *value = *(uint32_t*)(region->data + offset); break;
    }

    return 0;
}
```

### Write Protection

```c
int emu_memory_write(EmuMemory *mem, uint32_t addr, uint32_t value, uint8_t size) {
    EmuMemoryRegion *region = find_region(mem, addr);
    if (!region) {
        return EMU_ERROR_BUS_FAULT;
    }

    // Check write permission
    if (region->read_only) {
        return EMU_ERROR_PERMISSION;
    }

    // ... write logic
}
```

## Peripheral Routing

When an address maps to a peripheral:

```c
static EmuPeripheral* find_peripheral(EmuMemory *mem, uint32_t addr) {
    for (size_t i = 0; i < mem->num_peripherals; i++) {
        EmuPeripheralEntry *entry = &mem->peripherals[i];
        if (addr >= entry->base && addr < entry->base + entry->size) {
            return entry->periph;
        }
    }
    return NULL;
}
```

## Bulk Operations

For ELF loading and DMA:

```c
int emu_memory_write_bytes(EmuMemory *mem, uint32_t addr,
                            const uint8_t *data, size_t len) {
    EmuMemoryRegion *region = find_region(mem, addr);
    if (!region || region->type == EMU_MEM_PERIPHERAL) {
        return EMU_ERROR_BUS_FAULT;
    }

    if (addr + len > region->base + region->size) {
        return EMU_ERROR_BUS_FAULT;  // Crosses region boundary
    }

    uint32_t offset = addr - region->base;
    memcpy(region->data + offset, data, len);
    return 0;
}
```

## Error Handling

Memory errors return codes and can trigger exceptions:

```c
typedef enum {
    EMU_ERROR_OK = 0,
    EMU_ERROR_BUS_FAULT = -1,    // No region at address
    EMU_ERROR_ALIGNMENT = -2,    // Unaligned access
    EMU_ERROR_PERMISSION = -3,   // Write to read-only
} EmuMemoryError;
```

The executor translates these to ARM exceptions:

- BusFault for invalid addresses
- MemManage for MPU violations

## Testing

```cpp
TEST(Memory, ReadWrite)
{
    EmuMemory *mem = emu_memory_create();

    uint8_t ram[1024];
    emu_memory_add_region(mem, 0x20000000, sizeof(ram), EMU_MEM_RAM, ram);

    // Write and read back
    emu_memory_write(mem, 0x20000000, 0x12345678, 4);

    uint32_t value;
    emu_memory_read(mem, 0x20000000, &value, 4);
    CHECK_EQUAL(0x12345678, value);

    emu_memory_destroy(mem);
}
```

## See Also

- [MPU Module](mpu.md) - Memory protection
- [Peripheral Framework](../python-layer/peripheral-framework.md) - Peripheral callbacks
