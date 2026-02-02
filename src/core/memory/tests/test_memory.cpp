/**
 * @file test_memory.cpp
 * @brief CppUTest tests for the generic memory subsystem
 */

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include <cstring>

extern "C" {
#include "emu/emu_memory.h"
#include "emu/emu_types.h"
}

/*============================================================================
 * Test Group: Memory Initialization
 *============================================================================*/

TEST_GROUP(MemoryInit)
{
    EmuMemorySystem mem;

    void setup()
    {
        emu_mem_init(&mem);
    }

    void teardown()
    {
    }
};

TEST(MemoryInit, InitSetsDefaults)
{
    CHECK_EQUAL(0, mem.num_regions);
}

/*============================================================================
 * Test Group: RAM Operations
 *============================================================================*/

TEST_GROUP(RAMOperations)
{
    EmuMemorySystem mem;
    uint8_t ram[1024];

    void setup()
    {
        memset(ram, 0, sizeof(ram));
        emu_mem_init(&mem);
        emu_mem_add_ram(&mem, 0x20000000, sizeof(ram), ram);
    }

    void teardown()
    {
    }
};

TEST(RAMOperations, WriteAndReadByte)
{
    bool fault = false;

    emu_mem_write(&mem, 0x20000000, 0x42, 1, true, &fault);
    CHECK_FALSE(fault);

    uint64_t value = emu_mem_read(&mem, 0x20000000, 1, true, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x42u, value);
}

TEST(RAMOperations, WriteAndReadHalfword)
{
    bool fault = false;

    emu_mem_write(&mem, 0x20000000, 0x1234, 2, true, &fault);
    CHECK_FALSE(fault);

    uint64_t value = emu_mem_read(&mem, 0x20000000, 2, true, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x1234u, value);
}

TEST(RAMOperations, WriteAndReadWord)
{
    bool fault = false;

    emu_mem_write(&mem, 0x20000000, 0x12345678, 4, true, &fault);
    CHECK_FALSE(fault);

    uint64_t value = emu_mem_read(&mem, 0x20000000, 4, true, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x12345678u, value);
}

TEST(RAMOperations, LittleEndianByteOrder)
{
    bool fault = false;

    // Write a word
    emu_mem_write(&mem, 0x20000000, 0x12345678, 4, true, &fault);

    // Read individual bytes - should be little-endian
    CHECK_EQUAL(0x78u, emu_mem_read(&mem, 0x20000000, 1, true, &fault));
    CHECK_EQUAL(0x56u, emu_mem_read(&mem, 0x20000001, 1, true, &fault));
    CHECK_EQUAL(0x34u, emu_mem_read(&mem, 0x20000002, 1, true, &fault));
    CHECK_EQUAL(0x12u, emu_mem_read(&mem, 0x20000003, 1, true, &fault));
}

/*============================================================================
 * Test Group: ROM Operations
 *============================================================================*/

TEST_GROUP(ROMOperations)
{
    EmuMemorySystem mem;
    uint8_t rom[256];

    void setup()
    {
        // Initialize ROM with test pattern
        for (int i = 0; i < 256; i++) {
            rom[i] = (uint8_t)i;
        }
        emu_mem_init(&mem);
        emu_mem_add_rom(&mem, 0x00000000, sizeof(rom), rom);
    }

    void teardown()
    {
    }
};

TEST(ROMOperations, ReadFromROM)
{
    bool fault = false;

    uint64_t value = emu_mem_read(&mem, 0x00000000, 1, true, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x00u, value);

    value = emu_mem_read(&mem, 0x00000042, 1, true, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x42u, value);
}

TEST(ROMOperations, WriteToROMFaults)
{
    bool fault = false;

    emu_mem_write(&mem, 0x00000000, 0xFF, 1, true, &fault);
    CHECK_TRUE(fault);

    // Original value should be unchanged
    uint64_t value = emu_mem_read(&mem, 0x00000000, 1, true, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x00u, value);
}

