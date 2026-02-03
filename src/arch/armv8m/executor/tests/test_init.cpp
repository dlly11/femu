/**
 * @file test_init.cpp
 * @brief Executor initialization and basic utility tests
 */

#include "test_common.h"

TEST_GROUP(ExecutorInit) {
  Executor exec;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    mock().clear();
  }

  void teardown() {
    mock().checkExpectations();
    mock().clear();
  }
};

TEST(ExecutorInit, InitSetsDefaultState) {
  armv8m_exec_init(&exec);

  CHECK_EQUAL(0u, exec.cpu.r[0]);
  CHECK_EQUAL(0u, exec.cpu.r[1]);
  CHECK_EQUAL(ARMV8M_XPSR_T, exec.cpu.xpsr);
  CHECK_EQUAL(MODE_THREAD, exec.cpu.mode);
  CHECK_FALSE(exec.cpu.halted);
  CHECK_FALSE(exec.cpu.sleeping);
}

TEST(ExecutorInit, ResetLoadsVectorTable) {
  armv8m_exec_init(&exec);
  exec.mem.ctx = NULL;
  exec.mem.read = mock_mem_read;

  /* Set up vector table */
  mock_memory[0] = 0x00;
  mock_memory[1] = 0x10;
  mock_memory[2] = 0x00;
  mock_memory[3] = 0x20; /* Initial SP = 0x20001000 */
  mock_memory[4] = 0x01;
  mock_memory[5] = 0x01;
  mock_memory[6] = 0x00;
  mock_memory[7] = 0x00; /* Reset handler = 0x00000101 */

  armv8m_exec_reset(&exec, 0);

  CHECK_EQUAL(0x20001000u, exec.cpu.sp_main);
  CHECK_EQUAL(0x100u, exec.cpu.r[ARMV8M_REG_PC]);
}

/*============================================================================
 * Test Group: Condition Checking
 *============================================================================*/

TEST_GROUP(ConditionCheck){void setup(){mock().clear();
}

void teardown() {
  mock().checkExpectations();
  mock().clear();
}
}
;

TEST(ConditionCheck, EqTrueWhenZSet) {
  uint32_t xpsr = ARMV8M_XPSR_T | ARMV8M_XPSR_Z;
  CHECK_TRUE(armv8m_check_condition(xpsr, COND_EQ));
}

TEST(ConditionCheck, EqFalseWhenZClear) {
  uint32_t xpsr = ARMV8M_XPSR_T;
  CHECK_FALSE(armv8m_check_condition(xpsr, COND_EQ));
}

TEST(ConditionCheck, NeTrueWhenZClear) {
  uint32_t xpsr = ARMV8M_XPSR_T;
  CHECK_TRUE(armv8m_check_condition(xpsr, COND_NE));
}

TEST(ConditionCheck, NeFalseWhenZSet) {
  uint32_t xpsr = ARMV8M_XPSR_T | ARMV8M_XPSR_Z;
  CHECK_FALSE(armv8m_check_condition(xpsr, COND_NE));
}

TEST(ConditionCheck, CsTrueWhenCSet) {
  uint32_t xpsr = ARMV8M_XPSR_T | ARMV8M_XPSR_C;
  CHECK_TRUE(armv8m_check_condition(xpsr, COND_CS));
}

TEST(ConditionCheck, CcTrueWhenCClear) {
  uint32_t xpsr = ARMV8M_XPSR_T;
  CHECK_TRUE(armv8m_check_condition(xpsr, COND_CC));
}

TEST(ConditionCheck, MiTrueWhenNSet) {
  uint32_t xpsr = ARMV8M_XPSR_T | ARMV8M_XPSR_N;
  CHECK_TRUE(armv8m_check_condition(xpsr, COND_MI));
}

TEST(ConditionCheck, PlTrueWhenNClear) {
  uint32_t xpsr = ARMV8M_XPSR_T;
  CHECK_TRUE(armv8m_check_condition(xpsr, COND_PL));
}

TEST(ConditionCheck, VsTrueWhenVSet) {
  uint32_t xpsr = ARMV8M_XPSR_T | ARMV8M_XPSR_V;
  CHECK_TRUE(armv8m_check_condition(xpsr, COND_VS));
}

TEST(ConditionCheck, VcTrueWhenVClear) {
  uint32_t xpsr = ARMV8M_XPSR_T;
  CHECK_TRUE(armv8m_check_condition(xpsr, COND_VC));
}

TEST(ConditionCheck, HiTrueWhenCSetAndZClear) {
  uint32_t xpsr = ARMV8M_XPSR_T | ARMV8M_XPSR_C;
  CHECK_TRUE(armv8m_check_condition(xpsr, COND_HI));
}

TEST(ConditionCheck, LsTrueWhenCClearOrZSet) {
  uint32_t xpsr = ARMV8M_XPSR_T | ARMV8M_XPSR_Z;
  CHECK_TRUE(armv8m_check_condition(xpsr, COND_LS));
}

TEST(ConditionCheck, GeTrueWhenNEqualsV) {
  uint32_t xpsr = ARMV8M_XPSR_T | ARMV8M_XPSR_N | ARMV8M_XPSR_V;
  CHECK_TRUE(armv8m_check_condition(xpsr, COND_GE));
}

