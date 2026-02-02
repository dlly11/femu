/* Test Framework for ARMv8-M Emulator Tests
 *
 * Provides lightweight assertion macros that write results to memory
 * for verification by the Python test harness.
 *
 * Memory layout (defined in link.ld):
 *   0x20000000 - 0x200000FF: Test result area (256 bytes)
 *   0x20000100 - 0x20000107: Test counters (pass/fail)
 *   0x20000108+: .data, .bss, and other variables
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdint.h>

/* Result memory base address - fixed at start of RAM */
#define RESULT_BASE 0x20000000

/* Test counters at fixed addresses after result area */
#define TEST_COUNTERS_BASE 0x20000100

/* Done markers */
#define DONE_SUCCESS 0xC0FFEE42
#define DONE_FAILURE 0xDEADDEAD

/* Store a value at a specific offset from RESULT_BASE */
#define STORE_RESULT(offset, value) \
    do { \
        volatile uint32_t *addr = (volatile uint32_t *)(RESULT_BASE + (offset)); \
        *addr = (value); \
    } while (0)

/* Store a float as its bit pattern at a specific offset */
#define STORE_RESULT_FLOAT(offset, fvalue) \
    do { \
        volatile uint32_t *addr = (volatile uint32_t *)(RESULT_BASE + (offset)); \
        union { float f; uint32_t u; } conv; \
        conv.f = (fvalue); \
        *addr = conv.u; \
    } while (0)

/* Test result counters at fixed addresses */
static volatile uint32_t *const test_pass_count = (volatile uint32_t *)TEST_COUNTERS_BASE;
static volatile uint32_t *const test_fail_count = (volatile uint32_t *)(TEST_COUNTERS_BASE + 4);

/* Initialize test framework */
static inline void test_init(void) {
    *test_pass_count = 0;
    *test_fail_count = 0;
}

/* Record test result */
static inline void test_record(int passed) {
    if (passed) {
        (*test_pass_count)++;
    } else {
        (*test_fail_count)++;
    }
}

/* Assert equality for 32-bit values */
#define ASSERT_EQ(actual, expected) \
    do { \
        uint32_t _a = (actual); \
        uint32_t _e = (expected); \
        test_record(_a == _e); \
    } while (0)

/* Assert equality for signed 32-bit values */
#define ASSERT_EQ_SIGNED(actual, expected) \
    do { \
        int32_t _a = (actual); \
        int32_t _e = (expected); \
        test_record(_a == _e); \
    } while (0)

/* Assert true (non-zero) */
#define ASSERT_TRUE(condition) \
    do { \
        test_record((condition) != 0); \
    } while (0)

/* Assert false (zero) */
#define ASSERT_FALSE(condition) \
    do { \
        test_record((condition) == 0); \
    } while (0)

/* Assert less than */
#define ASSERT_LT(actual, expected) \
    do { \
        int32_t _a = (actual); \
        int32_t _e = (expected); \
        test_record(_a < _e); \
    } while (0)

/* Assert less than or equal */
#define ASSERT_LE(actual, expected) \
    do { \
        int32_t _a = (actual); \
        int32_t _e = (expected); \
        test_record(_a <= _e); \
    } while (0)

/* Assert greater than */
#define ASSERT_GT(actual, expected) \
    do { \
        int32_t _a = (actual); \
        int32_t _e = (expected); \
        test_record(_a > _e); \
    } while (0)

/* Assert greater than or equal */
#define ASSERT_GE(actual, expected) \
    do { \
        int32_t _a = (actual); \
        int32_t _e = (expected); \
        test_record(_a >= _e); \
    } while (0)

/* Assert float equality by bit pattern */
#define ASSERT_FLOAT_BITS_EQ(actual, expected_bits) \
    do { \
        union { float f; uint32_t u; } _conv; \
        _conv.f = (actual); \
        test_record(_conv.u == (expected_bits)); \
    } while (0)

/* Assert float values are approximately equal (within epsilon) */
#define ASSERT_FLOAT_NEAR(actual, expected, epsilon) \
    do { \
        float _a = (actual); \
        float _e = (expected); \
        float _diff = _a - _e; \
        if (_diff < 0) _diff = -_diff; \
        test_record(_diff <= (epsilon)); \
    } while (0)

/* Signal test completion with success */
static inline void test_done(void) {
    /* Write done marker - use success if no failures */
    volatile uint32_t *done = (volatile uint32_t *)(RESULT_BASE + 0x24);
    *done = (*test_fail_count == 0) ? DONE_SUCCESS : DONE_FAILURE;

    /* Halt */
    __asm__ volatile ("bkpt #0");
}

/* Signal test completion with explicit offset for done marker */
static inline void test_done_at(uint32_t offset) {
    volatile uint32_t *done = (volatile uint32_t *)(RESULT_BASE + offset);
    *done = (*test_fail_count == 0) ? DONE_SUCCESS : DONE_FAILURE;

    __asm__ volatile ("bkpt #0");
}

#endif /* TEST_FRAMEWORK_H */
