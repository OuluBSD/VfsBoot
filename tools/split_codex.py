#!/usr/bin/env python3
"""
Script to split monolithic codex.cpp into modules.
This helps automate the tedious extraction process.
"""

import re
import sys
from pathlib import Path

def read_file(path):
    with open(path, 'r', encoding='utf-8') as f:
        return f.readlines()

def write_file(path, lines):
    with open(path, 'w', encoding='utf-8') as f:
        f.writelines(lines)

def extract_section(lines, start_line, end_line):
    """Extract lines from start_line to end_line (1-indexed)"""
    return lines[start_line-1:end_line]

def main():
    codex_cpp = Path(__file__).parent.parent / "VfsShell" / "codex.cpp"
    vfs_shell_dir = Path(__file__).parent.parent / "VfsShell"

    if not codex_cpp.exists():
        print(f"Error: {codex_cpp} not found")
        return 1

    print(f"Reading {codex_cpp}...")
    lines = read_file(codex_cpp)
    total_lines = len(lines)
    print(f"Total lines: {total_lines}")

    # Section boundaries (approximate, will need manual adjustment)
    sections = {
        'utils': (68, 105),  # trim_copy, join_path, normalize_path, path_basename
        'overlay_core': (106, 598),  # serialize/deserialize, mount/save overlay
        'binary_io': (598, 690),  # BinaryWriter/BinaryReader
        's_ast_serial': (690, 818),  # S-expression AST serialization
        'cpp_ast_serial': (818, 1283),  # C++ AST serialization
        'terminal': (1283, 1578),  # RawTerminalMode and terminal handling
        'command_pipeline': (1578, 1863),  # Command execution
        'value_show': (1863, 1888),  # Value::show implementation
        'ast_ctors': (1874, 1928),  # AST node constructors and eval
        'vfs_impl': (1928, 2308),  # VFS implementation
        'parser': (2308, 2392),  # S-expression parser
        'builtins': (2392, 2535),  # Builtins
        'exec_utils': (2535, 2588),  # exec_capture, has_cmd
        'cpp_ast_nodes': (2588, 2803),  # C++ AST node implementations
        'ai_bridge': (2803, 3203),  # AI functions
        'repl': (3203, total_lines),  # REPL and main
    }

    print("\nSection boundaries:")
    for name, (start, end) in sections.items():
        print(f"  {name:20s}: lines {start:5d}-{end:5d} ({end-start:4d} lines)")

    return 0

if __name__ == '__main__':
    sys.exit(main())
