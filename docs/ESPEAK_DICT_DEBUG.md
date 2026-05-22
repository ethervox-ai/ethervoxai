# Espeak Dictionary Debug Guide

## Overview

The espeak dictionary lookup has comprehensive debug logging to help diagnose pronunciation issues. This guide explains how to enable and interpret the debug output.

## Enabling Debug Logging

Set the log level to DEBUG to see dictionary lookups:

```c
#include "ethervox/logging.h"

// In your initialization code:
ethervox_log_set_level(ETHERVOX_LOG_LEVEL_DEBUG);
```

Or via command-line flag (if supported in your build):
```bash
./your_app --log-level=DEBUG
```

## Debug Output Format

### Phonemizer Flow

For each word being phonemized, you'll see the 4-tier lookup priority:

```
[Phonemizer] 🔍 Trying espeak dictionary lookup for 'hello' (language=0)
[Phonemizer] 📚 Searching en-us espeak dict (126054 entries)...
[EspeakDict] 🔍 Looking up 'hello' in dictionary (size=126054 entries)
[EspeakDict]   iter=1: left=0, mid=63027, right=126053, entries[mid].word='infringement', cmp=-1
[EspeakDict]   iter=2: left=0, mid=31513, right=63026, entries[mid].word='dunning', cmp=1
[EspeakDict]   iter=3: left=31514, mid=47270, right=63026, entries[mid].word='handout', cmp=0
[EspeakDict] ✅ Found 'hello' → 'həˈloʊ' (after 3 iterations)
[Phonemizer] ✅ Espeak dict (en-us): 'hello' → 'həˈloʊ'
```

### Lookup Success Indicators

| Symbol | Meaning |
|--------|---------|
| 🔍 | Starting lookup |
| 📚 | Searching specific dictionary (en-us/en-gb/de) |
| ✅ | Found pronunciation |
| ❌ | Not found, trying next priority |
| ⚠️ | Warning or fallback |

### Priority System

1. **User Overrides** (1st priority):
   ```
   [Phonemizer] Using phonemes from override (will convert): 'HH AH0 L OW1'
   ```

2. **Espeak Dictionary** (2nd priority):
   ```
   [Phonemizer] ✅ Espeak dict (en-us): 'hello' → 'həˈloʊ'
   ```

3. **CMU Dictionary** (3rd priority):
   ```
   [Phonemizer] Dictionary found 'hello' → 'HH AH0 L OW1'
   ```

4. **G2P Rules** (4th priority, fallback):
   ```
   [Phonemizer] Using G2P fallback for 'xyzzyx'
   ```

## Common Debug Scenarios

### Case 1: Word Not Found

```
[Phonemizer] 🔍 Trying espeak dictionary lookup for 'xylophone' (language=0)
[Phonemizer] 📚 Searching en-us espeak dict (126054 entries)...
[EspeakDict] 🔍 Looking up 'xylophone' in dictionary (size=126054 entries)
[EspeakDict]   iter=1: left=0, mid=63027, right=126053, entries[mid].word='infringement', cmp=1
[EspeakDict]   iter=2: left=63028, mid=94540, right=126053, entries[mid].word='tangerine', cmp=1
[EspeakDict]   iter=17: left=122045, mid=122045, right=122045, entries[mid].word='xylophone', cmp=0
[EspeakDict] ✅ Found 'xylophone' → 'ˈzaɪləˌfoʊn' (after 17 iterations)
[Phonemizer] ✅ Espeak dict (en-us): 'xylophone' → 'ˈzaɪləˌfoʊn'
```

### Case 2: Dictionary Miss, Fallback to CMU

```
[Phonemizer] 🔍 Trying espeak dictionary lookup for 'uncommon' (language=0)
[Phonemizer] 📚 Searching en-us espeak dict (126054 entries)...
[EspeakDict] ❌ Not found: 'uncommon' (searched 18 iterations, final left=98456, right=98455)
[Phonemizer] ❌ Not found in en-us espeak dict: 'uncommon'
[Phonemizer] ⚠️  Espeak lookup failed for 'uncommon', falling back to next priority
[Phonemizer] Dictionary found 'uncommon' → 'AH0 N K AA1 M AH0 N'
```

### Case 3: All Dictionaries Miss, G2P Fallback

```
[Phonemizer] 🔍 Trying espeak dictionary lookup for 'supercalifragilisticexpialidocious' (language=0)
[EspeakDict] ❌ Not found: 'supercalifragilisticexpialidocious' (searched 20 iterations)
[Phonemizer] ❌ Not found in en-us espeak dict
[Phonemizer] Dictionary lookup failed for 'supercalifragilisticexpialidocious'
[Phonemizer] Using G2P fallback...
```

### Case 4: Invalid Parameters

```
[EspeakDict] Lookup failed: invalid parameters (entries=(nil), word=(nil), ipa_out=0x7fff..., size=0, max_len=512)
```

This indicates a programming error - check that the dictionary is properly loaded.

### Case 5: Dictionary Not Compiled In

```
[Phonemizer] ⚠️  ESPEAK_DICT_EN_US_ENABLED not defined
```

This means the espeak dictionary was not enabled at compile time. Check CMakeLists.txt:
```cmake
option(ENABLE_ESPEAK_DICT "Enable embedded espeak dictionaries" ON)
```

## Performance Analysis

### Binary Search Efficiency

Expected iterations for 126,054 entries:
- **Best case**: 1 iteration (exact middle)
- **Average case**: ~17 iterations (log₂(126054) ≈ 16.9)
- **Worst case**: 17 iterations

Example from logs:
```
[EspeakDict] ✅ Found 'hello' → 'həˈloʊ' (after 17 iterations)
```

