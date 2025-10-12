## EthervoxAI — Copilot instructions for editing and codegen

This file gives short, repo-specific guidance for AI coding assistants working on EthervoxAI.

Be concise. Prefer minimal, testable changes and cite the files below when referencing architecture.

- Big picture
  - Primary implementation: TypeScript/Node.js (root repository). Key directories and files: `README.md`, `docs/`, `implementations/`, `scripts/`.
  - Multi-language targets: `implementations/python/`, `implementations/cpp/`, `implementations/micropython/`. Keep changes to cross-language interfaces in `specs/` or `specs/ethervoxai-protocol.md`.
  - Main modules to inspect before edits: the modules listed in `README.md` under `src/modules/` (multilingualRuntime, localLLMStack, privacyDashboard, modelManager, inferenceEngine, platformDetector, crossPlatformAudio).

- Developer workflows (commands to run)
  - Install deps: `npm install`
  - Build core: `npm run build`
  - Run demo: `npm run demo` (Windows-specific demo: `npm run demo:windows`)
  - Audio tests: `npm run test:audio`
  - Web/mobile UI builds are optional and require installing React/React Native separately (`npm run build:web`, `npm run build:mobile`). See README for details.

- Project conventions and patterns
  - The repo has one authoritative README that lists canonical module names and locations; prefer referencing it when adding or changing modules.
  - Audio systems use a fallback chain; when modifying audio code, update `config/audio.json` and ensure `CrossPlatformAudioManager` fallback logic remains intact.
  - Model management is centralized in `modelManager` (downloads, caching, compatibility). Changes that affect model files must preserve cache paths and compatibility checks.
  - Hardware detection and optimization logic live in `platformDetector`. Avoid hard-coding performance tiers; use `platformDetector.getCapabilities()`.

- Integration points & external dependencies
  - Local model files and downloads: `modelManager` interacts with remote model sources and local cache directories. Keep download logic idempotent and resumable.
  - Audio platform bindings: Windows SAPI, espeak, node-speaker, and play-sound are supported. For platform-specific code, prefer small wrappers under `scripts/` or `implementations/`.
  - CI: workflows in `.github/workflows/` run build-and-test and code-quality checks. Expect unit tests and linting to run on PRs.

- Codegen / editing rules for AI agents
  - Make the smallest change that satisfies the task. Preserve public APIs and TypeScript types unless asked to change them.
  - Prefer to add unit tests for new behavior in `tests/` when feasible. Use `npm run test` and `npm run lint` locally.
  - When touching multi-language implementations, only change the language-specific folders if the change is language-specific; otherwise, update shared `specs/` first.
  - Do not add heavyweight dependencies (e.g., React) without documenting why and updating `README.md` installation steps.

- Examples from the codebase (where to look)
  - Hardware detection: see `platformDetector` usage in `README.md` examples and mirror its API when writing new optimizations.
  - Model recommendations: use `modelManager.getRecommendedModels()` to obtain model lists rather than embedding names.
  - Audio tests: `CrossPlatformAudioManager.testOutputMethods()` is the recommended test harness for audio changes.

- When unsure
  - Consult `README.md` and files under `docs/` (especially `docs/modules/` if present) before making design-level changes.
  - Run CI (`.github/workflows/`) locally by running `npm run build` and `npm run test` to catch lint/type errors.

If anything here is unclear or you want more detail (e.g., list of actual `src/` files), tell me which area to expand and I'll iterate.
