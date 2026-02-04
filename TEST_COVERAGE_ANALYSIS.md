# FEMU Test Coverage Analysis

## Executive Summary

FEMU has a multi-layered test infrastructure with **1,069+ C/C++ unit test functions**,
**46+ firmware integration tests**, and **20+ Python tests** across ~14,900 lines of test
code. While the executor and decoder modules have reasonable coverage of the "happy path"
for core instruction types, there are significant gaps in **error handling**, **edge cases**,
**optimization subsystems**, **Python bindings**, and **cross-module integration**.

This document identifies concrete areas where additional tests would improve reliability
and catch regressions.

---

## Current Test Infrastructure

| Layer | Framework | Files | Approx. Functions |
|-------|-----------|-------|-------------------|
| C/C++ Unit Tests | CppUTest | 24 | 1,069 |
| Firmware Integration | pytest + ARM assembly | 46+ .S files, 8 .c files | 44+ |
| Python Unit Tests | pytest | 3 | 20+ |
| **Total** | | **~81 files** | **~1,133** |

---

## Gap Analysis by Module

### 1. Instruction Cache (`icache.c`) — **0% Coverage**

The instruction cache is a performance-critical subsystem with **zero dedicated unit
tests**. All functions are untested:

| Function | Risk |
|----------|------|
| `armv8m_icache_init()` | Initialization correctness |
| `armv8m_icache_lookup()` | Cache hit/miss logic, generation validation |
| `armv8m_icache_insert()` | Index collision, entry replacement |
| `armv8m_icache_invalidate()` | Generation counter increment, wrap-around to 1 |
| `armv8m_icache_invalidate_range()` | Small vs. large range threshold (64 bytes), partial overlap |
| `armv8m_icache_get_stats()` / `reset_stats()` | Statistics tracking accuracy |

**Recommended tests:**
- Insert and lookup round-trip for single entries
- Cache collision (two different PCs mapping to the same index)
- Full invalidation resets all entries
- Range invalidation for overlapping, non-overlapping, and boundary-crossing ranges
- Generation counter wrap-around behavior
- Hit/miss counters accuracy

---

### 2. Basic Block Cache (`blocks.c`) — **0% Coverage**

The block cache is the primary execution optimization with **zero unit tests**:

| Function | Risk |
|----------|------|
| `armv8m_blocks_init()` | Initialization |
| `armv8m_blocks_get()` / `build_block()` | Block construction, terminator detection |
| `armv8m_blocks_link()` / `get_next()` | Block chaining correctness |
| `armv8m_blocks_invalidate()` | Link clearing on invalidation |
| `armv8m_blocks_invalidate_range()` | Overlap detection, partial invalidation |
| `armv8m_blocks_is_terminator()` | Correct classification of block-ending instructions |
| `armv8m_blocks_get_stats()` / `reset_stats()` | Statistics tracking |

**Recommended tests:**
- Build a block from a known instruction sequence and verify contents
- Terminator detection for all terminating instruction types (branch, call, return, IT, system)
- Block linking and link-following correctness
- Invalidation after code modification (self-modifying code scenario)
- Block statistics (hits, misses, build count)
- Maximum block size (32 instructions) enforcement

---

### 3. VFP/FPU Decoder (`decode_thumb32_vfp.c`) — **0% Decoder-Level Coverage**

The entire VFP decoder file (~340 lines) has no decoder-level unit tests. While executor
tests exercise FPU instructions end-to-end, the decoder itself is not validated in
isolation:

- `VLDR` / `VSTR` — float load/store decoding
- `VMOV` — all register/immediate variants
- `VADD` / `VSUB` / `VMUL` / `VDIV` — arithmetic decoding
- `VCMP` / `VCMPE` — comparison decoding
- `VCVT` — conversion instruction decoding
- `VPUSH` / `VPOP` / `VLDM` / `VSTM` — multiple load/store
- `VFMA` / `VFMS` / `VNMLA` / `VNMLS` — fused multiply-accumulate