If you see significantly more iterations, the dictionary may not be properly sorted.

### Lookup Time

Typical O(log n) performance:
- ~5μs per lookup on modern hardware
- ~17 iterations × ~300ns per comparison ≈ 5.1μs

## Troubleshooting Guide

### Problem: Speaking quality degraded after adding espeak dictionaries

**Symptoms**:
```
[Phonemizer] ✅ Espeak dict (en-us): 'hello' → 'həˈloʊ'
[TTS] Synthesizing phonemes: həˈloʊ
[TTS] Output sounds robotic/garbled
```

**Possible Causes**:

1. **IPA → ARPAbet conversion issues**
   - Check if TTS backend expects ARPAbet but receives IPA
   - Solution: Ensure `override_is_ipa = 1` flag is set correctly

2. **Stress markers incompatible with TTS**
   - Some TTS models don't handle IPA stress markers (ˈ, ˌ)
   - Solution: Strip stress markers or convert to ARPAbet format

3. **Unicode encoding issues**
   - IPA uses UTF-8 special characters
   - Solution: Verify TTS backend accepts UTF-8 input

**Debug Steps**:

1. Compare espeak output vs CMU output:
   ```
   [Phonemizer] ✅ Espeak dict: 'test' → 'tɛst'
   [Phonemizer] Dictionary found: 'test' → 'T EH1 S T'
   ```

2. Check if TTS prefers one format:
   - Temporarily disable espeak dict (undefine `ESPEAK_DICT_EN_US_ENABLED`)
   - Test speech quality with CMU dictionary only
   - If quality improves, TTS backend needs ARPAbet conversion

3. Verify IPA→ARPAbet conversion:
   ```c
   // In phonemizer.c, after espeak lookup:
   if (override_is_ipa) {
       ETHERVOX_LOG_DEBUG("[Phonemizer] Converting IPA '%s' to ARPAbet...", word_ipa);
       ipa_to_arpabet(word_ipa, arpabet, MAX_ARPABET_LENGTH);
       ETHERVOX_LOG_DEBUG("[Phonemizer] Converted to: '%s'", arpabet);
   }
   ```

### Problem: Dictionary lookups always failing

**Symptoms**:
```
[EspeakDict] ❌ Not found: 'hello' (searched 17 iterations, final left=X, right=Y)
```

**Causes**:
1. Dictionary not sorted alphabetically
2. Case-sensitivity mismatch
3. Dictionary size mismatch

**Debug**:
```bash
# Check if dictionary is sorted
head -20 src/tts/phonemizer/data/espeak_en_us.dict

# Should show alphabetical order:
# a ə
# aardvark ˈɑːdvɑːk
# aardvarks ˈɑːdvɑːks
# ...
```

### Problem: Crashes during lookup

**Symptoms**:
- Segmentation fault
- Out-of-bounds access

**Causes**:
1. Dictionary array size mismatch
2. Missing null terminator in strings
3. Uninitialized dictionary pointer

**Check**:
```c
// Verify in espeak_dict_data.c:
extern const size_t espeak_dict_en_us_size = sizeof(espeak_dict_en_us) / sizeof(espeak_dict_en_us[0]);
```

## Recommended Logging for Production

For production builds, use INFO or WARN level to avoid performance impact:

```c
ethervox_log_set_level(ETHERVOX_LOG_LEVEL_INFO);
```

Only enable DEBUG during:
- Development
- User-reported pronunciation issues
- Performance profiling

## Example Full Debug Session

```
[Phonemizer] Processing text: "Hello world"
[Phonemizer] Tokenized: [Hello] [world]

Token 1: "Hello"
[Phonemizer] 🔍 Trying espeak dictionary lookup for 'Hello' (language=0)
[Phonemizer] 📚 Searching en-us espeak dict (126054 entries)...
[EspeakDict] 🔍 Looking up 'Hello' in dictionary (size=126054 entries)
[EspeakDict]   iter=1: left=0, mid=63027, right=126053, entries[mid].word='infringement', cmp=-1
[EspeakDict]   iter=2: left=0, mid=31513, right=63026, entries[mid].word='dunning', cmp=1
[EspeakDict]   iter=3: left=31514, mid=47270, right=63026, entries[mid].word='hello', cmp=0
[EspeakDict] ✅ Found 'Hello' → 'həˈloʊ' (after 3 iterations)
[Phonemizer] ✅ Espeak dict (en-us): 'Hello' → 'həˈloʊ'

Token 2: "world"
[Phonemizer] 🔍 Trying espeak dictionary lookup for 'world' (language=0)
[Phonemizer] 📚 Searching en-us espeak dict (126054 entries)...
[EspeakDict] 🔍 Looking up 'world' in dictionary (size=126054 entries)
[EspeakDict]   iter=1: left=0, mid=63027, right=126053, entries[mid].word='infringement', cmp=1
[EspeakDict]   iter=2: left=63028, mid=94540, right=126053, entries[mid].word='tangerine', cmp=1
[EspeakDict]   iter=16: left=119034, mid=119034, right=119034, entries[mid].word='world', cmp=0
[EspeakDict] ✅ Found 'world' → 'wɜːld' (after 16 iterations)
[Phonemizer] ✅ Espeak dict (en-us): 'world' → 'wɜːld'

[Phonemizer] Final IPA: həˈloʊ wɜːld
[TTS] Synthesizing: həˈloʊ wɜːld
```

## See Also

- `docs/ESPEAK_DICT_LICENSING.md` - Legal aspects
- `docs/PHONEMIZER_TRAINING.md` - How to regenerate dictionaries
- `src/tts/phonemizer/phonemizer.c` - Lookup implementation
- `src/tts/phonemizer/espeak_dict.c` - Binary search algorithm
