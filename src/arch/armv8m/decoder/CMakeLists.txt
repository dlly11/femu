# Decoder module CMakeLists.txt
#
# This builds the decoder module and its tests.
# Can be built standalone or as part of the full project.

# Standalone build support
if(NOT CMAKE_PROJECT_NAME)
    cmake_minimum_required(VERSION 3.16)
    project(armv8m-decoder C CXX)
    include(${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/StandaloneModule.cmake)
    femu_standalone_module()
endif()

# Source files
set(DECODER_SOURCES
    decoder.c
    decoder_vtable.c
)

# Add optional source files if they exist
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/decode_thumb16.c)
    list(APPEND DECODER_SOURCES decode_thumb16.c)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/decode_thumb32.c)
    list(APPEND DECODER_SOURCES
        decode_thumb32.c
        decode_thumb32_branch.c
        decode_thumb32_data.c
        decode_thumb32_loadstore.c
        decode_thumb32_multiply.c
        decode_thumb32_dsp.c
        decode_thumb32_vfp.c
    )
endif()

# Build decoder as a static library
add_library(armv8m_decoder STATIC ${DECODER_SOURCES})

target_include_directories(armv8m_decoder PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    $<$<BOOL:${STANDALONE_BUILD}>:${CMAKE_SOURCE_DIR}/../../../include>
)

# Link to logging
if(TARGET emu_log)
    target_link_libraries(armv8m_decoder emu_log)
endif()

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
