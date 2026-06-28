# Peripherals

FEMU includes peripheral emulation for common hardware devices like UARTs and GPIOs. Peripherals are memory-mapped and interact with firmware through register reads and writes.

## Built-in Peripherals

### UART (simple_uart)

A basic UART peripheral that captures firmware output and can inject input.

**Configuration:**

```yaml
peripherals:
  - type: simple_uart
    name: USART1
    base: 0x40013800
    size: 0x400
    config:
      irq: 37        # IRQ number (-1 to disable)
      echo: true     # Print TX to console
      max_buffer: 4096  # Buffer size
```

**Registers:**

| Offset | Name   | Access | Description                  |
| ------ | ------ | ------ | ---------------------------- |
| 0x00   | DATA   | R/W    | Data register                |
| 0x04   | STATUS | R      | Status (RXNE, TXE bits)      |
| 0x08   | CTRL   | R/W    | Control (EN, TE, RE bits)    |

**Status bits:**

- Bit 0 (RXNE): Receive buffer not empty
- Bit 1 (TXE): Transmit buffer empty (always 1)

**Control bits:**

- Bit 0 (EN): UART enable
- Bit 3 (TE): Transmitter enable
- Bit 4 (RE): Receiver enable

**Python API:**

```python
# Get UART peripheral
uart = machine.get_peripheral("USART1")
# or
uart = machine["USART1"]

# Get transmitted output
output = uart.get_output()       # As string
output = uart.get_output_bytes() # As bytes

# Inject input (simulates receive)
uart.inject_input("hello\n")
uart.inject_input(b"\x01\x02\x03")

# Clear output buffer
uart.clear_output()

# Check state
if uart.has_input:
    print("Data waiting in RX buffer")
print(f"TX buffer has {uart.output_length} bytes")
```

### GPIO (simple_gpio)

A 16-pin GPIO port with configurable modes.

**Configuration:**

```yaml
peripherals:
  - type: simple_gpio
    name: GPIOA
    base: 0x48000000
    size: 0x400
    config:
      num_pins: 16   # Number of pins (max 16)
      irq: -1        # IRQ number (-1 to disable)
```

**Registers:**

| Offset | Name   | Access | Description                     |
| ------ | ------ | ------ | ------------------------------- |
| 0x00   | MODER  | R/W    | Mode register (2 bits per pin)  |
| 0x04   | OTYPER | R/W    | Output type register            |
| 0x08   | OSPEED | R/W    | Output speed register           |
| 0x0C   | PUPDR  | R/W    | Pull-up/pull-down register      |
| 0x10   | IDR    | R      | Input data register             |
| 0x14   | ODR    | R/W    | Output data register            |
| 0x18   | BSRR   | W      | Bit set/reset register          |

**Pin modes (2 bits per pin in MODER):**

- 00: Input
- 01: Output
- 10: Alternate function
- 11: Analog

**Python API:**

```python
# Get GPIO peripheral
gpio = machine.get_peripheral("GPIOA")

# Simulate external input (button press, sensor, etc.)
gpio.set_input_pin(0, True)   # Pin 0 high
gpio.set_input_pin(0, False)  # Pin 0 low

# Read output state (what firmware set)
led_on = gpio.get_output_pin(5)

# Get pin mode
mode = gpio.get_pin_mode(0)  # 0=input, 1=output, 2=altfunc, 3=analog

# Get all pins at once
output_bits = gpio.output_state  # 16-bit value
input_bits = gpio.input_state    # 16-bit value

# Register callback for output changes
def on_gpio_change(pin, state):
    print(f"Pin {pin} changed to {state}")

gpio.on_output_change(on_gpio_change)
```

## Adding Peripherals

### Via YAML Configuration

```yaml
machine:
  name: my_board
  arch: armv8m

memory:
  - type: flash
    base: 0x00000000
    size: 512K
  - type: ram
    base: 0x20000000
    size: 256K

peripherals:
  - type: simple_uart
    name: USART1
    base: 0x40013800
    size: 0x400
    config:
      irq: 37
      echo: true
  - type: simple_gpio
    name: GPIOA
    base: 0x48000000
    size: 0x400
```

### Via Python API

```python
from femu import Machine
from femu.peripherals import SimpleUART, SimpleGPIO

# Create machine
machine = Machine.from_dict({
    "machine": {"name": "test", "arch": "armv8m"},
    "memory": [
        {"type": "flash", "base": 0x00000000, "size": "512K"},
        {"type": "ram", "base": 0x20000000, "size": "256K"},
    ],
})

# Create and add peripherals
uart = SimpleUART(name="USART1", irq=37, echo=True)
machine.emu.add_peripheral(uart, 0x40013800, 0x400)

gpio = SimpleGPIO(name="GPIOA")
machine.emu.add_peripheral(gpio, 0x48000000, 0x400)
```

### Using the Registry

The peripheral registry provides a unified way to create peripherals:

