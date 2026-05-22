# Custom Phonemizer Implementation Plan

## Overview

Replace espeak-ng (GPL-3.0) with a custom, commercial-friendly phonemizer for text-to-IPA conversion in Piper TTS backend.

**License Target**: MIT/BSD-compatible (no GPL contamination)  
**Timeline**: 2-3 weeks for production-ready English support  
**Priority**: English → Chinese → German

---

## Architecture

### High-Level Pipeline

```
Text Input → Preprocessing → Dictionary Lookup → Rule-based Fallback → IPA Tokenization → Phoneme IDs
```

### File Structure

```
src/tts/phonemizer/
├── phonemizer.h              # Public API
├── phonemizer.c              # Core orchestration
├── dictionary.h/c            # Dictionary lookup engine
├── rules_en.h/c              # English G2P rules
├── rules_zh.h/c              # Chinese G2P rules (future)
├── rules_de.h/c              # German G2P rules (future)
└── data/
    ├── cmudict.txt           # CMU Pronouncing Dictionary (public domain)
    ├── exceptions_en.txt     # Common exceptions
    └── phoneme_maps/
        ├── arpabet_to_ipa.txt
        └── pinyin_to_ipa.txt
```

---

## Phase 1: English Phonemizer (Week 1-2)

### 1.1 Data Preparation

**CMU Pronouncing Dictionary**
- Source: http://www.speech.cs.cmu.edu/cgi-bin/cmudict (Public Domain)
- Format: `WORD  F OW N IY M` (ARPAbet)
- Size: ~134,000 entries
- License: Public domain

**Tasks**:
1. Download CMU Dict 0.7b
2. Convert ARPAbet → IPA mappings
3. Build hash table for O(1) lookup
4. Handle multiple pronunciations (select primary)

**ARPAbet → IPA Conversion Table** (subset):
```
AA → ɑ    (father)
AE → æ    (cat)
AH → ə    (about)
AO → ɔ    (dog)
AW → aʊ   (how)
AY → aɪ   (my)
B  → b
CH → tʃ
D  → d
DH → ð    (this)
...
```

### 1.2 Dictionary Lookup Engine

**API Design**:
```c
typedef struct {
    char word[64];
    char ipa[128];
    float frequency;  // For disambiguation
} dict_entry_t;

typedef struct phoneme_dict phoneme_dict_t;

// Initialize dictionary from embedded data or file
phoneme_dict_t* phoneme_dict_create(const char* dict_path);

// Lookup word, returns IPA string or NULL
const char* phoneme_dict_lookup(phoneme_dict_t* dict, const char* word);

// Case-insensitive lookup with normalization
const char* phoneme_dict_lookup_normalized(phoneme_dict_t* dict, const char* word);

void phoneme_dict_destroy(phoneme_dict_t* dict);
```

**Implementation Strategy**:
- Hash table with chaining (djb2 hash)
- Embedded C array for common words (~10K most frequent)
- Optional file loading for full dictionary
- Memory footprint: ~2-4MB for full dict

### 1.3 English G2P Rules

**Rule-based fallback for OOV (Out-of-Vocabulary) words**:

**Common patterns**:
```c
// Silent E: "make" → /meɪk/
if (ends_with(word, "e") && consonant(word[-2])) {
    // Drop final 'e', apply long vowel
}

// -tion ending: "nation" → /neɪʃən/
if (ends_with(word, "tion")) {
    return phonemize(stem) + "ʃən";
}

// -ing ending: "running" → /rʌnɪŋ/
if (ends_with(word, "ing")) {
    return phonemize(stem) + "ɪŋ";
}

// Double consonants: "letter" → /lɛtər/
// Soft 'c': "city" → /sɪti/
// Hard 'c': "cat" → /kæt/
```

**Letter-to-sound rules** (simplified):
- Single letter → phoneme (context-dependent)
- Digraphs: "th", "sh", "ch", "ph", etc.
- Vowel combinations: "ea", "oo", "ou", "ai", etc.
- Stress assignment heuristics

**Accuracy expectations**:
- Dictionary hits: 95%+ of common text
- Rule-based: 70-80% accuracy for OOV
- Combined: ~92% word-level accuracy

### 1.4 Text Preprocessing

```c
typedef struct {
    char* words[256];     // Tokenized words
    size_t word_count;
    bool is_sentence_end[256];
} text_tokens_t;

text_tokens_t* phonemizer_tokenize(const char* text);
```

**Preprocessing steps**:
1. Normalize Unicode (NFC)
2. Expand contractions: "don't" → "do not"
3. Handle numbers: "123" → "one hundred twenty three"
4. Handle abbreviations: "Dr." → "doctor"
5. Punctuation handling (preserve for prosody)
6. Case folding for lookup

### 1.5 IPA Assembly & Tokenization

**Convert word-level IPA → character-level tokens**:

```c
// Input:  "hello world" → "həˈloʊ wɝld"
// Output: ["h", "ə", "ˈ", "l", "o", "ʊ", " ", "w", "ɝ", "l", "d"]

int phonemizer_to_tokens(const char* ipa_string, 
                         char tokens[][8], 
                         size_t max_tokens);
```

**Special handling**:
- Stress markers: ˈ (primary), ˌ (secondary)
- Length markers: ː
- Word boundaries: space or period
- Syllable separators

### 1.6 Phonemizer API Interface

**Public API** (`src/tts/phonemizer/phonemizer.h`):

