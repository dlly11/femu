/**
 * @file decoder_internal.h
 * @brief Internal prototypes shared by the ARMv8-M decoder translation units.
 *
 * The top-level dispatcher (decoder.c) and the per-width decoders
 * (decode_thumb16.c, decode_thumb32.c) share these declarations so each
 * definition is checked against a visible prototype.
 */
#ifndef ARMV8M_DECODER_INTERNAL_H
#define ARMV8M_DECODER_INTERNAL_H

#include "arch/armv8m/armv8m_decoder.h"

/* Decode a 16-bit Thumb instruction (defined in decode_thumb16.c). */
int decode_thumb16(uint16_t insn, uint32_t pc, DecodedInsn *out);

/* Decode a 32-bit Thumb-2 instruction (defined in decode_thumb32.c). */
int decode_thumb32(uint16_t hw1, uint16_t hw2, uint32_t pc, DecodedInsn *out);

#endif /* ARMV8M_DECODER_INTERNAL_H */
