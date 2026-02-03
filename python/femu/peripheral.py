"""
Peripheral base classes for Python, C, and plugin peripherals.

This module provides the foundation for implementing peripherals in the emulator:

- Peripheral: Base class for Python-implemented peripherals
- CPeripheral: Wrapper for C peripherals compiled into the library
- PluginPeripheral: Peripherals loaded from shared library plugins

Example Python peripheral::

    from femu import Peripheral, PeripheralRegistry

    @PeripheralRegistry.register("my_uart")
    class MyUART(Peripheral):
        def __init__(self, name: str = "uart"):
            super().__init__(name, "my_uart")
            self._data = 0

        def read(self, offset: int, size: int) -> int:
            if offset == 0x04:
                return self._data
            return 0

        def write(self, offset: int, value: int, size: int) -> None:
            if offset == 0x04:
                self._data = value
                print(chr(value & 0xFF), end='')
"""

from __future__ import annotations

import json
from abc import ABC, abstractmethod
from collections.abc import Callable
from pathlib import Path
from typing import TYPE_CHECKING, cast

from . import _emulator_cffi as cffi
from ._cffi_types import CData, PeripheralStruct
from .logging import LogCategory, get_logger

if TYPE_CHECKING:
    from .arch.armv8m import ARMv8MEmulator

logger = get_logger(LogCategory.PERIPHERAL)


# =============================================================================
# Abstract Base
# =============================================================================


class PeripheralBase(ABC):
    """Abstract base for all peripheral types."""

    @property
    @abstractmethod
    def c_struct(self) -> PeripheralStruct:
        """Return the underlying EmuPeripheral C struct pointer."""
        ...

    @property
    @abstractmethod
    def name(self) -> str:
        """Peripheral instance name (e.g., 'USART1')."""
        ...

    @property
    @abstractmethod
    def peripheral_type(self) -> str:
        """Peripheral type identifier (e.g., 'simple_uart')."""
        ...


# =============================================================================
# Python Peripheral Base Class
# =============================================================================