```c
#ifndef ETHERVOX_PHONEMIZER_H
#define ETHERVOX_PHONEMIZER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Supported languages for phonemization
 */
typedef enum {
    PHONEMIZER_LANG_UNKNOWN = 0,
    PHONEMIZER_LANG_EN_US,      // English (American)
    PHONEMIZER_LANG_EN_GB,      // English (British)
    PHONEMIZER_LANG_ZH_CN,      // Chinese (Mandarin, Simplified)
    PHONEMIZER_LANG_DE_DE,      // German (Standard)
    PHONEMIZER_LANG_ES_MX,      // Spanish (Mexican)
} phonemizer_language_t;

/**
 * Opaque phonemizer context
 */
typedef struct phonemizer_context phonemizer_t;

/**
 * Create phonemizer for specified language
 * 
 * @param lang_code Language identifier (e.g., "en-us", "zh", "de")
 * @return Phonemizer context or NULL on failure
 */
phonemizer_t* phonemizer_create(const char* lang_code);

/**
 * Convert text to IPA phoneme string
 * 
 * @param ctx Phonemizer context
 * @param text Input text (UTF-8)
 * @param ipa_output Buffer for IPA output (UTF-8)
 * @param max_len Maximum output buffer size
 * @return 0 on success, -1 on error
 * 
 * Output format: Space-separated IPA tokens with stress markers
 * Example: "hello world" → "h ə ˈ l o ʊ   w ɝ l d"
 */
int phonemizer_text_to_ipa(phonemizer_t* ctx,
                           const char* text,
                           char* ipa_output,
                           size_t max_len);

/**
 * Get phonemizer language
 */
phonemizer_language_t phonemizer_get_language(phonemizer_t* ctx);

/**
 * Destroy phonemizer and free resources
 */
void phonemizer_destroy(phonemizer_t* ctx);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_PHONEMIZER_H
```

### 1.7 Integration with Piper Backend

**Modified `text_to_phonemes()` in `src/tts/piper_backend.c`**:

```c
#include "phonemizer/phonemizer.h"

/**
 * Convert text to phonemes using custom phonemizer
 */
static int text_to_phonemes(piper_context_t* ctx, 
                            const char* text, 
                            int64_t* phoneme_ids, 
                            size_t* phoneme_count) {
    
    fprintf(stderr, "[Piper] Phonemizing text: '%s'\n", text);
    fprintf(stderr, "[Piper] Language: %s\n", ctx->piper_voice);
    
    // 1. Create phonemizer for language
    phonemizer_t* phonemizer = phonemizer_create(ctx->piper_voice);
    if (!phonemizer) {
        fprintf(stderr, "[Piper] ERROR: Failed to create phonemizer for language: %s\n", 
                ctx->piper_voice);
        return -1;
    }
    
    // 2. Convert text to IPA string
    char ipa_buffer[2048];
    int result = phonemizer_text_to_ipa(phonemizer, text, ipa_buffer, sizeof(ipa_buffer));
    if (result < 0) {
        fprintf(stderr, "[Piper] ERROR: Phonemization failed\n");
        phonemizer_destroy(phonemizer);
        return -1;
    }
    
    fprintf(stderr, "[Piper] IPA output: %s\n", ipa_buffer);
    
    // 3. Tokenize IPA string into individual phoneme characters
    char ipa_tokens[PIPER_MAX_PHONEMES][8];
    size_t token_count = tokenize_ipa(ipa_buffer, ipa_tokens, PIPER_MAX_PHONEMES);
    
    if (token_count == 0) {
        fprintf(stderr, "[Piper] ERROR: No IPA tokens generated\n");
        phonemizer_destroy(phonemizer);
        return -1;
    }
    
    fprintf(stderr, "[Piper] Generated %zu IPA tokens\n", token_count);
    
    // 4. Convert IPA tokens to phoneme IDs using model's phoneme_id_map
    size_t id_count = 0;
    
    // Add BOS token (^)
    phoneme_ids[id_count++] = phoneme_to_id(ctx, "^");
    
    for (size_t i = 0; i < token_count && id_count < PIPER_MAX_PHONEMES - 2; i++) {
        int id = phoneme_to_id(ctx, ipa_tokens[i]);
        
        // Debug output for first 10 tokens
        if (i < 10) {
            fprintf(stderr, "[Piper] Token '%s' → ID %d\n", ipa_tokens[i], id);
        }
        
        phoneme_ids[id_count++] = id;
    }
    
    // Add EOS token ($)
    phoneme_ids[id_count++] = phoneme_to_id(ctx, "$");
    
    *phoneme_count = id_count;
    
    fprintf(stderr, "[Piper] Final phoneme sequence (%zu IDs): [", id_count);
    for (size_t i = 0; i < (id_count < 20 ? id_count : 20); i++) {
        fprintf(stderr, "%lld%s", (long long)phoneme_ids[i], i < id_count-1 ? "," : "");
    }
    if (id_count > 20) fprintf(stderr, "...");
    fprintf(stderr, "]\n");
    
    // 5. Cleanup
    phonemizer_destroy(phonemizer);
    
    return 0;
}
```

### 1.8 Piper Model Requirements

**What Piper Expects**:

1. **Input Format**: Array of `int64_t` phoneme IDs
   - Must start with BOS token (usually ID 1, represented as "^")
   - Must end with EOS token (usually ID 2, represented as "$")
   - Phoneme IDs mapped via model's `phoneme_id_map` in `.onnx.json`

