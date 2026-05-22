#!/usr/bin/env python3
"""
Binary Tool Manifest Generator

Parses existing tool definitions and generates tools.bin in the portable format
defined by tool_manifest.h.

Usage:
    python3 generate_tool_manifest.py -o tools.bin

Copyright (c) 2024-2025 EthervoxAI Team
SPDX-License-Identifier: CC-BY-NC-SA-4.0
"""

import struct
import json
import sys
import os
from pathlib import Path
from typing import List, Dict, Any
import hashlib

# Constants matching tool_manifest.h
TOOL_MANIFEST_MAGIC = 0x45544F4C  # "ETOL"
TOOL_MANIFEST_VERSION = 1
TOOL_NAME_MAX = 64
TOOL_DESC_MAX = 256
TOOL_CATEGORY_MAX = 32
TOOL_PARAM_NAME_MAX = 64
TOOL_PARAM_DEFAULT_MAX = 128
TOOL_MAX_PARAMS = 32
TOOL_MAX_TRIGGERS = 16

# Checksum types
CHECKSUM_NONE = 0
CHECKSUM_CRC32 = 1
CHECKSUM_SHA256 = 2

class ToolParameter:
    """Represents a tool parameter"""
    def __init__(self, name: str, param_type: str, required: bool = False, default: str = ""):
        self.name = name[:TOOL_PARAM_NAME_MAX - 1]
        self.type = param_type[:TOOL_PARAM_NAME_MAX - 1]
        self.required = required
        self.default = default[:TOOL_PARAM_DEFAULT_MAX - 1]
    
    def to_bytes(self) -> bytes:
        """Serialize to binary format"""
        # typedef struct {
        #     char name[64];
        #     char type[64];  
        #     uint8_t required;
        #     char default_value[128];
        # } tool_param_t;  // 256 bytes
        
        data = bytearray(256)
        data[0:len(self.name)] = self.name.encode('utf-8')
        data[64:64+len(self.type)] = self.type.encode('utf-8')
        data[128] = 1 if self.required else 0
        data[129:129+len(self.default)] = self.default.encode('utf-8')
        
        return bytes(data)

class Tool:
    """Represents a complete tool definition"""
    def __init__(self, name: str, one_line: str, category: str = "general", 
                 priority: int = 5, enabled: bool = True):
        self.name = name[:TOOL_NAME_MAX - 1]
        self.one_line = one_line[:TOOL_DESC_MAX - 1]
        self.category = category[:TOOL_CATEGORY_MAX - 1]
        self.priority = min(255, max(0, priority))
        self.enabled = enabled
        self.parameters: List[ToolParameter] = []
        self.triggers: List[str] = []
        self.description_full = ""
        self.usage_example = ""
    
    def add_parameter(self, param: ToolParameter):
        """Add a parameter (max 32)"""
        if len(self.parameters) < TOOL_MAX_PARAMS:
            self.parameters.append(param)
    
    def add_trigger(self, trigger: str):
        """Add a trigger phrase (max 16)"""
        if len(self.triggers) < TOOL_MAX_TRIGGERS:
            self.triggers.append(trigger[:TOOL_DESC_MAX - 1])
    
    def to_index_entry(self, detail_offset: int, detail_size: int) -> bytes:
        """Serialize index entry to binary"""
        # typedef struct {
        #     char name[64];
        #     char one_line[256];
        #     char category[32];
        #     uint8_t priority;
        #     uint8_t enabled;
        #     uint16_t detail_size;
        #     uint32_t detail_offset;
        # } tool_index_entry_t;  // 360 bytes
        
        data = bytearray(360)
        data[0:len(self.name)] = self.name.encode('utf-8')
        data[64:64+len(self.one_line)] = self.one_line.encode('utf-8')
        data[320:320+len(self.category)] = self.category.encode('utf-8')
        data[352] = self.priority
        data[353] = 1 if self.enabled else 0
        struct.pack_into('<H', data, 354, detail_size)
        struct.pack_into('<I', data, 356, detail_offset)
        
        return bytes(data)
    
    def to_detail(self) -> bytes:
        """Serialize detail record to binary"""
        # typedef struct {
        #     char name[64];
        #     char description_full[512];
        #     char usage_example[256];
        #     uint8_t param_count;
        #     uint8_t trigger_count;
        # } tool_detail_header_t;  // 834 bytes
        # Followed by param_count * tool_param_t (256 bytes each)
        # Followed by trigger_count * char[256]
        
        header = bytearray(834)
        header[0:len(self.name)] = self.name.encode('utf-8')
        
        desc = self.description_full[:511]
        header[64:64+len(desc)] = desc.encode('utf-8')
        
        usage = self.usage_example[:255]
        header[576:576+len(usage)] = usage.encode('utf-8')
        
        header[832] = len(self.parameters)
        header[833] = len(self.triggers)
        
        # Build complete detail record
        data = bytes(header)
        
        # Append parameters
        for param in self.parameters:
            data += param.to_bytes()
        
        # Append triggers
        for trigger in self.triggers:
            trigger_data = bytearray(256)
            trigger_data[0:len(trigger)] = trigger.encode('utf-8')
            data += bytes(trigger_data)
        
        return data

