# Third-Party Licenses and Attributions

This document lists all third-party libraries, dictionaries, and data sources used in EthervoxAI, along with their respective licenses.

---

## Core Dependencies

### ONNX Runtime
- **Version**: 1.22.2+
- **License**: MIT License
- **Purpose**: Neural network inference for Piper TTS models
- **URL**: https://github.com/microsoft/onnxruntime
- **Copyright**: Copyright (c) Microsoft Corporation

```
MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
```

### Speex DSP
- **Version**: 1.2.1+
- **License**: BSD 3-Clause License
- **Purpose**: Audio resampling (22050Hz → 16000Hz for TTS)
- **URL**: https://github.com/xiph/speexdsp
- **Copyright**: Copyright 2002-2008 Xiph.org Foundation, Jean-Marc Valin

```
BSD 3-Clause License

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"...
```

### llama.cpp
- **License**: MIT License
- **Purpose**: LLM inference engine
- **URL**: https://github.com/ggerganov/llama.cpp
- **Copyright**: Copyright (c) 2023 Georgi Gerganov

### whisper.cpp
- **License**: MIT License
- **Purpose**: Speech-to-text (STT) engine
- **URL**: https://github.com/ggerganov/whisper.cpp
- **Copyright**: Copyright (c) 2022 Georgi Gerganov

### cJSON
- **License**: MIT License
- **Purpose**: JSON parsing for configuration files
- **URL**: https://github.com/DaveGamble/cJSON
- **Copyright**: Copyright (c) 2009-2017 Dave Gamble and cJSON contributors

---

## Weather Data Source

### Open-Meteo Weather API
- **License (API data)**: Creative Commons Attribution 4.0 International (CC BY 4.0) — commercial
  use is permitted, attribution is required. Verified directly against
  https://open-meteo.com/en/licence (previous "MIT License" entry here was incorrect).