```python
from femu import PeripheralRegistry

# Create by type name
uart = PeripheralRegistry.create("simple_uart", "USART1", irq=37)
gpio = PeripheralRegistry.create("simple_gpio", "GPIOA")

# List available types
for info in PeripheralRegistry.list_types():
    print(f"{info.name}: {info.description} ({info.source})")
```

## Firmware Integration

### UART Example

Firmware code to write to a UART:

```c
#define UART_BASE 0x40013800
#define UART_DATA   (*(volatile uint32_t *)(UART_BASE + 0x00))
#define UART_STATUS (*(volatile uint32_t *)(UART_BASE + 0x04))
#define UART_CTRL   (*(volatile uint32_t *)(UART_BASE + 0x08))

#define CTRL_EN  (1 << 0)
#define CTRL_TE  (1 << 3)
#define STATUS_TXE (1 << 1)

void uart_init(void) {
    UART_CTRL = CTRL_EN | CTRL_TE;  // Enable UART and transmitter
}

void uart_putc(char c) {
    while (!(UART_STATUS & STATUS_TXE));  // Wait for TX ready
    UART_DATA = c;
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}
```

### GPIO Example

Firmware code to use GPIO:

```c
#define GPIO_BASE 0x48000000
#define GPIO_MODER (*(volatile uint32_t *)(GPIO_BASE + 0x00))
#define GPIO_IDR   (*(volatile uint32_t *)(GPIO_BASE + 0x10))
#define GPIO_ODR   (*(volatile uint32_t *)(GPIO_BASE + 0x14))
#define GPIO_BSRR  (*(volatile uint32_t *)(GPIO_BASE + 0x18))

void gpio_init(void) {
    // Set pin 5 as output (bits [11:10] = 01)
    GPIO_MODER = (GPIO_MODER & ~(0x3 << 10)) | (0x1 << 10);
}

void led_on(void) {
    GPIO_BSRR = (1 << 5);  // Set pin 5
}

void led_off(void) {
    GPIO_BSRR = (1 << 21);  // Reset pin 5 (bit 5 + 16)
}

int button_pressed(void) {
    return (GPIO_IDR & (1 << 0)) != 0;  // Read pin 0
}
```

## Testing with Peripherals

### Capturing UART Output

```python
from femu import Machine

machine = Machine.from_yaml("boards/my_board.yaml")
machine.load_elf("firmware.elf")

# Run firmware
machine.run(max_cycles=1000000)

# Check output
uart = machine["USART1"]
output = uart.get_output()
assert "Hello, World!" in output
```

### Simulating GPIO Input

```python
from femu import Machine

machine = Machine.from_yaml("boards/my_board.yaml")
machine.load_elf("firmware.elf")

gpio = machine["GPIOA"]

# Simulate button press on pin 0
gpio.set_input_pin(0, True)

# Run firmware to handle the input
machine.run(max_cycles=10000)

# Check that LED (pin 5) turned on
assert gpio.get_output_pin(5) == True

# Release button
gpio.set_input_pin(0, False)
machine.run(max_cycles=10000)
```

### Interactive Testing

```python
from femu import Machine
import time

machine = Machine.from_yaml("boards/my_board.yaml")
machine.load_elf("firmware.elf")

uart = machine["USART1"]
gpio = machine["GPIOA"]

# Register for GPIO changes
def on_led_change(pin, state):
    if pin == 5:
        print(f"LED {'ON' if state else 'OFF'}")

gpio.on_output_change(on_led_change)

# Interactive loop
while True:
    # Run some cycles
    machine.run(max_cycles=10000)

    # Check for UART output
    if uart.output_length > 0:
        print(uart.get_output(), end="")
        uart.clear_output()

    # Inject command via UART
    uart.inject_input("status\n")
```

## Plugin Peripherals

FEMU can load additional peripherals from plugin libraries:

```python
from femu import PluginPeripheral, PeripheralRegistry

# Load plugin
types = PluginPeripheral.load_plugin("./vendor_peripherals.so")
print(f"Available types: {list(types.keys())}")

# Create peripheral from plugin
adc = PluginPeripheral.from_plugin(
    "./vendor_peripherals.so",
    "custom_adc",
    name="ADC1",
    config={"channels": 8}
)
machine.emu.add_peripheral(adc, 0x50000000, 0x100)

# Or register plugin types globally
PeripheralRegistry.load_plugin("./vendor_peripherals.so")
adc = PeripheralRegistry.create("custom_adc", "ADC1", channels=8)
```

From YAML:

```yaml
plugins:
  - ./vendor_peripherals.so

peripherals:
  - type: custom_adc
    name: ADC1
    base: 0x50000000
    size: 0x100
    config:
      channels: 8
```

## Next Steps

- [Machine Configuration](machine-configuration.md) - Full configuration format
- [Running Firmware](running-firmware.md) - Firmware requirements
- See [Writing Peripherals](../developer/writing-peripherals.md) for creating custom peripherals
