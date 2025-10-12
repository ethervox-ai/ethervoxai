# Copilot Instructions for EtherVoxAI

## Quick orientation
- Start with `README.md` for platform goals and supported hardware, then `docs/mvp.md` for scope and guiding principles.
- Runtime C sources live in `src/` with mirrored headers in `include/ethervox/`; HAL glue is in `src/platform/` and platform audio drivers in `src/audio/`.
- The Vue 3 dashboard is in `dashboard/` (Vite + Tailwind + Pinia) and talks to the core through REST endpoints under `/api/*`.
- Developer tooling scripts (cross-toolchain setup, deployments) are under `scripts/` and `cmake/`.

## Core architecture
- `src/platform/platform_core.c` registers the correct HAL by checking the `TARGET_*` macros set from `CMakeLists.txt`; new platform work must add a `*_hal.c` file and extend the `PLATFORM_SOURCES` selection block.
- Audio flows through `ethervox_audio_runtime_t` in `src/audio/audio_core.c`, which defers hardware work to platform drivers (`platform_linux.c`, `platform_rpi.c`, etc.). Keep callbacks (`on_audio_data`, `on_stt_result`) wired in `ethervox_audio_runtime` when adding processing steps.
- Dialogue, STT, wake word, and plugin orchestration each sit in their own subdirectory under `src/` and are pulled in via the top-level `file(GLOB ...)` in `CMakeLists.txt`; dropping a new `.c` file into those folders is usually enough for CMake to pick it up.
- The SDK (`sdk/ethervox_sdk.c` + `sdk/ethervox_sdk.h`) mirrors runtime concepts for external integrators; reuse SDK helpers instead of duplicating plugin bookkeeping inside the core.

## Build & test workflows
- Native development: `make configure` + `make build` drives dependency install (`make install-deps`) before calling CMake; the build step falls back to `npm run build` when a `package.json` is present, so ensure front-end builds succeed before invoking it.
- Direct CMake: `cmake -B build -DTARGET_PLATFORM=LINUX` then `cmake --build build`. Pass `-DBUILD_TESTS=ON` to pull in `tests/` targets.
- Raspberry Pi and Windows cross-builds rely on toolchain files in `cmake/`; invoke `make configure-rpi` or `make configure-windows` after running the matching `scripts/setup-*-toolchain.sh`.
- ESP32 builds must enter `esp-idf` environment (`. $HOME/esp/esp-idf/export.sh`) and run `make build-esp32` or `idf.py build` inside `esp32-project/` where `src/` and `include/` are symlinked.
- Tests live under `tests/unit` and `tests/integration`; they are plain `assert`-style executables compiled via CMake and run with `ctest` or directly (`./tests/test_audio_core`). Expect audio and HAL tests to degrade gracefully when hardware is unavailable.

## Platform & plugin conventions
- Platform detection happens through `TARGET_PLATFORM` cache variables; when adding new code paths, gate them with the matching `ETHERVOX_PLATFORM_*` macro defined in `include/ethervox/config.h`.
- `ethervox_platform_register_hal` expects each HAL to expose `*_hal_register` that populates GPIO/audio callbacks. Keep HAL structs lean and avoid heap allocation—existing implementations store state in the passed `ethervox_platform_t`.
- The plugin manager (`src/plugins/plugin_manager.c`) uses a fixed-size array (`ETHERVOX_MAX_PLUGINS`); prefer reusing slots and update `plugin->status` transitions so `ethervox_plugin_execute` can short-circuit correctly.
- Built-in LLM plugins wrap stateless helpers (e.g., `openai_execute_wrapper`); follow the wrapper pattern when adding new built-ins so tests that assume `plugin->execute` exist continue to work.

## Dashboard guidelines
- Routing lives in `dashboard/src/router/` and global state in the Pinia store `dashboard/src/stores/system.js`; API calls go through axios and expect `/api/system/status` and `/api/system/metrics` JSON payloads. Mock these endpoints when adding new panels to keep the dashboard usable offline.
- Styling is composed with Tailwind classes from `dashboard/tailwind.config.js`; prefer utility classes and co-locate component-specific CSS in `<style scoped>` blocks when Tailwind lacks an expressive primitive.
- Run `npm install` once per root (`/` and `dashboard/`) and use `npm run dev` (Vite) during UI work; CI also runs `npm run lint` and Lighthouse per `dashboard/lighthouserc.js`, so keep performance thresholds in mind.

