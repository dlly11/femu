/**
 * @file emulator.c
 * @brief Emulator glue layer implementation
 *
 * Integrates executor, memory, NVIC, and MPU into a unified emulator.
 */

#include "arch/armv8m/armv8m_emulator.h"
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Memory Callbacks
 *============================================================================*/

/**
 * Memory read callback for executor.
 */
static uint32_t mem_read_callback(void *ctx, uint32_t addr, uint8_t size, bool *fault)
{
    Emulator *emu = (Emulator *)ctx;

    /* Determine privilege level */
    bool privileged = (emu->exec.cpu.privilege == PRIV_PRIVILEGED);

    return (uint32_t)emu_mem_read(&emu->mem, addr, size, privileged, fault);
}

/**
 * Memory write callback for executor.
 */
static void mem_write_callback(void *ctx, uint32_t addr, uint32_t value, uint8_t size, bool *fault)
{
    Emulator *emu = (Emulator *)ctx;

    bool privileged = (emu->exec.cpu.privilege == PRIV_PRIVILEGED);

    emu_mem_write(&emu->mem, addr, value, size, privileged, fault);
}

/**
 * Memory get_ptr callback for instruction fetch.
 */
static const uint8_t *mem_get_ptr_callback(void *ctx, uint32_t addr, uint32_t size)
{
    Emulator *emu = (Emulator *)ctx;
    return emu_mem_get_ptr(&emu->mem, addr, size);
}

/*============================================================================
 * NVIC Callbacks
 *============================================================================*/

/**
 * Get highest priority pending exception.
 */
static int nvic_get_pending_callback(void *ctx)
{
    Emulator *emu = (Emulator *)ctx;

    /* Calculate current execution priority */
    int current_pri = -1;
    if (emu->exec.cpu.current_exception > 0) {
        if (emu->exec.cpu.current_exception < NVIC_NUM_EXCEPTIONS) {
            current_pri = emu->nvic.shpr[emu->exec.cpu.current_exception - 4];
        } else {
            int irq = emu->exec.cpu.current_exception - NVIC_NUM_EXCEPTIONS;
            current_pri = emu->nvic.priority[irq];
        }
    }

    return armv8m_nvic_get_pending_exception(&emu->nvic,
                                              (uint8_t)emu->exec.cpu.basepri,
                                              (uint8_t)emu->exec.cpu.primask,
                                              (uint8_t)emu->exec.cpu.faultmask,
                                              current_pri);
}

/**
 * Get priority of exception.
 */
static int nvic_get_priority_callback(void *ctx, int exc)
{
    Emulator *emu = (Emulator *)ctx;

    if (exc < 4) {
        /* Fixed priority exceptions */
        switch (exc) {
            case ARMV8M_EXC_RESET: return -3;
            case ARMV8M_EXC_NMI: return -2;
            case ARMV8M_EXC_HARDFAULT: return -1;
            default: return 0;
        }
    } else if (exc < NVIC_NUM_EXCEPTIONS) {
        return emu->nvic.shpr[exc - 4];
    } else {
        int irq = exc - NVIC_NUM_EXCEPTIONS;
        return armv8m_nvic_get_priority(&emu->nvic, irq);
    }
}

/**
 * Clear pending state of exception.
 */
static void nvic_clear_pending_callback(void *ctx, int exc)
{
    Emulator *emu = (Emulator *)ctx;

    if (exc >= NVIC_NUM_EXCEPTIONS) {
        int irq = exc - NVIC_NUM_EXCEPTIONS;
        armv8m_nvic_clear_pending(&emu->nvic, irq);
    } else {
        armv8m_nvic_clear_exception_pending(&emu->nvic, exc);
    }
}

/**
 * Set pending state of exception.
 */
