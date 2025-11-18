# 📘 EthervoxAI Product Requirements Document (PRD)

Version: 0.1 (Draft)
Status: For internal engineering alignment
Source Basis: `docs/mvp.md`
Owner: Product & Core Engineering

---

## 1. Purpose
Provide an actionable specification translating the MVP vision into implementable modules, interfaces, milestones, and acceptance criteria for the EthervoxAI local-first, multilingual voice intelligence platform across embedded (ESP32-S3), edge Linux (Raspberry Pi 5), and desktop reference builds.

## 2. Product Vision (Concise)
Ambient, privacy-first, multilingual voice interaction that runs locally, is extensible via plugins, and gives end users transparent control over data and optional cloud augmentation.

## 3. Personas
1. End User (Home / Small Lab) – Wants private voice control without setup complexity.
2. Embedded Integrator – Adds EthervoxAI runtime to hardware (appliances, robots).
3. Developer / Researcher – Builds custom intents, LLM routing strategies, diagnostics tools.
4. Privacy / Compliance Reviewer – Audits data flows & cloud opt-ins.

## 4. High-Level Use Cases
| ID | Title | Summary | Primary Persona |
|----|-------|---------|-----------------|
| UC1 | Ambient Command | User speaks multilingual command; system transcribes, resolves intent, executes action locally. | End User |
| UC2 | Follow-up Dialogue | Dialogue engine maintains short context for clarifications. | End User |
| UC3 | Optional Cloud Query | User asks factual / web query; explicit confirmation sends anonymized text upstream. | End User |
| UC4 | Device Profile Adaptation | Runtime loads mic + latency profiles for target hardware. | Integrator |
| UC5 | Plugin Intent | Developer ships new intent module via SDK plugin. | Developer |
| UC6 | Audit Review | Privacy reviewer inspects dashboard logs (local + cloud events). | Reviewer |
| UC7 | Language Auto-Switch | User changes language mid-session; system responds seamlessly. | End User |

## 5. Scope (MVP)
IN: Local STT, local TTS (baseline voice), language ID, intent parser (rule + embedding similarity), lightweight embedded LLM container (supports local model + pluggable external dispatch), plugin API (intents + LLM routers), privacy dashboard (web UI), hardware abstraction layer (audio + performance profiles), developer SDK (Python-first), audit logs.

OUT (Post-MVP): Wake-word training UI, emotional prosody TTS, continuous long-form summarization, >3 languages baseline, federated learning, enterprise SSO, fine-tuning pipeline.

## 6. System Architecture Overview
Layers:
1. Hardware Abstraction Layer (HAL): Audio I/O, device capabilities, performance tuning (buffer sizes, thread affinity).
2. Core Runtime: Pipelines for audio capture → VAD → language ID → STT → intent → action / response → TTS.
3. Dialogue Engine: Context store (bounded), intent dispatcher, LLM routing.
4. Privacy & Governance: Local event bus → structured log store → dashboard API.
5. Plugin Framework: Intent providers, LLM router adapters, model registry extensions.
6. SDK: High-level APIs, test harness, CLI utilities.
7. UI Layer: Dashboard (configuration, logs, permissions, language profiles, plugin management).

Data Flow (Happy Path):
Audio Frames → VAD Gate → (If speech) Lang ID → STT (Whisper Tiny / variant GGUF) → Transcript → Intent Parser (rules + embedding) → Intent Result → Action (local / plugin) → (Optional external LLM call if allowed) → Response Text → TTS Synth → Audio Out.

## 7. Core Modules & Requirements

### 7.1 Speech Runtime
Functional:
- SR-1: Process <120ms frame cadence end-to-end (capture → STT partial hypothesis) on RPi 5.
- SR-2: Provide streaming partial transcripts (token or phrase-level) with incremental language ID updates.
- SR-3: Support dynamic multi-language detection (English, Spanish, Mandarin) within a single session.
- SR-4: Expose gRPC / local IPC and in-process Python API interfaces.
- SR-5: Configurable latency vs. accuracy profiles (low_latency, balanced, accurate).
Non-Functional:
- Latency target: First partial <700ms; finalization <1.5s after end of utterance.
- Memory budget (RPi 5): <800MB resident for baseline models loaded.
- ESP32-S3: Delegated minimal footprint path (offloaded heavy STT via companion board OR simplified keyword/intent stub) – documented fallback.

### 7.2 Language Identification
- LI-1: Provide probability distribution per active language every 500ms during speech.
- LI-2: Trigger automatic response language switch when confidence >0.75 sustained for 1s and differs from current.

