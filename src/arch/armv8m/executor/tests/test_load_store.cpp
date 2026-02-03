/**
 * @file test_load_store.cpp
 * @brief Load/store instruction tests including exclusive access
 */

#include "test_common.h"

TEST_GROUP(LoadStore) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.mem.ctx = NULL;
    exec.mem.read = mock_mem_read;
    exec.mem.write = mock_mem_write;
    exec.mem.get_ptr = mock_mem_get_ptr;
    init_insn(insn);
    mock().clear();
  }

  void teardown() {
    mock().checkExpectations();
    mock().clear();
  }
};

TEST(LoadStore, LdrImmediate) {
  mock_memory[0x100] = 0x78;
  mock_memory[0x101] = 0x56;
  mock_memory[0x102] = 0x34;
  mock_memory[0x103] = 0x12;

  exec.cpu.r[1] = 0x100;
  insn.type = INSN_LOAD_IMM;
  insn.rt = 0;
  insn.rn = 1;
  insn.imm = 0;
  insn.access_size = ACCESS_WORD;
  insn.add = true;
  insn.pre_index = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x12345678u, exec.cpu.r[0]);
}

TEST(LoadStore, LdrbImmediate) {
  mock_memory[0x100] = 0xAB;

  exec.cpu.r[1] = 0x100;
  insn.type = INSN_LOAD_IMM;
  insn.rt = 0;
  insn.rn = 1;
  insn.imm = 0;
  insn.access_size = ACCESS_BYTE;
  insn.add = true;
  insn.pre_index = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0xABu, exec.cpu.r[0]);
}

TEST(LoadStore, LdrsbImmediate) {
  mock_memory[0x100] = 0xFF;

  exec.cpu.r[1] = 0x100;
  insn.type = INSN_LOAD_IMM;
  insn.rt = 0;
  insn.rn = 1;
  insn.imm = 0;
  insn.access_size = ACCESS_BYTE;
  insn.is_signed = true;
  insn.add = true;
  insn.pre_index = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0xFFFFFFFFu, exec.cpu.r[0]);
}

TEST(LoadStore, LdrWithWriteback) {
  mock_memory[0x100] = 0x42;
  mock_memory[0x101] = 0x00;
  mock_memory[0x102] = 0x00;
  mock_memory[0x103] = 0x00;

  exec.cpu.r[1] = 0x100;
  insn.type = INSN_LOAD_IMM;
  insn.rt = 0;
  insn.rn = 1;
  insn.imm = 4;
  insn.access_size = ACCESS_WORD;
  insn.add = true;
  insn.pre_index = true;
  insn.writeback = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x104u, exec.cpu.r[1]);
}

TEST(LoadStore, StrImmediate) {
  exec.cpu.r[0] = 0x12345678;
  exec.cpu.r[1] = 0x200;
  insn.type = INSN_STORE_IMM;
  insn.rt = 0;
  insn.rn = 1;
  insn.imm = 0;
  insn.access_size = ACCESS_WORD;
  insn.add = true;
  insn.pre_index = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x78u, mock_memory[0x200]);
  CHECK_EQUAL(0x56u, mock_memory[0x201]);
  CHECK_EQUAL(0x34u, mock_memory[0x202]);
  CHECK_EQUAL(0x12u, mock_memory[0x203]);
}

TEST(LoadStore, LdrRegister) {
  mock_memory[0x108] = 0xEF;
  mock_memory[0x109] = 0xBE;
  mock_memory[0x10A] = 0xAD;
  mock_memory[0x10B] = 0xDE;

  exec.cpu.r[1] = 0x100;
  exec.cpu.r[2] = 2;
  insn.type = INSN_LOAD_REG;
  insn.rt = 0;
  insn.rn = 1;
  insn.rm = 2;
  insn.access_size = ACCESS_WORD;
  insn.shift_amount = 2;
  insn.add = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0xDEADBEEFu, exec.cpu.r[0]);
}

TEST(LoadStore, LdrLiteral) {
  /* LDR literal uses PC+4 in Thumb mode:
   * PC = 0x100, PC+4 = 0x104, Align(PC+4,4) = 0x104, address = 0x104 + 4 =
   * 0x108 */
  mock_memory[0x108] = 0x11;
  mock_memory[0x109] = 0x22;
  mock_memory[0x10A] = 0x33;
  mock_memory[0x10B] = 0x44;

  exec.cpu.r[ARMV8M_REG_PC] = 0x100;
  insn.type = INSN_LOAD_LITERAL;
  insn.rt = 0;
  insn.imm = 4;
  insn.access_size = ACCESS_WORD;
  insn.add = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x44332211u, exec.cpu.r[0]);
}