static void nvic_set_pending_callback(void *ctx, int exc)
{
    Emulator *emu = (Emulator *)ctx;

    if (exc >= NVIC_NUM_EXCEPTIONS) {
        int irq = exc - NVIC_NUM_EXCEPTIONS;
        armv8m_nvic_set_pending(&emu->nvic, irq);
    } else {
        armv8m_nvic_set_exception_pending(&emu->nvic, exc);
    }
}

/*============================================================================
 * MPU Callback
 *============================================================================*/

/**
 * MPU check callback for memory system.
 */
static bool mpu_check_callback(void *ctx, uint64_t addr, uint64_t size,
                               bool is_write, bool privileged)
{
    Emulator *emu = (Emulator *)ctx;

    if (emu->mpu.num_regions == 0) {
        return true;  /* No MPU */
    }

    /* Determine if we're in HardFault/NMI context */
    bool in_hardfault_nmi = (emu->exec.cpu.current_exception == ARMV8M_EXC_HARDFAULT ||
                             emu->exec.cpu.current_exception == ARMV8M_EXC_NMI);

    MPUFaultInfo fault_info;
    bool ok = armv8m_mpu_check(&emu->mpu, (uint32_t)addr, (uint32_t)size, is_write,
                               false, privileged, in_hardfault_nmi, &fault_info);
    return ok;
}

/*============================================================================
 * Peripheral MMIO Callbacks
 *============================================================================*/

/**
 * MMIO read for a peripheral.
 */
static uint64_t periph_mmio_read(void *ctx, uint64_t offset, uint8_t size)
{
    EmuPeripheral *periph = (EmuPeripheral *)ctx;
    if (periph && periph->vtable.read) {
        return periph->vtable.read(periph->context, (uint32_t)offset, size);
    }
    return 0;
}

/**
 * MMIO write for a peripheral.
 */
static void periph_mmio_write(void *ctx, uint64_t offset, uint64_t value, uint8_t size)
{
    EmuPeripheral *periph = (EmuPeripheral *)ctx;
    if (periph && periph->vtable.write) {
        periph->vtable.write(periph->context, (uint32_t)offset, (uint32_t)value, size);
    }
}

/*============================================================================
 * System Register MMIO (NVIC/SCB/MPU)
 *============================================================================*/

/**
 * NVIC MMIO read callback (0xE000E100).
 */
static uint64_t nvic_mmio_read(void *ctx, uint64_t offset, uint8_t size)
{
    Emulator *emu = (Emulator *)ctx;
    return armv8m_nvic_read(&emu->nvic, (uint32_t)offset, size);
}

/**
 * NVIC MMIO write callback.
 */
static void nvic_mmio_write(void *ctx, uint64_t offset, uint64_t value, uint8_t size)
{
    Emulator *emu = (Emulator *)ctx;
    armv8m_nvic_write(&emu->nvic, (uint32_t)offset, (uint32_t)value, size);
}

/**
 * SCB MMIO read callback (0xE000ED00).
 */
static uint64_t scb_mmio_read(void *ctx, uint64_t offset, uint8_t size)
{
    Emulator *emu = (Emulator *)ctx;
    return armv8m_scb_read(&emu->nvic, (uint32_t)offset, size);
}

/**
 * SCB MMIO write callback.
 */
static void scb_mmio_write(void *ctx, uint64_t offset, uint64_t value, uint8_t size)
{
    Emulator *emu = (Emulator *)ctx;
    armv8m_scb_write(&emu->nvic, (uint32_t)offset, (uint32_t)value, size);
}

/**
 * MPU MMIO read callback (0xE000ED90).
 */
static uint64_t mpu_mmio_read(void *ctx, uint64_t offset, uint8_t size)
{
    Emulator *emu = (Emulator *)ctx;
    return armv8m_mpu_read(&emu->mpu, (uint32_t)offset, size);
}

/**
 * MPU MMIO write callback.
 */
