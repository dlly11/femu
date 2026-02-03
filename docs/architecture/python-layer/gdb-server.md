# GDB Server

FEMU includes a GDB Remote Serial Protocol (RSP) server for debugging firmware.

## Overview

```text
python/femu/gdb/
├── __init__.py
├── server.py           # Main GDB server
├── protocol.py         # RSP packet handling
├── commands.py         # GDB command handlers
└── target.py           # Target abstraction
```

## Architecture

```text
GDB Client  ◄───TCP───►  GDBServer  ◄───►  ARMv8MEmulator
                              │
                              ▼
                         GDBProtocol
                              │
                              ▼
                        GDBTarget (adapter)
```

## GDBServer

Main server class using asyncio:

```python
class GDBServer:
    """GDB Remote Serial Protocol server."""

    def __init__(self, emulator: ARMv8MEmulator, port: int = 3333):
        self._emu = emulator
        self._port = port
        self._protocol = GDBProtocol()
        self._target = GDBTarget(emulator)

    async def start(self) -> None:
        """Start the GDB server."""
        server = await asyncio.start_server(
            self._handle_client,
            host="127.0.0.1",
            port=self._port
        )
        await server.serve_forever()

    async def _handle_client(self, reader, writer) -> None:
        """Handle a GDB client connection."""
        while True:
            packet = await self._protocol.read_packet(reader)
            if packet is None:
                break

            response = self._handle_command(packet)
            await self._protocol.write_packet(writer, response)
```

## RSP Protocol

The GDB Remote Serial Protocol uses packets:

```text
$<data>#<checksum>

Example: $g#67
- $ = packet start
- g = command (read registers)
- # = checksum delimiter
- 67 = two-digit hex checksum
```

### Packet Handling

```python
class GDBProtocol:
    """RSP packet encoding/decoding."""

    async def read_packet(self, reader) -> str | None:
        """Read a packet from the connection."""
        # Wait for '$'
        while True:
            char = await reader.read(1)
            if char == b"$":
                break
            if char == b"":
                return None

        # Read until '#'
        data = b""
        while True:
            char = await reader.read(1)
            if char == b"#":
                break
            data += char

        # Read checksum
        checksum = await reader.read(2)

        # Verify checksum
        expected = sum(data) % 256
        if int(checksum, 16) != expected:
            raise ProtocolError("Checksum mismatch")

        return data.decode("ascii")

    async def write_packet(self, writer, data: str) -> None:
        """Write a packet to the connection."""
        checksum = sum(data.encode("ascii")) % 256
        packet = f"${data}#{checksum:02x}"
        writer.write(packet.encode("ascii"))
        await writer.drain()
```

## Supported Commands

### General

| Command | Description                    |
| ------- | ------------------------------ |
| `?`     | Query halt reason              |
| `g`     | Read all registers             |
| `G`     | Write all registers            |
| `p`     | Read single register           |
| `P`     | Write single register          |
| `m`     | Read memory                    |
| `M`     | Write memory                   |
| `c`     | Continue execution             |
| `s`     | Single step                    |
| `k`     | Kill (disconnect)              |

### Breakpoints

| Command  | Description                   |
| -------- | ----------------------------- |
| `Z0,addr`| Set software breakpoint       |
| `z0,addr`| Remove software breakpoint    |
| `Z2,addr`| Set write watchpoint          |
| `Z3,addr`| Set read watchpoint           |
| `Z4,addr`| Set access watchpoint         |

### Query

| Command         | Description                |
| --------------- | -------------------------- |
| `qSupported`    | Query supported features   |
| `qAttached`     | Query attach status        |
| `qfThreadInfo`  | Query thread info          |

## Command Handlers

```python
class GDBCommands:
    """GDB command implementations."""

    def __init__(self, target: GDBTarget):
        self._target = target

    def handle(self, packet: str) -> str:
        """Dispatch command to handler."""
        cmd = packet[0]

        handlers = {
            "?": self._halt_reason,
            "g": self._read_registers,
            "G": self._write_registers,
            "m": self._read_memory,
            "M": self._write_memory,
            "c": self._continue,
            "s": self._step,
            "Z": self._set_breakpoint,
            "z": self._remove_breakpoint,
        }

        handler = handlers.get(cmd, self._unsupported)
        return handler(packet)

    def _read_registers(self, packet: str) -> str:
        """Read all registers."""
        regs = self._target.read_registers()
        # Format as hex string
        return "".join(f"{r:08x}" for r in regs)

    def _read_memory(self, packet: str) -> str:
        """Read memory: m<addr>,<length>"""
        match = re.match(r"m([0-9a-f]+),([0-9a-f]+)", packet)
        addr = int(match.group(1), 16)
        length = int(match.group(2), 16)

        try:
            data = self._target.read_memory(addr, length)
            return data.hex()
        except MemoryError:
            return "E01"  # Error response

    def _continue(self, packet: str) -> str:
        """Continue execution."""
        status = self._target.run()
        return self._format_stop_reply(status)
```

## GDBTarget

Adapter between GDB protocol and emulator:

```python
class GDBTarget:
    """Target abstraction for GDB server."""

    def __init__(self, emulator: ARMv8MEmulator):
        self._emu = emulator
        self._breakpoints: dict[int, int] = {}  # addr -> bp_id

    def read_registers(self) -> list[int]:
        """Read all registers in GDB order."""
        regs = []
        for i in range(16):  # R0-R15
            regs.append(self._emu.get_reg(i))
        regs.append(self._emu.get_xpsr())  # xPSR
        return regs

    def read_memory(self, addr: int, length: int) -> bytes:
        """Read memory."""
        return self._emu.read_bytes(addr, length)

    def run(self) -> EmuStatus:
        """Run until breakpoint or error."""
        return self._emu.run()

    def step(self) -> EmuStatus:
        """Single step."""
        return self._emu.step()

    def set_breakpoint(self, addr: int) -> bool:
        """Set a breakpoint."""
        bp_id = self._emu.set_breakpoint(addr)
        if bp_id >= 0:
            self._breakpoints[addr] = bp_id
            return True
        return False
```

## Register Mapping

GDB expects registers in a specific order:

| Index | Register | Size   |
| ----- | -------- | ------ |
| 0-15  | R0-R15   | 32-bit |
| 16    | xPSR     | 32-bit |
| 17-24 | (FPU)    | 32-bit |

```python
def format_register(value: int) -> str:
    """Format register as little-endian hex."""
    # GDB expects little-endian
    return "".join(f"{(value >> (i*8)) & 0xff:02x}" for i in range(4))
```

## Stop Replies

When execution stops, send stop reply:

```python
def _format_stop_reply(self, status: EmuStatus) -> str:
    """Format stop reply packet."""
    if status == EmuStatus.HALTED:
        return "S05"  # SIGTRAP
    elif status == EmuStatus.FAULT:
        return "S0B"  # SIGSEGV
    else:
        return "S05"  # Default to SIGTRAP
```

## Usage

```bash
# Start FEMU with GDB server
femu run firmware.elf --gdb-port 3333

# Connect with GDB
arm-none-eabi-gdb firmware.elf -ex "target remote :3333"
```

## See Also

- [GDB Remote Serial Protocol](https://sourceware.org/gdb/onlinedocs/gdb/Remote-Protocol.html)
- [User Guide: GDB Debugging](../../user/gdb-debugging.md)
