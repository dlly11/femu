#!/usr/bin/env python3
"""
AI Session Helper

This tool helps AI assistants understand which files to read for each module
and validates that implementations match their contracts.

Usage:
    ./ai_session.py context <module>     Show files AI should read
    ./ai_session.py validate <module>    Check implementation matches header
    ./ai_session.py test <module>        Run module tests
    ./ai_session.py status               Show status of all modules
    ./ai_session.py list                 List all modules
"""

import sys
import os
from pathlib import Path

# Module definitions
MODULES = {
    'decoder': {
        'description': 'Instruction decoder - Thumb to DecodedInsn',
        'header': 'include/armv8m_decoder.h',
        'readme': 'src/core/decoder/README.md',
        'impl_dir': 'src/core/decoder/',
        'deps': ['include/armv8m_types.h'],
        'impl_files': ['decoder.c', 'decode_thumb16.c', 'decode_thumb32.c'],
        'test_files': ['tests/test_decoder.c'],
    },
    'executor': {
        'description': 'Instruction executor - runs decoded instructions',
        'header': 'include/armv8m_executor.h',
        'readme': 'src/core/executor/README.md',
        'impl_dir': 'src/core/executor/',
        'deps': ['include/armv8m_types.h', 'include/armv8m_decoder.h', 'include/armv8m_memory.h'],
        'impl_files': ['executor.c', 'exec_data_proc.c', 'exec_load_store.c', 'exec_branch.c', 'exec_system.c'],
        'test_files': ['tests/test_executor.c'],
    },
    'memory': {
        'description': 'Memory subsystem - RAM, Flash, MMIO dispatch',
        'header': 'include/armv8m_memory.h',
        'readme': 'src/core/memory/README.md',
        'impl_dir': 'src/core/memory/',
        'deps': ['include/armv8m_types.h', 'include/peripheral_interface.h'],
        'impl_files': ['memory.c', 'bus.c'],
        'test_files': ['tests/test_memory.c'],
    },
    'nvic': {
        'description': 'Nested Vectored Interrupt Controller',
        'header': 'include/armv8m_nvic.h',
        'readme': 'src/core/nvic/README.md',
        'impl_dir': 'src/core/nvic/',
        'deps': ['include/armv8m_types.h'],
        'impl_files': ['nvic.c'],
        'test_files': ['tests/test_nvic.c'],
    },
    'mpu': {
        'description': 'Memory Protection Unit (PMSAv8)',
        'header': 'include/armv8m_mpu.h',
        'readme': 'src/core/mpu/README.md',
        'impl_dir': 'src/core/mpu/',
        'deps': ['include/armv8m_types.h'],
        'impl_files': ['mpu.c'],
        'test_files': ['tests/test_mpu.c'],
    },
}

# Peripheral modules
PERIPHERALS = {
    'uart': {
        'description': 'UART peripheral (STM32-style)',
        'languages': ['c', 'python'],
        'c_dir': 'peripherals/c/uart/',
        'python_file': 'peripherals/python/uart.py',
        'deps': ['include/peripheral_interface.h'],
    },
    'gpio': {
        'description': 'GPIO peripheral (STM32-style)',
        'languages': ['rust', 'python'],
        'rust_file': 'peripherals/rust/src/gpio.rs',
        'python_file': 'peripherals/python/gpio.py',
        'deps': ['include/peripheral_interface.h'],
    },
    'timer': {
        'description': 'Basic timer peripheral',
        'languages': ['c', 'python'],
        'c_dir': 'peripherals/c/timer/',
        'python_file': 'peripherals/python/timer.py',
        'deps': ['include/peripheral_interface.h'],
    },
}


def get_project_root():
    """Find project root by looking for docs/ARCHITECTURE.md"""
    path = Path(__file__).resolve().parent
    while path != path.parent:
        if (path / 'docs' / 'ARCHITECTURE.md').exists():
            return path
        path = path.parent
    # Fallback to current directory
    return Path.cwd()


def file_exists(root, filepath):
    """Check if a file exists relative to project root."""
    return (root / filepath).exists()


def get_file_lines(root, filepath):
    """Get line count of a file."""
    path = root / filepath
    if path.exists():
        return len(path.read_text().splitlines())
    return 0


