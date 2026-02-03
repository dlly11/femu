/**
 * @file test_integration_main.cpp
 * @brief Main entry point for integration tests
 */

#include "test_integration_common.h"

/*============================================================================
 * Global Variables
 *============================================================================*/

uint8_t test_memory[MEMORY_SIZE];
Executor exec;

/*============================================================================
 * Test Runner
 *============================================================================*/

int main(int argc, char **argv) {
  return CommandLineTestRunner::RunAllTests(argc, argv);
}
