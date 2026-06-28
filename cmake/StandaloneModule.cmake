# StandaloneModule.cmake
#
# Shared settings for building a single FEMU module on its own, i.e. when the
# module's CMakeLists.txt is used as the top-level CMake project rather than
# being pulled in by the root build. This replaces ~18 lines of identical
# boilerplate that were previously copy-pasted into every module.
#
# CMake requires cmake_minimum_required() and project() to be literal top-level
# calls, so those stay in each module; everything after them is shared here.
#
# Usage (inside an `if(NOT CMAKE_PROJECT_NAME)` guard):
#
#   cmake_minimum_required(VERSION 3.16)
#   project(armv8m-decoder C CXX)
#   include(${CMAKE_CURRENT_LIST_DIR}/<rel-path-to-root>/cmake/StandaloneModule.cmake)
#   femu_standalone_module()            # module with CppUTest tests
#   femu_standalone_module(NO_CPPUTEST) # module without tests
#
# The repository root is derived from this file's own location, so modules at
# any directory depth get correct include and CppUTest paths.

# This file lives in <repo-root>/cmake, so the root is one level up.
get_filename_component(_FEMU_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# Implemented as a macro (not a function) so the standard/flag settings take
# effect in the including scope, exactly as if written inline.
macro(femu_standalone_module)
    cmake_parse_arguments(_FSM "NO_CPPUTEST" "" "" ${ARGN})

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -pedantic")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -pedantic")

    include_directories(${_FEMU_ROOT}/include)

    # Module code calls the shared logging helpers (emu_log_*). The full build
    # provides the emu_log target from src/emu; supply it here so the module's
    # `if(TARGET emu_log)` link works standalone.
    if(NOT TARGET emu_log)
        add_library(emu_log STATIC ${_FEMU_ROOT}/src/emu/emu_log.c)
        target_include_directories(emu_log PUBLIC ${_FEMU_ROOT}/include)
    endif()

    if(NOT _FSM_NO_CPPUTEST)
        set(CPPUTEST_DIR ${_FEMU_ROOT}/lib/cpputest)
        add_subdirectory(${CPPUTEST_DIR} ${CMAKE_BINARY_DIR}/cpputest)
        include_directories(${CPPUTEST_DIR}/include)
    endif()

    enable_testing()
    set(STANDALONE_BUILD TRUE)
endmacro()
