cmake_minimum_required(VERSION 3.16)
project(femu
    VERSION 0.1.0
    LANGUAGES C CXX
    DESCRIPTION "FEMU - Fast EMUlator (Multi-Architecture)"
)

# =============================================================================
# Compiler Selection
# =============================================================================

# Allow selecting compiler via -DUSE_CLANG=ON or -DUSE_GCC=ON
option(USE_CLANG "Use Clang compiler" OFF)
option(USE_GCC "Use GCC compiler" OFF)

if(USE_CLANG AND USE_GCC)
    message(FATAL_ERROR "Cannot specify both USE_CLANG and USE_GCC")
endif()

# Note: Compiler should be set before project() or via CMAKE_C_COMPILER
# These options are informational for the build summary

# =============================================================================
# Language Standards
# =============================================================================

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)  # Required for computed gotos in executor

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Export compile commands for clang-tidy and other tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Compiler Warnings
# =============================================================================

# Base warning flags for both compilers
set(COMMON_WARNING_FLAGS "-Wall -Wextra -Werror -pedantic")

# Additional warnings
set(EXTRA_C_WARNINGS "-Wconversion -Wshadow -Wundef -Wcast-align -Wwrite-strings")
set(EXTRA_CXX_WARNINGS "-Wconversion -Wshadow -Wundef -Wcast-align")

# Compiler-specific flags
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(COMPILER_SPECIFIC_C_FLAGS "-Wdocumentation -Wno-gnu-zero-variadic-macro-arguments")
    set(COMPILER_SPECIFIC_CXX_FLAGS "-Wdocumentation")
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU")
    set(COMPILER_SPECIFIC_C_FLAGS "-Wlogical-op -Wduplicated-cond")
    set(COMPILER_SPECIFIC_CXX_FLAGS "-Wlogical-op -Wduplicated-cond")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_WARNING_FLAGS} ${EXTRA_C_WARNINGS} ${COMPILER_SPECIFIC_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_WARNING_FLAGS} ${EXTRA_CXX_WARNINGS} ${COMPILER_SPECIFIC_CXX_FLAGS}")

# =============================================================================
# Sanitizers (Debug builds)
# =============================================================================

option(ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan in Debug builds" ON)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND ENABLE_SANITIZERS)
    # AddressSanitizer + UndefinedBehaviorSanitizer
    set(SANITIZER_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer")

    # Additional UBSan options
    set(UBSAN_FLAGS "-fsanitize=integer-divide-by-zero,null,return,signed-integer-overflow,bounds,alignment")

    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${SANITIZER_FLAGS} -g3")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${SANITIZER_FLAGS} -g3")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${SANITIZER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} ${SANITIZER_FLAGS}")

    message(STATUS "Sanitizers enabled: ASan + UBSan")
endif()

# Coverage (for CI)
if(ENABLE_COVERAGE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    message(STATUS "Code coverage enabled")
endif()

# =============================================================================
# Static Analysis Targets
# =============================================================================

# Find static analysis tools
find_program(CPPCHECK cppcheck)
find_program(CLANG_TIDY clang-tidy)

# Add cppcheck target
if(CPPCHECK)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK}
            --enable=all
            --std=c11
            --suppress=missingIncludeSystem
            --suppress=missingInclude  # External test framework headers
            --suppress=unusedFunction
            --suppress=constVariablePointer  # Style only
            --suppress=constVariable  # Style only
            --suppress=constParameterCallback  # Style only
            --suppress=staticFunction  # Style only - internal linkage choice
            --suppress=unusedVariable  # Test code often has unused vars
            --suppress=unreadVariable  # May be used for debugging
            --suppress=unusedLabelConfiguration  # Labels for computed goto
            --suppress=variableScope  # Style only
            --suppress=duplicateCondition  # Code clarity choice
            --suppress=knownConditionTrueFalse  # Defensive coding
            --suppress=shiftTooManyBitsSigned  # False positive for ASR instructions
            --suppress=assignBoolToPointer  # False positive for GCC computed goto
            --suppress=normalCheckLevelMaxBranches  # Just an info message
            --suppress=checkLevelNormal  # Just an info message
            --suppress=checkersReport  # Just an info message
            --error-exitcode=1
            --inline-suppr
            -I ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running cppcheck static analysis"
    )
else()
    message(STATUS "cppcheck not found - static analysis target disabled")
endif()

# Add clang-tidy target (requires compile_commands.json)
if(CLANG_TIDY)
    add_custom_target(clang-tidy
        COMMAND ${CLANG_TIDY}
            -p ${CMAKE_BINARY_DIR}
            --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
            ${CMAKE_SOURCE_DIR}/src/core/*/*.c
            ${CMAKE_SOURCE_DIR}/src/arch/*/*.c
            ${CMAKE_SOURCE_DIR}/src/arch/*/*/*.c
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy static analysis"
    )
else()
    message(STATUS "clang-tidy not found - static analysis target disabled")
endif()

# Combined analysis target
add_custom_target(analyze
    COMMENT "Running all static analyzers"
)
if(CPPCHECK)
    add_dependencies(analyze cppcheck)
