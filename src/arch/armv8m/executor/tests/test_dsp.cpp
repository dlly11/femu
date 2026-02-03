/**
 * @file test_dsp.cpp
 * @brief DSP instruction tests for ARMv8-M executor
 *
 * Tests for saturating arithmetic, parallel operations, pack halfword,
 * and GE flag updates.
 */

#include "test_common.h"

/*============================================================================
 * Test Group: DSP Saturating Arithmetic (QADD, QSUB, QDADD, QDSUB)
 *============================================================================*/

TEST_GROUP(DSP_SatArith) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_dsp = true;
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

TEST(DSP_SatArith, QADD_NoSaturation) {
  exec.cpu.r[0] = 100; /* Rm */
  exec.cpu.r[1] = 200; /* Rn */
  insn.type = INSN_SAT_ARITH;
  insn.op = 0; /* QADD */
  insn.rd = 2;
  insn.rm = 0;
  insn.rn = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(300u, exec.cpu.r[2]);
  CHECK_FALSE(exec.cpu.xpsr & ARMV8M_XPSR_Q);
}

TEST(DSP_SatArith, QADD_PositiveOverflow) {
  exec.cpu.r[0] = 0x7FFFFFFF; /* INT32_MAX */
  exec.cpu.r[1] = 1;
  insn.type = INSN_SAT_ARITH;
  insn.op = 0; /* QADD */
  insn.rd = 2;
  insn.rm = 0;
  insn.rn = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x7FFFFFFFu, exec.cpu.r[2]); /* Saturated to INT32_MAX */
  CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_Q);
}

TEST(DSP_SatArith, QADD_NegativeOverflow) {
  exec.cpu.r[0] = 0x80000000; /* INT32_MIN */
  exec.cpu.r[1] = 0xFFFFFFFF; /* -1 */
  insn.type = INSN_SAT_ARITH;
  insn.op = 0; /* QADD */
  insn.rd = 2;
  insn.rm = 0;
  insn.rn = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x80000000u, exec.cpu.r[2]); /* Saturated to INT32_MIN */
  CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_Q);
}

TEST(DSP_SatArith, QSUB_NoSaturation) {
  exec.cpu.r[0] = 300; /* Rm */
  exec.cpu.r[1] = 100; /* Rn */
  insn.type = INSN_SAT_ARITH;
  insn.op = 2; /* QSUB */
  insn.rd = 2;
  insn.rm = 0;
  insn.rn = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(200u, exec.cpu.r[2]);
  CHECK_FALSE(exec.cpu.xpsr & ARMV8M_XPSR_Q);
}

TEST(DSP_SatArith, QSUB_PositiveOverflow) {
  exec.cpu.r[0] = 0x7FFFFFFF; /* INT32_MAX */
  exec.cpu.r[1] = 0x80000000; /* INT32_MIN - subtracting this overflows */
  insn.type = INSN_SAT_ARITH;
  insn.op = 2; /* QSUB */
  insn.rd = 2;
  insn.rm = 0;
  insn.rn = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x7FFFFFFFu, exec.cpu.r[2]); /* Saturated to INT32_MAX */
  CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_Q);
}

TEST(DSP_SatArith, QSUB_NegativeOverflow) {
  exec.cpu.r[0] = 0x80000000; /* INT32_MIN */
  exec.cpu.r[1] = 1;
  insn.type = INSN_SAT_ARITH;
  insn.op = 2; /* QSUB */
  insn.rd = 2;
  insn.rm = 0;
  insn.rn = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x80000000u, exec.cpu.r[2]); /* Saturated to INT32_MIN */
  CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_Q);
}

TEST(DSP_SatArith, QDADD_NoSaturation) {
  /* QDADD: Rd = SAT(Rm + SAT(Rn * 2)) */
  exec.cpu.r[0] = 100; /* Rm */
  exec.cpu.r[1] = 50;  /* Rn */
  insn.type = INSN_SAT_ARITH;
  insn.op = 1; /* QDADD */
  insn.rd = 2;
  insn.rm = 0;
  insn.rn = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(200u, exec.cpu.r[2]); /* 100 + (50 * 2) */
  CHECK_FALSE(exec.cpu.xpsr & ARMV8M_XPSR_Q);
}

