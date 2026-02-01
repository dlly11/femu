/**
 * @file decode_thumb32_internal.h
 * @brief Internal header for 32-bit Thumb instruction decoding
 *
 * Shared macros, helpers, and function declarations for decode_thumb32_*.c files.
 */

#ifndef DECODE_THUMB32_INTERNAL_H
#define DECODE_THUMB32_INTERNAL_H

#include "arch/armv8m/armv8m_decoder.h"

/*============================================================================
 * Helper Macros
 *============================================================================*/

/* Extract bit field from value */
#define EXTRACT(val, start, len) (((uint32_t)(val) >> (start)) & ((1U << (len)) - 1))

/* Extract single bit */
#define BIT(val, n) (((val) >> (n)) & 1)

/*============================================================================
 * Inline Helper Functions
 *============================================================================*/

/* Sign extend a value */
static inline int32_t sign_extend(uint32_t val, int bits)
{
    int32_t shift = 32 - bits;
    return ((int32_t)(val << shift)) >> shift;
}

/**
 * Expand a Thumb-2 modified immediate constant (ThumbExpandImm).
 */
static inline uint32_t thumb_expand_imm(uint32_t imm12)
{
    uint32_t imm8 = imm12 & 0xFF;

    if ((imm12 & 0xC00) == 0) {
        uint32_t op = (imm12 >> 8) & 0x3;
        switch (op) {
        case 0: return imm8;
        case 1: return (imm8 << 16) | imm8;
        case 2: return (imm8 << 24) | (imm8 << 8);
        case 3: return (imm8 << 24) | (imm8 << 16) | (imm8 << 8) | imm8;
        default: return imm8;
        }
    }

    uint32_t val = 0x80 | (imm12 & 0x7F);
    uint32_t rot = (imm12 >> 7) & 0x1F;
    return (val >> rot) | (val << (32 - rot));
}

/*============================================================================
 * Parallel Add/Sub Operation Codes
 *============================================================================*/

#define PARALLEL_SADD8      0x00
#define PARALLEL_SSUB8      0x04
#define PARALLEL_SADD16     0x01
#define PARALLEL_SSUB16     0x05
#define PARALLEL_SASX       0x02
#define PARALLEL_SSAX       0x06
#define PARALLEL_SHADD8     0x20
#define PARALLEL_SHSUB8     0x24
#define PARALLEL_SHADD16    0x21
#define PARALLEL_SHSUB16    0x25
#define PARALLEL_SHASX      0x22
#define PARALLEL_SHSAX      0x26
#define PARALLEL_UADD8      0x40
#define PARALLEL_USUB8      0x44
#define PARALLEL_UADD16     0x41
#define PARALLEL_USUB16     0x45
#define PARALLEL_UASX       0x42
#define PARALLEL_USAX       0x46
#define PARALLEL_UHADD8     0x60
#define PARALLEL_UHSUB8     0x64
#define PARALLEL_UHADD16    0x61
#define PARALLEL_UHSUB16    0x65
#define PARALLEL_UHASX      0x62
#define PARALLEL_UHSAX      0x66
#define PARALLEL_QADD8      0x10
#define PARALLEL_QSUB8      0x14
#define PARALLEL_QADD16     0x11
#define PARALLEL_QSUB16     0x15
#define PARALLEL_QASX       0x12
#define PARALLEL_QSAX       0x16
#define PARALLEL_UQADD8     0x50
#define PARALLEL_UQSUB8     0x54
#define PARALLEL_UQADD16    0x51
#define PARALLEL_UQSUB16    0x55
#define PARALLEL_UQASX      0x52
#define PARALLEL_UQSAX      0x56

/*============================================================================
 * Saturating Arithmetic Operation Codes
 *============================================================================*/

#define SAT_QADD    0
#define SAT_QSUB    1
#define SAT_QDADD   2
#define SAT_QDSUB   3

/*============================================================================
 * VFP Operation Codes
 *============================================================================*/

/* VFP arithmetic operations */
#define VFP_VMLA    0x00
#define VFP_VMLS    0x01
#define VFP_VNMLA   0x02
#define VFP_VNMLS   0x03
#define VFP_VNMUL   0x04
#define VFP_VMUL    0x05
#define VFP_VADD    0x06
#define VFP_VSUB    0x07
#define VFP_VDIV    0x08
#define VFP_VABS    0x10
#define VFP_VNEG    0x11
#define VFP_VSQRT   0x12
#define VFP_VFMA    0x13  /* Fused multiply-add: Sd += Sn * Sm */
#define VFP_VFMS    0x14  /* Fused multiply-subtract: Sd -= Sn * Sm */
#define VFP_VFNMA   0x15  /* Fused negate multiply-add: Sd = -(Sd + Sn * Sm) */
#define VFP_VFNMS   0x16  /* Fused negate multiply-subtract: Sd = -Sd + Sn * Sm */

/* VFP move operations */
#define VFP_VMOV_IMM    0x00
#define VFP_VMOV_REG    0x01
#define VFP_VMOV_ARM    0x02
#define VFP_VMOV_2ARM   0x03

/* VFP conversion operations */
#define VFP_VCVT_F32_F64    0x00
#define VFP_VCVT_F64_F32    0x01
#define VFP_VCVT_S32_F      0x02
#define VFP_VCVT_U32_F      0x03
#define VFP_VCVT_F_S32      0x04
#define VFP_VCVT_F_U32      0x05

/*============================================================================
 * Decode Function Declarations
 *============================================================================*/

/* Branch and miscellaneous control */
int decode_branch_misc(uint16_t hw1, uint16_t hw2, uint32_t pc, DecodedInsn *out);

/* Data processing */
int decode_data_proc_modified_imm(uint16_t hw1, uint16_t hw2, DecodedInsn *out);
int decode_data_proc_plain_imm(uint16_t hw1, uint16_t hw2, DecodedInsn *out);
int decode_data_proc_shifted_reg(uint16_t hw1, uint16_t hw2, DecodedInsn *out);

/* Load/store */
int decode_load_store_single(uint16_t hw1, uint16_t hw2, DecodedInsn *out);
int decode_load_store_dual(uint16_t hw1, uint16_t hw2, DecodedInsn *out);
int decode_load_store_multiple(uint16_t hw1, uint16_t hw2, DecodedInsn *out);

/* Multiply */
int decode_multiply(uint16_t hw1, uint16_t hw2, DecodedInsn *out);
int decode_long_multiply(uint16_t hw1, uint16_t hw2, DecodedInsn *out);

/* DSP and miscellaneous */
int decode_parallel_add_sub(uint16_t hw1, uint16_t hw2, DecodedInsn *out);
int decode_misc_ops(uint16_t hw1, uint16_t hw2, DecodedInsn *out);

/* VFP/FPU */
int decode_vfp(uint16_t hw1, uint16_t hw2, DecodedInsn *out);

#endif /* DECODE_THUMB32_INTERNAL_H */
