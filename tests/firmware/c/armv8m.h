/* ARMv8-M Intrinsics and System Register Access
 *
 * Provides inline assembly wrappers for:
 * - System register access (PRIMASK, BASEPRI, PSP, etc.)
 * - Bit manipulation (CLZ, RBIT, REV, etc.)
 * - DSP intrinsics (SADD16, QADD8, SHADD16, etc.)
 * - Saturation (SSAT, USAT, QADD, QSUB)
 * - Barriers (DMB, DSB, ISB)
 * - Exclusive access (LDREX, STREX, CLREX)
 */

#ifndef ARMV8M_H
#define ARMV8M_H

#include <stdint.h>

/* ========== System Register Access ========== */

static inline uint32_t get_primask(void) {
    uint32_t result;
    __asm__ volatile ("mrs %0, primask" : "=r" (result));
    return result;
}

static inline void set_primask(uint32_t value) {
    __asm__ volatile ("msr primask, %0" : : "r" (value));
}

static inline void enable_interrupts(void) {
    __asm__ volatile ("cpsie i" ::: "memory");
}

static inline void disable_interrupts(void) {
    __asm__ volatile ("cpsid i" ::: "memory");
}

static inline uint32_t get_basepri(void) {
    uint32_t result;
    __asm__ volatile ("mrs %0, basepri" : "=r" (result));
    return result;
}

static inline void set_basepri(uint32_t value) {
    __asm__ volatile ("msr basepri, %0" : : "r" (value));
}

static inline uint32_t get_faultmask(void) {
    uint32_t result;
    __asm__ volatile ("mrs %0, faultmask" : "=r" (result));
    return result;
}

static inline void set_faultmask(uint32_t value) {
    __asm__ volatile ("msr faultmask, %0" : : "r" (value));
}

static inline uint32_t get_control(void) {
    uint32_t result;
    __asm__ volatile ("mrs %0, control" : "=r" (result));
    return result;
}

static inline void set_control(uint32_t value) {
    __asm__ volatile ("msr control, %0" : : "r" (value) : "memory");
}

static inline uint32_t get_psp(void) {
    uint32_t result;
    __asm__ volatile ("mrs %0, psp" : "=r" (result));
    return result;
}

static inline void set_psp(uint32_t value) {
    __asm__ volatile ("msr psp, %0" : : "r" (value));
}

static inline uint32_t get_msp(void) {
    uint32_t result;
    __asm__ volatile ("mrs %0, msp" : "=r" (result));
    return result;
}

static inline void set_msp(uint32_t value) {
    __asm__ volatile ("msr msp, %0" : : "r" (value));
}

/* ========== Bit Manipulation ========== */

static inline uint32_t clz(uint32_t value) {
    uint32_t result;
    __asm__ volatile ("clz %0, %1" : "=r" (result) : "r" (value));
    return result;
}

static inline uint32_t rbit(uint32_t value) {
    uint32_t result;
    __asm__ volatile ("rbit %0, %1" : "=r" (result) : "r" (value));
    return result;
}

static inline uint32_t rev(uint32_t value) {
    uint32_t result;
    __asm__ volatile ("rev %0, %1" : "=r" (result) : "r" (value));
    return result;
}

static inline uint32_t rev16(uint32_t value) {
    uint32_t result;
    __asm__ volatile ("rev16 %0, %1" : "=r" (result) : "r" (value));
    return result;
}

static inline int32_t revsh(int32_t value) {
    int32_t result;
    __asm__ volatile ("revsh %0, %1" : "=r" (result) : "r" (value));
    return result;
}

/* ========== DSP Parallel Add/Subtract (Signed) ========== */

