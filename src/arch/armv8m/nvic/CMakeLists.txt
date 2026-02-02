# NVIC module CMakeLists.txt
#
# This builds the NVIC module and its tests.
# Can be built standalone or as part of the full project.

# Standalone build support
if(NOT CMAKE_PROJECT_NAME)
    cmake_minimum_required(VERSION 3.16)
    project(armv8m-nvic C CXX)

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
set(NVIC_SOURCES
    nvic.c
)

# Build NVIC as a static library
add_library(armv8m_nvic STATIC ${NVIC_SOURCES})

target_include_directories(armv8m_nvic PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    $<$<BOOL:${STANDALONE_BUILD}>:${CMAKE_SOURCE_DIR}/../../../include>
)

# Link to logging
if(TARGET emu_log)
    target_link_libraries(armv8m_nvic emu_log)
endif()

# Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_nvic.cpp)
    add_executable(test_nvic tests/test_nvic.cpp)
    target_link_libraries(test_nvic
        armv8m_nvic
        CppUTest
        CppUTestExt
    )
    add_test(NAME nvic_tests COMMAND test_nvic)
endif()