TEST(DSP_SatArith, QDADD_DoubleSaturates) {
  /* QDADD: Rd = SAT(Rm + SAT(Rn * 2)) */
  exec.cpu.r[0] = 0;
  exec.cpu.r[1] = 0x40000001; /* Doubling this overflows */
  insn.type = INSN_SAT_ARITH;
  insn.op = 1; /* QDADD */
  insn.rd = 2;
  insn.rm = 0;
  insn.rn = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x7FFFFFFFu, exec.cpu.r[2]); /* Double saturated to INT32_MAX */
  CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_Q);
}

TEST(DSP_SatArith, QDSUB_NoSaturation) {
  /* QDSUB: Rd = SAT(Rm - SAT(Rn * 2)) */
  exec.cpu.r[0] = 200; /* Rm */
  exec.cpu.r[1] = 50;  /* Rn */
  insn.type = INSN_SAT_ARITH;
  insn.op = 3; /* QDSUB */
  insn.rd = 2;
  insn.rm = 0;
  insn.rn = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(100u, exec.cpu.r[2]); /* 200 - (50 * 2) */
  CHECK_FALSE(exec.cpu.xpsr & ARMV8M_XPSR_Q);
}

TEST(DSP_SatArith, QFlagIsSticky) {
  /* Set Q flag with first operation */
  exec.cpu.r[0] = 0x7FFFFFFF;
  exec.cpu.r[1] = 1;
  insn.type = INSN_SAT_ARITH;
  insn.op = 0; /* QADD */
  insn.rd = 2;
  insn.rm = 0;
  insn.rn = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_Q);

  /* Non-saturating operation should not clear Q flag */
  exec.cpu.r[0] = 10;
  exec.cpu.r[1] = 20;
  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_Q);
}

/*============================================================================
 * Test Group: DSP Parallel Operations (SADD16, SSUB8, UADD16, etc.)
 *============================================================================*/

TEST_GROUP(DSP_Parallel) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_dsp = true;
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

TEST(DSP_Parallel, SADD16_Basic) {
  /* SADD16: parallel add of two signed 16-bit values */
  exec.cpu.r[0] = 0x00100020; /* [16, 32] */
  exec.cpu.r[1] = 0x00050010; /* [5, 16] */
  insn.type = INSN_PARALLEL;
  insn.op = 0x80; /* Signed regular 16-bit add: is_16bit=1, type=0, subop=0 */
  insn.rd = 2;
  insn.rn = 0;
  insn.rm = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x00150030u, exec.cpu.r[2]); /* [21, 48] */
}

TEST(DSP_Parallel, SSUB16_Basic) {
  /* SSUB16: parallel subtract of two signed 16-bit values */
  exec.cpu.r[0] = 0x00200020; /* [32, 32] */
  exec.cpu.r[1] = 0x00050010; /* [5, 16] */
  insn.type = INSN_PARALLEL;
  insn.op = 0x82; /* Signed regular 16-bit sub: is_16bit=1, type=0, subop=2 */
  insn.rd = 2;
  insn.rn = 0;
  insn.rm = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x001B0010u, exec.cpu.r[2]); /* [27, 16] */
}

TEST(DSP_Parallel, SADD8_Basic) {
  /* SADD8: parallel add of four signed 8-bit values */
  exec.cpu.r[0] = 0x10203040; /* [16, 32, 48, 64] */
  exec.cpu.r[1] = 0x01020304; /* [1, 2, 3, 4] */
  insn.type = INSN_PARALLEL;
  insn.op = 0x00; /* Signed regular 8-bit add: is_16bit=0, type=0, subop=0 */
  insn.rd = 2;
  insn.rn = 0;
  insn.rm = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x11223344u, exec.cpu.r[2]); /* [17, 34, 51, 68] */
}

TEST(DSP_Parallel, UADD16_Basic) {
  /* UADD16: parallel add of two unsigned 16-bit values */
  exec.cpu.r[0] = 0xFF000100; /* [255<<8, 256] */
  exec.cpu.r[1] = 0x01000100; /* [256, 256] */
  insn.type = INSN_PARALLEL;
  insn.op = 0xC0; /* Unsigned regular 16-bit add: is_16bit=1, type=4, subop=0 */
  insn.rd = 2;
  insn.rn = 0;
  insn.rm = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  /* Low: 0x0100 + 0x0100 = 0x0200, High: 0xFF00 + 0x0100 = 0x0000 (with carry)
   */
}