## Testing & quality gates
- Enable coverage locally via `cmake -DENABLE_COVERAGE=ON` before `ctest`; coverage targets are defined in `tests/CMakeLists.txt`.
- GitHub Actions workflows in `.github/workflows/` enforce clang-format, clang-tidy, Semgrep, npm audit (`dashboard/.auditci.json`), and multi-platform builds. Replicate failing checks locally by running the matching script from `docs/cicd-pipeline.md`.
- Many tests deliberately tolerate missing audio hardware; log informative messages rather than failing hard when `ethervox_audio_init` or HAL bindings return errors.
# Copilot Instructions for EtherVoxAI

## Quick orientation
- Start with `README.md` for platform goals and supported hardware, then `docs/mvp.md` for scope and guiding principles.
- Runtime C sources live in `src/` with mirrored headers in `include/ethervox/`; HAL glue is in `src/platform/` and platform audio drivers in `src/audio/`.
- The Vue 3 dashboard is in `dashboard/` (Vite + Tailwind + Pinia) and talks to the core through REST endpoints under `/api/*`.
- Developer tooling scripts (cross-toolchain setup, deployments) are under `scripts/` and `cmake/`.

## Core architecture (C runtime)
- `src/platform/platform_core.c` registers the correct HAL by checking the `TARGET_*` macros set from `CMakeLists.txt`; new platform work must add a `*_hal.c` file and extend the `PLATFORM_SOURCES` selection block.
- Audio flows through `ethervox_audio_runtime_t` in `src/audio/audio_core.c`, which defers hardware work to platform drivers (`platform_linux.c`, `platform_rpi.c`, etc.). Keep callbacks (`on_audio_data`, `on_stt_result`) wired in `ethervox_audio_runtime` when adding processing steps.
- Dialogue, STT, wake word, and plugin orchestration each sit in their own subdirectory under `src/` and are pulled in via the top-level `file(GLOB ...)` in `CMakeLists.txt`; dropping a new `.c` file into those folders is usually enough for CMake to pick it up.
- The SDK (`sdk/ethervox_sdk.c` + `sdk/ethervox_sdk.h`) mirrors runtime concepts for external integrators; reuse SDK helpers instead of duplicating plugin bookkeeping inside the core.

## Build & test workflows (C runtime)
- Native development: `make configure` + `make build` drives dependency install (`make install-deps`) before calling CMake; the build step falls back to `npm run build` when a `package.json` is present, so ensure front-end builds succeed before invoking it.
- Direct CMake: `cmake -B build -DTARGET_PLATFORM=LINUX` then `cmake --build build`. Pass `-DBUILD_TESTS=ON` to pull in `tests/` targets.
- Raspberry Pi and Windows cross-builds rely on toolchain files in `cmake/`; invoke `make configure-rpi` or `make configure-windows` after running the matching `scripts/setup-*-toolchain.sh`.
- ESP32 builds must enter `esp-idf` environment (`. $HOME/esp/esp-idf/export.sh`) and run `make build-esp32` or `idf.py build` inside `esp32-project/` where `src/` and `include/` are symlinked.
- Tests live under `tests/unit` and `tests/integration`; they are plain `assert`-style executables compiled via CMake and run with `ctest` or directly (`./tests/test_audio_core`). Expect audio and HAL tests to degrade gracefully when hardware is unavailable.

## Platform & plugin conventions (C runtime)
- Platform detection happens through `TARGET_PLATFORM` cache variables; when adding new code paths, gate them with the matching `ETHERVOX_PLATFORM_*` macro defined in `include/ethervox/config.h`.
- `ethervox_platform_register_hal` expects each HAL to expose `*_hal_register` that populates GPIO/audio callbacks. Keep HAL structs lean and avoid heap allocation—existing implementations store state in the passed `ethervox_platform_t`.
- The plugin manager (`src/plugins/plugin_manager.c`) uses a fixed-size array (`ETHERVOX_MAX_PLUGINS`); prefer reusing slots and update `plugin->status` transitions so `ethervox_plugin_execute` can short-circuit correctly.
- Built-in LLM plugins wrap stateless helpers (e.g., `openai_execute_wrapper`); follow the wrapper pattern when adding new built-ins so tests that assume `plugin->execute` exist continue to work.

