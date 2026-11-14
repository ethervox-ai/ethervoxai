#!/usr/bin/env python3
"""
Add event_callback parameter to all ethervox_governor_execute calls.
The event_callback goes after progress_callback and before token_callback.
"""

import re
import sys
from pathlib import Path

# Find all .c files in ethervox_core
root = Path("/Users/timk/repos/ethervoxai-apple/ethervox_core")
c_files = list(root.rglob("*.c"))

# Pattern to match ethervox_governor_execute calls
# We need to find calls and add NULL after progress_callback
pattern = re.compile(
    r'(ethervox_governor_execute(?:_with_context)?\s*\([^;]+?'
    r',\s*progress_callback\s*,)'
    r'(\s*(?:NULL|token_callback|[a-zA-Z_][a-zA-Z0-9_]*)\s*,)',
    re.MULTILINE | re.DOTALL
)

def process_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()
    
    original = content
    
    # Add NULL, for event_callback after progress_callback
    # The pattern captures: (... progress_callback,)(token_callback or NULL,)
    # We want: (...progress_callback,) NULL, (token_callback or NULL,)
    content = pattern.sub(r'\1 NULL,\2', content)
    
    if content != original:
        with open(filepath, 'w') as f:
            f.write(content)
        return True
    return False

modified = []
for c_file in c_files:
    if process_file(c_file):
        modified.append(c_file)
        print(f"Modified: {c_file}")

print(f"\nModified {len(modified)} files")