static void mpu_mmio_write(void *ctx, uint64_t offset, uint64_t value, uint8_t size)
{
    Emulator *emu = (Emulator *)ctx;
    armv8m_mpu_write(&emu->mpu, (uint32_t)offset, (uint32_t)value, size);
}

/*============================================================================
 * Lifecycle API
 *============================================================================*/

void armv8m_emu_default_config(EmulatorConfig *config)
{
    if (!config) return;

    memset(config, 0, sizeof(*config));

    config->has_fpu = false;
    config->has_dsp = false;
    config->has_trustzone = false;
    config->num_mpu_regions = 8;
    config->num_irqs = 32;

    /* Default STM32-like memory map */
    config->default_flash_base = 0x08000000;
    config->default_flash_size = 0x00080000;  /* 512KB */
    config->default_ram_base = 0x20000000;
    config->default_ram_size = 0x00020000;    /* 128KB */
}

int armv8m_emu_init(Emulator *emu, const EmulatorConfig *config)
{
    if (!emu) return ARMV8M_ERR_INVALID_PARAM;

    memset(emu, 0, sizeof(*emu));

    /* Apply config or defaults */
    EmulatorConfig cfg;
    if (config) {
        cfg = *config;
    } else {
        armv8m_emu_default_config(&cfg);
    }

    /* Initialize executor */
    armv8m_exec_init(&emu->exec);
    emu->exec.has_fpu = cfg.has_fpu;
    emu->exec.has_dsp = cfg.has_dsp;
    emu->exec.has_trustzone = cfg.has_trustzone;
    emu->exec.num_mpu_regions = (uint32_t)cfg.num_mpu_regions;

    /* Initialize memory system */
    emu_mem_init(&emu->mem);

    /* Initialize NVIC */
    armv8m_nvic_init(&emu->nvic, cfg.num_irqs);

    /* Initialize MPU */
    armv8m_mpu_init(&emu->mpu, cfg.num_mpu_regions);

    /* Wire memory callbacks */
    emu->exec.mem.ctx = emu;
    emu->exec.mem.read = mem_read_callback;
    emu->exec.mem.write = mem_write_callback;
    emu->exec.mem.get_ptr = mem_get_ptr_callback;

    /* Wire NVIC callbacks */
    emu->exec.nvic.ctx = emu;
    emu->exec.nvic.get_pending = nvic_get_pending_callback;
    emu->exec.nvic.get_priority = nvic_get_priority_callback;
    emu->exec.nvic.clear_pending = nvic_clear_pending_callback;
    emu->exec.nvic.set_pending = nvic_set_pending_callback;

    /* Wire MPU to memory system */
    if (cfg.num_mpu_regions > 0) {
        emu->mem.mpu_ctx = emu;
        emu->mem.mpu_check = mpu_check_callback;
    }

    /* Add system register MMIO regions */
    /* NVIC registers: 0xE000E100 - 0xE000ECFF */
    emu_mem_add_mmio(&emu->mem, 0xE000E100, 0x0C00,
                        emu, nvic_mmio_read, nvic_mmio_write);

    /* SCB registers: 0xE000ED00 - 0xE000ED8F */
    emu_mem_add_mmio(&emu->mem, 0xE000ED00, 0x0090,
                        emu, scb_mmio_read, scb_mmio_write);

    /* MPU registers: 0xE000ED90 - 0xE000EDBF */
    emu_mem_add_mmio(&emu->mem, 0xE000ED90, 0x0030,
                        emu, mpu_mmio_read, mpu_mmio_write);

    emu->state = EMU_STATE_STOPPED;

    return ARMV8M_OK;
}

void armv8m_emu_destroy(Emulator *emu)
{
    if (!emu) return;

    /* Free memory backing */
    free(emu->flash_data);
    free(emu->ram_data);

    /* Destroy peripherals */
    for (int i = 0; i < emu->num_peripherals; i++) {
        if (emu->peripherals[i] && emu->peripherals[i]->vtable.destroy) {
            emu->peripherals[i]->vtable.destroy(emu->peripherals[i]->context);
        }
    }

    memset(emu, 0, sizeof(*emu));
}

