# Decoder module CMakeLists.txt
#
# This builds the decoder module and its tests.
# Can be built standalone or as part of the full project.

# Standalone build support
if(NOT CMAKE_PROJECT_NAME)
    cmake_minimum_required(VERSION 3.16)
    project(armv8m-decoder C CXX)

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
set(DECODER_SOURCES
    decoder.c
)

# Add optional source files if they exist
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/decode_thumb16.c)
    list(APPEND DECODER_SOURCES decode_thumb16.c)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/decode_thumb32.c)
    list(APPEND DECODER_SOURCES decode_thumb32.c)
endif()

# Build decoder as a static library
add_library(armv8m_decoder STATIC ${DECODER_SOURCES})

target_include_directories(armv8m_decoder PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    $<$<BOOL:${STANDALONE_BUILD}>:${CMAKE_SOURCE_DIR}/../../../include>
)

# Tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_decoder.cpp)
    add_executable(test_decoder tests/test_decoder.cpp)
    target_link_libraries(test_decoder
        armv8m_decoder
        CppUTest
        CppUTestExt
    )
    add_test(NAME decoder_tests COMMAND test_decoder)
endif()