class ManifestBuilder:
    """Builds the binary manifest file"""
    def __init__(self):
        self.tools: List[Tool] = []
    
    def add_tool(self, tool: Tool):
        """Add a tool to the manifest"""
        self.tools.append(tool)
    
    def build(self, output_path: str, checksum_type: int = CHECKSUM_CRC32):
        """Generate the binary manifest file"""
        # Sort by priority (lower = higher priority)
        self.tools.sort(key=lambda t: (t.priority, t.name))
        
        # Calculate offsets
        header_size = 64
        index_size = len(self.tools) * 360
        index_offset = header_size
        detail_offset = header_size + index_size
        
        # Pre-calculate detail offsets and sizes
        current_detail_offset = detail_offset
        tool_details = []
        
        for tool in self.tools:
            detail_data = tool.to_detail()
            tool_details.append({
                'offset': current_detail_offset,
                'size': len(detail_data),
                'data': detail_data
            })
            current_detail_offset += len(detail_data)
        
        # Write file
        with open(output_path, 'wb') as f:
            # Header
            header = bytearray(64)
            struct.pack_into('<I', header, 0, TOOL_MANIFEST_MAGIC)
            struct.pack_into('<I', header, 4, TOOL_MANIFEST_VERSION)
            struct.pack_into('<I', header, 8, len(self.tools))
            struct.pack_into('<I', header, 12, index_offset)
            struct.pack_into('<I', header, 16, detail_offset)
            header[20] = checksum_type
            struct.pack_into('<Q', header, 24, 0)  # timestamp (unused)
            struct.pack_into('<Q', header, 32, current_detail_offset)  # file_size
            
            f.write(header)
            
            # Index
            for i, tool in enumerate(self.tools):
                index_entry = tool.to_index_entry(
                    tool_details[i]['offset'],
                    tool_details[i]['size']
                )
                f.write(index_entry)
            
            # Details
            for detail_info in tool_details:
                f.write(detail_info['data'])
            
        # Calculate and append checksum
        if checksum_type == CHECKSUM_CRC32:
            with open(output_path, 'rb') as f:
                data = f.read()
            crc = self._crc32(data)
            with open(output_path, 'ab') as f:
                f.write(struct.pack('<I', crc))
        elif checksum_type == CHECKSUM_SHA256:
            with open(output_path, 'rb') as f:
                data = f.read()
            sha = hashlib.sha256(data).digest()
            with open(output_path, 'ab') as f:
                f.write(sha)
        
        print(f"Generated manifest: {output_path}")
        print(f"  Tools: {len(self.tools)}")
        print(f"  File size: {current_detail_offset} bytes")
        print(f"  Checksum: {['None', 'CRC32', 'SHA256'][checksum_type]}")
    
    @staticmethod
    def _crc32(data: bytes) -> int:
        """Calculate CRC32 checksum (IEEE 802.3)"""
        crc = 0xFFFFFFFF
        poly = 0xEDB88320
        
        for byte in data:
            crc ^= byte
            for _ in range(8):
                if crc & 1:
                    crc = (crc >> 1) ^ poly
                else:
                    crc >>= 1
        
        return crc ^ 0xFFFFFFFF

