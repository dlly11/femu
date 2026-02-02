/* Test C Memory Operations
 *
 * Tests memory access operations compiled from C:
 * Word/halfword/byte load/store, arrays, pointers
 */

#include "test_framework.h"
#include <stdint.h>

/* Test data in RAM */
static volatile uint32_t word_data;
static volatile uint16_t half_data;
static volatile uint8_t byte_data;
static volatile uint32_t array_data[4];

int main(void) {
    test_init();

    /* Word store/load */
    word_data = 0x12345678;
    STORE_RESULT(0x00, word_data);
    ASSERT_EQ(word_data, 0x12345678);

    /* Halfword store/load */
    half_data = 0xABCD;
    STORE_RESULT(0x04, half_data);
    ASSERT_EQ(half_data, 0xABCD);

    /* Byte store/load */
    byte_data = 0x42;
    STORE_RESULT(0x08, byte_data);
    ASSERT_EQ(byte_data, 0x42);

    /* Array access */
    array_data[0] = 0x11111111;
    array_data[1] = 0x22222222;
    array_data[2] = 0x33333333;
    array_data[3] = 0x44444444;

    STORE_RESULT(0x0C, array_data[0]);
    STORE_RESULT(0x10, array_data[1]);
    STORE_RESULT(0x14, array_data[2]);
    STORE_RESULT(0x18, array_data[3]);

    ASSERT_EQ(array_data[0], 0x11111111);
    ASSERT_EQ(array_data[1], 0x22222222);
    ASSERT_EQ(array_data[2], 0x33333333);
    ASSERT_EQ(array_data[3], 0x44444444);

    /* Pointer arithmetic */
    volatile uint32_t *ptr = &array_data[0];
    uint32_t sum = 0;
    for (int i = 0; i < 4; i++) {
        sum += ptr[i];
    }
    STORE_RESULT(0x1C, sum);
    ASSERT_EQ(sum, 0xAAAAAAAA);

    /* Indexed access */
    volatile int index = 2;
    STORE_RESULT(0x20, array_data[index]);
    ASSERT_EQ(array_data[index], 0x33333333);

    /* Done marker */
    STORE_RESULT(0x24, 0xC0FFEE42);
    test_done();
    return 0;
}
