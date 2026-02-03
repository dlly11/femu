/**
 * @file test_fpu.cpp
 * @brief FPU instruction tests for ARMv8-M executor
 *
 * Tests for VFP load/store, move, arithmetic, compare, convert operations,
 * and edge cases (NaN, infinity, denormals).
 */

#include "test_common.h"
#include <cmath>

/* VFP operation codes (from exec_fpu.c) */
#define VFP_VMLA 0x00
#define VFP_VMLS 0x01
#define VFP_VNMLA 0x02
#define VFP_VNMLS 0x03
#define VFP_VNMUL 0x04
#define VFP_VMUL 0x05
#define VFP_VADD 0x06
#define VFP_VSUB 0x07
#define VFP_VDIV 0x08
#define VFP_VABS 0x10
#define VFP_VNEG 0x11
#define VFP_VSQRT 0x12

#define VFP_VMOV_IMM 0x00
#define VFP_VMOV_REG 0x01
#define VFP_VMOV_ARM 0x02
#define VFP_VMOV_2ARM 0x03

/* Helper to convert float to uint32 */
static inline uint32_t float_to_bits(float f) {
  union {
    float f;
    uint32_t i;
  } u;
  u.f = f;
  return u.i;
}

/* Helper to convert uint32 to float */
static inline float bits_to_float(uint32_t i) {
  union {
    float f;
    uint32_t i;
  } u;
  u.i = i;
  return u.f;
}

/* Helper to convert double to uint64 */
static inline uint64_t double_to_bits(double d) {
  union {
    double d;
    uint64_t i;
  } u;
  u.d = d;
  return u.i;
}

/*============================================================================
 * Test Group: FPU Load/Store (VLDR, VSTR)
 *============================================================================*/

TEST_GROUP(FPU_LoadStore) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_fpu = true;
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

TEST(FPU_LoadStore, VLDR_Single) {
  /* Store a float value in memory */
  float value = 3.14159f;
  uint32_t bits = float_to_bits(value);
  mock_memory[0x100] = (uint8_t)(bits & 0xFF);
  mock_memory[0x101] = (uint8_t)((bits >> 8) & 0xFF);
  mock_memory[0x102] = (uint8_t)((bits >> 16) & 0xFF);
  mock_memory[0x103] = (uint8_t)((bits >> 24) & 0xFF);

  exec.cpu.r[0] = 0x100;
  insn.type = INSN_FPU_LOAD;
  insn.rn = 0;
  insn.sd = 4;
  insn.imm = 0;
  insn.add = true;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(bits, exec.cpu.s[4]);
  CHECK_TRUE(exec.cpu.control & ARMV8M_CONTROL_FPCA);
}

TEST(FPU_LoadStore, VLDR_SingleWithOffset) {
  float value = 2.71828f;
  uint32_t bits = float_to_bits(value);
  mock_memory[0x108] = (uint8_t)(bits & 0xFF);
  mock_memory[0x109] = (uint8_t)((bits >> 8) & 0xFF);
  mock_memory[0x10A] = (uint8_t)((bits >> 16) & 0xFF);
  mock_memory[0x10B] = (uint8_t)((bits >> 24) & 0xFF);

  exec.cpu.r[0] = 0x100;
  insn.type = INSN_FPU_LOAD;
  insn.rn = 0;
  insn.sd = 5;
  insn.imm = 8;
  insn.add = true;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(bits, exec.cpu.s[5]);
}

TEST(FPU_LoadStore, VLDR_NegativeOffset) {
  float value = 1.5f;
  uint32_t bits = float_to_bits(value);
  mock_memory[0xF8] = (uint8_t)(bits & 0xFF);
  mock_memory[0xF9] = (uint8_t)((bits >> 8) & 0xFF);
  mock_memory[0xFA] = (uint8_t)((bits >> 16) & 0xFF);
  mock_memory[0xFB] = (uint8_t)((bits >> 24) & 0xFF);

  exec.cpu.r[0] = 0x100;
  insn.type = INSN_FPU_LOAD;
  insn.rn = 0;
  insn.sd = 6;
  insn.imm = 8;
  insn.add = false; /* Subtract offset */
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(bits, exec.cpu.s[6]);
}

