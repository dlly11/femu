#!/usr/bin/env python3
"""
AI Session Helper - Compatibility wrapper.

This script is a compatibility wrapper for the new CLI.
Use 'femu dev' commands instead:

    femu dev context <module>   # Show AI context files
    femu dev status             # Show module status
    femu dev list               # List all modules
    femu dev validate <module>  # Validate implementation
"""

import sys


def main() -> int:
    """Main entry point - redirect to new CLI."""
    # Try to use the new CLI
    try:
        from femu.dev.session import list_modules, show_context, show_status

        if len(sys.argv) < 2:
            print(__doc__)
            return 1

        cmd = sys.argv[1]

        if cmd == "context" and len(sys.argv) >= 3:
            show_context(sys.argv[2])
            return 0
        elif cmd == "status":
            show_status()
            return 0
        elif cmd == "list":
            list_modules()
            return 0
        else:
            print(__doc__)
            return 1

    except ImportError:
        print("Error: femu package not installed.")
        print("Run 'uv pip install -e .' or enter the nix shell with 'nix develop'")
        return 1


if __name__ == "__main__":
    sys.exit(main())
