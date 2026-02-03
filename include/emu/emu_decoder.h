/**
 * @file emu_decoder.h
 * @brief Abstract decoder interface for multi-architecture support
 *
 * This file defines the abstract instruction decoder interface using a vtable
 * pattern. Architecture-specific implementations provide concrete decoding
 * functions.
 */

#ifndef EMU_DECODER_H
#define EMU_DECODER_H

#include "emu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Forward Declarations
 *============================================================================*/

typedef struct EmuDecoder EmuDecoder;
typedef struct EmuDecoderVTable EmuDecoderVTable;
typedef struct EmuDecodedInsn EmuDecodedInsn;

/*============================================================================
 * Decoded Instruction (Abstract)
 *============================================================================*/

/**
 * Architecture-agnostic decoded instruction metadata.
 *
 * Architecture implementations may extend this with additional fields
 * through the arch_insn pointer.
 */
struct EmuDecodedInsn {
  uint64_t pc;       /**< PC value of this instruction */
  uint32_t encoding; /**< Raw instruction encoding (first word) */
  uint8_t size;      /**< Instruction size in bytes */

  /* Classification flags */
  bool is_branch;      /**< Is a branch instruction */
  bool is_call;        /**< Is a call (saves return address) */
  bool is_return;      /**< Is a return instruction */
  bool is_conditional; /**< Is conditionally executed */

  /* Target for branches (if known statically) */
  uint64_t branch_target; /**< Branch target address (0 if indirect) */
  bool target_known;      /**< Branch target is statically known */

  /* Architecture-specific decoded data */
  void *arch_insn; /**< Pointer to arch-specific decoded struct */
};

/*============================================================================
 * Decoder Virtual Table
 *============================================================================*/

/**
 * Virtual function table for decoder operations.
 */
struct EmuDecoderVTable {
  /**
   * Destroy decoder and free resources.
   *
   * @param dec       Decoder instance
   */
  void (*destroy)(EmuDecoder *dec);

  /**
   * Decode a single instruction.
   *
   * @param dec       Decoder instance
   * @param mem       Pointer to instruction bytes
   * @param pc        Program counter value
   * @param insn      Output: decoded instruction structure
   * @return          Bytes consumed on success, negative on error
   */
  int (*decode)(EmuDecoder *dec, const uint8_t *mem, uint64_t pc,
                EmuDecodedInsn *insn);

  /**
   * Generate disassembly string for decoded instruction.
   *
   * @param dec       Decoder instance
   * @param insn      Decoded instruction
   * @param buf       Output buffer
   * @param size      Buffer size
   * @return          Characters written, or negative on error
   */
  int (*disassemble)(EmuDecoder *dec, const EmuDecodedInsn *insn, char *buf,
                     size_t size);

  /**
   * Get maximum instruction size for this architecture.
   *
   * @param dec       Decoder instance
   * @return          Maximum instruction size in bytes
   */
  int (*get_max_insn_size)(const EmuDecoder *dec);

  /**
   * Initialize a decoded instruction structure.
   *
   * @param dec       Decoder instance
   * @param insn      Instruction structure to initialize
   */
  void (*init_insn)(EmuDecoder *dec, EmuDecodedInsn *insn);

  /**
   * Free architecture-specific decoded instruction data.
   *
   * @param dec       Decoder instance
   * @param insn      Instruction to clean up
   */
  void (*free_insn)(EmuDecoder *dec, EmuDecodedInsn *insn);
};

/*============================================================================
 * Decoder Structure
 *============================================================================*/

/**
 * Abstract decoder instance.
 */
struct EmuDecoder {
  const EmuDecoderVTable *vtable; /**< Virtual function table */
  EmuArchType arch;               /**< Architecture type */
  void *arch_state;               /**< Architecture-specific state */
};

/*============================================================================
 * Convenience Macros
 *============================================================================*/

#define EMU_DECODER_INIT(dec, vtbl, arch_type, state)                          \
  do {                                                                         \
    (dec)->vtable = (vtbl);                                                    \
    (dec)->arch = (arch_type);                                                 \
    (dec)->arch_state = (state);                                               \
  } while (0)

/*============================================================================
 * Inline Convenience Functions
 *============================================================================*/

static inline void emu_decoder_destroy(EmuDecoder *dec) {
  if (dec && dec->vtable && dec->vtable->destroy) {
    dec->vtable->destroy(dec);
  }
}

static inline int emu_decode(EmuDecoder *dec, const uint8_t *mem, uint64_t pc,
                             EmuDecodedInsn *insn) {
  return (dec && dec->vtable) ? dec->vtable->decode(dec, mem, pc, insn)
                              : EMU_ERR_NOT_INITIALIZED;
}

static inline int emu_disassemble(EmuDecoder *dec, const EmuDecodedInsn *insn,
                                  char *buf, size_t size) {
  return (dec && dec->vtable && dec->vtable->disassemble)
             ? dec->vtable->disassemble(dec, insn, buf, size)
             : EMU_ERR_NOT_SUPPORTED;
}

static inline int emu_decoder_get_max_insn_size(const EmuDecoder *dec) {
  return (dec && dec->vtable) ? dec->vtable->get_max_insn_size(dec) : 0;
}

static inline void emu_decoder_init_insn(EmuDecoder *dec,
                                         EmuDecodedInsn *insn) {
  if (dec && dec->vtable && dec->vtable->init_insn) {
    dec->vtable->init_insn(dec, insn);
  }
}

static inline void emu_decoder_free_insn(EmuDecoder *dec,
                                         EmuDecodedInsn *insn) {
  if (dec && dec->vtable && dec->vtable->free_insn) {
    dec->vtable->free_insn(dec, insn);
  }
}

#ifdef __cplusplus
}
#endif

#endif /* EMU_DECODER_H */
