/**
 * @file emu_peripheral.h
 * @brief Unified peripheral ABI for C, Rust, and Python peripherals
 *
 * This file defines the peripheral interface using a vtable pattern.
 * All peripherals (regardless of implementation language) use this interface.
 *
 * - C peripherals: implement the vtable functions directly
 * - Rust peripherals: use #[no_mangle] extern "C" functions
 * - Python peripherals: cffi wraps Python methods to match this ABI
 *
 * This allows hot-swapping peripheral implementations without changing
 * the emulator core.
 */

#ifndef EMU_PERIPHERAL_H
#define EMU_PERIPHERAL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Forward Declarations
 *============================================================================*/

typedef struct EmuPeripheral EmuPeripheral;
typedef struct EmuPeripheralVTable EmuPeripheralVTable;

/*============================================================================
 * Callback Types
 *============================================================================*/

/**
 * IRQ callback - peripheral calls this to assert/deassert an interrupt.
 *
 * @param emu_ctx   Emulator context (opaque, passed back from registration)
 * @param irq       IRQ number (0-based external interrupt number)
 * @param level     1 = assert, 0 = deassert
 */
typedef void (*EmuPeripheralIRQCallback)(void *emu_ctx, int irq, int level);

/**
 * DMA request callback - peripheral requests a DMA transfer.
 *
 * @param emu_ctx   Emulator context
 * @param channel   DMA channel number
 * @param request   Request type (peripheral-specific)
 */
typedef void (*EmuPeripheralDMACallback)(void *emu_ctx, int channel, int request);

/*============================================================================
 * Peripheral Virtual Table
 *============================================================================*/

/**
 * Virtual function table for peripheral operations.
 *
 * All function pointers except read/write may be NULL if not needed.
 */
struct EmuPeripheralVTable {
    /**
     * Read from peripheral register.
     *
     * @param ctx       Peripheral context (from EmuPeripheral.context)
     * @param offset    Byte offset from peripheral base address
     * @param size      Access size: 1, 2, or 4 bytes
     * @return          Value read (zero-extended to 32 bits)
     */
    uint32_t (*read)(void *ctx, uint32_t offset, uint8_t size);

    /**
     * Write to peripheral register.
     *
     * @param ctx       Peripheral context
     * @param offset    Byte offset from peripheral base address
     * @param value     Value to write
     * @param size      Access size: 1, 2, or 4 bytes
     */
    void (*write)(void *ctx, uint32_t offset, uint32_t value, uint8_t size);

    /**
     * Reset peripheral to initial state.
     * Called on system reset.
     *
     * @param ctx       Peripheral context
     */
    void (*reset)(void *ctx);

    /**
     * Advance peripheral time by given cycles.
     * Used for cycle-accurate peripherals (timers, etc.)
     *
     * @param ctx       Peripheral context
     * @param cycles    Number of CPU cycles elapsed
     */
    void (*tick)(void *ctx, uint64_t cycles);

    /**
     * Destroy peripheral and free resources.
     *
     * @param ctx       Peripheral context
     */
    void (*destroy)(void *ctx);

    /**
     * Set IRQ callback for this peripheral.
     *
     * @param ctx       Peripheral context
     * @param cb        Callback function (NULL to disable)
     * @param emu_ctx   Emulator context to pass to callback
     */
    void (*set_irq_callback)(void *ctx, EmuPeripheralIRQCallback cb, void *emu_ctx);

    /**
     * Set DMA callback for this peripheral.
     *
     * @param ctx       Peripheral context
     * @param cb        Callback function (NULL to disable)
     * @param emu_ctx   Emulator context to pass to callback
     */
    void (*set_dma_callback)(void *ctx, EmuPeripheralDMACallback cb, void *emu_ctx);

    /**
     * Get human-readable peripheral state for debugging.
     *
     * @param ctx       Peripheral context
     * @param buf       Output buffer
     * @param buf_size  Size of output buffer
     * @return          Number of characters written
     */
    int (*debug_state)(void *ctx, char *buf, size_t buf_size);
};

/*============================================================================
 * Peripheral Structure
 *============================================================================*/