TEST(LoadStore, Ldm) {
  mock_memory[0x100] = 0x11;
  mock_memory[0x101] = 0x11;
  mock_memory[0x102] = 0x11;
  mock_memory[0x103] = 0x11;
  mock_memory[0x104] = 0x22;
  mock_memory[0x105] = 0x22;
  mock_memory[0x106] = 0x22;
  mock_memory[0x107] = 0x22;

  exec.cpu.r[4] = 0x100;
  insn.type = INSN_LOAD_MULTIPLE;
  insn.rn = 4;
  insn.register_list = 0x0003;
  insn.add = true;
  insn.writeback = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x11111111u, exec.cpu.r[0]);
  CHECK_EQUAL(0x22222222u, exec.cpu.r[1]);
  CHECK_EQUAL(0x108u, exec.cpu.r[4]);
}

TEST(LoadStore, Stm) {
  exec.cpu.r[0] = 0xAAAAAAAA;
  exec.cpu.r[1] = 0xBBBBBBBB;
  exec.cpu.r[4] = 0x200;
  insn.type = INSN_STORE_MULTIPLE;
  insn.rn = 4;
  insn.register_list = 0x0003;
  insn.add = true;
  insn.writeback = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0xAAu, mock_memory[0x200]);
  CHECK_EQUAL(0xBBu, mock_memory[0x204]);
  CHECK_EQUAL(0x208u, exec.cpu.r[4]);
}

/*============================================================================
 * Test Group: Branch Instructions
 *============================================================================*/

TEST_GROUP(Exclusive) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    armv8m_exec_init(&exec);
    exec.mem.read = mock_mem_read;
    exec.mem.write = mock_mem_write;
    exec.mem.get_ptr = mock_mem_get_ptr;
    memset(mock_memory, 0, sizeof(mock_memory));
    init_insn(insn);
    mock().clear();
  }

  void teardown() {
    mock().checkExpectations();
    mock().clear();
  }
};

TEST(Exclusive, LdrexLoadsAndMarksMonitor) {
  /* LDREX R0, [R1] */
  insn.type = INSN_LOAD_EXCLUSIVE;
  insn.rt = 0;
  insn.rn = 1;
  insn.imm = 0;
  insn.add = true;

  exec.cpu.r[1] = 0x100; /* Base address */

  /* Set up memory value */
  mock_memory[0x100] = 0x78;
  mock_memory[0x101] = 0x56;
  mock_memory[0x102] = 0x34;
  mock_memory[0x103] = 0x12;

  int result = armv8m_exec_insn(&exec, &insn);
  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_EQUAL(0x12345678u, exec.cpu.r[0]); /* Loaded value */
  CHECK(exec.cpu.exclusive_valid);
  CHECK_EQUAL(0x100u, exec.cpu.exclusive_addr);
}

TEST(Exclusive, LdrexWithOffset) {
  /* LDREX R0, [R1, #8] */
  insn.type = INSN_LOAD_EXCLUSIVE;
  insn.rt = 0;
  insn.rn = 1;
  insn.imm = 8; /* Offset */
  insn.add = true;

  exec.cpu.r[1] = 0x100; /* Base address */

  /* Set up memory value at 0x108 */
  mock_memory[0x108] = 0xEF;
  mock_memory[0x109] = 0xBE;
  mock_memory[0x10A] = 0xAD;
  mock_memory[0x10B] = 0xDE;

  int result = armv8m_exec_insn(&exec, &insn);
  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_EQUAL(0xDEADBEEFu, exec.cpu.r[0]);
  CHECK(exec.cpu.exclusive_valid);
  CHECK_EQUAL(0x108u, exec.cpu.exclusive_addr);
}

TEST(Exclusive, StrexSucceedsWhenMonitorValid) {
  /* STREX R0, R2, [R1] - success case */
  insn.type = INSN_STORE_EXCLUSIVE;
  insn.rd = 0; /* Status register */
  insn.rt = 2; /* Value to store */
  insn.rn = 1; /* Base address */
  insn.imm = 0;
  insn.add = true;

  exec.cpu.r[1] = 0x100;
  exec.cpu.r[2] = 0xCAFEBABE;
  exec.cpu.exclusive_valid = true;
  exec.cpu.exclusive_addr = 0x100;

  int result = armv8m_exec_insn(&exec, &insn);
  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_EQUAL(0u, exec.cpu.r[0]);        /* Success status */
  CHECK_FALSE(exec.cpu.exclusive_valid); /* Monitor cleared */

  /* Verify memory was written */
  uint32_t stored = mock_memory[0x100] | ((uint32_t)mock_memory[0x101] << 8) |
                    ((uint32_t)mock_memory[0x102] << 16) |
                    ((uint32_t)mock_memory[0x103] << 24);
  CHECK_EQUAL(0xCAFEBABEu, stored);
}

