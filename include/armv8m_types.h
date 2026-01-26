/**
 * @file armv8m_types.h
 * @brief Core type definitions for ARMv8-M emulator
 * 
 * AI INSTRUCTIONS:
 * - This file defines shared types used across all modules
 * - Do NOT modify without updating all dependent modules
 * - All modules should include this file
 */

#ifndef ARMV8M_TYPES_H
#define ARMV8M_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define ARMV8M_NUM_REGS     16
#define ARMV8M_REG_SP       13
#define ARMV8M_REG_LR       14
#define ARMV8M_REG_PC       15
#define ARMV8M_REG_NONE     0xFF

/* Exception numbers */
#define ARMV8M_EXC_RESET            1
#define ARMV8M_EXC_NMI              2
#define ARMV8M_EXC_HARDFAULT        3
#define ARMV8M_EXC_MEMMANAGE        4
#define ARMV8M_EXC_BUSFAULT         5
#define ARMV8M_EXC_USAGEFAULT       6
#define ARMV8M_EXC_SECUREFAULT      7   /* TrustZone only */
#define ARMV8M_EXC_SVCALL           11
#define ARMV8M_EXC_DEBUGMON         12
#define ARMV8M_EXC_PENDSV           14
#define ARMV8M_EXC_SYSTICK          15
#define ARMV8M_EXC_EXTERNAL_BASE    16  /* External interrupts start here */

/* xPSR bit positions */
#define ARMV8M_XPSR_N       (1U << 31)  /* Negative flag */
#define ARMV8M_XPSR_Z       (1U << 30)  /* Zero flag */
#define ARMV8M_XPSR_C       (1U << 29)  /* Carry flag */
#define ARMV8M_XPSR_V       (1U << 28)  /* Overflow flag */
#define ARMV8M_XPSR_Q       (1U << 27)  /* Saturation flag */
#define ARMV8M_XPSR_T       (1U << 24)  /* Thumb state (always 1 for M-profile) */

/* CONTROL register bits */
#define ARMV8M_CONTROL_NPRIV    (1U << 0)   /* Thread mode privilege */
#define ARMV8M_CONTROL_SPSEL    (1U << 1)   /* Stack pointer select */
#define ARMV8M_CONTROL_FPCA     (1U << 2)   /* FP context active */
#define ARMV8M_CONTROL_SFPA     (1U << 3)   /* Secure FP active (TZ) */

/*============================================================================
 * Instruction Types
 *============================================================================*/

typedef enum {
    INSN_UNDEFINED = 0,
    
    /* Data processing */
    INSN_DATA_PROC_IMM,         /* ADD, SUB, MOV, CMP with immediate */
    INSN_DATA_PROC_REG,         /* ADD, SUB, etc. with register */
    INSN_DATA_PROC_SHIFTED,     /* With shifted register operand */
    INSN_MULTIPLY,              /* MUL, MLA, etc. */
    INSN_DIVIDE,                /* SDIV, UDIV */
    INSN_SATURATE,              /* SSAT, USAT */
    INSN_BITFIELD,              /* BFI, BFC, UBFX, SBFX */
    INSN_EXTEND,                /* SXTH, UXTH, etc. */
    
    /* Load/Store */
    INSN_LOAD_IMM,              /* LDR with immediate offset */
    INSN_LOAD_REG,              /* LDR with register offset */
    INSN_LOAD_LITERAL,          /* LDR from PC-relative */
    INSN_STORE_IMM,             /* STR with immediate offset */
    INSN_STORE_REG,             /* STR with register offset */
    INSN_LOAD_MULTIPLE,         /* LDM, POP */
    INSN_STORE_MULTIPLE,        /* STM, PUSH */
    INSN_LOAD_EXCLUSIVE,        /* LDREX */
    INSN_STORE_EXCLUSIVE,       /* STREX */
    
    /* Branch */
    INSN_BRANCH,                /* B */
    INSN_BRANCH_LINK,           /* BL */
    INSN_BRANCH_EXCHANGE,       /* BX */
    INSN_BRANCH_LINK_EXCHANGE,  /* BLX */
    INSN_COMPARE_BRANCH,        /* CBZ, CBNZ */
    INSN_TABLE_BRANCH,          /* TBB, TBH */
    
    /* System */
    INSN_SVC,                   /* Supervisor call */
    INSN_MRS,                   /* Move from special register */
    INSN_MSR,                   /* Move to special register */
    INSN_CPS,                   /* Change processor state */
    INSN_BARRIER,               /* DMB, DSB, ISB */
    INSN_HINT,                  /* NOP, WFI, WFE, SEV, YIELD */
    INSN_IT,                    /* If-Then */
    
    /* TrustZone (optional) */
    INSN_SG,                    /* Secure Gateway */
    INSN_BXNS,                  /* Branch with exchange to non-secure */
    INSN_BLXNS,                 /* Branch with link and exchange to NS */
    INSN_TT,                    /* Test Target */
    
    /* Coprocessor (optional) */
    INSN_MCR,                   /* Move to coprocessor */
    INSN_MRC,                   /* Move from coprocessor */
    
    INSN_TYPE_COUNT
} InstructionType;

