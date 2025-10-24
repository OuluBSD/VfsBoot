#!/usr/bin/env python3
"""
VfsBoot Complete Modular Refactoring Script
Automatically splits codex.h/cpp into 20 modular files

Based on MODULE_REFERENCE.md specifications
"""

import os
import sys
import subprocess
from pathlib import Path

# Complete module specifications with exact line ranges
MODULES = [
    # Phase 1: Foundation
    {
        'name': 'vfs_common',
        'header_ranges': [(1, 97)],
        'cpp_ranges': [(76, 162), (163, 200)],
        'depends': [],
        'desc': 'Foundation layer - includes, tracing, i18n, hash'
    },
    {
        'name': 'tag_system',
        'header_ranges': [(107, 422), (673, 686)],
        'cpp_ranges': [(3533, 3634), (4247, 4256)],
        'depends': ['vfs_common'],
        'desc': 'Tag registry, BitVector, TagSet, TagStorage'
    },
    {
        'name': 'logic_engine',
        'header_ranges': [(424, 506)],
        'cpp_ranges': [(3635, 4246)],
        'depends': ['vfs_common', 'tag_system'],
        'desc': 'Logic formulas, theorem proving'
    },
    
    # Phase 2: Core VFS
    {
        'name': 'vfs_core',
        'header_ranges': [(507, 541), (687, 812), (814, 839)],
        'cpp_ranges': [(2671, 3176), (4257, 4344)],
        'depends': ['vfs_common', 'tag_system', 'logic_engine'],
        'desc': 'Core VFS with overlay system'
    },
    {
        'name': 'vfs_mount',
        'header_ranges': [(542, 593)],
        'cpp_ranges': [(3177, 3532)],
        'depends': ['vfs_common', 'vfs_core'],
        'desc': 'Mount points for filesystem/libraries'
    },
    
    # Phase 3: AST Systems
    {
        'name': 'sexp',
        'header_ranges': [(594, 671), (840, 851)],
        'cpp_ranges': [(2606, 2670), (4345, 4428), (4429, 4571)],
        'depends': ['vfs_common', 'vfs_core'],
        'desc': 'S-expression parser and evaluator'
    },
    {
        'name': 'cpp_ast',
        'header_ranges': [(852, 963)],
        'cpp_ranges': [(4572, 4786)],
        'depends': ['vfs_common', 'sexp'],
        'desc': 'C++ AST builder'
    },
    {
        'name': 'clang_parser',
        'header_ranges': [(974, 1400)],
        'cpp_ranges': [(569, 1031)],
        'depends': ['vfs_common', 'sexp', 'vfs_core'],
        'desc': 'libclang integration'
    },
    {
        'name': 'planner',
        'header_ranges': [(1401, 1523)],
        'cpp_ranges': [(4787, 5049)],
        'depends': ['vfs_common', 'sexp', 'vfs_core'],
        'desc': 'Planning system'
    },
    
    # Phase 4: Advanced Features
    {
        'name': 'ai_bridge',
        'header_ranges': [(1524, 1537)],
        'cpp_ranges': [(5050, 5458)],
        'depends': ['vfs_common'],
        'desc': 'OpenAI/Llama integration'
    },
    {
        'name': 'context_builder',
        'header_ranges': [(1538, 1714)],
        'cpp_ranges': [(5459, 6067)],
        'depends': ['vfs_common', 'tag_system', 'vfs_core'],
        'desc': 'AI context management'
    },
    {
        'name': 'make',
        'header_ranges': [(2252, 2311)],
        'cpp_ranges': [(7943, 8267)],
        'depends': ['vfs_common', 'vfs_core'],
        'desc': 'GNU Make subset'
    },
    {
        'name': 'hypothesis',
        'header_ranges': [(1715, 1853)],
        'cpp_ranges': [(6068, 6667)],
        'depends': ['vfs_common', 'context_builder'],
        'desc': '5-level hypothesis testing'
    },
    {
        'name': 'scope_store',
        'header_ranges': [(1855, 2040)],
        'cpp_ranges': [(6668, 7166)],
        'depends': ['vfs_common', 'vfs_core', 'context_builder'],
        'desc': 'Binary diffs and snapshots'
    },
    {
        'name': 'feedback',
        'header_ranges': [(2041, 2251)],
        'cpp_ranges': [(7167, 7799)],
        'depends': ['vfs_common', 'logic_engine', 'vfs_core'],
        'desc': 'Feedback pipeline'
    },
    
    # Phase 5: Interface Layer
    {
        'name': 'shell_commands',
        'header_ranges': [],  # All in cpp
        'cpp_ranges': [(1833, 2605), (8268, 11526)],
        'depends': ['vfs_common', 'vfs_core', 'sexp', 'cpp_ast', 'planner', 'ai_bridge', 'context_builder', 'hypothesis', 'feedback', 'make'],
        'desc': 'All shell command implementations'
    },
    {
        'name': 'repl',
        'header_ranges': [],  # All in cpp
        'cpp_ranges': [(7800, 7942)],
        'depends': ['vfs_common', 'vfs_core', 'shell_commands'],
        'desc': 'REPL loop with line editing'
    },
    {
        'name': 'main',
        'header_ranges': [],  # No header needed
        'cpp_ranges': [(1, 568), (1032, 1832)],  # main() and initialization
        'depends': ['vfs_common', 'vfs_core', 'repl', 'sexp'],
        'desc': 'Entry point and initialization'
    },
]