### 7.3 Intent & Dialogue Engine
Functional:
- DE-1: Hybrid intent resolution: (a) Rule templates (YAML), (b) Embedding semantic similarity, (c) LLM fallback (local first, external optional).
- DE-2: Maintain rolling window of last N (configurable, default 6) utterances for context.
- DE-3: Provide pluggable action handlers with side-effect sandboxing.
- DE-4: Deterministic mode (no LLM) for offline / compliance scenarios.
- DE-5: Return structured intent object: {intent_id, confidence, slots, source_channel, trace}.
Non-Functional:
- Deterministic path latency (rule + embedding) <120ms after transcript ready.
- Pluggable LLM call timeouts & circuit breaker.

### 7.4 External LLM Plugin API
- LLM-1: Unified adapter interface: load(), metadata(), generate(prompt, context, settings), stream(optional), close().
- LLM-2: Support provider capability flags: {supports_stream, supports_system_prompt, max_context_tokens}.
- LLM-3: Local policy enforcement: Disallow outbound call unless privacy flag & runtime policy permit.
- LLM-4: Sandbox logs call: {timestamp, provider_id, token_counts, redaction_applied}.

### 7.5 Privacy Dashboard & Governance
Functional:
- PD-1: Real-time view of recent events (audio start/stop, transcript, intent, external calls) with severity tagging.
- PD-2: Local-only: No dashboard asset fetched from remote CDN.
- PD-3: Explicit toggle UI for each external provider + per-intent cloud allowlist.
- PD-4: Export audit bundle (JSON + cryptographic hash) for last N hours.
- PD-5: Provide redaction engine (emails, phone numbers) before optional cloud send.
Non-Functional:
- All logs stored in local append-only file or embedded DB (e.g., SQLite / LiteFS local mode) with rotation policy.

### 7.6 Hardware Abstraction Layer (HAL)
- HAL-1: Abstract audio backend (ALSA, PulseAudio) behind uniform capture / playback API.
- HAL-2: Provide device profiles: {id, sample_rate, frame_size, buffer_ms, channels, vad_params, dsp_flags}.
- HAL-3: Benchmark harness auto-generates performance JSON per device.

### 7.7 Developer SDK
- SDK-1: Provide CLI: `ethervox` (init plugin, test intent, profile device, tail logs).
- SDK-2: Python API surface (initial): SpeechSession, IntentEngine, PluginRegistry, AudioStreamSource.
- SDK-3: Intent plugin packaging spec: entrypoint (`ethervox_plugin.py`) exporting `register(plugin_registry)`.
- SDK-4: Type hints + pydantic (or lightweight dataclass-based) models for contracts.
- SDK-5: Provide test harness with synthetic audio clips + expected transcripts / intents.

### 7.8 Data Models (Initial Contracts)
Minimal illustrative (final definitions in code):
```
TranscriptChunk: { id, text, language, start_ms, end_ms, is_final, tokens? }
IntentResult: { intent_id, confidence, slots: {k:v}, mode: rule|embed|llm, trace_id }
LLMCallLog: { id, provider_id, prompt_hash, tokens_in, tokens_out, redactions:[...], allowed_by }
EventLog: { ts, type, payload, severity }
DeviceProfile: { id, arch, cpu_features, sample_rate, frame_size, vad, tts_voice }
```

## 8. Non-Functional Requirements (Cross-Cutting)
| Category | Requirement |
|----------|-------------|
| Privacy | No raw audio leaves device; transcripts only leave if explicit per-request consent. |
| Security | Signed plugin manifests (future flag); sandbox path restrictions. |
| Performance | 95th percentile end-to-end (speech end → TTS start) <2.2s on RPi 5. |
| Reliability | Recover from STT model load failure with fallback model; auto-retry once. |
| Observability | Structured logs + optional OpenTelemetry exporter (post-MVP toggle). |
| Internationalization | Unicode-safe processing; language configuration persisted per user profile. |
| Energy | Duty-cycle VAD to reduce CPU when silence >3s (enter low-power loop). |

## 9. API Surface (Draft Summaries)
Python (names tentative):
```
speech = SpeechRuntime(config)
for chunk in speech.stream(mic_source):
    intent = intent_engine.process(chunk)
    if intent: dialogue.respond(intent)

intent_engine.register_rule_intent(patterns=[...], id="set_timer")
plugin_registry.load(path)
llm = llm_router.route(prompt, context)
```
gRPC Endpoints (outline):
- StreamAudio (client stream) → TranscriptStream (server stream)
- GetIntent (unary) / StreamIntents (server stream)
- ListPlugins / InstallPlugin (local only) / ListLLMProviders
- GetEvents / ExportAuditLog

