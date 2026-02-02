/**
 * @file test_emulator.cpp
 * @brief Unit tests for emulator glue layer
 */

#include "CppUTest/TestHarness.h"
#include "arch/armv8m/armv8m_emulator.h"
#include <cstring>

TEST_GROUP(EmulatorTests)
{
    Emulator emu;

    void setup()
    {
        memset(&emu, 0, sizeof(emu));
    }

    void teardown()
    {
        armv8m_emu_destroy(&emu);
    }
};

TEST(EmulatorTests, DefaultConfig)
{
    EmulatorConfig config;
    armv8m_emu_default_config(&config);

    CHECK_EQUAL(false, config.has_fpu);
    CHECK_EQUAL(false, config.has_dsp);
    CHECK_EQUAL(false, config.has_trustzone);
    CHECK_EQUAL(8, config.num_mpu_regions);
    CHECK_EQUAL(32, config.num_irqs);
    CHECK_EQUAL(0x08000000u, config.default_flash_base);
    CHECK_EQUAL(0x20000000u, config.default_ram_base);
}

TEST(EmulatorTests, InitWithNullConfig)
{
    int result = armv8m_emu_init(&emu, nullptr);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(EMU_STATE_STOPPED, emu.state);
}

TEST(EmulatorTests, InitWithConfig)
{
    EmulatorConfig config;
    armv8m_emu_default_config(&config);
    config.has_fpu = true;
    config.num_irqs = 64;

    int result = armv8m_emu_init(&emu, &config);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(true, emu.exec.has_fpu);
}

TEST(EmulatorTests, AddFlash)
{
    armv8m_emu_init(&emu, nullptr);

    int result = armv8m_emu_add_flash(&emu, 0x08000000, 0x10000);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0x08000000u, emu.flash_base);
    CHECK_EQUAL(0x10000u, emu.flash_size);
    CHECK(emu.flash_data != nullptr);
}

TEST(EmulatorTests, AddRam)
{
    armv8m_emu_init(&emu, nullptr);

    int result = armv8m_emu_add_ram(&emu, 0x20000000, 0x8000);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0x20000000u, emu.ram_base);
    CHECK_EQUAL(0x8000u, emu.ram_size);
    CHECK(emu.ram_data != nullptr);
}

TEST(EmulatorTests, LoadData)
{
    armv8m_emu_init(&emu, nullptr);
    armv8m_emu_add_flash(&emu, 0x08000000, 0x10000);

    uint8_t data[] = {0x00, 0x20, 0x00, 0x20,  // Initial SP
                      0x01, 0x00, 0x00, 0x08}; // Reset vector

    int result = armv8m_emu_load(&emu, 0x08000000, data, sizeof(data));
    CHECK_EQUAL(ARMV8M_OK, result);

    // Verify data was loaded
    bool fault = false;
    uint32_t val = armv8m_emu_read_mem(&emu, 0x08000000, 4, &fault);
    CHECK_EQUAL(false, fault);
    CHECK_EQUAL(0x20002000u, val);
}

TEST(EmulatorTests, GetSetRegisters)
{
    armv8m_emu_init(&emu, nullptr);

    // Set and get R0
    armv8m_emu_set_reg(&emu, 0, 0x12345678);
    CHECK_EQUAL(0x12345678u, armv8m_emu_get_reg(&emu, 0));

    // Set and get PC
    armv8m_emu_set_pc(&emu, 0x08000100);
    CHECK_EQUAL(0x08000100u, armv8m_emu_get_pc(&emu));

    // Set and get xPSR
    armv8m_emu_set_xpsr(&emu, 0x41000000);
    CHECK_EQUAL(0x41000000u, armv8m_emu_get_xpsr(&emu));
}

