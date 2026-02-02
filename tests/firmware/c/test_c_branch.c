/* Test C Branch Operations
 *
 * Tests control flow operations compiled from C:
 * Function calls, conditionals, loops
 */

#include "test_framework.h"

/* Forward declarations */
static int add_func(int a, int b);
static int fibonacci(int n);
static int factorial(int n);

/* Simple function call */
static int add_func(int a, int b) {
    return a + b;
}

/* Recursive function */
static int fibonacci(int n) {
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

/* Iterative function */
static int factorial(int n) {
    int result = 1;
    while (n > 1) {
        result *= n;
        n--;
    }
    return result;
}

int main(void) {
    test_init();

    volatile int x, y;
    int result;

    /* Simple function call */
    result = add_func(10, 20);
    STORE_RESULT(0x00, result);
    ASSERT_EQ(result, 30);

    /* If-else: equal */
    x = 5;
    y = 5;
    if (x == y) {
        result = 1;
    } else {
        result = 0;
    }
    STORE_RESULT(0x04, result);
    ASSERT_EQ(result, 1);

    /* If-else: not equal */
    x = 5;
    y = 10;
    if (x != y) {
        result = 2;
    } else {
        result = 0;
    }
    STORE_RESULT(0x08, result);
    ASSERT_EQ(result, 2);

    /* If-else: greater than */
    x = 10;
    y = 5;
    if (x > y) {
        result = 3;
    } else {
        result = 0;
    }
    STORE_RESULT(0x0C, result);
    ASSERT_EQ(result, 3);

    /* If-else: less than */
    x = 3;
    y = 8;
    if (x < y) {
        result = 4;
    } else {
        result = 0;
    }
    STORE_RESULT(0x10, result);
    ASSERT_EQ(result, 4);

    /* For loop */
    result = 0;
    for (int i = 0; i < 5; i++) {
        result += i;
    }
    STORE_RESULT(0x14, result);  /* 0+1+2+3+4 = 10 */
    ASSERT_EQ(result, 10);

    /* While loop */
    x = 10;
    result = 0;
    while (x > 0) {
        result += x;
        x--;
    }
    STORE_RESULT(0x18, result);  /* 10+9+8+7+6+5+4+3+2+1 = 55 */
    ASSERT_EQ(result, 55);

    /* Recursive function: Fibonacci(7) = 13 */
    result = fibonacci(7);
    STORE_RESULT(0x1C, result);
    ASSERT_EQ(result, 13);

    /* Factorial(5) = 120 */
    result = factorial(5);
    STORE_RESULT(0x20, result);
    ASSERT_EQ(result, 120);

    /* Switch statement */
    x = 2;
    switch (x) {
        case 0: result = 100; break;
        case 1: result = 200; break;
        case 2: result = 300; break;
        default: result = 0; break;
    }
    STORE_RESULT(0x24, result);
    ASSERT_EQ(result, 300);

    /* Ternary operator */
    x = 15;
    result = (x > 10) ? 500 : 600;
    STORE_RESULT(0x28, result);
    ASSERT_EQ(result, 500);

    /* Done marker */
    STORE_RESULT(0x2C, 0xC0FFEE42);
    test_done_at(0x2C);
    return 0;
}
