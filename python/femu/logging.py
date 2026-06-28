"""Unified logging system for FEMU.

This module provides a logging bridge between C code and Python's logging module,
enabling unified output with configurable verbosity.

Usage::

    from femu import configure_logging, LogLevel, LogCategory

    # Basic logging to stderr
    configure_logging(level=LogLevel.DEBUG)

    # TRACE for executor only, log to file
    configure_logging(
        level=LogLevel.INFO,
        category_levels={LogCategory.EXECUTOR: LogLevel.TRACE},
        log_file="trace.log"
    )

    # JSON format to file
    configure_logging(
        level=LogLevel.DEBUG,
        log_file="debug.json",
        json_format=True
    )
"""

from __future__ import annotations

import json
import logging
import sys
from datetime import datetime, timezone
from enum import IntEnum
from typing import TYPE_CHECKING, TextIO

from . import _emulator_cffi as cffi

if TYPE_CHECKING:
    from pathlib import Path

    from ._cffi_types import EmulatorLib

# =============================================================================
# Custom TRACE log level
# =============================================================================

# Add TRACE level below DEBUG (standard DEBUG is 10)
TRACE = 5
logging.addLevelName(TRACE, "TRACE")


def _trace(self: logging.Logger, message: str, *args: object) -> None:
    """Log at TRACE level."""
    if self.isEnabledFor(TRACE):
        self._log(TRACE, message, args)


# Add trace method to Logger class
logging.Logger.trace = _trace  # type: ignore[attr-defined]


# =============================================================================
# Log Level Enum
# =============================================================================


class LogLevel(IntEnum):
    """Log severity levels matching C EmuLogLevel."""

    TRACE = 0
    DEBUG = 1
    INFO = 2
    WARNING = 3
    ERROR = 4


# Map C log levels to Python logging levels
_LEVEL_MAP: dict[int, int] = {
    LogLevel.TRACE: TRACE,
    LogLevel.DEBUG: logging.DEBUG,
    LogLevel.INFO: logging.INFO,
    LogLevel.WARNING: logging.WARNING,
    LogLevel.ERROR: logging.ERROR,
}


# =============================================================================
# Log Category Enum
# =============================================================================


class LogCategory(IntEnum):
    """Log categories matching C EmuLogCategory."""

    DECODER = 0
    EXECUTOR = 1
    MEMORY = 2
    NVIC = 3
    MPU = 4
    PERIPHERAL = 5
    GDB = 6
    EMULATOR = 7


# Map C category indices to logger names
_CATEGORY_NAMES: dict[int, str] = {
    LogCategory.DECODER: "decoder",
    LogCategory.EXECUTOR: "executor",
    LogCategory.MEMORY: "memory",
    LogCategory.NVIC: "nvic",
    LogCategory.MPU: "mpu",
    LogCategory.PERIPHERAL: "peripheral",
    LogCategory.GDB: "gdb",
    LogCategory.EMULATOR: "emulator",
}


# =============================================================================
# JSON Formatter
# =============================================================================


class JSONFormatter(logging.Formatter):
    """JSON log formatter for structured logging."""

    def format(self, record: logging.LogRecord) -> str:
        """Format log record as JSON."""
        log_dict = {
            "timestamp": datetime.now(timezone.utc).replace(tzinfo=None).isoformat() + "Z",
            "level": record.levelname,
            "logger": record.name,
            "file": record.filename,
            "line": record.lineno,
            "message": record.getMessage(),
        }

        # Add extra fields if present
        if hasattr(record, "c_file"):
            log_dict["c_file"] = record.c_file
        if hasattr(record, "c_line"):
            log_dict["c_line"] = record.c_line
        if hasattr(record, "c_func"):
            log_dict["c_func"] = record.c_func

        return json.dumps(log_dict)


# =============================================================================
# C Callback
# =============================================================================

# Keep references to prevent garbage collection
_ffi = cffi.get_ffi()
_callback_handle = None
_log_file_handle: TextIO | None = None


@_ffi.callback("void(void*, int, int, const char*, int, const char*, const char*)")
def _c_log_callback(  # noqa: PLR0913 - signature fixed by the C EmuLogCallback ABI
    _ctx: object,
    level: int,
    category: int,
    file_ptr: object,
    line: int,
    func_ptr: object,
    msg_ptr: object,
) -> None:
    """C log callback that routes messages to Python logging.

    This function is called from C code through the EmuLogCallback mechanism.
    """
    # Get string values from C pointers
    try:
        filename = _ffi.string(file_ptr).decode("utf-8") if file_ptr else "unknown"
        func = _ffi.string(func_ptr).decode("utf-8") if func_ptr else "unknown"
        message = _ffi.string(msg_ptr).decode("utf-8") if msg_ptr else ""
    except Exception:  # noqa: BLE001 - C log callback must not raise into C
        return  # Silently ignore decode errors

    # Get the appropriate logger
    cat_name = _CATEGORY_NAMES.get(category, "unknown")
    logger = logging.getLogger(f"femu.{cat_name}")

    # Map C level to Python level
    py_level = _LEVEL_MAP.get(level, logging.INFO)

    # Create log record with extra context
    extra = {
        "c_file": filename,
        "c_line": line,
        "c_func": func,
    }

    # Log the message
    logger.log(py_level, message, extra=extra)


