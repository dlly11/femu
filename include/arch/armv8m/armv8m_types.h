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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define ARMV8M_NUM_REGS 16
#define ARMV8M_REG_SP 13
#define ARMV8M_REG_LR 14
#define ARMV8M_REG_PC 15
#define ARMV8M_REG_NONE 0xFF

/* Exception numbers */
#define ARMV8M_EXC_RESET 1
#define ARMV8M_EXC_NMI 2
#define ARMV8M_EXC_HARDFAULT 3
#define ARMV8M_EXC_MEMMANAGE 4
#define ARMV8M_EXC_BUSFAULT 5
#define ARMV8M_EXC_USAGEFAULT 6
#define ARMV8M_EXC_SECUREFAULT 7 /* TrustZone only */
#define ARMV8M_EXC_SVCALL 11
#define ARMV8M_EXC_DEBUGMON 12
#define ARMV8M_EXC_PENDSV 14
#define ARMV8M_EXC_SYSTICK 15
#define ARMV8M_EXC_EXTERNAL_BASE 16 /* External interrupts start here */

/* xPSR bit positions */
#define ARMV8M_XPSR_N (1U << 31) /* Negative flag */
#define ARMV8M_XPSR_Z (1U << 30) /* Zero flag */
#define ARMV8M_XPSR_C (1U << 29) /* Carry flag */
#define ARMV8M_XPSR_V (1U << 28) /* Overflow flag */
#define ARMV8M_XPSR_Q (1U << 27) /* Saturation flag */
#define ARMV8M_XPSR_T (1U << 24) /* Thumb state (always 1 for M-profile) */
#define ARMV8M_XPSR_GE_MASK (0xFU << 16) /* GE[3:0] flags for parallel ops */
#define ARMV8M_XPSR_GE_SHIFT 16

/* CONTROL register bits */
#define ARMV8M_CONTROL_NPRIV (1U << 0) /* Thread mode privilege */
#define ARMV8M_CONTROL_SPSEL (1U << 1) /* Stack pointer select */
#define ARMV8M_CONTROL_FPCA (1U << 2)  /* FP context active */
#define ARMV8M_CONTROL_SFPA (1U << 3)  /* Secure FP active (TZ) */

/* CCR (Configuration and Control Register) bits */
#define ARMV8M_CCR_UNALIGN_TRP (1U << 3) /* Unaligned access trap */
#define ARMV8M_CCR_DIV_0_TRP (1U << 4)   /* Divide by zero trap */
#define ARMV8M_CCR_BFHFNMIGN                                                   \
  (1U << 8)                           /* Ignore BusFault during HardFault/NMI */
#define ARMV8M_CCR_STKALIGN (1U << 9) /* Stack 8-byte alignment on exception   \
                                       */

/* CFSR (Configurable Fault Status Register) - combines MMFSR, BFSR, UFSR */
/* MMFSR bits [7:0] */
#define ARMV8M_MMFSR_IACCVIOL (1U << 0)  /* Instruction access violation */
#define ARMV8M_MMFSR_DACCVIOL (1U << 1)  /* Data access violation */
#define ARMV8M_MMFSR_MUNSTKERR (1U << 3) /* MemManage on unstacking */
#define ARMV8M_MMFSR_MSTKERR (1U << 4)   /* MemManage on stacking */
#define ARMV8M_MMFSR_MLSPERR                                                   \
  (1U << 5) /* MemManage on lazy FP state preservation */
#define ARMV8M_MMFSR_MMARVALID (1U << 7) /* MMFAR valid */

/* BFSR bits [15:8] */
#define ARMV8M_BFSR_IBUSERR (1U << 8)      /* Instruction bus error */
#define ARMV8M_BFSR_PRECISERR (1U << 9)    /* Precise data bus error */
#define ARMV8M_BFSR_IMPRECISERR (1U << 10) /* Imprecise data bus error */
#define ARMV8M_BFSR_UNSTKERR (1U << 11)    /* BusFault on unstacking */
#define ARMV8M_BFSR_STKERR (1U << 12)      /* BusFault on stacking */
#define ARMV8M_BFSR_LSPERR                                                     \
  (1U << 13) /* BusFault on lazy FP state preservation */
#define ARMV8M_BFSR_BFARVALID (1U << 15) /* BFAR valid */