## 10. Platform Targets & Adaptations
| Platform | Strategy | Notes |
|----------|----------|-------|
| ESP32-S3 | Minimal capture + wake / keyword (stub) or offload via UART/SPI to edge node | Document fallback pipeline. |
| Raspberry Pi 5 | Full pipeline + baseline models (Whisper Tiny) | Primary reference. |
| Desktop (Dev) | Debug + profiling; enables heavier local LLM. | Not marketed. |

## 11. Milestones & Engineering Deliverables
| Milestone | Description | Exit Criteria |
|-----------|-------------|---------------|
| M1 | Core audio + STT + partial transcript | Streaming API returns partial + final segments; latency targets instrumented. |
| M2 | Intent engine (rule + embedding) + dialogue context | 5 sample intents pass test harness. |
| M3 | Privacy dashboard (events + cloud toggle) | UI lists last 100 events; export works. |
| M4 | External LLM plugin framework | OpenAI + local-gguf adapters functional behind policy gate. |
| M5 | Auto language switching + multi-language TTS response | Live demo switching EN⇄ES. |
| M6 | SDK + plugin packaging + sample plugin repo | `ethervox init-plugin` creates runnable example. |
| M7 | Hardware profiles + performance baseline published | JSON report for RPi 5 in repo. |

## 12. Acceptance Tests (Representative)
| ID | Scenario | Criteria |
|----|----------|---------|
| AT1 | Partial transcript speed | First partial <700ms (median) in 10 utterance test set. |
| AT2 | Intent resolution deterministic mode | Rule-only mode yields same outputs across 5 runs. |
| AT3 | Cloud policy enforcement | External LLM blocked when toggle off; log entry created. |
| AT4 | Language auto-switch | Mixed-language utterances produce correct response language in 3 test cases. |
| AT5 | Plugin isolation | Malformed plugin raises controlled error without crashing runtime. |
| AT6 | Audit export integrity | Export hash recomputed identical after re-import. |

## 13. Risks & Mitigations
| Risk | Impact | Mitigation |
|------|--------|------------|
| Model load memory spikes | OOM on low-memory devices | Lazy load + quantized variants + memory guard. |
| Latency regression with multi-language | Poor UX | Adaptive frame sizing + early finalize heuristics. |
| Plugin security | Malicious code | Signed manifests (post-MVP), sandbox import runner. |
| External provider leakage | Privacy breach | Central policy gate + explicit per-request consent token. |
| Audio driver variance | Instability | HAL abstraction + integration tests per backend. |

## 14. Observability & Telemetry
- Structured JSON logs (stdout + rotating file): `level, ts, module, event_type, payload`.
- Metrics (post-MVP optional): latency histograms, token counts, memory watermark.
- Trace correlation: `trace_id` propagated across transcript → intent → LLM call → TTS.

## 15. Directory & Code Organization (Proposed Python)
```
implementations/python/ethervoxai/
  audio/ (capture, VAD, language_id)
  stt/ (whisper loader, streaming wrapper)
  tts/ (baseline voice synth)
  intent/ (rule_engine.py, embeddings.py, dialogue.py)
  llm/ (adapters/, router.py, policies.py)
  plugins/ (registry.py, sandbox.py)
  privacy/ (logs.py, redaction.py, audit_export.py)
  hal/ (profiles.py, backend_alsa.py, backend_pulse.py)
  sdk/ (cli/, api.py, plugin_scaffold/)
  ui/ (dashboard backend bridge)
  config/ (defaults.yaml)
  tests/ (structured by module)
```

## 16. Testing Strategy
- Unit: Core transforms (VAD gating, rule intent match, embedding similarity).
- Integration: Audio→Intent pipeline using prerecorded multilingual samples.
- Plugin: Load/unload, failure injection.
- Performance: Benchmark script capturing latency distributions.
- Privacy: Policy gate tests (simulate blocked outbound calls).
- Hardware: Profile script executed on RPi 5 generating JSON stored under `docs/perf/`.

## 17. Definition of Done (Per Feature)
1. Code implemented with type hints.
2. Tests (unit + minimal integration) passing (>80% line coverage for core runtime modules).
3. Lint + static analysis (ruff / mypy) clean (if adopted).
4. Docs updated (API reference snippet + README section if public surface changed).
5. Performance budget not violated (checked via benchmark harness where relevant).

## 18. Open Questions
- Do we standardize on pydantic vs. dataclasses (performance vs. validation)?
- Which minimal quantized model for Mandarin TTS baseline?
- Do we embed OpenTelemetry early or defer entirely post-MVP?

## 19. Post-MVP Candidates
- Federated adaptation of acoustic / language profiles.
- Edge-to-edge encrypted shared context across devices.
- Rich emotion / style transfer TTS voices.
- On-device summarization window for long sessions.

## 20. Change Log
v0.1: Initial draft from MVP.

---
End of Document.