- **License (Open-Meteo's own server source, not used by us)**: GNU AGPLv3 — this only binds
  someone who runs/modifies Open-Meteo's own server software; it does not apply to us, since we
  are only an HTTP client of their public API, not a redistributor of their source code.
- **Purpose**: Weather forecast data for get_weather tool
- **URL**: https://open-meteo.com
- **API Documentation**: https://open-meteo.com/en/docs
- **Attribution text required by the license** (must be visibly reachable in-app, not just in
  this file — see "Attribution in EthervoxAI" below):
  ```
  Weather data by Open-Meteo.com
  ```

**Attribution in EthervoxAI**: shown in the Settings → About section of both the Android and iOS
apps as a link to https://open-meteo.com, satisfying the CC BY 4.0 attribution requirement.

**Data Sources**: Open-Meteo aggregates data from national weather services including:
- DWD (Deutscher Wetterdienst - Germany)
- NOAA (National Oceanic and Atmospheric Administration - USA)
- Météo-France (France)
- Met Office (United Kingdom)
- And other meteorological services worldwide

**Usage Notes**:
- Free unlimited API access (no API key required)
- Data updates every hour
- Global coverage (195+ countries)
- WMO weather codes used for condition classification

---

## Phonemizer Data Sources

### CMU Pronouncing Dictionary (English)
- **Version**: cmudict-0.7b
- **License**: Public Domain
- **Purpose**: English text-to-phoneme conversion (135,000+ entries)
- **URL**: https://github.com/cmusphinx/cmudict
- **Download Script**: `scripts/download_phonemizer_data.sh`
- **File Size**: ~3.5 MB
- **Attribution**: 
  - Created by Carnegie Mellon University
  - Maintained by the CMU Sphinx project

```
This dictionary is in the public domain and may be freely used, modified,
and distributed. No warranty or support is provided.
```

**Usage in EthervoxAI**: Dictionary-based phonemization for English words. Maps words to ARPAbet phonetic notation, which is then converted to IPA for Piper TTS models.

### Unicode Unihan Database (Chinese)
- **Version**: Unicode 17.0.0 (2025)
- **License**: Unicode License v3 (permissive, similar to MIT/BSD)
- **Purpose**: Chinese character-to-Pinyin conversion (44,000+ characters)
- **URL**: https://www.unicode.org/charts/unihan.html
- **Download Script**: `scripts/download_phonemizer_data.sh`
- **File Size**: ~8.5 MB (Unihan_Readings.txt)
- **Attribution**:
  - Copyright © 1991-2025 Unicode, Inc.
  - Unicode and the Unicode Logo are registered trademarks

```
UNICODE LICENSE V3

Permission is hereby granted, free of charge, to any person obtaining a
copy of data files and any associated documentation (the "Data Files") or
software and any associated documentation (the "Software") to deal in the
Data Files or Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, and/or sell
copies of the Data Files or Software, and to permit persons to whom the
Data Files or Software are furnished to do so, provided that either (a)
this copyright and permission notice appear with all copies of the Data
Files or Software, or (b) this copyright and permission notice appear in
associated Documentation.

https://www.unicode.org/license.txt
```

**Usage in EthervoxAI**: Provides character-level Mandarin Pinyin pronunciations with Unicode tone marks. The kMandarin property from Unihan_Readings.txt is used for phonemization.

### German Phonemizer (Rule-Based Implementation)
- **Version**: 1.0.0
- **License**: CC BY-NC-SA 4.0 (original implementation, part of EthervoxAI)
- **Purpose**: German text-to-phoneme conversion using orthographic rules
- **Implementation**: `src/tts/phonemizer/rules_de.c`
- **External Dependencies**: None (pure algorithmic approach)
- **Attribution**: Part of EthervoxAI core, Copyright (c) 2024 Mike Kostersitz

```
Rule-based German G2P system leveraging German's highly regular orthography.
Implements standard German phonological rules:
- Vowel length (doubled vowels, vowel+h)
- ich-Laut vs ach-Laut (/ç/ vs /x/)
- Word-final devoicing (b→p, d→t, g→k)
- Consonant clusters (sch, sp-, st-)
- Umlauts (ä, ö, ü) and diphthongs
```

**Usage in EthervoxAI**: No dictionary dependency required. German spelling is predictable enough for rule-based conversion with high accuracy (tested at 100% on common words).

---

## Neural Network Models (Not Included in Repository)

### Piper TTS Models
- **License**: MIT License (model weights)
- **Purpose**: Neural text-to-speech synthesis
- **URL**: https://github.com/rhasspy/piper
- **Download**: Users must download separately from Hugging Face
- **Supported Languages**: English, Chinese, German, Spanish, and 50+ others
- **Attribution**: 
  - Created by Michael Hansen / Rhasspy
  - Trained on various open-source datasets (LJSpeech, VCTK, etc.)

**Note**: EthervoxAI uses a custom Piper backend that does **not** include the GPL-licensed espeak-ng phonemizer from the official Piper project. Our implementation uses custom dictionaries and rules.

### Whisper Models
- **License**: MIT License
- **Purpose**: Automatic speech recognition
- **URL**: https://github.com/openai/whisper
- **Attribution**: Copyright (c) 2022 OpenAI
- **Status**: ⚠️ No longer part of the active architecture — replaced end-to-end by IBM Granite
  Speech (see below) during the Granite Speech voice-integration rewrite. Still referenced by
  legacy `ModelType` cases in both apps' model config files; MIT license means no legal conflict,
  but this is stale/dead code that should be removed as a follow-up cleanup, not a licensing issue.

### IBM Granite / Granite Speech Models
- **License**: Apache License 2.0 — verified directly against Hugging Face model metadata
  (`license:apache-2.0` tag on `ibm-granite/granite-speech-3.3-2b` and the Granite 4.x text
  models); permissive, no restriction on commercial use.
- **Purpose**: Governor (chat/tool-calling) LLM and Mode 1/4 voice conversation's
  speech-recognition model (replacing Whisper above)
- **URL**: https://huggingface.co/ibm-granite
- **Download**: Users/app download GGUF conversions separately from Hugging Face
- **Attribution**: Copyright (c) IBM Corporation

### IBM Granite Docling Models
- **License**: Apache License 2.0 — verified from Hugging Face model card
  (`license:apache-2.0` tag on `ibm-granite/granite-docling-258M`)
- **Purpose**: Document understanding and conversion — vision-language model for OCR, table
  extraction, formula recognition, and structured document parsing (C3.1 multimodal API testing)
- **URL**: https://huggingface.co/ibm-granite/granite-docling-258M
- **Architecture**: Idefics3-based VLM with siglip2 vision encoder + Granite 165M LLM
- **Status**: ⚠️ Architecture not yet supported by llama.cpp mtmd library (as of 2026-08-15).
  See `docs/GRANITE_DOCLING_INVESTIGATION.md` for details.
- **Download**: Model downloaded from Hugging Face, converted to GGUF format (f32) with mmproj
- **Attribution**: Copyright (c) IBM Research
- **Model Size**: ~656MB (text model) + ~374MB (vision encoder mmproj)
- **Training Data**: SynthCodeNet, SynthFormulaNet, SynthChartNet, DoclingMatix (all Apache 2.0 / MIT)

### LLaMA Models
- **License**: Various (depends on model)
- **Purpose**: Large language model inference
- **URL**: https://github.com/meta-llama
- **Note**: Users must comply with model-specific licenses

---

## Generated Data (GPL-Safe)

### Espeak-Trained Pronunciation Dictionaries
- **Files**: `src/tts/phonemizer/data/espeak_dict_*.h`, `src/tts/phonemizer/data/espeak_*.dict`
- **License**: MIT (generated factual data, not derivative work)
- **Purpose**: IPA pronunciation mappings for English and German
- **URL**: N/A (generated in-house)
- **Copyright**: Copyright (c) 2025 EthervoxAI

**Legal Status**:
- Generated using espeak-ng (GPL-3.0) as a **development/build tool only**
- Espeak-ng is **never linked or distributed** with EthervoxAI
- Dictionary data contains **factual IPA pronunciations** (not copyrightable code)
- Analogous to using GCC (GPL compiler) to build MIT-licensed software
- All lookup/integration code is original MIT-licensed implementation

**Key Points**:
1. **No Dynamic Linking**: Espeak-ng never loaded at runtime
2. **No Static Linking**: No espeak-ng code in binaries
3. **Data Only**: Only word→IPA mappings embedded
4. **Original Code**: All C implementation is original work
5. **Build Tool Pattern**: Same as GCC compiling non-GPL code

For detailed legal analysis, see `docs/ESPEAK_DICT_LICENSING.md`

---

## Removed Dependencies (GPL-free Migration)

### ~~espeak-ng~~ (REMOVED from Runtime)
- **Previous License**: GPL-3.0
- **Status**: **REMOVED from distributed software** (still used as dev tool only)
- **Reason**: Incompatible with commercial distribution
- **Current Use**: Development/training tool only - generates pronunciation data offline

**Migration Notes**:
- Phase 1 (Dec 2024): Replaced with CMU Dictionary (public domain)
- Phase 2 (Dec 2024): Initially used CC-CEDICT, then migrated to Unicode Unihan (license compatibility)
- Phase 3 (Dec 2024): Implemented rule-based German phonemizer (no external dependencies)
- Phase 4 (Dec 2024): Added espeak-trained dictionaries (GPL-safe, data only)
- See `docs/PHONEMIZER_IMPLEMENTATION_PLAN.md` and `docs/ESPEAK_DICT_LICENSING.md` for details

---

## Build Tools and Development Dependencies

### CMake
- **License**: BSD 3-Clause License
- **Purpose**: Build system
- **URL**: https://cmake.org/

### pkg-config
- **License**: GPL-2.0+ (build tool only, not linked)
- **Purpose**: Library discovery during build
- **Note**: Used only at build time, not distributed

---

## License Compatibility Matrix

| Component | License | Commercial Use | Copyleft | EthervoxAI Compatible |
|-----------|---------|----------------|----------|----------------------|
| EthervoxAI (Core) | CC BY-NC-SA 4.0 | No* | Yes | N/A |
| ONNX Runtime | MIT | Yes | No | ✅ Yes |
| Speex DSP | BSD-3-Clause | Yes | No | ✅ Yes |
| llama.cpp | MIT | Yes | No | ✅ Yes |
| whisper.cpp | MIT | Yes | No | ✅ Yes |
| cJSON | MIT | Yes | No | ✅ Yes |
| CMU Dict | Public Domain | Yes | No | ✅ Yes |
| Unicode Unihan | Unicode License v3 | Yes | No | ✅ Yes |
| Espeak Dict (data) | MIT (generated) | Yes | No | ✅ Yes |
| ~~espeak-ng (runtime)~~ | ~~GPL-3.0~~ | Yes | Yes | ❌ **REMOVED** |

\* Commercial licensing available via licensing@ethervox.ai

**Note**: espeak-ng is used as a development/build tool only (like GCC), not distributed.

---

## Attribution Requirements

When using EthervoxAI, you must provide attribution for:

1. **EthervoxAI itself**:
   ```
   Powered by EthervoxAI
   Copyright (c) 2024 Mike Kostersitz
   Licensed under CC BY-NC-SA 4.0
   https://github.com/ethervox-ai/ethervoxai
   ```

2. **CMU Pronouncing Dictionary** (if using English phonemizer):
   ```
   English pronunciation data from CMU Pronouncing Dictionary
   Public Domain - Carnegie Mellon University
   ```

3. **Unicode Unihan Database** (if using Chinese phonemizer):
   ```
   Chinese pronunciation data from Unicode Unihan Database
   Copyright © 1991-2025 Unicode, Inc.
   Licensed under Unicode License v3
   https://www.unicode.org/charts/unihan.html
   ```

4. **FrequencyWords Vocabulary Lists** (German and Spanish dictionary generation):
   ```
   Word frequency lists from hermitdave/FrequencyWords
   Copyright © 2016 Hermit Dave
   MIT License
   https://github.com/hermitdave/FrequencyWords
   
   Used as input vocabulary for espeak-ng pronunciation generation.
   The generated IPA pronunciations (output data) are embedded in EthervoxAI.
   ```

5. **German Phonemizer**: No external attribution required (original implementation, part of EthervoxAI core)

6. **Other dependencies**: Follow individual license requirements (MIT/BSD typically require copyright notice in distributions)

---

## Data Privacy and Model Attribution

EthervoxAI processes all data locally and does not transmit user data to external servers. When using pretrained models:

- **Piper TTS models**: Trained on open datasets (LJSpeech, VCTK, CSS10, etc.)
- **Whisper models**: Trained by OpenAI on diverse speech data
- **LLaMA models**: Follow Meta's licensing and usage policies

Users are responsible for complying with the licenses of any models they download and use with EthervoxAI.

---

## Contact

For licensing questions or commercial use inquiries:
- **Email**: licensing@ethervox.ai
- **Repository**: https://github.com/ethervox-ai/ethervoxai
- **Author**: Mike Kostersitz

## Changelog

- **2024-12-16**: Completed Phase 3 - Implemented rule-based German phonemizer (no external dependencies)
- **2024-12-16**: Replaced CC-CEDICT with Unicode Unihan (license compatibility fix)
- **2024-12-16**: Added CMU Dictionary (English phonemizer, Phase 1)
- **2024-12-16**: Removed espeak-ng (GPL-3.0 incompatibility)
- **2024-12**: Initial third-party licenses documentationr, Phase 1)
- **2024-12-16**: Removed espeak-ng (GPL-3.0 incompatibility)
- **2024-12**: Initial third-party licenses documentation

---

*Last Updated: December 16, 2024*
