# Emulator glue layer
# Integrates all modules into a unified emulator

# Check if we're building standalone
if(NOT TARGET armv8m_decoder)
    # Standalone build - set up basic project
    cmake_minimum_required(VERSION 3.16)
    project(armv8m_emulator LANGUAGES C CXX)

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)

    set(STANDALONE_BUILD ON)

    # Assume relative paths
    set(FEMU_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../include)

    # Need to add parent modules
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../decoder ${CMAKE_BINARY_DIR}/decoder)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../executor ${CMAKE_BINARY_DIR}/executor)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../memory ${CMAKE_BINARY_DIR}/memory)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../nvic ${CMAKE_BINARY_DIR}/nvic)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../mpu ${CMAKE_BINARY_DIR}/mpu)
else()
    set(STANDALONE_BUILD OFF)
    set(FEMU_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
endif()

# =============================================================================
# Emulator Library (Static)
# =============================================================================

add_library(armv8m_emulator STATIC
    emulator.c
)

target_include_directories(armv8m_emulator
    PUBLIC ${FEMU_INCLUDE_DIR}
)

target_link_libraries(armv8m_emulator
    PUBLIC
        armv8m_executor
        armv8m_decoder
        armv8m_memory
        armv8m_nvic
        armv8m_mpu
)

# =============================================================================
# Emulator Shared Library (for CFFI)
# =============================================================================

add_library(armv8m_emulator_shared SHARED
    emulator.c
)

target_include_directories(armv8m_emulator_shared
    PUBLIC ${FEMU_INCLUDE_DIR}
)

target_link_libraries(armv8m_emulator_shared
    PUBLIC
        armv8m_executor
        armv8m_decoder
        armv8m_memory
        armv8m_nvic
        armv8m_mpu
)

# Set shared library properties
set_target_properties(armv8m_emulator_shared PROPERTIES
    OUTPUT_NAME armv8m_emulator
    POSITION_INDEPENDENT_CODE ON
)

# Set RPATH for shared library
if(NOT STANDALONE_BUILD)
    set_target_properties(armv8m_emulator_shared PROPERTIES
        INSTALL_RPATH "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

# =============================================================================
# Tests
# =============================================================================

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_emulator.cpp)
    # CppUTest should be available from parent project
    if(NOT STANDALONE_BUILD)
        add_executable(test_emulator
            tests/test_main.cpp
            tests/test_emulator.cpp
        )

        target_include_directories(test_emulator
            PRIVATE
                ${FEMU_INCLUDE_DIR}
                ${CPPUTEST_DIR}/include
        )

        target_link_libraries(test_emulator
            PRIVATE
                armv8m_emulator
                CppUTest
                CppUTestExt
        )

        add_test(NAME emulator_tests COMMAND test_emulator)
    endif()
endif()