endif()
if(CLANG_TIDY)
    add_dependencies(analyze clang-tidy)
endif()

# =============================================================================
# CppUTest
# =============================================================================

set(CPPUTEST_DIR ${CMAKE_SOURCE_DIR}/lib/cpputest)
if(EXISTS ${CPPUTEST_DIR}/CMakeLists.txt)
    # Disable CppUTest's own tests
    set(TESTS OFF CACHE BOOL "Disable CppUTest tests")
    set(CPPUTEST_TESTS OFF CACHE BOOL "Disable CppUTest tests")
    set(CPPUTEST_EXTENSIONS ON CACHE BOOL "Enable CppUTest extensions")
    add_subdirectory(${CPPUTEST_DIR} ${CMAKE_BINARY_DIR}/cpputest)
else()
    message(FATAL_ERROR "CppUTest not found at ${CPPUTEST_DIR}. Run: git submodule update --init")
endif()

# =============================================================================
# Include Directories
# =============================================================================

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CPPUTEST_DIR}/include
)

# =============================================================================
# Testing
# =============================================================================

enable_testing()

# =============================================================================
# Architecture Selection
# =============================================================================

option(BUILD_ARCH_ARMV8M "Build ARMv8-M architecture support" ON)
option(BUILD_PYTHON_BINDINGS "Build shared libraries for Python bindings" ON)

# Enable position-independent code for all libraries (required for shared library)
if(BUILD_PYTHON_BINDINGS)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# =============================================================================
# Logging Configuration
# =============================================================================

option(EMU_DISABLE_LOGGING "Disable all logging for release builds (zero overhead)" OFF)

if(EMU_DISABLE_LOGGING)
    add_compile_definitions(EMU_LOG_DISABLE_ALL)
    message(STATUS "Logging disabled (EMU_DISABLE_LOGGING=ON)")
endif()

# =============================================================================
# Core (Shared) Modules
# =============================================================================

option(BUILD_MEMORY "Build memory module" ON)

# Logging utilities (used by all modules)
if(EXISTS ${CMAKE_SOURCE_DIR}/src/emu/emu_log.c)
    add_subdirectory(src/emu)
endif()

# Memory is shared across all architectures
if(BUILD_MEMORY AND EXISTS ${CMAKE_SOURCE_DIR}/src/core/memory/memory.c)
    add_subdirectory(src/core/memory)
endif()

# Emulator base (factory and common infrastructure)
option(BUILD_EMU_CORE "Build emulator core" ON)

if(BUILD_EMU_CORE AND EXISTS ${CMAKE_SOURCE_DIR}/src/core/emulator/emulator_base.c)
    add_subdirectory(src/core/emulator)
endif()

# =============================================================================
# ARMv8-M Architecture Modules
# =============================================================================

if(BUILD_ARCH_ARMV8M)
    option(BUILD_DECODER "Build decoder module" ON)
    option(BUILD_EXECUTOR "Build executor module" ON)
    option(BUILD_NVIC "Build NVIC module" ON)
    option(BUILD_MPU "Build MPU module" ON)
    option(BUILD_EMULATOR "Build emulator glue layer" ON)

    # Add ARMv8-M architecture subdirectory
    if(EXISTS ${CMAKE_SOURCE_DIR}/src/arch/armv8m)
        add_subdirectory(src/arch/armv8m)
    endif()
endif()

# =============================================================================
# Build Summary
# =============================================================================

message(STATUS "")
message(STATUS "╔══════════════════════════════════════════════════════════════╗")
message(STATUS "║                    FEMU Build Configuration                   ║")
message(STATUS "╠══════════════════════════════════════════════════════════════╣")
message(STATUS "║  Version:        ${PROJECT_VERSION}")
message(STATUS "║  Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "║  C Compiler:     ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "║  C++ Compiler:   ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "║  Sanitizers:     ${ENABLE_SANITIZERS}")
message(STATUS "║  Coverage:       ${ENABLE_COVERAGE}")
if(EMU_DISABLE_LOGGING)
    message(STATUS "║  Logging:        Disabled (zero overhead)")
else()
    message(STATUS "║  Logging:        Enabled")
endif()
message(STATUS "╠══════════════════════════════════════════════════════════════╣")
message(STATUS "║  Python bindings: ${BUILD_PYTHON_BINDINGS}")
message(STATUS "║  Architectures:")
if(BUILD_ARCH_ARMV8M)
    message(STATUS "║    ARMv8-M:      Enabled")
else()
    message(STATUS "║    ARMv8-M:      Disabled")
endif()
message(STATUS "╠══════════════════════════════════════════════════════════════╣")
message(STATUS "║  Static Analysis Tools:")
if(CPPCHECK)
    message(STATUS "║    cppcheck:     Found")
else()
    message(STATUS "║    cppcheck:     Not found")
endif()
if(CLANG_TIDY)
    message(STATUS "║    clang-tidy:   Found")
else()
    message(STATUS "║    clang-tidy:   Not found")
endif()
message(STATUS "╚══════════════════════════════════════════════════════════════╝")
message(STATUS "")
