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
foreach(SRC exec_data_proc.c exec_load_store.c exec_branch.c exec_system.c exec_exception.c)
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

# Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_executor.cpp)
    add_executable(test_executor tests/test_executor.cpp)
    target_link_libraries(test_executor
        armv8m_executor
        CppUTest
        CppUTestExt
    )
    add_test(NAME executor_tests COMMAND test_executor)
endif()