TEST(FPU_LoadStore, VSTR_Single) {
  float value = 42.0f;
  exec.cpu.s[3] = float_to_bits(value);
  exec.cpu.r[0] = 0x200;

  insn.type = INSN_FPU_STORE;
  insn.rn = 0;
  insn.sd = 3;
  insn.imm = 0;
  insn.add = true;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));

  uint32_t stored = mock_memory[0x200] | ((uint32_t)mock_memory[0x201] << 8) |
                    ((uint32_t)mock_memory[0x202] << 16) |
                    ((uint32_t)mock_memory[0x203] << 24);
  CHECK_EQUAL(float_to_bits(value), stored);
}

TEST(FPU_LoadStore, VLDR_Double) {
  double value = 3.141592653589793;
  uint64_t bits = double_to_bits(value);
  for (int i = 0; i < 8; i++) {
    mock_memory[0x100 + i] = (uint8_t)((bits >> (i * 8)) & 0xFF);
  }

  exec.cpu.r[0] = 0x100;
  insn.type = INSN_FPU_LOAD;
  insn.rn = 0;
  insn.dd = 2;
  insn.sd = 4; /* D2 uses S4 and S5 */
  insn.imm = 0;
  insn.add = true;
  insn.is_double = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL((uint32_t)(bits & 0xFFFFFFFF), exec.cpu.s[4]);
  CHECK_EQUAL((uint32_t)(bits >> 32), exec.cpu.s[5]);
}

TEST(FPU_LoadStore, FPU_Disabled_Faults) {
  exec.has_fpu = false;
  exec.cpu.r[0] = 0x100;
  insn.type = INSN_FPU_LOAD;
  insn.rn = 0;
  insn.sd = 0;
  insn.imm = 0;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_ERR_USAGE_FAULT, armv8m_exec_insn(&exec, &insn));
  CHECK_TRUE(exec.cpu.cfsr & ARMV8M_UFSR_NOCP);
}

/*============================================================================
 * Test Group: FPU Multiple Load/Store (VPUSH, VPOP, VLDM, VSTM)
 *============================================================================*/

TEST_GROUP(FPU_Multi) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_fpu = true;
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

TEST(FPU_Multi, VPUSH_SingleRegisters) {
  exec.cpu.s[0] = 0x11111111;
  exec.cpu.s[1] = 0x22222222;
  exec.cpu.s[2] = 0x33333333;
  exec.cpu.s[3] = 0x44444444;
  exec.cpu.r[13] = 0x400; /* SP */
  exec.cpu.sp_main = 0x400;

  insn.type = INSN_FPU_MULTI;
  insn.rn = 13; /* SP */
  insn.sd = 0;
  insn.imm = 4;     /* 4 registers */
  insn.add = false; /* Pre-decrement */
  insn.writeback = true;
  insn.pre_index = true;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));

  /* SP should be decremented by 16 (4 * 4 bytes) */
  CHECK_EQUAL(0x3F0u, exec.cpu.sp_main);
}

TEST(FPU_Multi, VPOP_SingleRegisters) {
  /* Put values in memory */
  for (int i = 0; i < 4; i++) {
    uint32_t val = (uint32_t)(0x10101010 * (i + 1));
    mock_memory[0x300 + i * 4 + 0] = (uint8_t)(val & 0xFF);
    mock_memory[0x300 + i * 4 + 1] = (uint8_t)((val >> 8) & 0xFF);
    mock_memory[0x300 + i * 4 + 2] = (uint8_t)((val >> 16) & 0xFF);
    mock_memory[0x300 + i * 4 + 3] = (uint8_t)((val >> 24) & 0xFF);
  }

  exec.cpu.r[13] = 0x300;
  exec.cpu.sp_main = 0x300;

  insn.type = INSN_FPU_MULTI;
  insn.rn = 13; /* SP */
  insn.sd = 8;
  insn.imm = 4;    /* 4 registers */
  insn.add = true; /* Post-increment */
  insn.writeback = true;
  insn.pre_index = false;
  insn.is_double = false;
  insn.op = 1; /* op=1 for load (VPOP) */

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));

  CHECK_EQUAL(0x10101010u, exec.cpu.s[8]);
  CHECK_EQUAL(0x20202020u, exec.cpu.s[9]);
  CHECK_EQUAL(0x30303030u, exec.cpu.s[10]);
  CHECK_EQUAL(0x40404040u, exec.cpu.s[11]);
  CHECK_EQUAL(0x310u, exec.cpu.sp_main);
}