void armv8m_emu_reset(Emulator *emu)
{
    if (!emu) return;

    /* Reset all modules */
    armv8m_nvic_reset(&emu->nvic);
    armv8m_mpu_reset(&emu->mpu);

    /* Reset executor - uses VTOR for initial SP/PC */
    armv8m_exec_reset(&emu->exec, emu->exec.vtor);

    /* Read initial SP and PC from vector table */
    if (emu->flash_data) {
        const uint8_t *vt = emu_mem_get_ptr(&emu->mem, emu->exec.vtor, 8);
        if (vt) {
            uint32_t initial_sp = (uint32_t)vt[0] | ((uint32_t)vt[1] << 8) |
                                  ((uint32_t)vt[2] << 16) | ((uint32_t)vt[3] << 24);
            uint32_t reset_vector = (uint32_t)vt[4] | ((uint32_t)vt[5] << 8) |
                                    ((uint32_t)vt[6] << 16) | ((uint32_t)vt[7] << 24);

            emu->exec.cpu.sp_main = initial_sp;
            emu->exec.cpu.r[ARMV8M_REG_SP] = initial_sp;
            emu->exec.cpu.r[ARMV8M_REG_PC] = reset_vector & ~1u;  /* Clear Thumb bit */
        }
    }

    /* Reset peripherals */
    for (int i = 0; i < emu->num_peripherals; i++) {
        if (emu->peripherals[i] && emu->peripherals[i]->vtable.reset) {
            emu->peripherals[i]->vtable.reset(emu->peripherals[i]->context);
        }
    }

    emu->state = EMU_STATE_STOPPED;
    emu->stop_requested = false;
}

/*============================================================================
 * Memory Setup API
 *============================================================================*/

int armv8m_emu_add_flash(Emulator *emu, uint32_t base, uint32_t size)
{
    if (!emu) return ARMV8M_ERR_INVALID_PARAM;
    if (size == 0) return ARMV8M_ERR_INVALID_PARAM;

    /* Free existing flash */
    free(emu->flash_data);

    /* Allocate backing storage */
    emu->flash_data = (uint8_t *)calloc(1, size);
    if (!emu->flash_data) {
        return ARMV8M_ERR_INVALID_PARAM;  /* Out of memory */
    }

    emu->flash_base = base;
    emu->flash_size = size;

    /* Set VTOR to flash base */
    emu->exec.vtor = base;

    /* Add as RAM region (to allow loading data) */
    /* In a real system flash would be read-only, but for emulation we need to write it first */
    return emu_mem_add_ram(&emu->mem, base, size, emu->flash_data);
}

int armv8m_emu_add_ram(Emulator *emu, uint32_t base, uint32_t size)
{
    if (!emu) return ARMV8M_ERR_INVALID_PARAM;
    if (size == 0) return ARMV8M_ERR_INVALID_PARAM;

    /* Free existing RAM */
    free(emu->ram_data);

    /* Allocate backing storage */
    emu->ram_data = (uint8_t *)calloc(1, size);
    if (!emu->ram_data) {
        return ARMV8M_ERR_INVALID_PARAM;  /* Out of memory */
    }

    emu->ram_base = base;
    emu->ram_size = size;

    return emu_mem_add_ram(&emu->mem, base, size, emu->ram_data);
}

int armv8m_emu_load(Emulator *emu, uint32_t addr, const uint8_t *data, uint32_t size)
{
    if (!emu || !data) return ARMV8M_ERR_INVALID_PARAM;
    return emu_mem_load(&emu->mem, addr, data, size);
}

/*============================================================================
 * Execution API
 *============================================================================*/

