/**
 * @file test_memory.cpp
 * @brief CppUTest tests for the ARMv8-M memory subsystem
 */

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

extern "C" {
#include "armv8m_memory.h"
#include "armv8m_types.h"
}

/*============================================================================
 * Test Group: Memory Initialization
 *============================================================================*/

TEST_GROUP(MemoryInit)
{
    MemorySystem mem;

    void setup()
    {
        armv8m_mem_init(&mem);
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
    MemorySystem mem;
    uint8_t ram[1024];

    void setup()
    {
        memset(ram, 0, sizeof(ram));
        armv8m_mem_init(&mem);
        armv8m_mem_add_ram(&mem, 0x20000000, sizeof(ram), ram);
    }

    void teardown()
    {
    }
};

TEST(RAMOperations, WriteAndReadByte)
{
    bool fault = false;

    armv8m_mem_write(&mem, 0x20000000, 0x42, 1, &fault);
    CHECK_FALSE(fault);

    uint32_t value = armv8m_mem_read(&mem, 0x20000000, 1, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x42u, value);
}

TEST(RAMOperations, WriteAndReadHalfword)
{
    bool fault = false;

    armv8m_mem_write(&mem, 0x20000000, 0x1234, 2, &fault);
    CHECK_FALSE(fault);

    uint32_t value = armv8m_mem_read(&mem, 0x20000000, 2, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x1234u, value);
}

TEST(RAMOperations, WriteAndReadWord)
{
    bool fault = false;

    armv8m_mem_write(&mem, 0x20000000, 0x12345678, 4, &fault);
    CHECK_FALSE(fault);

    uint32_t value = armv8m_mem_read(&mem, 0x20000000, 4, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x12345678u, value);
}

TEST(RAMOperations, LittleEndianByteOrder)
{
    bool fault = false;

    // Write a word
    armv8m_mem_write(&mem, 0x20000000, 0x12345678, 4, &fault);

    // Read individual bytes - should be little-endian
    CHECK_EQUAL(0x78u, armv8m_mem_read(&mem, 0x20000000, 1, &fault));
    CHECK_EQUAL(0x56u, armv8m_mem_read(&mem, 0x20000001, 1, &fault));
    CHECK_EQUAL(0x34u, armv8m_mem_read(&mem, 0x20000002, 1, &fault));
    CHECK_EQUAL(0x12u, armv8m_mem_read(&mem, 0x20000003, 1, &fault));
}

/*============================================================================
 * Test Group: ROM Operations
 *============================================================================*/

TEST_GROUP(ROMOperations)
{
    MemorySystem mem;
    uint8_t rom[256];

    void setup()
    {
        // Initialize ROM with test pattern
        for (int i = 0; i < 256; i++) {
            rom[i] = (uint8_t)i;
        }
        armv8m_mem_init(&mem);
        armv8m_mem_add_rom(&mem, 0x00000000, sizeof(rom), rom);
    }

    void teardown()
    {
    }
};

TEST(ROMOperations, ReadFromROM)
{
    bool fault = false;

    uint32_t value = armv8m_mem_read(&mem, 0x00000000, 1, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x00u, value);

    value = armv8m_mem_read(&mem, 0x00000042, 1, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x42u, value);
}

TEST(ROMOperations, WriteToROMFaults)
{
    bool fault = false;

    armv8m_mem_write(&mem, 0x00000000, 0xFF, 1, &fault);
    CHECK_TRUE(fault);

    // Original value should be unchanged
    uint32_t value = armv8m_mem_read(&mem, 0x00000000, 1, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x00u, value);
}

/*============================================================================
 * Test Group: MMIO Operations
 *============================================================================*/

static uint32_t mmio_last_read_offset;
static uint32_t mmio_last_write_offset;
static uint32_t mmio_last_write_value;
static uint32_t mmio_read_return;

static uint32_t mock_mmio_read(void *ctx, uint32_t offset, uint8_t size) {
    (void)ctx;
    (void)size;
    mmio_last_read_offset = offset;
    return mmio_read_return;
}

static void mock_mmio_write(void *ctx, uint32_t offset, uint32_t value, uint8_t size) {
    (void)ctx;
    (void)size;
    mmio_last_write_offset = offset;
    mmio_last_write_value = value;
}

TEST_GROUP(MMIOOperations)
{
    MemorySystem mem;

    void setup()
    {
        armv8m_mem_init(&mem);
        armv8m_mem_add_mmio(&mem, 0x40000000, 0x1000,
                           NULL, mock_mmio_read, mock_mmio_write);
        mmio_last_read_offset = 0xFFFFFFFF;
        mmio_last_write_offset = 0xFFFFFFFF;
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

    uint32_t value = armv8m_mem_read(&mem, 0x40000100, 4, &fault);

    CHECK_FALSE(fault);
    CHECK_EQUAL(0x100u, mmio_last_read_offset);
    CHECK_EQUAL(0xDEADBEEFu, value);
}

TEST(MMIOOperations, WriteInvokesCallback)
{
    bool fault = false;

    armv8m_mem_write(&mem, 0x40000200, 0xCAFEBABE, 4, &fault);

    CHECK_FALSE(fault);
    CHECK_EQUAL(0x200u, mmio_last_write_offset);
    CHECK_EQUAL(0xCAFEBABEu, mmio_last_write_value);
}

/*============================================================================
 * Test Group: Unmapped Memory
 *============================================================================*/

TEST_GROUP(UnmappedMemory)
{
    MemorySystem mem;

    void setup()
    {
        armv8m_mem_init(&mem);
    }

    void teardown()
    {
    }
};

TEST(UnmappedMemory, ReadFaults)
{
    bool fault = false;

    armv8m_mem_read(&mem, 0x12345678, 4, &fault);
    CHECK_TRUE(fault);
}

TEST(UnmappedMemory, WriteFaults)
{
    bool fault = false;

    armv8m_mem_write(&mem, 0x12345678, 0, 4, &fault);
    CHECK_TRUE(fault);
}

/*============================================================================
 * Test Group: Memory Load
 *============================================================================*/

TEST_GROUP(MemoryLoad)
{
    MemorySystem mem;
    uint8_t ram[1024];

    void setup()
    {
        memset(ram, 0, sizeof(ram));
        armv8m_mem_init(&mem);
        armv8m_mem_add_ram(&mem, 0x20000000, sizeof(ram), ram);
    }

    void teardown()
    {
    }
};

TEST(MemoryLoad, LoadData)
{
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};

    int result = armv8m_mem_load(&mem, 0x20000100, data, sizeof(data));
    CHECK_EQUAL(ARMV8M_OK, result);

    bool fault = false;
    CHECK_EQUAL(0x04030201u, armv8m_mem_read(&mem, 0x20000100, 4, &fault));
}

/*============================================================================
 * Test Group: Memory Pointer Access
 *============================================================================*/

TEST_GROUP(MemoryPointer)
{
    MemorySystem mem;
    uint8_t ram[256];
    uint8_t rom[256];

    void setup()
    {
        memset(ram, 0xAA, sizeof(ram));
        memset(rom, 0xBB, sizeof(rom));
        armv8m_mem_init(&mem);
        armv8m_mem_add_ram(&mem, 0x20000000, sizeof(ram), ram);
        armv8m_mem_add_rom(&mem, 0x00000000, sizeof(rom), rom);
    }

    void teardown()
    {
    }
};

TEST(MemoryPointer, GetPtrFromRAM)
{
    const uint8_t *ptr = armv8m_mem_get_ptr(&mem, 0x20000000, 16);
    CHECK(ptr != NULL);
    CHECK_EQUAL(0xAAu, ptr[0]);
}

TEST(MemoryPointer, GetPtrFromROM)
{
    const uint8_t *ptr = armv8m_mem_get_ptr(&mem, 0x00000000, 16);
    CHECK(ptr != NULL);
    CHECK_EQUAL(0xBBu, ptr[0]);
}

TEST(MemoryPointer, GetPtrUnmappedReturnsNull)
{
    const uint8_t *ptr = armv8m_mem_get_ptr(&mem, 0x80000000, 16);
    CHECK(ptr == NULL);
}

TEST(MemoryPointer, GetPtrBeyondRegionReturnsNull)
{
    // Request size that extends beyond region
    const uint8_t *ptr = armv8m_mem_get_ptr(&mem, 0x20000000, 512);
    CHECK(ptr == NULL);
}

TEST(MemoryPointer, GetPtrFromMMIOReturnsNull)
{
    armv8m_mem_add_mmio(&mem, 0x40000000, 0x1000, NULL, NULL, NULL);
    const uint8_t *ptr = armv8m_mem_get_ptr(&mem, 0x40000000, 16);
    CHECK(ptr == NULL);
}

/*============================================================================
 * Test Group: Find Region
 *============================================================================*/

TEST_GROUP(FindRegion)
{
    MemorySystem mem;
    uint8_t ram[256];

    void setup()
    {
        armv8m_mem_init(&mem);
        armv8m_mem_add_ram(&mem, 0x20000000, sizeof(ram), ram);
    }

    void teardown()
    {
    }
};

TEST(FindRegion, FindExistingRegion)
{
    const MemRegion *r = armv8m_mem_find_region(&mem, 0x20000080);
    CHECK(r != NULL);
    CHECK_EQUAL(0x20000000u, r->base);
    CHECK_EQUAL(256u, r->size);
}

TEST(FindRegion, FindNonExistentRegionReturnsNull)
{
    const MemRegion *r = armv8m_mem_find_region(&mem, 0x80000000);
    CHECK(r == NULL);
}

TEST(FindRegion, FindAddressBelowRegionReturnsNull)
{
    // Address below the region base
    const MemRegion *r = armv8m_mem_find_region(&mem, 0x10000000);
    CHECK(r == NULL);
}

/*============================================================================
 * Test Group: MPU Callbacks
 *============================================================================*/

static bool mpu_allow_access;
static bool mpu_check_called;
static uint32_t mpu_last_addr;
static bool mpu_last_is_write;

static bool mock_mpu_check(void *ctx, uint32_t addr, uint32_t size, bool is_write, bool privileged)
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
    MemorySystem mem;
    uint8_t ram[256];

    void setup()
    {
        memset(ram, 0, sizeof(ram));
        armv8m_mem_init(&mem);
        armv8m_mem_add_ram(&mem, 0x20000000, sizeof(ram), ram);
        mem.mpu_ctx = NULL;
        mem.mpu_check = mock_mpu_check;
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

    armv8m_mem_read(&mem, 0x20000000, 4, &fault);

    CHECK_TRUE(mpu_check_called);
    CHECK_FALSE(fault);
}

TEST(MPUCallbacks, MPUDeniesRead)
{
    bool fault = false;
    mpu_allow_access = false;

    armv8m_mem_read(&mem, 0x20000000, 4, &fault);

    CHECK_TRUE(mpu_check_called);
    CHECK_TRUE(fault);
}

TEST(MPUCallbacks, MPUAllowsWrite)
{
    bool fault = false;
    mpu_allow_access = true;

    armv8m_mem_write(&mem, 0x20000000, 0x12345678, 4, &fault);

    CHECK_TRUE(mpu_check_called);
    CHECK_FALSE(fault);
}

TEST(MPUCallbacks, MPUDeniesWrite)
{
    bool fault = false;
    mpu_allow_access = false;

    armv8m_mem_write(&mem, 0x20000000, 0x12345678, 4, &fault);

    CHECK_TRUE(mpu_check_called);
    CHECK_TRUE(fault);
}

/*============================================================================
 * Test Group: Fault Callbacks
 *============================================================================*/

static bool fault_callback_called;
static uint32_t fault_last_addr;
static bool fault_last_is_write;
static int fault_last_type;

static void mock_fault_callback(void *ctx, uint32_t addr, bool is_write, int fault_type)
{
    (void)ctx;
    fault_callback_called = true;
    fault_last_addr = addr;
    fault_last_is_write = is_write;
    fault_last_type = fault_type;
}

TEST_GROUP(FaultCallbacks)
{
    MemorySystem mem;
    uint8_t rom[256];

    void setup()
    {
        armv8m_mem_init(&mem);
        armv8m_mem_add_rom(&mem, 0x00000000, sizeof(rom), rom);
        mem.fault_ctx = NULL;
        mem.on_fault = mock_fault_callback;
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

    armv8m_mem_write(&mem, 0x00000010, 0xFF, 1, &fault);

    CHECK_TRUE(fault);
    CHECK_TRUE(fault_callback_called);
    CHECK_EQUAL(0x00000010u, fault_last_addr);
    CHECK_TRUE(fault_last_is_write);
}

TEST(FaultCallbacks, FaultCallbackOnUnmappedRead)
{
    bool fault = false;

    armv8m_mem_read(&mem, 0x80000000, 4, &fault);

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
    MemorySystem mem;
    uint8_t ram[256];

    void setup()
    {
        memset(ram, 0, sizeof(ram));
        armv8m_mem_init(&mem);
        armv8m_mem_add_ram(&mem, 0x20000000, sizeof(ram), ram);
    }

    void teardown()
    {
    }
};

TEST(BoundaryAccess, ReadSpanningBoundaryFaults)
{
    bool fault = false;

    // Try to read 4 bytes starting at last byte of region
    armv8m_mem_read(&mem, 0x200000FF, 4, &fault);

    CHECK_TRUE(fault);
}

TEST(BoundaryAccess, WriteSpanningBoundaryFaults)
{
    bool fault = false;

    // Try to write 4 bytes starting at last byte of region
    armv8m_mem_write(&mem, 0x200000FF, 0x12345678, 4, &fault);

    CHECK_TRUE(fault);
}

/*============================================================================
 * Test Group: MMIO Edge Cases
 *============================================================================*/

TEST_GROUP(MMIOEdgeCases)
{
    MemorySystem mem;

    void setup()
    {
        armv8m_mem_init(&mem);
    }

    void teardown()
    {
    }
};

TEST(MMIOEdgeCases, ReadWithNullCallbackFaults)
{
    // Add MMIO region with NULL read callback
    armv8m_mem_add_mmio(&mem, 0x40000000, 0x1000, NULL, NULL, mock_mmio_write);

    bool fault = false;
    armv8m_mem_read(&mem, 0x40000000, 4, &fault);

    CHECK_TRUE(fault);
}

TEST(MMIOEdgeCases, WriteWithNullCallbackFaults)
{
    // Add MMIO region with NULL write callback
    armv8m_mem_add_mmio(&mem, 0x40000000, 0x1000, NULL, mock_mmio_read, NULL);

    bool fault = false;
    armv8m_mem_write(&mem, 0x40000000, 0x12345678, 4, &fault);

    CHECK_TRUE(fault);
}

/*============================================================================
 * Test Group: Memory Load Edge Cases
 *============================================================================*/

TEST_GROUP(MemoryLoadEdgeCases)
{
    MemorySystem mem;
    uint8_t ram[256];
    uint8_t rom[256];

    void setup()
    {
        memset(ram, 0, sizeof(ram));
        memset(rom, 0, sizeof(rom));
        armv8m_mem_init(&mem);
        armv8m_mem_add_ram(&mem, 0x20000000, sizeof(ram), ram);
        armv8m_mem_add_rom(&mem, 0x00000000, sizeof(rom), rom);
    }

    void teardown()
    {
    }
};

TEST(MemoryLoadEdgeCases, LoadToUnmappedFails)
{
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    int result = armv8m_mem_load(&mem, 0x80000000, data, sizeof(data));
    CHECK_EQUAL(ARMV8M_ERR_BUS_FAULT, result);
}

TEST(MemoryLoadEdgeCases, LoadToROMFails)
{
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    int result = armv8m_mem_load(&mem, 0x00000000, data, sizeof(data));
    CHECK_EQUAL(ARMV8M_ERR_MEM_FAULT, result);
}

TEST(MemoryLoadEdgeCases, LoadSpanningBoundaryFails)
{
    uint8_t data[512];
    memset(data, 0xAA, sizeof(data));
    // Try to load 512 bytes into 256 byte region
    int result = armv8m_mem_load(&mem, 0x20000000, data, sizeof(data));
    CHECK_EQUAL(ARMV8M_ERR_BUS_FAULT, result);
}

/*============================================================================
 * Test Group: Region Management
 *============================================================================*/

TEST_GROUP(RegionManagement)
{
    MemorySystem mem;

    void setup()
    {
        armv8m_mem_init(&mem);
    }

    void teardown()
    {
    }
};

TEST(RegionManagement, AddMaxRegions)
{
    uint8_t ram[16];
    int result = ARMV8M_OK;

    // Add maximum number of regions
    for (int i = 0; i < MEM_MAX_REGIONS; i++) {
        result = armv8m_mem_add_ram(&mem, (uint32_t)(0x20000000 + i * 0x1000), sizeof(ram), ram);
        CHECK_EQUAL(ARMV8M_OK, result);
    }

    CHECK_EQUAL(MEM_MAX_REGIONS, mem.num_regions);

    // Try to add one more - should fail
    result = armv8m_mem_add_ram(&mem, 0x30000000, sizeof(ram), ram);
    CHECK_EQUAL(ARMV8M_ERR_MEM_FAULT, result);
}

TEST(RegionManagement, AddRegionDirectly)
{
    MemRegion region;
    memset(&region, 0, sizeof(region));
    region.base = 0x50000000;
    region.size = 0x1000;
    region.type = MEM_REGION_UNMAPPED;
    region.attr = MEM_ATTR_NORMAL;

    int result = armv8m_mem_add_region(&mem, &region);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(1, mem.num_regions);
}

/*============================================================================
 * Test Group: UNMAPPED Region Type
 *============================================================================*/

TEST_GROUP(UnmappedRegionType)
{
    MemorySystem mem;

    void setup()
    {
        armv8m_mem_init(&mem);
        // Add an explicit UNMAPPED region
        MemRegion region;
        memset(&region, 0, sizeof(region));
        region.base = 0x50000000;
        region.size = 0x1000;
        region.type = MEM_REGION_UNMAPPED;
        region.attr = MEM_ATTR_NORMAL;
        armv8m_mem_add_region(&mem, &region);
    }

    void teardown()
    {
    }
};

TEST(UnmappedRegionType, ReadFromUnmappedRegionFaults)
{
    bool fault = false;
    armv8m_mem_read(&mem, 0x50000000, 4, &fault);
    CHECK_TRUE(fault);
}

TEST(UnmappedRegionType, WriteToUnmappedRegionFaults)
{
    bool fault = false;
    armv8m_mem_write(&mem, 0x50000000, 0x12345678, 4, &fault);
    CHECK_TRUE(fault);
}

/*============================================================================
 * Test Group: Invalid Access Size
 *============================================================================*/

TEST_GROUP(InvalidAccessSize)
{
    MemorySystem mem;
    uint8_t ram[256];

    void setup()
    {
        memset(ram, 0xAA, sizeof(ram));
        armv8m_mem_init(&mem);
        armv8m_mem_add_ram(&mem, 0x20000000, sizeof(ram), ram);
    }

    void teardown()
    {
    }
};

TEST(InvalidAccessSize, ReadWithInvalidSizeFaults)
{
    bool fault = false;
    // Size 3 is invalid - should fault
    armv8m_mem_read(&mem, 0x20000000, 3, &fault);
    CHECK_TRUE(fault);
}

TEST(InvalidAccessSize, WriteWithInvalidSizeFaults)
{
    bool fault = false;
    // Size 3 is invalid - should fault
    armv8m_mem_write(&mem, 0x20000000, 0x12345678, 3, &fault);
    CHECK_TRUE(fault);
    // Memory should be unchanged (still 0xAA pattern)
    CHECK_EQUAL(0xAAu, armv8m_mem_read(&mem, 0x20000000, 1, &fault));
}

TEST(InvalidAccessSize, ReadWithZeroSizeFaults)
{
    bool fault = false;
    armv8m_mem_read(&mem, 0x20000000, 0, &fault);
    CHECK_TRUE(fault);
}

TEST(InvalidAccessSize, WriteWithZeroSizeFaults)
{
    bool fault = false;
    armv8m_mem_write(&mem, 0x20000000, 0x12345678, 0, &fault);
    CHECK_TRUE(fault);
}

/*============================================================================
 * Test Group: Alignment Checks
 *============================================================================*/

TEST_GROUP(AlignmentChecks)
{
    MemorySystem mem;
    uint8_t ram[256];

    void setup()
    {
        memset(ram, 0, sizeof(ram));
        armv8m_mem_init(&mem);
        armv8m_mem_add_ram(&mem, 0x20000000, sizeof(ram), ram);
    }

    void teardown()
    {
    }
};

TEST(AlignmentChecks, AlignedWordAccessSucceeds)
{
    bool fault = false;
    armv8m_mem_write(&mem, 0x20000000, 0x12345678, 4, &fault);
    CHECK_FALSE(fault);
    uint32_t value = armv8m_mem_read(&mem, 0x20000000, 4, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x12345678u, value);
}

TEST(AlignmentChecks, AlignedHalfwordAccessSucceeds)
{
    bool fault = false;
    armv8m_mem_write(&mem, 0x20000002, 0x1234, 2, &fault);
    CHECK_FALSE(fault);
    uint32_t value = armv8m_mem_read(&mem, 0x20000002, 2, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x1234u, value);
}

TEST(AlignmentChecks, UnalignedWordAccessOnNormalMemorySucceeds)
{
    bool fault = false;
    // Normal memory (RAM) allows unaligned access
    armv8m_mem_write(&mem, 0x20000001, 0x12345678, 4, &fault);
    CHECK_FALSE(fault);
    uint32_t value = armv8m_mem_read(&mem, 0x20000001, 4, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x12345678u, value);
}

TEST(AlignmentChecks, UnalignedHalfwordAccessOnNormalMemorySucceeds)
{
    bool fault = false;
    // Normal memory (RAM) allows unaligned access
    armv8m_mem_write(&mem, 0x20000001, 0x1234, 2, &fault);
    CHECK_FALSE(fault);
    uint32_t value = armv8m_mem_read(&mem, 0x20000001, 2, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x1234u, value);
}

/*============================================================================
 * Test Group: Device Memory Alignment
 *============================================================================*/

static uint32_t device_reg_value;

static uint32_t device_read(void *ctx, uint32_t offset, uint8_t size) {
    (void)ctx;
    (void)offset;
    (void)size;
    return device_reg_value;
}

static void device_write(void *ctx, uint32_t offset, uint32_t value, uint8_t size) {
    (void)ctx;
    (void)offset;
    (void)size;
    device_reg_value = value;
}

TEST_GROUP(DeviceMemoryAlignment)
{
    MemorySystem mem;

    void setup()
    {
        armv8m_mem_init(&mem);
        // MMIO regions have MEM_ATTR_DEVICE by default
        armv8m_mem_add_mmio(&mem, 0x40000000, 0x1000, NULL, device_read, device_write);
        device_reg_value = 0;
    }

    void teardown()
    {
    }
};

TEST(DeviceMemoryAlignment, AlignedAccessToDeviceMemorySucceeds)
{
    bool fault = false;
    armv8m_mem_write(&mem, 0x40000000, 0xDEADBEEF, 4, &fault);
    CHECK_FALSE(fault);
    uint32_t value = armv8m_mem_read(&mem, 0x40000000, 4, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0xDEADBEEFu, value);
}

TEST(DeviceMemoryAlignment, UnalignedWordReadFromDeviceMemoryFaults)
{
    bool fault = false;
    armv8m_mem_read(&mem, 0x40000001, 4, &fault);
    CHECK_TRUE(fault);
}

TEST(DeviceMemoryAlignment, UnalignedWordWriteToDeviceMemoryFaults)
{
    bool fault = false;
    armv8m_mem_write(&mem, 0x40000001, 0x12345678, 4, &fault);
    CHECK_TRUE(fault);
}

TEST(DeviceMemoryAlignment, UnalignedHalfwordReadFromDeviceMemoryFaults)
{
    bool fault = false;
    armv8m_mem_read(&mem, 0x40000001, 2, &fault);
    CHECK_TRUE(fault);
}

TEST(DeviceMemoryAlignment, UnalignedHalfwordWriteToDeviceMemoryFaults)
{
    bool fault = false;
    armv8m_mem_write(&mem, 0x40000001, 0x1234, 2, &fault);
    CHECK_TRUE(fault);
}

TEST(DeviceMemoryAlignment, ByteAccessAlwaysSucceeds)
{
    bool fault = false;
    // Byte access is always aligned
    armv8m_mem_write(&mem, 0x40000001, 0x42, 1, &fault);
    CHECK_FALSE(fault);
    uint32_t value = armv8m_mem_read(&mem, 0x40000001, 1, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x42u, value);
}

/*============================================================================
 * Test Group: High Address Region (Overflow Protection)
 *============================================================================*/

TEST_GROUP(HighAddressRegion)
{
    MemorySystem mem;
    uint8_t ram[256];

    void setup()
    {
        memset(ram, 0x55, sizeof(ram));
        armv8m_mem_init(&mem);
        // Region at high address where base + size would overflow uint32_t
        armv8m_mem_add_ram(&mem, 0xFFFFFF00, sizeof(ram), ram);
    }

    void teardown()
    {
    }
};

TEST(HighAddressRegion, ReadFromHighRegion)
{
    bool fault = false;
    uint32_t value = armv8m_mem_read(&mem, 0xFFFFFF00, 1, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x55u, value);
}

TEST(HighAddressRegion, WriteToHighRegion)
{
    bool fault = false;
    armv8m_mem_write(&mem, 0xFFFFFF00, 0xAA, 1, &fault);
    CHECK_FALSE(fault);

    uint32_t value = armv8m_mem_read(&mem, 0xFFFFFF00, 1, &fault);
    CHECK_EQUAL(0xAAu, value);
}

TEST(HighAddressRegion, AccessAtEndOfHighRegion)
{
    bool fault = false;
    // Last valid byte in region (0xFFFFFF00 + 255 = 0xFFFFFFFF)
    uint32_t value = armv8m_mem_read(&mem, 0xFFFFFFFF, 1, &fault);
    CHECK_FALSE(fault);
    CHECK_EQUAL(0x55u, value);
}

TEST(HighAddressRegion, FindHighRegion)
{
    const MemRegion *r = armv8m_mem_find_region(&mem, 0xFFFFFF80);
    CHECK(r != NULL);
    CHECK_EQUAL(0xFFFFFF00u, r->base);
}

TEST(HighAddressRegion, GetPtrFromHighRegion)
{
    const uint8_t *ptr = armv8m_mem_get_ptr(&mem, 0xFFFFFF00, 128);
    CHECK(ptr != NULL);
    CHECK_EQUAL(0x55u, ptr[0]);
}

TEST(HighAddressRegion, LoadToHighRegion)
{
    uint8_t data[] = {0x11, 0x22, 0x33, 0x44};
    int result = armv8m_mem_load(&mem, 0xFFFFFF00, data, sizeof(data));
    CHECK_EQUAL(ARMV8M_OK, result);

    bool fault = false;
    CHECK_EQUAL(0x44332211u, armv8m_mem_read(&mem, 0xFFFFFF00, 4, &fault));
}

/*============================================================================
 * Test Runner
 *============================================================================*/

int main(int argc, char** argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
