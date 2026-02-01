# Generic Emulator Base
# Provides architecture-agnostic emulator factory and base functionality.
#
# This builds the generic emulator infrastructure. Architecture-specific
# emulators (like ARMv8-M) link against this for factory registration.

# Standalone build support
if(NOT CMAKE_PROJECT_NAME)
    cmake_minimum_required(VERSION 3.16)
    project(emu-core C CXX)

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -pedantic")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -pedantic")

    include_directories(
        ${CMAKE_SOURCE_DIR}/../../../include
    )

    enable_testing()
    set(STANDALONE_BUILD TRUE)
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
