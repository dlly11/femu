"""
Peripheral type registry supporting Python, C, and plugin peripherals.

The PeripheralRegistry provides a unified factory for creating peripherals
regardless of their implementation (Python, C, or plugin).

Example:
    from femu import Peripheral, PeripheralRegistry

    # Register a Python peripheral using decorator
    @PeripheralRegistry.register("my_uart")
    class MyUART(Peripheral):
        def __init__(self, name: str = "uart", baud: int = 9600):
            super().__init__(name, "my_uart")
            self._baud = baud
        ...

    # Register a C peripheral factory
    PeripheralRegistry.register_c("stm32_gpio", "stm32_gpio_create")

    # Load plugin peripherals
    PeripheralRegistry.load_plugin("./vendor_peripherals.so")

    # Create any peripheral by type name
    uart = PeripheralRegistry.create("my_uart", name="USART1", config={"baud": 115200})
    gpio = PeripheralRegistry.create("stm32_gpio", name="GPIOA")
    adc = PeripheralRegistry.create("custom_adc", name="ADC1")  # From plugin
"""

from __future__ import annotations

import platform
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path

from .peripheral import CPeripheral, Peripheral, PeripheralBase, PluginPeripheral


@dataclass
class PeripheralTypeInfo:
    """Information about a registered peripheral type."""

    name: str
    source: str  # "python", "c", or "plugin"
    description: str
    plugin_path: str | None = None