TEST(DSP_Parallel, SEL_SelectBytes) {
  /* SEL: select bytes based on GE flags */
  exec.cpu.r[0] = 0xAABBCCDD; /* Selected if GE bit set */
  exec.cpu.r[1] = 0x11223344; /* Selected if GE bit clear */

  /* Set GE[0] and GE[2] (select bytes 0 and 2 from Rn)
   * GE = 0x5 = 0b0101, so GE[0]=1, GE[1]=0, GE[2]=1, GE[3]=0
   */
  exec.cpu.xpsr =
      (exec.cpu.xpsr & ~ARMV8M_XPSR_GE_MASK) | (0x5 << ARMV8M_XPSR_GE_SHIFT);

  insn.type = INSN_PARALLEL;
  insn.op = 0xFF; /* SEL */
  insn.rd = 2;
  insn.rn = 0;
  insn.rm = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  /* Byte 0 from Rn (DD), byte 1 from Rm (33), byte 2 from Rn (BB), byte 3 from
   * Rm (11) Result: 0x11BB33DD */
  CHECK_EQUAL(0x11BB33DDu, exec.cpu.r[2]);
}

TEST(DSP_Parallel, SEL_AllFromRn) {
  exec.cpu.r[0] = 0xAABBCCDD;
  exec.cpu.r[1] = 0x11223344;

  /* Set all GE flags (select all from Rn) */
  exec.cpu.xpsr =
      (exec.cpu.xpsr & ~ARMV8M_XPSR_GE_MASK) | (0xF << ARMV8M_XPSR_GE_SHIFT);

  insn.type = INSN_PARALLEL;
  insn.op = 0xFF; /* SEL */
  insn.rd = 2;
  insn.rn = 0;
  insn.rm = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0xAABBCCDDu, exec.cpu.r[2]);
}

TEST(DSP_Parallel, SEL_AllFromRm) {
  exec.cpu.r[0] = 0xAABBCCDD;
  exec.cpu.r[1] = 0x11223344;

  /* Clear all GE flags (select all from Rm) */
  exec.cpu.xpsr &= ~ARMV8M_XPSR_GE_MASK;

  insn.type = INSN_PARALLEL;
  insn.op = 0xFF; /* SEL */
  insn.rd = 2;
  insn.rn = 0;
  insn.rm = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x11223344u, exec.cpu.r[2]);
}

/*============================================================================
 * Test Group: DSP Pack Halfword (PKHBT, PKHTB)
 *============================================================================*/

TEST_GROUP(DSP_Pack) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_dsp = true;
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

TEST(DSP_Pack, PKHBT_NoShift) {
  /* PKHBT: Rd[15:0] = Rn[15:0], Rd[31:16] = (Rm << 0)[15:0] */
  exec.cpu.r[0] = 0xAABBCCDD; /* Rn */
  exec.cpu.r[1] = 0x11223344; /* Rm */
  insn.type = INSN_PACK;
  insn.op = 0; /* PKHBT */
  insn.rd = 2;
  insn.rn = 0;
  insn.rm = 1;
  insn.shift_type = SHIFT_LSL;
  insn.shift_amount = 0;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  /* Rd[15:0] = 0xCCDD, Rd[31:16] = Rm[15:0] = 0x3344 */
  CHECK_EQUAL(0x3344CCDDu, exec.cpu.r[2]);
}

TEST(DSP_Pack, PKHBT_WithShift) {
  /* PKHBT: Rd[15:0] = Rn[15:0], Rd[31:16] = (Rm << shift)[15:0] */
  exec.cpu.r[0] = 0xAABBCCDD; /* Rn */
  exec.cpu.r[1] = 0x00112233; /* Rm */
  insn.type = INSN_PACK;
  insn.op = 0; /* PKHBT */
  insn.rd = 2;
  insn.rn = 0;
  insn.rm = 1;
  insn.shift_type = SHIFT_LSL;
  insn.shift_amount = 8;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  /* Rm << 8 = 0x11223300, take LOW half = 0x3300 */
  CHECK_EQUAL(0x3300CCDDu, exec.cpu.r[2]);
}

