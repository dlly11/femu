# ARMv8-M Architecture CMakeLists.txt
#
# This file builds all ARMv8-M specific modules:
# - Decoder
# - Executor
# - NVIC
# - MPU
# - Emulator glue layer

# Add subdirectories for each module
if(BUILD_DECODER AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/decoder/decoder.c)
    add_subdirectory(decoder)
endif()

# MPU is configured before the executor so the executor can link it (the TT
# instruction handler uses the MPU region/permission helpers).
if(BUILD_MPU AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/mpu/mpu.c)
    add_subdirectory(mpu)
endif()

if(BUILD_EXECUTOR AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/executor/executor.c)
    add_subdirectory(executor)
endif()

if(BUILD_NVIC AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/nvic/nvic.c)
    add_subdirectory(nvic)
endif()

# Build the emulator glue layer as a static library
if(BUILD_EMULATOR AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/armv8m_emulator.c)
    set(EMULATOR_SOURCES
        armv8m_emulator.c
        armv8m_cpu.c
        executor_vtable.c
    )

    add_library(armv8m_emulator STATIC ${EMULATOR_SOURCES})

    target_include_directories(armv8m_emulator PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    )

    # Link to all ARMv8-M components, shared memory, emulator core, and logging
    target_link_libraries(armv8m_emulator
        armv8m_decoder
        armv8m_executor
        armv8m_nvic
        armv8m_mpu
        emu_memory
        emu_core
        emu_log
    )

    # Tests for the emulator (currently in src/core/emulator/tests/)
    if(EXISTS ${CMAKE_SOURCE_DIR}/src/core/emulator/tests/test_emulator.cpp)
        add_executable(test_emulator
            ${CMAKE_SOURCE_DIR}/src/core/emulator/tests/test_main.cpp
            ${CMAKE_SOURCE_DIR}/src/core/emulator/tests/test_emulator.cpp
        )
        target_link_libraries(test_emulator
            armv8m_emulator
            CppUTest
            CppUTestExt
        )
        add_test(NAME emulator_tests COMMAND test_emulator)
    endif()
endif()

# Combined ARMv8-M library (convenience target)
add_library(femu_armv8m INTERFACE)
target_link_libraries(femu_armv8m INTERFACE
    armv8m_emulator
)

# Build shared library for Python bindings
if(BUILD_PYTHON_BINDINGS)
    add_library(armv8m_emulator_shared SHARED ${EMULATOR_SOURCES})

    target_include_directories(armv8m_emulator_shared PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    )

    # For shared library, we need to link all dependencies with -fPIC
    target_link_libraries(armv8m_emulator_shared
        armv8m_decoder
        armv8m_executor
        armv8m_nvic
        armv8m_mpu
        emu_memory
        emu_core
        emu_log
    )

    # Set output name and properties
    set_target_properties(armv8m_emulator_shared PROPERTIES
        OUTPUT_NAME "armv8m_emulator"
        POSITION_INDEPENDENT_CODE ON
    )
endif()
