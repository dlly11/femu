/**
 * @file test_executor.cpp
 * @brief CppUTest tests for the ARMv8-M instruction executor
 */

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

extern "C" {
#include "armv8m_executor.h"
#include "armv8m_types.h"
}

/*============================================================================
 * Mock Memory Callbacks
 *============================================================================*/

static uint8_t mock_memory[4096];

static uint32_t mock_mem_read(void *ctx, uint32_t addr, uint8_t size, bool *fault) {
    (void)ctx;
    *fault = false;

    if (addr >= sizeof(mock_memory)) {
        *fault = true;
        return 0;
    }

    switch (size) {
        case 1: return mock_memory[addr];
        case 2: return mock_memory[addr] | (mock_memory[addr + 1] << 8);
        case 4: return mock_memory[addr] | (mock_memory[addr + 1] << 8) |
                       (mock_memory[addr + 2] << 16) | (mock_memory[addr + 3] << 24);
    }
    return 0;
}

static void mock_mem_write(void *ctx, uint32_t addr, uint32_t value, uint8_t size, bool *fault) {
    (void)ctx;
    *fault = false;

    if (addr >= sizeof(mock_memory)) {
        *fault = true;
        return;
    }

    switch (size) {
        case 1:
            mock_memory[addr] = value & 0xFF;
            break;
        case 2:
            mock_memory[addr] = value & 0xFF;
            mock_memory[addr + 1] = (value >> 8) & 0xFF;
            break;
        case 4:
            mock_memory[addr] = value & 0xFF;
            mock_memory[addr + 1] = (value >> 8) & 0xFF;
            mock_memory[addr + 2] = (value >> 16) & 0xFF;
            mock_memory[addr + 3] = (value >> 24) & 0xFF;
            break;
    }
}

/*============================================================================
 * Test Group: Executor Initialization
 *============================================================================*/

TEST_GROUP(ExecutorInit)
{
    Executor exec;

    void setup()
    {
        memset(mock_memory, 0, sizeof(mock_memory));
        armv8m_exec_init(&exec);
        exec.mem.ctx = NULL;
        exec.mem.read = mock_mem_read;
        exec.mem.write = mock_mem_write;
    }

    void teardown()
    {
    }
};

TEST(ExecutorInit, InitSetsDefaultState)
{
    CHECK_EQUAL(0, exec.cpu.r[0]);
    CHECK_EQUAL(MODE_THREAD, exec.cpu.mode);
    CHECK_FALSE(exec.cpu.halted);
}

/*============================================================================
 * Test Group: Condition Checking
 *============================================================================*/

TEST_GROUP(ConditionCheck)
{
    void setup() {}
    void teardown() {}
};

TEST(ConditionCheck, AlwaysCondition)
{
    CHECK_TRUE(armv8m_check_condition(0, COND_AL));
}

TEST(ConditionCheck, EqualWhenZeroSet)
{
    uint32_t xpsr = ARMV8M_XPSR_Z;
    CHECK_TRUE(armv8m_check_condition(xpsr, COND_EQ));
    CHECK_FALSE(armv8m_check_condition(xpsr, COND_NE));
}

TEST(ConditionCheck, NegativeWhenNSet)
{
    uint32_t xpsr = ARMV8M_XPSR_N;
    CHECK_TRUE(armv8m_check_condition(xpsr, COND_MI));
    CHECK_FALSE(armv8m_check_condition(xpsr, COND_PL));
}

/*============================================================================
 * Test Group: Flag Updates
 *============================================================================*/

TEST_GROUP(FlagUpdates)
{
    CPUState cpu;

    void setup()
    {
        memset(&cpu, 0, sizeof(cpu));
    }

    void teardown()
    {
    }
};

TEST(FlagUpdates, ZeroResult)
{
    armv8m_update_flags(&cpu, 0, false, false);
    CHECK_TRUE(cpu.xpsr & ARMV8M_XPSR_Z);
    CHECK_FALSE(cpu.xpsr & ARMV8M_XPSR_N);
}