**Recommended tests:**
- Decode each VFP instruction type and verify `DecodedInsn` fields
- Single vs. double precision flag decoding
- Register field extraction for all operands
- Immediate offset calculation

---

### 4. DSP Decoder (`decode_thumb32_dsp.c`) — **~5% Coverage**

Only a generic parallel instruction test exists. Missing tests for:

- **Parallel add/sub**: SADD16, SADD8, SSUB16, SSUB8, UADD16, UADD8, USUB16, USUB8
- **Halving variants**: SHADD16, SHSUB16, UHADD16, UHSUB16
- **Saturating variants**: QADD16, QSUB16, QADD8, QSUB8, UQADD16, UQSUB16
- **Exchange variants**: SASX, SSAX, UASX, USAX (and their halving/saturating forms)
- **Saturating arithmetic**: QADD, QSUB, QDADD, QDSUB
- **Miscellaneous ops**: SEL, CLZ, REV, REV16, RBIT, REVSH

---

### 5. Multiply Decoder (`decode_thumb32_multiply.c`) — **~30% Coverage**

Missing tests for DSP halfword multiply family:

- `SMULBB` / `SMULBT` / `SMULTB` / `SMULTT` (16 variants)
- `SMUAD` / `SMUADX` / `SMUSD` / `SMUSDX`
- `SMULWB` / `SMULWT` / `SMLAWB` / `SMLAWT`
- `SMLALBB` / `SMLALBT` / `SMLALTB` / `SMLALTT`
- `SMLALD` / `SMLALDX` / `SMLSLD` / `SMLSLDX`
- `SMMUL` / `SMMULR` / `SMMLA` / `SMMLAR`
- `USAD8` / `USADA8`

---

### 6. Exception Handling (`exec_exception.c`) — **~25% Coverage**

Exception entry and return are among the most complex parts of ARMv8-M. Major gaps:

| Area | Gap |
|------|-----|
| FPU context during exceptions | Lazy FPU stacking (FPCCR.LSPACT), eager stacking, extended frames |
| TrustZone exceptions | Security state switching, banked register handling, S/ES bits in EXC_RETURN |
| Exception return with FPU | FPU context restoration, FPCCR.LSPACT clearing |
| Tail-chaining | Returning from one exception directly into another |
| Stack overflow escalation | Nested stack overflow during entry → HardFault → lockup |
| EXC_RETURN validation | PSP return in Handler mode, secure-to-nonsecure return, DCRS bit |
| IT state restoration | xPSR[26:25] and [15:10] on exception return |

**Recommended tests:**
- Exception entry with FPU enabled (verify extended frame: 104 bytes)
- Lazy FPU stacking trigger on first FPU instruction after context switch
- Tail-chaining: pending higher-priority exception during return
- Stack overflow during exception entry with escalation to HardFault
- Invalid EXC_RETURN values and their handling

---

### 7. System Instructions (`exec_system.c`) — **~40% Coverage**

| Area | Gap |
|------|-----|
| Privilege enforcement | Unprivileged access to BASEPRI, FAULTMASK, PRIMASK should return 0 |
| TrustZone banked registers | MSR/MRS for MSP_NS, PSP_NS from secure state |
| BASEPRI_MAX | Conditional write: only writes if new value < current |
| FAULTMASK in Thread mode | Should be ignored |
| CONTROL.SPSEL in Handler | Should be ignored |
| TT instruction | SAU region matching, MPU integration (marked TODO in source) |
| CPS privilege checks | Unprivileged CPS should be ignored |
| Hint instructions | BKPT, WFE without event, SEVL |
| Barrier semantics | DSB/DMB/ISB have no functional verification |

---

### 8. Execution Control Functions — **~20% Coverage**

The main execution loop functions have minimal testing:

- `armv8m_exec_run()` — Continuous execution with cycle limits
- `armv8m_exec_run_blocks()` — Block-based execution loop
- `armv8m_exec_run_threaded()` — Threaded interpretation
- `armv8m_exec_run_linked()` — Linked block execution
- `armv8m_exec_block_threaded()` — Threaded block execution
- Sleeping and halted state handling
- Cycle limit enforcement

---