/**
 * Peripheral instance.
 *
 * Created by peripheral factory functions and registered with the emulator.
 */
struct EmuPeripheral {
    const char *name;               /**< Instance name (e.g., "uart1") */
    const char *type;               /**< Peripheral type (e.g., "stm32_uart") */
    void *context;                  /**< Opaque peripheral state */
    EmuPeripheralVTable vtable;     /**< Virtual function table */

    /* Filled in by emulator during registration */
    uint64_t base_addr;             /**< Base address (set by emulator) */
    uint64_t size;                  /**< Address space size (set by emulator) */
    void *emu_ctx;                  /**< Emulator context (set by emulator) */
};

/*============================================================================
 * Peripheral Factory Function Type
 *============================================================================*/

/**
 * Factory function prototype for creating peripherals.
 *
 * C example:
 *   EmuPeripheral* stm32_uart_create(int irq_num);
 *
 * Rust example:
 *   #[no_mangle]
 *   pub extern "C" fn stm32_gpio_create() -> *mut EmuPeripheral;
 */
typedef EmuPeripheral* (*EmuPeripheralFactory)(void);

/*============================================================================
 * Helper Macros
 *============================================================================*/

/**
 * Initialize a peripheral structure with required fields.
 */
#define EMU_PERIPHERAL_INIT(p, n, t, c) do { \
    (p)->name = (n);                          \
    (p)->type = (t);                          \
    (p)->context = (c);                       \
    (p)->base_addr = 0;                       \
    (p)->size = 0;                            \
    (p)->emu_ctx = NULL;                      \
} while(0)

/**
 * Check if a peripheral implements a given method.
 */
#define EMU_PERIPHERAL_HAS(p, method) ((p)->vtable.method != NULL)

/**
 * Safely call a peripheral method if it exists.
 */
#define EMU_PERIPHERAL_CALL(p, method, ...) \
    (EMU_PERIPHERAL_HAS(p, method) ? (p)->vtable.method((p)->context, ##__VA_ARGS__) : (void)0)

/*============================================================================
 * Common Peripheral Registers (for standardization)
 *============================================================================*/

/**
 * Suggested register offsets for common peripheral types.
 * Individual peripherals may vary; these are just conventions.
 */

/* UART-like peripherals */
#define EMU_PERIPH_UART_SR      0x00    /* Status register */
#define EMU_PERIPH_UART_DR      0x04    /* Data register */
#define EMU_PERIPH_UART_BRR     0x08    /* Baud rate register */
#define EMU_PERIPH_UART_CR1     0x0C    /* Control register 1 */
#define EMU_PERIPH_UART_CR2     0x10    /* Control register 2 */
#define EMU_PERIPH_UART_CR3     0x14    /* Control register 3 */

/* GPIO-like peripherals */
#define EMU_PERIPH_GPIO_MODER   0x00    /* Mode register */
#define EMU_PERIPH_GPIO_OTYPER  0x04    /* Output type */
#define EMU_PERIPH_GPIO_OSPEEDR 0x08    /* Output speed */
#define EMU_PERIPH_GPIO_PUPDR   0x0C    /* Pull-up/pull-down */
#define EMU_PERIPH_GPIO_IDR     0x10    /* Input data */
#define EMU_PERIPH_GPIO_ODR     0x14    /* Output data */
#define EMU_PERIPH_GPIO_BSRR    0x18    /* Bit set/reset */

/* Timer-like peripherals */
#define EMU_PERIPH_TIM_CR1      0x00    /* Control register 1 */
#define EMU_PERIPH_TIM_CR2      0x04    /* Control register 2 */
#define EMU_PERIPH_TIM_DIER     0x0C    /* DMA/interrupt enable */
#define EMU_PERIPH_TIM_SR       0x10    /* Status register */
#define EMU_PERIPH_TIM_CNT      0x24    /* Counter value */
#define EMU_PERIPH_TIM_PSC      0x28    /* Prescaler */
#define EMU_PERIPH_TIM_ARR      0x2C    /* Auto-reload */

#ifdef __cplusplus
}
#endif

#endif /* EMU_PERIPHERAL_H */
