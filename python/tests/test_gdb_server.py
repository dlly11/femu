"""Unit tests for the GDB RSP server (femu.gdb_server).

These tests drive the protocol handlers directly with a fake emulator, so they
need neither a real socket nor the compiled C library.
"""

from __future__ import annotations

from femu.emulator import EmulatorState
from femu.gdb_server import GDBServer


class FakeEmulator:
    """Minimal in-memory stand-in for the emulator interface GDBServer uses."""

    def __init__(self) -> None:
        self.pc = 0x08000000
        self.sp = 0x20010000
        self.lr = 0xFFFFFFFF
        self.status = 0x01000000
        self.fpscr = 0
        self.state = EmulatorState.HALTED
        self.watchpoint_hit_addr = 0
        self.watchpoint_hit_type = 0
        self._regs = [0] * 13
        self._fpu = [0] * 32
        self._mem: dict[int, int] = {}
        self.breakpoints: set[int] = set()
        self.watchpoints: list[tuple[int, int, int]] = []
        self.ran = False
        self.stepped = False

    # Registers
    def get_reg(self, n: int) -> int:
        return self._regs[n]

    def set_reg(self, n: int, value: int) -> None:
        self._regs[n] = value & 0xFFFFFFFF

    def get_fpu_reg(self, n: int) -> int:
        return self._fpu[n]

    def set_fpu_reg(self, n: int, value: int) -> None:
        self._fpu[n] = value & 0xFFFFFFFF

    # Memory
    def read_bytes(self, addr: int, length: int) -> bytes:
        return bytes(self._mem.get(addr + i, 0) for i in range(length))

    def write_bytes(self, addr: int, data: bytes) -> int:
        for i, b in enumerate(data):
            self._mem[addr + i] = b
        return len(data)

    # Execution
    def run(self) -> None:
        self.ran = True

    def step(self) -> None:
        self.stepped = True

    def stop(self) -> None:
        pass

    # Breakpoints / watchpoints
    def add_breakpoint(self, addr: int) -> None:
        self.breakpoints.add(addr)

    def remove_breakpoint(self, addr: int) -> None:
        self.breakpoints.discard(addr)

    def add_watchpoint(self, addr: int, size: int, wp_type: int) -> None:
        self.watchpoints.append((addr, size, wp_type))

    def remove_watchpoint(self, addr: int, size: int, wp_type: int) -> None:
        if (addr, size, wp_type) in self.watchpoints:
            self.watchpoints.remove((addr, size, wp_type))


def make_server() -> tuple[GDBServer, FakeEmulator]:
    emu = FakeEmulator()
    return GDBServer(emu, port=0), emu


class TestChecksumAndHex:
    def test_checksum_known_value(self) -> None:
        server, _ = make_server()
        # sum of ord('O','K') = 79 + 75 = 154 = 0x9a
        assert server._checksum("OK") == "9a"

    def test_checksum_empty(self) -> None:
        server, _ = make_server()
        assert server._checksum("") == "00"

    def test_checksum_wraps_mod_256(self) -> None:
        server, _ = make_server()
        # 0xFF char repeated; modulo 256
        assert server._checksum("\xff\xff") == f"{(255 + 255) % 256:02x}"

    def test_le_hex_roundtrip(self) -> None:
        server, _ = make_server()
        for value in (0, 1, 0x08000000, 0xDEADBEEF, 0xFFFFFFFF):
            assert server._from_le_hex(server._to_le_hex(value)) == value

    def test_to_le_hex_byte_order(self) -> None:
        server, _ = make_server()
        # 0x12345678 little-endian => 78 56 34 12
        assert server._to_le_hex(0x12345678) == "78563412"

    def test_from_le_hex_short_input_padded(self) -> None:
        server, _ = make_server()
        # "01" padded to "01000000" -> 0x00000001
        assert server._from_le_hex("01") == 1


class TestUnescapeBinary:
    def test_plain_bytes(self) -> None:
        server, _ = make_server()
        assert server._unescape_binary("ABC") == b"ABC"

    def test_escape_sequence(self) -> None:
        server, _ = make_server()
        # '}' followed by char XOR 0x20. 0x7d ('}') escaped is '}' + (0x7d ^ 0x20)
        escaped = "}" + chr(0x7D ^ 0x20)
        assert server._unescape_binary(escaped) == bytes([0x7D])


