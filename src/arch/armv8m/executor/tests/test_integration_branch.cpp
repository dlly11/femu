/**
 * @file test_integration_branch.cpp
 * @brief Branch integration tests
 */

#include "test_integration_common.h"

TEST_GROUP(IntegrationBranch){
    void setup(){memset(test_memory, 0, sizeof(test_memory));
memset(&exec, 0, sizeof(exec));
exec.mem.read = integ_mem_read;
exec.mem.write = integ_mem_write;
exec.mem.get_ptr = integ_mem_get_ptr;
exec.cpu.sp_main = STACK_BASE; /* Main stack pointer */
exec.cpu.r[15] = CODE_BASE;
exec.cpu.mode = MODE_THREAD;
exec.cpu.control = 0;
}

void teardown() {}
}
;

TEST(IntegrationBranch, UnconditionalBranchForward) {
  /* B +10 (branch forward 10 bytes from PC+4) */
  /* PC at 0, instruction at 0, PC+4 = 4, target = 4 + 10 = 14 */
  write_insn16(0, B_UNCOND(10));

  int result = armv8m_exec_step(&exec);

  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_EQUAL(14u, exec.cpu.r[15]);
}

TEST(IntegrationBranch, UnconditionalBranchBackward) {
  /* Start at PC=0x20, B -8 (branch back 8 bytes) */
  exec.cpu.r[15] = 0x20;
  write_insn16(0x20, B_UNCOND(-8));

  int result = armv8m_exec_step(&exec);

  CHECK_EQUAL(ARMV8M_OK, result);
  /* PC was 0x20, +4 = 0x24, -8 = 0x1C */
  CHECK_EQUAL(0x1Cu, exec.cpu.r[15]);
}

TEST(IntegrationBranch, ConditionalBranchTaken) {
  /* Set Z flag, then BEQ +6 */
  exec.cpu.xpsr = (1u << 30); /* Z flag set */
  write_insn16(0, B_COND(COND_EQ, 6));

  int result = armv8m_exec_step(&exec);

  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_EQUAL(10u, exec.cpu.r[15]); /* PC+4+6 = 10 */
}

TEST(IntegrationBranch, ConditionalBranchNotTaken) {
  /* Clear Z flag, then BEQ +6 (not taken) */
  exec.cpu.xpsr = 0; /* Z flag clear */
  write_insn16(0, B_COND(COND_EQ, 6));

  int result = armv8m_exec_step(&exec);

  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_EQUAL(2u, exec.cpu.r[15]); /* Just advance by 2 */
}

TEST(IntegrationBranch, BranchNotEqual) {
  /* Clear Z flag, then BNE +8 */
  exec.cpu.xpsr = 0; /* Z flag clear */
  write_insn16(0, B_COND(COND_NE, 8));

  int result = armv8m_exec_step(&exec);

  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_EQUAL(12u, exec.cpu.r[15]); /* PC+4+8 = 12 */
}

TEST(IntegrationBranch, BranchExchange) {
  /* R0 = 0x101 (bit 0 set for Thumb mode), BX R0 */
  exec.cpu.r[0] = 0x101;
  write_insn16(0, BX(0));

  int result = armv8m_exec_step(&exec);

  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_EQUAL(0x100u, exec.cpu.r[15]); /* Bit 0 cleared */
}

TEST(IntegrationBranch, BranchLinkExchange) {
  /* R0 = 0x201, BLX R0 - should set LR */
  exec.cpu.r[0] = 0x201;
  exec.cpu.r[15] = 0x100;
  write_insn16(0x100, BLX(0));

  int result = armv8m_exec_step(&exec);

  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_EQUAL(0x200u, exec.cpu.r[15]); /* Target address */
  CHECK_EQUAL(0x103u, exec.cpu.r[14]); /* LR = return address (PC+2) | 1 */
}

/*============================================================================
 * Test Group: Stack Operations
 *============================================================================*/
