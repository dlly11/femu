/* Test C Exception Handling
 *
 * Tests SVC handler invocation and return from C
 */

#include "test_framework.h"
#include "armv8m.h"

/* SVC handler state */
static volatile uint32_t svc_called = 0;
static volatile uint32_t svc_number = 0;
static volatile uint32_t svc_r0 = 0;
static volatile uint32_t svc_r1 = 0;
static volatile uint32_t svc_r2 = 0;
static volatile uint32_t svc_r3 = 0;

/* Custom SVC handler - override weak default */
void _svc_handler(void) __attribute__((naked));
void _svc_handler(void) {
    __asm__ volatile (
        /* Get correct stack pointer (using EXC_RETURN in LR) */
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"

        /* r0 now points to stacked frame: R0, R1, R2, R3, R12, LR, PC, xPSR */

        /* Save stacked R0-R3 to our variables */
        "ldr r1, [r0, #0]\n"     /* Stacked R0 */
        "ldr r2, =svc_r0\n"
        "str r1, [r2]\n"

        "ldr r1, [r0, #4]\n"     /* Stacked R1 */
        "ldr r2, =svc_r1\n"
        "str r1, [r2]\n"

        "ldr r1, [r0, #8]\n"     /* Stacked R2 */
        "ldr r2, =svc_r2\n"
        "str r1, [r2]\n"

        "ldr r1, [r0, #12]\n"    /* Stacked R3 */
        "ldr r2, =svc_r3\n"
        "str r1, [r2]\n"

        /* Get SVC number from instruction */
        "ldr r1, [r0, #24]\n"    /* Stacked PC */
        "ldrb r1, [r1, #-2]\n"   /* SVC number is in the instruction */
        "ldr r2, =svc_number\n"
        "str r1, [r2]\n"

        /* Mark as called */
        "mov r1, #1\n"
        "ldr r2, =svc_called\n"
        "str r1, [r2]\n"

        /* Set return value in R0 on stack */
        "ldr r1, =0x1234\n"
        "str r1, [r0, #0]\n"

        /* Return from exception */
        "bx lr\n"
    );
}

int main(void) {
    test_init();

    uint32_t result;

    /* Call SVC #42 */
    svc_called = 0;
    svc_number = 0;
    __asm__ volatile (
        "mov r0, #0x10\n"
        "mov r1, #0x11\n"
        "mov r2, #0x12\n"
        "mov r3, #0x13\n"
        "svc #42\n"
        "mov %0, r0\n"
        : "=r" (result)
        :
        : "r0", "r1", "r2", "r3"
    );

    /* Verify SVC handler was called */
    STORE_RESULT(0x00, svc_called);
    ASSERT_EQ(svc_called, 1);

    /* Verify SVC number */
    STORE_RESULT(0x04, svc_number);
    ASSERT_EQ(svc_number, 42);

    /* Verify return value */
    STORE_RESULT(0x08, result);
    ASSERT_EQ(result, 0x1234);

    /* Verify stacked registers */
    STORE_RESULT(0x0C, svc_r0);
    ASSERT_EQ(svc_r0, 0x10);

    STORE_RESULT(0x10, svc_r1);
    ASSERT_EQ(svc_r1, 0x11);

    STORE_RESULT(0x14, svc_r2);
    ASSERT_EQ(svc_r2, 0x12);

    STORE_RESULT(0x18, svc_r3);
    ASSERT_EQ(svc_r3, 0x13);

    /* Call SVC #7 (second test) */
    svc_called = 0;
    svc_number = 0;
    __asm__ volatile ("svc #7");

    STORE_RESULT(0x1C, svc_called);
    ASSERT_EQ(svc_called, 1);

    STORE_RESULT(0x20, svc_number);
    ASSERT_EQ(svc_number, 7);

    /* Done marker */
    STORE_RESULT(0x24, 0xC0FFEE42);
    test_done();
    return 0;
}
