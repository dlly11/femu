#!/usr/bin/env python3
"""Build the emulator."""
import argparse
import os
import sys
from pathlib import Path

# Add the python directory to the path for imports
SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "python"))
os.chdir(PROJECT_ROOT)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", choices=["gcc", "clang"], default="gcc")
    parser.add_argument("--build-type", default="Debug")
    args = parser.parse_args()

    from femu.build import compile_project, configure

    configure(
        build_type=args.build_type,
        compiler=args.compiler,
        sanitizers=True,
    )
    compile_project()


if __name__ == "__main__":
    main()
