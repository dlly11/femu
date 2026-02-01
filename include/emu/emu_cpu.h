/**
 * @file emu_cpu.h
 * @brief Abstract CPU interface for multi-architecture support
 *
 * This file defines the abstract CPU interface using a vtable pattern.
 * Architecture-specific implementations provide concrete vtable functions.
 */

#ifndef EMU_CPU_H
#define EMU_CPU_H

#include "emu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Forward Declarations
 *============================================================================*/

typedef struct EmuCPU EmuCPU;
typedef struct EmuCPUVTable EmuCPUVTable;

/*============================================================================
 * CPU Virtual Table
 *============================================================================*/

/**
 * Virtual function table for CPU operations.
 *
 * All function pointers must be non-NULL for a valid CPU implementation.
 */
struct EmuCPUVTable {
    /**
     * Destroy CPU and free resources.
     *
     * @param cpu       CPU instance
     */
    void (*destroy)(EmuCPU *cpu);

    /**
     * Reset CPU to initial state.
     *
     * @param cpu           CPU instance
     * @param entry_point   Entry point address (architecture-specific meaning)
     */
    void (*reset)(EmuCPU *cpu, uint64_t entry_point);

    /**
     * Get CPU architecture information.
     *
     * @param cpu       CPU instance
     * @return          Pointer to static CPU info structure
     */
    const EmuCPUInfo* (*get_info)(const EmuCPU *cpu);

    /**
     * Get general purpose register value.
     *
     * @param cpu       CPU instance
     * @param reg       Register number (0 to num_gp_regs-1)
     * @return          Register value (zero-extended to 64 bits)
     */
    uint64_t (*get_reg)(const EmuCPU *cpu, int reg);

    /**
     * Set general purpose register value.
     *
     * @param cpu       CPU instance
     * @param reg       Register number
     * @param value     New value
     */
    void (*set_reg)(EmuCPU *cpu, int reg, uint64_t value);

    /**
     * Get program counter.
     *
     * @param cpu       CPU instance
     * @return          PC value
     */
    uint64_t (*get_pc)(const EmuCPU *cpu);

    /**
     * Set program counter.
     *
     * @param cpu       CPU instance
     * @param value     New PC value
     */
    void (*set_pc)(EmuCPU *cpu, uint64_t value);

    /**
     * Get status/flags register.
     *
     * @param cpu       CPU instance
     * @return          Status register value (architecture-specific format)
     */
    uint64_t (*get_status)(const EmuCPU *cpu);

    /**
     * Set status/flags register.
     *
     * @param cpu       CPU instance
     * @param value     New status value
     */
    void (*set_status)(EmuCPU *cpu, uint64_t value);

    /**
     * Get cycle counter.
     *
     * @param cpu       CPU instance
     * @return          Total cycles executed
     */
    uint64_t (*get_cycles)(const EmuCPU *cpu);

    /**
     * Check if CPU is halted.
     *
     * @param cpu       CPU instance
     * @return          true if halted
     */
    bool (*is_halted)(const EmuCPU *cpu);

    /**
     * Get special register by ID.
     *
     * @param cpu       CPU instance
     * @param reg_id    Architecture-specific register ID
     * @return          Register value
     */
    uint64_t (*get_special_reg)(const EmuCPU *cpu, int reg_id);

    /**
     * Set special register by ID.
     *
     * @param cpu       CPU instance
     * @param reg_id    Architecture-specific register ID
     * @param value     New value
     */
    void (*set_special_reg)(EmuCPU *cpu, int reg_id, uint64_t value);
};

/*============================================================================
 * CPU Structure
 *============================================================================*/

/**
 * Abstract CPU instance.
 *
 * Architecture implementations embed this as the first member of their
 * concrete CPU structure, allowing safe casting.
 */
struct EmuCPU {
    const EmuCPUVTable *vtable; /**< Virtual function table */
    void *arch_state;           /**< Architecture-specific state (optional) */
};

/*============================================================================
 * Convenience Macros
 *============================================================================*/

/**
 * Call a CPU vtable method.
 */
#define EMU_CPU_CALL(cpu, method, ...) \
    ((cpu)->vtable->method((cpu), ##__VA_ARGS__))

/**
 * Initialize EmuCPU base structure.
 */
#define EMU_CPU_INIT(cpu, vtbl, state) do { \
    (cpu)->vtable = (vtbl);                  \
    (cpu)->arch_state = (state);             \
} while(0)

/*============================================================================
 * Inline Convenience Functions
 *============================================================================*/

static inline void emu_cpu_destroy(EmuCPU *cpu) {
    if (cpu && cpu->vtable && cpu->vtable->destroy) {
        cpu->vtable->destroy(cpu);
    }
}

static inline void emu_cpu_reset(EmuCPU *cpu, uint64_t entry_point) {
    if (cpu && cpu->vtable) {
        cpu->vtable->reset(cpu, entry_point);
    }
}

static inline const EmuCPUInfo* emu_cpu_get_info(const EmuCPU *cpu) {
    return (cpu && cpu->vtable) ? cpu->vtable->get_info(cpu) : NULL;
}

static inline uint64_t emu_cpu_get_reg(const EmuCPU *cpu, int reg) {
    return (cpu && cpu->vtable) ? cpu->vtable->get_reg(cpu, reg) : 0;
}

static inline void emu_cpu_set_reg(EmuCPU *cpu, int reg, uint64_t value) {
    if (cpu && cpu->vtable) {
        cpu->vtable->set_reg(cpu, reg, value);
    }
}

static inline uint64_t emu_cpu_get_pc(const EmuCPU *cpu) {
    return (cpu && cpu->vtable) ? cpu->vtable->get_pc(cpu) : 0;
}

static inline void emu_cpu_set_pc(EmuCPU *cpu, uint64_t value) {
    if (cpu && cpu->vtable) {
        cpu->vtable->set_pc(cpu, value);
    }
}

static inline uint64_t emu_cpu_get_status(const EmuCPU *cpu) {
    return (cpu && cpu->vtable) ? cpu->vtable->get_status(cpu) : 0;
}

static inline void emu_cpu_set_status(EmuCPU *cpu, uint64_t value) {
    if (cpu && cpu->vtable) {
        cpu->vtable->set_status(cpu, value);
    }
}

static inline uint64_t emu_cpu_get_cycles(const EmuCPU *cpu) {
    return (cpu && cpu->vtable) ? cpu->vtable->get_cycles(cpu) : 0;
}

static inline bool emu_cpu_is_halted(const EmuCPU *cpu) {
    return (cpu && cpu->vtable) ? cpu->vtable->is_halted(cpu) : true;
}

#ifdef __cplusplus
}
#endif

#endif /* EMU_CPU_H */
