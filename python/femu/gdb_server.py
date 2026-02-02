"""
GDB Remote Serial Protocol (RSP) server for FEMU.

This module implements a GDB stub that allows debugging firmware
running in the emulator using standard GDB tools.

Usage:
    from femu.emulator import Emulator
    from femu.gdb_server import GDBServer

    emu = Emulator()
    emu.load_elf("firmware.elf")

    server = GDBServer(emu, port=3333)
    server.start()  # Blocks until GDB disconnects

Connect with:
    arm-none-eabi-gdb firmware.elf -ex "target remote :3333"
"""

from __future__ import annotations

import socket
from typing import TYPE_CHECKING

from .emulator import EmulatorState
from .logging import LogCategory, get_logger

if TYPE_CHECKING:
    from .emulator import Emulator

logger = get_logger(LogCategory.GDB)


class GDBServer:
    """
    GDB Remote Serial Protocol server.

    Implements the subset of RSP commands needed for basic debugging:
    - Reading/writing registers
    - Reading/writing memory
    - Single stepping
    - Continue execution
    - Software breakpoints
    """

    # ARM Cortex-M register order for GDB
    # R0-R12, SP, LR, PC, xPSR (17 registers)
    NUM_CORE_REGS = 17

    def __init__(self, emulator: Emulator, port: int = 3333, host: str = "127.0.0.1"):
        """
        Initialize GDB server.

        Args:
            emulator: Emulator instance to debug
            port: TCP port to listen on
            host: Host address to bind to
        """
        self.emu = emulator
        self.port = port
        self.host = host

        self._socket: socket.socket | None = None
        self._client: socket.socket | None = None
        self._running = False
        self._no_ack_mode = False

    def start(self) -> None:
        """
        Start the GDB server.

        This blocks until the client disconnects or the server is stopped.
        """
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        try:
            self._socket.bind((self.host, self.port))
            self._socket.listen(1)

            logger.info(f"GDB server listening on {self.host}:{self.port}")
            print(f"GDB server listening on port {self.port}")
            print(f"Connect with: arm-none-eabi-gdb -ex 'target remote :{self.port}'")

            self._client, addr = self._socket.accept()
            logger.info(f"GDB client connected from {addr}")
            print(f"GDB client connected from {addr}")

            self._running = True
            self._main_loop()

        except KeyboardInterrupt:
            logger.info("GDB server interrupted")
        finally:
            self._cleanup()

    def stop(self) -> None:
        """Stop the GDB server."""
        self._running = False
        self._cleanup()

    def _cleanup(self) -> None:
        """Clean up sockets."""
        if self._client:
            try:
                self._client.close()
            except Exception:
                pass
            self._client = None

        if self._socket:
            try:
                self._socket.close()
            except Exception:
                pass
            self._socket = None

    def _main_loop(self) -> None:
        """Process GDB commands until disconnect."""
        while self._running:
            packet = self._recv_packet()
            if packet is None:
                break

            logger.debug(f"GDB <- {packet}")

            response = self._handle_command(packet)

            if response is not None:
                logger.debug(f"GDB -> {response}")
                self._send_packet(response)

    # =========================================================================
    # Packet I/O
    # =========================================================================

    def _recv_packet(self) -> str | None:
        """
        Receive a GDB packet.

        Packet format: $<data>#<checksum>
        Returns the data portion, or None on disconnect.
        """
        if not self._client:
            return None

        try:
            # Wait for packet start
            while True:
                c = self._client.recv(1)
                if not c:
                    return None

                if c == b"$":
                    break
                elif c == b"+":
                    # ACK - ignore
                    continue
                elif c == b"-":
                    # NACK - would need to resend, but we don't track that
                    continue
                elif c == b"\x03":
                    # Ctrl-C interrupt
                    self.emu.stop()
                    return "?"  # Request halt reason

            # Read until #
            data = b""
            while True:
                c = self._client.recv(1)
                if not c:
                    return None
                if c == b"#":
                    break
                data += c

            # Read checksum (2 hex digits)
            checksum = self._client.recv(2)
            if len(checksum) != 2:
                return None

            # Verify checksum
            expected = self._checksum(data.decode("latin-1"))
            received = checksum.decode("latin-1")

            if expected != received:
                logger.warning(f"Checksum mismatch: expected {expected}, got {received}")
                if not self._no_ack_mode:
                    self._client.send(b"-")  # NACK
                return None

            if not self._no_ack_mode:
                self._client.send(b"+")  # ACK

            return data.decode("latin-1")

        except (OSError, ConnectionResetError):
            return None

    def _send_packet(self, data: str) -> None:
        """Send a GDB packet."""
        if not self._client:
            return

        checksum = self._checksum(data)
        packet = f"${data}#{checksum}"

        try:
            self._client.send(packet.encode("latin-1"))
        except (OSError, ConnectionResetError):
            self._running = False

    def _checksum(self, data: str) -> str:
        """Calculate GDB packet checksum."""
        total = sum(ord(c) for c in data) % 256
        return f"{total:02x}"

    # =========================================================================
    # Command Handlers
    # =========================================================================

    def _handle_command(self, cmd: str) -> str | None:
        """
        Handle a GDB command.

        Returns response string, or None for no response.
        """
        if not cmd:
            return ""

        # Query commands
        if cmd.startswith("qSupported"):
            return self._cmd_query_supported(cmd)
        elif cmd.startswith("qAttached"):
            return "1"  # Attached to existing process
        elif cmd.startswith("qC"):
            return "QC1"  # Current thread ID
        elif cmd.startswith("qfThreadInfo"):
            return "m1"  # Thread list
        elif cmd.startswith("qsThreadInfo"):
            return "l"  # End of thread list
        elif cmd.startswith("qOffsets"):
            return "Text=0;Data=0;Bss=0"
        elif cmd.startswith("qSymbol"):
            return "OK"

        # Halt reason
        elif cmd == "?":
            return self._cmd_halt_reason()

        # Register access
        elif cmd == "g":
            return self._cmd_read_registers()
        elif cmd.startswith("G"):
            return self._cmd_write_registers(cmd[1:])
        elif cmd.startswith("p"):
            return self._cmd_read_register(cmd[1:])
        elif cmd.startswith("P"):
            return self._cmd_write_register(cmd[1:])

        # Memory access
        elif cmd.startswith("m"):
            return self._cmd_read_memory(cmd[1:])
        elif cmd.startswith("M"):
            return self._cmd_write_memory(cmd[1:])
        elif cmd.startswith("X"):
            return self._cmd_write_memory_binary(cmd[1:])

        # Execution control
        elif cmd == "c" or cmd.startswith("c"):
            return self._cmd_continue(cmd)
        elif cmd == "s" or cmd.startswith("s"):
            return self._cmd_step(cmd)
        elif cmd.startswith("vCont"):
            return self._cmd_vcont(cmd)

        # Breakpoints
        elif cmd.startswith("Z"):
            return self._cmd_set_breakpoint(cmd[1:])
        elif cmd.startswith("z"):
            return self._cmd_clear_breakpoint(cmd[1:])

        # Kill/detach
        elif cmd == "D":
            return self._cmd_detach()
        elif cmd == "k":
            return self._cmd_kill()

        # Set commands
        elif cmd.startswith("QStartNoAckMode"):
            self._no_ack_mode = True
            return "OK"

        # Unknown command - return empty response
        else:
            logger.debug(f"Unknown GDB command: {cmd}")
            return ""

    def _cmd_query_supported(self, cmd: str) -> str:
        """Handle qSupported query."""
        # Advertise watchpoint support (hwbreak for hw breakpoints which we emulate as sw)
        return "PacketSize=4096;QStartNoAckMode+;hwbreak+"

    def _cmd_halt_reason(self) -> str:
        """Return halt reason (? command)."""
        state = self.emu.state
        if state == EmulatorState.BREAKPOINT:
            return "S05"  # SIGTRAP
        elif state == EmulatorState.WATCHPOINT:
            # Return stop reply with watchpoint info
            return self._watchpoint_stop_reply()
        elif state == EmulatorState.HALTED:
            return "S00"  # No signal
        elif state == EmulatorState.FAULT:
            return "S0B"  # SIGSEGV
        else:
            return "S05"  # SIGTRAP (default halt)

    def _cmd_read_registers(self) -> str:
        """Read all registers (g command)."""
        result = ""

        # R0-R12
        for i in range(13):
            val = self.emu.get_reg(i)
            result += self._to_le_hex(val)

        # SP (R13)
        result += self._to_le_hex(self.emu.sp)

        # LR (R14)
        result += self._to_le_hex(self.emu.lr)

        # PC (R15)
        result += self._to_le_hex(self.emu.pc)

        # xPSR (CPSR in GDB parlance)
        result += self._to_le_hex(self.emu.xpsr)

        return result

    def _cmd_write_registers(self, data: str) -> str:
        """Write all registers (G command)."""
        try:
            offset = 0

            # R0-R12
            for i in range(13):
                val = self._from_le_hex(data[offset : offset + 8])
                self.emu.set_reg(i, val)
                offset += 8

            # SP
            self.emu.sp = self._from_le_hex(data[offset : offset + 8])
            offset += 8

            # LR
            self.emu.lr = self._from_le_hex(data[offset : offset + 8])
            offset += 8

            # PC
            self.emu.pc = self._from_le_hex(data[offset : offset + 8])
            offset += 8

            # xPSR
            if offset + 8 <= len(data):
                self.emu.xpsr = self._from_le_hex(data[offset : offset + 8])

            return "OK"
        except Exception as e:
            logger.error(f"Error writing registers: {e}")
            return "E01"

    def _cmd_read_register(self, args: str) -> str:
        """Read single register (p command)."""
        try:
            reg_num = int(args, 16)

            if reg_num < 13:
                val = self.emu.get_reg(reg_num)
            elif reg_num == 13:
                val = self.emu.sp
            elif reg_num == 14:
                val = self.emu.lr
            elif reg_num == 15:
                val = self.emu.pc
            elif reg_num == 25:  # xPSR/CPSR
                val = self.emu.xpsr
            elif 26 <= reg_num <= 57:  # FPU S0-S31
                val = self.emu.get_fpu_reg(reg_num - 26)
            elif reg_num == 58:  # FPSCR
                val = self.emu.fpscr
            else:
                return "E01"

            return self._to_le_hex(val)
        except Exception as e:
            logger.error(f"Error reading register {args}: {e}")
            return "E01"

    def _cmd_write_register(self, args: str) -> str:
        """Write single register (P command)."""
        try:
            reg_str, val_str = args.split("=")
            reg_num = int(reg_str, 16)
            val = self._from_le_hex(val_str)

            if reg_num < 13:
                self.emu.set_reg(reg_num, val)
            elif reg_num == 13:
                self.emu.sp = val
            elif reg_num == 14:
                self.emu.lr = val
            elif reg_num == 15:
                self.emu.pc = val
            elif reg_num == 25:
                self.emu.xpsr = val
            elif 26 <= reg_num <= 57:
                self.emu.set_fpu_reg(reg_num - 26, val)
            elif reg_num == 58:
                self.emu.fpscr = val
            else:
                return "E01"

            return "OK"
        except Exception as e:
            logger.error(f"Error writing register {args}: {e}")
            return "E01"

    def _cmd_read_memory(self, args: str) -> str:
        """Read memory (m command)."""
        try:
            addr_str, length_str = args.split(",")
            addr = int(addr_str, 16)
            length = int(length_str, 16)

            data = self.emu.read_bytes(addr, length)

            # Convert to hex
            return data.hex()
        except Exception as e:
            logger.error(f"Error reading memory {args}: {e}")
            return "E01"

    def _cmd_write_memory(self, args: str) -> str:
        """Write memory (M command)."""
        try:
            params, data_hex = args.split(":")
            addr_str, length_str = params.split(",")
            addr = int(addr_str, 16)
            length = int(length_str, 16)

            data = bytes.fromhex(data_hex)
            if len(data) != length:
                return "E01"

            written = self.emu.write_bytes(addr, data)
            if written != length:
                return "E01"

            return "OK"
        except Exception as e:
            logger.error(f"Error writing memory {args}: {e}")
            return "E01"

    def _cmd_write_memory_binary(self, args: str) -> str:
        """Write memory with binary data (X command)."""
        try:
            # Find the colon separating params from data
            colon_idx = args.index(":")
            params = args[:colon_idx]
            data = args[colon_idx + 1 :]

            addr_str, length_str = params.split(",")
            addr = int(addr_str, 16)
            length = int(length_str, 16)

            # Unescape binary data
            binary_data = self._unescape_binary(data)

            if len(binary_data) != length:
                return "E01"

            written = self.emu.write_bytes(addr, binary_data)
            if written != length:
                return "E01"

            return "OK"
        except Exception as e:
            logger.error(f"Error writing binary memory: {e}")
            return "E01"

    def _cmd_continue(self, cmd: str) -> str:
        """Continue execution (c command)."""
        # Optional address argument
        if len(cmd) > 1:
            addr = int(cmd[1:], 16)
            self.emu.pc = addr

        self.emu.run()
        return self._stop_reply()

    def _cmd_step(self, cmd: str) -> str:
        """Single step (s command)."""
        # Optional address argument
        if len(cmd) > 1:
            addr = int(cmd[1:], 16)
            self.emu.pc = addr

        self.emu.step()
        return self._stop_reply()

    def _cmd_vcont(self, cmd: str) -> str:
        """Handle vCont command."""
        if cmd == "vCont?":
            return "vCont;c;s;C;S"

        # Parse vCont;action
        if cmd.startswith("vCont;"):
            action = cmd[6:]
            if action.startswith("c"):
                return self._cmd_continue("c")
            elif action.startswith("s"):
                return self._cmd_step("s")

        return ""

    def _cmd_set_breakpoint(self, args: str) -> str:
        """Set breakpoint or watchpoint (Z command)."""
        try:
            parts = args.split(",")
            bp_type = int(parts[0])
            addr = int(parts[1], 16)
            size = int(parts[2], 16) if len(parts) > 2 else 4

            if bp_type == 0:  # Software breakpoint
                self.emu.add_breakpoint(addr)
                return "OK"
            elif bp_type == 1:  # Hardware breakpoint
                # Treat as software breakpoint
                self.emu.add_breakpoint(addr)
                return "OK"
            elif bp_type == 2:  # Write watchpoint
                from . import _emulator_cffi as cffi

                self.emu.add_watchpoint(addr, size, cffi.WATCHPOINT_WRITE)
                return "OK"
            elif bp_type == 3:  # Read watchpoint
                from . import _emulator_cffi as cffi

                self.emu.add_watchpoint(addr, size, cffi.WATCHPOINT_READ)
                return "OK"
            elif bp_type == 4:  # Access watchpoint (read/write)
                from . import _emulator_cffi as cffi

                self.emu.add_watchpoint(addr, size, cffi.WATCHPOINT_ACCESS)
                return "OK"
            else:
                return ""  # Unsupported
        except Exception as e:
            logger.error(f"Error setting breakpoint/watchpoint: {e}")
            return "E01"

    def _cmd_clear_breakpoint(self, args: str) -> str:
        """Clear breakpoint or watchpoint (z command)."""
        try:
            parts = args.split(",")
            bp_type = int(parts[0])
            addr = int(parts[1], 16)
            size = int(parts[2], 16) if len(parts) > 2 else 4

            if bp_type in (0, 1):  # Software/hardware breakpoint
                self.emu.remove_breakpoint(addr)
                return "OK"
            elif bp_type == 2:  # Write watchpoint
                from . import _emulator_cffi as cffi

                self.emu.remove_watchpoint(addr, size, cffi.WATCHPOINT_WRITE)
                return "OK"
            elif bp_type == 3:  # Read watchpoint
                from . import _emulator_cffi as cffi

                self.emu.remove_watchpoint(addr, size, cffi.WATCHPOINT_READ)
                return "OK"
            elif bp_type == 4:  # Access watchpoint (read/write)
                from . import _emulator_cffi as cffi

                self.emu.remove_watchpoint(addr, size, cffi.WATCHPOINT_ACCESS)
                return "OK"
            else:
                return ""
        except Exception as e:
            logger.error(f"Error clearing breakpoint/watchpoint: {e}")
            return "E01"

    def _cmd_detach(self) -> str:
        """Detach from target (D command)."""
        self._running = False
        return "OK"

    def _cmd_kill(self) -> str:
        """Kill target (k command)."""
        self.emu.stop()
        self._running = False
        return "OK"

    def _stop_reply(self) -> str:
        """Generate stop reply based on emulator state."""
        state = self.emu.state
        if state == EmulatorState.BREAKPOINT:
            return "S05"  # SIGTRAP
        elif state == EmulatorState.WATCHPOINT:
            return self._watchpoint_stop_reply()
        elif state == EmulatorState.HALTED:
            return "S00"
        elif state == EmulatorState.FAULT:
            return "S0B"  # SIGSEGV
        else:
            return "S05"

    def _watchpoint_stop_reply(self) -> str:
        """Generate stop reply for watchpoint hit."""
        from . import _emulator_cffi as cffi

        addr = self.emu.watchpoint_hit_addr
        wp_type = self.emu.watchpoint_hit_type

        # GDB expects: T05watch:<addr>;  or  T05rwatch:<addr>;  or  T05awatch:<addr>;
        if wp_type == cffi.WATCHPOINT_WRITE:
            return f"T05watch:{addr:x};"
        elif wp_type == cffi.WATCHPOINT_READ:
            return f"T05rwatch:{addr:x};"
        else:  # ACCESS
            return f"T05awatch:{addr:x};"

    # =========================================================================
    # Utilities
    # =========================================================================

    def _to_le_hex(self, value: int) -> str:
        """Convert 32-bit value to little-endian hex string."""
        # GDB expects little-endian byte order
        b0, b1 = value & 0xFF, (value >> 8) & 0xFF
        b2, b3 = (value >> 16) & 0xFF, (value >> 24) & 0xFF
        return f"{b0:02x}{b1:02x}{b2:02x}{b3:02x}"

    def _from_le_hex(self, hex_str: str) -> int:
        """Convert little-endian hex string to 32-bit value."""
        if len(hex_str) < 8:
            hex_str = hex_str + "0" * (8 - len(hex_str))

        b0 = int(hex_str[0:2], 16)
        b1 = int(hex_str[2:4], 16)
        b2 = int(hex_str[4:6], 16)
        b3 = int(hex_str[6:8], 16)

        return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24)

    def _unescape_binary(self, data: str) -> bytes:
        """Unescape GDB binary data."""
        result = bytearray()
        i = 0
        while i < len(data):
            c = data[i]
            if c == "}":
                # Escape sequence
                i += 1
                if i < len(data):
                    result.append(ord(data[i]) ^ 0x20)
            else:
                result.append(ord(c))
            i += 1
        return bytes(result)
