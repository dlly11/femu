# MPU Module

## Purpose

Implements the Memory Protection Unit (PMSAv8): region configuration, access permission checking, and fault generation.

## Interface

See `include/armv8m_mpu.h`

## Files to Implement

| File | Description | ~LOC |
|------|-------------|------|
| `mpu.c` | MPU implementation | 400 |

## Dependencies

- `armv8m_types.h` (types only)

## AI Development Notes

### What You Need to Know

1. **PMSAv8-M Region Model**:
   - Up to 16 regions (typically 8)
   - Each region: base address, limit address, attributes
   - Regions can overlap (higher number wins)
   - 32-byte minimum granularity

2. **Region Registers** (per region):
   - **RBAR** (Region Base Address):
     - Bits [31:5]: Base address (32-byte aligned)
     - Bits [4:3]: Shareability (SH)
     - Bits [2:1]: Access Permission (AP)
     - Bit [0]: Execute Never (XN)
   - **RLAR** (Region Limit Address):
     - Bits [31:5]: Limit address (inclusive)
     - Bits [3:1]: Attribute Index (into MAIR)
     - Bit [0]: Enable

3. **Access Permissions**:
   | AP | Privileged | Unprivileged |
   |----|------------|--------------|
   | 00 | RW         | None         |
   | 01 | RW         | RW           |
   | 10 | RO         | None         |
   | 11 | RO         | RO           |

4. **Memory Attributes (MAIR)**:
   - 8 attribute slots, 8 bits each
   - Defines cacheability, shareability
   - For emulation: mainly used to distinguish Device vs Normal memory

5. **Default Memory Map** (when PRIVDEFENA=1):
   - Used for privileged access when no region matches
   - Provides basic memory protection

### What You Do NOT Need

- Memory implementation (MPU just checks, doesn't store)
- Cache implementation (attribute bits recorded but not acted on)
- TrustZone SAU (separate unit)

### Implementation Tips

```c
// Check if address is in region
static bool addr_in_region(const MPURegion *r, uint32_t addr) {
    uint32_t base = r->rbar & MPU_RBAR_BASE_MASK;
    uint32_t limit = r->rlar | 0x1F;  // Limit is inclusive, round up
    return addr >= base && addr <= limit;
}

// Find matching region (highest number wins)
static int find_region(const MPU *mpu, uint32_t addr) {
    for (int i = mpu->num_regions - 1; i >= 0; i--) {
        if ((mpu->regions[i].rlar & MPU_RLAR_EN) &&
            addr_in_region(&mpu->regions[i], addr)) {
            return i;
        }
    }
    return -1;  // No match
}

// Check access permission
static bool check_permission(const MPURegion *r, bool is_write,
                             bool is_fetch, bool privileged) {
    int ap = (r->rbar >> MPU_RBAR_AP_SHIFT) & MPU_RBAR_AP_MASK;
    bool xn = r->rbar & MPU_RBAR_XN;

    // Check XN for instruction fetch
    if (is_fetch && xn) return false;

    // Check read/write permission
    switch (ap) {
        case MPU_AP_RW_PRIV:
            return privileged;
        case MPU_AP_RW_ALL:
            return true;
        case MPU_AP_RO_PRIV:
            return privileged && !is_write;
        case MPU_AP_RO_ALL:
            return !is_write;
    }
    return false;
}
```

### Test Vectors

```c
// Configure region 0: 0x20000000-0x20000FFF, RW for all
// Access 0x20000100 privileged write -> should pass
// Access 0x20000100 unprivileged write -> should pass

// Configure region 1: 0x20000000-0x200001FF, RO for all
// Region 1 overlaps region 0 but higher number
// Access 0x20000100 write -> should fail (region 1 wins, RO)

// Configure region with XN
// Instruction fetch -> should fail
```

### Edge Cases

1. **Overlapping regions**: Higher region number takes precedence
2. **Boundary crossing**: Access spanning regions should check both
3. **PRIVDEFENA**: When set, privileged access uses default map if no region matches
4. **HFNMIENA**: When clear, MPU disabled during HardFault/NMI handlers
5. **Disabled MPU**: All accesses permitted when CTRL.ENABLE=0

## Building & Testing

```bash
femu build all
femu test c --filter mpu
```

## Checklist

- [ ] `mpu.c` - Complete implementation
- [ ] Region configuration
- [ ] Access permission checking
- [ ] XN (Execute Never) enforcement
- [ ] Region overlap handling
- [ ] PRIVDEFENA default map
- [ ] Register read/write (MMIO interface)
- [ ] All tests pass
