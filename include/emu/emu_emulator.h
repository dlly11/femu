/**
 * @file emu_emulator.h
 * @brief Generic emulator orchestrator for multi-architecture support
 *
 * This file defines the abstract emulator interface that ties together
 * CPU, decoder, executor, memory, and interrupt controller components.
 * Architecture-specific emulators implement the vtable functions.
 */

#ifndef EMU_EMULATOR_H
#define EMU_EMULATOR_H

#include "emu_cpu.h"
#include "emu_decoder.h"
#include "emu_executor.h"
#include "emu_interrupt.h"
#include "emu_memory.h"
#include "emu_peripheral.h"
#include "emu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Forward Declarations
 *============================================================================*/

typedef struct EmuEmulator EmuEmulator;
typedef struct EmuEmulatorVTable EmuEmulatorVTable;
typedef struct EmuEmulatorConfig EmuEmulatorConfig;

/*============================================================================
 * Emulator Configuration
 *============================================================================*/

/**
 * Generic emulator configuration.
 *
 * Architecture-specific configurations extend this through arch_config.
 */
struct EmuEmulatorConfig {
  EmuArchType arch; /**< Architecture type */

  /* Memory configuration */
  uint64_t flash_base; /**< Flash base address */
  uint64_t flash_size; /**< Flash size */
  uint64_t ram_base;   /**< RAM base address */
  uint64_t ram_size;   /**< RAM size */

  /* Interrupt configuration */
  int num_irqs; /**< Number of external IRQs */

  /* Architecture-specific config */
  void *arch_config; /**< Pointer to arch-specific config */
};

/*============================================================================
 * Emulator Virtual Table
 *============================================================================*/

/**
 * Virtual function table for emulator operations.
 */
struct EmuEmulatorVTable {
  /**
   * Destroy emulator and free resources.
   *
   * @param emu       Emulator instance
   */
  void (*destroy)(EmuEmulator *emu);

  /**
   * Reset emulator to initial state.
   *
   * @param emu       Emulator instance
   */
  void (*reset)(EmuEmulator *emu);

  /**
   * Get architecture type.
   *
   * @param emu       Emulator instance
   * @return          Architecture type
   */
  EmuArchType (*get_arch)(const EmuEmulator *emu);

  /**
   * Get CPU interface.
   *
   * @param emu       Emulator instance
   * @return          CPU interface
   */
  EmuCPU *(*get_cpu)(EmuEmulator *emu);

  /**
   * Get decoder interface.
   *
   * @param emu       Emulator instance
   * @return          Decoder interface
   */
  EmuDecoder *(*get_decoder)(EmuEmulator *emu);

  /**
   * Get executor interface.
   *
   * @param emu       Emulator instance
   * @return          Executor interface
   */
  EmuExecutor *(*get_executor)(EmuEmulator *emu);

  /**
   * Get memory system.
   *
   * @param emu       Emulator instance
   * @return          Memory system
   */
  EmuMemorySystem *(*get_memory)(EmuEmulator *emu);

  /**
   * Get interrupt controller.
   *
   * @param emu       Emulator instance
   * @return          Interrupt controller
   */
  EmuInterruptController *(*get_interrupt_controller)(EmuEmulator *emu);

  /* Memory setup */

  /**
   * Add flash memory region.
   *
   * @param emu       Emulator instance
   * @param base      Base address
   * @param size      Size in bytes
   * @return          EMU_OK or error code
   */
  int (*add_flash)(EmuEmulator *emu, uint64_t base, uint64_t size);

  /**
   * Add RAM region.
   *
   * @param emu       Emulator instance
   * @param base      Base address
   * @param size      Size in bytes
   * @return          EMU_OK or error code
   */
  int (*add_ram)(EmuEmulator *emu, uint64_t base, uint64_t size);

  /**
   * Load data into memory.
   *
   * @param emu       Emulator instance
   * @param addr      Destination address
   * @param data      Data to load
   * @param size      Size in bytes
   * @return          EMU_OK or error code
   */
  int (*load)(EmuEmulator *emu, uint64_t addr, const uint8_t *data,
              uint64_t size);

