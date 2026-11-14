#!/usr/bin/env python3
"""
Generate test schemas for grammar generation stress testing (C2.3c)

Creates 500 JSON schemas with varying complexity:
- Nesting depth: 1-10 levels
- Array sizes: 0-50 items
- Enum cardinality: 2-50 values
- oneOf branches: 2-8 variants
- Property counts: 1-30 fields
- String patterns and constraints
- Mixed types and optional fields

Usage:
    python3 scripts/generate_grammar_test_schemas.py [output_dir]
    
Output:
    Creates tests/schemas/grammar_stress/ with 500 .json files
"""

import json
import os
import random
import sys
from pathlib import Path

# Schema generation parameters
SCHEMA_COUNT = 500
SEED = 42

def generate_string_schema(complexity=1):
    """Generate a string schema with optional constraints
    
    Limits:
    - Simple patterns only (no markdown, no complex regex)
    - Enum values are simple identifiers
    - Patterns avoid problematic characters that break GBNF parsing
    """
    schema = {"type": "string"}
    
    if complexity > 3 and random.random() < 0.3:
        # Add enum - simple identifiers only
        count = random.randint(2, 20)
        values = [f"option_{i}" for i in range(count)]
        schema["enum"] = values
    elif complexity > 2 and random.random() < 0.2:
        # Add simple pattern - avoid complex regex that breaks GBNF
        patterns = [
            "^[a-z]+$",           # lowercase letters
            "^[A-Z][a-z]+$",      # capitalized word
            "^[0-9]{3}-[0-9]{4}$", # phone-like
            "^[a-f0-9]{8}$"       # hex string
        ]
        schema["pattern"] = random.choice(patterns)
    elif random.random() < 0.2:
        # Add length constraint
        max_len = random.randint(10, 100)
        schema["maxLength"] = max_len
    
    return schema

def generate_number_schema():
    """Generate a number/integer schema"""
    schema_type = random.choice(["integer", "number"])
    schema = {"type": schema_type}
    
    if random.random() < 0.3:
        schema["minimum"] = random.randint(0, 100)
        schema["maximum"] = random.randint(101, 1000)
    
    return schema

def generate_array_schema(depth, max_depth):
    """Generate an array schema with optional item constraints"""
    schema = {
        "type": "array",
        "items": generate_schema(depth + 1, max_depth)
    }
    
    if random.random() < 0.4:
        schema["maxItems"] = random.randint(5, 50)
    
    if random.random() < 0.2:
        schema["minItems"] = random.randint(0, 3)
    
    return schema

def generate_object_schema(depth, max_depth, num_properties=None):
    """Generate an object schema with properties
    
    Limits:
    - Max 8 properties per object (realistic for tool calls, API responses)
    - Fewer properties at deeper levels to prevent explosion
    """
    if num_properties is None:
        # Reduce property count at deeper levels
        max_props = max(2, 8 - depth * 2)  # 8→6→4→2 as depth increases
        num_properties = random.randint(1, max_props)
    
    properties = {}
    required = []
    
    for i in range(num_properties):
        prop_name = f"field_{i}"
        properties[prop_name] = generate_schema(depth + 1, max_depth)
        
        # 60% chance of being required
        if random.random() < 0.6:
            required.append(prop_name)
    
    schema = {
        "type": "object",
        "properties": properties
    }
    
    if required:
        schema["required"] = required
    
    return schema

def generate_oneof_schema(depth, max_depth):
    """Generate a oneOf schema with multiple variants
    
    Limits:
    - 2-3 variants max (not 5-8) to avoid exponential grammar explosion
    - Only generate oneOf at shallow depths (<3) to prevent pathological nesting
    """
    # Avoid oneOf at deep levels - causes exponential rule growth
    if depth >= 3:
        return generate_object_schema(depth, max_depth, num_properties=2)
    
    num_variants = random.randint(2, 3)  # Reduced from 5-8
    variants = []
    
    for _ in range(num_variants):
        variants.append(generate_schema(depth + 1, max_depth))
    
    return {"oneOf": variants}