class TestRegisterCommands:
    def test_read_all_registers_length(self) -> None:
        server, emu = make_server()
        emu.set_reg(0, 0x11111111)
        out = server._cmd_read_registers()
        # 17 core registers, 8 hex chars each
        assert len(out) == 17 * 8
        assert out.startswith("11111111")

    def test_write_all_registers_roundtrip(self) -> None:
        server, emu = make_server()
        values = list(range(17))
        data = "".join(server._to_le_hex(v) for v in values)
        assert server._cmd_write_registers(data) == "OK"
        assert emu.get_reg(0) == 0
        assert emu.get_reg(1) == 1
        assert emu.sp == 13
        assert emu.lr == 14
        assert emu.pc == 15

    def test_read_single_register(self) -> None:
        server, emu = make_server()
        emu.set_reg(5, 0xCAFEBABE)
        assert server._from_le_hex(server._cmd_read_register("5")) == 0xCAFEBABE

    def test_read_pc_register(self) -> None:
        server, emu = make_server()
        emu.pc = 0x08001234
        assert server._from_le_hex(server._cmd_read_register("f")) == 0x08001234  # reg 15

    def test_read_invalid_register(self) -> None:
        server, _ = make_server()
        assert server._cmd_read_register("ff") == "E01"

    def test_write_single_register(self) -> None:
        server, emu = make_server()
        server._cmd_write_register("3=" + server._to_le_hex(0x55))
        assert emu.get_reg(3) == 0x55

    def test_read_register_bad_input_returns_error(self) -> None:
        server, _ = make_server()
        assert server._cmd_read_register("zz") == "E01"


class TestMemoryCommands:
    def test_write_then_read_memory(self) -> None:
        server, _ = make_server()
        assert server._cmd_write_memory("20000000,4:deadbeef") == "OK"
        assert server._cmd_read_memory("20000000,4") == "deadbeef"

    def test_write_memory_length_mismatch(self) -> None:
        server, _ = make_server()
        # declared length 4 but only 2 bytes of hex data
        assert server._cmd_write_memory("20000000,4:dead") == "E01"

    def test_read_memory_bad_args(self) -> None:
        server, _ = make_server()
        assert server._cmd_read_memory("nonsense") == "E01"

    def test_write_memory_binary(self) -> None:
        server, emu = make_server()
        assert server._cmd_write_memory_binary("20000000,3:ABC") == "OK"
        assert emu.read_bytes(0x20000000, 3) == b"ABC"


class TestDispatchAndControl:
    def test_handle_empty_command(self) -> None:
        server, _ = make_server()
        assert server._handle_command("") == ""

    def test_handle_unknown_command(self) -> None:
        server, _ = make_server()
        assert server._handle_command("UNSUPPORTED") == ""

    def test_qattached(self) -> None:
        server, _ = make_server()
        assert server._handle_command("qAttached") == "1"

    def test_no_ack_mode_enable(self) -> None:
        server, _ = make_server()
        assert server._handle_command("QStartNoAckMode") == "OK"
        assert server._no_ack_mode is True

    def test_continue_runs_emulator(self) -> None:
        server, emu = make_server()
        server._handle_command("c")
        assert emu.ran is True

    def test_step_steps_emulator(self) -> None:
        server, emu = make_server()
        server._handle_command("s")
        assert emu.stepped is True

    def test_continue_with_address_sets_pc(self) -> None:
        server, emu = make_server()
        server._cmd_continue("c08000100")
        assert emu.pc == 0x08000100

    def test_detach(self) -> None:
        server, _ = make_server()
        assert server._handle_command("D") == "OK"

    def test_dispatch_read_registers(self) -> None:
        server, _ = make_server()
        assert len(server._handle_command("g")) == 17 * 8


class TestBreakpointCommands:
    def test_set_and_clear_software_breakpoint(self) -> None:
        server, emu = make_server()
        assert server._cmd_set_breakpoint("0,8000100,2") == "OK"
        assert 0x8000100 in emu.breakpoints
        assert server._cmd_clear_breakpoint("0,8000100,2") == "OK"
        assert 0x8000100 not in emu.breakpoints

    def test_set_write_watchpoint(self) -> None:
        server, emu = make_server()
        assert server._cmd_set_breakpoint("2,20000000,4") == "OK"
        assert len(emu.watchpoints) == 1
        addr, size, _wp = emu.watchpoints[0]
        assert addr == 0x20000000
        assert size == 4

    def test_set_breakpoint_bad_args(self) -> None:
        server, _ = make_server()
        assert server._cmd_set_breakpoint("garbage") == "E01"


class TestStopReply:
    def test_breakpoint_stop_reply(self) -> None:
        server, emu = make_server()
        emu.state = EmulatorState.BREAKPOINT
        assert server._stop_reply() == "S05"

    def test_halted_stop_reply(self) -> None:
        server, emu = make_server()
        emu.state = EmulatorState.HALTED
        assert server._stop_reply() == "S00"

    def test_fault_stop_reply(self) -> None:
        server, emu = make_server()
        emu.state = EmulatorState.FAULT
        assert server._stop_reply() == "S0B"