2. **IPA Token Format**: Individual UTF-8 characters
   - Each IPA phoneme is a separate token: "h", "ə", "ˈ", "l", "o", "ʊ"
   - Stress markers (ˈ, ˌ) are separate tokens
   - Length markers (ː) are separate tokens
   - Word boundaries represented as spaces or periods

3. **Model Config Parsing**: Already implemented in `load_phoneme_map()`
   ```json
   {
     "phonemes": {
       "language": "en-us"
     },
     "phoneme_id_map": {
       "_": [0],    // Padding token
       "^": [1],    // BOS (Beginning of Sequence)
       "$": [2],    // EOS (End of Sequence)
       " ": [3],    // Word boundary
       "h": [4],
       "ə": [5],
       "ˈ": [6],    // Primary stress
       // ... rest of IPA characters
     }
   }
   ```

4. **Existing Helper Functions**:
   - `phoneme_to_id()`: Maps IPA string → integer ID using loaded phoneme_id_map
   - `tokenize_ipa()`: Already implemented, splits IPA string into UTF-8 character tokens
   - `load_phoneme_map()`: Loads phoneme_id_map from model JSON config

**Key Requirements for Phonemizer**:
- ✅ Output must be **IPA characters** matching model's phoneme_id_map
- ✅ Include **stress markers** (ˈ for primary, ˌ for secondary)
- ✅ Use **word boundaries** (space character between words)
- ✅ Handle **UTF-8 encoding** correctly (multi-byte IPA characters)
- ✅ Be **deterministic** (same input → same output)

### 1.9 Language Code Mapping

**Map Piper model language codes to phonemizer**:

```c
static phonemizer_language_t parse_language_code(const char* lang_code) {
    if (strstr(lang_code, "en-us") || strstr(lang_code, "en_US")) {
        return PHONEMIZER_LANG_EN_US;
    } else if (strstr(lang_code, "en-gb") || strstr(lang_code, "en_GB")) {
        return PHONEMIZER_LANG_EN_GB;
    } else if (strstr(lang_code, "zh") || strstr(lang_code, "cmn")) {
        return PHONEMIZER_LANG_ZH_CN;
    } else if (strstr(lang_code, "de")) {
        return PHONEMIZER_LANG_DE_DE;
    } else if (strstr(lang_code, "es")) {
        return PHONEMIZER_LANG_ES_MX;
    }
    return PHONEMIZER_LANG_UNKNOWN;
}
```

**Common Piper model language codes**:
```
en-us    → English (US)
en-gb    → English (UK)
zh       → Chinese (Mandarin)
de       → German
es-419   → Spanish (Latin America)
es       → Spanish (Spain)
fr-fr    → French (France)
it-it    → Italian
```

---

## Phase 2: Chinese Phonemizer (Week 3)

### 2.1 Data Sources

**CC-CEDICT** (Creative Commons BY-SA 4.0)
- Traditional/Simplified Chinese → Pinyin
- Format: `你好 ni3 hao3 [hello]`
- Size: ~120,000 entries
- Download: https://www.mdbg.net/chinese/dictionary?page=cc-cedict
- License: CC BY-SA 4.0 (commercial-friendly with attribution)

**Alternative: Unihan Database** (Unicode Consortium)
- License: Unicode License (permissive)
- Contains: Mandarin readings, Cantonese readings
- Size: 90,000+ characters
- More complete coverage for rare characters

### 2.2 Pinyin → IPA Conversion

**Complete Pinyin Initial Consonants**:
```
b  → /p/     p  → /pʰ/    m  → /m/     f  → /f/
d  → /t/     t  → /tʰ/    n  → /n/     l  → /l/
g  → /k/     k  → /kʰ/    h  → /x/
j  → /tɕ/    q  → /tɕʰ/   x  → /ɕ/
zh → /ʈʂ/    ch → /ʈʂʰ/   sh → /ʂ/    r  → /ɻ/
z  → /ts/    c  → /tsʰ/   s  → /s/
```

**Pinyin Final Vowels**:
```
a   → /a/     o   → /o/     e   → /ɤ/
ai  → /aɪ/    ei  → /eɪ/    ao  → /ɑʊ/    ou  → /oʊ/
an  → /an/    en  → /ən/    ang → /ɑŋ/    eng → /əŋ/
i   → /i/     u   → /u/     ü   → /y/
ia  → /ia/    ie  → /iɛ/    iao → /iɑʊ/   iou → /ioʊ/
ian → /iɛn/   in  → /in/    iang → /iɑŋ/  ing → /iŋ/
ua  → /ua/    uo  → /uo/    uai → /uaɪ/   ui  → /ueɪ/
uan → /uan/   un  → /uən/   uang → /uɑŋ/  ong → /ʊŋ/
üe  → /yɛ/    üan → /yɛn/   ün  → /yn/
er  → /ɚ/
```

**Tone Representation** (IPA tone letters):
```
Tone 1 (High level):     ma¹ → /ma˥/    ˥ = U+02E5
Tone 2 (Rising):         ma² → /ma˧˥/   ˧˥ = U+02E7 + U+02E5
Tone 3 (Dipping):        ma³ → /ma˨˩˦/  ˨˩˦ = U+02E8 + U+02E9 + U+02E6
Tone 4 (Falling):        ma⁴ → /ma˥˩/   ˥˩ = U+02E5 + U+02E9
Tone 5 (Neutral):        ma  → /ma/     (no marker)
```