int armv8m_emu_step(Emulator *emu)
{
    if (!emu) return ARMV8M_ERR_INVALID_PARAM;

    /* Check for breakpoint at current PC */
    uint32_t pc = emu->exec.cpu.r[ARMV8M_REG_PC];
    if (armv8m_emu_has_breakpoint(emu, pc)) {
        emu->state = EMU_STATE_BREAKPOINT;
        return ARMV8M_ERR_BREAKPOINT;
    }

    /* Execute one instruction */
    emu->state = EMU_STATE_RUNNING;
    int result = armv8m_exec_step(&emu->exec);

    if (result == ARMV8M_ERR_BREAKPOINT) {
        emu->state = EMU_STATE_BREAKPOINT;
    } else if (result == ARMV8M_ERR_HALTED) {
        emu->state = EMU_STATE_HALTED;
    } else if (result != ARMV8M_OK) {
        emu->state = EMU_STATE_FAULT;
        emu->last_error = result;
    } else {
        emu->state = EMU_STATE_STOPPED;
    }

    return result;
}

int64_t armv8m_emu_run(Emulator *emu, uint64_t max_cycles)
{
    if (!emu) return ARMV8M_ERR_INVALID_PARAM;

    emu->state = EMU_STATE_RUNNING;
    emu->stop_requested = false;

    uint64_t start_cycles = emu->exec.cpu.cycles;
    uint64_t target_cycles = max_cycles > 0 ? start_cycles + max_cycles : UINT64_MAX;

    while (!emu->stop_requested && emu->exec.cpu.cycles < target_cycles) {
        /* Check for breakpoint */
        uint32_t pc = emu->exec.cpu.r[ARMV8M_REG_PC];
        if (armv8m_emu_has_breakpoint(emu, pc)) {
            emu->state = EMU_STATE_BREAKPOINT;
            break;
        }

        /* Execute one instruction */
        int result = armv8m_exec_step(&emu->exec);

        if (result == ARMV8M_ERR_BREAKPOINT) {
            emu->state = EMU_STATE_BREAKPOINT;
            break;
        } else if (result == ARMV8M_ERR_HALTED) {
            emu->state = EMU_STATE_HALTED;
            break;
        } else if (result != ARMV8M_OK) {
            emu->state = EMU_STATE_FAULT;
            emu->last_error = result;
            return -(int64_t)result;
        }

        /* Tick peripherals */
        for (int i = 0; i < emu->num_peripherals; i++) {
            if (emu->peripherals[i] && emu->peripherals[i]->vtable.tick) {
                emu->peripherals[i]->vtable.tick(emu->peripherals[i]->context, 1);
            }
        }
    }

    if (emu->stop_requested) {
        emu->state = EMU_STATE_STOPPED;
    }

    return (int64_t)(emu->exec.cpu.cycles - start_cycles);
}

void armv8m_emu_stop(Emulator *emu)
{
    if (emu) {
        emu->stop_requested = true;
    }
}

/*============================================================================
 * State Access API
 *============================================================================*/

uint32_t armv8m_emu_get_reg(const Emulator *emu, int reg)
{
    if (!emu || reg < 0 || reg >= ARMV8M_NUM_REGS) return 0;
    return emu->exec.cpu.r[reg];
}

void armv8m_emu_set_reg(Emulator *emu, int reg, uint32_t value)
{
    if (!emu || reg < 0 || reg >= ARMV8M_NUM_REGS) return;
    emu->exec.cpu.r[reg] = value;
}

uint32_t armv8m_emu_get_pc(const Emulator *emu)
{
    if (!emu) return 0;
    return emu->exec.cpu.r[ARMV8M_REG_PC];
}

void armv8m_emu_set_pc(Emulator *emu, uint32_t value)
{
    if (!emu) return;
    emu->exec.cpu.r[ARMV8M_REG_PC] = value;
}

uint32_t armv8m_emu_get_xpsr(const Emulator *emu)
{
    if (!emu) return 0;
    return emu->exec.cpu.xpsr;
}

