# Copilot Instructions for EtherVoxAI

## Quick orientation

- Start with `README.md` for platform goals and supported hardware, then `docs/mvp.md` for scope and guiding principles.
- Runtime C sources live in `src/` with mirrored headers in `include/ethervox/`; HAL glue is in `src/platform/` and platform audio drivers in `src/audio/`.
- The Vue 3 dashboard is in `dashboard/` (Vite + Tailwind + Pinia) and talks to the core through REST endpoints under `/api/*`.
- Developer tooling scripts (cross-toolchain setup, deployments) are under `scripts/` and `cmake/`.
- **NEW**: Error handling specification in `docs/error-handling-spec.md` defines standardized error codes and logging patterns.

## Core architecture (C runtime)

- `src/platform/platform_core.c` registers the correct HAL by checking the `TARGET_*` macros set from `CMakeLists.txt`; new platform work must add a `*_hal.c` file and extend the `PLATFORM_SOURCES` selection block.
- Audio flows through `ethervox_audio_runtime_t` in `src/audio/audio_core.c`, which defers hardware work to platform drivers (`platform_linux.c`, `platform_rpi.c`, etc.). Keep callbacks (`on_audio_data`, `on_stt_result`) wired in `ethervox_audio_runtime` when adding processing steps.
- Dialogue, STT, wake word, and plugin orchestration each sit in their own subdirectory under `src/` and are pulled in via the top-level `file(GLOB ...)` in `CMakeLists.txt`; dropping a new `.c` file into those folders is usually enough for CMake to pick it up.
- The SDK (`sdk/ethervox_sdk.c` + `sdk/ethervox_sdk.h`) mirrors runtime concepts for external integrators; reuse SDK helpers instead of duplicating plugin bookkeeping inside the core.
- **NEW**: Error handling uses `ethervox_result_t` return codes (defined in `include/ethervox/error.h`) with thread-local context tracking. Use macros `ETHERVOX_CHECK(expr)`, `ETHERVOX_CHECK_PTR(ptr)`, and `ETHERVOX_RETURN_ERROR(code, msg)` for consistency.

## Build & test workflows (C runtime)

- Native development: `make configure` + `make build` drives dependency install (`make install-deps`) before calling CMake; the build step falls back to `npm run build` when a `package.json` is present, so ensure front-end builds succeed before invoking it.
- Direct CMake: `cmake -B build -DTARGET_PLATFORM=LINUX` then `cmake --build build`. Pass `-DBUILD_TESTS=ON` to pull in `tests/` targets.
- Raspberry Pi and Windows cross-builds rely on toolchain files in `cmake/`; invoke `make configure-rpi` or `make configure-windows` after running the matching `scripts/setup-*-toolchain.sh`.
- ESP32 builds must enter `esp-idf` environment (`. $HOME/esp/esp-idf/export.sh`) and run `make build-esp32` or `idf.py build` inside `esp32-project/` where `src/` and `include/` are symlinked.
- Tests live under `tests/unit` and `tests/integration`; they are plain `assert`-style executables compiled via CMake and run with `ctest` or directly (`./tests/test_audio_core`). Expect audio and HAL tests to degrade gracefully when hardware is unavailable.
- **NEW**: Coverage can be enabled via `cmake -DENABLE_COVERAGE=ON` before running `ctest`; coverage targets are defined in `tests/CMakeLists.txt`.

## Platform & plugin conventions (C runtime)

- Platform detection happens through `TARGET_PLATFORM` cache variables; when adding new code paths, gate them with the matching `ETHERVOX_PLATFORM_*` macro defined in `include/ethervox/config.h`.
- `ethervox_platform_register_hal` expects each HAL to expose `*_hal_register` that populates GPIO/audio callbacks. Keep HAL structs lean and avoid heap allocation—existing implementations store state in the passed `ethervox_platform_t`.
- The plugin manager (`src/plugins/plugin_manager.c`) uses a fixed-size array (`ETHERVOX_MAX_PLUGINS`); prefer reusing slots and update `plugin->status` transitions so `ethervox_plugin_execute` can short-circuit correctly.
- Built-in LLM plugins wrap stateless helpers (e.g., `openai_execute_wrapper`); follow the wrapper pattern when adding new built-ins so tests that assume `plugin->execute` exist continue to work.
- **NEW**: All functions should return `ethervox_result_t` and use error handling macros. See `docs/error-handling-spec.md` for migration patterns.

## Error handling (follow docs/error-handling-spec.md)