TEST(Exclusive, StrexFailsWhenMonitorInvalid) {
  /* STREX R0, R2, [R1] - failure case (no prior LDREX) */
  insn.type = INSN_STORE_EXCLUSIVE;
  insn.rd = 0;
  insn.rt = 2;
  insn.rn = 1;
  insn.imm = 0;
  insn.add = true;

  exec.cpu.r[1] = 0x100;
  exec.cpu.r[2] = 0x12345678;
  exec.cpu.exclusive_valid = false; /* No prior LDREX */

  int result = armv8m_exec_insn(&exec, &insn);
  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_EQUAL(1u, exec.cpu.r[0]); /* Failure status */
  CHECK_FALSE(exec.cpu.exclusive_valid);

  /* Memory should NOT be modified */
  CHECK_EQUAL(0u, mock_memory[0x100]);
}

TEST(Exclusive, StrexFailsWhenAddressMismatch) {
  /* STREX fails when address doesn't match LDREX */
  insn.type = INSN_STORE_EXCLUSIVE;
  insn.rd = 0;
  insn.rt = 2;
  insn.rn = 1;
  insn.imm = 0;
  insn.add = true;

  exec.cpu.r[1] = 0x200; /* Different address */
  exec.cpu.r[2] = 0x12345678;
  exec.cpu.exclusive_valid = true;
  exec.cpu.exclusive_addr = 0x100; /* LDREX was at different address */

  int result = armv8m_exec_insn(&exec, &insn);
  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_EQUAL(1u, exec.cpu.r[0]); /* Failure status */
  CHECK_FALSE(exec.cpu.exclusive_valid);
}

TEST(Exclusive, LdrexStrexAtomicSequence) {
  /* Complete LDREX/STREX atomic increment sequence */

  /* Step 1: LDREX R0, [R1] */
  DecodedInsn ldrex;
  init_insn(ldrex);
  ldrex.type = INSN_LOAD_EXCLUSIVE;
  ldrex.rt = 0;
  ldrex.rn = 1;
  ldrex.imm = 0;
  ldrex.add = true;

  exec.cpu.r[1] = 0x100;

  /* Initial value in memory */
  mock_memory[0x100] = 42;
  mock_memory[0x101] = 0;
  mock_memory[0x102] = 0;
  mock_memory[0x103] = 0;

  int result = armv8m_exec_insn(&exec, &ldrex);
  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_EQUAL(42u, exec.cpu.r[0]);

  /* Step 2: Increment value (simulate ADD R0, R0, #1) */
  exec.cpu.r[0]++;

  /* Step 3: STREX R2, R0, [R1] */
  DecodedInsn strex;
  init_insn(strex);
  strex.type = INSN_STORE_EXCLUSIVE;
  strex.rd = 2; /* Status */
  strex.rt = 0; /* Incremented value */
  strex.rn = 1; /* Base */
  strex.imm = 0;
  strex.add = true;

  result = armv8m_exec_insn(&exec, &strex);
  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_EQUAL(0u, exec.cpu.r[2]);       /* Success */
  CHECK_EQUAL(43u, mock_memory[0x100]); /* Memory updated */
}

TEST(Exclusive, LdrexUnalignedFaults) {
  /* LDREX at unaligned address should fault */
  insn.type = INSN_LOAD_EXCLUSIVE;
  insn.rt = 0;
  insn.rn = 1;
  insn.imm = 0;
  insn.add = true;

  exec.cpu.r[1] = 0x101; /* Unaligned address */

  int result = armv8m_exec_insn(&exec, &insn);
  CHECK_EQUAL(ARMV8M_ERR_USAGE_FAULT, result);
}

TEST(Exclusive, StrexUnalignedFaults) {
  /* STREX at unaligned address should fault */
  insn.type = INSN_STORE_EXCLUSIVE;
  insn.rd = 0;
  insn.rt = 2;
  insn.rn = 1;
  insn.imm = 0;
  insn.add = true;

  exec.cpu.r[1] = 0x102; /* Unaligned address */
  exec.cpu.exclusive_valid = true;
  exec.cpu.exclusive_addr = 0x102;

  int result = armv8m_exec_insn(&exec, &insn);
  CHECK_EQUAL(ARMV8M_ERR_USAGE_FAULT, result);
}

TEST(Exclusive, ResetClearsExclusiveMonitor) {
  /* Set up exclusive monitor */
  exec.cpu.exclusive_valid = true;
  exec.cpu.exclusive_addr = 0x100;

  /* Set up vector table */
  mock_memory[0] = 0x00;
  mock_memory[1] = 0x10;
  mock_memory[2] = 0x00;
  mock_memory[3] = 0x20; /* SP = 0x20001000 */
  mock_memory[4] = 0x01;
  mock_memory[5] = 0x00;
  mock_memory[6] = 0x00;
  mock_memory[7] = 0x00; /* Reset = 0x00000001 */

  armv8m_exec_reset(&exec, 0);

  CHECK_FALSE(exec.cpu.exclusive_valid);
  CHECK_EQUAL(0u, exec.cpu.exclusive_addr);
}