/*============================================================================
 * Test Group: FPU Move (VMOV variants)
 *============================================================================*/

TEST_GROUP(FPU_Move) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_fpu = true;
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

TEST(FPU_Move, VMOV_Register) {
  exec.cpu.s[5] = float_to_bits(123.456f);

  insn.type = INSN_FPU_MOVE;
  insn.op = VFP_VMOV_REG;
  insn.sd = 10;
  insn.sm = 5;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(exec.cpu.s[5], exec.cpu.s[10]);
}

TEST(FPU_Move, VMOV_ARM_ToFPU) {
  exec.cpu.r[3] = 0xDEADBEEF;

  insn.type = INSN_FPU_MOVE;
  insn.op = VFP_VMOV_ARM;
  insn.rd = 3;
  insn.sn = 7;
  insn.add = false; /* To FPU */
  insn.sysreg = 0;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0xDEADBEEFu, exec.cpu.s[7]);
}

TEST(FPU_Move, VMOV_ARM_FromFPU) {
  exec.cpu.s[12] = 0xCAFEBABE;

  insn.type = INSN_FPU_MOVE;
  insn.op = VFP_VMOV_ARM;
  insn.rd = 5;
  insn.sn = 12;
  insn.add = true; /* From FPU */
  insn.sysreg = 0;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0xCAFEBABEu, exec.cpu.r[5]);
}

TEST(FPU_Move, VMRS_ToAPSR) {
  /* Set FPSCR flags */
  exec.cpu.fpscr = ARMV8M_FPSCR_N | ARMV8M_FPSCR_C;

  insn.type = INSN_FPU_MOVE;
  insn.op = VFP_VMOV_ARM;
  insn.rd = 15;       /* APSR */
  insn.sysreg = 0x80; /* FPSCR */
  insn.add = true;    /* VMRS (to ARM) */

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_N);
  CHECK_TRUE(exec.cpu.xpsr & ARMV8M_XPSR_C);
}

TEST(FPU_Move, VMSR_FromARM) {
  exec.cpu.r[4] = 0x12340000;

  insn.type = INSN_FPU_MOVE;
  insn.op = VFP_VMOV_ARM;
  insn.rd = 4;
  insn.sysreg = 0x80; /* FPSCR */
  insn.add = false;   /* VMSR (from ARM) */

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x12340000u, exec.cpu.fpscr);
}

/*============================================================================
 * Test Group: FPU Arithmetic (VADD, VSUB, VMUL, VDIV, VNEG, VABS, VSQRT)
 *============================================================================*/

TEST_GROUP(FPU_Arith) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_fpu = true;
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

  float get_s(uint8_t reg) { return bits_to_float(exec.cpu.s[reg]); }

  void set_s(uint8_t reg, float val) { exec.cpu.s[reg] = float_to_bits(val); }
};

TEST(FPU_Arith, VADD_Single) {
  set_s(0, 3.5f);
  set_s(1, 2.5f);

  insn.type = INSN_FPU_ARITH;
  insn.op = VFP_VADD;
  insn.sd = 2;
  insn.sn = 0;
  insn.sm = 1;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  DOUBLES_EQUAL(6.0f, get_s(2), 0.001f);
}

TEST(FPU_Arith, VSUB_Single) {
  set_s(0, 10.5f);
  set_s(1, 3.5f);

  insn.type = INSN_FPU_ARITH;
  insn.op = VFP_VSUB;
  insn.sd = 2;
  insn.sn = 0;
  insn.sm = 1;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  DOUBLES_EQUAL(7.0f, get_s(2), 0.001f);
}