TEST(EmulatorTests, MemoryReadWrite)
{
    armv8m_emu_init(&emu, nullptr);
    armv8m_emu_add_ram(&emu, 0x20000000, 0x8000);

    bool fault = false;

    // Write 32-bit
    armv8m_emu_write_mem(&emu, 0x20000000, 0xDEADBEEF, 4, &fault);
    CHECK_EQUAL(false, fault);

    // Read 32-bit
    uint32_t val = armv8m_emu_read_mem(&emu, 0x20000000, 4, &fault);
    CHECK_EQUAL(false, fault);
    CHECK_EQUAL(0xDEADBEEFu, val);

    // Read 8-bit
    val = armv8m_emu_read_mem(&emu, 0x20000000, 1, &fault);
    CHECK_EQUAL(false, fault);
    CHECK_EQUAL(0xEFu, val);
}

TEST(EmulatorTests, BlockReadWrite)
{
    armv8m_emu_init(&emu, nullptr);
    armv8m_emu_add_ram(&emu, 0x20000000, 0x8000);

    uint8_t write_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t written = armv8m_emu_write_block(&emu, 0x20000000, write_data, sizeof(write_data));
    CHECK_EQUAL(sizeof(write_data), written);

    uint8_t read_data[5];
    uint32_t read = armv8m_emu_read_block(&emu, 0x20000000, read_data, sizeof(read_data));
    CHECK_EQUAL(sizeof(read_data), read);

    MEMCMP_EQUAL(write_data, read_data, sizeof(write_data));
}

TEST(EmulatorTests, Breakpoints)
{
    armv8m_emu_init(&emu, nullptr);

    // Add breakpoint
    int result = armv8m_emu_add_breakpoint(&emu, 0x08000100);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(1, emu.num_breakpoints);
    CHECK(armv8m_emu_has_breakpoint(&emu, 0x08000100));

    // Add same breakpoint again (should be OK)
    result = armv8m_emu_add_breakpoint(&emu, 0x08000100);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(1, emu.num_breakpoints);  // Still 1

    // Add another breakpoint
    result = armv8m_emu_add_breakpoint(&emu, 0x08000200);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(2, emu.num_breakpoints);

    // Remove first breakpoint
    result = armv8m_emu_remove_breakpoint(&emu, 0x08000100);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(1, emu.num_breakpoints);
    CHECK(!armv8m_emu_has_breakpoint(&emu, 0x08000100));
    CHECK(armv8m_emu_has_breakpoint(&emu, 0x08000200));

    // Clear all
    armv8m_emu_clear_breakpoints(&emu);
    CHECK_EQUAL(0, emu.num_breakpoints);
}

TEST(EmulatorTests, WatchpointAddRemove)
{
    armv8m_emu_init(&emu, nullptr);

    // Add write watchpoint
    int result = armv8m_emu_add_watchpoint(&emu, 0x20000100, 4, WATCHPOINT_WRITE);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(1, emu.num_watchpoints);

    // Add same watchpoint again (should be OK, no duplicate)
    result = armv8m_emu_add_watchpoint(&emu, 0x20000100, 4, WATCHPOINT_WRITE);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(1, emu.num_watchpoints);

    // Add different type at same address
    result = armv8m_emu_add_watchpoint(&emu, 0x20000100, 4, WATCHPOINT_READ);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(2, emu.num_watchpoints);

    // Add access watchpoint at different address
    result = armv8m_emu_add_watchpoint(&emu, 0x20000200, 4, WATCHPOINT_ACCESS);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(3, emu.num_watchpoints);

    // Remove write watchpoint
    result = armv8m_emu_remove_watchpoint(&emu, 0x20000100, 4, WATCHPOINT_WRITE);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(2, emu.num_watchpoints);

    // Remove non-existent watchpoint (should be OK)
    result = armv8m_emu_remove_watchpoint(&emu, 0x20000300, 4, WATCHPOINT_WRITE);
    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(2, emu.num_watchpoints);

    // Clear all watchpoints
    armv8m_emu_clear_watchpoints(&emu);
    CHECK_EQUAL(0, emu.num_watchpoints);
}