/*============================================================================
 * Test Group: MMIO Operations
 *============================================================================*/

static uint64_t mmio_last_read_offset;
static uint64_t mmio_last_write_offset;
static uint64_t mmio_last_write_value;
static uint64_t mmio_read_return;

static uint64_t mock_mmio_read(void *ctx, uint64_t offset, uint8_t size) {
    (void)ctx;
    (void)size;
    mmio_last_read_offset = offset;
    return mmio_read_return;
}

static void mock_mmio_write(void *ctx, uint64_t offset, uint64_t value, uint8_t size) {
    (void)ctx;
    (void)size;
    mmio_last_write_offset = offset;
    mmio_last_write_value = value;
}

TEST_GROUP(MMIOOperations)
{
    EmuMemorySystem mem;

    void setup()
    {
        emu_mem_init(&mem);
        emu_mem_add_mmio(&mem, 0x40000000, 0x1000,
                         NULL, mock_mmio_read, mock_mmio_write);
        mmio_last_read_offset = 0xFFFFFFFFFFFFFFFF;
        mmio_last_write_offset = 0xFFFFFFFFFFFFFFFF;
        mmio_last_write_value = 0;
        mmio_read_return = 0;
    }

    void teardown()
    {
    }
};

TEST(MMIOOperations, ReadInvokesCallback)
{
    bool fault = false;
    mmio_read_return = 0xDEADBEEF;

    uint64_t value = emu_mem_read(&mem, 0x40000100, 4, true, &fault);

    CHECK_FALSE(fault);
    CHECK_EQUAL(0x100u, mmio_last_read_offset);
    CHECK_EQUAL(0xDEADBEEFu, value);
}

TEST(MMIOOperations, WriteInvokesCallback)
{
    bool fault = false;

    emu_mem_write(&mem, 0x40000200, 0xCAFEBABE, 4, true, &fault);

    CHECK_FALSE(fault);
    CHECK_EQUAL(0x200u, mmio_last_write_offset);
    CHECK_EQUAL(0xCAFEBABEu, mmio_last_write_value);
}

/*============================================================================
 * Test Group: Unmapped Memory
 *============================================================================*/

TEST_GROUP(UnmappedMemory)
{
    EmuMemorySystem mem;

    void setup()
    {
        emu_mem_init(&mem);
    }

    void teardown()
    {
    }
};

TEST(UnmappedMemory, ReadFaults)
{
    bool fault = false;

    emu_mem_read(&mem, 0x12345678, 4, true, &fault);
    CHECK_TRUE(fault);
}

TEST(UnmappedMemory, WriteFaults)
{
    bool fault = false;

    emu_mem_write(&mem, 0x12345678, 0, 4, true, &fault);
    CHECK_TRUE(fault);
}

/*============================================================================
 * Test Group: Memory Load
 *============================================================================*/

TEST_GROUP(MemoryLoad)
{
    EmuMemorySystem mem;
    uint8_t ram[1024];

    void setup()
    {
        memset(ram, 0, sizeof(ram));
        emu_mem_init(&mem);
        emu_mem_add_ram(&mem, 0x20000000, sizeof(ram), ram);
    }

    void teardown()
    {
    }
};

TEST(MemoryLoad, LoadData)
{
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};

    int result = emu_mem_load(&mem, 0x20000100, data, sizeof(data));
    CHECK_EQUAL(EMU_OK, result);

    bool fault = false;
    CHECK_EQUAL(0x04030201u, emu_mem_read(&mem, 0x20000100, 4, true, &fault));
}

/*============================================================================
 * Test Group: Memory Pointer Access
 *============================================================================*/

TEST_GROUP(MemoryPointer)
{
    EmuMemorySystem mem;
    uint8_t ram[256];
    uint8_t rom[256];

    void setup()
    {
        memset(ram, 0xAA, sizeof(ram));
        memset(rom, 0xBB, sizeof(rom));
        emu_mem_init(&mem);
        emu_mem_add_ram(&mem, 0x20000000, sizeof(ram), ram);
        emu_mem_add_rom(&mem, 0x00000000, sizeof(rom), rom);
    }

    void teardown()
    {
    }
};