**Alternative: Use numeric suffixes** (simpler for Piper):
```
ma1 → /ma1/
ma2 → /ma2/
ma3 → /ma3/
ma4 → /ma4/
ma  → /ma0/
```

### 2.3 Chinese-Specific Challenges & Solutions

**Challenge 1: Character Segmentation**

Chinese text has no word boundaries. Must segment before lookup.

**Solution: Maximum Matching Algorithm**
```c
// Forward maximum matching (FMM)
size_t segment_chinese(const char* text, char** words, size_t max_words) {
    size_t word_count = 0;
    const char* p = text;
    
    while (*p && word_count < max_words) {
        // Try longest match first (4 chars → 1 char)
        int matched = 0;
        for (int len = 4; len >= 1; len--) {
            char candidate[16];
            if (extract_hanzi(p, len, candidate)) {
                if (dict_lookup(candidate)) {
                    words[word_count++] = strdup(candidate);
                    p += utf8_char_len(p) * len;
                    matched = 1;
                    break;
                }
            }
        }
        if (!matched) {
            // Single character fallback
            p += utf8_char_len(p);
        }
    }
    return word_count;
}
```

**Challenge 2: Polyphone Disambiguation (多音字)**

Same character, different pronunciations: 行 (xíng "walk" vs háng "row")

**Solution: Context-based disambiguation**
```c
typedef struct {
    const char* word;       // Multi-char word
    const char* reading;    // Specific reading
} context_rule_t;

// Common bi-gram rules
static context_rule_t bichar_rules[] = {
    {"银行", "yín háng"},    // bank (háng, not xíng)
    {"行走", "xíng zǒu"},    // walk (xíng, not háng)
    {"长城", "cháng chéng"}, // Great Wall (cháng, not zhǎng)
    {"成长", "chéng zhǎng"}, // grow (zhǎng, not cháng)
    // ... ~1000 common cases
};

const char* disambiguate_reading(const char* prev_char, 
                                 const char* curr_char, 
                                 const char* next_char) {
    // Check bi-char rules first
    char bigram[16];
    snprintf(bigram, sizeof(bigram), "%s%s", prev_char, curr_char);
    for (size_t i = 0; i < ARRAY_SIZE(bichar_rules); i++) {
        if (strcmp(bigram, bichar_rules[i].word) == 0) {
            return bichar_rules[i].reading;
        }
    }
    
    // Fallback to most common reading
    return dict_lookup_primary(curr_char);
}
```

**Challenge 3: Tone Sandhi (变调)**

Tones change in certain contexts:
- Two 3rd tones → 1st + 3rd: 你好 nǐhǎo → níhǎo
- 一 (yī) changes: 一个 yí ge, 一样 yí yàng
- 不 (bù) changes: 不去 bú qù

**Solution: Post-processing tone rules**
```c
void apply_tone_sandhi(pinyin_syllable_t* syllables, size_t count) {
    for (size_t i = 0; i < count - 1; i++) {
        // Rule 1: 3rd + 3rd → 2nd + 3rd
        if (syllables[i].tone == 3 && syllables[i+1].tone == 3) {
            syllables[i].tone = 2;
        }
        
        // Rule 2: 一 (yī) tone changes
        if (strcmp(syllables[i].text, "yi") == 0) {
            if (syllables[i+1].tone == 4) {
                syllables[i].tone = 2;  // yí
            } else if (syllables[i+1].tone != 1) {
                syllables[i].tone = 4;  // yì
            }
        }
        
        // Rule 3: 不 (bù) before 4th tone → 2nd tone
        if (strcmp(syllables[i].text, "bu") == 0 && syllables[i+1].tone == 4) {
            syllables[i].tone = 2;  // bú
        }
    }
}
```

**Challenge 4: Erhua (儿化音)**

Retroflex suffix that modifies preceding vowel: 玩儿 wánr

**Solution: Special suffix handling**
```c
bool is_erhua_suffix(const char* hanzi) {
    return strcmp(hanzi, "儿") == 0;
}

void apply_erhua(char* ipa_string, const char* base_ipa) {
    // Remove final consonant (if any) and add /ɚ/
    // Examples:
    // /wan/ + 儿 → /waɚ/
    // /huɑ/ + 儿 → /huɑɚ/
    
    char* p = ipa_string + strlen(ipa_string) - 1;
    // Remove final nasal consonant
    if (*p == 'n' || *p == 'ŋ') {
        *p = '\0';
    }
    strcat(ipa_string, "ɚ");
}
```

**Challenge 5: Regional Variations**

Standard Mandarin (Putonghua) vs Taiwan Mandarin vs regional accents

**Solution: Support primary standard first**
- Default: Mainland Mandarin (Putonghua)
- Optional: Taiwan reading variants (e.g., 垃圾 lèsè vs lājī)
- Future: Dialect support (Cantonese, Hokkien)

### 2.4 Data Structure Design

```c
typedef struct {
    uint32_t hanzi_utf8;     // Unicode codepoint
    char pinyin[8];          // Primary reading with tone number
    char alt_pinyin[3][8];   // Alternative readings
    char ipa[16];            // IPA with tone markers
    uint8_t frequency;       // 0-255 (for disambiguation)
} chinese_dict_entry_t;

typedef struct {
    chinese_dict_entry_t* entries;
    size_t count;
    // Hash table for O(1) lookup
    chinese_dict_entry_t** hash_table;
    size_t hash_size;
} chinese_dict_t;
```