def cmd_context(module):
    """Print files AI should read for this module."""
    root = get_project_root()
    
    if module in MODULES:
        m = MODULES[module]
        print(f"\n{'='*60}")
        print(f"  AI CONTEXT FOR: {module}")
        print(f"  {m['description']}")
        print(f"{'='*60}\n")
        
        print("📖 READ THESE FILES:\n")
        print(f"  1. docs/ARCHITECTURE.md")
        print(f"     (Focus on: Part 5, Part 9)")
        print()
        print(f"  2. {m['header']}")
        lines = get_file_lines(root, m['header'])
        status = "✓" if file_exists(root, m['header']) else "✗ MISSING"
        print(f"     [{status}] {lines} lines - Interface definition")
        print()
        print(f"  3. {m['readme']}")
        status = "✓" if file_exists(root, m['readme']) else "✗ MISSING"
        print(f"     [{status}] Implementation guidance")
        print()
        
        for i, dep in enumerate(m['deps'], start=4):
            lines = get_file_lines(root, dep)
            status = "✓" if file_exists(root, dep) else "✗ MISSING"
            print(f"  {i}. {dep}")
            print(f"     [{status}] {lines} lines - Dependency")
        
        print(f"\n📝 IMPLEMENT IN: {m['impl_dir']}\n")
        for f in m['impl_files']:
            path = m['impl_dir'] + f
            status = "✓ exists" if file_exists(root, path) else "○ to create"
            print(f"     [{status}] {f}")
        
        print(f"\n🧪 TESTS IN: {m['impl_dir']}tests/\n")
        for f in m['test_files']:
            path = m['impl_dir'] + f
            status = "✓ exists" if file_exists(root, path) else "○ to create"
            print(f"     [{status}] {f}")
        
        print(f"\n{'='*60}")
        print("  DO NOT READ other module implementations!")
        print(f"{'='*60}\n")
        
    elif module in PERIPHERALS:
        p = PERIPHERALS[module]
        print(f"\n{'='*60}")
        print(f"  AI CONTEXT FOR PERIPHERAL: {module}")
        print(f"  {p['description']}")
        print(f"{'='*60}\n")
        
        print("📖 READ THESE FILES:\n")
        print(f"  1. docs/ARCHITECTURE.md")
        print(f"     (Focus on: Part 8)")
        print()
        for i, dep in enumerate(p['deps'], start=2):
            status = "✓" if file_exists(root, dep) else "✗ MISSING"
            print(f"  {i}. {dep}")
            print(f"     [{status}] Peripheral ABI")
        
        print(f"\n📝 IMPLEMENT IN:\n")
        if 'c_dir' in p:
            print(f"     C:      {p['c_dir']}")
        if 'rust_file' in p:
            print(f"     Rust:   {p['rust_file']}")
        if 'python_file' in p:
            print(f"     Python: {p['python_file']}")
        print()
    else:
        print(f"Unknown module: {module}")
        print(f"Available modules: {', '.join(MODULES.keys())}")
        print(f"Available peripherals: {', '.join(PERIPHERALS.keys())}")
        return 1
    
    return 0


def cmd_status():
    """Show status of all modules."""
    root = get_project_root()
    
    print(f"\n{'='*60}")
    print("  MODULE STATUS")
    print(f"{'='*60}\n")
    
    print("Core Modules:\n")
    for name, m in MODULES.items():
        header_ok = file_exists(root, m['header'])
        readme_ok = file_exists(root, m['readme'])
        
        impl_count = sum(1 for f in m['impl_files'] 
                        if file_exists(root, m['impl_dir'] + f))
        impl_total = len(m['impl_files'])
        
        test_count = sum(1 for f in m['test_files'] 
                        if file_exists(root, m['impl_dir'] + f))
        test_total = len(m['test_files'])
        
        if impl_count == impl_total and impl_count > 0:
            status = "✅ Complete"
        elif impl_count > 0:
            status = "🔨 In Progress"
        elif header_ok and readme_ok:
            status = "📋 Ready"
        else:
            status = "❌ Not Started"
        
        print(f"  {name:12} {status:15} impl: {impl_count}/{impl_total}  tests: {test_count}/{test_total}")
    
    print("\nPeripherals:\n")
    for name, p in PERIPHERALS.items():
        langs = []
        if 'c_dir' in p and file_exists(root, p['c_dir']):
            langs.append('C')
        if 'rust_file' in p and file_exists(root, p['rust_file']):
            langs.append('Rust')
        if 'python_file' in p and file_exists(root, p['python_file']):
            langs.append('Python')
        
        lang_str = ', '.join(langs) if langs else 'none'
        print(f"  {name:12} implemented in: {lang_str}")
    
    print()
    return 0


def cmd_list():
    """List all modules."""
    print("\nCore Modules:")
    for name, m in MODULES.items():
        print(f"  {name:12} - {m['description']}")
    
    print("\nPeripherals:")
    for name, p in PERIPHERALS.items():
        print(f"  {name:12} - {p['description']}")
    
    print()
    return 0


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    
    cmd = sys.argv[1]
    
    if cmd == 'context' and len(sys.argv) >= 3:
        return cmd_context(sys.argv[2])
    elif cmd == 'status':
        return cmd_status()
    elif cmd == 'list':
        return cmd_list()
    elif cmd == 'validate' and len(sys.argv) >= 3:
        print("TODO: Implement validation")
        return 0
    elif cmd == 'test' and len(sys.argv) >= 3:
        print("TODO: Implement test runner")
        return 0
    else:
        print(__doc__)
        return 1


if __name__ == '__main__':
    sys.exit(main())