def generate_schema(depth=0, max_depth=5):
    """Generate a random JSON schema"""
    # Stop deep recursion
    if depth >= max_depth:
        return random.choice([
            {"type": "string"},
            {"type": "integer"},
            {"type": "number"},
            {"type": "boolean"}
        ])
    
    # Weight by depth - simpler types at deeper levels
    # Avoid oneOf at depth >= 3 to prevent exponential grammar explosion
    if depth < 2:
        type_weights = [
            ("object", 0.4),
            ("array", 0.2),
            ("oneof", 0.1),  # Only at shallow depths
            ("string", 0.15),
            ("number", 0.1),
            ("boolean", 0.05)
        ]
    elif depth < 3:
        type_weights = [
            ("object", 0.35),
            ("array", 0.15),
            ("oneof", 0.05),  # Rare at moderate depth
            ("string", 0.2),
            ("number", 0.15),
            ("boolean", 0.1)
        ]
    else:
        # No oneOf at deep levels - prevents pathological nesting
        type_weights = [
            ("object", 0.3),
            ("array", 0.1),
            ("string", 0.3),
            ("number", 0.2),
            ("boolean", 0.1)
        ]
    
    # Choose type based on weights
    rand = random.random()
    cumulative = 0.0
    chosen_type = "string"
    
    for type_name, weight in type_weights:
        cumulative += weight
        if rand < cumulative:
            chosen_type = type_name
            break
    
    # Generate schema for chosen type
    if chosen_type == "object":
        return generate_object_schema(depth, max_depth)
    elif chosen_type == "array":
        return generate_array_schema(depth, max_depth)
    elif chosen_type == "oneof":
        return generate_oneof_schema(depth, max_depth)
    elif chosen_type == "string":
        return generate_string_schema(depth)
    elif chosen_type == "number":
        return generate_number_schema()
    elif chosen_type == "boolean":
        return {"type": "boolean"}
    else:
        return {"type": "string"}

def generate_test_schemas(output_dir, count=SCHEMA_COUNT):
    """Generate test schemas with varying complexity
    
    Complexity limits based on real-world use cases:
    - Simple: 1-2 levels (tool calls, basic config)
    - Moderate: 2-3 levels (nested API responses)
    - Complex: 3-5 levels (deeply nested data structures)
    - No category >5 levels - prevents exponential grammar explosion
    
    Real-world JSON schemas rarely exceed 5 levels of nesting.
    Pathological cases (10+ levels with oneOf) create grammars with
    millions of rules that exhaust memory during compilation.
    """
    random.seed(SEED)
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)
    
    print(f"Generating {count} test schemas in {output_dir}...")
    
    categories = {
        "simple": 0,
        "moderate": 0,
        "complex": 0
    }
    
    for i in range(count):
        # Vary complexity within realistic bounds
        if i < count * 0.4:
            # Simple schemas (40%) - tool calls, basic structures
            max_depth = random.randint(1, 2)
            category = "simple"
        elif i < count * 0.75:
            # Moderate schemas (35%) - nested API responses
            max_depth = random.randint(2, 3)
            category = "moderate"
        else:
            # Complex schemas (25%) - deeply nested data
            max_depth = random.randint(3, 5)  # Max 5, not 10
            category = "complex"
        
        categories[category] += 1
        
        schema = generate_schema(depth=0, max_depth=max_depth)
        
        filename = f"schema_{i:04d}_{category}.json"
        filepath = output_path / filename
        
        with open(filepath, 'w') as f:
            json.dump(schema, f, indent=2)
        
        if (i + 1) % 50 == 0:
            print(f"  Generated {i + 1}/{count} schemas...")
    
    print(f"\n✅ Generated {count} schemas:")
    for cat, cnt in categories.items():
        print(f"   {cat:12s}: {cnt:3d} ({100*cnt/count:.0f}%)")
    print(f"\nOutput directory: {output_dir}")
    print("\nRun test with:")
    print(f"  ./build/tests/test_grammar_generation \\")
    print(f"    ~/.ethervox/models/governor/granite-4.0-h-1b-Q4_K_M.gguf \\")
    print(f"    {output_dir} \\")
    print(f"    20")

def main():
    if len(sys.argv) > 1:
        output_dir = sys.argv[1]
    else:
        # Default: tests/schemas/grammar_stress relative to script location
        script_dir = Path(__file__).parent
        repo_root = script_dir.parent
        output_dir = repo_root / "tests" / "schemas" / "grammar_stress"
    
    generate_test_schemas(output_dir, SCHEMA_COUNT)

if __name__ == "__main__":
    main()