class PeripheralRegistry:
    """
    Central registry for peripheral types.

    Supports three kinds of peripherals:
    - Python: Registered via @register decorator
    - C: Built into the emulator library, registered via register_c()
    - Plugin: Loaded from shared libraries via load_plugin()

    All types can be created uniformly via create().
    """

    # Python peripheral classes: type_name -> class
    _python_types: dict[str, type[Peripheral]] = {}

    # C peripheral factory names: type_name -> factory_name
    _c_types: dict[str, str] = {}

    # Plugin peripheral types: type_name -> (plugin_path, type_name_in_plugin)
    _plugin_types: dict[str, tuple[str, str]] = {}

    # Descriptions for all types
    _descriptions: dict[str, str] = {}

    # ==========================================================================
    # Python Peripherals
    # ==========================================================================

    @classmethod
    def register(
        cls, name: str, description: str = ""
    ) -> Callable[[type[Peripheral]], type[Peripheral]]:
        """
        Decorator to register a Python peripheral class.

        Args:
            name: Type name for the registry (used in YAML configs)
            description: Human-readable description

        Example:
            @PeripheralRegistry.register("simple_uart", "Simple UART with TX/RX")
            class SimpleUART(Peripheral):
                ...
        """

        def decorator(periph_class: type[Peripheral]) -> type[Peripheral]:
            if not issubclass(periph_class, Peripheral):
                raise TypeError(f"{periph_class.__name__} must be a subclass of Peripheral")

            if name in cls._python_types:
                raise ValueError(f"Peripheral type '{name}' is already registered")

            cls._python_types[name] = periph_class
            cls._descriptions[name] = description or periph_class.__doc__ or ""
            return periph_class

        return decorator

    @classmethod
    def register_python(
        cls, name: str, periph_class: type[Peripheral], description: str = ""
    ) -> None:
        """
        Register a Python peripheral class (non-decorator version).

        Args:
            name: Type name for the registry
            periph_class: Peripheral class to register
            description: Human-readable description
        """
        if not issubclass(periph_class, Peripheral):
            raise TypeError(f"{periph_class.__name__} must be a subclass of Peripheral")

        if name in cls._python_types:
            raise ValueError(f"Peripheral type '{name}' is already registered")

        cls._python_types[name] = periph_class
        cls._descriptions[name] = description or periph_class.__doc__ or ""

    # ==========================================================================
    # C Peripherals
    # ==========================================================================

    @classmethod
    def register_c(cls, type_name: str, factory_name: str, description: str = "") -> None:
        """
        Register a C peripheral factory function.

        The factory function should be exported from the emulator library
        with the signature:
            EmuPeripheral* {factory_name}_create(const char *name, const char *config_json);

        Args:
            type_name: Type name for the registry
            factory_name: Base name of factory function (without _create suffix)
            description: Human-readable description
        """
        if cls.has_type(type_name):
            raise ValueError(f"Peripheral type '{type_name}' is already registered")

        cls._c_types[type_name] = factory_name
        cls._descriptions[type_name] = description

    # ==========================================================================
    # Plugin Peripherals
    # ==========================================================================

    @classmethod
    def load_plugin(cls, plugin_path: str | Path, prefix: str = "") -> list[str]:
        """
        Load peripheral types from a plugin shared library.

        Args:
            plugin_path: Path to .so/.dll/.dylib file
            prefix: Optional prefix for type names (to avoid conflicts)

        Returns:
            List of registered type names

        Example:
            types = PeripheralRegistry.load_plugin("./vendor.so", prefix="vendor_")
            # Creates types like "vendor_uart", "vendor_gpio", etc.
        """
        path_str = str(Path(plugin_path).resolve())
        types = PluginPeripheral.load_plugin(path_str)

        registered = []
        for type_name, type_def in types.items():
            full_name = f"{prefix}{type_name}" if prefix else type_name

            # Check for conflicts
            if cls.has_type(full_name):
                raise ValueError(
                    f"Type name conflict: '{full_name}' is already registered. "
                    f"Use prefix parameter to avoid conflicts."
                )

            cls._plugin_types[full_name] = (path_str, type_name)
            # description is always a string in practice
            cls._descriptions[full_name] = str(type_def.get("description", ""))
            registered.append(full_name)

        return registered

    @classmethod
    def load_plugins_from_directory(
        cls, directory: str | Path, pattern: str | None = None
    ) -> dict[str, list[str]]:
        """
        Load all plugins from a directory.

        Args:
            directory: Directory to scan
            pattern: Glob pattern for plugin files (auto-detected if None)

        Returns:
            Dict mapping plugin paths to list of registered types
        """
        # Auto-detect pattern based on platform
        if pattern is None:
            system = platform.system()
            if system == "Darwin":
                pattern = "*.dylib"
            elif system == "Windows":
                pattern = "*.dll"
            else:
                pattern = "*.so"

        results = {}
        dir_path = Path(directory)

        if not dir_path.is_dir():
            raise ValueError(f"Not a directory: {directory}")

        for plugin_file in dir_path.glob(pattern):
            try:
                types = cls.load_plugin(plugin_file)
                results[str(plugin_file)] = types
            except Exception as e:
                # Log warning but continue loading other plugins
                print(f"Warning: Failed to load plugin {plugin_file}: {e}")

        return results

    # ==========================================================================
    # Factory
    # ==========================================================================

    @classmethod
    def create(
        cls,
        type_name: str,
        name: str,
        config: dict[str, str | int | bool] | None = None,
        **kwargs: str | int | bool,
    ) -> PeripheralBase:
        """
        Create a peripheral instance by type name.

        Args:
            type_name: Registered type name
            name: Instance name (e.g., "USART1")
            config: Configuration dict (merged with kwargs for Python peripherals,
                    passed as JSON for C/plugin peripherals)
            **kwargs: Additional keyword arguments (for Python peripherals only)

        Returns:
            Peripheral instance

        Example:
            uart = PeripheralRegistry.create("simple_uart", "USART1", irq=37)
            gpio = PeripheralRegistry.create("stm32_gpio", "GPIOA", config={"port": "A"})
        """
        # Merge config dict with kwargs for Python peripherals
        merged_config = dict(config or {})
        merged_config.update(kwargs)

        # Check Python types first (most common case)
        if type_name in cls._python_types:
            periph_class = cls._python_types[type_name]
            # Config dict is passed as kwargs to peripheral constructor
            return periph_class(name=name, **merged_config)  # type: ignore[arg-type]

        # Check C types
        if type_name in cls._c_types:
            factory_name = cls._c_types[type_name]
            return CPeripheral.from_factory(factory_name, name, merged_config)

        # Check plugin types
        if type_name in cls._plugin_types:
            plugin_path, plugin_type_name = cls._plugin_types[type_name]
            return PluginPeripheral.from_plugin(plugin_path, plugin_type_name, name, merged_config)

        # Not found
        raise ValueError(
            f"Unknown peripheral type: '{type_name}'. "
            f"Use PeripheralRegistry.list_types() to see available types."
        )

    # ==========================================================================
    # Introspection
    # ==========================================================================

    @classmethod
    def list_types(cls) -> list[PeripheralTypeInfo]:
        """
        List all registered peripheral types.

        Returns:
            List of PeripheralTypeInfo objects
        """
        result = []

        for name in sorted(cls._python_types.keys()):
            result.append(
                PeripheralTypeInfo(
                    name=name, source="python", description=cls._descriptions.get(name, "")
                )
            )

        for name in sorted(cls._c_types.keys()):
            result.append(
                PeripheralTypeInfo(
                    name=name, source="c", description=cls._descriptions.get(name, "")
                )
            )

        for name in sorted(cls._plugin_types.keys()):
            path, _ = cls._plugin_types[name]
            result.append(
                PeripheralTypeInfo(
                    name=name,
                    source="plugin",
                    description=cls._descriptions.get(name, ""),
                    plugin_path=path,
                )
            )

        return result

    @classmethod
    def has_type(cls, type_name: str) -> bool:
        """
        Check if a type is registered.

        Args:
            type_name: Type name to check

        Returns:
            True if registered, False otherwise
        """
        return (
            type_name in cls._python_types
            or type_name in cls._c_types
            or type_name in cls._plugin_types
        )

    @classmethod
    def get_type_info(cls, type_name: str) -> PeripheralTypeInfo | None:
        """
        Get information about a specific type.

        Args:
            type_name: Type name to look up

        Returns:
            PeripheralTypeInfo or None if not found
        """
        if type_name in cls._python_types:
            return PeripheralTypeInfo(
                name=type_name, source="python", description=cls._descriptions.get(type_name, "")
            )

        if type_name in cls._c_types:
            return PeripheralTypeInfo(
                name=type_name, source="c", description=cls._descriptions.get(type_name, "")
            )

        if type_name in cls._plugin_types:
            path, _ = cls._plugin_types[type_name]
            return PeripheralTypeInfo(
                name=type_name,
                source="plugin",
                description=cls._descriptions.get(type_name, ""),
                plugin_path=path,
            )

        return None

    @classmethod
    def get_type_source(cls, type_name: str) -> str | None:
        """
        Get the source of a peripheral type.

        Args:
            type_name: Type name to look up

        Returns:
            "python", "c", "plugin", or None if not found
        """
        if type_name in cls._python_types:
            return "python"
        if type_name in cls._c_types:
            return "c"
        if type_name in cls._plugin_types:
            return "plugin"
        return None

    # ==========================================================================
    # Management
    # ==========================================================================

    @classmethod
    def unregister(cls, type_name: str) -> bool:
        """
        Unregister a peripheral type.

        Args:
            type_name: Type name to unregister

        Returns:
            True if type was found and removed, False otherwise
        """
        removed = False

        if type_name in cls._python_types:
            del cls._python_types[type_name]
            removed = True

        if type_name in cls._c_types:
            del cls._c_types[type_name]
            removed = True

        if type_name in cls._plugin_types:
            del cls._plugin_types[type_name]
            removed = True

        if type_name in cls._descriptions:
            del cls._descriptions[type_name]

        return removed

    @classmethod
    def clear(cls) -> None:
        """
        Clear all registered types.

        Mainly useful for testing.
        """
        cls._python_types.clear()
        cls._c_types.clear()
        cls._plugin_types.clear()
        cls._descriptions.clear()


# =============================================================================
# Exports
# =============================================================================

__all__ = [
    "PeripheralRegistry",
    "PeripheralTypeInfo",
]