TEST(MemoryPointer, GetPtrFromRAM)
{
    const uint8_t *ptr = emu_mem_get_ptr(&mem, 0x20000000, 16);
    CHECK(ptr != NULL);
    CHECK_EQUAL(0xAAu, ptr[0]);
}

TEST(MemoryPointer, GetPtrFromROM)
{
    const uint8_t *ptr = emu_mem_get_ptr(&mem, 0x00000000, 16);
    CHECK(ptr != NULL);
    CHECK_EQUAL(0xBBu, ptr[0]);
}

TEST(MemoryPointer, GetPtrUnmappedReturnsNull)
{
    const uint8_t *ptr = emu_mem_get_ptr(&mem, 0x80000000, 16);
    CHECK(ptr == NULL);
}

TEST(MemoryPointer, GetPtrBeyondRegionReturnsNull)
{
    // Request size that extends beyond region
    const uint8_t *ptr = emu_mem_get_ptr(&mem, 0x20000000, 512);
    CHECK(ptr == NULL);
}

TEST(MemoryPointer, GetPtrFromMMIOReturnsNull)
{
    emu_mem_add_mmio(&mem, 0x40000000, 0x1000, NULL, NULL, NULL);
    const uint8_t *ptr = emu_mem_get_ptr(&mem, 0x40000000, 16);
    CHECK(ptr == NULL);
}

/*============================================================================
 * Test Group: Find Region
 *============================================================================*/

TEST_GROUP(FindRegion)
{
    EmuMemorySystem mem;
    uint8_t ram[256];

    void setup()
    {
        emu_mem_init(&mem);
        emu_mem_add_ram(&mem, 0x20000000, sizeof(ram), ram);
    }

    void teardown()
    {
    }
};

TEST(FindRegion, FindExistingRegion)
{
    const EmuMemRegion *r = emu_mem_find_region(&mem, 0x20000080);
    CHECK(r != NULL);
    CHECK_EQUAL(0x20000000u, r->base);
    CHECK_EQUAL(256u, r->size);
}

TEST(FindRegion, FindNonExistentRegionReturnsNull)
{
    const EmuMemRegion *r = emu_mem_find_region(&mem, 0x80000000);
    CHECK(r == NULL);
}

TEST(FindRegion, FindAddressBelowRegionReturnsNull)
{
    // Address below the region base
    const EmuMemRegion *r = emu_mem_find_region(&mem, 0x10000000);
    CHECK(r == NULL);
}

/*============================================================================
 * Test Group: MPU Callbacks
 *============================================================================*/

static bool mpu_allow_access;
static bool mpu_check_called;
static uint64_t mpu_last_addr;
static bool mpu_last_is_write;

static bool mock_mpu_check(void *ctx, uint64_t addr, uint64_t size,
                           bool is_write, bool privileged)
{
    (void)ctx;
    (void)size;
    (void)privileged;
    mpu_check_called = true;
    mpu_last_addr = addr;
    mpu_last_is_write = is_write;
    return mpu_allow_access;
}

TEST_GROUP(MPUCallbacks)
{
    EmuMemorySystem mem;
    uint8_t ram[256];

    void setup()
    {
        memset(ram, 0, sizeof(ram));
        emu_mem_init(&mem);
        emu_mem_add_ram(&mem, 0x20000000, sizeof(ram), ram);
        emu_mem_set_mpu(&mem, NULL, mock_mpu_check);
        mpu_allow_access = true;
        mpu_check_called = false;
        mpu_last_addr = 0;
        mpu_last_is_write = false;
    }

    void teardown()
    {
    }
};

TEST(MPUCallbacks, MPUAllowsRead)
{
    bool fault = false;
    mpu_allow_access = true;

    emu_mem_read(&mem, 0x20000000, 4, true, &fault);

    CHECK_TRUE(mpu_check_called);
    CHECK_FALSE(fault);
}