TEST(EmulatorTests, WatchpointCheck)
{
    armv8m_emu_init(&emu, nullptr);

    // Add watchpoints
    armv8m_emu_add_watchpoint(&emu, 0x20000100, 4, WATCHPOINT_WRITE);
    armv8m_emu_add_watchpoint(&emu, 0x20000200, 4, WATCHPOINT_READ);
    armv8m_emu_add_watchpoint(&emu, 0x20000300, 4, WATCHPOINT_ACCESS);

    // Check write watchpoint - should match write access
    const Watchpoint *wp = armv8m_emu_check_watchpoint(&emu, 0x20000100, 4, true);
    CHECK(wp != nullptr);
    CHECK_EQUAL(WATCHPOINT_WRITE, wp->type);

    // Write watchpoint should NOT match read access
    wp = armv8m_emu_check_watchpoint(&emu, 0x20000100, 4, false);
    CHECK(wp == nullptr);

    // Check read watchpoint - should match read access
    wp = armv8m_emu_check_watchpoint(&emu, 0x20000200, 4, false);
    CHECK(wp != nullptr);
    CHECK_EQUAL(WATCHPOINT_READ, wp->type);

    // Read watchpoint should NOT match write access
    wp = armv8m_emu_check_watchpoint(&emu, 0x20000200, 4, true);
    CHECK(wp == nullptr);

    // Access watchpoint should match both read and write
    wp = armv8m_emu_check_watchpoint(&emu, 0x20000300, 4, true);
    CHECK(wp != nullptr);
    CHECK_EQUAL(WATCHPOINT_ACCESS, wp->type);

    wp = armv8m_emu_check_watchpoint(&emu, 0x20000300, 4, false);
    CHECK(wp != nullptr);
    CHECK_EQUAL(WATCHPOINT_ACCESS, wp->type);

    // Check overlapping access (1 byte within 4-byte watchpoint)
    wp = armv8m_emu_check_watchpoint(&emu, 0x20000102, 1, true);
    CHECK(wp != nullptr);

    // Check non-overlapping access
    wp = armv8m_emu_check_watchpoint(&emu, 0x20000104, 4, true);
    CHECK(wp == nullptr);
}

TEST(EmulatorTests, WatchpointHitInfo)
{
    armv8m_emu_init(&emu, nullptr);

    // Initially should return defaults
    CHECK_EQUAL(0u, armv8m_emu_get_watchpoint_hit_addr(&emu));

    // Set hit info (normally done by memory callbacks)
    emu.watchpoint_hit_addr = 0x20000100;
    emu.watchpoint_hit_type = WATCHPOINT_WRITE;

    CHECK_EQUAL(0x20000100u, armv8m_emu_get_watchpoint_hit_addr(&emu));
    CHECK_EQUAL(WATCHPOINT_WRITE, armv8m_emu_get_watchpoint_hit_type(&emu));
}

TEST(EmulatorTests, WatchpointNullPointerHandling)
{
    // All watchpoint functions should handle NULL gracefully
    CHECK_EQUAL(ARMV8M_ERR_INVALID_PARAM, armv8m_emu_add_watchpoint(nullptr, 0, 4, WATCHPOINT_WRITE));
    CHECK_EQUAL(ARMV8M_ERR_INVALID_PARAM, armv8m_emu_remove_watchpoint(nullptr, 0, 4, WATCHPOINT_WRITE));
    CHECK(armv8m_emu_check_watchpoint(nullptr, 0, 4, true) == nullptr);
    CHECK_EQUAL(0u, armv8m_emu_get_watchpoint_hit_addr(nullptr));

    // These should not crash
    armv8m_emu_clear_watchpoints(nullptr);
}

