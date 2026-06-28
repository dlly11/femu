#!/usr/bin/env python3
"""Check or fix code formatting."""
import argparse
import subprocess
import sys
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="Check only, don't modify")
    args = parser.parse_args()

    results = []

    # Python formatting
    ruff_args = ["ruff", "format"]
    if args.check:
        ruff_args.append("--check")
    ruff_args.append("python/")
    results.append(subprocess.call(ruff_args))

    # C/C++ formatting
    c_files = list(Path("src").rglob("*.[ch]")) + list(Path("src").rglob("*.cpp"))
    if c_files:
        clang_args = ["clang-format"]
        if args.check:
            clang_args.extend(["--dry-run", "-Werror"])
        else:
            clang_args.append("-i")
        clang_args.extend(str(f) for f in c_files)
        results.append(subprocess.call(clang_args))

    sys.exit(1 if any(results) else 0)


if __name__ == "__main__":
    main()
