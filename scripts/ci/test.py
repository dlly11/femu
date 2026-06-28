#!/usr/bin/env python3
"""Run all tests."""
import os
import subprocess
import sys
from pathlib import Path

# Add the python directory to the path for imports
SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "python"))
os.chdir(PROJECT_ROOT)


def find_asan_library() -> str | None:
    """Find the ASan runtime library for LD_PRELOAD.

    Detects which compiler built the shared library and uses the matching ASan runtime.
    """
    lib_path = PROJECT_ROOT / "build" / "src" / "arch" / "armv8m" / "libarmv8m_emulator.so"
    if not lib_path.exists():
        return None

    # Check which compiler built the library by examining CMake cache
    cmake_cache = PROJECT_ROOT / "build" / "CMakeCache.txt"
    uses_clang = False
    if cmake_cache.exists():
        try:
            for line in cmake_cache.read_text().splitlines():
                if line.startswith("CMAKE_C_COMPILER:FILEPATH="):
                    compiler_path = line.split("=", 1)[1].lower()
                    uses_clang = "clang" in compiler_path
                    break
        except Exception:
            pass

    # Alternatively, check ldd output for dynamic ASan linkage
    if not uses_clang:
        try:
            result = subprocess.run(
                ["ldd", str(lib_path)],
                capture_output=True,
                text=True,
            )
            uses_clang = "libclang_rt.asan" in result.stdout
        except FileNotFoundError:
            pass

    if uses_clang:
        # Try Clang's ASan runtime
        try:
            result = subprocess.run(
                ["clang", "--print-file-name=libclang_rt.asan-x86_64.so"],
                capture_output=True,
                text=True,
            )
            if result.returncode == 0:
                path = result.stdout.strip()
                if path and "/" in path and Path(path).exists():
                    return path
        except FileNotFoundError:
            pass
    else:
        # Try GCC's libasan
        try:
            result = subprocess.run(
                ["gcc", "--print-file-name=libasan.so"],
                capture_output=True,
                text=True,
            )
            if result.returncode == 0:
                path = result.stdout.strip()
                if path and path != "libasan.so" and Path(path).exists():
                    return path
        except FileNotFoundError:
            pass

    return None


def is_asan_build() -> bool:
    """Check if the build has ASan enabled by examining the shared library."""
    lib_path = PROJECT_ROOT / "build" / "src" / "arch" / "armv8m" / "libarmv8m_emulator.so"
    if not lib_path.exists():
        return False

    # Check if the library links against ASan dynamically
    try:
        result = subprocess.run(
            ["ldd", str(lib_path)],
            capture_output=True,
            text=True,
        )
        if "libasan" in result.stdout or "libclang_rt.asan" in result.stdout:
            return True
    except FileNotFoundError:
        pass

    # Check if ASan is statically linked (common with Clang)
    try:
        result = subprocess.run(
            ["nm", "-C", str(lib_path)],
            capture_output=True,
            text=True,
        )
        if "__asan_" in result.stdout:
            return True
    except FileNotFoundError:
        pass

    return False


def get_asan_env() -> dict[str, str]:
    """Get environment with ASan preloaded if needed."""
    env = os.environ.copy()

    if is_asan_build():
        asan_lib = find_asan_library()
        if asan_lib:
            existing_preload = env.get("LD_PRELOAD", "")
            if existing_preload:
                env["LD_PRELOAD"] = f"{asan_lib}:{existing_preload}"
            else:
                env["LD_PRELOAD"] = asan_lib
            print(f"[ASan] Preloading: {asan_lib}")

            # Suppress Python's internal leak reports (false positives)
            # but still detect actual memory errors and leaks in our code
            asan_options = env.get("ASAN_OPTIONS", "")
            new_options = "detect_leaks=0"  # Python has many intentional "leaks"
            if asan_options:
                env["ASAN_OPTIONS"] = f"{asan_options}:{new_options}"
            else:
                env["ASAN_OPTIONS"] = new_options

    return env


def main() -> None:
    from femu.dev.test import run_c_tests

    # Run C tests (returns True if passed)
    c_passed = run_c_tests(verbose=False, test_filter=None)

    # Get environment with ASan preloaded for Python tests
    asan_env = get_asan_env()
    # Add PYTHONPATH to ASan env
    asan_env["PYTHONPATH"] = str(PROJECT_ROOT / "python")

    # Run Python tests with ASan preloaded
    print("\n" + "=" * 60)
    print("Running Python Tests (pytest) with ASan")
    print("=" * 60 + "\n")
    py_result = subprocess.call(
        [
            "python",
            "-m",
            "pytest",
            str(PROJECT_ROOT / "python" / "tests"),
            "-v",
            "--cov=femu",
            "--cov-report=term-missing",
            "--cov-report=xml",
            "--cov-fail-under=50",
        ],
        env=asan_env,
    )
    py_passed = py_result == 0
    if py_passed:
        print("\n✓ Python tests passed!")
    else:
        print("\n✗ Python tests failed!")

    # Run firmware emulator tests with ASan preloaded
    print("\n" + "=" * 60)
    print("Running Firmware Emulator Tests with ASan")
    print("=" * 60 + "\n")
    firmware_result = subprocess.call(
        ["python", "tests/firmware/test_emulator.py"],
        env=asan_env,
    )

    # Exit with failure if any tests failed
    failed = (not c_passed) or (not py_passed) or (firmware_result != 0)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
