#!/usr/bin/env python3
"""Replace Unicode box-drawing characters with ASCII equivalents."""

import os
import sys
from pathlib import Path

# Define replacements
REPLACEMENTS = {
    '✓': '[OK]',
    '✗': '[FAIL]',
    '━': '=',
    '┃': '|',
    '│': '|',
    '─': '-',
    '┏': '+',
    '┓': '+',
    '┗': '+',
    '┛': '+',
    '├': '+',
    '┤': '+',
    '┬': '+',
    '┴': '+',
    '┼': '+',
    '╳': 'X',
}

def process_file(filepath):
    """Process a single file, replacing Unicode characters."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Check if file contains any target Unicode
        if not any(char in content for char in REPLACEMENTS.keys()):
            return False
        
        # Apply replacements
        new_content = content
        for old, new in REPLACEMENTS.items():
            new_content = new_content.replace(old, new)
        
        # Write back
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(new_content)
        
        return True
    except Exception as e:
        print(f"Error processing {filepath}: {e}", file=sys.stderr)
        return False

def main():
    """Main entry point."""
    src_dir = Path(__file__).parent / 'src'
    if not src_dir.exists():
        print(f"Error: src directory not found at {src_dir}", file=sys.stderr)
        return 1
    
    # Find all C/C++ source files
    patterns = ['**/*.c', '**/*.cpp', '**/*.h', '**/*.hpp']
    files = []
    for pattern in patterns:
        files.extend(src_dir.glob(pattern))
    
    # Filter out external, build, node_modules
    files = [f for f in files if not any(
        part in f.parts for part in ['external', 'build', 'node_modules']
    )]
    
    print(f"Found {len(files)} files to process")
    
    updated = 0
    for filepath in files:
        if process_file(filepath):
            print(f"Updated: {filepath}")
            updated += 1
    
    print(f"\nProcessed {len(files)} files, updated {updated}")
    return 0

if __name__ == '__main__':
    sys.exit(main())
