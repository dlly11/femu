cmake_minimum_required(VERSION 3.16)
project(femu
    VERSION 0.1.0
    LANGUAGES C CXX
    DESCRIPTION "FEMU - Fast EMUlator"
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
set(CMAKE_C_EXTENSIONS OFF)

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
            --suppress=unusedFunction
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
# Module Options
# =============================================================================

option(BUILD_DECODER "Build decoder module" ON)
option(BUILD_EXECUTOR "Build executor module" ON)
option(BUILD_MEMORY "Build memory module" ON)
option(BUILD_NVIC "Build NVIC module" ON)
option(BUILD_MPU "Build MPU module" ON)
option(BUILD_EMULATOR "Build emulator glue layer" ON)

# =============================================================================
# Core Modules
# =============================================================================

# Only add modules if their main implementation file exists
if(BUILD_DECODER AND EXISTS ${CMAKE_SOURCE_DIR}/src/core/decoder/decoder.c)
    add_subdirectory(src/core/decoder)
endif()

if(BUILD_EXECUTOR AND EXISTS ${CMAKE_SOURCE_DIR}/src/core/executor/executor.c)
    add_subdirectory(src/core/executor)
endif()

if(BUILD_MEMORY AND EXISTS ${CMAKE_SOURCE_DIR}/src/core/memory/memory.c)
    add_subdirectory(src/core/memory)
endif()

if(BUILD_NVIC AND EXISTS ${CMAKE_SOURCE_DIR}/src/core/nvic/nvic.c)
    add_subdirectory(src/core/nvic)
endif()

if(BUILD_MPU AND EXISTS ${CMAKE_SOURCE_DIR}/src/core/mpu/mpu.c)
    add_subdirectory(src/core/mpu)
endif()

if(BUILD_EMULATOR AND EXISTS ${CMAKE_SOURCE_DIR}/src/core/emulator/emulator.c)
    add_subdirectory(src/core/emulator)
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