### C Runtime conventions

- **Return codes**: All public functions return `ethervox_result_t` (0 = success, negative = error). Use `ethervox_is_success()` and `ethervox_is_error()` to check results.
- **Error propagation**: Use `ETHERVOX_CHECK(expr)` to automatically propagate errors up the call stack.
- **Null checking**: Use `ETHERVOX_CHECK_PTR(ptr)` at function entry for all pointer arguments.
- **Error context**: Use `ETHERVOX_RETURN_ERROR(code, msg)` or `ETHERVOX_ERROR_SET(code, msg)` to preserve file/line/function context.
- **Logging**: Use `ETHERVOX_LOG_ERROR()`, `ETHERVOX_LOG_WARN()`, etc. for structured logging. Combine with error returns using `ETHERVOX_LOG_RETURN_ERROR(code, msg)`.
- **Error codes**: Categorized by subsystem (audio: -200s, STT: -300s, plugins: -500s, etc.). See `include/ethervox/error.h` for full list.

Example:

```c
ethervox_result_t ethervox_audio_init(ethervox_audio_t* audio) {
    ETHERVOX_CHECK_PTR(audio);
    
    ethervox_result_t result = platform_audio_init();
    if (ethervox_is_error(result)) {
        ETHERVOX_LOG_ERROR("Platform audio init failed: %s", 
                          ethervox_error_string(result));
        return result;
    }
    
    return ETHERVOX_SUCCESS;
}
```

### TypeScript/Node.js conventions

- **Custom errors**: Use specific error classes (`AudioError`, `ModelError`, `PlatformError`, `NetworkError`, `PluginError`) from `src/common/errors.ts`.
- **Result type**: Prefer `Result<T, E>` for operations that may fail. Use `Ok(value)` and `Err(error)` helpers.
- **Context preservation**: Always include relevant context in error constructors.
- **Error serialization**: Use `.toJSON()` for logging and transport.

Example:

```typescript
async function downloadModel(url: string): Promise<Result<string>> {
  try {
    const response = await fetch(url);
    if (!response.ok) {
      return Err(new NetworkError('Download failed', {
        url,
        status: response.status
      }));
    }
    return Ok(await response.text());
  } catch (err) {
    return Err(new NetworkError('Request failed', { url, originalError: err }));
  }
}

// Usage
const result = await downloadModel('https://example.com/model');
if (!result.success) {
  console.error('Download failed:', result.error.toJSON());
  return;
}
```

## Dashboard guidelines

- Routing lives in `dashboard/src/router/` and global state in the Pinia store `dashboard/src/stores/system.js`; API calls go through axios and expect `/api/system/status` and `/api/system/metrics` JSON payloads. Mock these endpoints when adding new panels to keep the dashboard usable offline.
- Styling is composed with Tailwind classes from `dashboard/tailwind.config.js`; prefer utility classes and co-locate component-specific CSS in `<style scoped>` blocks when Tailwind lacks an expressive primitive.
- Run `npm install` once per root (`/` and `dashboard/`) and use `npm run dev` (Vite) during UI work; CI also runs `npm run lint` and Lighthouse per `dashboard/lighthouserc.js`, so keep performance thresholds in mind.

## Testing & quality gates

- Enable coverage locally via `cmake -DENABLE_COVERAGE=ON` before `ctest`; coverage targets are defined in `tests/CMakeLists.txt`.
- GitHub Actions workflows in `.github/workflows/` enforce clang-format, clang-tidy, Semgrep, npm audit (`dashboard/.auditci.json`), and multi-platform builds. Replicate failing checks locally by running the matching script from `docs/cicd-pipeline.md`.
- Many tests deliberately tolerate missing audio hardware; log informative messages rather than failing hard when `ethervox_audio_init` or HAL bindings return errors.
- **NEW**: Error handling tests verify proper use of `ethervox_result_t`, error context preservation, and graceful degradation. See `tests/unit/test_error.c` for patterns.

## Licensing & docs

- Every source file carries `SPDX-License-Identifier: CC-BY-NC-SA-4.0`; preserve headers when creating new files.
- Place new multi-platform or privacy-related references under `docs/` (see `docs/legal.md`, `docs/ethical-ai.md`, `docs/error-handling-spec.md`) and link them from `README.md` if they affect developer workflows.
- For SDK-facing additions, update both the code and the narrative in `sdk/README.md` so external integrators inherit the correct usage pattern.

## Key documentation files

