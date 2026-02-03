/**
 * @file test_integration_common.h
 * @brief Shared infrastructure for ARMv8-M executor integration tests
 */

#ifndef TEST_INTEGRATION_COMMON_H
#define TEST_INTEGRATION_COMMON_H

#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTest/TestHarness.h"

extern "C" {
#include "arch/armv8m/armv8m_decoder.h"
#include "arch/armv8m/armv8m_executor.h"
#include "arch/armv8m/armv8m_types.h"
}

#include <cstring>

/*============================================================================
 * Instruction Encoding Macros
 *============================================================================*/

#define THUMB16(hw) (uint8_t)((hw) & 0xFF), (uint8_t)(((hw) >> 8) & 0xFF)

#define THUMB32(hw1, hw2)                                                      \
  (uint8_t)((hw1) & 0xFF), (uint8_t)(((hw1) >> 8) & 0xFF),                     \
      (uint8_t)((hw2) & 0xFF), (uint8_t)(((hw2) >> 8) & 0xFF)

/*============================================================================
 * Common Instruction Encodings
 *============================================================================*/

/* 16-bit instructions */
#define NOP 0xBF00
#define WFI 0xBF30
#define BKPT_0 0xBE00

#define MOVS_IMM(rd, imm) (0x2000 | ((rd) << 8) | (imm))
#define ADDS_IMM3(rd, rn, imm) (0x1C00 | ((imm) << 6) | ((rn) << 3) | (rd))
#define ADDS_REG(rd, rn, rm) (0x1800 | ((rm) << 6) | ((rn) << 3) | (rd))
#define SUBS_IMM3(rd, rn, imm) (0x1E00 | ((imm) << 6) | ((rn) << 3) | (rd))
#define SUBS_REG(rd, rn, rm) (0x1A00 | ((rm) << 6) | ((rn) << 3) | (rd))
#define ADDS_IMM8(rd, imm) (0x3000 | ((rd) << 8) | (imm))
#define SUBS_IMM8(rd, imm) (0x3800 | ((rd) << 8) | (imm))
#define CMP_IMM8(rn, imm) (0x2800 | ((rn) << 8) | (imm))
#define CMP_REG(rn, rm) (0x4280 | ((rm) << 3) | (rn))
#define MOV_REG(rd, rm) (0x4600 | (((rd) & 8) << 4) | ((rm) << 3) | ((rd) & 7))
#define LSLS_IMM(rd, rm, imm) (0x0000 | ((imm) << 6) | ((rm) << 3) | (rd))
#define LSRS_IMM(rd, rm, imm) (0x0800 | ((imm) << 6) | ((rm) << 3) | (rd))
#define ASRS_IMM(rd, rm, imm) (0x1000 | ((imm) << 6) | ((rm) << 3) | (rd))
#define ANDS_REG(rd, rm) (0x4000 | ((rm) << 3) | (rd))
#define EORS_REG(rd, rm) (0x4040 | ((rm) << 3) | (rd))
#define ORRS_REG(rd, rm) (0x4300 | ((rm) << 3) | (rd))
#define BICS_REG(rd, rm) (0x4380 | ((rm) << 3) | (rd))
#define MVNS_REG(rd, rm) (0x43C0 | ((rm) << 3) | (rd))
#define MULS_REG(rd, rm) (0x4340 | ((rm) << 3) | (rd))
#define TST_REG(rn, rm) (0x4200 | ((rm) << 3) | (rn))

#define PUSH(reglist) (0xB400 | (reglist))
#define PUSH_LR(reglist) (0xB500 | (reglist))
#define POP(reglist) (0xBC00 | (reglist))
#define POP_PC(reglist) (0xBD00 | (reglist))

#define STR_IMM(rt, rn, imm) (0x6000 | (((imm) / 4) << 6) | ((rn) << 3) | (rt))
#define LDR_IMM(rt, rn, imm) (0x6800 | (((imm) / 4) << 6) | ((rn) << 3) | (rt))
#define STRB_IMM(rt, rn, imm) (0x7000 | ((imm) << 6) | ((rn) << 3) | (rt))
#define LDRB_IMM(rt, rn, imm) (0x7800 | ((imm) << 6) | ((rn) << 3) | (rt))
#define STRH_IMM(rt, rn, imm) (0x8000 | (((imm) / 2) << 6) | ((rn) << 3) | (rt))
#define LDRH_IMM(rt, rn, imm) (0x8800 | (((imm) / 2) << 6) | ((rn) << 3) | (rt))
#define STR_SP(rt, imm) (0x9000 | ((rt) << 8) | ((imm) / 4))
#define LDR_SP(rt, imm) (0x9800 | ((rt) << 8) | ((imm) / 4))