TEST(FlagUpdates, NegativeResult)
{
    armv8m_update_flags(&cpu, 0x80000000, false, false);
    CHECK_TRUE(cpu.xpsr & ARMV8M_XPSR_N);
    CHECK_FALSE(cpu.xpsr & ARMV8M_XPSR_Z);
}

TEST(FlagUpdates, CarrySet)
{
    armv8m_update_flags(&cpu, 1, true, false);
    CHECK_TRUE(cpu.xpsr & ARMV8M_XPSR_C);
}

TEST(FlagUpdates, OverflowSet)
{
    armv8m_update_flags(&cpu, 1, false, true);
    CHECK_TRUE(cpu.xpsr & ARMV8M_XPSR_V);
}

/*============================================================================
 * Test Group: Data Processing
 *============================================================================*/

TEST_GROUP(DataProcessing)
{
    Executor exec;
    DecodedInsn insn;

    void setup()
    {
        memset(mock_memory, 0, sizeof(mock_memory));
        armv8m_exec_init(&exec);
        exec.mem.ctx = NULL;
        exec.mem.read = mock_mem_read;
        exec.mem.write = mock_mem_write;
        armv8m_decode_init(&insn);
    }

    void teardown()
    {
    }
};

TEST(DataProcessing, MovImmediate)
{
    // MOV R0, #42
    insn.type = INSN_DATA_PROC_IMM;
    insn.op = DP_MOV;
    insn.rd = 0;
    insn.imm = 42;
    insn.set_flags = true;

    int result = armv8m_exec_insn(&exec, &insn);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(42u, exec.cpu.r[0]);
}

TEST(DataProcessing, AddRegisters)
{
    // ADD R0, R1, R2 where R1=10, R2=20
    exec.cpu.r[1] = 10;
    exec.cpu.r[2] = 20;

    insn.type = INSN_DATA_PROC_REG;
    insn.op = DP_ADD;
    insn.rd = 0;
    insn.rn = 1;
    insn.rm = 2;
    insn.set_flags = true;

    int result = armv8m_exec_insn(&exec, &insn);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(30u, exec.cpu.r[0]);
}

/*============================================================================
 * Test Group: Load/Store
 *============================================================================*/

TEST_GROUP(LoadStore)
{
    Executor exec;
    DecodedInsn insn;

    void setup()
    {
        memset(mock_memory, 0, sizeof(mock_memory));
        armv8m_exec_init(&exec);
        exec.mem.ctx = NULL;
        exec.mem.read = mock_mem_read;
        exec.mem.write = mock_mem_write;
        armv8m_decode_init(&insn);
    }

    void teardown()
    {
    }
};

TEST(LoadStore, LoadWord)
{
    // Store test value in memory
    mock_memory[0x100] = 0x78;
    mock_memory[0x101] = 0x56;
    mock_memory[0x102] = 0x34;
    mock_memory[0x103] = 0x12;

    // LDR R0, [R1] where R1=0x100
    exec.cpu.r[1] = 0x100;

    insn.type = INSN_LOAD_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 0;
    insn.access_size = ACCESS_WORD;

    int result = armv8m_exec_insn(&exec, &insn);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0x12345678u, exec.cpu.r[0]);
}

TEST(LoadStore, StoreWord)
{
    // STR R0, [R1] where R0=0xDEADBEEF, R1=0x200
    exec.cpu.r[0] = 0xDEADBEEF;
    exec.cpu.r[1] = 0x200;

    insn.type = INSN_STORE_IMM;
    insn.rt = 0;
    insn.rn = 1;
    insn.imm = 0;
    insn.access_size = ACCESS_WORD;

    int result = armv8m_exec_insn(&exec, &insn);

    CHECK_EQUAL(ARMV8M_OK, result);
    CHECK_EQUAL(0xEF, mock_memory[0x200]);
    CHECK_EQUAL(0xBE, mock_memory[0x201]);
    CHECK_EQUAL(0xAD, mock_memory[0x202]);
    CHECK_EQUAL(0xDE, mock_memory[0x203]);
}

/*============================================================================
 * Test Runner
 *============================================================================*/

int main(int argc, char** argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
