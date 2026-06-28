# MPU module CMakeLists.txt
#
# This builds the MPU module and its tests.
# Can be built standalone or as part of the full project.

# Standalone build support
if(NOT CMAKE_PROJECT_NAME)
    cmake_minimum_required(VERSION 3.16)
    project(armv8m-mpu C CXX)
    include(${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/StandaloneModule.cmake)
    femu_standalone_module()
endif()

# Source files
set(MPU_SOURCES
    mpu.c
)

# Build MPU as a static library
add_library(armv8m_mpu STATIC ${MPU_SOURCES})

target_include_directories(armv8m_mpu PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    $<$<BOOL:${STANDALONE_BUILD}>:${CMAKE_SOURCE_DIR}/../../../include>
)

# Link to logging
if(TARGET emu_log)
    target_link_libraries(armv8m_mpu emu_log)
endif()

# Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mpu.cpp)
    add_executable(test_mpu tests/test_mpu.cpp)
    target_link_libraries(test_mpu
        armv8m_mpu
        CppUTest
        CppUTestExt
    )
    add_test(NAME mpu_tests COMMAND test_mpu)
endif()