### 9. Lazy Flag Evaluation — **No Dedicated Tests**

The lazy flag system is tested indirectly through instruction tests, but the
materialization logic itself lacks targeted tests:

- `LAZY_OP_ADD` overflow/carry at boundaries (e.g., `0x80000000 + 0x80000000`)
- `LAZY_OP_SUB` borrow edge cases
- `LAZY_OP_LOGIC` shifter carry preservation
- Flag materialization after multiple deferred operations
- Interaction with IT block condition evaluation

---

### 10. GDB Server (`gdb_server.py`) — **0% Functional Coverage**

The GDB RSP server has **no functional tests** at all:

- Packet parsing (`$...#xx` format)
- Register read/write commands (`g`, `G`, `p`, `P`)
- Memory read/write commands (`m`, `M`)
- Breakpoint set/clear (`Z0`/`z0`)
- Watchpoint set/clear (`Z2`/`z2`, `Z3`/`z3`, `Z4`/`z4`)
- Step/continue (`s`, `c`)
- Halt reason (`?`)
- Binary data encoding/decoding
- Error recovery and malformed packet handling
- Connection lifecycle (connect, interrupt, disconnect)

---

### 11. Python Emulator Bindings (`emulator.py`) — **~10% Coverage**

Only import-level and constant verification tests exist. Missing:

- `create_emulator()` factory function
- `get_supported_architectures()`
- Emulator lifecycle: init → load firmware → step → read registers → cleanup
- Error handling for invalid architectures
- Memory read/write through Python API
- Breakpoint management through Python API
- Peripheral registration through Python API

---

### 12. Emulator Integration Layer — **Partial Coverage**

The high-level emulator API (`armv8m_emulator.c`) tests cover setup but not execution:

| Tested | Not Tested |
|--------|------------|
| Memory region setup (flash, RAM) | `armv8m_emu_step()` instruction execution |
| Breakpoint add/remove | `armv8m_emu_run()` batch execution |
| Watchpoint add/remove | Watchpoint triggering during access |
| Peripheral registration | Peripheral tick/reset/IRQ callbacks |
| Basic configuration | MPU + memory integration |
| | Multiple peripheral interaction |
| | Error propagation from sub-modules |

---

### 13. Load/Store Decoder — **~40% Coverage**

Missing decoder tests for:

- Load-acquire / Store-release: `LDA`, `LDAB`, `LDAH`, `STL`, `STLB`, `STLH`
- Acquire-exclusive: `LDAEX`, `LDAEXB`, `LDAEXH`
- Release-exclusive: `STLEX`, `STLEXB`, `STLEXH`
- `CLREX` (clear exclusive monitor)
- Unprivileged access variants: `LDRT`, `STRT`
- All P/U/W (pre/post-index, up/down, writeback) combinations for T3 encoding
- TrustZone instructions: `SG`, `BXNS`, `BLXNS`

---

### 14. Memory Module — **Moderate Coverage with Edge Case Gaps**

| Area | Gap |
|------|-----|
| Multiple overlapping regions | Priority/selection behavior |
| Page table rebuild after region changes | Invalidation correctness |
| Region at address space boundary | Near-UINT32_MAX access |
| Page-boundary-crossing access | Single access spanning two pages |
| Capacity exhaustion | Adding regions until limit reached |
| `emu_mem_free_page_table()` | Cleanup function untested |
| `emu_mem_invalidate_page_table()` | Cache invalidation untested |

---

### 15. NVIC Module — **Moderate Coverage with Priority Gaps**

| Area | Gap |
|------|-----|
| PRIGROUP values 0-7 systematically | Priority grouping edge cases |
| 3+ IRQs with identical priority | Sub-priority ordering |
| HardFault preemption with PRIMASK/BASEPRI | Complex masking interaction |
| External IRQ beyond `num_irqs` | Out-of-bounds handling |
| Multiple system handlers pending simultaneously | MemFault + BusFault + UsageFault |
| PRIGROUP change mid-execution | Priority recalculation |
| Reserved exception numbers (8, 9, 10, 13) | Should not be selectable |

