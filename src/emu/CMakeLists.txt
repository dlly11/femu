# Emulator Core Utilities CMakeLists.txt
#
# This builds shared utilities used across all architectures:
# - emu_log: Unified logging system

# Logging library
add_library(emu_log STATIC
    emu_log.c
)

target_include_directories(emu_log PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Handle compile-time logging disable
if(EMU_DISABLE_LOGGING)
    target_compile_definitions(emu_log PUBLIC EMU_LOG_DISABLE_ALL)
endif()
