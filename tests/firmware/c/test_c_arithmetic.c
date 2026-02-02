/* Test C Arithmetic Operations
 *
 * Tests basic arithmetic operations compiled from C:
 * ADD, SUB, MUL, AND, OR, XOR, shifts
 */

#include "test_framework.h"

int main(void) {
    test_init();

    volatile int32_t a, b;
    volatile uint32_t ua, ub;
    uint32_t result;

    /* Addition */
    a = 15;
    b = 7;
    result = a + b;
    STORE_RESULT(0x00, result);
    ASSERT_EQ(result, 22);

    /* Subtraction */
    a = 100;
    b = 37;
    result = a - b;
    STORE_RESULT(0x04, result);
    ASSERT_EQ(result, 63);

    /* Multiplication */
    a = 6;
    b = 7;
    result = a * b;
    STORE_RESULT(0x08, result);
    ASSERT_EQ(result, 42);

    /* Bitwise AND */
    ua = 0xFF;
    ub = 0x0F;
    result = ua & ub;
    STORE_RESULT(0x0C, result);
    ASSERT_EQ(result, 0x0F);

    /* Bitwise OR */
    ua = 0xF0;
    ub = 0x0F;
    result = ua | ub;
    STORE_RESULT(0x10, result);
    ASSERT_EQ(result, 0xFF);

    /* Bitwise XOR */
    ua = 0xAA;
    ub = 0xFF;
    result = ua ^ ub;
    STORE_RESULT(0x14, result);
    ASSERT_EQ(result, 0x55);

    /* Left shift */
    ua = 1;
    result = ua << 4;
    STORE_RESULT(0x18, result);
    ASSERT_EQ(result, 16);

    /* Logical right shift */
    ua = 256;
    result = ua >> 4;
    STORE_RESULT(0x1C, result);
    ASSERT_EQ(result, 16);

    /* Arithmetic right shift (signed) */
    a = -8;
    result = (uint32_t)(a >> 2);
    STORE_RESULT(0x20, result);
    ASSERT_EQ(result, 0xFFFFFFFE);

    /* Done marker at offset 0x24 */
    STORE_RESULT(0x24, 0xCAFEBABE);
    test_done();
    return 0;
}