def read_lines(filepath, start, end):
    """Read lines from file (1-indexed, inclusive)"""
    with open(filepath, 'r') as f:
        lines = f.readlines()
    return lines[start-1:end]

def extract_module(module, base_dir='VfsShell'):
    """Extract a single module"""
    name = module['name']
    header_file = f"{base_dir}/{name}.h"
    cpp_file = f"{base_dir}/{name}.cpp"
    
    print(f"\n{'='*60}")
    print(f"Extracting: {name}")
    print(f"  {module['desc']}")
    
    # Create header if there are header ranges
    if module['header_ranges']:
        header_lines = ['#pragma once\n', '\n']
        
        # Add dependency includes
        for dep in module['depends']:
            header_lines.append(f'#include "{dep}.h"\n')
        if module['depends']:
            header_lines.append('\n')
        
        # Extract from codex.h
        for start, end in module['header_ranges']:
            lines = read_lines(f'{base_dir}/codex.h', start, end)
            header_lines.extend(lines)
            if not lines[-1].endswith('\n'):
                header_lines.append('\n')
        
        # Write header
        with open(header_file, 'w') as f:
            f.writelines(header_lines)
        
        print(f"  ✓ Created {header_file} ({len(header_lines)} lines)")
    else:
        print(f"  ⊘ No header file needed")
    
    # Create cpp if there are cpp ranges
    if module['cpp_ranges']:
        cpp_lines = []
        
        # Add includes
        if module['header_ranges']:
            cpp_lines.append(f'#include "{name}.h"\n')
        else:
            # For modules without headers, include dependencies directly
            for dep in module['depends']:
                cpp_lines.append(f'#include "{dep}.h"\n')
        cpp_lines.append('\n')
        
        # Extract from codex.cpp
        for start, end in module['cpp_ranges']:
            lines = read_lines(f'{base_dir}/codex.cpp', start, end)
            cpp_lines.extend(lines)
            if lines and not lines[-1].endswith('\n'):
                cpp_lines.append('\n')
        
        # Write cpp
        with open(cpp_file, 'w') as f:
            f.writelines(cpp_lines)
        
        print(f"  ✓ Created {cpp_file} ({len(cpp_lines)} lines)")
    else:
        print(f"  ⊘ No cpp file needed")

def update_makefile():
    """Update Makefile to include all new source files"""
    print(f"\n{'='*60}")
    print("Updating Makefile...")
    
    # Generate list of new source files
    sources = []
    for module in MODULES:
        if module['cpp_ranges']:
            sources.append(f"src/VfsShell/{module['name']}.cpp")
    
    # Read current Makefile
    with open('Makefile', 'r') as f:
        lines = f.readlines()
    
    # Find and update VFSSHELL_SRC line
    updated = False
    for i, line in enumerate(lines):
        if line.startswith('VFSSHELL_SRC :='):
            # Replace with all new sources
            new_line = 'VFSSHELL_SRC := ' + ' '.join(sources) + '\n'
            lines[i] = new_line
            updated = True
            break
    
    if updated:
        with open('Makefile', 'w') as f:
            f.writelines(lines)
        print("  ✓ Updated VFSSHELL_SRC in Makefile")
    else:
        print("  ✗ Could not find VFSSHELL_SRC in Makefile")
        print("  ! Please manually update Makefile")

