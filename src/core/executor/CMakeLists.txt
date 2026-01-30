# Executor module CMakeLists.txt
#
# This builds the executor module and its tests.
# Can be built standalone or as part of the full project.

# Standalone build support
if(NOT CMAKE_PROJECT_NAME)
    cmake_minimum_required(VERSION 3.16)
    project(armv8m-executor C CXX)

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
set(EXECUTOR_SOURCES
    executor.c
)

# Add optional source files if they exist
foreach(SRC exec_data_proc.c exec_load_store.c exec_branch.c exec_system.c exec_exception.c exec_fpu.c)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${SRC})
        list(APPEND EXECUTOR_SOURCES ${SRC})
    endif()
endforeach()

# Build executor as a static library
add_library(armv8m_executor STATIC ${EXECUTOR_SOURCES})

target_include_directories(armv8m_executor PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    $<$<BOOL:${STANDALONE_BUILD}>:${CMAKE_SOURCE_DIR}/../../../include>
)

# Link to decoder (executor depends on decoder types)
if(TARGET armv8m_decoder)
    target_link_libraries(armv8m_executor armv8m_decoder)
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