class Peripheral(PeripheralBase):
    """
    Base class for Python-implemented peripherals.

    Subclass this to create peripherals in pure Python. Override the
    read() and write() methods at minimum.

    Example::

        class MyTimer(Peripheral):
            def __init__(self, name: str = "timer", irq: int | None = None):
                super().__init__(name, "my_timer")
                self._irq = irq
                self._counter = 0
                self._reload = 0
                self._enabled = False

            def read(self, offset: int, size: int) -> int:
                if offset == 0x00:
                    return self._counter
                elif offset == 0x04:
                    return self._reload
                return 0

            def write(self, offset: int, value: int, size: int) -> None:
                if offset == 0x00:
                    self._counter = value
                elif offset == 0x04:
                    self._reload = value
                elif offset == 0x08:
                    self._enabled = bool(value & 1)

            def tick(self, cycles: int) -> None:
                if self._enabled and self._counter > 0:
                    self._counter -= 1
                    if self._counter == 0:
                        self._counter = self._reload
                        if self._irq is not None:
                            self.pulse_irq(self._irq)
    """

    # Class-level storage to prevent garbage collection of instances
    # that have been passed to C code
    _instances: dict[int, Peripheral] = {}
    _handles: dict[int, CData] = {}

    def __init__(self, name: str, peripheral_type: str):
        """
        Initialize peripheral.

        Args:
            name: Instance name (e.g., "USART1")
            peripheral_type: Type identifier (e.g., "simple_uart")
        """
        self._name = name
        self._peripheral_type = peripheral_type
        self._ffi = cffi.get_ffi()
        self._irq_callback: Callable[[int, int], None] | None = None
        self._emu: ARMv8MEmulator | None = None

        # C IRQ callback (set by emulator when peripheral is added)
        self._c_irq_callback: CData | None = None
        self._c_emu_ctx: CData | None = None

        # Create persistent handle for C callbacks
        self._handle = self._ffi.new_handle(self)
        Peripheral._instances[id(self)] = self
        Peripheral._handles[id(self)] = self._handle

        # Allocate C struct and string buffers (must keep references)
        self._c_periph = self._ffi.new("EmuPeripheral *")
        self._name_buf = self._ffi.new("char[]", name.encode("utf-8"))
        self._type_buf = self._ffi.new("char[]", peripheral_type.encode("utf-8"))

        # Initialize C struct
        self._c_periph.name = self._name_buf
        self._c_periph.type = self._type_buf
        self._c_periph.context = self._handle

        # Set up vtable with callback functions
        self._c_periph.vtable.read = _py_periph_read
        self._c_periph.vtable.write = _py_periph_write
        self._c_periph.vtable.reset = _py_periph_reset
        self._c_periph.vtable.tick = _py_periph_tick
        self._c_periph.vtable.set_irq_callback = _py_periph_set_irq_callback
        # Don't set destroy - prevent C from freeing Python object
        self._c_periph.vtable.destroy = self._ffi.NULL

    def __del__(self) -> None:
        """Clean up instance tracking."""
        Peripheral._instances.pop(id(self), None)
        Peripheral._handles.pop(id(self), None)

    @property
    def c_struct(self) -> PeripheralStruct:
        """Return the underlying EmuPeripheral C struct pointer."""
        return cast(PeripheralStruct, self._c_periph)

    @property
    def name(self) -> str:
        """Peripheral instance name."""
        return self._name

    @property
    def peripheral_type(self) -> str:
        """Peripheral type identifier."""
        return self._peripheral_type

    # =========================================================================
    # Methods to Override
    # =========================================================================

    @abstractmethod
    def read(self, offset: int, size: int) -> int:
        """
        Read from peripheral register.

        Args:
            offset: Byte offset from peripheral base address
            size: Access size (1, 2, or 4 bytes)

        Returns:
            Register value (will be masked to 32 bits)
        """
        ...

    @abstractmethod
    def write(self, offset: int, value: int, size: int) -> None:
        """
        Write to peripheral register.

        Args:
            offset: Byte offset from peripheral base address
            value: Value to write (masked to size)
            size: Access size (1, 2, or 4 bytes)
        """
        ...

    def reset(self) -> None:
        """
        Reset peripheral to initial state.

        Called on emulator reset. Override to clear registers and state.
        """
        pass

    def tick(self, cycles: int) -> None:
        """
        Advance peripheral time.

        Called after each instruction with the number of cycles elapsed.
        Override for cycle-accurate peripherals like timers.

        Args:
            cycles: Number of CPU cycles since last tick
        """
        pass

    # =========================================================================
    # IRQ Support
    # =========================================================================

    def assert_irq(self, irq: int) -> None:
        """
        Assert an interrupt line (set level to 1).

        Args:
            irq: IRQ number (external interrupt number, not exception number)
        """
        # Use C callback if available (set by emulator when peripheral is added)
        if self._c_irq_callback is not None and self._c_emu_ctx is not None:
            # CFFI function pointer is callable at runtime
            self._c_irq_callback(self._c_emu_ctx, irq, 1)  # type: ignore[operator]
        elif self._irq_callback:
            self._irq_callback(irq, 1)

    def deassert_irq(self, irq: int) -> None:
        """
        Deassert an interrupt line (set level to 0).

        Args:
            irq: IRQ number
        """
        # Use C callback if available
        if self._c_irq_callback is not None and self._c_emu_ctx is not None:
            # CFFI function pointer is callable at runtime
            self._c_irq_callback(self._c_emu_ctx, irq, 0)  # type: ignore[operator]
        elif self._irq_callback:
            self._irq_callback(irq, 0)

    def pulse_irq(self, irq: int) -> None:
        """
        Pulse an interrupt (assert then immediately deassert).

        Useful for edge-triggered interrupts.

        Args:
            irq: IRQ number
        """
        self.assert_irq(irq)
        self.deassert_irq(irq)

    def _set_irq_callback(self, callback: Callable[[int, int], None]) -> None:
        """
        Set the IRQ callback (called by emulator during registration).

        Args:
            callback: Function taking (irq_num, level)
        """
        self._irq_callback = callback


# =============================================================================
# CFFI Callbacks (module-level to ensure they stay alive)
# =============================================================================

_ffi = cffi.get_ffi()


@_ffi.callback("uint32_t(void*, uint32_t, uint8_t)")
def _py_periph_read(ctx: CData, offset: int, size: int) -> int:
    """CFFI callback that routes to Python peripheral read()."""
    try:
        periph: Peripheral = _ffi.from_handle(ctx)
        result = periph.read(offset, size)
        return result & 0xFFFFFFFF
    except Exception as e:
        logger.error("Peripheral read error at offset 0x%x: %s", offset, e)
        return 0