TEST(MPUCallbacks, MPUDeniesRead)
{
    bool fault = false;
    mpu_allow_access = false;

    emu_mem_read(&mem, 0x20000000, 4, true, &fault);

    CHECK_TRUE(mpu_check_called);
    CHECK_TRUE(fault);
}

TEST(MPUCallbacks, MPUAllowsWrite)
{
    bool fault = false;
    mpu_allow_access = true;

    emu_mem_write(&mem, 0x20000000, 0x12345678, 4, true, &fault);

    CHECK_TRUE(mpu_check_called);
    CHECK_FALSE(fault);
}

TEST(MPUCallbacks, MPUDeniesWrite)
{
    bool fault = false;
    mpu_allow_access = false;

    emu_mem_write(&mem, 0x20000000, 0x12345678, 4, true, &fault);

    CHECK_TRUE(mpu_check_called);
    CHECK_TRUE(fault);
}

/*============================================================================
 * Test Group: Fault Callbacks
 *============================================================================*/

static bool fault_callback_called;
static uint64_t fault_last_addr;
static bool fault_last_is_write;
static int fault_last_type;

static void mock_fault_callback(void *ctx, uint64_t addr, bool is_write, int fault_type)
{
    (void)ctx;
    fault_callback_called = true;
    fault_last_addr = addr;
    fault_last_is_write = is_write;
    fault_last_type = fault_type;
}

TEST_GROUP(FaultCallbacks)
{
    EmuMemorySystem mem;
    uint8_t rom[256];

    void setup()
    {
        memset(rom, 0, sizeof(rom));
        emu_mem_init(&mem);
        emu_mem_add_rom(&mem, 0x00000000, sizeof(rom), rom);
        emu_mem_set_fault_callback(&mem, NULL, mock_fault_callback);
        fault_callback_called = false;
        fault_last_addr = 0;
        fault_last_is_write = false;
        fault_last_type = 0;
    }

    void teardown()
    {
    }
};

TEST(FaultCallbacks, FaultCallbackOnROMWrite)
{
    bool fault = false;

    emu_mem_write(&mem, 0x00000010, 0xFF, 1, true, &fault);

    CHECK_TRUE(fault);
    CHECK_TRUE(fault_callback_called);
    CHECK_EQUAL(0x00000010u, fault_last_addr);
    CHECK_TRUE(fault_last_is_write);
}

TEST(FaultCallbacks, FaultCallbackOnUnmappedRead)
{
    bool fault = false;

    emu_mem_read(&mem, 0x80000000, 4, true, &fault);

    CHECK_TRUE(fault);
    CHECK_TRUE(fault_callback_called);
    CHECK_EQUAL(0x80000000u, fault_last_addr);
    CHECK_FALSE(fault_last_is_write);
}

/*============================================================================
 * Test Group: Boundary Access
 *============================================================================*/

TEST_GROUP(BoundaryAccess)
{
    EmuMemorySystem mem;
    uint8_t ram[256];

    void setup()
    {
        memset(ram, 0, sizeof(ram));
        emu_mem_init(&mem);
        emu_mem_add_ram(&mem, 0x20000000, sizeof(ram), ram);
    }

    void teardown()
    {
    }
};

TEST(BoundaryAccess, ReadSpanningBoundaryFaults)
{
    bool fault = false;

    // Try to read 4 bytes starting at last byte of region
    emu_mem_read(&mem, 0x200000FF, 4, true, &fault);

    CHECK_TRUE(fault);
}

TEST(BoundaryAccess, WriteSpanningBoundaryFaults)
{
    bool fault = false;

    // Try to write 4 bytes starting at last byte of region
    emu_mem_write(&mem, 0x200000FF, 0x12345678, 4, true, &fault);

    CHECK_TRUE(fault);
}

/*============================================================================
 * Test Group: MMIO Edge Cases
 *============================================================================*/

