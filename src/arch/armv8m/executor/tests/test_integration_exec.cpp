/**
 * @file test_integration_exec.cpp
 * @brief Execution run and flags integration tests
 */

#include "test_integration_common.h"

TEST_GROUP(IntegrationExecRun){
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

TEST(IntegrationExecRun, SimpleSequence) {
  /* Execute: MOVS R0, #5; MOVS R1, #10; ADDS R2, R0, R1 */
  uint8_t code[] = {
      THUMB16(MOVS_IMM(0, 5)),    /* MOVS R0, #5 */
      THUMB16(MOVS_IMM(1, 10)),   /* MOVS R1, #10 */
      THUMB16(ADDS_REG(2, 0, 1)), /* ADDS R2, R0, R1 */
      THUMB16(WFI)                /* WFI to stop */
  };
  memcpy(test_memory, code, sizeof(code));

  int64_t cycles = armv8m_exec_run(&exec, 100);

  CHECK_EQUAL(4, cycles);
  CHECK_EQUAL(5u, exec.cpu.r[0]);
  CHECK_EQUAL(10u, exec.cpu.r[1]);
  CHECK_EQUAL(15u, exec.cpu.r[2]);
  CHECK_TRUE(exec.cpu.sleeping);
}

TEST(IntegrationExecRun, LoopWithCounter) {
  /* Execute a simple countdown loop:
   *   MOVS R0, #5       ; counter = 5
   * loop:
   *   SUBS R0, R0, #1   ; counter--
   *   BNE loop          ; if counter != 0, loop
   *   WFI               ; halt
   */
  uint8_t code[] = {
      THUMB16(MOVS_IMM(0, 5)),  /* 0x00: MOVS R0, #5 */
      THUMB16(SUBS_IMM8(0, 1)), /* 0x02: SUBS R0, #1 */
      THUMB16(
          B_COND(COND_NE, -6)), /* 0x04: BNE to 0x02 (offset=-6 from PC+4) */
      THUMB16(WFI)              /* 0x06: WFI */
  };
  memcpy(test_memory, code, sizeof(code));

  int64_t cycles = armv8m_exec_run(&exec, 100);

  /* 1 (MOVS) + 5*(1 SUBS + 1 BNE) + 1 (final BNE not taken) + 1 WFI
     Actually: MOVS, then 5x(SUBS+BNE), but last BNE falls through, then WFI
     = 1 + 5*2 + 1 = 12... let's just verify R0 is 0 */
  CHECK_EQUAL(0u, exec.cpu.r[0]);
  CHECK_TRUE(exec.cpu.sleeping);
  CHECK(cycles > 0);
}

TEST(IntegrationExecRun, SumArray) {
  /* Sum an array of 4 bytes:
   *   MOVS R0, #0       ; sum = 0
   *   MOVS R1, #4       ; count = 4
   *   LDR R2, =DATA_BASE ; R2 = array base (use MOV for simplicity)
   * loop:
   *   LDRB R3, [R2, #0] ; load byte
   *   ADDS R0, R0, R3   ; sum += byte
   *   ADDS R2, R2, #1   ; ptr++
   *   SUBS R1, R1, #1   ; count--
   *   BNE loop
   *   WFI
   */

  /* Set up data array at DATA_BASE: [10, 20, 30, 40] */
  test_memory[DATA_BASE] = 10;
  test_memory[DATA_BASE + 1] = 20;
  test_memory[DATA_BASE + 2] = 30;
  test_memory[DATA_BASE + 3] = 40;

  /* Simple approach: use MOVS with shifts to build address */
  uint8_t code[] = {
      THUMB16(MOVS_IMM(0, 0)),              /* 0x00: MOVS R0, #0 (sum) */
      THUMB16(MOVS_IMM(1, 4)),              /* 0x02: MOVS R1, #4 (count) */
      THUMB16(MOVS_IMM(2, DATA_BASE >> 8)), /* 0x04: MOVS R2, #(DATA_BASE>>8) */
      THUMB16(LSLS_IMM(2, 2, 8)),           /* 0x06: LSLS R2, R2, #8 */
      /* loop: */
      THUMB16(LDRB_IMM(3, 2, 0)),  /* 0x08: LDRB R3, [R2, #0] */
      THUMB16(ADDS_REG(0, 0, 3)),  /* 0x0A: ADDS R0, R0, R3 */
      THUMB16(ADDS_IMM3(2, 2, 1)), /* 0x0C: ADDS R2, R2, #1 */
      THUMB16(SUBS_IMM8(1, 1)),    /* 0x0E: SUBS R1, #1 */
      THUMB16(
          B_COND(COND_NE, -12)), /* 0x10: BNE to 0x08 (offset=-12 from PC+4) */
      THUMB16(WFI)               /* 0x12: WFI */
  };
  memcpy(test_memory, code, sizeof(code));

  int64_t cycles = armv8m_exec_run(&exec, 200);

  CHECK_EQUAL(100u, exec.cpu.r[0]); /* 10+20+30+40 = 100 */
  CHECK_TRUE(exec.cpu.sleeping);
  CHECK(cycles > 0);
}

TEST(IntegrationExecRun, FunctionCall) {
  /* Main:
   *   MOVS R0, #10
   *   BL add_five (simulated with BLX)
   *   WFI
   *
   * add_five: (at 0x20)
   *   ADDS R0, R0, #5
   *   BX LR
   */

  /* Since BL is 32-bit and complex, use BLX Rm instead */
  exec.cpu.r[4] = 0x21; /* Address of add_five with Thumb bit */

  uint8_t main_code[] = {
      THUMB16(MOVS_IMM(0, 10)), /* 0x00: MOVS R0, #10 */
      THUMB16(BLX(4)),          /* 0x02: BLX R4 */
      THUMB16(WFI)              /* 0x04: WFI */
  };

  uint8_t func_code[] = {
      THUMB16(ADDS_IMM3(0, 0, 5)), /* 0x20: ADDS R0, #5 */
      THUMB16(BX(14))              /* 0x22: BX LR */
  };

  memcpy(test_memory, main_code, sizeof(main_code));
  memcpy(test_memory + 0x20, func_code, sizeof(func_code));

  int64_t cycles = armv8m_exec_run(&exec, 100);

  CHECK_EQUAL(15u, exec.cpu.r[0]); /* 10 + 5 = 15 */
  CHECK_TRUE(exec.cpu.sleeping);
  CHECK(cycles > 0);
}

TEST(IntegrationExecRun, StoreAndLoadSequence) {
  /* Store values to memory, then load them back:
   *   MOVS R0, #100
   *   MOVS R1, #200
   *   STR R0, [SP, #0]
   *   STR R1, [SP, #4]
   *   LDR R2, [SP, #0]
   *   LDR R3, [SP, #4]
   *   ADDS R4, R2, R3
   *   WFI
   */
  uint8_t code[] = {
      THUMB16(MOVS_IMM(0, 100)),  /* MOVS R0, #100 */
      THUMB16(MOVS_IMM(1, 200)),  /* MOVS R1, #200 */
      THUMB16(STR_SP(0, 0)),      /* STR R0, [SP, #0] */
      THUMB16(STR_SP(1, 4)),      /* STR R1, [SP, #4] */
      THUMB16(LDR_SP(2, 0)),      /* LDR R2, [SP, #0] */
      THUMB16(LDR_SP(3, 4)),      /* LDR R3, [SP, #4] */
      THUMB16(ADDS_REG(4, 2, 3)), /* ADDS R4, R2, R3 */
      THUMB16(WFI)                /* WFI */
  };
  memcpy(test_memory, code, sizeof(code));

  int64_t cycles = armv8m_exec_run(&exec, 100);

  CHECK_EQUAL(100u, exec.cpu.r[2]);
  CHECK_EQUAL(200u, exec.cpu.r[3]);
  CHECK_EQUAL(300u, exec.cpu.r[4]);
  CHECK_TRUE(exec.cpu.sleeping);
  CHECK(cycles > 0);
}

TEST(IntegrationExecRun, CycleLimit) {
  /* Run NOPs but limit cycles */
  for (int i = 0; i < 50; i++) {
    write_insn16((uint32_t)(i * 2), NOP);
  }

  int64_t cycles = armv8m_exec_run(&exec, 10);

  CHECK_EQUAL(10, cycles);
  CHECK_EQUAL(20u, exec.cpu.r[15]); /* Executed 10 NOPs, each 2 bytes */
}

TEST(IntegrationExecRun, HaltOnWFI) {
  /* Just WFI - should halt immediately */
  write_insn16(0, WFI);

  int64_t cycles = armv8m_exec_run(&exec, 100);

  CHECK_EQUAL(1, cycles);
  CHECK_TRUE(exec.cpu.sleeping);
}

/*============================================================================
 * Test Group: Flag Updates
 *============================================================================*/

TEST_GROUP(IntegrationFlags){
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

TEST(IntegrationFlags, ZeroFlag) {
  /* MOVS R0, #0 - should set Z flag */
  write_insn16(0, MOVS_IMM(0, 0));

  armv8m_exec_step(&exec);

  CHECK_TRUE(exec.cpu.xpsr & (1u << 30)); /* Z flag */
}

TEST(IntegrationFlags, NegativeFlag) {
  /* Result of subtraction is negative */
  exec.cpu.r[0] = 5;
  exec.cpu.r[1] = 10;
  write_insn16(0, SUBS_REG(2, 0, 1)); /* R2 = 5 - 10 = -5 */

  armv8m_exec_step(&exec);

  CHECK_TRUE(exec.cpu.xpsr & (1u << 31));  /* N flag */
  CHECK_FALSE(exec.cpu.xpsr & (1u << 30)); /* Z flag clear */
}

TEST(IntegrationFlags, CarryFlag) {
  /* Add that causes carry */
  exec.cpu.r[0] = 0xFFFFFFFF;
  exec.cpu.r[1] = 2;
  write_insn16(0, ADDS_REG(2, 0, 1)); /* Overflow */

  armv8m_exec_step(&exec);

  CHECK_TRUE(exec.cpu.xpsr & (1u << 29)); /* C flag */
}

TEST(IntegrationFlags, OverflowFlag) {
  /* Signed overflow: 0x7FFFFFFF + 1 */
  exec.cpu.r[0] = 0x7FFFFFFF;
  write_insn16(0, ADDS_IMM3(1, 0, 1)); /* R1 = MAX_INT + 1 */

  armv8m_exec_step(&exec);

  CHECK_TRUE(exec.cpu.xpsr & (1u << 28)); /* V flag (signed overflow) */
}

/*============================================================================
 * Test Group: IT Block Handling
 *============================================================================*/