## Dashboard guidelines
- Routing lives in `dashboard/src/router/` and global state in the Pinia store `dashboard/src/stores/system.js`; API calls go through axios and expect `/api/system/status` and `/api/system/metrics` JSON payloads. Mock these endpoints when adding new panels to keep the dashboard usable offline.
- Styling is composed with Tailwind classes from `dashboard/tailwind.config.js`; prefer utility classes and co-locate component-specific CSS in `<style scoped>` blocks when Tailwind lacks an expressive primitive.
- Run `npm install` once per root (`/` and `dashboard/`) and use `npm run dev` (Vite) during UI work; CI also runs `npm run lint` and Lighthouse per `dashboard/lighthouserc.js`, so keep performance thresholds in mind.

## Testing & quality gates
- Enable coverage locally via `cmake -DENABLE_COVERAGE=ON` before `ctest`; coverage targets are defined in `tests/CMakeLists.txt`.
- GitHub Actions workflows in `.github/workflows/` enforce clang-format, clang-tidy, Semgrep, npm audit (`dashboard/.auditci.json`), and multi-platform builds. Replicate failing checks locally by running the matching script from `docs/cicd-pipeline.md`.
- Many tests deliberately tolerate missing audio hardware; log informative messages rather than failing hard when `ethervox_audio_init` or HAL bindings return errors.

## Licensing & docs
- Every source file carries `SPDX-License-Identifier: CC-BY-NC-SA-4.0`; preserve headers when creating new files.
- Place new multi-platform or privacy-related references under `docs/` (see `docs/legal.md`, `docs/ethical-ai.md`) and link them from `README.md` if they affect developer workflows.
- For SDK-facing additions, update both the code and the narrative in `sdk/README.md` so external integrators inherit the correct usage pattern.

---

## Node.js / TypeScript implementation (supplementary)

If you're working on the Node.js/TypeScript side of EthervoxAI, follow these focused pointers in addition to the C guidance above:

### Where to look
- Primary narrative and examples live in `README.md` and `docs/` (see `docs/mvp.md`).
- Implementation folders: `implementations/` (python/cpp/micropython) and the TypeScript core referenced by the README (`src/modules/` examples).
- Dashboard (optional UI) lives in `dashboard/` (Vite + Vue 3 + Pinia).

### Common commands
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
```

### Node.js-specific conventions
- Model management and downloads are centralized (see `modelManager` in README examples). Use `modelManager.getRecommendedModels()` and `modelManager.getModelPath()` rather than hard-coding model names/paths.
- Hardware detection: call `platformDetector.getCapabilities()` to determine performance tiers and recommended thread counts.
- Audio: prefer `CrossPlatformAudioManager` and its `testOutputMethods()` harness when modifying audio chains; update `config/audio.json` for fallback changes.
- Keep public TypeScript types stable. Add tests under `tests/` for new behavior.

### Integration notes
- Dashboard UI calls REST endpoints under `/api/*`; mock `/api/system/status` and `/api/system/metrics` when adding UI panels.
- Downloads and caching must be idempotent and resumable; preserve existing cache paths.
- Avoid adding heavy front-end deps without updating README and CI.

### PR / CI checklist (short)

Before opening a PR, ensure the following locally:

- Build: the relevant runtime builds cleanly
	- C runtime: `make configure && make build` (or `cmake -B build -DTARGET_PLATFORM=... && cmake --build build`)
	- Node.js: `npm install && npm run build`
- Lint & types: `npm run lint` and `npm run typecheck` (if present)
- Tests: run unit tests and the audio harness where applicable
	- C: `ctest` or run `./tests/test_audio_core` where present
	- Node.js: `npm run test` and `npm run test:audio` (audio tests can be skipped in headless CI but should be run locally)

Also: update `README.md` or `docs/` when public APIs or developer workflows change.

If you want, I can expand the TypeScript file path list with explicit `src/` filenames — tell me and I'll scan the repo and append them.