- `docs/mvp.md` - MVP scope and guiding principles
- `docs/cicd-pipeline.md` - CI/CD requirements and local replication
- `docs/error-handling-spec.md` - Standardized error handling patterns (NEW)
- `docs/legal.md` - Legal and licensing information
- `docs/ethical-ai.md` - Ethical AI guidelines
- `sdk/README.md` - SDK usage and integration patterns

---

## Node.js / TypeScript implementation (supplementary)

If you're working on the Node.js/TypeScript side of EthervoxAI, follow these focused pointers in addition to the C guidance above:

## Where to look

- Primary narrative and examples live in `README.md` and `docs/` (see `docs/mvp.md`).
- Implementation folders: `implementations/` (python/cpp/micropython) and the TypeScript core referenced by the README (`src/modules/` examples).
- Dashboard (optional UI) lives in `dashboard/` (Vite + Vue 3 + Pinia).
- **NEW**: Error classes and Result type in `src/common/errors.ts`.

## Common commands

```bash
# Install deps
npm install

# Build core
npm run build

# Run the demo
npm run demo

# Audio test harness
npm run test:audio

# Tests & lint
npm run test
npm run lint

# Type checking (if available)
npm run typecheck
```

## Node.js-specific conventions

- Model management and downloads are centralized (see `modelManager` in README examples). Use `modelManager.getRecommendedModels()` and `modelManager.getModelPath()` rather than hard-coding model names/paths.
- Hardware detection: call `platformDetector.getCapabilities()` to determine performance tiers and recommended thread counts.
- Audio: prefer `CrossPlatformAudioManager` and its `testOutputMethods()` harness when modifying audio chains; update `config/audio.json` for fallback changes.
- Keep public TypeScript types stable. Add tests under `tests/` for new behavior.
- **NEW**: Use custom error classes from `src/common/errors.ts` and prefer `Result<T>` pattern for fallible operations.

## Integration notes

- Dashboard UI calls REST endpoints under `/api/*`; mock `/api/system/status` and `/api/system/metrics` when adding UI panels.
- Downloads and caching must be idempotent and resumable; preserve existing cache paths.
- Avoid adding heavy front-end deps without updating README and CI.
- **NEW**: API error responses should use standardized error format matching `EthervoxError.toJSON()`.

## PR / CI checklist

Before opening a PR, ensure the following locally:

### Build
- C runtime: `make configure && make build` (or `cmake -B build -DTARGET_PLATFORM=... && cmake --build build`)
- Node.js: `npm install && npm run build`

### Lint & types
- `npm run lint` and `npm run typecheck` (if present)
- C code follows clang-format and passes clang-tidy

### Tests
- C: `ctest` or run `./tests/test_audio_core` and other unit tests
- Node.js: `npm run test` and `npm run test:audio` (audio tests can be skipped in headless CI but should be run locally)
- **NEW**: Error handling tests pass and new code uses standardized error patterns

### Documentation
- Update `README.md` or `docs/` when public APIs or developer workflows change
- Add/update error codes in `include/ethervox/error.h` if introducing new failure modes
- Document new error types in TypeScript if extending `src/common/errors.ts`

### Error handling checklist (NEW)
- [ ] C functions return `ethervox_result_t` instead of int/bool
- [ ] Use `ETHERVOX_CHECK_PTR()` for all pointer arguments
- [ ] Use `ETHERVOX_CHECK()` or explicit error handling for all fallible calls
- [ ] Errors include context via `ETHERVOX_RETURN_ERROR()` or `ETHERVOX_ERROR_SET()`
- [ ] TypeScript uses specific error classes (not generic `Error`)
- [ ] Async TypeScript operations use try-catch or Result type
- [ ] Error context includes relevant diagnostic information

## Migration notes (error handling)

We're currently migrating to standardized error handling per `docs/error-handling-spec.md`:

- **Phase 1 (Complete)**: Core infrastructure (`include/ethervox/error.h`, `src/common/error.c`, `src/common/errors.ts`)
- **Phase 2 (In Progress)**: Critical path (audio, platform HAL, plugin manager)
- **Phase 3 (Planned)**: Full migration (STT, wake word, dialogue, dashboard API)
- **Phase 4 (Future)**: Enforcement via clang-tidy and CI checks

When touching existing code:
1. Prefer incremental migration over breaking changes
2. Maintain backward compatibility where possible
3. Update tests to verify error handling behavior
4. Add error handling examples to relevant documentation