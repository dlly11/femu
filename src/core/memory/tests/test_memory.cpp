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
            rom[i] = i;
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
 * Test Runner
 *============================================================================*/

int main(int argc, char** argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
