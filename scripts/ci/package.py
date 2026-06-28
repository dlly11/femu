#!/usr/bin/env python3
"""Build release wheel."""
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
    from femu.build import compile_project, configure

    # Build release version
    configure(build_type="Release", sanitizers=False)
    compile_project()

    # Build wheel (uv drives the setuptools build backend declared in pyproject.toml)
    sys.exit(subprocess.call(["uv", "build", "--wheel", "--out-dir", "dist/"]))


if __name__ == "__main__":
    main()
