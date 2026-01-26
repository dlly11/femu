# Memory Module

## Purpose

Manages the memory subsystem: RAM, ROM, and MMIO regions. Dispatches memory accesses to the appropriate backing store or peripheral.

## Interface

See `include/armv8m_memory.h`

## Files to Implement

| File | Description | ~LOC |
|------|-------------|------|
| `memory.c` | Memory system implementation | 400 |

## Dependencies

- `armv8m_types.h` (types only)

## AI Development Notes

### What You Need to Know

1. **Memory Map** (typical Cortex-M33):
   ```
   0x00000000 - 0x1FFFFFFF  Code (ROM/Flash)
   0x20000000 - 0x3FFFFFFF  SRAM
   0x40000000 - 0x5FFFFFFF  Peripherals
   0x60000000 - 0x9FFFFFFF  External RAM
   0xA0000000 - 0xDFFFFFFF  External Device
   0xE0000000 - 0xE00FFFFF  Private Peripheral Bus (PPB)
   0xE0100000 - 0xFFFFFFFF  Vendor-specific
   ```

2. **Access Sizes**:
   - Byte (8-bit), Halfword (16-bit), Word (32-bit)
   - Unaligned access may be supported or fault

3. **Region Types**:
   - **RAM**: Read/write, backed by host memory
   - **ROM**: Read-only, backed by host memory
   - **MMIO**: Read/write via callbacks (peripherals)

4. **Little-Endian**:
   ```c
   // Word read from bytes
   uint32_t word = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
   ```

### What You Do NOT Need

- MPU implementation (optional callback)
- Peripheral implementations (just dispatch via callbacks)
- Cache behavior (not modeled)

### Implementation Tips

```c
// Find region by address
static MemRegion *find_region(MemorySystem *mem, uint32_t addr) {
    for (int i = 0; i < mem->num_regions; i++) {
        MemRegion *r = &mem->regions[i];
        if (addr >= r->base && addr < r->base + r->size) {
            return r;
        }
    }
    return NULL;  // Unmapped
}

// Read from RAM/ROM region
static uint32_t read_ram(MemRegion *r, uint32_t addr, uint8_t size) {
    uint32_t offset = addr - r->base;
    switch (size) {
        case 1: return r->data[offset];
        case 2: return r->data[offset] | (r->data[offset + 1] << 8);
        case 4: return r->data[offset] | (r->data[offset + 1] << 8) |
                       (r->data[offset + 2] << 16) | (r->data[offset + 3] << 24);
    }
    return 0;
}
```

### Test Vectors

```c
// Add 1KB RAM at 0x20000000
// Write 0x12345678 to 0x20000000
// Read back word -> should be 0x12345678
// Read byte at 0x20000000 -> should be 0x78 (little-endian)

// Add ROM at 0x00000000 with data
// Read should work
// Write should fault

// Add MMIO at 0x40000000
// Read/write should invoke callbacks
```

### Edge Cases

1. **Overlapping regions**: Later-added regions should override earlier ones (or reject)
2. **Boundary access**: Access spanning two regions should fault
3. **Alignment**: Misaligned access handling depends on configuration
4. **Null regions**: Access to unmapped memory faults

## Building & Testing

```bash
femu build all
femu test c --filter memory
```

## Checklist

- [ ] `memory.c` - Complete implementation
- [ ] Region management (add, find)
- [ ] RAM read/write
- [ ] ROM read (write faults)
- [ ] MMIO dispatch
- [ ] Proper endianness
- [ ] All tests pass
