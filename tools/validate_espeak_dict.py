#!/usr/bin/env python3
"""
Validate espeak dictionary accuracy by comparing against live espeak output.

This helps ensure the embedded dictionaries match espeak phonemization.
"""

import sys
import subprocess
import argparse
from pathlib import Path

def get_espeak_ipa(word, lang='en-us'):
    """Get IPA from espeak-ng."""
    try:
        result = subprocess.run(
            ['espeak-ng', '-v', lang, '-x', '--ipa=3', word],
            capture_output=True,
            text=True,
            timeout=5
        )
        if result.returncode == 0:
            return result.stdout.strip()
    except Exception as e:
        print(f"WARNING: espeak-ng failed for '{word}': {e}", file=sys.stderr)
    return None

def load_dict_file(path):
    """Load our generated dictionary file."""
    pronunciations = {}
    with open(path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split('\t')
            if len(parts) == 2:
                word, ipa = parts
                pronunciations[word.lower()] = ipa
    return pronunciations

def compare_dictionaries(dict_path, lang, sample_size=100):
    """Compare our dictionary against live espeak."""
    print(f"Loading dictionary: {dict_path}")
    our_dict = load_dict_file(dict_path)
    
    print(f"Testing {sample_size} random words against espeak-ng...")
    
    import random
    words = random.sample(list(our_dict.keys()), min(sample_size, len(our_dict)))
    
    matches = 0
    mismatches = []
    
    for word in words:
        our_ipa = our_dict[word]
        espeak_ipa = get_espeak_ipa(word, lang)
        
        if espeak_ipa:
            # Normalize for comparison (remove some espeak markers)
            our_normalized = our_ipa.replace(' ', '')
            espeak_normalized = espeak_ipa.replace(' ', '').replace('_', '')
            
            if our_normalized == espeak_normalized:
                matches += 1
            else:
                mismatches.append((word, our_ipa, espeak_ipa))
    
    accuracy = (matches / len(words)) * 100
    
    print(f"\n{'='*60}")
    print(f"Validation Results")
    print(f"{'='*60}")
    print(f"Tested:      {len(words)} words")
    print(f"Exact match: {matches} ({accuracy:.1f}%)")
    print(f"Mismatches:  {len(mismatches)}")
    
    if mismatches and len(mismatches) <= 20:
        print(f"\nMismatch examples:")
        for word, ours, espeak in mismatches[:20]:
            print(f"  {word:15s}  Ours: {ours:30s}  Espeak: {espeak}")
    
    print(f"\n{'='*60}")
    
    if accuracy >= 95:
        print("✅ PASS: Dictionary is highly accurate")
        return 0
    elif accuracy >= 85:
        print("⚠️  WARN: Dictionary has some differences (may be acceptable)")
        return 0
    else:
        print("❌ FAIL: Dictionary accuracy too low")
        return 1

def main():
    parser = argparse.ArgumentParser(description='Validate espeak dictionary accuracy')
    parser.add_argument('--dict', required=True, help='Path to dictionary file')
    parser.add_argument('--lang', default='en-us', help='Language code')
    parser.add_argument('--sample-size', type=int, default=100, 
                       help='Number of words to test')
    
    args = parser.parse_args()
    
    if not Path(args.dict).exists():
        print(f"ERROR: Dictionary not found: {args.dict}", file=sys.stderr)
        return 1
    
    return compare_dictionaries(args.dict, args.lang, args.sample_size)

if __name__ == '__main__':
    sys.exit(main())
