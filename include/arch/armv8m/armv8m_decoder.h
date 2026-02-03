/**
 * @file armv8m_decoder.h
 * @brief Instruction decoder for ARMv8-M Thumb instructions
 *
 * AI INSTRUCTIONS:
 * - This header defines the COMPLETE interface for the decoder module
 * - Implementation goes in src/core/decoder/
 * - Do NOT modify this header without updating MODULE_CONTRACTS.md
 * - All types used are defined in armv8m_types.h
 * - See src/core/decoder/README.md for implementation guidance
 */

#ifndef ARMV8M_DECODER_H
#define ARMV8M_DECODER_H

#include "arch/armv8m/armv8m_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Decoded Instruction Structure
 *============================================================================*/

/**
 * Represents a fully decoded instruction.
 * Contains all information needed by the executor to run the instruction.
 */
typedef struct {
  /* Instruction classification */
  InstructionType type; /**< Major instruction category */
  uint8_t op;           /**< Specific operation (e.g., DataProcOp) */

  /* Register operands (ARMV8M_REG_NONE if not used) */
  uint8_t rd;  /**< Destination register */
  uint8_t rn;  /**< First source register */
  uint8_t rm;  /**< Second source register */
  uint8_t rs;  /**< Shift amount register (if reg-controlled) */
  uint8_t rt;  /**< Transfer register (for load/store) */
  uint8_t rt2; /**< Second transfer register (for LDRD/STRD) */

  /* Immediate value */
  uint32_t imm; /**< Immediate operand (zero-extended) */

  /* Shift specification */
  ShiftType shift_type; /**< Type of shift (LSL, LSR, ASR, ROR, RRX) */
  uint8_t shift_amount; /**< Shift amount (0-31) */

  /* Flags */
  bool set_flags; /**< Update APSR flags (N, Z, C, V)? */
  bool is_32bit;  /**< Is this a 32-bit Thumb instruction? */
  bool writeback; /**< Write back to base register? */
  bool pre_index; /**< Pre-index addressing? (vs post-index) */
  bool add;       /**< Add offset? (vs subtract) */
  bool wback;     /**< Writeback to Rn? */
  bool index;     /**< Index addressing? */
  bool is_signed; /**< Signed load/extend? */

  /* Branch-specific */
  int32_t branch_offset; /**< Signed branch offset (PC-relative) */
  bool link;             /**< Save return address in LR? */

  /* Condition (for IT block or conditional branches) */
  ConditionCode cond; /**< Condition code (COND_AL if unconditional) */

  /* Load/store specific */
  AccessSize access_size; /**< BYTE, HALF, or WORD */
  uint16_t register_list; /**< Bitmask for LDM/STM (bit N = register N) */

  /* For IT instruction */
  uint8_t it_mask; /**< IT block mask */
  uint8_t it_cond; /**< IT block base condition */

  /* Special register (for MRS/MSR) */
  uint8_t sysreg; /**< Special register number */

  /* FPU fields */
  uint8_t sd;     /**< Destination S register (0-31) */
  uint8_t sn;     /**< First source S register */
  uint8_t sm;     /**< Second source S register */
  uint8_t dd;     /**< Destination D register (0-15) */
  uint8_t dn;     /**< First source D register */
  uint8_t dm;     /**< Second source D register */
  bool is_double; /**< true for F64, false for F32 */

  /* Debug/diagnostic */
  uint32_t encoding; /**< Raw instruction encoding */
  uint32_t pc;       /**< PC value of this instruction */
  uint8_t size;      /**< Instruction size in bytes (2 or 4) */
} DecodedInsn;

/*============================================================================
 * Decoder API
 *============================================================================*/

/**
 * Decode a single Thumb instruction.
 *
 * Decodes the instruction at the given memory location and fills in
 * the DecodedInsn structure with all information needed for execution.
 *
 * @param mem       Pointer to instruction bytes (must have at least 4 readable
 * bytes)
 * @param pc        Current program counter value (address of this instruction)
 * @param insn      Output: decoded instruction structure
 * @return          Number of bytes consumed (2 or 4) on success, negative on
 * error
 *
 * @retval 2        Successfully decoded 16-bit instruction
 * @retval 4        Successfully decoded 32-bit instruction
 * @retval ARMV8M_ERR_UNDEFINED_INSN   Undefined instruction encoding
 * @retval ARMV8M_ERR_UNPREDICTABLE    Unpredictable encoding (CONSTRAINED
 * UNPREDICTABLE)
 *
 * @note The decoder does not track IT block state; the caller must do this
 *       and apply the condition code appropriately.
 */
int armv8m_decode(const uint8_t *mem, uint32_t pc, DecodedInsn *insn);

/**
 * Initialize a DecodedInsn structure to default values.
 *
 * Call this before decoding to ensure all fields have known values.
 *
 * @param insn      Instruction structure to initialize
 */
void armv8m_decode_init(DecodedInsn *insn);

/**
 * Generate disassembly string for a decoded instruction.
 *
 * Produces a human-readable disassembly string in standard ARM syntax.
 *
 * @param insn      Decoded instruction
 * @param buf       Output buffer
 * @param buf_size  Size of output buffer
 * @return          Number of characters written (excluding null terminator),
 *                  or negative if buffer too small
 */
int armv8m_disasm(const DecodedInsn *insn, char *buf, size_t buf_size);

/**
 * Check if an instruction encoding is a 32-bit Thumb instruction.
 *
 * In Thumb mode, 32-bit instructions start with 0b11101, 0b11110, or 0b11111
 * in bits [15:11] of the first halfword.
 *
 * @param hw1       First halfword of instruction (at PC)
 * @return          true if this is the start of a 32-bit instruction
 */
static inline bool armv8m_is_thumb32(uint16_t hw1) {
  uint16_t op = (hw1 >> 11) & 0x1F;
  return (op == 0x1D || op == 0x1E || op == 0x1F);
}

/**
 * Get instruction mnemonic string.
 *
 * @param type      Instruction type
 * @param op        Specific operation code
 * @return          Static string with mnemonic, or "???" if unknown
 */
const char *armv8m_insn_mnemonic(InstructionType type, uint8_t op);

#ifdef __cplusplus
}
#endif

#endif /* ARMV8M_DECODER_H */
