#!/usr/bin/env python3
"""Run all linters."""
import os
import subprocess
import sys
from pathlib import Path

# Add the python directory to the path for imports
SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "python"))
os.chdir(PROJECT_ROOT)


def main() -> None:
    results = []

    # C/C++ static analysis. clang-tidy needs compile_commands.json, which the
    # CMake configure step generates in the build directory.
    from femu.build import configure, run_analysis

    build_dir = PROJECT_ROOT / "build"
    if not build_dir.exists():
        configure()

    # cppcheck and clang-tidy. The CMake clang-tidy target injects the compiler's
    # system include paths so it works inside the Nix dev shell (see CMakeLists.txt).
    results.append(0 if run_analysis(tool="cppcheck") else 1)
    results.append(0 if run_analysis(tool="clang-tidy") else 1)

    # Python linting
    results.append(subprocess.call(["ruff", "check", "python/"]))

    # Python type checking
    results.append(subprocess.call(["mypy", "python/femu/"]))

    sys.exit(1 if any(results) else 0)


if __name__ == "__main__":
    main()