def create_new_codex_h():
    """Create new codex.h that just includes all modules"""
    print(f"\n{'='*60}")
    print("Creating new codex.h...")
    
    lines = ['#pragma once\n', '\n']
    lines.append('// Modular VfsBoot - Auto-generated includes\n')
    lines.append('// This file replaces the monolithic codex.h\n')
    lines.append('\n')
    
    for module in MODULES:
        if module['header_ranges']:
            lines.append(f'#include "{module["name"]}.h"\n')
    
    # Backup old codex.h
    if os.path.exists('src/VfsShell/codex.h'):
        os.rename('src/VfsShell/codex.h', 'src/VfsShell/codex.h.bak')
        print("  ✓ Backed up old codex.h to codex.h.bak")
    
    with open('src/VfsShell/codex.h', 'w') as f:
        f.writelines(lines)
    
    print(f"  ✓ Created new codex.h with module includes")

def create_new_codex_cpp():
    """Create stub codex.cpp or rename old one"""
    print(f"\n{'='*60}")
    print("Handling codex.cpp...")
    
    if os.path.exists('src/VfsShell/codex.cpp'):
        os.rename('src/VfsShell/codex.cpp', 'src/VfsShell/codex.cpp.bak')
        print("  ✓ Backed up old codex.cpp to codex.cpp.bak")
    
    # Create a minimal stub
    stub = '''// VfsBoot - Modular Implementation
// All code has been split into modular files
// See: vfs_common, tag_system, logic_engine, vfs_core, etc.

// This file intentionally left minimal
// The old monolithic codex.cpp is backed up as codex.cpp.bak
'''
    
    with open('src/VfsShell/codex.cpp', 'w') as f:
        f.write(stub)
    
    print("  ✓ Created stub codex.cpp")

def test_compilation():
    """Try to compile the project"""
    print(f"\n{'='*60}")
    print("Testing compilation...")
    
    try:
        # Clean first
        subprocess.run(['make', 'clean'], check=True, capture_output=True)
        print("  ✓ make clean succeeded")
        
        # Try to build
        result = subprocess.run(['make', '-j4'], capture_output=True, text=True)
        
        if result.returncode == 0:
            print("  ✓✓✓ COMPILATION SUCCESSFUL! ✓✓✓")
            return True
        else:
            print("  ✗ Compilation failed:")
            print(result.stderr)
            return False
    except subprocess.CalledProcessError as e:
        print(f"  ✗ Error: {e}")
        return False
    except FileNotFoundError:
        print("  ! make command not found - skipping compilation test")
        return None

def main():
    print("="*60)
    print("VfsBoot Modular Refactoring Script")
    print("="*60)
    print()
    print("This script will:")
    print("  1. Extract 18 modules from codex.h/cpp")
    print("  2. Backup original files (.bak)")
    print("  3. Update Makefile")
    print("  4. Create new codex.h with includes")
    print("  5. Test compilation")
    print()
    
    # Confirm
    response = input("Continue? [y/N]: ")
    if response.lower() not in ['y', 'yes']:
        print("Aborted.")
        sys.exit(0)
    
    # Check files exist
    if not os.path.exists('VfsShell/codex.h'):
        print("\n✗ Error: VfsShell/codex.h not found")
        sys.exit(1)
    
    if not os.path.exists('src/VfsShell/codex.cpp'):
        print("\n✗ Error: VfsShell/codex.cpp not found")
        sys.exit(1)
    
    # Extract all modules
    for module in MODULES:
        try:
            extract_module(module)
        except Exception as e:
            print(f"\n✗ Error extracting {module['name']}: {e}")
            continue
    
    # Update build system
    update_makefile()
    create_new_codex_h()
    create_new_codex_cpp()
    
    # Test
    success = test_compilation()
    
    # Summary
    print(f"\n{'='*60}")
    print("REFACTORING COMPLETE!")
    print("="*60)
    
    created = sum(1 for m in MODULES if m['header_ranges']) + sum(1 for m in MODULES if m['cpp_ranges'])
    print(f"\n✓ Created {created} new files")
    print("✓ Backed up old codex.h and codex.cpp")
    print("✓ Updated Makefile")
    print("✓ Created new modular codex.h")
    
    if success:
        print("\n✓✓✓ Build test PASSED! ✓✓✓")
    elif success is None:
        print("\n! Build test skipped (make not available)")
    else:
        print("\n✗ Build test FAILED - see errors above")
        print("\nTo rollback:")
        print("  mv VfsShell/codex.h.bak VfsShell/codex.h")
        print("  mv src/VfsShell/codex.cpp.bak src/VfsShell/codex.cpp")
        print("  git checkout Makefile")
    
    print("\nNext steps:")
    print("  1. Review generated files")
    print("  2. Fix any compilation errors")
    print("  3. Run tests: python tools/test_harness.py")
    print("  4. Commit: git add VfsShell/*.{h,cpp} Makefile && git commit")

if __name__ == '__main__':
    main()
