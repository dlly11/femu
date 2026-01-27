/**
 * @file decoder.c
 * @brief Main entry point for ARMv8-M Thumb instruction decoder
 *
 * Provides armv8m_decode() and armv8m_decode_init() functions,
 * dispatching to 16-bit or 32-bit decoders as appropriate.
 */

#include "armv8m_decoder.h"
#include <string.h>
#include <stdio.h>

/*============================================================================
 * Internal Function Declarations
 *============================================================================*/

/* Defined in decode_thumb16.c */
int decode_thumb16(uint16_t insn, uint32_t pc, DecodedInsn *out);

/* Defined in decode_thumb32.c */
int decode_thumb32(uint16_t hw1, uint16_t hw2, uint32_t pc, DecodedInsn *out);

/*============================================================================
 * Helper Functions
 *============================================================================*/

/**
 * Read a 16-bit halfword from memory (little-endian)
 */
static inline uint16_t read_hw(const uint8_t *mem)
{
    return (uint16_t)(mem[0] | (mem[1] << 8));
}

/*============================================================================
 * Public API Implementation
 *============================================================================*/

void armv8m_decode_init(DecodedInsn *insn)
{
    memset(insn, 0, sizeof(*insn));

    /* Set default register values to "not used" */
    insn->rd = ARMV8M_REG_NONE;
    insn->rn = ARMV8M_REG_NONE;
    insn->rm = ARMV8M_REG_NONE;
    insn->rs = ARMV8M_REG_NONE;
    insn->rt = ARMV8M_REG_NONE;
    insn->rt2 = ARMV8M_REG_NONE;

    /* Default condition is always execute */
    insn->cond = COND_AL;

    /* Default type is undefined */
    insn->type = INSN_UNDEFINED;
}

int armv8m_decode(const uint8_t *mem, uint32_t pc, DecodedInsn *insn)
{
    uint16_t hw1;
    int result;

    if (mem == NULL || insn == NULL) {
        return ARMV8M_ERR_UNDEFINED_INSN;
    }

    /* Initialize the output structure */
    armv8m_decode_init(insn);
    insn->pc = pc;

    /* Read first halfword */
    hw1 = read_hw(mem);

    /* Check if this is a 32-bit instruction */
    if (armv8m_is_thumb32(hw1)) {
        /* 32-bit Thumb instruction */
        uint16_t hw2 = read_hw(mem + 2);

        insn->encoding = ((uint32_t)hw1 << 16) | hw2;
        insn->is_32bit = true;
        insn->size = 4;

        result = decode_thumb32(hw1, hw2, pc, insn);
        if (result < 0) {
            return result;
        }
        return 4;
    }

    /* 16-bit Thumb instruction */
    insn->encoding = hw1;
    insn->is_32bit = false;
    insn->size = 2;

    result = decode_thumb16(hw1, pc, insn);
    if (result < 0) {
        return result;
    }
    return 2;
}

const char* armv8m_insn_mnemonic(InstructionType type, uint8_t op)
{
    static const char *dp_mnemonics[] = {
        "AND", "EOR", "LSL", "LSR", "ASR", "ADC", "SBC", "ROR",
        "TST", "RSB", "CMP", "CMN", "ORR", "MUL", "BIC", "MVN",
        "ADD", "SUB", "MOV", "ORN"
    };

    switch (type) {
    case INSN_DATA_PROC_IMM:
    case INSN_DATA_PROC_REG:
    case INSN_DATA_PROC_SHIFTED:
        if (op < sizeof(dp_mnemonics) / sizeof(dp_mnemonics[0])) {
            return dp_mnemonics[op];
        }
        break;
    case INSN_LOAD_IMM:
    case INSN_LOAD_REG:
    case INSN_LOAD_LITERAL:
        return "LDR";
    case INSN_STORE_IMM:
    case INSN_STORE_REG:
        return "STR";
    case INSN_LOAD_MULTIPLE:
        return "LDM";
    case INSN_STORE_MULTIPLE:
        return "STM";
    case INSN_BRANCH:
        return "B";
    case INSN_BRANCH_LINK:
        return "BL";
    case INSN_BRANCH_EXCHANGE:
        return "BX";
    case INSN_BRANCH_LINK_EXCHANGE:
        return "BLX";
    case INSN_COMPARE_BRANCH:
        return "CBZ/CBNZ";
    case INSN_SVC:
        return "SVC";
    case INSN_HINT:
        return "NOP";
    case INSN_IT:
        return "IT";
    case INSN_MRS:
        return "MRS";
    case INSN_MSR:
        return "MSR";
    case INSN_BARRIER:
        return "DMB/DSB/ISB";
    case INSN_MULTIPLY:
        return "MUL";
    case INSN_DIVIDE:
        return "SDIV/UDIV";
    case INSN_EXTEND:
        return "SXTH/UXTH";
    default:
        break;
    }
    return "???";
}

int armv8m_disasm(const DecodedInsn *insn, char *buf, size_t buf_size)
{
    const char *mnemonic;
    int written;

    if (insn == NULL || buf == NULL || buf_size == 0) {
        return -1;
    }

    mnemonic = armv8m_insn_mnemonic(insn->type, insn->op);

    /* Simple disassembly - just show mnemonic and main operands */
    switch (insn->type) {
    case INSN_DATA_PROC_IMM:
        written = snprintf(buf, buf_size, "%s R%d, #%u",
                          mnemonic, insn->rd, insn->imm);
        break;
    case INSN_DATA_PROC_REG:
        written = snprintf(buf, buf_size, "%s R%d, R%d, R%d",
                          mnemonic, insn->rd, insn->rn, insn->rm);
        break;
    case INSN_LOAD_IMM:
    case INSN_STORE_IMM:
        written = snprintf(buf, buf_size, "%s R%d, [R%d, #%u]",
                          mnemonic, insn->rt, insn->rn, insn->imm);
        break;
    case INSN_BRANCH:
    case INSN_BRANCH_LINK:
        written = snprintf(buf, buf_size, "%s %+d",
                          mnemonic, insn->branch_offset);
        break;
    case INSN_BRANCH_EXCHANGE:
        written = snprintf(buf, buf_size, "%s R%d", mnemonic, insn->rm);
        break;
    default:
        written = snprintf(buf, buf_size, "%s", mnemonic);
        break;
    }

    return written;
}
