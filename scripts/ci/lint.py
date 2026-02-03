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

    # C/C++ static analysis (cppcheck only - doesn't need compile_commands.json)
    from femu.build import configure, run_analysis

    build_dir = PROJECT_ROOT / "build"
    if not build_dir.exists():
        configure()

    # Run cppcheck only (clang-tidy has issues with Nix system headers)
    analysis_passed = run_analysis(tool="cppcheck")
    results.append(0 if analysis_passed else 1)

    # Python linting
    results.append(subprocess.call(["ruff", "check", "python/"]))

    # Python type checking
    results.append(subprocess.call(["mypy", "python/femu/"]))

    sys.exit(1 if any(results) else 0)


if __name__ == "__main__":
    main()