#define B_COND(cond, offset) (0xD000 | ((cond) << 8) | (((offset) / 2) & 0xFF))
#define B_UNCOND(offset) (0xE000 | (((offset) / 2) & 0x7FF))
#define BX(rm) (0x4700 | ((rm) << 3))
#define BLX(rm) (0x4780 | ((rm) << 3))

#define ADD_SP_IMM(imm) (0xB000 | ((imm) / 4))
#define SUB_SP_IMM(imm) (0xB080 | ((imm) / 4))

/* Condition codes */
#define INTEG_COND_EQ 0
#define INTEG_COND_NE 1
#define INTEG_COND_CS 2
#define INTEG_COND_CC 3
#define INTEG_COND_MI 4
#define INTEG_COND_PL 5
#define INTEG_COND_VS 6
#define INTEG_COND_VC 7
#define INTEG_COND_HI 8
#define INTEG_COND_LS 9
#define INTEG_COND_GE 10
#define INTEG_COND_LT 11
#define INTEG_COND_GT 12
#define INTEG_COND_LE 13

/*============================================================================
 * Test Memory Infrastructure
 *============================================================================*/

#define MEMORY_SIZE 16384
#define CODE_BASE 0x0000
#define DATA_BASE 0x2000
#define STACK_BASE 0x3000

extern uint8_t test_memory[MEMORY_SIZE];
extern Executor exec;

inline void write_insn16(uint32_t addr, uint16_t hw) {
  test_memory[addr] = (uint8_t)(hw & 0xFF);
  test_memory[addr + 1] = (uint8_t)((hw >> 8) & 0xFF);
}

inline uint32_t integ_mem_read(void *ctx, uint32_t addr, uint8_t size,
                               bool *fault) {
  (void)ctx;
  if (fault)
    *fault = false;

  if (addr + size > MEMORY_SIZE) {
    if (fault)
      *fault = true;
    return 0;
  }

  switch (size) {
  case 1:
    return test_memory[addr];
  case 2:
    return test_memory[addr] | ((uint32_t)test_memory[addr + 1] << 8);
  case 4:
    return test_memory[addr] | ((uint32_t)test_memory[addr + 1] << 8) |
           ((uint32_t)test_memory[addr + 2] << 16) |
           ((uint32_t)test_memory[addr + 3] << 24);
  }
  return 0;
}

inline void integ_mem_write(void *ctx, uint32_t addr, uint32_t value,
                            uint8_t size, bool *fault) {
  (void)ctx;
  if (fault)
    *fault = false;

  if (addr + size > MEMORY_SIZE) {
    if (fault)
      *fault = true;
    return;
  }

  switch (size) {
  case 1:
    test_memory[addr] = (uint8_t)value;
    break;
  case 2:
    test_memory[addr] = (uint8_t)value;
    test_memory[addr + 1] = (uint8_t)(value >> 8);
    break;
  case 4:
    test_memory[addr] = (uint8_t)value;
    test_memory[addr + 1] = (uint8_t)(value >> 8);
    test_memory[addr + 2] = (uint8_t)(value >> 16);
    test_memory[addr + 3] = (uint8_t)(value >> 24);
    break;
  }
}

inline const uint8_t *integ_mem_get_ptr(void *ctx, uint32_t addr,
                                        uint32_t size) {
  (void)ctx;
  if (addr + size > MEMORY_SIZE) {
    return NULL;
  }
  return &test_memory[addr];
}

inline void write_word(uint32_t addr, uint32_t value) {
  test_memory[addr] = (uint8_t)value;
  test_memory[addr + 1] = (uint8_t)(value >> 8);
  test_memory[addr + 2] = (uint8_t)(value >> 16);
  test_memory[addr + 3] = (uint8_t)(value >> 24);
}

inline uint32_t read_word(uint32_t addr) {
  return test_memory[addr] | ((uint32_t)test_memory[addr + 1] << 8) |
         ((uint32_t)test_memory[addr + 2] << 16) |
         ((uint32_t)test_memory[addr + 3] << 24);
}

#endif /* TEST_INTEGRATION_COMMON_H */
