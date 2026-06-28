# Generic Emulator Base
# Provides architecture-agnostic emulator factory and base functionality.
#
# This builds the generic emulator infrastructure. Architecture-specific
# emulators (like ARMv8-M) link against this for factory registration.

# Standalone build support
if(NOT CMAKE_PROJECT_NAME)
    cmake_minimum_required(VERSION 3.16)
    project(emu-core C CXX)
    include(${CMAKE_CURRENT_LIST_DIR}/../../../cmake/StandaloneModule.cmake)
    femu_standalone_module(NO_CPPUTEST)
endif()

# Source files
set(EMULATOR_BASE_SOURCES
    emulator_base.c
)

# Build emulator base as a static library
add_library(emu_core STATIC ${EMULATOR_BASE_SOURCES})

target_include_directories(emu_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    $<$<BOOL:${STANDALONE_BUILD}>:${CMAKE_SOURCE_DIR}/../../../include>
)