### 2.5 Implementation Steps

**Step 1: Download & Parse CC-CEDICT**
```bash
wget https://www.mdbg.net/chinese/export/cedict/cedict_1_0_ts_utf-8_mdbg.txt.gz
gunzip cedict_1_0_ts_utf-8_mdbg.txt.gz

# Parse format: Traditional Simplified [pin1 yin1] /definition/
# Example: 你好 你好 [ni3 hao3] /hello/hi/
```

**Step 2: Build Pinyin→IPA Mapping Table**
```python
# scripts/build_chinese_dict.py
def pinyin_to_ipa(pinyin_with_tone):
    initial, final, tone = parse_pinyin(pinyin_with_tone)
    ipa = INITIAL_MAP[initial] + FINAL_MAP[final] + TONE_MAP[tone]
    return ipa

# Generate C header file
with open('chinese_dict_data.h', 'w') as f:
    f.write('static const chinese_dict_entry_t chinese_dict[] = {\n')
    for entry in cc_cedict:
        f.write(f'  {{0x{entry.hanzi:04X}, "{entry.pinyin}", ..., "{entry.ipa}"}},\n')
    f.write('};\n')
```

**Step 3: Implement Lookup Function**
```c
const char* phonemizer_chinese_lookup(const char* hanzi_utf8) {
    uint32_t codepoint = utf8_to_codepoint(hanzi_utf8);
    
    // Hash lookup
    size_t idx = hash_codepoint(codepoint) % dict->hash_size;
    chinese_dict_entry_t* entry = dict->hash_table[idx];
    
    while (entry) {
        if (entry->hanzi_utf8 == codepoint) {
            return entry->ipa;
        }
        entry = entry->next;  // Chain collision
    }
    
    return NULL;  // OOV character
}
```

**Step 4: Full Text Conversion Pipeline**
```c
int phonemizer_chinese_text_to_ipa(const char* text, 
                                    char* ipa_output, 
                                    size_t max_len) {
    // 1. Segment into words
    char* words[256];
    size_t word_count = segment_chinese(text, words, 256);
    
    // 2. Convert each character to pinyin
    pinyin_syllable_t syllables[1024];
    size_t syllable_count = 0;
    
    for (size_t i = 0; i < word_count; i++) {
        const char* p = words[i];
        while (*p) {
            const char* ipa = phonemizer_chinese_lookup(p);
            if (ipa) {
                parse_pinyin_to_syllable(ipa, &syllables[syllable_count++]);
            }
            p += utf8_char_len(p);
        }
    }
    
    // 3. Apply tone sandhi
    apply_tone_sandhi(syllables, syllable_count);
    
    // 4. Convert to IPA string
    assemble_ipa_string(syllables, syllable_count, ipa_output, max_len);
    
    return 0;
}
```

### 2.6 Testing Data

**Test sentences** (varying complexity):
```
1. 你好世界 (nǐ hǎo shì jiè) - Hello world
2. 一个人走路很快 (yí ge rén zǒu lù hěn kuài) - One person walks fast (tone sandhi)
3. 银行在前面 (yín háng zài qián miàn) - The bank is ahead (polyphone)
4. 我们去玩儿吧 (wǒ men qù wánr ba) - Let's go play (erhua)
5. 长城很长 (cháng chéng hěn cháng) - The Great Wall is very long (same char, context)
```

### 2.7 Performance Targets

- **Lookup speed**: < 2μs per character
- **Segmentation**: < 5ms for 100 characters
- **Total latency**: < 20ms for typical sentence
- **Memory**: ~5-8MB for full dictionary
- **Accuracy**: 92%+ for common texts

---

## Phase 3: German Phonemizer (Week 4)

### 3.1 Data Sources

**Primary: Wiktionary German Pronunciations**
- Source: Wikimedia dumps (CC BY-SA 3.0)
- Format: XML with IPA transcriptions
- Size: ~300,000 German entries
- Download: https://dumps.wikimedia.org/dewiktionary/
- License: CC BY-SA 3.0 (commercial-friendly with attribution)

**Secondary: phonetisaurus/g2p-seq2seq trained models**
- Pre-trained German G2P models (Apache 2.0)
- Can extract rules and patterns
- Backup for missing words

**Tertiary: Rule-based system**
- German orthography is highly regular (80%+ predictable)
- Rules can cover most common words
- See section 3.2 for detailed rules

### 3.2 German Orthography → IPA Rules

German has more consistent spelling than English, making rule-based G2P effective.

#### Vowel Rules

**Short vowels** (before double consonants or consonant clusters):
```
a + CC → /a/     Tasse → /ˈtasə/
e + CC → /ɛ/     Bett → /bɛt/
i + CC → /ɪ/     Kinder → /ˈkɪndɐ/
o + CC → /ɔ/     Sonne → /ˈzɔnə/
u + CC → /ʊ/     Mutter → /ˈmʊtɐ/
```

**Long vowels** (before single consonant, doubled, or followed by 'h'):
```
a, aa, ah → /aː/    Vater → /ˈfaːtɐ/, Saal → /zaːl/, fahren → /ˈfaːʁən/
e, ee, eh → /eː/    geben → /ˈɡeːbn̩/, See → /zeː/, gehen → /ˈɡeːən/
i, ie, ih → /iː/    Tiger → /ˈtiːɡɐ/, Liebe → /ˈliːbə/, ihr → /iːɐ̯/
o, oo, oh → /oː/    Ofen → /ˈoːfn̩/, Boot → /boːt/, Sohn → /zoːn/
u, uh    → /uː/    Mut → /muːt/, Uhr → /uːɐ̯/
```