@_ffi.callback("void(void*, uint32_t, uint32_t, uint8_t)")
def _py_periph_write(ctx: CData, offset: int, value: int, size: int) -> None:
    """CFFI callback that routes to Python peripheral write()."""
    try:
        periph: Peripheral = _ffi.from_handle(ctx)
        periph.write(offset, value, size)
    except Exception as e:
        logger.error("Peripheral write error at offset 0x%x: %s", offset, e)


@_ffi.callback("void(void*)")
def _py_periph_reset(ctx: CData) -> None:
    """CFFI callback that routes to Python peripheral reset()."""
    try:
        periph: Peripheral = _ffi.from_handle(ctx)
        periph.reset()
    except Exception as e:
        logger.error("Peripheral reset error: %s", e)


@_ffi.callback("void(void*, uint64_t)")
def _py_periph_tick(ctx: CData, cycles: int) -> None:
    """CFFI callback that routes to Python peripheral tick()."""
    try:
        periph: Peripheral = _ffi.from_handle(ctx)
        periph.tick(cycles)
    except Exception as e:
        logger.error("Peripheral tick error: %s", e)


@_ffi.callback("void(void*, EmuPeriphIRQCallback, void*)")
def _py_periph_set_irq_callback(ctx: CData, callback: CData, emu_ctx: CData) -> None:
    """CFFI callback that receives the IRQ callback from the emulator."""
    try:
        periph: Peripheral = _ffi.from_handle(ctx)
        # Store the callback and context for later use
        periph._c_irq_callback = callback
        periph._c_emu_ctx = emu_ctx
    except Exception as e:
        logger.error("Peripheral set_irq_callback error: %s", e)


# =============================================================================
# C Peripheral Wrapper
# =============================================================================


class CPeripheral(PeripheralBase):
    """
    Wrapper for C-implemented peripherals compiled into the library.

    Use this to access peripherals implemented in C that are part of the
    emulator library (not loaded from plugins).

    Example:
        # Assumes stm32_gpio_create() is exported from the library
        gpio = CPeripheral.from_factory("stm32_gpio", name="GPIOA")
        emu.add_peripheral(gpio, 0x42020000, 0x400)
    """

    def __init__(
        self,
        c_periph_ptr: CData,
        name: str,
        peripheral_type: str,
        keep_alive: list[CData] | None = None,
    ) -> None:
        """
        Initialize C peripheral wrapper.

        Args:
            c_periph_ptr: Pointer to EmuPeripheral struct
            name: Instance name
            peripheral_type: Type identifier
            keep_alive: List of objects to prevent GC
        """
        self._c_periph = c_periph_ptr
        self._name = name
        self._peripheral_type = peripheral_type
        self._keep_alive: list[CData] = keep_alive or []

    @classmethod
    def from_factory(
        cls, factory_name: str, name: str, config: dict[str, str | int | bool] | None = None
    ) -> CPeripheral:
        """
        Create peripheral by calling a C factory function.

        The factory function should have the signature::

            EmuPeripheral* <factory_name>_create(const char *name, const char *config_json);

        Args:
            factory_name: Base name of factory function (e.g., "stm32_gpio")
            name: Instance name (e.g., "GPIOA")
            config: Optional configuration dict (passed as JSON)

        Returns:
            CPeripheral wrapper instance
        """
        ffi = cffi.get_ffi()
        lib = cffi.get_lib()

        # Look for factory function: {name}_create
        factory_fn_name = f"{factory_name}_create"
        if not hasattr(lib, factory_fn_name):
            raise ValueError(f"Factory function not found: {factory_fn_name}")

        factory_fn = getattr(lib, factory_fn_name)

        # Prepare arguments
        config_json = json.dumps(config or {}).encode("utf-8")
        name_bytes = name.encode("utf-8")

        name_buf = ffi.new("char[]", name_bytes)
        config_buf = ffi.new("char[]", config_json)

        # Call factory
        c_periph = factory_fn(name_buf, config_buf)

        if c_periph == ffi.NULL:
            raise RuntimeError(f"Factory {factory_fn_name} returned NULL")

        return cls(c_periph, name, factory_name, keep_alive=[name_buf, config_buf])

    @property
    def c_struct(self) -> PeripheralStruct:
        """Return the underlying EmuPeripheral C struct pointer."""
        return cast(PeripheralStruct, self._c_periph)

    @property
    def name(self) -> str:
        """Peripheral instance name."""
        return self._name

    @property
    def peripheral_type(self) -> str:
        """Peripheral type identifier."""
        return self._peripheral_type


