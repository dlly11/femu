/* Test C FPU Operations
 *
 * Tests floating-point operations compiled from C:
 * VADD, VSUB, VMUL, VDIV, VCMP, VCVT, VSQRT, VABS, VNEG
 */

#include "test_framework.h"

int main(void) {
    test_init();

    volatile float a, b, c;
    float result;
    int32_t iresult;

    /* VADD - floating point addition */
    a = 1.5f;
    b = 2.5f;
    result = a + b;
    STORE_RESULT_FLOAT(0x00, result);
    ASSERT_FLOAT_BITS_EQ(result, 0x40800000);  /* 4.0f */

    /* VSUB - floating point subtraction */
    a = 5.0f;
    b = 2.5f;
    result = a - b;
    STORE_RESULT_FLOAT(0x04, result);
    ASSERT_FLOAT_BITS_EQ(result, 0x40200000);  /* 2.5f */

    /* VMUL - floating point multiplication */
    a = 2.0f;
    b = 3.0f;
    result = a * b;
    STORE_RESULT_FLOAT(0x08, result);
    ASSERT_FLOAT_BITS_EQ(result, 0x40C00000);  /* 6.0f */

    /* VDIV - floating point division */
    a = 8.0f;
    b = 2.0f;
    result = a / b;
    STORE_RESULT_FLOAT(0x0C, result);
    ASSERT_FLOAT_BITS_EQ(result, 0x40800000);  /* 4.0f */

    /* VCMP - equal */
    a = 3.0f;
    b = 3.0f;
    iresult = (a == b) ? 1 : 0;
    STORE_RESULT(0x10, iresult);
    ASSERT_EQ(iresult, 1);

    /* VCMP - less than */
    a = 2.0f;
    b = 5.0f;
    iresult = (a < b) ? 1 : 0;
    STORE_RESULT(0x14, iresult);
    ASSERT_EQ(iresult, 1);

    /* VCMP - greater than */
    a = 7.0f;
    b = 3.0f;
    iresult = (a > b) ? 1 : 0;
    STORE_RESULT(0x18, iresult);
    ASSERT_EQ(iresult, 1);

    /* VCVT - integer to float */
    volatile int32_t i = 42;
    result = (float)i;
    STORE_RESULT_FLOAT(0x1C, result);
    ASSERT_FLOAT_BITS_EQ(result, 0x42280000);  /* 42.0f */

    /* VCVT - float to integer (truncate) */
    a = 3.7f;
    iresult = (int32_t)a;
    STORE_RESULT(0x20, iresult);
    ASSERT_EQ(iresult, 3);

    /* VABS - absolute value */
    a = -3.5f;
    result = (a < 0) ? -a : a;
    STORE_RESULT_FLOAT(0x24, result);
    ASSERT_FLOAT_BITS_EQ(result, 0x40600000);  /* 3.5f */

    /* VNEG - negation */
    a = 2.5f;
    result = -a;
    STORE_RESULT_FLOAT(0x28, result);
    ASSERT_FLOAT_BITS_EQ(result, 0xC0200000);  /* -2.5f */

    /* VSQRT - square root */
    a = 16.0f;
    /* Use inline assembly for VSQRT since compiler may not use it */
    __asm__ volatile ("vsqrt.f32 %0, %1" : "=t" (result) : "t" (a));
    STORE_RESULT_FLOAT(0x2C, result);
    ASSERT_FLOAT_BITS_EQ(result, 0x40800000);  /* 4.0f */

    /* Compound expression */
    a = 2.0f;
    b = 3.0f;
    c = 4.0f;
    result = a + b * c;  /* 2 + 12 = 14 */
    STORE_RESULT_FLOAT(0x30, result);
    ASSERT_FLOAT_BITS_EQ(result, 0x41600000);  /* 14.0f */

    /* Done marker */
    STORE_RESULT(0x34, 0xC0FFEE42);
    test_done_at(0x34);
    return 0;
}