void armv8m_emu_set_xpsr(Emulator *emu, uint32_t value)
{
    if (!emu) return;
    emu->exec.cpu.xpsr = value;
}

uint64_t armv8m_emu_get_cycles(const Emulator *emu)
{
    if (!emu) return 0;
    return emu->exec.cpu.cycles;
}

EmuState armv8m_emu_get_state(const Emulator *emu)
{
    if (!emu) return EMU_STATE_STOPPED;
    return emu->state;
}

int armv8m_emu_get_last_error(const Emulator *emu)
{
    if (!emu) return ARMV8M_OK;
    return emu->last_error;
}

/*============================================================================
 * Memory Access API
 *============================================================================*/

uint32_t armv8m_emu_read_mem(const Emulator *emu, uint32_t addr, uint8_t size, bool *fault)
{
    if (!emu) {
        if (fault) *fault = true;
        return 0;
    }

    /* Debug read bypasses MPU (privileged access) */
    return (uint32_t)emu_mem_read((EmuMemorySystem *)&emu->mem, addr, size, true, fault);
}

void armv8m_emu_write_mem(Emulator *emu, uint32_t addr, uint32_t value, uint8_t size, bool *fault)
{
    if (!emu) {
        if (fault) *fault = true;
        return;
    }

    /* Debug write bypasses MPU (privileged access) */
    emu_mem_write(&emu->mem, addr, value, size, true, fault);
}

uint32_t armv8m_emu_read_block(const Emulator *emu, uint32_t addr, uint8_t *data, uint32_t size)
{
    if (!emu || !data) return 0;

    uint32_t read = 0;
    for (uint32_t i = 0; i < size; i++) {
        bool fault = false;
        uint32_t val = armv8m_emu_read_mem(emu, addr + i, 1, &fault);
        if (fault) break;
        data[i] = (uint8_t)val;
        read++;
    }
    return read;
}

uint32_t armv8m_emu_write_block(Emulator *emu, uint32_t addr, const uint8_t *data, uint32_t size)
{
    if (!emu || !data) return 0;

    uint32_t written = 0;
    for (uint32_t i = 0; i < size; i++) {
        bool fault = false;
        armv8m_emu_write_mem(emu, addr + i, data[i], 1, &fault);
        if (fault) break;
        written++;
    }
    return written;
}

/*============================================================================
 * Breakpoint API
 *============================================================================*/

int armv8m_emu_add_breakpoint(Emulator *emu, uint32_t addr)
{
    if (!emu) return ARMV8M_ERR_INVALID_PARAM;

    /* Check if already exists */
    for (int i = 0; i < emu->num_breakpoints; i++) {
        if (emu->breakpoints[i] == addr) {
            return ARMV8M_OK;  /* Already exists */
        }
    }

    /* Check capacity */
    if (emu->num_breakpoints >= EMU_MAX_BREAKPOINTS) {
        return ARMV8M_ERR_INVALID_PARAM;
    }

    emu->breakpoints[emu->num_breakpoints++] = addr;
    return ARMV8M_OK;
}

int armv8m_emu_remove_breakpoint(Emulator *emu, uint32_t addr)
{
    if (!emu) return ARMV8M_ERR_INVALID_PARAM;

    for (int i = 0; i < emu->num_breakpoints; i++) {
        if (emu->breakpoints[i] == addr) {
            /* Shift remaining breakpoints */
            for (int j = i; j < emu->num_breakpoints - 1; j++) {
                emu->breakpoints[j] = emu->breakpoints[j + 1];
            }
            emu->num_breakpoints--;
            return ARMV8M_OK;
        }
    }

    return ARMV8M_OK;  /* Not found is OK */
}

bool armv8m_emu_has_breakpoint(const Emulator *emu, uint32_t addr)
{
    if (!emu) return false;

    for (int i = 0; i < emu->num_breakpoints; i++) {
        if (emu->breakpoints[i] == addr) {
            return true;
        }
    }
    return false;
}