TEST_GROUP(MMIOEdgeCases)
{
    EmuMemorySystem mem;

    void setup()
    {
        emu_mem_init(&mem);
    }

    void teardown()
    {
    }
};

TEST(MMIOEdgeCases, ReadWithNullCallbackFaults)
{
    // Add MMIO region with NULL read callback
    emu_mem_add_mmio(&mem, 0x40000000, 0x1000, NULL, NULL, mock_mmio_write);

    bool fault = false;
    emu_mem_read(&mem, 0x40000000, 4, true, &fault);

    CHECK_TRUE(fault);
}

TEST(MMIOEdgeCases, WriteWithNullCallbackFaults)
{
    // Add MMIO region with NULL write callback
    emu_mem_add_mmio(&mem, 0x40000000, 0x1000, NULL, mock_mmio_read, NULL);

    bool fault = false;
    emu_mem_write(&mem, 0x40000000, 0x12345678, 4, true, &fault);

    CHECK_TRUE(fault);
}

/*============================================================================
 * Test Group: Memory Load Edge Cases
 *============================================================================*/

TEST_GROUP(MemoryLoadEdgeCases)
{
    EmuMemorySystem mem;
    uint8_t ram[256];
    uint8_t rom[256];

    void setup()
    {
        memset(ram, 0, sizeof(ram));
        memset(rom, 0, sizeof(rom));
        emu_mem_init(&mem);
        emu_mem_add_ram(&mem, 0x20000000, sizeof(ram), ram);
        emu_mem_add_rom(&mem, 0x00000000, sizeof(rom), rom);
    }

    void teardown()
    {
    }
};

TEST(MemoryLoadEdgeCases, LoadToUnmappedFails)
{
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    int result = emu_mem_load(&mem, 0x80000000, data, sizeof(data));
    CHECK_EQUAL(EMU_ERR_BUS_FAULT, result);
}

TEST(MemoryLoadEdgeCases, LoadToROMFails)
{
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    int result = emu_mem_load(&mem, 0x00000000, data, sizeof(data));
    CHECK_EQUAL(EMU_ERR_MEM_FAULT, result);
}

TEST(MemoryLoadEdgeCases, LoadSpanningBoundaryFails)
{
    uint8_t data[512];
    memset(data, 0xAA, sizeof(data));
    // Try to load 512 bytes into 256 byte region
    int result = emu_mem_load(&mem, 0x20000000, data, sizeof(data));
    CHECK_EQUAL(EMU_ERR_BUS_FAULT, result);
}

/*============================================================================
 * Test Group: Region Management
 *============================================================================*/

TEST_GROUP(RegionManagement)
{
    EmuMemorySystem mem;

    void setup()
    {
        emu_mem_init(&mem);
    }

    void teardown()
    {
    }
};

TEST(RegionManagement, AddMaxRegions)
{
    uint8_t ram[16];
    int result = EMU_OK;

    // Add maximum number of regions
    for (int i = 0; i < EMU_MEM_MAX_REGIONS; i++) {
        result = emu_mem_add_ram(&mem, (uint64_t)(0x20000000 + i * 0x1000), sizeof(ram), ram);
        CHECK_EQUAL(EMU_OK, result);
    }

    CHECK_EQUAL(EMU_MEM_MAX_REGIONS, mem.num_regions);

    // Try to add one more - should fail
    result = emu_mem_add_ram(&mem, 0x30000000, sizeof(ram), ram);
    CHECK_EQUAL(EMU_ERR_MEM_FAULT, result);
}

TEST(RegionManagement, AddRegionDirectly)
{
    EmuMemRegion region;
    memset(&region, 0, sizeof(region));
    region.base = 0x50000000;
    region.size = 0x1000;
    region.type = EMU_MEM_REGION_UNMAPPED;

    int result = emu_mem_add_region(&mem, &region);
    CHECK_EQUAL(EMU_OK, result);
    CHECK_EQUAL(1, mem.num_regions);
}

