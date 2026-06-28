/**
 * @file emulator_base.c
 * @brief Generic emulator factory and base implementation
 *
 * Provides the factory function for creating architecture-specific emulators
 * through the abstract EmuEmulator interface.
 */

#include "emu/emu_emulator.h"
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Architecture Registration
 *============================================================================*/

/**
 * Registered architecture entry.
 */
typedef struct {
  EmuArchType arch;
  ArchEmulatorCreateFn create_fn;
} ArchRegistryEntry;

/* Maximum number of registered architectures */
#define MAX_ARCH_REGISTRY 8

/* Static registry of architecture creators */
static ArchRegistryEntry arch_registry[MAX_ARCH_REGISTRY];
static int num_registered_archs = 0;

/**
 * Register an architecture emulator creator.
 *
 * @param arch          Architecture type
 * @param create_fn     Creation function
 * @return              EMU_OK or error code
 */
int emu_register_arch(EmuArchType arch, ArchEmulatorCreateFn create_fn) {
  if (num_registered_archs >= MAX_ARCH_REGISTRY) {
    return EMU_ERR_OUT_OF_MEMORY;
  }

  /* Check for duplicate */
  for (int i = 0; i < num_registered_archs; i++) {
    if (arch_registry[i].arch == arch) {
      arch_registry[i].create_fn = create_fn;
      return EMU_OK;
    }
  }

  arch_registry[num_registered_archs].arch = arch;
  arch_registry[num_registered_archs].create_fn = create_fn;
  num_registered_archs++;

  return EMU_OK;
}

/*============================================================================
 * Factory Functions
 *============================================================================*/

EmuEmulator *emu_emulator_create(EmuArchType arch,
                                 const EmuEmulatorConfig *config) {
  /* Find the architecture creator */
  for (int i = 0; i < num_registered_archs; i++) {
    if (arch_registry[i].arch == arch && arch_registry[i].create_fn) {
      return arch_registry[i].create_fn(config);
    }
  }

  /* Architecture not registered */
  return NULL;
}

void emu_emulator_destroy(EmuEmulator *emu) {
  if (emu && emu->vtable && emu->vtable->destroy) {
    emu->vtable->destroy(emu);
  }
}

/*============================================================================
 * Default Configurations
 *============================================================================*/

/**
 * Initialize emulator config with defaults.
 *
 * @param config    Config to initialize
 * @param arch      Architecture type
 */
void emu_emulator_default_config(EmuEmulatorConfig *config, EmuArchType arch) {
  if (!config) {
    return;
  }

  memset(config, 0, sizeof(*config));
  config->arch = arch;

  /* Default memory layout */
  switch (arch) {
  case EMU_ARCH_ARMV8M:
  case EMU_ARCH_ARMV7M:
    /* ARM Cortex-M typical layout */
    config->flash_base = 0x08000000;
    config->flash_size = 0x00080000; /* 512KB */
    config->ram_base = 0x20000000;
    config->ram_size = 0x00020000; /* 128KB */
    config->num_irqs = 32;
    break;

  case EMU_ARCH_RISCV32:
  case EMU_ARCH_RISCV64:
    /* RISC-V typical layout */
    config->flash_base = 0x00000000;
    config->flash_size = 0x00100000; /* 1MB */
    config->ram_base = 0x80000000;
    config->ram_size = 0x00100000; /* 1MB */
    config->num_irqs = 32;
    break;

  case EMU_ARCH_UNKNOWN:
  default:
    /* Generic defaults */
    config->flash_base = 0x00000000;
    config->flash_size = 0x00100000;
    config->ram_base = 0x10000000;
    config->ram_size = 0x00100000;
    config->num_irqs = 16;
    break;
  }
}

/*============================================================================
 * Architecture Query
 *============================================================================*/

/**
 * Get name string for architecture type.
 *
 * @param arch      Architecture type
 * @return          Static name string
 */
const char *emu_arch_name(EmuArchType arch) {
  switch (arch) {
  case EMU_ARCH_ARMV8M:
    return "ARMv8-M";
  case EMU_ARCH_ARMV7M:
    return "ARMv7-M";
  case EMU_ARCH_RISCV32:
    return "RISC-V 32";
  case EMU_ARCH_RISCV64:
    return "RISC-V 64";
  case EMU_ARCH_UNKNOWN:
  default:
    return "Unknown";
  }
}

/**
 * Check if architecture is registered.
 *
 * @param arch      Architecture type
 * @return          true if registered
 */
bool emu_arch_is_available(EmuArchType arch) {
  for (int i = 0; i < num_registered_archs; i++) {
    if (arch_registry[i].arch == arch) {
      return true;
    }
  }
  return false;
}

/**
 * Get list of available architectures.
 *
 * @param archs     Output array (must have space for MAX_ARCH_REGISTRY entries)
 * @return          Number of available architectures
 */
int emu_get_available_archs(EmuArchType *archs) {
  if (!archs) {
    return num_registered_archs;
  }

  for (int i = 0; i < num_registered_archs; i++) {
    archs[i] = arch_registry[i].arch;
  }
  return num_registered_archs;
}