TEST(EmulatorTests, Reset)
{
    armv8m_emu_init(&emu, nullptr);
    armv8m_emu_add_flash(&emu, 0x08000000, 0x10000);
    armv8m_emu_add_ram(&emu, 0x20000000, 0x8000);

    // Create simple vector table
    uint8_t vector_table[] = {
        0x00, 0x80, 0x00, 0x20,  // Initial SP = 0x20008000
        0x01, 0x01, 0x00, 0x08   // Reset vector = 0x08000101 (Thumb)
    };
    armv8m_emu_load(&emu, 0x08000000, vector_table, sizeof(vector_table));

    // Reset
    armv8m_emu_reset(&emu);

    // Verify initial state
    CHECK_EQUAL(0x20008000u, emu.exec.cpu.sp_main);
    CHECK_EQUAL(0x08000100u, armv8m_emu_get_pc(&emu));  // Thumb bit cleared
    CHECK_EQUAL(EMU_STATE_STOPPED, emu.state);
}

TEST(EmulatorTests, StopRequest)
{
    armv8m_emu_init(&emu, nullptr);

    CHECK_EQUAL(false, emu.stop_requested);

    armv8m_emu_stop(&emu);

    CHECK_EQUAL(true, emu.stop_requested);
}

TEST(EmulatorTests, GetState)
{
    armv8m_emu_init(&emu, nullptr);

    CHECK_EQUAL(EMU_STATE_STOPPED, armv8m_emu_get_state(&emu));

    emu.state = EMU_STATE_RUNNING;
    CHECK_EQUAL(EMU_STATE_RUNNING, armv8m_emu_get_state(&emu));
}

TEST(EmulatorTests, FpuRegisters)
{
    EmulatorConfig config;
    armv8m_emu_default_config(&config);
    config.has_fpu = true;
    armv8m_emu_init(&emu, &config);

    // Set and get FPU register
    armv8m_emu_set_fpu_reg(&emu, 0, 0x40490FDB);  // Pi
    CHECK_EQUAL(0x40490FDBu, armv8m_emu_get_fpu_reg(&emu, 0));

    // Set and get FPSCR
    armv8m_emu_set_fpscr(&emu, 0x01000000);
    CHECK_EQUAL(0x01000000u, armv8m_emu_get_fpscr(&emu));
}

TEST(EmulatorTests, SpecialRegisters)
{
    armv8m_emu_init(&emu, nullptr);

    // Test PRIMASK
    armv8m_emu_set_special_reg(&emu, ARMV8M_SYSREG_PRIMASK, 1);
    CHECK_EQUAL(1u, armv8m_emu_get_special_reg(&emu, ARMV8M_SYSREG_PRIMASK));

    // Test MSP
    armv8m_emu_set_special_reg(&emu, ARMV8M_SYSREG_MSP, 0x20008000);
    CHECK_EQUAL(0x20008000u, armv8m_emu_get_special_reg(&emu, ARMV8M_SYSREG_MSP));

    // Test PSP
    armv8m_emu_set_special_reg(&emu, ARMV8M_SYSREG_PSP, 0x20004000);
    CHECK_EQUAL(0x20004000u, armv8m_emu_get_special_reg(&emu, ARMV8M_SYSREG_PSP));
}

TEST(EmulatorTests, Cycles)
{
    armv8m_emu_init(&emu, nullptr);

    CHECK_EQUAL(0u, armv8m_emu_get_cycles(&emu));

    emu.exec.cpu.cycles = 12345;
    CHECK_EQUAL(12345u, armv8m_emu_get_cycles(&emu));
}

TEST(EmulatorTests, NullPointerHandling)
{
    // All functions should handle NULL gracefully
    CHECK_EQUAL(ARMV8M_ERR_INVALID_PARAM, armv8m_emu_init(nullptr, nullptr));
    CHECK_EQUAL(0u, armv8m_emu_get_reg(nullptr, 0));
    CHECK_EQUAL(0u, armv8m_emu_get_pc(nullptr));
    CHECK_EQUAL(EMU_STATE_STOPPED, armv8m_emu_get_state(nullptr));

    // These should not crash
    armv8m_emu_destroy(nullptr);
    armv8m_emu_reset(nullptr);
    armv8m_emu_stop(nullptr);
    armv8m_emu_set_reg(nullptr, 0, 0);
}