void armv8m_emu_clear_breakpoints(Emulator *emu)
{
    if (emu) {
        emu->num_breakpoints = 0;
    }
}

/*============================================================================
 * Peripheral API
 *============================================================================*/

int armv8m_emu_add_peripheral(Emulator *emu, EmuPeripheral *periph, uint32_t base, uint32_t size)
{
    if (!emu || !periph) return ARMV8M_ERR_INVALID_PARAM;

    if (emu->num_peripherals >= EMU_MAX_PERIPHERALS) {
        return ARMV8M_ERR_INVALID_PARAM;
    }

    /* Configure peripheral */
    periph->base_addr = base;
    periph->size = size;
    periph->emu_ctx = emu;

    /* Add MMIO region */
    int result = emu_mem_add_mmio(&emu->mem, base, size,
                                      periph, periph_mmio_read, periph_mmio_write);
    if (result != ARMV8M_OK) {
        return result;
    }

    emu->peripherals[emu->num_peripherals++] = periph;
    return ARMV8M_OK;
}

/*============================================================================
 * Special Register Access (for GDB)
 *============================================================================*/

uint32_t armv8m_emu_get_special_reg(const Emulator *emu, int reg)
{
    if (!emu) return 0;

    switch (reg) {
        case ARMV8M_SYSREG_PRIMASK: return emu->exec.cpu.primask;
        case ARMV8M_SYSREG_BASEPRI: return emu->exec.cpu.basepri;
        case ARMV8M_SYSREG_FAULTMASK: return emu->exec.cpu.faultmask;
        case ARMV8M_SYSREG_CONTROL: return emu->exec.cpu.control;
        case ARMV8M_SYSREG_MSP: return emu->exec.cpu.sp_main;
        case ARMV8M_SYSREG_PSP: return emu->exec.cpu.sp_process;
        case ARMV8M_SYSREG_MSPLIM: return emu->exec.cpu.msplim;
        case ARMV8M_SYSREG_PSPLIM: return emu->exec.cpu.psplim;
        default: return 0;
    }
}

void armv8m_emu_set_special_reg(Emulator *emu, int reg, uint32_t value)
{
    if (!emu) return;

    switch (reg) {
        case ARMV8M_SYSREG_PRIMASK: emu->exec.cpu.primask = value; break;
        case ARMV8M_SYSREG_BASEPRI: emu->exec.cpu.basepri = value; break;
        case ARMV8M_SYSREG_FAULTMASK: emu->exec.cpu.faultmask = value; break;
        case ARMV8M_SYSREG_CONTROL: emu->exec.cpu.control = value; break;
        case ARMV8M_SYSREG_MSP: emu->exec.cpu.sp_main = value; break;
        case ARMV8M_SYSREG_PSP: emu->exec.cpu.sp_process = value; break;
        case ARMV8M_SYSREG_MSPLIM: emu->exec.cpu.msplim = value; break;
        case ARMV8M_SYSREG_PSPLIM: emu->exec.cpu.psplim = value; break;
        default: break;
    }
}

uint32_t armv8m_emu_get_fpu_reg(const Emulator *emu, int reg)
{
    if (!emu || reg < 0 || reg >= 32) return 0;
    return emu->exec.cpu.s[reg];
}

void armv8m_emu_set_fpu_reg(Emulator *emu, int reg, uint32_t value)
{
    if (!emu || reg < 0 || reg >= 32) return;
    emu->exec.cpu.s[reg] = value;
}

uint32_t armv8m_emu_get_fpscr(const Emulator *emu)
{
    if (!emu) return 0;
    return emu->exec.cpu.fpscr;
}

void armv8m_emu_set_fpscr(Emulator *emu, uint32_t value)
{
    if (!emu) return;
    emu->exec.cpu.fpscr = value;
}
