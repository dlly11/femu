/**
 * @file test_common.h
 * @brief Shared test infrastructure for ARMv8-M executor tests
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

extern "C" {
#include "arch/armv8m/armv8m_decoder.h"
#include "arch/armv8m/armv8m_executor.h"
#include "arch/armv8m/armv8m_types.h"
}

#include <cstring>

/*============================================================================
 * Hint Operation Constants (from exec_system.c)
 *============================================================================*/
#define HINT_NOP 0
#define HINT_YIELD 1
#define HINT_WFE 2
#define HINT_WFI 3
#define HINT_SEV 4
#define HINT_SEVL 5

/*============================================================================
 * Mock Memory
 *============================================================================*/

extern uint8_t mock_memory[8192];
extern DecodedInsn mock_decoded_insn;
extern int mock_decode_return_value;

/*============================================================================
 * Mock Memory Callbacks
 *============================================================================*/

inline uint32_t mock_mem_read(void *ctx, uint32_t addr, uint8_t size,
                              bool *fault) {
  (void)ctx;
  if (fault)
    *fault = false;

  if (addr + size > sizeof(mock_memory)) {
    if (fault)
      *fault = true;
    return 0;
  }

  switch (size) {
  case 1:
    return mock_memory[addr];
  case 2:
    return mock_memory[addr] | ((uint32_t)mock_memory[addr + 1] << 8);
  case 4:
    return mock_memory[addr] | ((uint32_t)mock_memory[addr + 1] << 8) |
           ((uint32_t)mock_memory[addr + 2] << 16) |
           ((uint32_t)mock_memory[addr + 3] << 24);
  }
  return 0;
}

inline void mock_mem_write(void *ctx, uint32_t addr, uint32_t value,
                           uint8_t size, bool *fault) {
  (void)ctx;
  if (fault)
    *fault = false;

  if (addr + size > sizeof(mock_memory)) {
    if (fault)
      *fault = true;
    return;
  }

  switch (size) {
  case 1:
    mock_memory[addr] = (uint8_t)(value & 0xFF);
    break;
  case 2:
    mock_memory[addr] = (uint8_t)(value & 0xFF);
    mock_memory[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
    break;
  case 4:
    mock_memory[addr] = (uint8_t)(value & 0xFF);
    mock_memory[addr + 1] = (uint8_t)((value >> 8) & 0xFF);
    mock_memory[addr + 2] = (uint8_t)((value >> 16) & 0xFF);
    mock_memory[addr + 3] = (uint8_t)((value >> 24) & 0xFF);
    break;
  }
}

inline const uint8_t *mock_mem_get_ptr(void *ctx, uint32_t addr,
                                       uint32_t size) {
  (void)ctx;
  if (addr + size > sizeof(mock_memory)) {
    return NULL;
  }
  return &mock_memory[addr];
}

/*============================================================================
 * Mock Helper Functions
 *============================================================================*/

inline void setup_mock_decode(const DecodedInsn &insn, int return_value = 2) {
  mock_decoded_insn = insn;
  mock_decode_return_value = return_value;
}

inline void expect_decode_init() { mock().expectOneCall("armv8m_decode_init"); }

inline void expect_decode(uint32_t pc, int return_value = 2) {
  mock()
      .expectOneCall("armv8m_decode")
      .withParameter("pc", pc)
      .withOutputParameterReturning("insn", &mock_decoded_insn,
                                    sizeof(mock_decoded_insn))
      .andReturnValue(return_value);
}

inline void init_insn(DecodedInsn &insn) {
  memset(&insn, 0, sizeof(DecodedInsn));
  insn.rd = ARMV8M_REG_NONE;
  insn.rn = ARMV8M_REG_NONE;
  insn.rm = ARMV8M_REG_NONE;
  insn.rs = ARMV8M_REG_NONE;
  insn.rt = ARMV8M_REG_NONE;
  insn.rt2 = ARMV8M_REG_NONE;
  insn.cond = COND_AL;
  insn.size = 2;
}

#endif /* TEST_COMMON_H */
