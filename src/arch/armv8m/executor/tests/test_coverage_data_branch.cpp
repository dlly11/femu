/**
 * @file test_coverage_data_branch.cpp
 * @brief Data processing and branch coverage tests
 */

#include "test_common.h"

TEST_GROUP(DataProcCoverage) {
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

/* Shift edge cases */
TEST(DataProcCoverage, LslBy32) {
  exec.cpu.r[0] = 0x80000001;
  exec.cpu.r[1] = 32;
  insn.type = INSN_DATA_PROC_REG;
  insn.op = DP_LSL;
  insn.rd = 0;
  insn.rn = 0;
  insn.rm = 1;
  insn.set_flags = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0u, exec.cpu.r[0]);
  CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_C); /* Carry = bit 0 of original */
}

TEST(DataProcCoverage, LslBy33) {
  exec.cpu.r[0] = 0xFFFFFFFF;
  exec.cpu.r[1] = 33;
  insn.type = INSN_DATA_PROC_REG;
  insn.op = DP_LSL;
  insn.rd = 0;
  insn.rn = 0;
  insn.rm = 1;
  insn.set_flags = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0u, exec.cpu.r[0]);
  CHECK_FALSE(exec.cpu.xpsr & ARMV8M_XPSR_C);
}

TEST(DataProcCoverage, LsrBy32) {
  exec.cpu.r[0] = 0x80000000;
  exec.cpu.r[1] = 32;
  insn.type = INSN_DATA_PROC_REG;
  insn.op = DP_LSR;
  insn.rd = 0;
  insn.rn = 0;
  insn.rm = 1;
  insn.set_flags = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0u, exec.cpu.r[0]);
  CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_C); /* Carry = bit 31 of original */
}

TEST(DataProcCoverage, LsrBy33) {
  exec.cpu.r[0] = 0xFFFFFFFF;
  exec.cpu.r[1] = 33;
  insn.type = INSN_DATA_PROC_REG;
  insn.op = DP_LSR;
  insn.rd = 0;
  insn.rn = 0;
  insn.rm = 1;
  insn.set_flags = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0u, exec.cpu.r[0]);
  CHECK_FALSE(exec.cpu.xpsr & ARMV8M_XPSR_C);
}

TEST(DataProcCoverage, AsrBy32) {
  exec.cpu.r[0] = 0x80000000;
  exec.cpu.r[1] = 32;
  insn.type = INSN_DATA_PROC_REG;
  insn.op = DP_ASR;
  insn.rd = 0;
  insn.rn = 0;
  insn.rm = 1;
  insn.set_flags = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0xFFFFFFFFu, exec.cpu.r[0]); /* Sign extended */
  CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_C);
}

TEST(DataProcCoverage, AsrBy33) {
  exec.cpu.r[0] = 0x80000000;
  exec.cpu.r[1] = 33;
  insn.type = INSN_DATA_PROC_REG;
  insn.op = DP_ASR;
  insn.rd = 0;
  insn.rn = 0;
  insn.rm = 1;
  insn.set_flags = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0xFFFFFFFFu, exec.cpu.r[0]);
}

TEST(DataProcCoverage, RorBy32) {
  exec.cpu.r[0] = 0x80000001;
  exec.cpu.r[1] = 32;
  exec.cpu.xpsr &= ~ARMV8M_XPSR_C;
  insn.type = INSN_DATA_PROC_REG;
  insn.op = DP_ROR;
  insn.rd = 0;
  insn.rn = 0;
  insn.rm = 1;
  insn.set_flags = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x80000001u, exec.cpu.r[0]);   /* Full rotation */
  CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_C); /* Carry = bit 31 */
}

/* MLA - multiply-accumulate */
TEST(DataProcCoverage, Mla) {
  exec.cpu.r[0] = 100; /* Accumulator */
  exec.cpu.r[1] = 5;
  exec.cpu.r[2] = 10;
  insn.type = INSN_MULTIPLY;
  insn.op = MUL_MLA;
  insn.rd = 3; /* Destination */
  insn.rn = 1; /* First multiplicand */
  insn.rm = 2; /* Second multiplicand */
  insn.rs = 0; /* Accumulator (Ra) */

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(150u, exec.cpu.r[3]); /* 5 * 10 + 100 */
}

TEST(DataProcCoverage, Mls) {
  exec.cpu.r[0] = 100; /* Minuend */
  exec.cpu.r[1] = 5;
  exec.cpu.r[2] = 10;
  insn.type = INSN_MULTIPLY;
  insn.op = MUL_MLS;
  insn.rd = 3; /* Destination */
  insn.rn = 1; /* First multiplicand */
  insn.rm = 2; /* Second multiplicand */
  insn.rs = 0; /* Ra */

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(50u, exec.cpu.r[3]); /* 100 - 5 * 10 */
}

