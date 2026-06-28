"""Unit tests for the Peripheral base class (femu.peripheral).

Covers the abstract interface, property accessors, and documents the
class-level instance registry lifecycle (instances are intentionally retained
for C-handle safety). No compiled library is required: creating a peripheral
only allocates CFFI buffers, it does not dlopen the emulator.
"""

from __future__ import annotations

import gc
from collections.abc import Iterator

import pytest
from femu.peripheral import Peripheral


class DummyPeripheral(Peripheral):
    """Minimal concrete peripheral for exercising the base class."""

    def __init__(self, name: str = "dummy") -> None:
        super().__init__(name, "dummy_type")
        self.value = 0

    def read(self, offset: int, size: int) -> int:
        return self.value

    def write(self, offset: int, value: int, size: int) -> None:
        self.value = value


@pytest.fixture
def clean_registry() -> Iterator[None]:
    """Snapshot and restore the class-level registries for test isolation."""
    instances = dict(Peripheral._instances)
    handles = dict(Peripheral._handles)
    try:
        yield
    finally:
        Peripheral._instances.clear()
        Peripheral._instances.update(instances)
        Peripheral._handles.clear()
        Peripheral._handles.update(handles)


class TestAbstractInterface:
    def test_cannot_instantiate_base_directly(self) -> None:
        with pytest.raises(TypeError):
            Peripheral("x", "y")  # type: ignore[abstract]

    def test_properties(self, clean_registry: None) -> None:
        p = DummyPeripheral("USART1")
        assert p.name == "USART1"
        assert p.peripheral_type == "dummy_type"

    def test_read_write_dispatch(self, clean_registry: None) -> None:
        p = DummyPeripheral()
        p.write(0, 0x42, 4)
        assert p.read(0, 4) == 0x42

    def test_reset_and_tick_default_noops(self, clean_registry: None) -> None:
        p = DummyPeripheral()
        # Base implementations should not raise.
        p.reset()
        p.tick(100)


class TestInstanceRegistry:
    def test_creation_registers_instance(self, clean_registry: None) -> None:
        before = len(Peripheral._instances)
        p = DummyPeripheral()
        assert len(Peripheral._instances) == before + 1
        assert id(p) in Peripheral._instances
        assert id(p) in Peripheral._handles

    def test_registry_retains_instance_after_del(self, clean_registry: None) -> None:
        # Documents the intentional leak-by-design: the strong reference in
        # _instances keeps the object alive, so it survives `del` + GC.
        p = DummyPeripheral()
        key = id(p)
        del p
        gc.collect()
        assert key in Peripheral._instances
