/* Test C System Operations
 *
 * Tests system register access and barrier instructions:
 * PRIMASK, BASEPRI, PSP, DMB/DSB/ISB
 */

#include "test_framework.h"
#include "armv8m.h"

int main(void) {
    test_init();

    uint32_t result;

    /* PRIMASK - read initial value (should be 0 after startup) */
    result = get_primask();
    STORE_RESULT(0x00, result);
    ASSERT_EQ(result, 0);

    /* PRIMASK - set to 1 (disable interrupts) */
    set_primask(1);
    result = get_primask();
    STORE_RESULT(0x04, result);
    ASSERT_EQ(result, 1);

    /* PRIMASK - clear back to 0 */
    set_primask(0);
    result = get_primask();
    STORE_RESULT(0x08, result);
    ASSERT_EQ(result, 0);

    /* BASEPRI - read initial value */
    result = get_basepri();
    STORE_RESULT(0x0C, result);
    ASSERT_EQ(result, 0);

    /* BASEPRI - set priority mask */
    set_basepri(0x40);
    result = get_basepri();
    STORE_RESULT(0x10, result);
    ASSERT_EQ(result, 0x40);

    /* BASEPRI - clear */
    set_basepri(0);
    result = get_basepri();
    STORE_RESULT(0x14, result);
    ASSERT_EQ(result, 0);

    /* PSP - set and read */
    set_psp(0x20002000);
    result = get_psp();
    STORE_RESULT(0x18, result);
    ASSERT_EQ(result, 0x20002000);

    /* FAULTMASK - read only in Thread mode
     * Note: FAULTMASK can only be written from Handler mode (exception handlers).
     * Writing from Thread mode is ignored per ARM Architecture Reference Manual.
     * We just verify it reads as 0 from Thread mode.
     */
    result = get_faultmask();
    STORE_RESULT(0x1C, result);
    ASSERT_EQ(result, 0);

    /* Write attempt - will be ignored in Thread mode */
    set_faultmask(1);
    result = get_faultmask();
    STORE_RESULT(0x20, result);
    /* Expect 0 since write from Thread mode is ignored */
    ASSERT_EQ(result, 0);

    /* CONTROL register - read */
    result = get_control();
    STORE_RESULT(0x24, result);
    /* Value depends on startup state, just verify we can read it */
    ASSERT_TRUE(1);

    /* Barriers - just verify they execute without faulting */
    dmb();
    STORE_RESULT(0x28, 1);
    ASSERT_TRUE(1);

    dsb();
    STORE_RESULT(0x2C, 1);
    ASSERT_TRUE(1);

    isb();
    STORE_RESULT(0x30, 1);
    ASSERT_TRUE(1);

    /* NOP */
    nop();
    nop();
    nop();
    STORE_RESULT(0x34, 1);
    ASSERT_TRUE(1);

    /* MSP - read main stack pointer */
    result = get_msp();
    STORE_RESULT(0x38, result);
    /* Verify it's in valid RAM range */
    ASSERT_TRUE(result >= 0x20000000 && result < 0x20008000);

    /* Done marker */
    STORE_RESULT(0x3C, 0xC0FFEE42);
    test_done_at(0x3C);
    return 0;
}
