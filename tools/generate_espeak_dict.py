#!/usr/bin/env python3
"""
Generate espeak-compatible pronunciation dictionary for offline training.

LICENSING NOTE - GPL-Safe Usage:
  This is a development tool only, NOT distributed software. It uses espeak-ng
  (GPL) locally to generate pronunciation data (facts, not copyrightable code).
  The generated .dict files are pure data and can be embedded under MIT license.
  Espeak-ng is NEVER linked or distributed with EthervoxAI.

This tool uses espeak-ng directly (via subprocess) to generate NATIVE phoneme
pronunciations, NOT IPA. This is what Piper TTS expects!

Usage:
    python3 generate_espeak_dict.py --lang en-us --output data/espeak_en_us.dict
    python3 generate_espeak_dict.py --lang de --output data/espeak_de.dict

Requirements:
    espeak-ng must be installed and in PATH
"""

import argparse
import sys
import os
import signal
import subprocess
import multiprocessing
from multiprocessing import cpu_count, Pool
from pathlib import Path
from functools import partial

# Global flag for graceful shutdown
_interrupted = False

def _get_pronunciations_batch(words, lang):
    """Get IPA pronunciations for a batch of words using espeak-ng (module-level for pickling)."""
    results = []
    for word in words:
        try:
            # Use --ipa for IPA format (what Piper ACTUALLY expects!)
            # Verified: Piper models have ə (IPA), not @ (X-SAMPA) in phoneme_id_map
            result = subprocess.run(
                ['espeak-ng', '-v', lang, '--ipa', '-q', word],
                capture_output=True,
                text=True,
                timeout=2
            )
            ipa = result.stdout.strip()
            results.append((word, ipa if ipa else None))
        except:
            results.append((word, None))
    return results

def _signal_handler(signum, frame):
    """Handle Ctrl+C gracefully."""
    global _interrupted
    _interrupted = True
    print("\n\n⚠️  Interrupted by user. Cleaning up...", file=sys.stderr)
    sys.exit(130)  # Standard exit code for SIGINT

def load_vocabulary(lang):
    """Load comprehensive word list for the target language."""
    vocab = set()
    
    # 1. CMU Dictionary words (English)
    cmu_path = Path(__file__).parent.parent / "src/tts/phonemizer/data/cmudict-0.7b.txt"
    if lang.startswith('en') and cmu_path.exists():
        with open(cmu_path, 'r', encoding='latin-1') as f:
            for line in f:
                if line.startswith(';;;'):
                    continue
                word = line.split()[0].split('(')[0]  # Remove (n) variants
                vocab.add(word.lower())
        print(f"Loaded {len(vocab)} words from CMU dictionary")
    
    # 2. Common words list (language-specific)
    common_words = {
        'en': ['the', 'be', 'to', 'of', 'and', 'a', 'in', 'that', 'have', 'I',
               'it', 'for', 'not', 'on', 'with', 'he', 'as', 'you', 'do', 'at',
               'hello', 'world', 'voice', 'assistant', 'computer', 'phone',
               'please', 'thank', 'sorry', 'yes', 'no', 'okay', 'help'],
        'de': ['der', 'die', 'das', 'und', 'ist', 'nicht', 'ein', 'eine',
               'hallo', 'welt', 'bitte', 'danke', 'ja', 'nein', 'gut'],
        'cmn': ['你好', '世界', '语音', '助手', '电脑', '手机', '谢谢', '对不起'],
        'es': ['el', 'la', 'de', 'que', 'y', 'a', 'en', 'un', 'ser', 'se',
               'hola', 'mundo', 'por favor', 'gracias', 'sí', 'no'],
    }
    
    # Map language code to common words key
    lang_prefix = lang.split('-')[0]
    if lang == 'cmn' or lang.startswith('zh'):
        lang_prefix = 'cmn'
    if lang_prefix in common_words:
        vocab.update(common_words[lang_prefix])
        print(f"Added {len(common_words[lang_prefix])} common {lang_prefix} words")
    
    # 3. Add contractions and variants for English
    if lang.startswith('en'):
        contractions = ["don't", "can't", "won't", "I'm", "you're", "it's", 
                       "we're", "they're", "isn't", "aren't", "wasn't", "weren't"]
        vocab.update(contractions)
    
    return sorted(vocab)