**Umlauts**:
```
ä, äh    → /ɛː/    Käse → /ˈkɛːzə/, wählen → /ˈvɛːlən/
ö, öh    → /øː/    schön → /ʃøːn/, Höhle → /ˈhøːlə/
ü, üh    → /yː/    Tür → /tyːɐ̯/, Bühne → /ˈbyːnə/

ä + CC   → /ɛ/     Hände → /ˈhɛndə/
ö + CC   → /œ/     können → /ˈkœnən/
ü + CC   → /ʏ/     Mütter → /ˈmʏtɐ/
```

**Diphthongs**:
```
ei, ai, ay → /aɪ̯/   mein → /maɪ̯n/, Mai → /maɪ̯/, Bayern → /ˈbaɪ̯ɐn/
au        → /aʊ̯/   Haus → /haʊ̯s/
eu, äu    → /ɔʏ̯/   neu → /nɔʏ̯/, Häuser → /ˈhɔʏ̯zɐ/
```

**Reduced vowels**:
```
e (unstressed final) → /ə/    Liebe → /ˈliːbə/
er (unstressed)      → /ɐ/    Vater → /ˈfaːtɐ/, aber → /ˈaːbɐ/
en (unstressed)      → /n̩/    gehen → /ˈɡeːn̩/, lesen → /ˈleːzn̩/
```

#### Consonant Rules

**Single consonants**:
```
b → /b/       aber → /ˈaːbɐ/
c → /k/ or /ts/  Cafe → /ˈkafe/, Celsius → /ˈtsɛlziʊs/
d → /d/       Dose → /ˈdoːzə/
f → /f/       Vater → /ˈfaːtɐ/
g → /ɡ/       gut → /ɡuːt/
h → /h/       Haus → /haʊ̯s/
j → /j/       ja → /jaː/
k → /k/       Kind → /kɪnt/
l → /l/       Liebe → /ˈliːbə/
m → /m/       Mutter → /ˈmʊtɐ/
n → /n/       nur → /nuːɐ̯/
p → /p/       Papier → /paˈpiːɐ̯/
q → /k/       Quelle → /ˈkvɛlə/
r → /ʁ/ or /ɐ̯/ Rat → /ʁaːt/, Vater → /ˈfaːtɐ/
s → /z/ or /s/  Sonne → /ˈzɔnə/, ist → /ɪst/
t → /t/       Tag → /taːk/
v → /f/ or /v/  Vater → /ˈfaːtɐ/, Vase → /ˈvaːzə/
w → /v/       Wasser → /ˈvasɐ/
x → /ks/      Hexe → /ˈhɛksə/
z → /ts/      Zeit → /tsaɪ̯t/
```

**Consonant clusters**:
```
ch → /ç/ after i,e,ä,ö,ü, consonants    ich → /ɪç/, Milch → /mɪlç/
ch → /x/ after a,o,u,au                 Bach → /bax/, Buch → /buːx/
sch → /ʃ/                               schön → /ʃøːn/
tsch → /tʃ/                             Deutsch → /dɔʏ̯tʃ/
ck → /k/                                Rücken → /ˈʁʏkn̩/
ng → /ŋ/                                jung → /jʊŋ/
nk → /ŋk/                               denken → /ˈdɛŋkn̩/
pf → /pf/                               Pferd → /pfeːɐ̯t/
qu → /kv/                               Quelle → /ˈkvɛlə/
sp → /ʃp/ (word-initial)                Sport → /ʃpɔʁt/
st → /ʃt/ (word-initial)                Stern → /ʃtɛʁn/
ß  → /s/                                groß → /ɡʁoːs/
```

**Final devoicing** (Auslautverhärtung):
```
b → /p/ word-finally    Dieb → /diːp/
d → /t/ word-finally    Rad → /ʁaːt/
g → /k/ word-finally    Tag → /taːk/
v → /f/ word-finally    brav → /bʁaːf/
```

#### Stress Rules

**Primary stress** (marked with ˈ before stressed syllable):

1. **Native German words**: Stress first syllable
   ```
   ˈMutter → /ˈmʊtɐ/
   ˈKinder → /ˈkɪndɐ/
   ```

2. **Compound words**: Stress first element
   ```
   ˈHausˌtür → /ˈhaʊ̯sˌtyːɐ̯/
   ```

3. **Prefixes**:
   - Separable prefixes: stressed
     ```
     ˈaufstehen → /ˈaʊ̯fˌʃteːən/
     ```
   - Inseparable prefixes (be-, ge-, er-, ver-, ent-, emp-, zer-): unstressed
     ```
     beˈsuchen → /bəˈzuːxn̩/
     verˈstehen → /fɛɐ̯ˈʃteːən/
     ```

4. **Loanwords**: Often stress final syllable
   ```
   Hoˈtel → /hoˈtɛl/
   Muˈsik → /muˈziːk/
   ```

### 3.3 Implementation Architecture

