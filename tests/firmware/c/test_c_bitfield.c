/* Test C Bitfield Operations
 *
 * Tests bit manipulation operations:
 * CLZ, RBIT, REV, REV16, BFI, BFC, UBFX, SBFX
 */

#include "test_framework.h"
#include "armv8m.h"

int main(void) {
    test_init();

    uint32_t result;
    int32_t sresult;

    /* CLZ - Count Leading Zeros */
    result = clz(0x00010000);
    STORE_RESULT(0x00, result);
    ASSERT_EQ(result, 15);

    result = clz(0);
    STORE_RESULT(0x04, result);
    ASSERT_EQ(result, 32);

    result = clz(0x80000000);
    STORE_RESULT(0x08, result);
    ASSERT_EQ(result, 0);

    /* RBIT - Reverse Bits */
    result = rbit(0x80000001);
    STORE_RESULT(0x0C, result);
    ASSERT_EQ(result, 0x80000001);

    result = rbit(0x12345678);
    STORE_RESULT(0x10, result);
    ASSERT_EQ(result, 0x1E6A2C48);

    /* REV - Reverse Bytes */
    result = rev(0x12345678);
    STORE_RESULT(0x14, result);
    ASSERT_EQ(result, 0x78563412);

    /* REV16 - Reverse bytes in halfwords */
    result = rev16(0x12345678);
    STORE_RESULT(0x18, result);
    ASSERT_EQ(result, 0x34127856);

    /* REVSH - Reverse bytes in low halfword, sign extend */
    sresult = revsh(0x0080);  /* 0x80 -> 0x8000 -> sign extend */
    STORE_RESULT(0x1C, (uint32_t)sresult);
    ASSERT_EQ((uint32_t)sresult, 0xFFFF8000);

    sresult = revsh(0x0100);  /* 0x00 -> 0x0001 */
    STORE_RESULT(0x20, (uint32_t)sresult);
    ASSERT_EQ((uint32_t)sresult, 0x00000001);

    /* BFC - Bit Field Clear */
    result = bfc(0xFFFFFFFF, 8, 8);  /* Clear bits 8-15 */
    STORE_RESULT(0x24, result);
    ASSERT_EQ(result, 0xFFFF00FF);

    /* BFI - Bit Field Insert */
    result = bfi(0, 0xAB, 16, 8);  /* Insert 0xAB at bits 16-23 */
    STORE_RESULT(0x28, result);
    ASSERT_EQ(result, 0x00AB0000);

    /* UBFX - Unsigned Bit Field Extract */
    result = ubfx(0x0000FF00, 8, 8);  /* Extract bits 8-15 */
    STORE_RESULT(0x2C, result);
    ASSERT_EQ(result, 0x000000FF);

    /* SBFX - Signed Bit Field Extract */
    sresult = sbfx(0x000000F0, 4, 4);  /* Extract 4 bits at position 4 (0xF), sign extend */
    STORE_RESULT(0x30, (uint32_t)sresult);
    ASSERT_EQ((uint32_t)sresult, 0xFFFFFFFF);

    sresult = sbfx(0x00000070, 4, 4);  /* Extract 4 bits at position 4 (0x7), no sign extend */
    STORE_RESULT(0x34, (uint32_t)sresult);
    ASSERT_EQ((uint32_t)sresult, 0x00000007);

    /* Done marker */
    STORE_RESULT(0x38, 0xC0FFEE42);
    test_done_at(0x38);
    return 0;
}