/*============================================================================
 * Test Group: UNMAPPED Region Type
 *============================================================================*/

TEST_GROUP(UnmappedRegionType)
{
    EmuMemorySystem mem;

    void setup()
    {
        emu_mem_init(&mem);
        // Add an explicit UNMAPPED region
        EmuMemRegion region;
        memset(&region, 0, sizeof(region));
        region.base = 0x50000000;
        region.size = 0x1000;
        region.type = EMU_MEM_REGION_UNMAPPED;
        emu_mem_add_region(&mem, &region);
    }

    void teardown()
    {
    }
};

TEST(UnmappedRegionType, ReadFromUnmappedRegionFaults)
{
    bool fault = false;
    emu_mem_read(&mem, 0x50000000, 4, true, &fault);
    CHECK_TRUE(fault);
}

TEST(UnmappedRegionType, WriteToUnmappedRegionFaults)
{
    bool fault = false;
    emu_mem_write(&mem, 0x50000000, 0x12345678, 4, true, &fault);
    CHECK_TRUE(fault);
}

/*============================================================================
 * Test Group: Invalid Access Size
 *============================================================================*/

TEST_GROUP(InvalidAccessSize)
{
    EmuMemorySystem mem;
    uint8_t ram[256];

    void setup()
    {
        memset(ram, 0xAA, sizeof(ram));
        emu_mem_init(&mem);
        emu_mem_add_ram(&mem, 0x20000000, sizeof(ram), ram);
    }

    void teardown()
    {
    }
};

TEST(InvalidAccessSize, ReadWithInvalidSizeFaults)
{
    bool fault = false;
    // Size 3 is invalid - should fault
    emu_mem_read(&mem, 0x20000000, 3, true, &fault);
    CHECK_TRUE(fault);
}

TEST(InvalidAccessSize, WriteWithInvalidSizeFaults)
{
    bool fault = false;
    // Size 3 is invalid - should fault
    emu_mem_write(&mem, 0x20000000, 0x12345678, 3, true, &fault);
    CHECK_TRUE(fault);
    // Memory should be unchanged (still 0xAA pattern)
    CHECK_EQUAL(0xAAu, emu_mem_read(&mem, 0x20000000, 1, true, &fault));
}

TEST(InvalidAccessSize, ReadWithZeroSizeFaults)
{
    bool fault = false;
    emu_mem_read(&mem, 0x20000000, 0, true, &fault);
    CHECK_TRUE(fault);
}

TEST(InvalidAccessSize, WriteWithZeroSizeFaults)
{
    bool fault = false;
    emu_mem_write(&mem, 0x20000000, 0x12345678, 0, true, &fault);
    CHECK_TRUE(fault);
}

/*============================================================================
 * Test Group: Alignment Checks (MMIO)
 *============================================================================*/

static uint64_t device_reg_value;

static uint64_t device_read(void *ctx, uint64_t offset, uint8_t size) {
    (void)ctx;
    (void)offset;
    (void)size;
    return device_reg_value;
}

static void device_write(void *ctx, uint64_t offset, uint64_t value, uint8_t size) {
    (void)ctx;
    (void)offset;
    (void)size;
    device_reg_value = value;
}

TEST_GROUP(DeviceMemoryAlignment)
{
    EmuMemorySystem mem;

    void setup()
    {
        emu_mem_init(&mem);
        // MMIO regions require aligned access
        emu_mem_add_mmio(&mem, 0x40000000, 0x1000, NULL, device_read, device_write);
        device_reg_value = 0;
    }

    void teardown()
    {
    }
};

TEST(DeviceMemoryAlignment, AlignedAccessToDeviceMemorySucceeds)
{
    bool fault = false;
    emu_mem_write(&mem, 0x40000000, 0xDEADBEEF, 4, true, &fault);
    CHECK_FALSE(fault);
    uint64_t value = emu_mem_read(&mem, 0x40000000, 4, true, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0xDEADBEEFu, value);
}

