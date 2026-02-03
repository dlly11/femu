# MPU Module

The Memory Protection Unit (MPU) enforces memory access permissions.

## Purpose

- Define memory regions with access permissions
- Enforce privilege levels (privileged vs unprivileged)
- Generate MemManage faults on violations
- Support PMSAv8 architecture

## API

```c
// include/arch/armv8m/armv8m_mpu.h

/**
 * Create MPU instance.
 */
ARMv8MMPU* armv8m_mpu_create(uint32_t num_regions);

/**
 * Destroy MPU instance.
 */
void armv8m_mpu_destroy(ARMv8MMPU *mpu);

/**
 * Configure MPU region.
 */
int armv8m_mpu_configure_region(ARMv8MMPU *mpu, uint8_t region,
                                 const MPURegionConfig *config);

/**
 * Check memory access.
 *
 * @return 0 if allowed, error code if denied
 */
int armv8m_mpu_check_access(ARMv8MMPU *mpu, uint32_t addr, uint32_t size,
                             bool write, bool privileged);
```

## Key Data Structures

### ARMv8MMPU

```c
typedef struct ARMv8MMPU {
    uint32_t num_regions;       // Number of regions (typically 8 or 16)
    MPURegion *regions;         // Region configurations
    bool enabled;               // MPU enable
    bool hfnmiena;              // Enable during HardFault/NMI
    bool privdefena;            // Enable default memory map for privileged
} ARMv8MMPU;
```

### MPURegion

```c
typedef struct MPURegion {
    bool enabled;
    uint32_t base;              // Base address (aligned)
    uint32_t limit;             // Limit address
    uint8_t attr_index;         // Attribute index (MAIR reference)

    // Access permissions
    bool xn;                    // Execute never
    bool ap_priv_rw;            // Privileged read/write
    bool ap_unpriv_rw;          // Unprivileged read/write
    bool ap_priv_ro;            // Privileged read-only
    bool ap_unpriv_ro;          // Unprivileged read-only
} MPURegion;
```

## Implementation Structure

```text
src/arch/armv8m/mpu/
├── mpu.c              # MPU implementation
└── tests/
    └── test_mpu.cpp
```

## Register Interface

MPU registers at 0xE000ED90:

| Offset | Name      | Description                  |
| ------ | --------- | ---------------------------- |
| 0x00   | MPU_TYPE  | MPU type (read-only)         |
| 0x04   | MPU_CTRL  | Control register             |
| 0x08   | MPU_RNR   | Region number register       |
| 0x0C   | MPU_RBAR  | Region base address          |
| 0x10   | MPU_RLAR  | Region limit and attributes  |

### MPU_CTRL

```text
Bit 2: PRIVDEFENA - Enable default map for privileged access
Bit 1: HFNMIENA   - Enable MPU during HardFault/NMI
Bit 0: ENABLE     - MPU enable
```

### MPU_RBAR (PMSAv8)

```text
Bits 31:5: BASE   - Base address (32-byte aligned)
Bits 4:1:  SH     - Shareability
Bit 0:     XN     - Execute never
```

### MPU_RLAR (PMSAv8)

```text
Bits 31:5: LIMIT  - Limit address
Bits 3:1:  AttrIndx - Attribute index
Bit 0:     EN     - Region enable
```

## Access Checking

```c
int armv8m_mpu_check_access(ARMv8MMPU *mpu, uint32_t addr, uint32_t size,
                             bool write, bool privileged) {
    if (!mpu->enabled) {
        return 0;  // MPU disabled, all access allowed
    }

    // Find matching region (highest numbered match wins)
    MPURegion *match = NULL;
    for (int i = mpu->num_regions - 1; i >= 0; i--) {
        MPURegion *r = &mpu->regions[i];
        if (r->enabled && addr >= r->base && addr + size <= r->limit + 1) {
            match = r;
            break;
        }
    }

    if (!match) {
        // No matching region
        if (mpu->privdefena && privileged) {
            return 0;  // Use default map
        }
        return EMU_ERROR_MPU_FAULT;
    }

    // Check permissions
    if (write) {
        if (privileged && !match->ap_priv_rw) {
            return EMU_ERROR_MPU_FAULT;
        }
        if (!privileged && !match->ap_unpriv_rw) {
            return EMU_ERROR_MPU_FAULT;
        }
    } else {
        if (privileged && !match->ap_priv_rw && !match->ap_priv_ro) {
            return EMU_ERROR_MPU_FAULT;
        }
        if (!privileged && !match->ap_unpriv_rw && !match->ap_unpriv_ro) {
            return EMU_ERROR_MPU_FAULT;
        }
    }

    return 0;
}
```

## Execute Permission

Instruction fetches check XN (Execute Never):

```c
int armv8m_mpu_check_execute(ARMv8MMPU *mpu, uint32_t addr, bool privileged) {
    MPURegion *match = find_region(mpu, addr);

    if (!match) {
        if (mpu->privdefena && privileged) {
            return 0;
        }
        return EMU_ERROR_MPU_FAULT;
    }

    if (match->xn) {
        return EMU_ERROR_MPU_FAULT;
    }

    return 0;
}
```

## PMSAv8 vs PMSAv7 Differences

ARMv8-M uses PMSAv8:

| Feature        | PMSAv7                | PMSAv8                    |
| -------------- | --------------------- | ------------------------- |
| Region size    | Power of 2            | Any (32-byte aligned)     |
| Subregions     | 8 per region          | None                      |
| Overlapping    | Higher region wins    | Same                      |
| Attributes     | TEX/C/B/S             | MAIR index                |

## Memory Attributes

PMSAv8 uses MAIR (Memory Attribute Indirection Register):

```c
typedef enum {
    ATTR_DEVICE_nGnRnE = 0x00,  // Device, non-Gathering, non-Reordering, non-Early
    ATTR_DEVICE_nGnRE  = 0x04,  // Device, non-Gathering, non-Reordering, Early
    ATTR_DEVICE_nGRE   = 0x08,  // Device, non-Gathering, Reordering, Early
    ATTR_DEVICE_GRE    = 0x0C,  // Device, Gathering, Reordering, Early
    ATTR_NORMAL_NC     = 0x44,  // Normal, Non-cacheable
    ATTR_NORMAL_WT     = 0xBB,  // Normal, Write-through
    ATTR_NORMAL_WB     = 0xFF,  // Normal, Write-back
} MemoryAttribute;
```

## Testing

```cpp
TEST(MPU, PrivilegedAccess)
{
    ARMv8MMPU *mpu = armv8m_mpu_create(8);
    mpu->enabled = true;

    // Configure region: 0x20000000-0x2000FFFF, RW privileged, RO unprivileged
    MPURegionConfig config = {
        .base = 0x20000000,
        .limit = 0x2000FFFF,
        .ap_priv_rw = true,
        .ap_unpriv_ro = true,
    };
    armv8m_mpu_configure_region(mpu, 0, &config);

    // Privileged write should succeed
    CHECK_EQUAL(0, armv8m_mpu_check_access(mpu, 0x20000000, 4, true, true));

    // Unprivileged write should fail
    CHECK(armv8m_mpu_check_access(mpu, 0x20000000, 4, true, false) != 0);

    armv8m_mpu_destroy(mpu);
}
```

## See Also

- ARMv8-M Architecture Reference Manual, Chapter B3.5 (MPU)
- [Memory Module](memory.md) - Memory access integration