TEST(DSP_Pack, PKHTB_NoShift) {
  /* PKHTB: Rd[15:0] = Rm[15:0], Rd[31:16] = Rn[31:16] */
  exec.cpu.r[0] = 0xAABBCCDD; /* Rn */
  exec.cpu.r[1] = 0x11223344; /* Rm */
  insn.type = INSN_PACK;
  insn.op = 1; /* PKHTB */
  insn.rd = 2;
  insn.rn = 0;
  insn.rm = 1;
  insn.shift_type = SHIFT_ASR;
  insn.shift_amount = 0;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0xAABB3344u, exec.cpu.r[2]);
}

TEST(DSP_Pack, PKHTB_WithShift) {
  /* PKHTB: Rd[15:0] = (Rm ASR shift)[15:0], Rd[31:16] = Rn[31:16] */
  exec.cpu.r[0] = 0xAABBCCDD; /* Rn */
  exec.cpu.r[1] = 0x11223300; /* Rm */
  insn.type = INSN_PACK;
  insn.op = 1; /* PKHTB */
  insn.rd = 2;
  insn.rn = 0;
  insn.rm = 1;
  insn.shift_type = SHIFT_ASR;
  insn.shift_amount = 8;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  /* Rm >> 8 = 0x00112233, take low half = 0x2233 */
  CHECK_EQUAL(0xAABB2233u, exec.cpu.r[2]);
}

/*============================================================================
 * Test Group: DSP GE Flags
 *============================================================================*/

TEST_GROUP(DSP_GEFlags) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_dsp = true;
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

  uint32_t get_ge_flags() {
    return (exec.cpu.xpsr >> ARMV8M_XPSR_GE_SHIFT) & 0xF;
  }
};

TEST(DSP_GEFlags, SADD16_SetsGEFlags) {
  /* GE[1:0] set if low half result >= 0, GE[3:2] set if high half result >= 0
   */
  exec.cpu.r[0] = 0x00010001; /* [1, 1] */
  exec.cpu.r[1] = 0x00010001; /* [1, 1] */
  insn.type = INSN_PARALLEL;
  insn.op = 0x80; /* Signed regular 16-bit add: is_16bit=1, type=0, subop=0 */
  insn.rd = 2;
  insn.rn = 0;
  insn.rm = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0xFu, get_ge_flags()); /* All results positive */
}

TEST(DSP_GEFlags, SSUB16_ClearsGEFlags) {
  /* Subtracting larger values results in negative */
  exec.cpu.r[0] = 0x00010001; /* [1, 1] */
  exec.cpu.r[1] = 0x00020002; /* [2, 2] */
  insn.type = INSN_PARALLEL;
  insn.op = 0x82; /* Signed regular 16-bit sub: is_16bit=1, type=0, subop=2 */
  insn.rd = 2;
  insn.rn = 0;
  insn.rm = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x0u, get_ge_flags()); /* All results negative */
}

TEST(DSP_GEFlags, SADD8_SetsGEFlagsPerByte) {
  /* GE[n] set if byte n result >= 0 */
  exec.cpu.r[0] = 0x01010101; /* [1, 1, 1, 1] */
  exec.cpu.r[1] = 0x01010101; /* [1, 1, 1, 1] */
  insn.type = INSN_PARALLEL;
  insn.op = 0x00; /* Signed regular 8-bit add: is_16bit=0, type=0, subop=0 */
  insn.rd = 2;
  insn.rn = 0;
  insn.rm = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0xFu, get_ge_flags()); /* All results positive */
}

TEST(DSP_GEFlags, SADD8_MixedResults) {
  /* Mix of positive and negative results */
  exec.cpu.r[0] = 0x7F0101FF; /* [127, 1, 1, -1] */
  exec.cpu.r[1] = 0x01010101; /* [1, 1, 1, 1] */
  insn.type = INSN_PARALLEL;
  insn.op = 0x00; /* Signed regular 8-bit add: is_16bit=0, type=0, subop=0 */
  insn.rd = 2;
  insn.rn = 0;
  insn.rm = 1;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  /* Byte 0: -1 + 1 = 0 (>= 0), Byte 1: 1 + 1 = 2, Byte 2: 1 + 1 = 2, Byte 3:
   * 127 + 1 = -128 (< 0) */
  CHECK_EQUAL(0x7u, get_ge_flags()); /* GE[2:0] set, GE[3] clear */
}
