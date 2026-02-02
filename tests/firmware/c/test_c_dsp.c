/* Test C DSP Operations
 *
 * Tests DSP SIMD parallel operations:
 * SADD16/8, SSUB16/8, QADD, SHADD, etc.
 */

#include "test_framework.h"
#include "armv8m.h"

int main(void) {
    test_init();

    uint32_t result;
    int32_t sresult;

    /* SADD16 - Signed add two halfwords */
    /* 0x00200010 contains [0x0020, 0x0010] */
    /* 0x00300020 contains [0x0030, 0x0020] */
    /* Result: [0x0050, 0x0030] = 0x00500030 */
    result = sadd16(0x00200010, 0x00300020);
    STORE_RESULT(0x00, result);
    ASSERT_EQ(result, 0x00500030);

    /* SADD8 - Signed add four bytes */
    /* [0x10, 0x20, 0x30, 0x40] + [0x01, 0x02, 0x03, 0x04] */
    result = sadd8(0x40302010, 0x04030201);
    STORE_RESULT(0x04, result);
    ASSERT_EQ(result, 0x44332211);

    /* SSUB16 - Signed subtract two halfwords */
    result = ssub16(0x00500030, 0x00100020);
    STORE_RESULT(0x08, result);
    ASSERT_EQ(result, 0x00400010);

    /* SSUB8 - Signed subtract four bytes */
    result = ssub8(0x44332211, 0x04030201);
    STORE_RESULT(0x0C, result);
    ASSERT_EQ(result, 0x40302010);

    /* UADD16 - Unsigned add two halfwords */
    result = uadd16(0x00200010, 0x00300020);
    STORE_RESULT(0x10, result);
    ASSERT_EQ(result, 0x00500030);

    /* UADD8 - Unsigned add four bytes */
    result = uadd8(0x40302010, 0x04030201);
    STORE_RESULT(0x14, result);
    ASSERT_EQ(result, 0x44332211);

    /* USUB16 - Unsigned subtract two halfwords */
    result = usub16(0x00500030, 0x00100020);
    STORE_RESULT(0x18, result);
    ASSERT_EQ(result, 0x00400010);

    /* USUB8 - Unsigned subtract four bytes */
    result = usub8(0x44332211, 0x04030201);
    STORE_RESULT(0x1C, result);
    ASSERT_EQ(result, 0x40302010);

    /* SHADD16 - Signed halving add halfwords */
    /* (0x0040 + 0x0030)/2 = 0x0038, (0x0020 + 0x0010)/2 = 0x0018 */
    result = shadd16(0x00400020, 0x00300010);
    STORE_RESULT(0x20, result);
    ASSERT_EQ(result, 0x00380018);

    /* SHADD8 - Signed halving add bytes */
    result = shadd8(0x40302010, 0x20100800);
    STORE_RESULT(0x24, result);
    ASSERT_EQ(result, 0x30201408);

    /* UHADD16 - Unsigned halving add halfwords */
    result = uhadd16(0x00400020, 0x00300010);
    STORE_RESULT(0x28, result);
    ASSERT_EQ(result, 0x00380018);

    /* UHADD8 - Unsigned halving add bytes */
    result = uhadd8(0x40302010, 0x20100800);
    STORE_RESULT(0x2C, result);
    ASSERT_EQ(result, 0x30201408);

    /* QADD16 - Saturating add halfwords */
    result = qadd16(0x7FFF0010, 0x00010010);  /* 0x7FFF + 0x0001 saturates to 0x7FFF */
    STORE_RESULT(0x30, result);
    ASSERT_EQ(result, 0x7FFF0020);

    /* QADD8 - Saturating add bytes */
    result = qadd8(0x7F000010, 0x01000010);  /* 0x7F + 0x01 saturates to 0x7F */
    STORE_RESULT(0x34, result);
    ASSERT_EQ(result, 0x7F000020);

    /* QSUB16 - Saturating subtract halfwords */
    result = qsub16(0x80000020, 0x00010010);  /* 0x8000 - 0x0001 saturates to 0x8000 */
    STORE_RESULT(0x38, result);
    ASSERT_EQ(result, 0x80000010);

    /* QSUB8 - Saturating subtract bytes */
    result = qsub8(0x80000020, 0x01000010);  /* 0x80 - 0x01 saturates to 0x80 */
    STORE_RESULT(0x3C, result);
    ASSERT_EQ(result, 0x80000010);

    /* UQADD8 - Unsigned saturating add (saturation test) */
    result = uqadd8(0xFF000000, 0xFF000000);  /* 0xFF + 0xFF saturates to 0xFF */
    STORE_RESULT(0x40, result);
    ASSERT_EQ(result, 0xFF000000);

    /* UQSUB8 - Unsigned saturating subtract (underflow test) */
    result = uqsub8(0x10000000, 0x20000000);  /* 0x10 - 0x20 saturates to 0x00 */
    STORE_RESULT(0x44, result);
    ASSERT_EQ(result, 0x00000000);

    /* 32-bit saturating add (QADD) */
    sresult = qadd(0x7FFFFFF0, 0x00000020);  /* Near max, should saturate */
    STORE_RESULT(0x48, (uint32_t)sresult);
    ASSERT_EQ((uint32_t)sresult, 0x7FFFFFFF);

    /* 32-bit saturating subtract (QSUB) */
    sresult = qsub((int32_t)0x80000010, 0x00000020);  /* Near min, should saturate */
    STORE_RESULT(0x4C, (uint32_t)sresult);
    ASSERT_EQ((uint32_t)sresult, 0x80000000);

    /* Done marker */
    STORE_RESULT(0x50, 0xC0FFEE42);
    test_done_at(0x50);
    return 0;
}
