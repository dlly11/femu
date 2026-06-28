# GDB Server

FEMU includes a GDB Remote Serial Protocol (RSP) server for debugging firmware.

## Overview

The GDB server is implemented as a single module:

```text
python/femu/gdb_server.py    # GDB RSP server (single-file implementation)
```

All protocol handling, command dispatch, and target interaction is contained in one `GDBServer` class.

## Architecture

```text
GDB Client  <---TCP--->  GDBServer  <--->  BaseEmulator
                              |
                        [blocking socket I/O]
                              |
                        _handle_command()
                              |
                   _cmd_*() method dispatch
```

The server uses **synchronous blocking sockets** (not asyncio). When `start()` is called, it blocks the current thread, listening for a single GDB client connection and processing commands in a loop until disconnect.

## GDBServer Class

```python
class GDBServer:
    """GDB Remote Serial Protocol server."""

    def __init__(self, emulator: BaseEmulator, port: int = 3333, host: str = "127.0.0.1"):
        self.emu = emulator
        self.port = port
        self.host = host
        self._socket: socket.socket | None = None
        self._client: socket.socket | None = None
        self._running = False
        self._no_ack_mode = False

    def start(self) -> None:
        """Start the GDB server (blocks until disconnect)."""
        # Binds, listens, accepts one client, enters _main_loop()

    def stop(self) -> None:
        """Stop the GDB server."""
```

### Main Loop

The `_main_loop()` method reads packets, dispatches to handlers, and sends responses:

```python
def _main_loop(self) -> None:
    while self._running:
        packet = self._recv_packet()
        if packet is None:
            break
        response = self._handle_command(packet)
        if response is not None:
            self._send_packet(response)
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

### Packet I/O

Packet reading and writing are methods on `GDBServer` itself (not a separate protocol class):

- `_recv_packet()` - Reads bytes from the socket, handles `+`/`-` ACK/NACK, Ctrl-C interrupts, and checksum verification
- `_send_packet()` - Formats and sends `$data#checksum` to the socket
- `_checksum()` - Computes the two-digit hex checksum

The server supports **QStartNoAckMode** for faster communication (skips ACK/NACK after initial handshake).

## Supported Commands

### General

| Command | Handler Method          | Description                    |
| ------- | ----------------------- | ------------------------------ |
| `?`     | `_cmd_halt_reason`      | Query halt reason              |
| `g`     | `_cmd_read_registers`   | Read all registers             |
| `G`     | `_cmd_write_registers`  | Write all registers            |
| `p`     | `_cmd_read_register`    | Read single register           |
| `P`     | `_cmd_write_register`   | Write single register          |
| `m`     | `_cmd_read_memory`      | Read memory                    |
| `M`     | `_cmd_write_memory`     | Write memory (hex)             |
| `X`     | `_cmd_write_memory_binary` | Write memory (binary)       |
| `c`     | `_cmd_continue`         | Continue execution             |
| `s`     | `_cmd_step`             | Single step                    |
| `vCont` | `_cmd_vcont`            | Verbose continue/step          |
| `D`     | `_cmd_detach`           | Detach from target             |
| `k`     | `_cmd_kill`             | Kill (disconnect)              |

### Breakpoints and Watchpoints

| Command   | Description                   |
| --------- | ----------------------------- |
| `Z0,addr` | Set software breakpoint       |
| `Z1,addr` | Set hardware breakpoint (emulated as software) |
| `Z2,addr` | Set write watchpoint          |
| `Z3,addr` | Set read watchpoint           |
| `Z4,addr` | Set access watchpoint         |
| `z0,addr` | Remove software breakpoint    |
| `z1,addr` | Remove hardware breakpoint    |
| `z2-4`    | Remove watchpoints            |

### Query Commands

| Command           | Response        | Description                |
| ----------------- | --------------- | -------------------------- |
| `qSupported`      | Feature list    | Query supported features   |
| `qAttached`       | `1`             | Attached to existing process |
| `qC`              | `QC1`           | Current thread ID          |
| `qfThreadInfo`    | `m1`            | Thread list (single thread)|
| `qsThreadInfo`    | `l`             | End of thread list         |
| `qOffsets`        | `Text=0;...`    | Section offsets            |
| `qSymbol`         | `OK`            | Symbol lookup              |
| `QStartNoAckMode` | `OK`            | Disable ACK mode           |

## Command Dispatch

All command handling is done via an if/elif chain in `_handle_command()`. There is no separate command class or dispatch table — the method directly matches packet prefixes and routes to `_cmd_*()` methods:

```python
def _handle_command(self, cmd: str) -> str | None:
    if cmd.startswith("qSupported"):
        return self._cmd_query_supported(cmd)
    elif cmd == "?":
        return self._cmd_halt_reason()
    elif cmd == "g":
        return self._cmd_read_registers()
    # ... etc
    else:
        return ""  # Unknown command
```

## Register Mapping

GDB expects registers in a specific order, formatted as **little-endian hex**:

| Index | Register | Size   |
| ----- | -------- | ------ |
| 0-12  | R0-R12   | 32-bit |
| 13    | SP       | 32-bit |
| 14    | LR       | 32-bit |
| 15    | PC       | 32-bit |
| 25    | xPSR     | 32-bit |
| 26-57 | S0-S31 (FPU) | 32-bit |
| 58    | FPSCR    | 32-bit |

Utility methods `_to_le_hex()` and `_from_le_hex()` handle the byte-order conversion.

## Stop Replies

When execution stops, the server sends a stop reply based on emulator state:

| EmulatorState   | Reply       | Signal  |
| --------------- | ----------- | ------- |
| `BREAKPOINT`    | `S05`       | SIGTRAP |
| `WATCHPOINT`    | `T05watch:` / `T05rwatch:` / `T05awatch:` | SIGTRAP + watchpoint info |
| `HALTED`        | `S00`       | No signal |
| `FAULT`         | `S0B`       | SIGSEGV |
| Other           | `S05`       | SIGTRAP (default) |

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
