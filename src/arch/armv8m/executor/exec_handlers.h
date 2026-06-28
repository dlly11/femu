/**
 * @file exec_handlers.h
 * @brief Internal prototypes for the ARMv8-M instruction handlers.
 *
 * The dispatcher (executor.c) and each handler translation unit
 * (exec_data_proc.c, exec_load_store.c, exec_branch.c, exec_system.c,
 * exec_fpu.c) share these declarations so the definitions are checked against a
 * visible prototype and the dispatch table cannot drift from the handlers.
 */
#ifndef ARMV8M_EXEC_HANDLERS_H
#define ARMV8M_EXEC_HANDLERS_H

#include "arch/armv8m/armv8m_decoder.h"
#include "arch/armv8m/armv8m_executor.h"

/* Data processing (exec_data_proc.c) */
int exec_data_proc_imm(Executor *exec, const DecodedInsn *insn);
int exec_data_proc_reg(Executor *exec, const DecodedInsn *insn);
int exec_data_proc_shifted(Executor *exec, const DecodedInsn *insn);
int exec_multiply(Executor *exec, const DecodedInsn *insn);
int exec_divide(Executor *exec, const DecodedInsn *insn);
int exec_extend(Executor *exec, const DecodedInsn *insn);
int exec_bitfield(Executor *exec, const DecodedInsn *insn);
int exec_saturate(Executor *exec, const DecodedInsn *insn);
int exec_sat_arith(Executor *exec, const DecodedInsn *insn);
int exec_parallel(Executor *exec, const DecodedInsn *insn);
int exec_pack(Executor *exec, const DecodedInsn *insn);

/* Load/store (exec_load_store.c) */
int exec_load_imm(Executor *exec, const DecodedInsn *insn);
int exec_load_reg(Executor *exec, const DecodedInsn *insn);
int exec_load_literal(Executor *exec, const DecodedInsn *insn);
int exec_store_imm(Executor *exec, const DecodedInsn *insn);
int exec_store_reg(Executor *exec, const DecodedInsn *insn);
int exec_load_multiple(Executor *exec, const DecodedInsn *insn);
int exec_store_multiple(Executor *exec, const DecodedInsn *insn);
int exec_load_exclusive(Executor *exec, const DecodedInsn *insn);
int exec_store_exclusive(Executor *exec, const DecodedInsn *insn);
int exec_clear_exclusive(Executor *exec, const DecodedInsn *insn);
int exec_load_acquire(Executor *exec, const DecodedInsn *insn);
int exec_store_release(Executor *exec, const DecodedInsn *insn);

/* Branch (exec_branch.c) */
int exec_branch(Executor *exec, const DecodedInsn *insn);
int exec_branch_link(Executor *exec, const DecodedInsn *insn);
int exec_branch_exchange(Executor *exec, const DecodedInsn *insn);
int exec_branch_link_exchange(Executor *exec, const DecodedInsn *insn);
int exec_compare_branch(Executor *exec, const DecodedInsn *insn);
int exec_table_branch(Executor *exec, const DecodedInsn *insn);
int exec_sg(Executor *exec, const DecodedInsn *insn);
int exec_bxns(Executor *exec, const DecodedInsn *insn);
int exec_blxns(Executor *exec, const DecodedInsn *insn);

/* System (exec_system.c) */
int exec_svc(Executor *exec, const DecodedInsn *insn);
int exec_mrs(Executor *exec, const DecodedInsn *insn);
int exec_msr(Executor *exec, const DecodedInsn *insn);
int exec_cps(Executor *exec, const DecodedInsn *insn);
int exec_barrier(Executor *exec, const DecodedInsn *insn);
int exec_hint(Executor *exec, const DecodedInsn *insn);
int exec_it(Executor *exec, const DecodedInsn *insn);
int exec_mcr(Executor *exec, const DecodedInsn *insn);
int exec_mrc(Executor *exec, const DecodedInsn *insn);
int exec_tt(Executor *exec, const DecodedInsn *insn);

/* FPU (exec_fpu.c) */
int exec_fpu_load(Executor *exec, const DecodedInsn *insn);
int exec_fpu_store(Executor *exec, const DecodedInsn *insn);
int exec_fpu_move(Executor *exec, const DecodedInsn *insn);
int exec_fpu_arith(Executor *exec, const DecodedInsn *insn);
int exec_fpu_cmp(Executor *exec, const DecodedInsn *insn);
int exec_fpu_cvt(Executor *exec, const DecodedInsn *insn);
int exec_fpu_multi(Executor *exec, const DecodedInsn *insn);

/* Security check shared by the dispatcher and branch/system handlers
 * (defined in executor.c). */
SecurityAttr armv8m_check_security(const Executor *exec, uint32_t addr);

#endif /* ARMV8M_EXEC_HANDLERS_H */