TEST(FPU_Arith, VMUL_Single) {
  set_s(0, 4.0f);
  set_s(1, 3.0f);

  insn.type = INSN_FPU_ARITH;
  insn.op = VFP_VMUL;
  insn.sd = 2;
  insn.sn = 0;
  insn.sm = 1;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  DOUBLES_EQUAL(12.0f, get_s(2), 0.001f);
}

TEST(FPU_Arith, VDIV_Single) {
  set_s(0, 15.0f);
  set_s(1, 3.0f);

  insn.type = INSN_FPU_ARITH;
  insn.op = VFP_VDIV;
  insn.sd = 2;
  insn.sn = 0;
  insn.sm = 1;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  DOUBLES_EQUAL(5.0f, get_s(2), 0.001f);
}

TEST(FPU_Arith, VNEG_Single) {
  set_s(1, 42.0f);

  insn.type = INSN_FPU_ARITH;
  insn.op = VFP_VNEG;
  insn.sd = 0;
  insn.sm = 1;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  DOUBLES_EQUAL(-42.0f, get_s(0), 0.001f);
}

TEST(FPU_Arith, VABS_Single) {
  set_s(1, -42.0f);

  insn.type = INSN_FPU_ARITH;
  insn.op = VFP_VABS;
  insn.sd = 0;
  insn.sm = 1;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  DOUBLES_EQUAL(42.0f, get_s(0), 0.001f);
}

TEST(FPU_Arith, VSQRT_Single) {
  set_s(1, 16.0f);

  insn.type = INSN_FPU_ARITH;
  insn.op = VFP_VSQRT;
  insn.sd = 0;
  insn.sm = 1;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  DOUBLES_EQUAL(4.0f, get_s(0), 0.001f);
}

TEST(FPU_Arith, VMLA_Single) {
  set_s(0, 10.0f); /* Accumulator */
  set_s(1, 2.0f);
  set_s(2, 3.0f);

  insn.type = INSN_FPU_ARITH;
  insn.op = VFP_VMLA;
  insn.sd = 0;
  insn.sn = 1;
  insn.sm = 2;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  DOUBLES_EQUAL(16.0f, get_s(0), 0.001f); /* 10 + 2*3 */
}

TEST(FPU_Arith, VMLS_Single) {
  set_s(0, 20.0f); /* Accumulator */
  set_s(1, 2.0f);
  set_s(2, 3.0f);

  insn.type = INSN_FPU_ARITH;
  insn.op = VFP_VMLS;
  insn.sd = 0;
  insn.sn = 1;
  insn.sm = 2;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  DOUBLES_EQUAL(14.0f, get_s(0), 0.001f); /* 20 - 2*3 */
}

/*============================================================================
 * Test Group: FPU Compare (VCMP, VCMPE)
 *============================================================================*/

TEST_GROUP(FPU_Compare) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_fpu = true;
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

  void set_s(uint8_t reg, float val) {
    union {
      float f;
      uint32_t i;
    } u;
    u.f = val;
    exec.cpu.s[reg] = u.i;
  }
};

TEST(FPU_Compare, VCMP_Equal) {
  set_s(0, 5.0f);
  set_s(1, 5.0f);

  insn.type = INSN_FPU_CMP;
  insn.sn = 0;
  insn.sm = 1;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_TRUE(exec.cpu.fpscr & ARMV8M_FPSCR_Z);
  CHECK_TRUE(exec.cpu.fpscr & ARMV8M_FPSCR_C);
  CHECK_FALSE(exec.cpu.fpscr & ARMV8M_FPSCR_N);
  CHECK_FALSE(exec.cpu.fpscr & ARMV8M_FPSCR_V);
}

TEST(FPU_Compare, VCMP_LessThan) {
  set_s(0, 3.0f);
  set_s(1, 5.0f);

  insn.type = INSN_FPU_CMP;
  insn.sn = 0;
  insn.sm = 1;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_TRUE(exec.cpu.fpscr & ARMV8M_FPSCR_N);
  CHECK_FALSE(exec.cpu.fpscr & ARMV8M_FPSCR_Z);
  CHECK_FALSE(exec.cpu.fpscr & ARMV8M_FPSCR_C);
  CHECK_FALSE(exec.cpu.fpscr & ARMV8M_FPSCR_V);
}

