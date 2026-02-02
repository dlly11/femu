"""
Tests for watchpoint functionality.

These tests verify the watchpoint API works correctly through Python bindings.
Note: Tests require building without sanitizers for Python compatibility.
"""

import pytest


class TestWatchpointConstants:
    """Test that watchpoint constants are properly defined."""

    def test_watchpoint_type_constants(self):
        """Verify watchpoint type constants exist and have correct values."""
        from femu._emulator_cffi import (
            WATCHPOINT_WRITE,
            WATCHPOINT_READ,
            WATCHPOINT_ACCESS,
        )

        # Values should match GDB Z packet types
        assert WATCHPOINT_WRITE == 2
        assert WATCHPOINT_READ == 3
        assert WATCHPOINT_ACCESS == 4

    def test_watchpoint_state_constant(self):
        """Verify EMU_STATE_WATCHPOINT exists."""
        from femu._emulator_cffi import EMU_STATE_WATCHPOINT

        assert EMU_STATE_WATCHPOINT == 4

    def test_watchpoint_error_constant(self):
        """Verify ARMV8M_ERR_WATCHPOINT exists."""
        from femu._emulator_cffi import ARMV8M_ERR_WATCHPOINT

        assert ARMV8M_ERR_WATCHPOINT == -11

    def test_emulator_state_enum(self):
        """Verify EmulatorState includes WATCHPOINT."""
        from femu.arch.base import EmulatorState

        assert hasattr(EmulatorState, "WATCHPOINT")
        assert EmulatorState.WATCHPOINT == 4


class TestWatchpointCFFI:
    """Test CFFI watchpoint bindings directly."""

    @pytest.fixture
    def cffi_modules(self):
        """Get CFFI modules, skip if library not available."""
        try:
            from femu._emulator_cffi import get_lib, get_ffi, create_emulator, ARMV8M_OK

            lib = get_lib()
            ffi = get_ffi()
            return lib, ffi, create_emulator, ARMV8M_OK
        except OSError as e:
            pytest.skip(f"Emulator library not available: {e}")

    def test_cffi_watchpoint_functions_exist(self, cffi_modules):
        """Verify watchpoint C functions are accessible."""
        lib, ffi, create_emulator, ARMV8M_OK = cffi_modules

        # These should not raise AttributeError
        assert hasattr(lib, "armv8m_emu_add_watchpoint")
        assert hasattr(lib, "armv8m_emu_remove_watchpoint")
        assert hasattr(lib, "armv8m_emu_clear_watchpoints")
        assert hasattr(lib, "armv8m_emu_get_watchpoint_hit_addr")
        assert hasattr(lib, "armv8m_emu_get_watchpoint_hit_type")

    def test_cffi_add_remove_watchpoint(self, cffi_modules):
        """Test adding and removing watchpoints via CFFI."""
        from femu._emulator_cffi import WATCHPOINT_WRITE, WATCHPOINT_READ

        lib, ffi, create_emulator, ARMV8M_OK = cffi_modules

        emu_ptr, cleanup, emu_buf = create_emulator()
        try:
            result = lib.armv8m_emu_init(emu_ptr, ffi.NULL)
            assert result == ARMV8M_OK

            # Add watchpoints
            result = lib.armv8m_emu_add_watchpoint(emu_ptr, 0x20000100, 4, WATCHPOINT_WRITE)
            assert result == ARMV8M_OK

            result = lib.armv8m_emu_add_watchpoint(emu_ptr, 0x20000200, 4, WATCHPOINT_READ)
            assert result == ARMV8M_OK

            # Remove watchpoint
            result = lib.armv8m_emu_remove_watchpoint(emu_ptr, 0x20000100, 4, WATCHPOINT_WRITE)
            assert result == ARMV8M_OK

            # Clear all
            lib.armv8m_emu_clear_watchpoints(emu_ptr)

        finally:
            cleanup()


class TestEmulatorWatchpoints:
    """Test watchpoint methods on ARMv8MEmulator class."""

    @pytest.fixture
    def emulator(self):
        """Create an emulator instance, skip if library not available."""
        try:
            from femu.arch.armv8m import ARMv8MEmulator, ARMv8MConfig

            config = ARMv8MConfig()
            emu = ARMv8MEmulator(config)
            emu.add_ram(0x20000000, 0x10000)
            return emu
        except OSError as e:
            pytest.skip(f"Emulator library not available: {e}")

    def test_emulator_has_watchpoint_methods(self, emulator):
        """Verify emulator has watchpoint methods."""
        assert hasattr(emulator, "add_watchpoint")
        assert hasattr(emulator, "remove_watchpoint")
        assert hasattr(emulator, "clear_watchpoints")
        assert hasattr(emulator, "watchpoint_hit_addr")
        assert hasattr(emulator, "watchpoint_hit_type")

    def test_add_watchpoint(self, emulator):
        """Test adding watchpoints."""
        from femu._emulator_cffi import WATCHPOINT_WRITE, WATCHPOINT_READ, WATCHPOINT_ACCESS

        # Should not raise
        emulator.add_watchpoint(0x20000100, 4, WATCHPOINT_WRITE)
        emulator.add_watchpoint(0x20000200, 4, WATCHPOINT_READ)
        emulator.add_watchpoint(0x20000300, 4, WATCHPOINT_ACCESS)

    def test_remove_watchpoint(self, emulator):
        """Test removing watchpoints."""
        from femu._emulator_cffi import WATCHPOINT_WRITE

        emulator.add_watchpoint(0x20000100, 4, WATCHPOINT_WRITE)
        # Should not raise
        emulator.remove_watchpoint(0x20000100, 4, WATCHPOINT_WRITE)

    def test_clear_watchpoints(self, emulator):
        """Test clearing all watchpoints."""
        from femu._emulator_cffi import WATCHPOINT_WRITE, WATCHPOINT_READ

        emulator.add_watchpoint(0x20000100, 4, WATCHPOINT_WRITE)
        emulator.add_watchpoint(0x20000200, 4, WATCHPOINT_READ)

        # Should not raise
        emulator.clear_watchpoints()

    def test_watchpoint_hit_properties(self, emulator):
        """Test watchpoint hit info properties."""
        # Should return values without raising
        addr = emulator.watchpoint_hit_addr
        wp_type = emulator.watchpoint_hit_type

        assert isinstance(addr, int)
        assert isinstance(wp_type, int)


class TestGDBServerWatchpoints:
    """Test GDB server watchpoint handling."""

    def test_gdb_server_imports(self):
        """Verify GDB server can be imported with watchpoint support."""
        from femu.gdb_server import GDBServer
        from femu.emulator import EmulatorState

        # Verify EmulatorState has WATCHPOINT
        assert hasattr(EmulatorState, "WATCHPOINT")