```c
typedef struct {
    const char* grapheme;      // Spelling
    const char* ipa;           // IPA transcription
    uint8_t stress_position;   // 0 = first syllable, 1 = second, etc.
    bool is_loanword;
} german_dict_entry_t;

typedef struct {
    char letter[4];            // UTF-8 character(s)
    char ipa[8];               // IPA equivalent
    char context_before[8];    // Required preceding context (regex)
    char context_after[8];     // Required following context
} german_rule_t;
```

### 3.4 Rule-Based G2P Implementation

```c
// Step 1: Identify syllable boundaries
void syllabify_german(const char* word, syllable_t* syllables, size_t* count) {
    // V.CV (single consonant goes with following vowel)
    // VC.CV (double consonant splits)
    // VCC.CV (consonant cluster splits optimally)
}

// Step 2: Apply phoneme rules per syllable
const char* apply_german_rules(const char* grapheme, 
                                const char* prev_context,
                                const char* next_context,
                                bool is_stressed) {
    // Check all rules in order
    for (size_t i = 0; i < RULE_COUNT; i++) {
        if (match_rule(&german_rules[i], grapheme, prev_context, next_context)) {
            return german_rules[i].ipa;
        }
    }
    return NULL;
}

// Step 3: Apply stress
void apply_german_stress(ipa_syllable_t* syllables, size_t count, 
                         const char* original_word) {
    if (is_compound_word(original_word)) {
        // Stress first element
        syllables[0].has_primary_stress = true;
    } else if (has_inseparable_prefix(original_word)) {
        // Stress root, not prefix
        size_t root_syllable = find_root_syllable(syllables, count, original_word);
        syllables[root_syllable].has_primary_stress = true;
    } else {
        // Default: stress first syllable
        syllables[0].has_primary_stress = true;
    }
}
```

### 3.5 Dictionary + Rules Hybrid

**Strategy**:
1. Check dictionary first (covers ~60% of common words)
2. Apply rules for OOV words (covers ~85% of remaining)
3. Fallback: letter-by-letter pronunciation

```c
const char* phonemizer_german_word(const char* word) {
    // 1. Normalize (lowercase, remove diacritics for lookup)
    char normalized[128];
    normalize_german(word, normalized);
    
    // 2. Dictionary lookup
    const char* dict_result = german_dict_lookup(normalized);
    if (dict_result) {
        return dict_result;
    }
    
    // 3. Check exception list (common irregular words)
    dict_result = german_exceptions_lookup(normalized);
    if (dict_result) {
        return dict_result;
    }
    
    // 4. Apply rules
    static char ipa_buffer[256];
    if (apply_german_g2p_rules(word, ipa_buffer, sizeof(ipa_buffer)) == 0) {
        return ipa_buffer;
    }
    
    // 5. Fallback
    return NULL;
}
```

### 3.6 Special Cases & Exceptions

**Irregular pronunciations** (high-frequency words):
```c
static const struct {
    const char* word;
    const char* ipa;
} german_exceptions[] = {
    {"sind", "zɪnt"},        // Not regular /zɪnd/
    {"haben", "ˈhaːbn̩"},    // Reduced ending
    {"viele", "ˈfiːlə"},     // v → /f/
    {"China", "ˈçiːna"},     // ch → /ç/ not /k/
    {"Chemie", "çeˈmiː"},    // ch → /ç/ not /k/
    // ... ~500 common exceptions
};
```

**Regional variations**:
- Standard German (Hochdeutsch): Primary target
- Austrian German: Some vowel differences
- Swiss German: Very different (defer to future)

### 3.7 Testing & Validation

**Test corpus**:
```
1. Ich liebe Deutschland.
   /ɪç ˈliːbə ˈdɔʏ̯tʃlant/

2. Der Zug fährt um acht Uhr.
   /deːɐ̯ tsuːk fɛːɐ̯t ʊm axt uːɐ̯/

3. Können Sie mir helfen?
   /ˈkœnən ziː miːɐ̯ ˈhɛlfn̩/

4. Das Wetter ist schön heute.
   /das ˈvɛtɐ ɪst ʃøːn ˈhɔʏ̯tə/

5. Entschuldigung, wo ist der Bahnhof?
   /ɛntˈʃʊldɪɡʊŋ voː ɪst deːɐ̯ ˈbaːnhoːf/
```

**Challenging words** (test rule coverage):
```
- Compound words: Donaudampfschifffahrtsgesellschaft
- Consonant clusters: Herbst, Arzt, kämpfst
- Umlauts: Brötchen, Mädchen, Übung
- Final devoicing: Hund, Wald, Lieb
- ch alternation: ich vs. auch, Buch vs. Bücher
```

### 3.8 Performance Targets

- **Dictionary coverage**: 60% of running text
- **Rule accuracy**: 85% for OOV words
- **Combined accuracy**: 90%+ overall
- **Lookup speed**: < 1μs per word (dict), < 100μs (rules)
- **Memory footprint**: ~2-3MB (smaller dict than English)

---

## Testing Strategy

### Unit Tests

```c
// Test dictionary lookup
assert_string_equal(
    phoneme_dict_lookup(dict, "hello"),
    "həˈloʊ"
);

// Test rule application
assert_string_equal(
    apply_rules_en("making"),
    "meɪkɪŋ"
);

// Test tokenization
char tokens[10][8];
int count = tokenize_ipa("həˈloʊ", tokens, 10);
assert_int_equal(count, 6);
assert_string_equal(tokens[2], "ˈ");
```

### Integration Tests