TEST(FPU_Compare, VCMP_GreaterThan) {
  set_s(0, 10.0f);
  set_s(1, 5.0f);

  insn.type = INSN_FPU_CMP;
  insn.sn = 0;
  insn.sm = 1;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_FALSE(exec.cpu.fpscr & ARMV8M_FPSCR_N);
  CHECK_FALSE(exec.cpu.fpscr & ARMV8M_FPSCR_Z);
  CHECK_TRUE(exec.cpu.fpscr & ARMV8M_FPSCR_C);
  CHECK_FALSE(exec.cpu.fpscr & ARMV8M_FPSCR_V);
}

TEST(FPU_Compare, VCMP_WithZero) {
  set_s(0, 0.0f);

  insn.type = INSN_FPU_CMP;
  insn.sn = 0;
  insn.rm = ARMV8M_REG_NONE; /* Compare with zero */
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_TRUE(exec.cpu.fpscr & ARMV8M_FPSCR_Z);
}

/*============================================================================
 * Test Group: FPU Conversion (VCVT)
 *============================================================================*/

TEST_GROUP(FPU_Convert) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_fpu = true;
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

TEST(FPU_Convert, VCVT_FloatToSigned) {
  exec.cpu.s[0] = float_to_bits(42.7f);
  exec.cpu.fpscr = (3 << 22); /* Round towards zero (truncate) */

  insn.type = INSN_FPU_CVT;
  insn.op = 2; /* S32.F32 */
  insn.sd = 1;
  insn.sm = 0;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(42u, exec.cpu.s[1]); /* Truncated to 42 */
}

TEST(FPU_Convert, VCVT_FloatToUnsigned) {
  exec.cpu.s[0] = float_to_bits(100.9f);
  exec.cpu.fpscr = (3 << 22); /* Round towards zero (truncate) */

  insn.type = INSN_FPU_CVT;
  insn.op = 3; /* U32.F32 */
  insn.sd = 1;
  insn.sm = 0;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(100u, exec.cpu.s[1]);
}

TEST(FPU_Convert, VCVT_SignedToFloat) {
  exec.cpu.s[0] = (uint32_t)-42; /* Signed integer */

  insn.type = INSN_FPU_CVT;
  insn.op = 4; /* F32.S32 */
  insn.sd = 1;
  insn.sm = 0;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  DOUBLES_EQUAL(-42.0f, bits_to_float(exec.cpu.s[1]), 0.001f);
}

TEST(FPU_Convert, VCVT_UnsignedToFloat) {
  exec.cpu.s[0] = 12345u;

  insn.type = INSN_FPU_CVT;
  insn.op = 5; /* F32.U32 */
  insn.sd = 1;
  insn.sm = 0;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  DOUBLES_EQUAL(12345.0f, bits_to_float(exec.cpu.s[1]), 0.001f);
}

/*============================================================================
 * Test Group: FPU Status (VMRS, VMSR)
 *============================================================================*/

TEST_GROUP(FPU_Status) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_fpu = true;
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

TEST(FPU_Status, VMRS_ReadFPSCR) {
  exec.cpu.fpscr = 0x12345678;

  insn.type = INSN_FPU_MOVE;
  insn.op = VFP_VMOV_ARM;
  insn.rd = 0;
  insn.sysreg = 0x80;
  insn.add = true;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x12345678u, exec.cpu.r[0]);
}

TEST(FPU_Status, VMSR_WriteFPSCR) {
  exec.cpu.r[0] = 0x87654321;

  insn.type = INSN_FPU_MOVE;
  insn.op = VFP_VMOV_ARM;
  insn.rd = 0;
  insn.sysreg = 0x80;
  insn.add = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_EQUAL(0x87654321u, exec.cpu.fpscr);
}

/*============================================================================
 * Test Group: FPU Edge Cases (NaN, Infinity, Denormals)
 *============================================================================*/