/*============================================================================
 * Data Processing Operations
 *============================================================================*/

typedef enum {
    DP_AND = 0,
    DP_EOR,
    DP_LSL,
    DP_LSR,
    DP_ASR,
    DP_ADC,
    DP_SBC,
    DP_ROR,
    DP_TST,
    DP_RSB,
    DP_CMP,
    DP_CMN,
    DP_ORR,
    DP_MUL,
    DP_BIC,
    DP_MVN,
    DP_ADD,
    DP_SUB,
    DP_MOV,
    DP_ORN,
} DataProcOp;

/*============================================================================
 * Shift Types
 *============================================================================*/

typedef enum {
    SHIFT_LSL = 0,  /* Logical shift left */
    SHIFT_LSR = 1,  /* Logical shift right */
    SHIFT_ASR = 2,  /* Arithmetic shift right */
    SHIFT_ROR = 3,  /* Rotate right */
    SHIFT_RRX = 4,  /* Rotate right with extend (shift by 1 with carry in) */
} ShiftType;

/*============================================================================
 * Condition Codes
 *============================================================================*/

typedef enum {
    COND_EQ = 0,    /* Equal (Z=1) */
    COND_NE = 1,    /* Not equal (Z=0) */
    COND_CS = 2,    /* Carry set / unsigned higher or same (C=1) */
    COND_CC = 3,    /* Carry clear / unsigned lower (C=0) */
    COND_MI = 4,    /* Minus / negative (N=1) */
    COND_PL = 5,    /* Plus / positive or zero (N=0) */
    COND_VS = 6,    /* Overflow (V=1) */
    COND_VC = 7,    /* No overflow (V=0) */
    COND_HI = 8,    /* Unsigned higher (C=1 && Z=0) */
    COND_LS = 9,    /* Unsigned lower or same (C=0 || Z=1) */
    COND_GE = 10,   /* Signed greater than or equal (N==V) */
    COND_LT = 11,   /* Signed less than (N!=V) */
    COND_GT = 12,   /* Signed greater than (Z=0 && N==V) */
    COND_LE = 13,   /* Signed less than or equal (Z=1 || N!=V) */
    COND_AL = 14,   /* Always */
    COND_NV = 15,   /* Never (used for some encodings) */
} ConditionCode;

/*============================================================================
 * Access Size
 *============================================================================*/

typedef enum {
    ACCESS_BYTE = 1,
    ACCESS_HALF = 2,
    ACCESS_WORD = 4,
} AccessSize;

/*============================================================================
 * Execution Mode
 *============================================================================*/

typedef enum {
    MODE_THREAD = 0,
    MODE_HANDLER = 1,
} ExecutionMode;

typedef enum {
    PRIV_PRIVILEGED = 0,
    PRIV_UNPRIVILEGED = 1,
} PrivilegeLevel;

typedef enum {
    SECURITY_SECURE = 0,
    SECURITY_NONSECURE = 1,
} SecurityState;

/*============================================================================
 * Error Codes
 *============================================================================*/

typedef enum {
    ARMV8M_OK = 0,
    ARMV8M_ERR_UNDEFINED_INSN = -1,
    ARMV8M_ERR_UNPREDICTABLE = -2,
    ARMV8M_ERR_BUS_FAULT = -3,
    ARMV8M_ERR_MEM_FAULT = -4,
    ARMV8M_ERR_USAGE_FAULT = -5,
    ARMV8M_ERR_HARD_FAULT = -6,
    ARMV8M_ERR_BREAKPOINT = -7,
    ARMV8M_ERR_HALTED = -8,
} ARMv8MError;

/*============================================================================
 * Callback Types
 *============================================================================*/

/* Memory access callback (for MMIO dispatch to peripherals) */
typedef uint32_t (*MemReadCallback)(void *ctx, uint32_t addr, uint8_t size);
typedef void (*MemWriteCallback)(void *ctx, uint32_t addr, uint32_t value, uint8_t size);

/* IRQ callback (peripheral -> NVIC) */
typedef void (*IRQCallback)(void *ctx, int irq, int level);

#ifdef __cplusplus
}
#endif

#endif /* ARMV8M_TYPES_H */