/* Extend with add */
TEST(DataProcCoverage, SxtabWithAdd) {
  exec.cpu.r[1] = 0x100;
  exec.cpu.r[2] = 0x80; /* -128 as signed byte */
  insn.type = INSN_EXTEND;
  insn.rd = 0;
  insn.rn = 1;
  insn.rm = 2;
  insn.access_size = ACCESS_BYTE;
  insn.is_signed = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x100u - 128u, exec.cpu.r[0]); /* 0x100 + (-128) */
}

TEST(DataProcCoverage, UxtahWithAdd) {
  exec.cpu.r[1] = 0x1000;
  exec.cpu.r[2] = 0x1234;
  insn.type = INSN_EXTEND;
  insn.rd = 0;
  insn.rn = 1;
  insn.rm = 2;
  insn.access_size = ACCESS_HALF;
  insn.is_signed = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x1000u + 0x1234u, exec.cpu.r[0]);
}

/* PC write in data processing */
TEST(DataProcCoverage, MovToPc) {
  exec.cpu.r[1] = 0x1001; /* Target with Thumb bit */
  insn.type = INSN_DATA_PROC_REG;
  insn.op = DP_MOV;
  insn.rd = 15; /* PC */
  insn.rm = 1;
  insn.set_flags = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x1000u, exec.cpu.r[15]); /* Thumb bit cleared */
}

/* LSR with immediate 0 (encoded as LSR #32) */
TEST(DataProcCoverage, LsrImmZero) {
  exec.cpu.r[0] = 0x80000000;
  insn.type = INSN_DATA_PROC_SHIFTED;
  insn.op = DP_MOV;
  insn.rd = 1;
  insn.rm = 0;
  insn.shift_type = SHIFT_LSR;
  insn.shift_amount = 0; /* Encoded as 32 */
  insn.set_flags = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0u, exec.cpu.r[1]);
  CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_C); /* Carry = bit 31 */
}

/* ASR with immediate 0 (encoded as ASR #32) */
TEST(DataProcCoverage, AsrImmZero) {
  exec.cpu.r[0] = 0x80000000; /* Negative */
  insn.type = INSN_DATA_PROC_SHIFTED;
  insn.op = DP_MOV;
  insn.rd = 1;
  insn.rm = 0;
  insn.shift_type = SHIFT_ASR;
  insn.shift_amount = 0; /* Encoded as 32 */
  insn.set_flags = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0xFFFFFFFFu, exec.cpu.r[1]); /* Sign extended */
  CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_C);
}

/* Data processing with SP as source */
TEST(DataProcCoverage, AddFromSp) {
  exec.cpu.sp_main = 0x20001000;
  exec.cpu.r[13] = 0x20001000;
  exec.cpu.r[1] = 0x100;
  insn.type = INSN_DATA_PROC_REG;
  insn.op = DP_ADD;
  insn.rd = 0;
  insn.rn = 13; /* SP */
  insn.rm = 1;
  insn.set_flags = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x20001100u, exec.cpu.r[0]);
}

/* Data processing with SP as destination */
TEST(DataProcCoverage, AddToSp) {
  exec.cpu.sp_main = 0x20001000;
  exec.cpu.r[13] = 0x20001000;
  exec.cpu.mode = MODE_THREAD;
  exec.cpu.control = 0; /* Use MSP */
  insn.type = INSN_DATA_PROC_IMM;
  insn.op = DP_ADD;
  insn.rd = 13; /* SP */
  insn.rn = 13; /* SP */
  insn.imm = 0x100;
  insn.set_flags = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x20001100u, exec.cpu.sp_main);
}

/*============================================================================
 * Test Group: Additional Branch Coverage
 *============================================================================*/