TEST_GROUP(FPU_EdgeCases) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_fpu = true;
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

  void set_s(uint8_t reg, float val) { exec.cpu.s[reg] = float_to_bits(val); }
};

TEST(FPU_EdgeCases, VCMP_NaN_Unordered) {
  /* NaN comparison should set V flag (unordered) */
  set_s(0, NAN);
  set_s(1, 5.0f);

  insn.type = INSN_FPU_CMP;
  insn.sn = 0;
  insn.sm = 1;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_TRUE(exec.cpu.fpscr & ARMV8M_FPSCR_V); /* Unordered */
  CHECK_TRUE(exec.cpu.fpscr & ARMV8M_FPSCR_C);
  CHECK_FALSE(exec.cpu.fpscr & ARMV8M_FPSCR_Z);
  CHECK_FALSE(exec.cpu.fpscr & ARMV8M_FPSCR_N);
}

TEST(FPU_EdgeCases, VCMP_BothNaN_Unordered) {
  set_s(0, NAN);
  set_s(1, NAN);

  insn.type = INSN_FPU_CMP;
  insn.sn = 0;
  insn.sm = 1;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_TRUE(exec.cpu.fpscr & ARMV8M_FPSCR_V);
}

TEST(FPU_EdgeCases, VDIV_ByZero) {
  set_s(0, 1.0f);
  set_s(1, 0.0f);

  insn.type = INSN_FPU_ARITH;
  insn.op = VFP_VDIV;
  insn.sd = 2;
  insn.sn = 0;
  insn.sm = 1;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_TRUE(std::isinf(bits_to_float(exec.cpu.s[2])));
}

TEST(FPU_EdgeCases, VSQRT_Negative) {
  set_s(1, -1.0f);

  insn.type = INSN_FPU_ARITH;
  insn.op = VFP_VSQRT;
  insn.sd = 0;
  insn.sm = 1;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_TRUE(std::isnan(bits_to_float(exec.cpu.s[0])));
}

TEST(FPU_EdgeCases, VADD_Infinity) {
  set_s(0, INFINITY);
  set_s(1, 1.0f);

  insn.type = INSN_FPU_ARITH;
  insn.op = VFP_VADD;
  insn.sd = 2;
  insn.sn = 0;
  insn.sm = 1;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_TRUE(std::isinf(bits_to_float(exec.cpu.s[2])));
}

TEST(FPU_EdgeCases, VSUB_InfinityMinusInfinity) {
  set_s(0, INFINITY);
  set_s(1, INFINITY);

  insn.type = INSN_FPU_ARITH;
  insn.op = VFP_VSUB;
  insn.sd = 2;
  insn.sn = 0;
  insn.sm = 1;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  CHECK_TRUE(std::isnan(bits_to_float(exec.cpu.s[2]))); /* Inf - Inf = NaN */
}

/*============================================================================
 * Test Group: FPU Fixed-Point Conversions
 *============================================================================*/

TEST_GROUP(FPU_FixedPoint) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_fpu = true;
    exec.mem.ctx = NULL;
    exec.mem.read = mock_mem_read;
    exec.mem.write = mock_mem_write;
    init_insn(insn);
  }

  void set_s(uint8_t reg, float val) {
    union {
      float f;
      uint32_t i;
    } u;
    u.f = val;
    exec.cpu.s[reg] = u.i;
  }

  float get_s(uint8_t reg) {
    union {
      float f;
      uint32_t i;
    } u;
    u.i = exec.cpu.s[reg];
    return u.f;
  }
};

/* VCVT.F32.FX - Fixed-point to float (signed, 16 fraction bits) */
TEST(FPU_FixedPoint, VCVT_F32_FromSignedFixedQ16) {
  /* Input: 0x00010000 in Q16 format = 1.0 */
  exec.cpu.s[1] = 0x00010000;

  insn.type = INSN_FPU_CVT;
  insn.op = 6; /* Fixed-point to float */
  insn.sd = 0;
  insn.sm = 1;
  insn.imm = 16; /* 32 - 16 = 16 fraction bits */
  insn.is_signed = true;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  DOUBLES_EQUAL(1.0f, get_s(0), 0.0001f);
}