static inline uint32_t sadd16(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("sadd16 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t sadd8(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("sadd8 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t ssub16(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("ssub16 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t ssub8(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("ssub8 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

/* ========== DSP Parallel Add/Subtract (Unsigned) ========== */

static inline uint32_t uadd16(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("uadd16 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t uadd8(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("uadd8 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t usub16(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("usub16 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t usub8(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("usub8 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

/* ========== DSP Halving Add/Subtract (Signed) ========== */

static inline uint32_t shadd16(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("shadd16 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t shadd8(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("shadd8 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t shsub16(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("shsub16 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t shsub8(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("shsub8 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

/* ========== DSP Halving Add/Subtract (Unsigned) ========== */

static inline uint32_t uhadd16(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("uhadd16 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t uhadd8(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("uhadd8 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t uhsub16(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("uhsub16 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t uhsub8(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("uhsub8 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

/* ========== DSP Saturating Add/Subtract (Signed) ========== */

static inline uint32_t qadd16(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("qadd16 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t qadd8(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("qadd8 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t qsub16(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("qsub16 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t qsub8(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("qsub8 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

/* ========== DSP Saturating Add/Subtract (Unsigned) ========== */

static inline uint32_t uqadd16(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("uqadd16 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t uqadd8(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("uqadd8 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t uqsub16(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("uqsub16 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t uqsub8(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("uqsub8 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

/* ========== DSP Exchange Add/Subtract ========== */

static inline uint32_t sasx(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("sasx %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t ssax(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("ssax %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t uasx(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("uasx %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t usax(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("usax %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t shasx(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("shasx %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t shsax(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("shsax %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t uhasx(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("uhasx %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t uhsax(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("uhsax %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t qasx(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("qasx %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t qsax(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("qsax %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t uqasx(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("uqasx %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t uqsax(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("uqsax %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

/* ========== 32-bit Saturating Arithmetic ========== */

static inline int32_t qadd(int32_t a, int32_t b) {
    int32_t result;
    __asm__ volatile ("qadd %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline int32_t qsub(int32_t a, int32_t b) {
    int32_t result;
    __asm__ volatile ("qsub %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline int32_t qdadd(int32_t a, int32_t b) {
    int32_t result;
    __asm__ volatile ("qdadd %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline int32_t qdsub(int32_t a, int32_t b) {
    int32_t result;
    __asm__ volatile ("qdsub %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

/* SSAT/USAT - note: saturation bit must be a constant */
#define ssat(value, sat_bit) ({ \
    int32_t _result; \
    __asm__ volatile ("ssat %0, %1, %2" : "=r" (_result) : "I" (sat_bit), "r" (value)); \
    _result; \
})

#define usat(value, sat_bit) ({ \
    uint32_t _result; \
    __asm__ volatile ("usat %0, %1, %2" : "=r" (_result) : "I" (sat_bit), "r" (value)); \
    _result; \
})

/* ========== Barriers ========== */

static inline void dmb(void) {
    __asm__ volatile ("dmb" ::: "memory");
}

static inline void dsb(void) {
    __asm__ volatile ("dsb" ::: "memory");
}

static inline void isb(void) {
    __asm__ volatile ("isb" ::: "memory");
}

/* ========== Exclusive Access ========== */

static inline uint32_t ldrex(volatile uint32_t *addr) {
    uint32_t result;
    __asm__ volatile ("ldrex %0, [%1]" : "=r" (result) : "r" (addr) : "memory");
    return result;
}

static inline uint32_t strex(uint32_t value, volatile uint32_t *addr) {
    uint32_t result;
    __asm__ volatile ("strex %0, %1, [%2]" : "=&r" (result) : "r" (value), "r" (addr) : "memory");
    return result;
}

static inline uint8_t ldrexb(volatile uint8_t *addr) {
    uint8_t result;
    __asm__ volatile ("ldrexb %0, [%1]" : "=r" (result) : "r" (addr) : "memory");
    return result;
}

static inline uint32_t strexb(uint8_t value, volatile uint8_t *addr) {
    uint32_t result;
    __asm__ volatile ("strexb %0, %1, [%2]" : "=&r" (result) : "r" (value), "r" (addr) : "memory");
    return result;
}

static inline uint16_t ldrexh(volatile uint16_t *addr) {
    uint16_t result;
    __asm__ volatile ("ldrexh %0, [%1]" : "=r" (result) : "r" (addr) : "memory");
    return result;
}

static inline uint32_t strexh(uint16_t value, volatile uint16_t *addr) {
    uint32_t result;
    __asm__ volatile ("strexh %0, %1, [%2]" : "=&r" (result) : "r" (value), "r" (addr) : "memory");
    return result;
}

static inline void clrex(void) {
    __asm__ volatile ("clrex" ::: "memory");
}

/* ========== Pack Halfword ========== */

static inline uint32_t pkhbt(uint32_t a, uint32_t b, uint32_t shift) {
    uint32_t result;
    /* For variable shifts, we need to use different approach */
    result = (a & 0xFFFF) | ((b << shift) & 0xFFFF0000);
    return result;
}

static inline uint32_t pkhtb(uint32_t a, uint32_t b, uint32_t shift) {
    uint32_t result;
    result = (a & 0xFFFF0000) | (((int32_t)b >> shift) & 0xFFFF);
    return result;
}

/* Pack halfword with constant shift (uses actual instruction) */
#define pkhbt_lsl(a, b, shift) ({ \
    uint32_t _result; \
    __asm__ volatile ("pkhbt %0, %1, %2, lsl %3" : "=r" (_result) : "r" (a), "r" (b), "I" (shift)); \
    _result; \
})

#define pkhtb_asr(a, b, shift) ({ \
    uint32_t _result; \
    __asm__ volatile ("pkhtb %0, %1, %2, asr %3" : "=r" (_result) : "r" (a), "r" (b), "I" (shift)); \
    _result; \
})

/* ========== Multiply Instructions ========== */

static inline int32_t smulbb(int32_t a, int32_t b) {
    int32_t result;
    __asm__ volatile ("smulbb %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline int32_t smulbt(int32_t a, int32_t b) {
    int32_t result;
    __asm__ volatile ("smulbt %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline int32_t smultb(int32_t a, int32_t b) {
    int32_t result;
    __asm__ volatile ("smultb %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline int32_t smultt(int32_t a, int32_t b) {
    int32_t result;
    __asm__ volatile ("smultt %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline int32_t smlabb(int32_t a, int32_t b, int32_t acc) {
    int32_t result;
    __asm__ volatile ("smlabb %0, %1, %2, %3" : "=r" (result) : "r" (a), "r" (b), "r" (acc));
    return result;
}

static inline int32_t smulwb(int32_t a, int32_t b) {
    int32_t result;
    __asm__ volatile ("smulwb %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline int32_t smulwt(int32_t a, int32_t b) {
    int32_t result;
    __asm__ volatile ("smulwt %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline int32_t smuad(int32_t a, int32_t b) {
    int32_t result;
    __asm__ volatile ("smuad %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline int32_t smuadx(int32_t a, int32_t b) {
    int32_t result;
    __asm__ volatile ("smuadx %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline int32_t smusd(int32_t a, int32_t b) {
    int32_t result;
    __asm__ volatile ("smusd %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline int32_t smmul(int32_t a, int32_t b) {
    int32_t result;
    __asm__ volatile ("smmul %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline int32_t smmla(int32_t a, int32_t b, int32_t acc) {
    int32_t result;
    __asm__ volatile ("smmla %0, %1, %2, %3" : "=r" (result) : "r" (a), "r" (b), "r" (acc));
    return result;
}

static inline uint32_t usad8(uint32_t a, uint32_t b) {
    uint32_t result;
    __asm__ volatile ("usad8 %0, %1, %2" : "=r" (result) : "r" (a), "r" (b));
    return result;
}

static inline uint32_t usada8(uint32_t a, uint32_t b, uint32_t acc) {
    uint32_t result;
    __asm__ volatile ("usada8 %0, %1, %2, %3" : "=r" (result) : "r" (a), "r" (b), "r" (acc));
    return result;
}

/* ========== Bit Field Instructions ========== */

/* Note: For BFI/BFC/UBFX/SBFX, the bit positions must be constants */
#define bfi(rd, rn, lsb, width) ({ \
    uint32_t _result = (rd); \
    __asm__ volatile ("bfi %0, %1, %2, %3" : "+r" (_result) : "r" (rn), "I" (lsb), "I" (width)); \
    _result; \
})

#define bfc(rd, lsb, width) ({ \
    uint32_t _result = (rd); \
    __asm__ volatile ("bfc %0, %1, %2" : "+r" (_result) : "I" (lsb), "I" (width)); \
    _result; \
})

#define ubfx(rn, lsb, width) ({ \
    uint32_t _result; \
    __asm__ volatile ("ubfx %0, %1, %2, %3" : "=r" (_result) : "r" (rn), "I" (lsb), "I" (width)); \
    _result; \
})

#define sbfx(rn, lsb, width) ({ \
    int32_t _result; \
    __asm__ volatile ("sbfx %0, %1, %2, %3" : "=r" (_result) : "r" (rn), "I" (lsb), "I" (width)); \
    _result; \
})

/* ========== SVC Call ========== */

#define svc(number) __asm__ volatile ("svc %0" : : "I" (number))

/* ========== Misc ========== */

static inline void nop(void) {
    __asm__ volatile ("nop");
}

static inline void wfi(void) {
    __asm__ volatile ("wfi");
}

static inline void wfe(void) {
    __asm__ volatile ("wfe");
}

static inline void sev(void) {
    __asm__ volatile ("sev");
}

#endif /* ARMV8M_H */
