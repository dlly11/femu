#!/usr/bin/env python3
"""
Emulator verification tests.

This script builds test firmware and runs it through the emulator,
verifying that instructions execute correctly.
"""

import subprocess
import sys
from pathlib import Path

# Add femu package to path
SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "python"))

from femu.emulator import Emulator, EmulatorState


def build_firmware():
    """Build test firmware using make."""
    print("Building test firmware...")
    result = subprocess.run(
        ["make", "-C", str(SCRIPT_DIR), "clean", "all"],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print(f"Build failed:\n{result.stderr}")
        return False
    print("Build successful.")
    return True


def run_test(name: str, elf_path: Path, expected: dict, max_cycles: int = 100000) -> bool:
    """
    Run a test firmware and verify results.

    Args:
        name: Test name for display
        elf_path: Path to ELF file
        expected: Dictionary of {address: expected_value}
        max_cycles: Maximum cycles to execute

    Returns:
        True if test passed, False otherwise
    """
    print(f"\n{'='*60}")
    print(f"Running test: {name}")
    print(f"{'='*60}")

    try:
        emu = Emulator()
        elf = emu.load_elf(elf_path)

        print(f"  Entry: 0x{elf.entry_point:08x}")
        print(f"  Initial SP: 0x{emu.sp:08x}")
        print(f"  Initial PC: 0x{emu.pc:08x}")

        # Run until breakpoint or max cycles
        cycles = emu.run(max_cycles)
        state = emu.state

        print(f"  Executed {cycles:,} cycles")
        print(f"  Final state: {state.name}")
        print(f"  Final PC: 0x{emu.pc:08x}")

        if state == EmulatorState.FAULT:
            print(f"  ERROR: Emulator faulted!")
            return False

        # Verify expected memory values
        all_pass = True
        print(f"\n  Checking expected values:")
        for addr, expected_val in expected.items():
            actual_val = emu.read_mem(addr, 4)
            if actual_val == expected_val:
                print(f"    [0x{addr:08x}] = 0x{actual_val:08x} OK")
            else:
                print(f"    [0x{addr:08x}] = 0x{actual_val:08x} FAIL (expected 0x{expected_val:08x})")
                all_pass = False

        if all_pass:
            print(f"\n  TEST PASSED")
        else:
            print(f"\n  TEST FAILED")

        return all_pass

    except Exception as e:
        print(f"  ERROR: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_simple():
    """Test simple counter loop."""
    return run_test(
        "Simple Counter",
        SCRIPT_DIR / "test_simple.elf",
        {
            0x20000000: 10,           # Counter value
            0x20000004: 0xDEADBEEF,   # Done marker
        },
    )


def test_arithmetic():
    """Test arithmetic operations."""
    return run_test(
        "Arithmetic Operations",
        SCRIPT_DIR / "test_arithmetic.elf",
        {
            0x20000000: 22,           # 15 + 7
            0x20000004: 63,           # 100 - 37
            0x20000008: 42,           # 6 * 7
            0x2000000C: 0x0F,         # 0xFF & 0x0F
            0x20000010: 0xFF,         # 0xF0 | 0x0F
            0x20000014: 0x55,         # 0xAA ^ 0xFF
            0x20000018: 16,           # 1 << 4
            0x2000001C: 16,           # 256 >> 4
            0x20000020: 0xFFFFFFFE,   # -8 >> 2 (sign extended)
            0x20000024: 0xCAFEBABE,   # Done marker
        },
    )


def test_memory():
    """Test memory operations."""
    return run_test(
        "Memory Operations",
        SCRIPT_DIR / "test_memory.elf",
        {
            0x20000100: 0x12345678,   # Word store/load
            0x20000104: 0xAB,         # Byte store/load
            0x20000108: 0xCDEF,       # Halfword store/load
            0x2000010C: 0x11111111,   # STM result 1
            0x20000110: 0x22222222,   # STM result 2
            0x20000114: 0x33333333,   # STM result 3
            0x20000118: 0x11111111,   # LDM verify 1
            0x2000011C: 0x22222222,   # LDM verify 2
            0x20000120: 0x33333333,   # LDM verify 3
            0x20000124: 0xDEADC0DE,   # Done marker
        },
    )


def test_branch():
    """Test branch operations."""
    return run_test(
        "Branch Operations",
        SCRIPT_DIR / "test_branch.elf",
        {
            0x20000000: 1,            # Unconditional branch
            0x20000004: 2,            # BL worked
            0x20000008: 3,            # BEQ worked
            0x2000000C: 4,            # BNE worked
            0x20000010: 5,            # BGT worked
            0x20000014: 6,            # BLT worked
            0x20000018: 7,            # CBZ worked
            0x2000001C: 8,            # CBNZ worked
            0x20000020: 0xB4A2C4ED,   # Done marker
        },
    )


def test_bitfield():
    """Test bit manipulation operations."""
    return run_test(
        "Bit Field Operations",
        SCRIPT_DIR / "test_bitfield.elf",
        {
            0x20000000: 15,           # CLZ(0x00010000) = 15
            0x20000004: 32,           # CLZ(0) = 32
            0x20000008: 0x80000001,   # RBIT(0x80000001)
            0x2000000C: 0x78563412,   # REV(0x12345678)
            0x20000010: 0x34127856,   # REV16(0x12345678)
            0x20000014: 0xFFFF8000,   # REVSH(0x80) sign extended
            0x20000018: 0xFFFF00FF,   # BFC: clear bits 8-15
            0x2000001C: 0x00AB0000,   # BFI: insert 0xAB at bits 16-23
            0x20000020: 0x000000FF,   # UBFX: extract bits 8-15
            0x20000024: 0xFFFFFFFF,   # SBFX: extract 4 bits, sign extend
            0x20000028: 0xBEEFCAFE,   # Done marker
        },
    )


def test_multiply():
    """Test extended multiply operations."""
    return run_test(
        "Multiply Operations",
        SCRIPT_DIR / "test_multiply.elf",
        {
            0x20000000: 17,           # MLA: 3*4+5
            0x20000004: 4,            # MLS: 10-3*2
            0x20000008: 0x10000000,   # UMULL low: 0x100000*0x100 = 0x10000000
            0x2000000C: 0x00000000,   # UMULL high: fits in 32 bits
            0x20000010: 0xFA0A1F00,   # SMULL low: -100*1000000
            0x20000014: 0xFFFFFFFF,   # SMULL high (negative)
            0x20000018: 14,           # UDIV: 100/7
            0x2000001C: 0xFFFFFFF2,   # SDIV: -100/7 = -14
            0x20000020: 0xFFFFFFF2,   # SDIV: 100/-7 = -14
            0x20000024: 0x01000100,   # UMLAL low: 0x100 + 0x1000*0x1000
            0x20000028: 0x00000000,   # UMLAL high
            0x2000002C: 0xCAFED00D,   # Done marker
        },
    )


def test_stack():
    """Test stack operations (PUSH/POP)."""
    return run_test(
        "Stack Operations",
        SCRIPT_DIR / "test_stack.elf",
        {
            0x20000000: 0x12345678,   # Push/pop single
            0x20000004: 0x11111111,   # Push/pop multiple 1
            0x20000008: 0x22222222,   # Push/pop multiple 2
            0x2000000C: 0x33333333,   # Push/pop multiple 3
            0x20000010: 5,            # Fibonacci(5) = 5
            0x20000014: 1,            # SP check (1 = correct)
            0x20000018: 0xDEADFACE,   # Done marker
        },
    )


def test_it_block():
    """Test IT block conditional execution."""
    return run_test(
        "IT Block Operations",
        SCRIPT_DIR / "test_it_block.elf",
        {
            0x20000000: 1,            # IT EQ true
            0x20000004: 0,            # IT EQ false
            0x20000008: 10,           # ITE NE then
            0x2000000C: 20,           # ITE NE else
            0x20000010: 1,            # ITT GT first
            0x20000014: 2,            # ITT GT second
            0x20000018: 1,            # ITEE LT pattern
            0x2000001C: 150,          # IT with arithmetic
            0x20000020: 0xC0FFEE42,   # Done marker
        },
    )


def test_compare():
    """Test comparison and test instructions."""
    return run_test(
        "Compare Operations",
        SCRIPT_DIR / "test_compare.elf",
        {
            0x20000000: 1,            # CMP equal
            0x20000004: 1,            # CMP not equal
            0x20000008: 1,            # CMP unsigned higher
            0x2000000C: 1,            # CMP unsigned lower/same
            0x20000010: 1,            # CMP signed greater
            0x20000014: 1,            # CMP signed less
            0x20000018: 1,            # CMN test
            0x2000001C: 1,            # TST zero result
            0x20000020: 1,            # TST non-zero result
            0x20000024: 1,            # TEQ equal
            0x20000028: 1,            # TEQ not equal
            0x2000002C: 1,            # Negative flag test
            0x20000030: 1,            # Overflow flag test
            0x20000034: 1,            # Carry flag test
            0x20000038: 0xC0FFEE42,   # Done marker
        },
    )


def test_extend():
    """Test sign/zero extension instructions."""
    return run_test(
        "Extension Operations",
        SCRIPT_DIR / "test_extend.elf",
        {
            0x20000000: 0x0000007F,   # SXTB positive
            0x20000004: 0xFFFFFF80,   # SXTB negative
            0x20000008: 0x00007FFF,   # SXTH positive
            0x2000000C: 0xFFFF8000,   # SXTH negative
            0x20000010: 0x000000AB,   # UXTB
            0x20000014: 0x0000ABCD,   # UXTH
            0x20000018: 0xFFFFFFCC,   # SXTB with ROR 8
            0x2000001C: 0x000000AB,   # UXTB with ROR 16
            0x20000020: 0x1000007F,   # SXTAB
            0x20000024: 0x10007FFF,   # SXTAH
            0x20000028: 0x100000FF,   # UXTAB
            0x2000002C: 0x1000FFFF,   # UXTAH
            0x20000030: 0x10000033,   # SXTAB with ROR 8
            0x20000034: 0xC0FFEE42,   # Done marker
        },
    )


def test_load_store_misc():
    """Test load/store variations."""
    return run_test(
        "Load/Store Misc Operations",
        SCRIPT_DIR / "test_load_store_misc.elf",
        {
            0x20000000: 0xDEADBEEF,   # LDRD low
            0x20000004: 0xCAFEBABE,   # LDRD high
            0x20000008: 0x12345678,   # STRD low
            0x2000000C: 0x9ABCDEF0,   # STRD high
            0x20000010: 0x00007FFF,   # LDRSH positive
            0x20000014: 0xFFFF8000,   # LDRSH negative
            0x20000018: 0x0000007F,   # LDRSB positive
            0x2000001C: 0xFFFFFF80,   # LDRSB negative
            0x20000020: 0xAABBCCDD,   # Pre-indexed load
            0x20000024: 0x11223344,   # Post-indexed load
            0x20000028: 0x55667788,   # Pre-indexed store
            0x2000002C: 0x99AABBCC,   # Post-indexed store
            0x20000030: 0xC0FFEE42,   # PC-relative LDR
            0x20000034: 0x0000BEEF,   # LDRH result
            0x2000003C: 0xC0FFEE42,   # Done marker
        },
    )


def test_move():
    """Test move instructions."""
    return run_test(
        "Move Operations",
        SCRIPT_DIR / "test_move.elf",
        {
            0x20000000: 0x000000FF,   # MOV immediate small
            0x20000004: 0x12000000,   # MOV immediate large
            0x20000008: 0xFFFFFF00,   # MVN immediate
            0x2000000C: 0xDEADBEEF,   # MOV register
            0x20000010: 0x0000ABCD,   # MOVW 16-bit
            0x20000014: 0x12340000,   # MOVT set top half
            0x20000018: 0x12345678,   # MOVW + MOVT combo
            0x2000001C: 0x000000F0,   # MOV with LSL
            0x20000020: 0x0000000F,   # MOV with LSR
            0x20000024: 0xF8000000,   # MOV with ASR
            0x20000028: 0x78123456,   # MOV with ROR
            0x2000002C: 0xFFFF0000,   # MVN register
            0x20000030: 1,            # MOV sets flags test
            0x20000034: 0xC0FFEE42,   # Done marker
        },
    )


def test_shift_rotate():
    """Test shift and rotate edge cases."""
    return run_test(
        "Shift/Rotate Operations",
        SCRIPT_DIR / "test_shift_rotate.elf",
        {
            0x20000000: 0x12345678,   # LSL by 0
            0x20000004: 0x00000080,   # LSL by 1 (64->128)
            0x20000008: 0x80000000,   # LSL by 31
            0x2000000C: 0x12345678,   # LSR by 0
            0x20000010: 0x00000040,   # LSR by 1 (128->64)
            0x20000014: 0x00000001,   # LSR by 31
            0x20000018: 0x12345678,   # ASR by 0
            0x2000001C: 0x00000040,   # ASR by 1 positive
            0x20000020: 0xC0000000,   # ASR by 1 negative
            0x20000024: 0xFFFFFFFF,   # ASR by 31 negative
            0x20000028: 0x12345678,   # ROR by 0
            0x2000002C: 0x78123456,   # ROR by 8
            0x20000030: 0x56781234,   # ROR by 16
            0x20000034: 0x80000001,   # RRX
            0x20000038: 1,            # LSL sets carry
            0x2000003C: 0x23456780,   # Variable shift
            0x20000040: 0xC0FFEE42,   # Done marker
        },
    )


def test_saturate():
    """Test saturating arithmetic instructions."""
    return run_test(
        "Saturating Operations",
        SCRIPT_DIR / "test_saturate.elf",
        {
            0x20000000: 100,          # SSAT no saturation
            0x20000004: 127,          # SSAT positive overflow
            0x20000008: 0xFFFFFF80,   # SSAT negative overflow (-128)
            0x2000000C: 100,          # USAT no saturation
            0x20000010: 255,          # USAT positive overflow
            0x20000014: 0,            # USAT negative to zero
            0x20000018: 1,            # Q flag set test
            0x2000001C: 127,          # SSAT with shift
            0x20000020: 255,          # USAT with shift
            0x20000024: 127,          # SSAT to 8 bits boundary
            0x20000028: 255,          # USAT to 8 bits boundary
            0x2000002C: 0xC0FFEE42,   # Done marker
        },
    )


def test_exclusive():
    """Test exclusive access instructions."""
    return run_test(
        "Exclusive Access Operations",
        SCRIPT_DIR / "test_exclusive.elf",
        {
            0x20000000: 0xDEADBEEF,   # LDREX result
            0x20000004: 0,            # STREX success
            0x20000008: 0xCAFEBABE,   # Value after STREX
            0x2000000C: 1,            # STREX after CLREX (fail)
            0x20000010: 0x42,         # LDREXB result
            0x20000014: 0,            # STREXB success
            0x20000018: 0x1234,       # LDREXH result
            0x2000001C: 0,            # STREXH success
            0x20000020: 11,           # Atomic increment (10+1)
            0x20000024: 0xC0FFEE42,   # Done marker
        },
    )


def test_table_branch():
    """Test table branch instructions."""
    return run_test(
        "Table Branch Operations",
        SCRIPT_DIR / "test_table_branch.elf",
        {
            0x20000000: 10,           # TBB case 0
            0x20000004: 20,           # TBB case 1
            0x20000008: 30,           # TBB case 2
            0x2000000C: 100,          # TBH case 0
            0x20000010: 200,          # TBH case 1
            0x20000014: 300,          # TBH case 2
            0x20000018: 60,           # TBB loop (10+20+30)
            0x2000001C: 0xC0FFEE42,   # Done marker
        },
    )


def test_adr():
    """Test PC-relative addressing instructions."""
    return run_test(
        "ADR Operations",
        SCRIPT_DIR / "test_adr.elf",
        {
            # ADR results are code addresses, verify marker and string length
            0x20000018: 13,           # String length test
            0x2000001C: 0xC0FFEE42,   # Done marker
        },
    )


def test_system():
    """Test system instructions."""
    return run_test(
        "System Operations",
        SCRIPT_DIR / "test_system.elf",
        {
            0x20000000: 0,            # Initial PRIMASK
            0x20000004: 1,            # PRIMASK after set
            0x20000008: 0,            # PRIMASK after clear
            0x2000000C: 0,            # Initial BASEPRI
            0x20000010: 0x40,         # BASEPRI after set
            0x20000018: 0x20000F00,   # PSP value
            0x20000020: 1,            # NOP executed
            0x20000024: 1,            # DMB executed
            0x20000028: 1,            # DSB executed
            0x2000002C: 1,            # ISB executed
            0x20000030: 0xC0FFEE42,   # Done marker
        },
    )


def test_exception():
    """Test exception handling."""
    return run_test(
        "Exception Handling",
        SCRIPT_DIR / "test_exception.elf",
        {
            0x20000000: 1,            # SVC handler executed
            0x20000004: 42,           # SVC number extracted
            0x20000008: 0x1234,       # Return value from SVC
            0x2000000C: 0x10,         # R0 from stack frame
            0x20000010: 0x11,         # R1 from stack frame
            0x20000014: 0x12,         # R2 from stack frame
            0x20000018: 0x13,         # R3 from stack frame
            0x2000001C: 0x1C,         # R12 from stack frame
            0x2000002C: 1,            # Second SVC executed
            0x20000030: 7,            # Second SVC number
            0x20000038: 0xC0FFEE42,   # Done marker
        },
    )


def test_misc_alu():
    """Test miscellaneous ALU instructions."""
    return run_test(
        "Misc ALU Operations",
        SCRIPT_DIR / "test_misc_alu.elf",
        {
            0x20000000: 0xFFFF0000,   # BIC result
            0x20000004: 0xFFFF00FF,   # BIC with shifted operand
            0x20000008: 0xFFFF00FF,   # ORN result
            0x2000000C: 150,          # ADC without carry
            0x20000010: 151,          # ADC with carry
            0x20000014: 70,           # SBC without borrow
            0x20000018: 69,           # SBC with borrow
            0x2000001C: 70,           # RSB result
            0x20000020: 0xFFFFFFD6,   # RSB immediate (-42)
            0x20000024: 0x00000001,   # 64-bit add low
            0x20000028: 0x00000002,   # 64-bit add high
            0x2000002C: 0xFFFFFFFF,   # 64-bit sub low
            0x20000030: 0x00000001,   # 64-bit sub high
            0x20000034: 0xFFFFFF00,   # BIC immediate
            0x20000038: 0xC0FFEE42,   # Done marker
        },
    )


def main():
    """Run all tests."""
    print("FEMU Emulator Verification Tests")
    print("=" * 60)

    # Build firmware
    if not build_firmware():
        print("\nFailed to build firmware. Exiting.")
        return 1

    # Run tests
    tests = [
        test_simple,
        test_arithmetic,
        test_memory,
        test_branch,
        test_bitfield,
        test_multiply,
        test_stack,
        test_it_block,
        test_compare,
        test_extend,
        test_load_store_misc,
        test_move,
        test_shift_rotate,
        test_saturate,
        test_exclusive,
        test_table_branch,
        test_adr,
        test_system,
        test_exception,
        test_misc_alu,
    ]

    results = []
    for test_func in tests:
        try:
            passed = test_func()
            results.append((test_func.__name__, passed))
        except Exception as e:
            print(f"\nTest {test_func.__name__} crashed: {e}")
            results.append((test_func.__name__, False))

    # Summary
    print("\n" + "=" * 60)
    print("SUMMARY")
    print("=" * 60)

    passed = sum(1 for _, p in results if p)
    failed = len(results) - passed

    for name, result in results:
        status = "PASS" if result else "FAIL"
        print(f"  {name}: {status}")

    print(f"\nTotal: {passed}/{len(results)} passed")

    if failed > 0:
        print("\nSome tests FAILED!")
        return 1
    else:
        print("\nAll tests PASSED!")
        return 0


if __name__ == "__main__":
    sys.exit(main())
