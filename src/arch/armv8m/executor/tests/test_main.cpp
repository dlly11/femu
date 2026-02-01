/**
 * @file test_main.cpp
 * @brief Main entry point and shared infrastructure for executor tests
 */

#include "test_common.h"

/*============================================================================
 * Global Variables (used by test_common.h inline functions)
 *============================================================================*/

uint8_t mock_memory[8192];
DecodedInsn mock_decoded_insn;
int mock_decode_return_value = 2;

/*============================================================================
 * Decoder Mock Implementation
 *============================================================================*/

extern "C" {

int armv8m_decode(const uint8_t *mem, uint32_t pc, DecodedInsn *insn) {
    (void)mem;
    mock().actualCall("armv8m_decode")
          .withParameter("pc", pc)
          .withOutputParameter("insn", insn);

    if (insn) {
        *insn = mock_decoded_insn;
        insn->pc = pc;
    }

    return mock().intReturnValue();
}

void armv8m_decode_init(DecodedInsn *insn) {
    mock().actualCall("armv8m_decode_init");

    if (insn) {
        memset(insn, 0, sizeof(DecodedInsn));
        insn->rd = ARMV8M_REG_NONE;
        insn->rn = ARMV8M_REG_NONE;
        insn->rm = ARMV8M_REG_NONE;
        insn->rs = ARMV8M_REG_NONE;
        insn->rt = ARMV8M_REG_NONE;
        insn->rt2 = ARMV8M_REG_NONE;
        insn->cond = COND_AL;
        insn->size = 2;
    }
}

} /* extern "C" */

/*============================================================================
 * Test Runner
 *============================================================================*/

int main(int argc, char** argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