/* UFSR bits [31:16] */
#define ARMV8M_UFSR_UNDEFINSTR (1U << 16) /* Undefined instruction */
#define ARMV8M_UFSR_INVSTATE (1U << 17)   /* Invalid state (T bit) */
#define ARMV8M_UFSR_INVPC (1U << 18)      /* Invalid PC load */
#define ARMV8M_UFSR_NOCP (1U << 19)       /* No coprocessor */
#define ARMV8M_UFSR_STKOF (1U << 20)      /* Stack overflow (v8-M) */
#define ARMV8M_UFSR_UNALIGNED (1U << 24)  /* Unaligned access */
#define ARMV8M_UFSR_DIVBYZERO (1U << 25)  /* Divide by zero */

/* FPSCR bits (FPU Status and Control Register) */
#define ARMV8M_FPSCR_N (1U << 31)          /* Negative flag */
#define ARMV8M_FPSCR_Z (1U << 30)          /* Zero flag */
#define ARMV8M_FPSCR_C (1U << 29)          /* Carry flag */
#define ARMV8M_FPSCR_V (1U << 28)          /* Overflow flag */
#define ARMV8M_FPSCR_AHP (1U << 26)        /* Alt half-precision */
#define ARMV8M_FPSCR_DN (1U << 25)         /* Default NaN */
#define ARMV8M_FPSCR_FZ (1U << 24)         /* Flush to zero */
#define ARMV8M_FPSCR_RMODE_MASK (3U << 22) /* Rounding mode mask */
#define ARMV8M_FPSCR_RMODE_RN (0U << 22)   /* Round to Nearest */
#define ARMV8M_FPSCR_RMODE_RP (1U << 22)   /* Round towards Plus infinity */
#define ARMV8M_FPSCR_RMODE_RM (2U << 22)   /* Round towards Minus infinity */
#define ARMV8M_FPSCR_RMODE_RZ (3U << 22)   /* Round towards Zero */

/* FPCCR bits (FPU Context Control Register - lazy preservation) */
#define ARMV8M_FPCCR_ASPEN (1U << 31) /* Auto state preservation enable */
#define ARMV8M_FPCCR_LSPEN (1U << 30) /* Lazy state preservation enable */
#define ARMV8M_FPCCR_LSPACT (1U << 0) /* Lazy state preservation active */

/* SAU (Security Attribution Unit) bits - TrustZone */
#define ARMV8M_SAU_CTRL_ENABLE (1U << 0) /* SAU enable */
#define ARMV8M_SAU_CTRL_ALLNS                                                  \
  (1U << 1) /* All memory non-secure when SAU disabled */
#define ARMV8M_SAU_RLAR_ENABLE (1U << 0) /* Region enable */
#define ARMV8M_SAU_RLAR_NSC (1U << 1)    /* Non-Secure Callable */

/* SAU region count */
#define ARMV8M_SAU_REGIONS_MAX 8

/*============================================================================
 * Instruction Types
 *============================================================================*/