TEST(ConditionCheck, LtTrueWhenNNotEqualsV) {
  uint32_t xpsr = ARMV8M_XPSR_T | ARMV8M_XPSR_N;
  CHECK_TRUE(armv8m_check_condition(xpsr, COND_LT));
}

TEST(ConditionCheck, GtTrueWhenZClearAndNEqualsV) {
  uint32_t xpsr = ARMV8M_XPSR_T;
  CHECK_TRUE(armv8m_check_condition(xpsr, COND_GT));
}

TEST(ConditionCheck, LeTrueWhenZSetOrNNotEqualsV) {
  uint32_t xpsr = ARMV8M_XPSR_T | ARMV8M_XPSR_Z;
  CHECK_TRUE(armv8m_check_condition(xpsr, COND_LE));
}

TEST(ConditionCheck, AlAlwaysTrue) {
  uint32_t xpsr = ARMV8M_XPSR_T;
  CHECK_TRUE(armv8m_check_condition(xpsr, COND_AL));
}

/*============================================================================
 * Test Group: Flag Updates
 *============================================================================*/

TEST_GROUP(FlagUpdates) {
  CPUState cpu;

  void setup() {
    memset(&cpu, 0, sizeof(cpu));
    cpu.xpsr = ARMV8M_XPSR_T;
    mock().clear();
  }

  void teardown() {
    mock().checkExpectations();
    mock().clear();
  }
};

TEST(FlagUpdates, ZeroResultSetsZ) {
  armv8m_update_flags(&cpu, 0, false, false);
  CHECK_TRUE(cpu.xpsr & ARMV8M_XPSR_Z);
  CHECK_FALSE(cpu.xpsr & ARMV8M_XPSR_N);
}

TEST(FlagUpdates, NegativeResultSetsN) {
  armv8m_update_flags(&cpu, 0x80000000, false, false);
  CHECK_TRUE(cpu.xpsr & ARMV8M_XPSR_N);
  CHECK_FALSE(cpu.xpsr & ARMV8M_XPSR_Z);
}

TEST(FlagUpdates, CarryOutSetsC) {
  armv8m_update_flags(&cpu, 1, true, false);
  CHECK_TRUE(cpu.xpsr & ARMV8M_XPSR_C);
}

TEST(FlagUpdates, OverflowSetsV) {
  armv8m_update_flags(&cpu, 1, false, true);
  CHECK_TRUE(cpu.xpsr & ARMV8M_XPSR_V);
}

TEST(FlagUpdates, ClearsFlags) {
  cpu.xpsr = ARMV8M_XPSR_T | ARMV8M_XPSR_N | ARMV8M_XPSR_Z | ARMV8M_XPSR_C |
             ARMV8M_XPSR_V;
  armv8m_update_flags(&cpu, 1, false, false);
  CHECK_FALSE(cpu.xpsr & ARMV8M_XPSR_N);
  CHECK_FALSE(cpu.xpsr & ARMV8M_XPSR_Z);
  CHECK_FALSE(cpu.xpsr & ARMV8M_XPSR_C);
  CHECK_FALSE(cpu.xpsr & ARMV8M_XPSR_V);
}

/*============================================================================
 * Test Group: Stack Pointer Handling
 *============================================================================*/

TEST_GROUP(StackPointer) {
  CPUState cpu;

  void setup() {
    memset(&cpu, 0, sizeof(cpu));
    cpu.xpsr = ARMV8M_XPSR_T;
    cpu.sp_main = 0x20001000;
    cpu.sp_process = 0x20002000;
    mock().clear();
  }

  void teardown() {
    mock().checkExpectations();
    mock().clear();
  }
};

TEST(StackPointer, ThreadModeUsesMainByDefault) {
  cpu.mode = MODE_THREAD;
  cpu.control = 0;
  CHECK_EQUAL(0x20001000u, armv8m_get_sp(&cpu));
}

TEST(StackPointer, ThreadModeUsesPspWhenSpselSet) {
  cpu.mode = MODE_THREAD;
  cpu.control = ARMV8M_CONTROL_SPSEL;
  CHECK_EQUAL(0x20002000u, armv8m_get_sp(&cpu));
}

TEST(StackPointer, HandlerModeAlwaysUsesMsp) {
  cpu.mode = MODE_HANDLER;
  cpu.control = ARMV8M_CONTROL_SPSEL;
  CHECK_EQUAL(0x20001000u, armv8m_get_sp(&cpu));
}

TEST(StackPointer, SetSpThreadModeMain) {
  cpu.mode = MODE_THREAD;
  cpu.control = 0;
  armv8m_set_sp(&cpu, 0x30000000);
  CHECK_EQUAL(0x30000000u, cpu.sp_main);
}

TEST(StackPointer, SetSpThreadModePsp) {
  cpu.mode = MODE_THREAD;
  cpu.control = ARMV8M_CONTROL_SPSEL;
  armv8m_set_sp(&cpu, 0x30000000);
  CHECK_EQUAL(0x30000000u, cpu.sp_process);
}

/*============================================================================
 * Test Group: Data Processing - Immediate
 *============================================================================*/