def generate_pronunciations(words, lang, batch_size=100, num_workers=None):
    """Use espeak-ng directly to generate NATIVE X-SAMPA phonemes (what Piper TTS expects)."""
    
    # Map our language codes to espeak language codes
    # Support both simple codes and full espeak variants
    lang_map = {
        'en-us': 'en-us',
        'en-gb': 'en-gb',
        'en-gb-rp': 'en-gb-x-rp',  # British Received Pronunciation (standard BBC)
        'de': 'de',
        'cmn': 'cmn',  # Mandarin Chinese (use native espeak code)
        'zh': 'cmn',   # Alias for Mandarin
        'es-419': 'es-419',  # Latin American Spanish (use native espeak code)
        'es-mx': 'es-419',   # Mexican Spanish (same as Latin American)
    }
    
    espeak_lang = lang_map.get(lang, lang)
    
    # Determine number of workers
    if num_workers is None:
        num_workers = max(1, cpu_count() - 1)  # Leave one core free
    
    print(f"Generating {len(words)} pronunciations for {espeak_lang}...")
    print(f"  Using espeak-ng IPA format (--ipa flag)")
    print(f"  Verified: Piper models expect IPA (ə not @, ˈ not ', ð not D)")
    print(f"  Parallel workers: {num_workers}, batch size: {batch_size} words/worker")
    print(f"  This may take 5-10 minutes...")
    
    pronunciations = {}
    total_words = len(words)
    processed = 0
    
    # Split words into chunks for parallel processing
    # Each worker gets batch_size words to process
    chunks = [words[i:i+batch_size] for i in range(0, len(words), batch_size)]
    
    # Use multiprocessing pool
    with multiprocessing.Pool(num_workers) as pool:
        worker = partial(_get_pronunciations_batch, lang=espeak_lang)
        
        try:
            # Map chunks to workers
            for batch_results in pool.imap_unordered(worker, chunks):
                for word, phonemes in batch_results:
                    if phonemes:
                        pronunciations[word] = phonemes
                
                processed += len(batch_results)
                progress_pct = (processed / total_words) * 100
                
                # Show progress
                if processed <= 5000 or processed % 1000 == 0:
                    print(f"  Processed {processed}/{total_words} words ({progress_pct:.1f}%)...", flush=True)
                
        except KeyboardInterrupt:
            print("\n\n⚠️  Interrupted by user. Stopping...", file=sys.stderr)
            pool.terminate()
        except Exception as e:
            print(f"  WARNING: Error: {e}", file=sys.stderr)
    
    print(f"Generated {len(pronunciations)} pronunciations")
    return pronunciations

