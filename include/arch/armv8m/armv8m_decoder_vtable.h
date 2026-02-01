/**
 * @file armv8m_decoder_vtable.h
 * @brief ARMv8-M decoder adapter for abstract EmuDecoder interface
 *
 * Provides the ARMv8-M implementation of the abstract decoder interface,
 * wrapping the existing armv8m_decode() function with the EmuDecoder vtable.
 */

#ifndef ARMV8M_DECODER_VTABLE_H
#define ARMV8M_DECODER_VTABLE_H

#include "emu/emu_decoder.h"
#include "arch/armv8m/armv8m_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * ARMv8-M Decoder Wrapper
 *============================================================================*/

/**
 * ARMv8-M specific decoder wrapper.
 *
 * Embeds EmuDecoder as first member for safe casting.
 */
typedef struct {
    EmuDecoder base;            /**< Base EmuDecoder (must be first) */
    DecodedInsn current_insn;   /**< Current decoded instruction storage */
} ARMv8MDecoder;

/*============================================================================
 * ARMv8-M Decoder API
 *============================================================================*/

/**
 * Get the vtable for ARMv8-M decoder.
 *
 * @return      Pointer to static vtable
 */
const EmuDecoderVTable *armv8m_decoder_get_vtable(void);

/**
 * Initialize ARMv8-M decoder wrapper.
 *
 * @param dec       Decoder wrapper to initialize
 */
void armv8m_decoder_vtable_init(ARMv8MDecoder *dec);

/**
 * Get ARMv8-M decoder wrapper from EmuDecoder pointer.
 *
 * @param dec       EmuDecoder pointer (must be ARMv8MDecoder)
 * @return          ARMv8MDecoder pointer
 */
static inline ARMv8MDecoder *armv8m_decoder_from_base(EmuDecoder *dec) {
    return (ARMv8MDecoder *)dec;
}

/**
 * Get const ARMv8-M decoder wrapper from const EmuDecoder pointer.
 *
 * @param dec       Const EmuDecoder pointer
 * @return          Const ARMv8MDecoder pointer
 */
static inline const ARMv8MDecoder *armv8m_decoder_from_base_const(const EmuDecoder *dec) {
    return (const ARMv8MDecoder *)dec;
}

#ifdef __cplusplus
}
#endif

#endif /* ARMV8M_DECODER_VTABLE_H */