TEST(DeviceMemoryAlignment, UnalignedWordReadFromDeviceMemoryFaults)
{
    bool fault = false;
    emu_mem_read(&mem, 0x40000001, 4, true, &fault);
    CHECK_TRUE(fault);
}

TEST(DeviceMemoryAlignment, UnalignedWordWriteToDeviceMemoryFaults)
{
    bool fault = false;
    emu_mem_write(&mem, 0x40000001, 0x12345678, 4, true, &fault);
    CHECK_TRUE(fault);
}

TEST(DeviceMemoryAlignment, UnalignedHalfwordReadFromDeviceMemoryFaults)
{
    bool fault = false;
    emu_mem_read(&mem, 0x40000001, 2, true, &fault);
    CHECK_TRUE(fault);
}

TEST(DeviceMemoryAlignment, UnalignedHalfwordWriteToDeviceMemoryFaults)
{
    bool fault = false;
    emu_mem_write(&mem, 0x40000001, 0x1234, 2, true, &fault);
    CHECK_TRUE(fault);
}

TEST(DeviceMemoryAlignment, ByteAccessAlwaysSucceeds)
{
    bool fault = false;
    // Byte access is always aligned
    emu_mem_write(&mem, 0x40000001, 0x42, 1, true, &fault);
    CHECK_FALSE(fault);
    uint64_t value = emu_mem_read(&mem, 0x40000001, 1, true, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x42u, value);
}

/*============================================================================
 * Test Group: High Address Region (Overflow Protection)
 *============================================================================*/

TEST_GROUP(HighAddressRegion)
{
    EmuMemorySystem mem;
    uint8_t ram[256];

    void setup()
    {
        memset(ram, 0x55, sizeof(ram));
        emu_mem_init(&mem);
        // Region at high address where base + size would overflow uint32_t
        emu_mem_add_ram(&mem, 0xFFFFFF00, sizeof(ram), ram);
    }

    void teardown()
    {
    }
};

TEST(HighAddressRegion, ReadFromHighRegion)
{
    bool fault = false;
    uint64_t value = emu_mem_read(&mem, 0xFFFFFF00, 1, true, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x55u, value);
}

TEST(HighAddressRegion, WriteToHighRegion)
{
    bool fault = false;
    emu_mem_write(&mem, 0xFFFFFF00, 0xAA, 1, true, &fault);
    CHECK_FALSE(fault);

    uint64_t value = emu_mem_read(&mem, 0xFFFFFF00, 1, true, &fault);
    CHECK_EQUAL(0xAAu, value);
}

TEST(HighAddressRegion, AccessAtEndOfHighRegion)
{
    bool fault = false;
    // Last valid byte in region (0xFFFFFF00 + 255 = 0xFFFFFFFF)
    uint64_t value = emu_mem_read(&mem, 0xFFFFFFFF, 1, true, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x55u, value);
}

TEST(HighAddressRegion, FindHighRegion)
{
    const EmuMemRegion *r = emu_mem_find_region(&mem, 0xFFFFFF80);
    CHECK(r != NULL);
    CHECK_EQUAL(0xFFFFFF00u, r->base);
}

TEST(HighAddressRegion, GetPtrFromHighRegion)
{
    const uint8_t *ptr = emu_mem_get_ptr(&mem, 0xFFFFFF00, 128);
    CHECK(ptr != NULL);
    CHECK_EQUAL(0x55u, ptr[0]);
}

TEST(HighAddressRegion, LoadToHighRegion)
{
    uint8_t data[] = {0x11, 0x22, 0x33, 0x44};
    int result = emu_mem_load(&mem, 0xFFFFFF00, data, sizeof(data));
    CHECK_EQUAL(EMU_OK, result);

    bool fault = false;
    CHECK_EQUAL(0x44332211u, emu_mem_read(&mem, 0xFFFFFF00, 4, true, &fault));
}

/*============================================================================
 * Test Runner
 *============================================================================*/

int main(int argc, char** argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
