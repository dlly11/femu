/**
 * @file test_main.cpp
 * @brief Test runner for emulator tests
 */

#include "CppUTest/CommandLineTestRunner.h"

int main(int argc, char **argv) {
  return CommandLineTestRunner::RunAllTests(argc, argv);
}