# =============================================================================
# Public API
# =============================================================================


def get_logger(category: LogCategory) -> logging.Logger:
    """Get a logger for a specific category.

    Args:
        category: The log category

    Returns:
        A Python logger instance configured for this category
    """
    cat_name = _CATEGORY_NAMES.get(category, "unknown")
    return logging.getLogger(f"femu.{cat_name}")


def _try_get_lib() -> EmulatorLib | None:
    """Return the emulator library, or None if it isn't available."""
    try:
        return cffi.get_lib()
    except OSError:
        return None


def _configure_c_logging(
    lib: EmulatorLib | None,
    level: LogLevel,
    category_levels: dict[LogCategory, LogLevel] | None,
) -> None:
    """Install the C->Python log callback and push level settings into C."""
    global _callback_handle  # noqa: PLW0603 - module-level handle kept alive for C
    if lib is None:
        return
    _callback_handle = _c_log_callback  # Keep a reference so it isn't GC'd.
    lib.emu_log_set_callback(_c_log_callback, _ffi.NULL)
    lib.emu_log_set_level(int(level))
    lib.emu_log_set_enabled(True)
    if category_levels:
        for cat, cat_level in category_levels.items():
            lib.emu_log_set_category_level(int(cat), int(cat_level))


def _make_formatter(*, json_format: bool) -> logging.Formatter:
    """Build the log formatter (JSON or human-readable)."""
    if json_format:
        return JSONFormatter()
    fmt = "%(asctime)s.%(msecs)03d [%(name)s] %(levelname)-5s %(filename)s:%(lineno)d: %(message)s"
    return logging.Formatter(fmt, datefmt="%H:%M:%S")


def _make_handlers(
    log_file: str | Path | None,
    stream: TextIO | None,
    formatter: logging.Formatter,
) -> list[logging.Handler]:
    """Build the stream and/or file handlers for the root logger."""
    global _log_file_handle  # noqa: PLW0603 - module-level handle kept alive for reuse
    handlers: list[logging.Handler] = []

    if log_file is None or stream is not None:
        stream_handler = logging.StreamHandler(stream or sys.stderr)
        stream_handler.setFormatter(formatter)
        handlers.append(stream_handler)

    if log_file is not None:
        if _log_file_handle is not None:
            _log_file_handle.close()
            _log_file_handle = None
        file_handler = logging.FileHandler(str(log_file), mode="w")
        file_handler.setFormatter(formatter)
        handlers.append(file_handler)

    return handlers


def _configure_python_category_levels(
    category_levels: dict[LogCategory, LogLevel] | None,
) -> None:
    """Apply per-category level overrides to the Python loggers."""
    if not category_levels:
        return
    for cat, cat_level in category_levels.items():
        cat_name = _CATEGORY_NAMES.get(cat, "unknown")
        cat_logger = logging.getLogger(f"femu.{cat_name}")
        cat_logger.setLevel(_LEVEL_MAP.get(cat_level, logging.INFO))


def configure_logging(
    level: LogLevel = LogLevel.INFO,
    category_levels: dict[LogCategory, LogLevel] | None = None,
    log_file: str | Path | None = None,
    json_format: bool = False,
    stream: TextIO | None = None,
) -> None:
    """Configure unified logging for C and Python code.

    This sets up the logging callback from C to Python and configures
    Python's logging module for unified output.

    Args:
        level: Global minimum log level
        category_levels: Per-category level overrides
            (e.g., ``{LogCategory.EXECUTOR: LogLevel.TRACE}``)
        log_file: Path to log file (None for stderr only)
        json_format: Use JSON output format
        stream: Output stream (defaults to stderr if log_file is None)

    Example::

        # Basic logging to stderr
        configure_logging(level=LogLevel.DEBUG)

        # TRACE only for executor, log to file
        configure_logging(
            level=LogLevel.INFO,
            category_levels={LogCategory.EXECUTOR: LogLevel.TRACE},
            log_file="trace.log"
        )
    """
    lib = _try_get_lib()
    _configure_c_logging(lib, level, category_levels)

    root_logger = logging.getLogger("femu")
    py_level = _LEVEL_MAP.get(level, logging.INFO)
    root_logger.setLevel(py_level)
    root_logger.handlers.clear()

    formatter = _make_formatter(json_format=json_format)
    for handler in _make_handlers(log_file, stream, formatter):
        handler.setLevel(py_level)
        root_logger.addHandler(handler)

    _configure_python_category_levels(category_levels)


def disable_logging() -> None:
    """Disable all logging from C code.

    This is a fast way to disable logging without removing the callback.
    """
    try:
        lib = cffi.get_lib()
        lib.emu_log_set_enabled(False)
    except OSError:
        pass


def enable_logging() -> None:
    """Re-enable logging from C code after disable_logging()."""
    try:
        lib = cffi.get_lib()
        lib.emu_log_set_enabled(True)
    except OSError:
        pass


# =============================================================================
# Module exports
# =============================================================================

__all__ = [
    "TRACE",
    "LogCategory",
    "LogLevel",
    "configure_logging",
    "disable_logging",
    "enable_logging",
    "get_logger",
]
