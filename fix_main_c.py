#!/usr/bin/env python3
"""Fix main.c execute calls that are missing cancel_token and event_callback."""

import re

file_path = "/Users/timk/repos/ethervoxai-apple/ethervox_core/src/main.c"

with open(file_path, 'r') as f:
    content = f.read()

# Pattern to match the specific issue: ethervox_governor_execute(g_governor, <query>, &response, &error,
# This pattern is missing NULL for cancel_token after the query
pattern = re.compile(
    r'(ethervox_governor_execute\s*\(\s*g_governor\s*,\s*\w+\s*,)\s*(&response\s*,\s*&error\s*,)',
    re.MULTILINE
)

# Replace with: ethervox_governor_execute(g_governor, <query>, NULL, &response, &error,
content = pattern.sub(r'\1 NULL, \2', content)

# Now fix the missing event_callback parameter
# Pattern to match calls with 9 parameters (missing event_callback)
# They should have: ..., NULL, NULL, NULL, NULL)  at the end for:
# metrics, progress_callback, event_callback, token_callback, user_data
pattern2 = re.compile(
    r'(NULL\s*,\s*//\s*No\s*metrics\s*\n\s*NULL\s*,\s*//\s*No\s*progress\s*callback\s*\n)(\s*NULL\s*,\s*//\s*No\s*token\s*callback)',
    re.MULTILINE
)

content = pattern2.sub(r'\1                                  NULL,  // No event callback\n\2', content)

with open(file_path, 'w') as f:
    f.write(content)

print(f"Fixed {file_path}")