---

### 16. MPU Module — **Moderate Coverage with Enforcement Gaps**

| Area | Gap |
|------|-----|
| Instruction fetch from non-code regions | Default map fetch restrictions |
| 3+ overlapping regions | Complex priority scenario |
| All 4 AP modes thoroughly | Permission matrix not fully covered |
| All 8 MAIR attribute indices | Only 0-3 tested |
| Address wraparound at UINT32_MAX | Region boundary conditions |
| XN enforcement specifics | Execute-Never during data access should NOT block |

---

## Priority Ranking

### P0 — Critical (zero coverage, high-impact subsystems)

1. **Instruction cache** (`icache.c`) — Performance-critical, 0% coverage
2. **Block cache** (`blocks.c`) — Primary optimization path, 0% coverage
3. **GDB server** (`gdb_server.py`) — User-facing feature, 0% functional coverage
4. **VFP decoder** (`decode_thumb32_vfp.c`) — Entire file untested at decoder level

### P1 — High (partial coverage, complex/security-sensitive)

5. **Exception handling with FPU** — Lazy stacking, extended frames untested
6. **Exception tail-chaining and escalation** — Stack overflow → HardFault → lockup
7. **Emulator execution API** — `emu_step()` / `emu_run()` not tested
8. **TrustZone instruction decoding** — SG, BXNS, BLXNS, TT decoder tests
9. **System register privilege enforcement** — Security-relevant, minimal tests
10. **Python emulator API** — Functional tests for `create_emulator()` lifecycle

### P2 — Medium (edge cases in reasonably-covered modules)

11. **DSP decoder coverage** — Parallel add/sub, exchange, saturating ops
12. **Halfword multiply decoder** — 20+ DSP multiply variants
13. **Lazy flag materialization** — Targeted boundary tests
14. **Execution loop functions** — `exec_run`, `exec_run_blocks`, cycle limits
15. **Load-acquire / store-release decoder** — ARMv8-M-specific instructions
16. **Memory module edge cases** — Overlapping regions, page boundaries
17. **NVIC priority grouping** — Systematic PRIGROUP testing

### P3 — Low (completeness, robustness)

18. **All 16 condition codes** in conditional branch decoder tests
19. **Thumb16 edge cases** — High register operations, all shift amounts
20. **Barrier instruction semantics** — DSB/DMB/ISB functional verification
21. **MPU attribute indices** — Full MAIR coverage
22. **Peripheral lifecycle callbacks** — tick, reset, IRQ
23. **Firmware test harness error reporting** — Better failure diagnostics

---

## Recommendations

### Quick Wins (high value, moderate effort)

1. **Add icache unit tests** — The API is simple (init/lookup/insert/invalidate);
   a single test file with ~200 lines could cover all functions and edge cases.

2. **Add block cache unit tests** — Test `is_terminator()`, build/get/link/invalidate
   with known instruction sequences.

3. **Add VFP decoder tests** — Encode known VFP instructions, decode them, and verify
   `DecodedInsn` fields match expected values.

4. **Add GDB server packet tests** — Unit test the packet
   parsing/generation functions without requiring a live emulator connection.

### Medium Effort

5. **Exception handling stress tests** — Create firmware tests that trigger nested
   exceptions, FPU context switches during exceptions, and stack overflow escalation.

6. **Python API integration tests** — Test the full lifecycle:
   `create_emulator()` → `load()` → `step()` → `read_register()` → cleanup.

7. **Execution loop tests** — Test `exec_run()` with cycle limits, breakpoints,
   and halt conditions.

### Structural Improvements

8. **Enable coverage reporting in CI** — Add `ENABLE_COVERAGE=ON` build and
   `gcovr` HTML report generation to the test workflow. This provides ongoing
   visibility into coverage trends.

9. **Add a test for each new instruction** — Establish a policy that every new
   instruction implementation includes both decoder and executor unit tests.

10. **Fuzz testing** — The decoder is an excellent candidate for fuzz testing:
    feed random 16/32-bit values and verify the decoder doesn't crash or produce
    invalid outputs.
