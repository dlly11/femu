# Memory module CMakeLists.txt
#
# This builds the generic memory module shared across all architectures.
# Can be built standalone or as part of the full project.

# Standalone build support
if(NOT CMAKE_PROJECT_NAME)
    cmake_minimum_required(VERSION 3.16)
    project(emu-memory C CXX)

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -pedantic")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -pedantic")

    # Find CppUTest
    set(CPPUTEST_DIR ${CMAKE_SOURCE_DIR}/../../../lib/cpputest)
    add_subdirectory(${CPPUTEST_DIR} ${CMAKE_BINARY_DIR}/cpputest)

    include_directories(
        ${CMAKE_SOURCE_DIR}/../../../include
        ${CPPUTEST_DIR}/include
    )

    enable_testing()
    set(STANDALONE_BUILD TRUE)
endif()

# Source files
set(MEMORY_SOURCES
    memory.c
)

# Build memory as a static library
add_library(emu_memory STATIC ${MEMORY_SOURCES})

target_include_directories(emu_memory PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    $<$<BOOL:${STANDALONE_BUILD}>:${CMAKE_SOURCE_DIR}/../../../include>
)

# Link to logging
if(TARGET emu_log)
    target_link_libraries(emu_memory emu_log)
endif()

# Provide backward-compatible alias for ARMv8-M code
add_library(armv8m_memory ALIAS emu_memory)

# Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_memory.cpp)
    add_executable(test_memory tests/test_memory.cpp)
    target_link_libraries(test_memory
        emu_memory
        CppUTest
        CppUTestExt
    )
    add_test(NAME memory_tests COMMAND test_memory)
endif()
