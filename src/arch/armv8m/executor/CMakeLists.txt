# Executor module CMakeLists.txt
#
# This builds the executor module and its tests.
# Can be built standalone or as part of the full project.

# Standalone build support
if(NOT CMAKE_PROJECT_NAME)
    cmake_minimum_required(VERSION 3.16)
    project(armv8m-executor C CXX)
    include(${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/StandaloneModule.cmake)
    femu_standalone_module()
endif()

# Source files
set(EXECUTOR_SOURCES
    executor.c
)

# Add optional source files if they exist
foreach(SRC exec_data_proc.c exec_load_store.c exec_branch.c exec_system.c exec_exception.c exec_fpu.c blocks.c icache.c)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${SRC})
        list(APPEND EXECUTOR_SOURCES ${SRC})
    endif()
endforeach()

# Build executor as a static library
add_library(armv8m_executor STATIC ${EXECUTOR_SOURCES})

# Disable -pedantic for executor because it uses computed gotos (GCC extension)
# for performance in the instruction dispatch loop
target_compile_options(armv8m_executor PRIVATE -Wno-pedantic)

target_include_directories(armv8m_executor PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    $<$<BOOL:${STANDALONE_BUILD}>:${CMAKE_SOURCE_DIR}/../../../include>
)

# Link to decoder (executor depends on decoder types) and logging
if(TARGET armv8m_decoder)
    target_link_libraries(armv8m_executor armv8m_decoder)
endif()
if(TARGET emu_log)
    target_link_libraries(armv8m_executor emu_log)
endif()
# MPU region/permission helpers are used by the TT instruction handler.
if(TARGET armv8m_mpu)
    target_link_libraries(armv8m_executor armv8m_mpu)
endif()

# Unit tests (with mocked decoder)
set(TEST_SOURCES
    tests/test_main.cpp
    tests/test_init.cpp
    tests/test_data_proc.cpp
    tests/test_arithmetic.cpp
    tests/test_load_store.cpp
    tests/test_control_flow.cpp
    tests/test_system.cpp
    tests/test_fault_handling.cpp
    tests/test_coverage_system.cpp
    tests/test_coverage_load_store.cpp
    tests/test_coverage_data_branch.cpp
    tests/test_coverage_exception.cpp
    tests/test_dsp.cpp
    tests/test_fpu.cpp
    tests/test_trustzone.cpp
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_main.cpp)
    add_executable(test_executor ${TEST_SOURCES})
    target_link_libraries(test_executor
        armv8m_executor
        CppUTest
        CppUTestExt
    )
    add_test(NAME executor_tests COMMAND test_executor)
endif()

# Integration tests (with real decoder)
set(INTEGRATION_TEST_SOURCES
    tests/test_integration_main.cpp
    tests/test_integration_data_proc.cpp
    tests/test_integration_memory.cpp
    tests/test_integration_branch.cpp
    tests/test_integration_exec.cpp
    tests/test_integration_misc.cpp
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_integration_main.cpp)
    add_executable(test_executor_integration ${INTEGRATION_TEST_SOURCES})
    target_link_libraries(test_executor_integration
        armv8m_executor
        armv8m_decoder
        CppUTest
    )
    add_test(NAME executor_integration_tests COMMAND test_executor_integration)
endif()