  /* Execution */

  /**
   * Execute a single instruction.
   *
   * @param emu       Emulator instance
   * @return          EMU_OK, EMU_ERR_BREAKPOINT, or error code
   */
  int (*step)(EmuEmulator *emu);

  /**
   * Run until stopped, breakpoint, or max cycles reached.
   *
   * @param emu           Emulator instance
   * @param max_cycles    Maximum cycles (0 = unlimited)
   * @return              Cycles executed, or negative on error
   */
  int64_t (*run)(EmuEmulator *emu, uint64_t max_cycles);

  /**
   * Request emulator to stop (thread-safe).
   *
   * @param emu       Emulator instance
   */
  void (*stop)(EmuEmulator *emu);

  /* State access */

  /**
   * Get emulator state.
   *
   * @param emu       Emulator instance
   * @return          Current state
   */
  EmuState (*get_state)(const EmuEmulator *emu);

  /**
   * Get last error code.
   *
   * @param emu       Emulator instance
   * @return          Last error code
   */
  int (*get_last_error)(const EmuEmulator *emu);

  /* Breakpoints */

  /**
   * Add a breakpoint.
   *
   * @param emu       Emulator instance
   * @param addr      Breakpoint address
   * @return          EMU_OK or error code
   */
  int (*add_breakpoint)(EmuEmulator *emu, uint64_t addr);

  /**
   * Remove a breakpoint.
   *
   * @param emu       Emulator instance
   * @param addr      Breakpoint address
   * @return          EMU_OK or error code
   */
  int (*remove_breakpoint)(EmuEmulator *emu, uint64_t addr);

  /**
   * Check if address has a breakpoint.
   *
   * @param emu       Emulator instance
   * @param addr      Address to check
   * @return          true if breakpoint exists
   */
  bool (*has_breakpoint)(const EmuEmulator *emu, uint64_t addr);

  /**
   * Clear all breakpoints.
   *
   * @param emu       Emulator instance
   */
  void (*clear_breakpoints)(EmuEmulator *emu);

  /* Peripherals */

  /**
   * Register a peripheral.
   *
   * @param emu       Emulator instance
   * @param periph    Peripheral to register
   * @param base      Base address
   * @param size      Address space size
   * @return          EMU_OK or error code
   */
  int (*add_peripheral)(EmuEmulator *emu, EmuPeripheral *periph, uint64_t base,
                        uint64_t size);
};

/*============================================================================
 * Emulator Structure
 *============================================================================*/

/**
 * Abstract emulator instance.
 *
 * Architecture implementations embed this as the first member of their
 * concrete emulator structure, allowing safe casting.
 */
struct EmuEmulator {
  const EmuEmulatorVTable *vtable; /**< Virtual function table */
  EmuArchType arch;                /**< Architecture type */
  EmuState state;                  /**< Current execution state */
  int last_error;                  /**< Last error code */
  void *arch_state;                /**< Architecture-specific state */
};

/*============================================================================
 * Factory Functions
 *============================================================================*/

/**
 * Create an emulator for the specified architecture.
 *
 * @param arch      Architecture type
 * @param config    Configuration (may be NULL for defaults)
 * @return          Emulator instance or NULL on error
 */
EmuEmulator *emu_emulator_create(EmuArchType arch,
                                 const EmuEmulatorConfig *config);

/**
 * Destroy an emulator and free all resources.
 *
 * @param emu       Emulator to destroy
 */
void emu_emulator_destroy(EmuEmulator *emu);

/*============================================================================
 * Convenience Macros
 *============================================================================*/

#define EMU_EMULATOR_INIT(emu, vtbl, arch_type, state)                         \
  do {                                                                         \
    (emu)->vtable = (vtbl);                                                    \
    (emu)->arch = (arch_type);                                                 \
    (emu)->state = EMU_STATE_STOPPED;                                          \
    (emu)->last_error = EMU_OK;                                                \
    (emu)->arch_state = (state);                                               \
  } while (0)

/*============================================================================
 * Inline Convenience Functions
 *============================================================================*/

