"""
Unified logging system for FEMU.

This module provides a logging bridge between C code and Python's logging module,
enabling unified output with configurable verbosity.

Usage:
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
from datetime import datetime
from enum import IntEnum
from pathlib import Path
from typing import TYPE_CHECKING, TextIO

from . import _emulator_cffi as cffi

if TYPE_CHECKING:
    pass

# =============================================================================
# Custom TRACE log level
# =============================================================================

# Add TRACE level below DEBUG (standard DEBUG is 10)
TRACE = 5
logging.addLevelName(TRACE, "TRACE")


def _trace(self, message, *args, **kwargs):
    """Log at TRACE level."""
    if self.isEnabledFor(TRACE):
        self._log(TRACE, message, args, **kwargs)


# Add trace method to Logger class
logging.Logger.trace = _trace


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
_LEVEL_MAP = {
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
_CATEGORY_NAMES = {
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
            "timestamp": datetime.utcnow().isoformat() + "Z",
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
def _c_log_callback(ctx, level, category, file_ptr, line, func_ptr, msg_ptr):
    """
    C log callback that routes messages to Python logging.

    This function is called from C code through the EmuLogCallback mechanism.
    """
    # Get string values from C pointers
    try:
        filename = _ffi.string(file_ptr).decode("utf-8") if file_ptr else "unknown"
        func = _ffi.string(func_ptr).decode("utf-8") if func_ptr else "unknown"
        message = _ffi.string(msg_ptr).decode("utf-8") if msg_ptr else ""
    except Exception:
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
    """
    Get a logger for a specific category.

    Args:
        category: The log category

    Returns:
        A Python logger instance configured for this category
    """
    cat_name = _CATEGORY_NAMES.get(category, "unknown")
    return logging.getLogger(f"femu.{cat_name}")


def configure_logging(
    level: LogLevel = LogLevel.INFO,
    category_levels: dict[LogCategory, LogLevel] | None = None,
    log_file: str | Path | None = None,
    json_format: bool = False,
    stream: TextIO | None = None,
) -> None:
    """
    Configure unified logging for C and Python code.

    This sets up the logging callback from C to Python and configures
    Python's logging module for unified output.

    Args:
        level: Global minimum log level
        category_levels: Per-category level overrides (e.g., {LogCategory.EXECUTOR: LogLevel.TRACE})
        log_file: Path to log file (None for stderr only)
        json_format: Use JSON output format
        stream: Output stream (defaults to stderr if log_file is None)

    Example:
        # Basic logging to stderr
        configure_logging(level=LogLevel.DEBUG)

        # TRACE only for executor, log to file
        configure_logging(
            level=LogLevel.INFO,
            category_levels={LogCategory.EXECUTOR: LogLevel.TRACE},
            log_file="trace.log"
        )
    """
    global _callback_handle, _log_file_handle

    # Get the library
    try:
        lib = cffi.get_lib()
    except OSError:
        # Library not available - configure Python logging only
        lib = None

    # Set up the C callback
    if lib is not None:
        _callback_handle = _c_log_callback  # Keep reference
        lib.emu_log_set_callback(_c_log_callback, _ffi.NULL)
        lib.emu_log_set_level(int(level))
        lib.emu_log_set_enabled(True)

        # Set per-category levels
        if category_levels:
            for cat, cat_level in category_levels.items():
                lib.emu_log_set_category_level(int(cat), int(cat_level))

    # Configure Python logging
    # Get the root femu logger
    root_logger = logging.getLogger("femu")

    # Map LogLevel to Python level
    py_level = _LEVEL_MAP.get(level, logging.INFO)
    root_logger.setLevel(py_level)

    # Clear existing handlers
    root_logger.handlers.clear()

    # Create formatter
    if json_format:
        formatter = JSONFormatter()
    else:
        # Default format with timestamp, logger name, level, file, line, and message
        fmt = "%(asctime)s.%(msecs)03d [%(name)s] %(levelname)-5s "
        fmt += "%(filename)s:%(lineno)d: %(message)s"
        formatter = logging.Formatter(fmt, datefmt="%H:%M:%S")

    # Add handlers
    handlers: list[logging.Handler] = []

    # Stream handler (stderr or custom stream)
    if log_file is None or stream is not None:
        stream_handler = logging.StreamHandler(stream or sys.stderr)
        stream_handler.setFormatter(formatter)
        handlers.append(stream_handler)

    # File handler
    if log_file is not None:
        # Close previous file handle if any
        if _log_file_handle is not None:
            _log_file_handle.close()
            _log_file_handle = None

        file_handler = logging.FileHandler(str(log_file), mode="w")
        file_handler.setFormatter(formatter)
        handlers.append(file_handler)

    # Add handlers to root logger
    for handler in handlers:
        handler.setLevel(py_level)
        root_logger.addHandler(handler)

    # Configure per-category loggers
    if category_levels:
        for cat, cat_level in category_levels.items():
            cat_name = _CATEGORY_NAMES.get(cat, "unknown")
            cat_logger = logging.getLogger(f"femu.{cat_name}")
            cat_py_level = _LEVEL_MAP.get(cat_level, logging.INFO)
            cat_logger.setLevel(cat_py_level)


def disable_logging() -> None:
    """
    Disable all logging from C code.

    This is a fast way to disable logging without removing the callback.
    """
    try:
        lib = cffi.get_lib()
        lib.emu_log_set_enabled(False)
    except OSError:
        pass


def enable_logging() -> None:
    """
    Re-enable logging from C code after disable_logging().
    """
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
    "LogLevel",
    "LogCategory",
    "get_logger",
    "configure_logging",
    "disable_logging",
    "enable_logging",
]
