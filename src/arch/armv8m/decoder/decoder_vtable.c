/**
 * @file decoder_vtable.c
 * @brief ARMv8-M decoder vtable implementation
 *
 * Implements the abstract EmuDecoder interface for ARMv8-M architecture.
 */

#include "arch/armv8m/armv8m_decoder_vtable.h"
#include <string.h>

/*============================================================================
 * VTable Implementation
 *============================================================================*/

static void armv8m_decoder_destroy(EmuDecoder *dec)
{
    /* Nothing to free - we don't own any dynamic resources */
    (void)dec;
}

static int armv8m_decoder_decode(EmuDecoder *dec, const uint8_t *mem,
                                  uint64_t pc, EmuDecodedInsn *insn)
{
    if (!dec || !mem || !insn) {
        return EMU_ERR_INVALID_PARAM;
    }

    ARMv8MDecoder *arm_dec = armv8m_decoder_from_base(dec);

    /* Initialize the ARM-specific decoded instruction */
    armv8m_decode_init(&arm_dec->current_insn);

    /* Decode using the ARM decoder */
    int result = armv8m_decode(mem, (uint32_t)pc, &arm_dec->current_insn);
    if (result < 0) {
        return result;
    }

    /* Fill in the generic EmuDecodedInsn structure */
    insn->pc = pc;
    insn->encoding = arm_dec->current_insn.encoding;
    insn->size = arm_dec->current_insn.size;

    /* Classify the instruction */
    InstructionType type = arm_dec->current_insn.type;
    insn->is_branch = (type == INSN_BRANCH || type == INSN_BRANCH_LINK ||
                       type == INSN_BRANCH_EXCHANGE || type == INSN_BRANCH_LINK_EXCHANGE ||
                       type == INSN_COMPARE_BRANCH || type == INSN_TABLE_BRANCH);
    insn->is_call = (type == INSN_BRANCH_LINK || type == INSN_BRANCH_LINK_EXCHANGE);
    insn->is_return = (type == INSN_BRANCH_EXCHANGE &&
                       arm_dec->current_insn.rm == ARMV8M_REG_LR);
    insn->is_conditional = (arm_dec->current_insn.cond != COND_AL);

    /* Calculate branch target if statically known */
    if (insn->is_branch && !insn->is_return) {
        if (type == INSN_BRANCH || type == INSN_BRANCH_LINK ||
            type == INSN_COMPARE_BRANCH) {
            /* PC-relative branch with immediate offset */
            insn->branch_target = pc + 4 + (uint64_t)(int64_t)arm_dec->current_insn.branch_offset;
            insn->target_known = true;
        } else {
            /* Indirect branch - target not statically known */
            insn->branch_target = 0;
            insn->target_known = false;
        }
    } else {
        insn->branch_target = 0;
        insn->target_known = false;
    }

    /* Link to the architecture-specific decoded data */
    insn->arch_insn = &arm_dec->current_insn;

    return result;
}

static int armv8m_decoder_disassemble(EmuDecoder *dec, const EmuDecodedInsn *insn,
                                       char *buf, size_t size)
{
    if (!dec || !insn || !buf || size == 0) {
        return EMU_ERR_INVALID_PARAM;
    }

    const DecodedInsn *arm_insn = (const DecodedInsn *)insn->arch_insn;
    if (!arm_insn) {
        return EMU_ERR_INVALID_PARAM;
    }

    return armv8m_disasm(arm_insn, buf, size);
}

static int armv8m_decoder_get_max_insn_size(const EmuDecoder *dec)
{
    (void)dec;
    return 4;  /* Thumb-2 max is 4 bytes */
}

static void armv8m_decoder_init_insn(EmuDecoder *dec, EmuDecodedInsn *insn)
{
    (void)dec;
    if (insn) {
        memset(insn, 0, sizeof(*insn));
    }
}

static void armv8m_decoder_free_insn(EmuDecoder *dec, EmuDecodedInsn *insn)
{
    /* We use embedded storage, nothing to free */
    (void)dec;
    (void)insn;
}

/*============================================================================
 * Static VTable
 *============================================================================*/

static const EmuDecoderVTable armv8m_decoder_vtable = {
    .destroy = armv8m_decoder_destroy,
    .decode = armv8m_decoder_decode,
    .disassemble = armv8m_decoder_disassemble,
    .get_max_insn_size = armv8m_decoder_get_max_insn_size,
    .init_insn = armv8m_decoder_init_insn,
    .free_insn = armv8m_decoder_free_insn,
};

/*============================================================================
 * Public API
 *============================================================================*/

const EmuDecoderVTable *armv8m_decoder_get_vtable(void)
{
    return &armv8m_decoder_vtable;
}

void armv8m_decoder_vtable_init(ARMv8MDecoder *dec)
{
    if (!dec) {
        return;
    }

    memset(dec, 0, sizeof(*dec));
    dec->base.vtable = &armv8m_decoder_vtable;
    dec->base.arch = EMU_ARCH_ARMV8M;
    dec->base.arch_state = dec;
}