def read_dictionary(input_path):
    """Read existing dictionary file and return pronunciations dict."""
    pronunciations = {}
    input_path = Path(input_path)
    
    if not input_path.exists():
        return None
    
    with open(input_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            
            parts = line.split('\t')
            if len(parts) == 2:
                word, ipa = parts
                # Strip frequency suffix if present (e.g., "word 1234" → "word")
                word = word.split()[0] if ' ' in word else word
                pronunciations[word] = ipa
    
    print(f"📖 Read {len(pronunciations)} entries from {input_path}")
    return pronunciations

def write_dictionary(pronunciations, output_path):
    """Write dictionary in our format: WORD<tab>PHONEMES."""
    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("# espeak-generated pronunciaIPA format (--ipa) for Piper TTS\n")
        f.write(f"# Format: WORD<tab>IPA_PRONUNCIATION-SAMPA format for Piper TTS\n")
        f.write(f"# Format: WORD<tab>X-SAMPA_PHONEMES\n\n")
        
        for word in sorted(pronunciations.keys()):
            ipa = pronunciations[word]
            f.write(f"{word}\t{ipa}\n")
    
    print(f"✅ Wrote {len(pronunciations)} entries to {output_path}")
    print(f"   File size: {output_path.stat().st_size / 1024:.1f} KB")

def generate_c_header(pronunciations, output_path, lang):
    """Generate C header with embedded dictionary data."""
    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    # Generate variable name from language
    var_name = lang.replace('-', '_').replace('.', '_')
    guard_name = var_name.upper()
    
    # Sort dictionary for binary search
    sorted_words = sorted(pronunciations.keys(), key=str.lower)
    
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(f"""/**
 * @file espeak_dict_{var_name}.h
 * @brief Espeak-trained pronunciation dictionary for {lang}
 * 
 * AUTO-GENERATED - DO NOT EDIT
 * Generated using espeak-ng for offline training.
 * 
 * This is training data derived from espeak-ng output, NOT espeak source code.
 * Dictionary is sorted alphabetically (case-insensitive) for binary search.
 * 
 * Generation date: {Path(__file__).stat().st_mtime}
 * Dictionary size: {len(pronunciations)} entries
 */

#ifndef ESPEAK_DICT_{guard_name}_H
#define ESPEAK_DICT_{guard_name}_H

#include "../espeak_dict.h"

#ifdef __cplusplus
extern "C" {{
#endif

#define ESPEAK_DICT_{guard_name}_ENABLED 1

const espeak_dict_entry_t espeak_dict_{var_name}[] = {{
""")
        
        for word in sorted_words:
            ipa = pronunciations[word].replace('"', '\\"').replace('\\', '\\\\')
            word_escaped = word.replace('"', '\\"').replace('\\', '\\\\')
            f.write(f'    {{"{word_escaped}", "{ipa}"}},\n')
        
        f.write(f"""    {{NULL, NULL}}  // Sentinel
}};

const size_t espeak_dict_{var_name}_size = {len(pronunciations)};

#ifdef __cplusplus
}}
#endif

#endif  // ESPEAK_DICT_{guard_name}_H
""")
    
    print(f"✅ Wrote C header to {output_path}")
    print(f"   Dictionary size: {len(pronunciations)} entries (sorted)")
    print(f"   Binary size estimate: ~{len(pronunciations) * 40 / 1024:.1f} KB")

def main():
    # Setup signal handler for main process
    signal.signal(signal.SIGINT, _signal_handler)
    
    parser = argparse.ArgumentParser(
        description='Generate espeak-compatible pronunciation dictionary',
        epilog='Example: python3 generate_espeak_dict.py --lang en-us --output data/espeak_en_us.dict'
    )
    parser.add_argument('--lang', required=True, 
                       choices=['en-us', 'en-gb-rp', 'de', 'cmn', 'es-419'],
                       help='Target language (espeak variant)')
    parser.add_argument('--output', required=True,
                       help='Output dictionary file path')
    parser.add_argument('--format', choices=['dict', 'header'], default='dict',
                       help='Output format: dict (text) or header (C header)')
    parser.add_argument('--vocab-file', 
                       help='Additional vocabulary file (one word per line)')
    parser.add_argument('--workers', type=int, default=None,
                       help='Number of parallel workers (default: CPU count - 1)')
    parser.add_argument('--dict-file',
                       help='Read from existing .dict file instead of regenerating')
    
    args = parser.parse_args()
    
    # Check if we should read from existing dict file
    if args.dict_file:
        pronunciations = read_dictionary(args.dict_file)
        if pronunciations is None:
            print(f"❌ Dictionary file not found: {args.dict_file}", file=sys.stderr)
            sys.exit(1)
    else:
        # Load vocabulary
        vocab = load_vocabulary(args.lang)
        
        # Add custom vocabulary if provided
        if args.vocab_file:
            with open(args.vocab_file, 'r', encoding='utf-8') as f:
                custom_words = [line.strip() for line in f if line.strip()]
                vocab.extend(custom_words)
                print(f"Added {len(custom_words)} custom words")
        
        # Generate pronunciations using espeak
        pronunciations = generate_pronunciations(vocab, args.lang, num_workers=args.workers)
    
    # Write output
    if args.format == 'header':
        # Also generate .dict file alongside header
        dict_path = Path(args.output).with_suffix('.dict')
        write_dictionary(pronunciations, dict_path)
        generate_c_header(pronunciations, args.output, args.lang)
    else:
        write_dictionary(pronunciations, args.output)
    
    print("\n✅ Dictionary generation complete!")
    print(f"   Use this data in your phonemizer for fast, espeak-compatible lookups")

if __name__ == '__main__':
    main()