/* VCVT.F32.FX - Fixed-point to float (signed, 16 fraction bits, negative) */
TEST(FPU_FixedPoint, VCVT_F32_FromSignedFixedQ16_Negative) {
  /* Input: 0xFFFF0000 in Q16 format = -1.0 */
  exec.cpu.s[1] = 0xFFFF0000;

  insn.type = INSN_FPU_CVT;
  insn.op = 6; /* Fixed-point to float */
  insn.sd = 0;
  insn.sm = 1;
  insn.imm = 16; /* 16 fraction bits */
  insn.is_signed = true;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  DOUBLES_EQUAL(-1.0f, get_s(0), 0.0001f);
}

/* VCVT.F32.FX - Fixed-point to float (unsigned) */
TEST(FPU_FixedPoint, VCVT_F32_FromUnsignedFixedQ16) {
  /* Input: 0x00020000 in UQ16 format = 2.0 */
  exec.cpu.s[1] = 0x00020000;

  insn.type = INSN_FPU_CVT;
  insn.op = 6; /* Fixed-point to float */
  insn.sd = 0;
  insn.sm = 1;
  insn.imm = 16; /* 16 fraction bits */
  insn.is_signed = false;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  DOUBLES_EQUAL(2.0f, get_s(0), 0.0001f);
}

/* VCVT.FX.F32 - Float to fixed-point (signed, 16 fraction bits) */
TEST(FPU_FixedPoint, VCVT_ToSignedFixedQ16) {
  set_s(1, 1.5f);

  insn.type = INSN_FPU_CVT;
  insn.op = 7; /* Float to fixed-point */
  insn.sd = 0;
  insn.sm = 1;
  insn.imm = 16; /* 16 fraction bits */
  insn.is_signed = true;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  /* 1.5 * 2^16 = 98304 = 0x00018000 */
  CHECK_EQUAL(0x00018000u, exec.cpu.s[0]);
}

/* VCVT.FX.F32 - Float to fixed-point (negative) */
TEST(FPU_FixedPoint, VCVT_ToSignedFixedQ16_Negative) {
  set_s(1, -2.25f);

  insn.type = INSN_FPU_CVT;
  insn.op = 7; /* Float to fixed-point */
  insn.sd = 0;
  insn.sm = 1;
  insn.imm = 16; /* 16 fraction bits */
  insn.is_signed = true;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  /* -2.25 * 2^16 = -147456 = 0xFFFDC000 */
  CHECK_EQUAL(0xFFFDC000u, exec.cpu.s[0]);
}

/* VCVT with different fraction bit counts */
TEST(FPU_FixedPoint, VCVT_Q8_Format) {
  /* Test Q8 format (8 fraction bits) */
  exec.cpu.s[1] = 0x00000180; /* 1.5 in Q8 */

  insn.type = INSN_FPU_CVT;
  insn.op = 6; /* Fixed-point to float */
  insn.sd = 0;
  insn.sm = 1;
  insn.imm = 24; /* 32 - 24 = 8 fraction bits */
  insn.is_signed = true;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  DOUBLES_EQUAL(1.5f, get_s(0), 0.01f);
}

/*============================================================================
 * Test Group: FPU Rounding Modes
 *============================================================================*/

/* FPSCR rounding mode bits [23:22]:
 * 00 = RN (Round to Nearest, ties to even)
 * 01 = RP (Round towards Plus infinity)
 * 10 = RM (Round towards Minus infinity)
 * 11 = RZ (Round towards Zero)
 */
#define FPSCR_RN (0 << 22) /* Round to Nearest */
#define FPSCR_RP (1 << 22) /* Round towards +Inf */
#define FPSCR_RM (2 << 22) /* Round towards -Inf */
#define FPSCR_RZ (3 << 22) /* Round towards Zero */