TEST_GROUP(BranchCoverage) {
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

TEST(BranchCoverage, BxExceptionReturn) {
  /* Set up for exception return */
  exec.cpu.mode = MODE_HANDLER;
  exec.cpu.current_exception = 11; /* SVCall */
  exec.cpu.sp_main = 0x100;
  exec.cpu.r[13] = 0x100;

  /* Set up stacked context */
  mock_memory[0x100] = 0x00;
  mock_memory[0x101] = 0x00;
  mock_memory[0x102] = 0x00;
  mock_memory[0x103] = 0x00; /* R0 */
  mock_memory[0x104] = 0x00;
  mock_memory[0x105] = 0x00;
  mock_memory[0x106] = 0x00;
  mock_memory[0x107] = 0x00; /* R1 */
  mock_memory[0x108] = 0x00;
  mock_memory[0x109] = 0x00;
  mock_memory[0x10A] = 0x00;
  mock_memory[0x10B] = 0x00; /* R2 */
  mock_memory[0x10C] = 0x00;
  mock_memory[0x10D] = 0x00;
  mock_memory[0x10E] = 0x00;
  mock_memory[0x10F] = 0x00; /* R3 */
  mock_memory[0x110] = 0x00;
  mock_memory[0x111] = 0x00;
  mock_memory[0x112] = 0x00;
  mock_memory[0x113] = 0x00; /* R12 */
  mock_memory[0x114] = 0x00;
  mock_memory[0x115] = 0x00;
  mock_memory[0x116] = 0x00;
  mock_memory[0x117] = 0x00; /* LR */
  mock_memory[0x118] = 0x01;
  mock_memory[0x119] = 0x10;
  mock_memory[0x11A] = 0x00;
  mock_memory[0x11B] = 0x00; /* Return PC = 0x1001 */
  mock_memory[0x11C] = 0x00;
  mock_memory[0x11D] = 0x00;
  mock_memory[0x11E] = 0x00;
  mock_memory[0x11F] = 0x01; /* xPSR with T bit */

  exec.cpu.r[0] = 0xFFFFFFF9; /* EXC_RETURN: return to Thread mode, MSP */
  insn.type = INSN_BRANCH_EXCHANGE;
  insn.rm = 0;

  int result = armv8m_exec_insn(&exec, &insn);
  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_EQUAL(MODE_THREAD, exec.cpu.mode);
}

TEST(BranchCoverage, BlxExceptionReturn) {
  exec.cpu.mode = MODE_HANDLER;
  exec.cpu.current_exception = 11;
  exec.cpu.sp_main = 0x100;
  exec.cpu.r[13] = 0x100;
  exec.cpu.r[15] = 0x200;

  /* Set up stacked context */
  mock_memory[0x100] = 0x00;
  mock_memory[0x101] = 0x00;
  mock_memory[0x102] = 0x00;
  mock_memory[0x103] = 0x00;
  mock_memory[0x104] = 0x00;
  mock_memory[0x105] = 0x00;
  mock_memory[0x106] = 0x00;
  mock_memory[0x107] = 0x00;
  mock_memory[0x108] = 0x00;
  mock_memory[0x109] = 0x00;
  mock_memory[0x10A] = 0x00;
  mock_memory[0x10B] = 0x00;
  mock_memory[0x10C] = 0x00;
  mock_memory[0x10D] = 0x00;
  mock_memory[0x10E] = 0x00;
  mock_memory[0x10F] = 0x00;
  mock_memory[0x110] = 0x00;
  mock_memory[0x111] = 0x00;
  mock_memory[0x112] = 0x00;
  mock_memory[0x113] = 0x00;
  mock_memory[0x114] = 0x00;
  mock_memory[0x115] = 0x00;
  mock_memory[0x116] = 0x00;
  mock_memory[0x117] = 0x00;
  mock_memory[0x118] = 0x01;
  mock_memory[0x119] = 0x10;
  mock_memory[0x11A] = 0x00;
  mock_memory[0x11B] = 0x00;
  mock_memory[0x11C] = 0x00;
  mock_memory[0x11D] = 0x00;
  mock_memory[0x11E] = 0x00;
  mock_memory[0x11F] = 0x01;

  exec.cpu.r[0] = 0xFFFFFFF9;
  insn.type = INSN_BRANCH_LINK_EXCHANGE;
  insn.rm = 0;
  insn.size = 2;

  int result = armv8m_exec_insn(&exec, &insn);
  CHECK_EQUAL(ARMV8M_OK, result);
  CHECK_EQUAL(MODE_THREAD, exec.cpu.mode);
}

/* BX with SP as source register (covers get_reg SP path in exec_branch.c) */
TEST(BranchCoverage, BxFromSp) {
  exec.cpu.sp_main = 0x1001; /* Target address with thumb bit */
  exec.cpu.r[13] = 0x1001;
  exec.cpu.r[15] = 0x2000;
  insn.type = INSN_BRANCH_EXCHANGE;
  insn.rm = 13; /* SP */

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x1000u, exec.cpu.r[15]); /* Thumb bit cleared */
}

/* TBB with no memory read function (covers mem_read fault path) */
TEST(BranchCoverage, TbbNoMemRead) {
  exec.mem.read = NULL; /* No read function */
  exec.cpu.r[0] = 0x1000;
  exec.cpu.r[1] = 0; /* Index */
  exec.cpu.r[15] = 0x2000;
  insn.type = INSN_TABLE_BRANCH;
  insn.rn = 0;
  insn.rm = 1;
  insn.access_size = ACCESS_BYTE; /* TBB */

  /* Should return bus fault error */
  CHECK_EQUAL(ARMV8M_ERR_BUS_FAULT, armv8m_exec_insn(&exec, &insn));
}

/*============================================================================
 * Test Group: Additional Executor Coverage
 *============================================================================*/
