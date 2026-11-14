#!/usr/bin/env python3
"""Fix all ethervox_governor_execute calls to add event_callback parameter."""

import re
from pathlib import Path

# Find all .c files
root = Path("/Users/timk/repos/ethervoxai-apple/ethervox_core")
c_files = list(root.rglob("*.c"))

def fix_execute_calls(content):
    """Add NULL event_callback parameter after progress_callback."""
    
    # Pattern to match ethervox_governor_execute/execute_with_context calls
    # Looking for patterns like:
    #   ethervox_governor_execute(..., progress_cb, token_cb, user_data)
    #   ethervox_governor_execute(..., NULL, token_cb, user_data)
    #   ethervox_governor_execute(..., progress_cb, NULL, NULL)
    
    # We need to insert NULL after the progress_callback (7th parameter)
    # and before the token_callback (8th parameter, which becomes 9th)
    
    # First, find all function calls
    pattern = re.compile(
        r'(ethervox_governor_execute(?:_with_context)?)\s*\(',
        re.MULTILINE
    )
    
    result = content
    matches = list(pattern.finditer(content))
    
    # Process matches in reverse to maintain positions
    for match in reversed(matches):
        func_name = match.group(1)
        start_pos = match.end()
        
        # Find matching closing paren
        paren_count = 1
        pos = start_pos
        while pos < len(content) and paren_count > 0:
            if content[pos] == '(':
                paren_count += 1
            elif content[pos] == ')':
                paren_count -= 1
            pos += 1
        
        if paren_count != 0:
            continue  # Couldn't find matching paren
        
        # Extract arguments
        args_str = content[start_pos:pos-1]
        
        # Count commas to determine parameter count (roughly)
        # We're looking for 9 parameters now, need 10
        comma_count = 0
        in_string = False
        paren_depth = 0
        for c in args_str:
            if c == '"' and (len(args_str) == 0 or args_str[args_str.index(c)-1] != '\\'):
                in_string = not in_string
            elif not in_string:
                if c == '(':
                    paren_depth += 1
                elif c == ')':
                    paren_depth -= 1
                elif c == ',' and paren_depth == 0:
                    comma_count += 1
        
        # If we have 9 commas (10 params), it's already fixed
        if comma_count >= 9:
            continue
        
        # If we have 8 commas (9 params), we need to add event_callback
        if comma_count == 8:
            # Find the 7th comma (after progress_callback)
            comma_num = 0
            comma_pos = -1
            in_string = False
            paren_depth = 0
            for i, c in enumerate(args_str):
                if c == '"':
                    in_string = not in_string
                elif not in_string:
                    if c == '(':
                        paren_depth += 1
                    elif c == ')':
                        paren_depth -= 1
                    elif c == ',' and paren_depth == 0:
                        comma_num += 1
                        if comma_num == 7:  # After progress_callback
                            comma_pos = i
                            break
            
            if comma_pos >= 0:
                # Insert " NULL," after the 7th comma
                insert_pos = start_pos + comma_pos + 1
                result = result[:insert_pos] + " NULL," + result[insert_pos:]
    
    return result

modified = []
for c_file in c_files:
    try:
        with open(c_file, 'r') as f:
            content = f.read()
        
        new_content = fix_execute_calls(content)
        
        if new_content != content:
            with open(c_file, 'w') as f:
                f.write(new_content)
            modified.append(c_file)
            print(f"Modified: {c_file}")
    except Exception as e:
        print(f"Error processing {c_file}: {e}")

print(f"\nModified {len(modified)} files")
