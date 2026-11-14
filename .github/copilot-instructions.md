# EthervoxAI Core (Native C/C++) — Guidelines

## Code Style

- **C/C++**: All functions use `ethervox_result_t` return type for error handling
- **Error macros**: `ETHERVOX_CHECK_PTR()`, `ETHERVOX_RETURN_ERROR()`, `ETHERVOX_CHECK()`
- **Always include**: `#include "ethervox/error.h"` in files using error handling
- **Naming**: `ethervox_` prefix for public APIs, snake_case for functions/variables
- Commercial license (CC BY-NC-SA 4.0) — avoid GPL/AGPL dependencies

## Architecture

**EthervoxAI Core**
- Dialogue engine with intent detection and response generation
- Pattern-based natural language processing
- Context-aware conversation management

**JNI Bridge**
- Kotlin ↔ C++ interop layer for Android
- Type conversions between JVM and native types

**Audio Backend**
- AAudio (API 26+) with OpenSL ES fallback
- Hardware abstraction layer for Android devices
- Audio systems use fallback chain; preserve fallback logic when modifying

See parent project [README.md](../../../../../README.md) for full architecture

## Build

This module builds as part of the parent Android project via CMake:

```bash
# From project root
./gradlew assembleDebug

# Clean native build
./gradlew clean
rm -rf ethervox_core/build/
```

## Conventions

**Coding & Editing Rules**
- Make the smallest change that satisfies the task
- Preserve public APIs unless explicitly asked to change them
- Prefer correctness and maintainability over speed — no "quick fixes" if correct solutions exist with moderate effort
- Add unit tests for new behavior when feasible
- For platform-specific code, prefer small wrappers rather than scattering platform checks

**Error Handling (Migration ~7/150 files complete)**
- All functions must return `ethervox_result_t`
- Use error macros (`ETHERVOX_CHECK_PTR()`, `ETHERVOX_RETURN_ERROR()`, `ETHERVOX_CHECK()`)
- Never use bare `int` returns for new code
- See parent `.github/error-handling-reference.md` for full error code reference and migration patterns
- Track migration progress in `MIGRATION_CHECKLIST.md`

**Memory Management & JNI**
- Clean up JNI local references explicitly
- Use RAII patterns where applicable
- Check null pointers before dereferencing
- Cache field/method IDs for performance
- Handle JNI exceptions immediately after calls
- Release UTF strings after use

**Hardware & Performance**
- Hardware detection logic uses platform capabilities
- Avoid hard-coding performance tiers
- Model management handles downloads, caching, and compatibility
- Keep download logic idempotent and resumable

**License Compliance**
- EthervoxAI is commercial (CC BY-NC-SA 4.0)
- MIT, Apache 2.0, BSD licenses are typically fine to use, but need to be reviewed to ensure compatability.
- Avoid GPL/AGPL licenses (require derivative works to be open-source)
- Document third-party libraries in `docs/thirde_party_licenses.md`
- Flag licensing concerns before adding dependencies
- Never commit API keys, tokens, or secrets