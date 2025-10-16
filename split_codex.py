#!/usr/bin/env python3
"""
VfsBoot Modular Refactoring Script
Splits monolithic codex.h/cpp into 20 modular files
"""

import os
import sys
import re
from pathlib import Path

# Define module extraction specifications
MODULES = {
    'vfs_common': {
        'header_ranges': [(1, 106)],
        'cpp_ranges': [(1, 202)],
        'depends': [],
        'description': 'Foundation layer - includes, tracing, i18n, hash'
    },
    'tag_system': {
        'header_ranges': [(107, 422), (676, 686)],
        'cpp_ranges': [(3533, 3634), (4247, 4256)],
        'depends': ['vfs_common'],
        'description': 'Tag registry, BitVector, TagSet, TagStorage'
    },
    'logic_engine': {
        'header_ranges': [(424, 506)],
        'cpp_ranges': [(3635, 4246)],
        'depends': ['vfs_common', 'tag_system'],
        'description': 'Logic formulas, theorem proving, rule inference'
    },
    # Add more modules here...
}

def read_file_lines(filepath, start=None, end=None):
    """Read specific line ranges from a file (1-indexed, inclusive)"""
    with open(filepath, 'r') as f:
        lines = f.readlines()
    
    if start is None:
        return lines
    
    # Convert to 0-indexed
    start_idx = start - 1
    end_idx = end if end is not None else len(lines)
    
    return lines[start_idx:end_idx]

def extract_module(module_name, spec, base_dir='VfsShell'):
    """Extract a single module"""
    header_file = f"{base_dir}/{module_name}.h"
    cpp_file = f"{base_dir}/{module_name}.cpp"
    
    print(f"Extracting {module_name}...")
    
    # Extract header
    header_lines = ['#pragma once\n', '\n']
    
    # Add includes for dependencies
    for dep in spec['depends']:
        header_lines.append(f'#include "{dep}.h"\n')
    if spec['depends']:
        header_lines.append('\n')
    
    # Extract from codex.h
    for start, end in spec['header_ranges']:
        lines = read_file_lines(f'{base_dir}/codex.h', start, end)
        header_lines.extend(lines)
    
    # Write header
    os.makedirs(base_dir, exist_ok=True)
    with open(header_file, 'w') as f:
        f.writelines(header_lines)
    
    print(f"  Created {header_file} ({len(header_lines)} lines)")
    
    # Extract cpp
    cpp_lines = [f'#include "{module_name}.h"\n', '\n']
    
    # Extract from codex.cpp
    for start, end in spec['cpp_ranges']:
        lines = read_file_lines(f'{base_dir}/codex.cpp', start, end)
        cpp_lines.extend(lines)
    
    # Write cpp
    with open(cpp_file, 'w') as f:
        f.writelines(cpp_lines)
    
    print(f"  Created {cpp_file} ({len(cpp_lines)} lines)")
    
    return header_file, cpp_file

def main():
    print("VfsBoot Modular Refactoring")
    print("=" * 60)
    
    # Check if codex files exist
    if not os.path.exists('VfsShell/codex.h'):
        print("Error: VfsShell/codex.h not found")
        sys.exit(1)
    
    if not os.path.exists('VfsShell/codex.cpp'):
        print("Error: VfsShell/codex.cpp not found")
        sys.exit(1)
    
    # Extract each module
    created_files = []
    for module_name, spec in MODULES.items():
        try:
            header, cpp = extract_module(module_name, spec)
            created_files.extend([header, cpp])
        except Exception as e:
            print(f"  ERROR: {e}")
            continue
    
    print("\n" + "=" * 60)
    print(f"Successfully created {len(created_files)} files:")
    for f in created_files:
        print(f"  {f}")
    
    print("\nNext steps:")
    print("1. Update Makefile to include new .cpp files")
    print("2. Update codex.h to #include the new modules")
    print("3. Test compilation: make clean && make")

if __name__ == '__main__':
    main()