TEST_GROUP(FPU_Rounding) {
  Executor exec;
  DecodedInsn insn;

  void setup() {
    memset(mock_memory, 0, sizeof(mock_memory));
    armv8m_exec_init(&exec);
    exec.has_fpu = true;
    exec.mem.ctx = NULL;
    exec.mem.read = mock_mem_read;
    exec.mem.write = mock_mem_write;
    init_insn(insn);
  }

  void set_s(uint8_t reg, float val) {
    union {
      float f;
      uint32_t i;
    } u;
    u.f = val;
    exec.cpu.s[reg] = u.i;
  }

  float get_s(uint8_t reg) {
    union {
      float f;
      uint32_t i;
    } u;
    u.i = exec.cpu.s[reg];
    return u.f;
  }
};

/* Test rounding mode affects float-to-int conversion */
TEST(FPU_Rounding, VCVT_ToInt_RoundToNearest) {
  set_s(1, 1.5f);
  exec.cpu.fpscr = FPSCR_RN; /* Round to nearest */

  insn.type = INSN_FPU_CVT;
  insn.op = 2; /* Float to signed int */
  insn.sd = 0;
  insn.sm = 1;
  insn.is_signed = true;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  /* 1.5 rounds to 2 (ties to even) */
  CHECK_EQUAL(2u, exec.cpu.s[0]);
}

TEST(FPU_Rounding, VCVT_ToInt_RoundTowardsPlusInf) {
  set_s(1, 1.1f);
  exec.cpu.fpscr = FPSCR_RP; /* Round towards +infinity */

  insn.type = INSN_FPU_CVT;
  insn.op = 2; /* Float to signed int */
  insn.sd = 0;
  insn.sm = 1;
  insn.is_signed = true;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  /* 1.1 rounds up to 2 */
  CHECK_EQUAL(2u, exec.cpu.s[0]);
}

TEST(FPU_Rounding, VCVT_ToInt_RoundTowardsMinusInf) {
  set_s(1, 1.9f);
  exec.cpu.fpscr = FPSCR_RM; /* Round towards -infinity */

  insn.type = INSN_FPU_CVT;
  insn.op = 2; /* Float to signed int */
  insn.sd = 0;
  insn.sm = 1;
  insn.is_signed = true;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  /* 1.9 rounds down to 1 */
  CHECK_EQUAL(1u, exec.cpu.s[0]);
}

TEST(FPU_Rounding, VCVT_ToInt_RoundTowardsZero) {
  set_s(1, -1.9f);
  exec.cpu.fpscr = FPSCR_RZ; /* Round towards zero */

  insn.type = INSN_FPU_CVT;
  insn.op = 2; /* Float to signed int */
  insn.sd = 0;
  insn.sm = 1;
  insn.is_signed = true;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  /* -1.9 rounds towards zero to -1 */
  CHECK_EQUAL(0xFFFFFFFFu, exec.cpu.s[0]); /* -1 in two's complement */
}

TEST(FPU_Rounding, VCVT_ToInt_Negative_RoundTowardsPlusInf) {
  set_s(1, -1.1f);
  exec.cpu.fpscr = FPSCR_RP; /* Round towards +infinity */

  insn.type = INSN_FPU_CVT;
  insn.op = 2; /* Float to signed int */
  insn.sd = 0;
  insn.sm = 1;
  insn.is_signed = true;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  /* -1.1 rounds up to -1 */
  CHECK_EQUAL(0xFFFFFFFFu, exec.cpu.s[0]); /* -1 */
}

/* Test rounding mode affects fixed-point conversion */
TEST(FPU_Rounding, VCVT_ToFixedPoint_RoundTowardsZero) {
  set_s(1, 1.999f);
  exec.cpu.fpscr = FPSCR_RZ; /* Round towards zero */

  insn.type = INSN_FPU_CVT;
  insn.op = 7; /* Float to fixed-point */
  insn.sd = 0;
  insn.sm = 1;
  insn.imm = 24; /* 8 fraction bits */
  insn.is_signed = true;
  insn.is_double = false;

  CHECK_EQUAL(ARMV8M_OK, armv8m_exec_insn(&exec, &insn));
  /* 1.999 * 256 = 511.744, rounds towards zero to 511 = 0x1FF */
  CHECK_EQUAL(0x000001FFu, exec.cpu.s[0]);
}