typedef enum {
  INSN_UNDEFINED = 0,

  /* Data processing */
  INSN_DATA_PROC_IMM,     /* ADD, SUB, MOV, CMP with immediate */
  INSN_DATA_PROC_REG,     /* ADD, SUB, etc. with register */
  INSN_DATA_PROC_SHIFTED, /* With shifted register operand */
  INSN_MULTIPLY,          /* MUL, MLA, etc. */
  INSN_DIVIDE,            /* SDIV, UDIV */
  INSN_SATURATE,          /* SSAT, USAT */
  INSN_SAT_ARITH,         /* QADD, QSUB, QDADD, QDSUB */
  INSN_PARALLEL,          /* Parallel add/sub (SADD16, SSUB8, etc.) */
  INSN_PACK,              /* PKHBT, PKHTB */
  INSN_BITFIELD,          /* BFI, BFC, UBFX, SBFX */
  INSN_EXTEND,            /* SXTH, UXTH, etc. */

  /* Load/Store */
  INSN_LOAD_IMM,        /* LDR with immediate offset */
  INSN_LOAD_REG,        /* LDR with register offset */
  INSN_LOAD_LITERAL,    /* LDR from PC-relative */
  INSN_STORE_IMM,       /* STR with immediate offset */
  INSN_STORE_REG,       /* STR with register offset */
  INSN_LOAD_MULTIPLE,   /* LDM, POP */
  INSN_STORE_MULTIPLE,  /* STM, PUSH */
  INSN_LOAD_EXCLUSIVE,  /* LDREX */
  INSN_STORE_EXCLUSIVE, /* STREX */
  INSN_CLEAR_EXCLUSIVE, /* CLREX */
  INSN_LOAD_ACQUIRE,    /* LDA, LDAB, LDAH, LDAEX, LDAEXB, LDAEXH */
  INSN_STORE_RELEASE,   /* STL, STLB, STLH, STLEX, STLEXB, STLEXH */

  /* Branch */
  INSN_BRANCH,               /* B */
  INSN_BRANCH_LINK,          /* BL */
  INSN_BRANCH_EXCHANGE,      /* BX */
  INSN_BRANCH_LINK_EXCHANGE, /* BLX */
  INSN_COMPARE_BRANCH,       /* CBZ, CBNZ */
  INSN_TABLE_BRANCH,         /* TBB, TBH */

  /* System */
  INSN_SVC,     /* Supervisor call */
  INSN_MRS,     /* Move from special register */
  INSN_MSR,     /* Move to special register */
  INSN_CPS,     /* Change processor state */
  INSN_BARRIER, /* DMB, DSB, ISB */
  INSN_HINT,    /* NOP, WFI, WFE, SEV, YIELD */
  INSN_IT,      /* If-Then */

  /* TrustZone (optional) */
  INSN_SG,    /* Secure Gateway */
  INSN_BXNS,  /* Branch with exchange to non-secure */
  INSN_BLXNS, /* Branch with link and exchange to NS */
  INSN_TT,    /* Test Target */

  /* Coprocessor (optional) */
  INSN_MCR, /* Move to coprocessor */
  INSN_MRC, /* Move from coprocessor */

  /* FPU (optional) */
  INSN_FPU_LOAD,  /* VLDR */
  INSN_FPU_STORE, /* VSTR */
  INSN_FPU_MOVE,  /* VMOV variants */
  INSN_FPU_ARITH, /* VADD, VSUB, VMUL, VDIV, etc. */
  INSN_FPU_CMP,   /* VCMP, VCMPE */
  INSN_FPU_CVT,   /* VCVT (conversions) */
  INSN_FPU_MULTI, /* VPUSH, VPOP, VLDM, VSTM */

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
  DP_TEQ,
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
 * Multiply Operations
 *============================================================================*/

typedef enum {
  MUL_MUL = 0, /* MUL - 32-bit multiply */
  MUL_MLA,     /* MLA - multiply accumulate */
  MUL_MLS,     /* MLS - multiply subtract */
  MUL_SMULL,   /* SMULL - signed long multiply */
  MUL_UMULL,   /* UMULL - unsigned long multiply */
  MUL_SMLAL,   /* SMLAL - signed long multiply accumulate */
  MUL_UMLAL,   /* UMLAL - unsigned long multiply accumulate */
  /* DSP halfword multiply instructions */
  MUL_SMULBB, /* SMULBB - signed halfword multiply (bottom, bottom) */
  MUL_SMULBT, /* SMULBT - signed halfword multiply (bottom, top) */
  MUL_SMULTB, /* SMULTB - signed halfword multiply (top, bottom) */
  MUL_SMULTT, /* SMULTT - signed halfword multiply (top, top) */
  MUL_SMLABB, /* SMLABB - signed halfword multiply-accumulate */
  MUL_SMLABT, /* SMLABT - signed halfword multiply-accumulate */
  MUL_SMLATB, /* SMLATB - signed halfword multiply-accumulate */
  MUL_SMLATT, /* SMLATT - signed halfword multiply-accumulate */
  MUL_SMULWB, /* SMULWB - signed halfword x word multiply (bottom) */
  MUL_SMULWT, /* SMULWT - signed halfword x word multiply (top) */
  MUL_SMLAWB, /* SMLAWB - signed halfword x word multiply-accumulate */
  MUL_SMLAWT, /* SMLAWT - signed halfword x word multiply-accumulate */
  /* DSP dual multiply instructions */
  MUL_SMUAD,  /* SMUAD - signed dual multiply add */
  MUL_SMUADX, /* SMUADX - signed dual multiply add (exchange) */
  MUL_SMUSD,  /* SMUSD - signed dual multiply subtract */
  MUL_SMUSDX, /* SMUSDX - signed dual multiply subtract (exchange) */
  MUL_SMLAD,  /* SMLAD - signed dual multiply-accumulate add */
  MUL_SMLADX, /* SMLADX - signed dual multiply-accumulate add (exchange) */
  MUL_SMLSD,  /* SMLSD - signed dual multiply-accumulate subtract */
  MUL_SMLSDX, /* SMLSDX - signed dual multiply-accumulate subtract (exchange) */
  MUL_SMLALD, /* SMLALD - signed dual multiply-accumulate long */
  MUL_SMLALDX, /* SMLALDX - signed dual multiply-accumulate long (exchange) */
  MUL_SMLSLD,  /* SMLSLD - signed dual multiply-subtract long */
  MUL_SMLSLDX, /* SMLSLDX - signed dual multiply-subtract long (exchange) */
  /* Most significant word multiply */
  MUL_SMMUL,  /* SMMUL - signed most significant word multiply */
  MUL_SMMULR, /* SMMULR - signed most significant word multiply (round) */
  MUL_SMMLA,  /* SMMLA - signed most significant word multiply-accumulate */
  MUL_SMMLAR, /* SMMLAR - signed most significant word multiply-accumulate
                 (round) */
  MUL_SMMLS,  /* SMMLS - signed most significant word multiply-subtract */
  MUL_SMMLSR, /* SMMLSR - signed most significant word multiply-subtract (round)
               */
  /* Unsigned sum of absolute differences */
  MUL_USAD8,  /* USAD8 - unsigned sum of absolute differences */
  MUL_USADA8, /* USADA8 - unsigned sum of absolute differences accumulate */
  /* Long multiply accumulate halfword */
  MUL_SMLALBB, /* SMLALBB - signed long halfword multiply-accumulate */
  MUL_SMLALBT, /* SMLALBT - signed long halfword multiply-accumulate */
  MUL_SMLALTB, /* SMLALTB - signed long halfword multiply-accumulate */
  MUL_SMLALTT, /* SMLALTT - signed long halfword multiply-accumulate */
} MultiplyOp;

/*============================================================================
 * Shift Types
 *============================================================================*/

typedef enum {
  SHIFT_LSL = 0, /* Logical shift left */
  SHIFT_LSR = 1, /* Logical shift right */
  SHIFT_ASR = 2, /* Arithmetic shift right */
  SHIFT_ROR = 3, /* Rotate right */
  SHIFT_RRX = 4, /* Rotate right with extend (shift by 1 with carry in) */
} ShiftType;

/*============================================================================
 * Condition Codes
 *============================================================================*/

typedef enum {
  COND_EQ = 0,  /* Equal (Z=1) */
  COND_NE = 1,  /* Not equal (Z=0) */
  COND_CS = 2,  /* Carry set / unsigned higher or same (C=1) */
  COND_CC = 3,  /* Carry clear / unsigned lower (C=0) */
  COND_MI = 4,  /* Minus / negative (N=1) */
  COND_PL = 5,  /* Plus / positive or zero (N=0) */
  COND_VS = 6,  /* Overflow (V=1) */
  COND_VC = 7,  /* No overflow (V=0) */
  COND_HI = 8,  /* Unsigned higher (C=1 && Z=0) */
  COND_LS = 9,  /* Unsigned lower or same (C=0 || Z=1) */
  COND_GE = 10, /* Signed greater than or equal (N==V) */
  COND_LT = 11, /* Signed less than (N!=V) */
  COND_GT = 12, /* Signed greater than (Z=0 && N==V) */
  COND_LE = 13, /* Signed less than or equal (Z=1 || N!=V) */
  COND_AL = 14, /* Always */
  COND_NV = 15, /* Never (used for some encodings) */
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
  ARMV8M_ERR_SECURE_FAULT = -9,   /* TrustZone security violation */
  ARMV8M_ERR_INVALID_PARAM = -10, /* Invalid parameter */
  ARMV8M_ERR_WATCHPOINT = -11,    /* Hit a watchpoint */
} ARMv8MError;

/*============================================================================
 * Special Register IDs (for MRS/MSR and debug access)
 *============================================================================*/

typedef enum {
  ARMV8M_SYSREG_PRIMASK = 0,
  ARMV8M_SYSREG_BASEPRI,
  ARMV8M_SYSREG_FAULTMASK,
  ARMV8M_SYSREG_CONTROL,
  ARMV8M_SYSREG_MSP,
  ARMV8M_SYSREG_PSP,
  ARMV8M_SYSREG_MSPLIM,
  ARMV8M_SYSREG_PSPLIM,
} SystemRegister;

/*============================================================================
 * Callback Types
 *============================================================================*/

/* Memory access callback (for MMIO dispatch to peripherals) */
typedef uint32_t (*MemReadCallback)(void *ctx, uint32_t addr, uint8_t size);
typedef void (*MemWriteCallback)(void *ctx, uint32_t addr, uint32_t value,
                                 uint8_t size);

/* IRQ callback (peripheral -> NVIC) */
typedef void (*IRQCallback)(void *ctx, int irq, int level);

#ifdef __cplusplus
}
#endif

#endif /* ARMV8M_TYPES_H */