def load_tools_from_source(src_dir: Path) -> List[Tool]:
    """
    Extract tool definitions from source code.
    
    This is a simple implementation that looks for tool registration patterns.
    For production, you might want to parse actual C source or use a JSON config.
    """
    tools = []
    
    # Example: Define tools manually (in production, parse from source)
    
    # Compute tools
    compute_tool = Tool(
        "compute",
        "Perform mathematical calculations and data analysis",
        category="computation",
        priority=2
    )
    compute_tool.description_full = "Evaluates mathematical expressions, performs statistical analysis, and handles numeric computations. Supports basic arithmetic, trigonometry, statistics, and unit conversions."
    compute_tool.usage_example = '<tool_call name="compute" expression="sqrt(144) + sin(pi/2)" />'
    compute_tool.add_parameter(ToolParameter("expression", "string", required=True))
    compute_tool.add_trigger("calculate")
    compute_tool.add_trigger("compute")
    compute_tool.add_trigger("what is")
    tools.append(compute_tool)
    
    # Memory tools
    memory_tool = Tool(
        "memory_store",
        "Store information for later recall",
        category="memory",
        priority=1
    )
    memory_tool.description_full = "Stores facts, preferences, and context in long-term memory. Use this to remember important information across conversations."
    memory_tool.usage_example = '<tool_call name="memory_store" key="user_name" value="Alice" category="personal" />'
    memory_tool.add_parameter(ToolParameter("key", "string", required=True))
    memory_tool.add_parameter(ToolParameter("value", "string", required=True))
    memory_tool.add_parameter(ToolParameter("category", "string", default="general"))
    memory_tool.add_trigger("remember")
    memory_tool.add_trigger("store")
    tools.append(memory_tool)
    
    memory_recall = Tool(
        "memory_recall",
        "Retrieve previously stored information",
        category="memory",
        priority=1
    )
    memory_recall.description_full = "Queries long-term memory to retrieve stored facts, preferences, and context."
    memory_recall.usage_example = '<tool_call name="memory_recall" query="user name" />'
    memory_recall.add_parameter(ToolParameter("query", "string", required=True))
    memory_recall.add_trigger("recall")
    memory_recall.add_trigger("what did I say about")
    tools.append(memory_recall)
    
    # File tools
    file_read = Tool(
        "file_read",
        "Read contents of a file",
        category="file_ops",
        priority=3
    )
    file_read.description_full = "Reads and returns the contents of a text file from the filesystem."
    file_read.usage_example = '<tool_call name="file_read" path="/path/to/file.txt" />'
    file_read.add_parameter(ToolParameter("path", "string", required=True))
    file_read.add_trigger("read file")
    file_read.add_trigger("show me file")
    tools.append(file_read)
    
    file_write = Tool(
        "file_write",
        "Write content to a file",
        category="file_ops",
        priority=3
    )
    file_write.description_full = "Writes or appends content to a text file on the filesystem."
    file_write.usage_example = '<tool_call name="file_write" path="/path/to/file.txt" content="Hello" mode="write" />'
    file_write.add_parameter(ToolParameter("path", "string", required=True))
    file_write.add_parameter(ToolParameter("content", "string", required=True))
    file_write.add_parameter(ToolParameter("mode", "string", default="write"))
    file_write.add_trigger("write to file")
    file_write.add_trigger("save to")
    tools.append(file_write)
    
    # System tools
    quit_tool = Tool(
        "/quit",
        "Exit the assistant",
        category="system",
        priority=0
    )
    quit_tool.description_full = "Gracefully terminates the voice assistant session."
    quit_tool.usage_example = '<tool_call name="/quit" />'
    quit_tool.add_trigger("quit")
    quit_tool.add_trigger("exit")
    quit_tool.add_trigger("goodbye")
    tools.append(quit_tool)
    
    help_tool = Tool(
        "/help",
        "Show available commands and usage",
        category="system",
        priority=0
    )
    help_tool.description_full = "Displays help information about available tools and commands."
    help_tool.usage_example = '<tool_call name="/help" topic="memory" />'
    help_tool.add_parameter(ToolParameter("topic", "string", default=""))
    help_tool.add_trigger("help")
    help_tool.add_trigger("what can you do")
    tools.append(help_tool)
    
    return tools

def main():
    import argparse
    
    parser = argparse.ArgumentParser(description="Generate binary tool manifest")
    parser.add_argument('-o', '--output', default='tools.bin', 
                       help='Output manifest file (default: tools.bin)')
    parser.add_argument('-s', '--source', default='src/plugins',
                       help='Source directory for tool definitions')
    parser.add_argument('--checksum', choices=['none', 'crc32', 'sha256'], 
                       default='crc32', help='Checksum algorithm')
    
    args = parser.parse_args()
    
    checksum_map = {'none': CHECKSUM_NONE, 'crc32': CHECKSUM_CRC32, 'sha256': CHECKSUM_SHA256}
    
    # Load tools
    src_path = Path(args.source)
    tools = load_tools_from_source(src_path)
    
    if not tools:
        print("Warning: No tools found!", file=sys.stderr)
        return 1
    
    # Build manifest
    builder = ManifestBuilder()
    for tool in tools:
        builder.add_tool(tool)
    
    builder.build(args.output, checksum_map[args.checksum])
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
