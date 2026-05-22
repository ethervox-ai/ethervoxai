#!/usr/bin/env python3
import sys

with open('src/main.c', 'r', encoding='utf-8') as f:
    content = f.read()

content = content.replace('open("/dev/null", O_WRONLY)', 'open_devnull()')

with open('src/main.c', 'w', encoding='utf-8') as f:
    f.write(content)

print("Replaced all open(\"/dev/null\", O_WRONLY) calls")