**Compare against espeak-ng output** (for validation only):
```bash
# Generate test corpus
echo "The quick brown fox" | espeak-ng --ipa > expected.txt
./ethervoxai --phonemize "The quick brown fox" > actual.txt
diff expected.txt actual.txt
```

**Piper model inference tests**:
- Synthesize test sentences
- Verify audio quality matches espeak-based synthesis
- Check phoneme ID sequences are valid

### Performance Benchmarks

**Target metrics**:
- Dictionary lookup: < 1μs per word
- Full text phonemization: < 10ms for 100 words
- Memory usage: < 5MB (embedded dict + runtime)

---

## Deployment Strategy

### Embedded vs. External Dictionary

**Option A: Embedded C Array** (recommended)
```c
static const dict_entry_t embedded_dict[] = {
    {"hello", "həˈloʊ", 1.0},
    {"world", "wɝld", 1.0},
    // ... top 10K words
};
```

**Pros**: No file I/O, fast startup, portable  
**Cons**: Larger binary (~500KB-1MB)

**Option B: External File**
- Load from `~/.ethervox/data/cmudict.txt`
- Pros: Smaller binary, updatable
- Cons: File I/O overhead, deployment complexity

### Build Integration

```cmake
# CMakeLists.txt
option(PHONEMIZER_EMBED_DICT "Embed dictionary in binary" ON)

if(PHONEMIZER_EMBED_DICT)
    # Generate C array from dictionary file
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/phoneme_dict_data.c
        COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/dict_to_c.py
                ${CMAKE_SOURCE_DIR}/src/tts/phonemizer/data/cmudict.txt
                ${CMAKE_BINARY_DIR}/phoneme_dict_data.c
        DEPENDS ${CMAKE_SOURCE_DIR}/src/tts/phonemizer/data/cmudict.txt
    )
    target_sources(ethervoxai PRIVATE ${CMAKE_BINARY_DIR}/phoneme_dict_data.c)
endif()
```

---

## Validation & Quality Assurance

### Test Corpus

**Standard datasets**:
1. **LibriSpeech dev-clean** (1000 sentences)
2. **Common Crawl sample** (diverse vocabulary)
3. **Edge cases**:
   - Technical terms
   - Proper nouns
   - Contractions
   - Numbers and dates

### Quality Metrics

**Phone Error Rate (PER)**:
```
PER = (Substitutions + Insertions + Deletions) / Total_Phonemes
```

**Target**: < 5% PER vs. espeak-ng reference

**Word Error Rate (WER)** (for TTS):
- Synthesize audio with Piper
- Run through STT (Whisper)
- Compare transcription to input text
- Target: < 3% WER

---

## Maintenance & Updates

### Dictionary Updates

**Process**:
1. Collect OOV words from production logs
2. Add to exception list
3. Regenerate embedded dictionary
4. Release minor version update

### Rule Improvements

**Iterative refinement**:
- Track phonemization errors
- Update rules based on patterns
- A/B test quality improvements
- Document rule changes

---

## Success Criteria

### Phase 1 (English) - Must Have
- ✅ 95%+ dictionary coverage of common words
- ✅ 90%+ overall accuracy on test corpus
- ✅ Audio quality parity with espeak-based Piper
- ✅ < 10ms latency for typical sentences
- ✅ Zero GPL dependencies

### Phase 2 (Chinese) - Should Have
- ✅ 90%+ character coverage
- ✅ Correct tone representation
- ✅ Intelligible synthesis output

### Phase 3 (German) - Nice to Have
- ✅ 85%+ word-level accuracy
- ✅ Acceptable audio quality

---

## Risk Mitigation

### High-Risk Items

1. **Accuracy gap vs. espeak-ng**
   - Mitigation: Extensive testing, iterative rule refinement
   - Fallback: Hybrid approach (rules + ML model)

2. **OOV word handling**
   - Mitigation: Comprehensive rule set, exception list
   - Fallback: Letter-by-letter fallback

3. **Binary size bloat**
   - Mitigation: Top-N dictionary embedding, compression
   - Fallback: External dictionary file

4. **Maintenance burden**
   - Mitigation: Automated testing, CI validation
   - Fallback: Community contributions

### Low-Risk Items

- Performance: Hash tables are O(1), proven fast
- Memory: 5MB is acceptable for desktop/mobile
- License: Public domain data eliminates risk

---

## Next Steps

1. **Immediate** (This week):
   - Download CMU Dictionary
   - Create phonemizer file structure
   - Implement hash table lookup

2. **Short-term** (Next 2 weeks):
   - ARPAbet → IPA conversion
   - Basic English rules
   - Integration with Piper backend

3. **Medium-term** (Weeks 3-4):
   - Comprehensive testing
   - Chinese phonemizer
   - Documentation

4. **Long-term** (Month 2+):
   - German support
   - Performance optimization
   - Production hardening

---

## References

- CMU Pronouncing Dictionary: http://www.speech.cs.cmu.edu/cgi-bin/cmudict
- IPA Chart: https://www.internationalphoneticalphabet.org/ipa-charts/
- ARPAbet: https://en.wikipedia.org/wiki/ARPABET
- CC-CEDICT: https://cc-cedict.org/
- Piper TTS: https://github.com/rhasspy/piper
- English G2P Rules: Jurafsky & Martin, Speech and Language Processing (3rd ed.)

---

**Document Version**: 1.0  
**Last Updated**: December 16, 2025  
**Author**: EthervoxAI Team