static inline void emu_reset(EmuEmulator *emu) {
  if (emu && emu->vtable && emu->vtable->reset) {
    emu->vtable->reset(emu);
  }
}

static inline EmuCPU *emu_get_cpu(EmuEmulator *emu) {
  return (emu && emu->vtable && emu->vtable->get_cpu)
             ? emu->vtable->get_cpu(emu)
             : NULL;
}

static inline EmuDecoder *emu_get_decoder(EmuEmulator *emu) {
  return (emu && emu->vtable && emu->vtable->get_decoder)
             ? emu->vtable->get_decoder(emu)
             : NULL;
}

static inline EmuExecutor *emu_get_executor(EmuEmulator *emu) {
  return (emu && emu->vtable && emu->vtable->get_executor)
             ? emu->vtable->get_executor(emu)
             : NULL;
}

static inline EmuMemorySystem *emu_get_memory(EmuEmulator *emu) {
  return (emu && emu->vtable && emu->vtable->get_memory)
             ? emu->vtable->get_memory(emu)
             : NULL;
}

static inline int emu_add_flash(EmuEmulator *emu, uint64_t base,
                                uint64_t size) {
  return (emu && emu->vtable && emu->vtable->add_flash)
             ? emu->vtable->add_flash(emu, base, size)
             : EMU_ERR_NOT_INITIALIZED;
}

static inline int emu_add_ram(EmuEmulator *emu, uint64_t base, uint64_t size) {
  return (emu && emu->vtable && emu->vtable->add_ram)
             ? emu->vtable->add_ram(emu, base, size)
             : EMU_ERR_NOT_INITIALIZED;
}

static inline int emu_load(EmuEmulator *emu, uint64_t addr, const uint8_t *data,
                           uint64_t size) {
  return (emu && emu->vtable && emu->vtable->load)
             ? emu->vtable->load(emu, addr, data, size)
             : EMU_ERR_NOT_INITIALIZED;
}

static inline int emu_step(EmuEmulator *emu) {
  return (emu && emu->vtable && emu->vtable->step) ? emu->vtable->step(emu)
                                                   : EMU_ERR_NOT_INITIALIZED;
}

static inline int64_t emu_run(EmuEmulator *emu, uint64_t max_cycles) {
  return (emu && emu->vtable && emu->vtable->run)
             ? emu->vtable->run(emu, max_cycles)
             : EMU_ERR_NOT_INITIALIZED;
}

static inline void emu_stop(EmuEmulator *emu) {
  if (emu && emu->vtable && emu->vtable->stop) {
    emu->vtable->stop(emu);
  }
}

static inline EmuState emu_get_state(const EmuEmulator *emu) {
  return (emu && emu->vtable && emu->vtable->get_state)
             ? emu->vtable->get_state(emu)
             : EMU_STATE_STOPPED;
}

static inline int emu_add_breakpoint(EmuEmulator *emu, uint64_t addr) {
  return (emu && emu->vtable && emu->vtable->add_breakpoint)
             ? emu->vtable->add_breakpoint(emu, addr)
             : EMU_ERR_NOT_INITIALIZED;
}

static inline int emu_remove_breakpoint(EmuEmulator *emu, uint64_t addr) {
  return (emu && emu->vtable && emu->vtable->remove_breakpoint)
             ? emu->vtable->remove_breakpoint(emu, addr)
             : EMU_ERR_NOT_INITIALIZED;
}

static inline void emu_clear_breakpoints(EmuEmulator *emu) {
  if (emu && emu->vtable && emu->vtable->clear_breakpoints) {
    emu->vtable->clear_breakpoints(emu);
  }
}

static inline int emu_add_peripheral(EmuEmulator *emu, EmuPeripheral *periph,
                                     uint64_t base, uint64_t size) {
  return (emu && emu->vtable && emu->vtable->add_peripheral)
             ? emu->vtable->add_peripheral(emu, periph, base, size)
             : EMU_ERR_NOT_INITIALIZED;
}

#ifdef __cplusplus
}
#endif

#endif /* EMU_EMULATOR_H */