# =============================================================================
# Plugin Peripheral
# =============================================================================


class PluginPeripheral(PeripheralBase):
    """
    Peripheral loaded from a plugin shared library.

    Plugins are .so/.dll/.dylib files that export peripheral factories
    via the emu_plugin_init() entry point.

    Example::

        # Load peripheral from plugin
        periph = PluginPeripheral.from_plugin(
            "./my_plugin.so",
            "custom_adc",
            name="ADC1",
            config={"channels": 16}
        )
        emu.add_peripheral(periph, 0x50000000, 0x100)

        # List available types in a plugin
        types = PluginPeripheral.load_plugin("./my_plugin.so")
        print(types.keys())  # ['custom_adc', 'custom_dac', ...]
    """

    # Cache of loaded plugins: path -> (lib, types_dict, info)
    _loaded_plugins: dict[
        str, tuple[CData, dict[str, dict[str, CData | str | None]], dict[str, str | int]]
    ] = {}

    def __init__(
        self,
        c_periph_ptr: CData,
        destroy_fn: CData | None,
        name: str,
        peripheral_type: str,
        plugin_path: str,
        keep_alive: list[CData] | None = None,
    ) -> None:
        """
        Initialize plugin peripheral.

        Args:
            c_periph_ptr: Pointer to EmuPeripheral struct
            destroy_fn: Plugin's destroy function (may be NULL)
            name: Instance name
            peripheral_type: Type identifier
            plugin_path: Path to source plugin
            keep_alive: Objects to prevent GC
        """
        self._c_periph = c_periph_ptr
        self._destroy_fn = destroy_fn
        self._name = name
        self._peripheral_type = peripheral_type
        self._plugin_path = plugin_path
        self._keep_alive: list[CData] = keep_alive or []
        self._destroyed = False

    def __del__(self) -> None:
        """Call plugin's destroy function if provided."""
        if self._destroyed:
            return

        ffi = cffi.get_ffi()
        if (
            self._destroy_fn
            and self._destroy_fn != ffi.NULL
            and self._c_periph
            and self._c_periph != ffi.NULL
        ):
            try:
                # CFFI function pointer is callable at runtime
                self._destroy_fn(self._c_periph)  # type: ignore[operator]
            except Exception:
                pass  # Ignore errors during cleanup
            self._destroyed = True

    @classmethod
    def load_plugin(cls, plugin_path: str | Path) -> dict[str, dict[str, CData | str | None]]:
        """
        Load a plugin and return its peripheral types.

        Args:
            plugin_path: Path to .so/.dll/.dylib file

        Returns:
            Dict mapping type names to type info dicts with keys:
            - 'create': Factory function
            - 'destroy': Destroy function (may be None)
            - 'description': Human-readable description
        """
        path_str = str(Path(plugin_path).resolve())

        if path_str in cls._loaded_plugins:
            return cls._loaded_plugins[path_str][1]

        ffi = cffi.get_ffi()

        # Load the shared library
        try:
            lib = ffi.dlopen(path_str)
        except OSError as e:
            raise RuntimeError(f"Failed to load plugin {path_str}: {e}") from e

        # Get the init function
        try:
            init_fn = lib.emu_plugin_init
        except AttributeError as e:
            raise RuntimeError(
                f"Plugin {path_str} missing 'emu_plugin_init' symbol. "
                "Ensure the function is exported with EMU_PLUGIN_EXPORT."
            ) from e

        # Call init to get peripheral types
        info_ptr = ffi.new("EmuPluginInfo **")
        types_ptr = init_fn(info_ptr)

        if types_ptr == ffi.NULL:
            raise RuntimeError(f"Plugin {path_str} emu_plugin_init returned NULL")

        # Check API version
        plugin_info = {}
        if info_ptr[0] != ffi.NULL:
            info = info_ptr[0]
            if info.api_version != cffi.EMU_PLUGIN_API_VERSION:
                raise RuntimeError(
                    f"Plugin {path_str} has incompatible API version "
                    f"{info.api_version} (expected {cffi.EMU_PLUGIN_API_VERSION})"
                )
            plugin_info = {
                "api_version": info.api_version,
                "name": ffi.string(info.name).decode("utf-8") if info.name else "",
                "version": ffi.string(info.version).decode("utf-8") if info.version else "",
                "author": ffi.string(info.author).decode("utf-8") if info.author else "",
                "description": (
                    ffi.string(info.description).decode("utf-8") if info.description else ""
                ),
            }

        # Extract peripheral types (NULL-terminated array)
        types_dict = {}
        i = 0
        while types_ptr[i].type_name != ffi.NULL:
            type_def = types_ptr[i]
            type_name = ffi.string(type_def.type_name).decode("utf-8")
            types_dict[type_name] = {
                "create": type_def.create,
                "destroy": type_def.destroy if type_def.destroy != ffi.NULL else None,
                "description": (
                    ffi.string(type_def.description).decode("utf-8")
                    if type_def.description and type_def.description != ffi.NULL
                    else ""
                ),
            }
            i += 1

        cls._loaded_plugins[path_str] = (lib, types_dict, plugin_info)
        return types_dict

    @classmethod
    def from_plugin(
        cls,
        plugin_path: str | Path,
        type_name: str,
        name: str,
        config: dict[str, str | int | bool] | None = None,
    ) -> PluginPeripheral:
        """
        Create a peripheral from a plugin.

        Args:
            plugin_path: Path to plugin .so/.dll/.dylib
            type_name: Peripheral type name registered by plugin
            name: Instance name (e.g., "ADC1")
            config: Optional configuration dict (passed as JSON to factory)

        Returns:
            PluginPeripheral instance
        """
        path_str = str(Path(plugin_path).resolve())
        types = cls.load_plugin(path_str)

        if type_name not in types:
            available = ", ".join(types.keys()) or "(none)"
            raise ValueError(
                f"Plugin {plugin_path} does not provide type '{type_name}'. "
                f"Available types: {available}"
            )

        type_def = types[type_name]
        ffi = cffi.get_ffi()

        # Call create function
        name_buf = ffi.new("char[]", name.encode("utf-8"))
        config_buf = ffi.new("char[]", json.dumps(config or {}).encode("utf-8"))

        # CFFI function pointer is callable at runtime
        create_fn = type_def["create"]
        c_periph = create_fn(name_buf, config_buf)  # type: ignore[misc, operator]

        if c_periph == ffi.NULL:
            raise RuntimeError(f"Plugin factory for '{type_name}' returned NULL")

        # destroy is CData | None (not str)
        destroy_fn = cast("CData | None", type_def["destroy"])
        return cls(
            c_periph,
            destroy_fn,
            name,
            type_name,
            path_str,
            keep_alive=[name_buf, config_buf],
        )

    @classmethod
    def get_plugin_info(cls, plugin_path: str | Path) -> dict[str, str | int | list[str]]:
        """
        Get metadata about a loaded plugin.

        Args:
            plugin_path: Path to plugin file

        Returns:
            Dict with plugin info and list of peripheral types
        """
        path_str = str(Path(plugin_path).resolve())

        if path_str not in cls._loaded_plugins:
            cls.load_plugin(path_str)

        _, types, info = cls._loaded_plugins[path_str]

        result: dict[str, str | int | list[str]] = dict(info)
        result["peripheral_types"] = list(types.keys())
        result["path"] = path_str
        return result

    @classmethod
    def unload_plugin(cls, plugin_path: str | Path) -> None:
        """
        Remove a plugin from the cache.

        Note: This doesn't truly unload the library (dlclose), but removes
        it from our tracking. Any existing PluginPeripheral instances from
        this plugin will continue to work.

        Args:
            plugin_path: Path to plugin file
        """
        path_str = str(Path(plugin_path).resolve())
        cls._loaded_plugins.pop(path_str, None)

    @property
    def c_struct(self) -> PeripheralStruct:
        """Return the underlying EmuPeripheral C struct pointer."""
        return cast(PeripheralStruct, self._c_periph)

    @property
    def name(self) -> str:
        """Peripheral instance name."""
        return self._name

    @property
    def peripheral_type(self) -> str:
        """Peripheral type identifier."""
        return self._peripheral_type

    @property
    def plugin_path(self) -> str:
        """Path to the source plugin."""
        return self._plugin_path


# =============================================================================
# Exports
# =============================================================================

__all__ = [
    "CData",
    "CPeripheral",
    "Peripheral",
    "PeripheralBase",
    "PeripheralStruct",
    "PluginPeripheral",
]
