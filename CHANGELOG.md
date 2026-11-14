# Changelog

All notable changes to this project are documented here, following
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
[Semantic Versioning](https://semver.org/).

---

## [Unreleased]

### Added
- **Role-structured prompts for `ethervox_generate_structured` (`C6.1a`).**
  `ethervox_structured_gen_params_t` gains `system_prompt`, `turns`/`turn_count`
  (`ethervox_chat_turn_t`: assistant or tool); `ethervox_structured_render_prompt` renders
  system + user + turns + assistant-start through the chat template, byte-identical to Granite's
  own Jinja output for a tool round trip. `chat_template_t` gains `tool_response_start/_end`
  (Granite 4's `<tool_response>` user turn) and `chat_template_format_tool_response`; every other
  template and `governor.c` are unchanged. `ETHERVOX_LOG_PROMPT=1` logs the exact tokenizer input.

### Fixed
- **`convert_object` (`json_schema_to_gbnf.c`) emitted the comma between members unconditionally**, so
  an optional member produced a mandatory comma (stray trailing/leading comma when omitted). The comma
  now lives inside the optional group of the member that follows another; members keep `properties`
  order (`BACKLOG-74`). New `test_schema_object_members`.
- **`ethervox_generate_structured` fired `ETHERVOX_EVENT_USAGE` a second time after `FINISHED`** on the
  EOG path, so the stream no longer ended with `Finished`.
- Corrected the `CHAT_TEMPLATE_GRANITE_4` enum comment: Granite 4 tool calls are
  `<tool_call>{json}</tool_call>`, not a `<|tool_call|>` token.
- **`prefill_prompt` never cleared the context's KV cache before tokenizing a new prompt.**
  Every other generation path in this codebase (`governor.c`, `llama_backend.c`, `session.c`,
  `granite_speech_backend.c`) clears memory via `llama_memory_clear` before decoding; this one
  never did. Since `ethervox_generate_structured`'s caller always resends the whole cumulative
  prompt on every call (never an incremental continuation), each call just kept appending at the
  context's own ever-advancing internal position — so repeated, entirely independent calls
  against the same loaded handle (exactly what `ev-agent`'s multi-turn tool-calling loop does)
  exhausted even a modest context regardless of any single prompt's size. Confirmed live: a
  handful of short calls aborted `llama_decode` with `"failed to find a memory slot"`, after
  which the same handle could never decode again for the rest of its lifetime. Fixed by clearing
  memory at the start of `prefill_prompt`, matching every other call site. **Follow-up**: that
  first fix passed `data=true` (clears the data buffers too, not just position bookkeeping),
  copying the nearby call sites without noticing they clear for a real "wipe this session"
  reason — here it just needs to reset positions so new tokens claim fresh slots; the new decode
  overwrites whatever bytes were there regardless. `data=true` physically zeroes the underlying
  buffers on every single turn of a multi-turn tool-calling conversation, real Metal/CPU work
  reported as the whole machine staying unresponsive for up to a minute after a reply had already
  appeared. Changed to `data=false`.

- **`ethervox_generate_structured` ignored its own cancellation signal.** `ethervox_event_cb`
  returns a `bool` (continue/stop), and the Rust FFI layer correctly returned `false` on
  cancellation, but `structured_generation.c`'s internal `fire_event()` helper was `void` and
  discarded that return value — so a cancelled generation always ran to `max_tokens` regardless.
  Observed live: once a lazy tool-call grammar triggered, a run that never reached a grammar-valid
  EOS token would grind through the full token budget with no way to interrupt it, pegging the CPU
  and freezing the whole app (and, under heavy contention, the OS UI) for up to a minute.
  `fire_event()` now returns the callback's result, and `generate_tokens()`'s main loop stops
  immediately (firing a `"cancelled"` `Finished` event) the first time it sees `false`.

---

## [core-v0.3.0] - 2026-08-10

**ADR-0023: One `ethervox_core`, one line** — Merged `feature/granite-speech-voice-integration` (37 commits) into `main`. Deleted 8 stale branches. All products now track one main line via tags.

### Added (Phase C1.0–C1.6)
- **4-profile CI matrix** (TASK-C0.1): All four profiles (EDGE, MOBILE, DESKTOP, WORKSPACE) now build and pass `ctest` on every PR via GitHub Actions
- **PR template** requiring consumer impact statement per ADR-0023 §5
- All features from C1.0–C1.6 (see Unreleased section below, now released)

### Changed
- **Repository policy** (ADR-0023): `main` is now the only long-lived branch. Products pin tags, not SHAs. Feature branches are short-lived and deleted on merge.
- All consumers must now use tags (e.g., `core-v0.3.0`) instead of branch heads

### Removed
- 8 stale remote branches: `feature/granite-speech-voice-integration`, `agents/unified-voice-model-architecture`, `dev/architecture_change__tool_manifest_approach`, `dev/llm_first_approach`, `feat-android-basic-frontend`, `feat/conversation-summarization`, `feat/memory-tool`, `feat/voice-tool`

### Breaking Changes
- **TTS subsystem removed**: Old `ethervox_tts_*` APIs deleted, replaced with `ethervox_tts_host_t` interface
- **Paths required**: Must supply `ethervox_paths_t`, no more hardcoded `HOME`/`Documents/` derivations
- **Profile required**: Must set `-DETHERVOX_PROFILE=<EDGE|MOBILE|DESKTOP|WORKSPACE>` in CMake
- **KV cache invalidated**: Old cache files (`system_prompt_*.kvcache`) won't be found, triggers one-time rebuild

### Consumer Impact
- ✅ **ethervoxai-apple (Workspace)**: Already at feda1a0, re-pinned to `core-v0.3.0`, builds successfully
- ⚠️ **ethervoxai-android (Friend'O'Mine)**: Pinned at 07fc7e7 with TTS/phonemizer code. Must implement N5.5 (TTS host) before re-pinning to avoid breaking read-aloud (accessibility-critical)
- ✅ **ethervoxai-ios**: Pinned at 07fc7e7, does not use C TTS API, safe to re-pin with paths API adoption

See TASK-C0.1 execution log in `ethervoxai-planning/tasks/PHASE-C/C0.1-unify-the-core-line.md` for detailed consumer impact audit.

---

## [Unreleased]

### Fixed
- **`structured_generation.c`'s `create_sampler()` crashed on grammar-constrained generation at
  `temperature <= 0`** (found while building `ev-agent`'s tool-call grammar, `BACKLOG-58`/`59`):
  the sampler chain applied `temperature` *before* `grammar`, so greedy sampling collapsed every
  candidate to `-inf` except the single argmax token before the grammar sampler ever saw them -- if
  that one token wasn't grammar-valid, the chain was left sampling from an all-`-inf` distribution
  and crashed deep in llama.cpp's sampling code (an uncaught exception crossing the C ABI,
  `SIGABRT`). Reordered to `penalties → grammar → temperature → top_k → top_p → dist`, matching
  the reasoning `llama_backend.c`'s own sampling path already uses more defensively. Reproduced
  and verified fixed with two real-model probes, independent of lazy mode or grammar complexity
  (`ev-llm::real_backend::tests::probe_temperature_zero_*`).
- **`create_sampler()` ignored lazy-grammar mode entirely** -- `ethervox_grammar_set_lazy_mode`
  (`grammar.h`) was fully implemented at the grammar-object level, but
  `ethervox_generate_structured`'s sampler creation never checked
  `ethervox_grammar_is_lazy`/`_get_trigger_words` and always built an immediate (non-lazy) grammar
  sampler regardless of what the caller set. Added the same `llama_sampler_init_grammar_lazy_patterns`
  branch `llama_backend.c` already uses. A separate, still-open crash remains once a lazy
  grammar's trigger actually fires mid-generation -- narrowed to
  `llama_sampler_init_grammar_lazy_patterns`'s own trigger-detection state machine, not this
  ordering bug (`BACKLOG-59`, `ethervoxai-planning/tasks/BACKLOG.md`).

### Fixed
- **`ethervox_embed_texts` (C2.4) never actually worked** - found while writing `ev-llm`'s first
  real `EmbeddingSession` test (`TASK-E4.7`), the C unit test only exercised NULL-argument checks:
  - `llama_tokenize(vocab, text, len, NULL, 0, ...)` is llama.cpp's own "probe the required count"
    idiom - it returns the *negative* of the token count on a too-small buffer (0 always is, for
    non-empty text), not a failure. The code checked `n_tokens < 0` directly, so every non-empty
    text failed tokenization unconditionally
  - `ethervox_model_config_t` had no way to request an embeddings-capable context -
    `llama_get_embeddings()`/`llama_get_embeddings_seq()` return `NULL` from a context created
    without `embeddings=true`, even on a model that otherwise supports it. Added a new
    `embeddings` field, threaded into context creation
  - `llama_get_embeddings_seq()` additionally requires `pooling_type` set at context-creation time
    - it cannot be chosen per-call the way `ethervox_embed_texts`'s own `pooling` parameter implies.
    Context creation now sets `pooling_type = LLAMA_POOLING_TYPE_MEAN` whenever `embeddings=true`.
    **Known limitation**: `ETHERVOX_EMBED_POOLING_CLS`/`_LAST` are accepted but not yet honored -
    every embeddings-enabled context is pooled as MEAN regardless of the per-call `pooling`
    argument, since `ethervox_model_config_t` has no per-strategy field yet

### Added
- **C3.6b (complete)**: runtime projector attach/detach without a model reload
  - `ethervox_stt_granite_speech_release_projector()`/`_reattach_projector()`: new public API on
    the STT runtime, pool-backed only (`runtime->config.pool` set) - detaches/reattaches the
    ~1.1 GB audio projector via the existing `ethervox_model_pool_detach_projector()`/
    `attach_projector()` primitives (C3.6a), refreshing the backend's cached `mtmd_ctx` pointer so
    a stale pointer can never be used after detach. Legacy direct-loading (no pool) returns
    `ETHERVOX_ERROR_NOT_SUPPORTED` - there's nothing pool-level to detach there
  - Verified end-to-end in `tests/integration/test_c36b_shared_governor_stt.c` against the real
    Granite Speech Plus GGUF: `release_projector()` frees 1113 MB without unloading the model,
    `reattach_projector()` restores it, and a transcription run afterward produces the identical
    correct transcript, proving the model was never reloaded
  - This was C3.6b's last open acceptance criterion - the packet is now done
- **C3.6b (partial, second pass)**: byte-identical-conversation + threading rule verified for real
  - `tests/integration/test_c36b_threading_and_context.c`: loads a Governor with audio support,
    populates a real conversation via `ethervox_governor_execute()`, snapshots `get_kv_pos()` +
    `get_kv_cache_contents()`, runs a real `ethervox_governor_transcribe_audio()` pass, confirms
    both are byte-for-byte unchanged, then runs `execute()` and `transcribe_audio()` concurrently
    from separate threads to exercise the new `ctx_mutex` under real contention
  - **Found and fixed two more real bugs**: (1) `ethervox_governor_transcribe_audio()` had never
    worked - `load_model_with_audio()`'s own `mtmd_init_from_file()` call left `media_marker` at
    mtmd's default based on a comment claiming the mmproj GGUF's metadata overrides it, which it
    doesn't; every call failed with `mtmd_tokenize rc=1`, the exact bug STT's own loader had already
    worked around elsewhere, never carried here. (2) `ethervox_governor_unload_model()` and
    `ethervox_governor_cleanup()` called `llama_free()`/`llama_model_free()` unconditionally even
    when pool-backed (C3.6a) - a real use-after-free/double-free once governor and STT actually
    share weights; both now route through `ethervox_model_pool_unload()` when appropriate
  - Added `ctx_mutex`: `governor.c` had zero mutex protection anywhere, yet Mode 1's unified voice
    architecture runs chat generation (seq 0) and ASR decode (seq 2) against the SAME
    `llama_context` - unsafe to decode concurrently regardless of sequence. Locked at every public
    entry point that touches `llm_ctx`, using a `cleanup`-attribute guard (not hand-placed
    lock/unlock) since `ethervox_governor_execute()` alone has many early returns across hundreds
    of lines where a missed unlock would deadlock permanently
- **C3.6b (partial)**: real governor+STT model sharing verified against a live model
  - `tests/integration/test_c36b_shared_governor_stt.c`: loads a governor-shaped handle
    (`role="main"`, no mmproj) then an STT handle (`role="speech"`, with mmproj) against the same
    real GGUF (`granite-speech-4.1-2b-plus-Q4_K_M.gguf`), confirms the memory delta matches
    KV+projector only (not a second full model), and runs a real transcription through the shared
    model - transcript comes out correct (`"[Speaker 1]: The quick brown fox jumps over..."`)
  - **Found and fixed a real accounting bug** while writing that test: `load_shared_context()`
    added the projector's bytes to `pool->used_bytes` twice - once directly inside
    `attach_projector()`, once again via the handle's `memory_bytes` at registration. A correctly
    shared model looked like a second full copy had loaded (delta 2235 MB, now 1121 MB)
  - Still open: runtime projector detach/reattach without a full handle unload (the API exists,
    nothing calls it from STT's lifecycle yet), the threading rule between governor and STT
    contexts sharing one model, and the full quality-comparison/ASan pass (→ C3.6c)
- **C3.6a (complete)**: Model-pool weight sharing wired into governor + STT
  - `ethervox_model_pool_load_shared_context()`/`attach_projector()`/`detach_projector()` were
    already fully implemented in `model_pool.c` but never declared in `model_pool.h` - an
    implicit-declaration bug that made them uncallable outside that translation unit. Fixed the
    header and added `model_path`/`use_gpu` tracking to `ethervox_model_handle_t` needed for the
    by-path lookup
  - `governor.c`'s pool-backed branch and `granite_speech_backend.c` both now call
    `ethervox_model_pool_load_shared_context()` instead of `ethervox_model_pool_load()` - whichever
    of governor (`role="main"`) or STT (`role="speech"`) loads a given GGUF first pays the full
    weight cost; the other reuses it and only pays for its own context/KV cache. Falls back to a
    fresh load when no handle for that `model_path` exists yet - a no-op for either caller alone
  - Verified end-to-end against a real GGUF (`granite-4.0-h-1b-Q4_K_M.gguf`, 988 MB): first handle
    charges the full 988 MB, a second handle sharing it charges +0 MB (KV-only)
  - `tests/unit/test_model_pool.c`'s `test_shared_context()`, runs against a real model path
    passed as `argv[1]` (skips without one)
  - `ethervoxai-android`/`ethervoxai-ios` consumer builds verified (ADR-0023)
- **C3.5 (complete)**: Streaming transcription events
  - `ETHERVOX_EVENT_TRANSCRIPTION_SEGMENT` + `ethervox_event_transcription_segment_t`
    (`event_stream.h`): segment_id (stable across revisions), speaker_id, text, is_final - reuses
    the existing C1.5 event stream rather than a new callback type
  - `ethervox_voice_tools_set_event_callback()`: register a callback on
    `ethervox_voice_session_t`, additive only - `ethervox_voice_tools_stop_listen()`'s returned
    transcript is unchanged whether or not a callback is registered
  - Wired into `finalize_chunk_and_restart()` (Mode 2/Granite Speech Plus): fires once per chunk
    already finalized during capture (every `GRANITE_SPEECH_CHUNK_SECONDS`), before the recording
    stops - this is the mechanism `ethervoxai-android` N7.8's segment-level interim already reads
    from at stop time; now it can be pushed as it happens instead
  - Every segment from this call site is emitted `is_final=true` at birth: Granite Speech Plus's
    `finalize()` is one-shot per chunk with no intra-chunk partial, so the revision path exists in
    the struct for a future backend with real incremental decoding, not exercised by this one
  - **Backpressure**: segments push onto a bounded (16-slot) queue delivered by a dedicated
    dispatch thread, so a slow host callback can never stall the capture thread. Capacity is
    generous (>10 minutes of undelivered backlog at this call site's own cadence); a
    pathologically slow host blocks the push rather than dropping a final segment. Torn down
    cleanly in `stop_listen()`/`cleanup()`
  - `tests/test_streaming_transcription.c`: event struct shape, callback register/clear/NULL-safety,
    callback invocation with expected field values (13 assertions, no live model needed)
  - `tests/test_streaming_transcription_live.c`: **live-model, real-audio verification** - two
    chunks fed through the real `ethervox_stt_init/start/process/finalize` sequence, mirroring
    `finalize_chunk_and_restart()` exactly. Confirms: real Granite Speech Plus decode produces a
    correct transcript per chunk (segments genuinely arrive incrementally, not just at stop);
    real non-Latin script (Japanese) survives a segment boundary intact as valid UTF-8
  - **Found and fixed a real crash bug** (BACKLOG-31): `config.prefix_text` was pointed directly
    at `session->prefix_carry` (a fixed array embedded in `ethervox_voice_session_t`, not heap
    memory), but `ethervox_stt_cleanup()` unconditionally `free()`s it - undefined behaviour,
    reliably crashing on cleanup after >=1 chunk with `prefix_text` set. Fixed by always
    `strdup()`ing into `config.prefix_text` and `free()`ing the previous value first, in both
    `finalize_chunk_and_restart()` and `ethervox_voice_tools_start_listen()`'s reset path
  - **Found and filed a separate quality bug** (BACKLOG-31): with the crash fixed, chunk 2's
    transcription still degrades to near-empty output whenever `prefix_text` is carried forward
    from the previous chunk - same audio, same model, only that field differs. Root cause not
    isolated; needs further live-model iteration on `granite_speech_decode.c`'s prefix-injection
    point, out of scope for this event-plumbing packet
  - Verified: all four `ETHERVOX_PROFILE` values build; `ctest -R
    "test_streaming_transcription|test_tool_catalogue"` green; `ethervoxai-android`'s
    `./gradlew assembleDevDebug` and `ethervoxai-ios`'s `./build_backend.sh macos` both succeed
- **C2.6b (complete)**: Tool catalogue migration finished — all ~40 tools now data
  - `tools/catalogue/conversation_tools.json`: `speak`, `listen`, `listen_and_summarize` migrated
  - `tools/catalogue/workspace_tools.json`: all 7 Tauri workspace tools migrated
    (`workspace_list_objects`, `workspace_search_objects`, `workspace_get_object`,
    `workspace_create_note`, `workspace_create_connection`, `workspace_update_object`,
    `workspace_export_to_docx`, `workspace_highlight_nodes`) - the only `WORKSPACE`-only contracts
    in the catalogue so far
  - `tools/catalogue/memory_tools.json`: all 10 memory tools migrated
  - `tools/catalogue/file_tools.json`: all 9 file/path tools migrated - corrected
    `file_write`/`file_append`'s `profiles` to exclude `WORKSPACE`, matching the plugin's existing
    `ETHERVOX_FEATURE_FILE_TOOLS=OFF` build-time exclusion for that profile
  - `tools/catalogue/system_info_tools.json`, `weather_tools.json`, `context_tools.json`,
    `meta_tools.json`, `startup_prompt_tools.json`, `timer_tools.json`, `unit_conversion.json`:
    remaining groups migrated (see prior entries in this section)
  - Deleted dead `src/plugins/conversation_tools/train_pronunciation.c` - unreferenced since C1.0's
    phonemizer removal
  - No `.parameters_json_schema =` literal remains anywhere under `src/plugins/`
  - `tests/test_tool_catalogue.c`: 17 golden + loader-behaviour tests, covering every migrated group
  - **Found a real, pre-existing bug while migrating** (BACKLOG-29): `ethervox_tool_t.description`
    is `char[256]`, but 7 tools' descriptions exceed that (`speak` 655 chars, `unit_convert` 569,
    `workspace_update_object` 516, `workspace_create_note` 514, `workspace_highlight_nodes` 477,
    `workspace_export_to_docx` 381, `listen_and_summarize` 307) - these were already
    over-length string-literal initializers before this migration, a C99 constraint violation most
    compilers only warn on and silently truncate **without a guaranteed NUL terminator** (undefined
    behaviour on every read). The catalogue loader's `memset`+`strncpy` makes the truncation
    deterministic and NUL-terminated instead - strictly safer, though it does still drop the
    description's tail. Growing the field is an ABI change to a struct copied by value everywhere
    in the registry, so it's filed as BACKLOG-29 rather than fixed in this migration packet.
  - Verified all four `ETHERVOX_PROFILE` values (`EDGE`, `MOBILE`, `DESKTOP`, `WORKSPACE`)
    configure and build `libethervoxai.a` cleanly with the new catalogue embedding
  - **Fixed a real `EDGE`-profile gap**: `file_tools`' CMake glob was gated only on `NOT WIN32`,
    never on `ETHERVOX_FEATURE_FILE_TOOLS` — only the `main.c` registration call site consulted the
    flag, so the plugin and its catalogue JSON still linked into `EDGE`/`WORKSPACE` binaries
    despite that flag being `OFF` for both. Gated the glob on the flag too, matching
    `weather_tools`' existing pattern. Verified via a scratch `EDGE`-profile build: `strings
    libethervoxai.a` shows zero occurrences of `file_list`/`path_set`/etc. afterward.
    `workspace_tools` has no equivalent flag and remains compiled into every profile — filed as
    BACKLOG-30 rather than added here (no equivalent exclusion is named in `16-TOOLS.md`)
  - Verified against real consumers, not just config: `ethervoxai-android`'s
    `./gradlew assembleDevDebug` (full NDK cross-compile + JNI + APK) and `ethervoxai-ios`'s own
    `./build_backend.sh macos` (macOS + iOS device + iOS Simulator `libethervoxai.a`) both succeed
- **C2.6a (complete)**: Tool catalogue format, loader and pilot migration
  - `tools/catalogue/compute_tools.json`: tool contracts (name, profiles, description, is_mutating,
    schema) for the `compute_tools` group, replacing hardcoded C literals (`16-TOOLS.md`)
  - `ethervox_tool_catalogue_load()` (`tool_catalogue.h`/`.c`): fills a tool's name/description/schema
    from a catalogue JSON array by name; refuses a build profile not in the contract's `profiles`
    list; rejects an unrecognised profile name as a data error
  - `cmake/EmbedCatalogue.cmake`: embeds catalogue JSON into a generated C header at configure time
    — no runtime file I/O, so it works sandboxed and on ESP32
  - Migrated `calculator_compute`, `percentage_calculate`, `get_time`, `get_date`, `get_day`,
    `time_get_week_number` to load their contracts from the catalogue; `execute`/`test_scenario`/
    flags remain hand-written C (a function pointer isn't data)
  - `tests/test_tool_catalogue.c`: golden test proving byte-identical name/description/schema for
    all six migrated tools, plus loader NOT_FOUND/NOT_SUPPORTED/INVALID_ARGUMENT/success tests
  - Remaining ~34 tools' migration split out to C2.6b (`ethervoxai-planning/tasks/PHASE-C/C2.6b-*.md`)
- **C2.2b: wire host tools into the governor's actual tool-dispatch loop** — C2.2 built the
  `is_mutating` refusal primitive (`ethervox_host_tool_is_mutating`/`_invoke`) but never connected
  it to `governor.c`'s real tool-execution path (`execute_tool_call_json`), which only ever
  searched the built-in `ethervox_tool_registry_t`. Host-registered tools (Android's `AlarmTool`,
  `TimerTool`, `SystemInfoTool`, etc. from N5.2) were listed in the system prompt via
  `manifest_registry` but had no execution path at all - calling one returned "Unknown tool".
  - Added `ethervox_host_tool_exists()` (`host_tools.h`/`.c`) - `is_mutating()` alone can't
    distinguish "not registered" from "registered, not mutating"
  - `execute_tool_call_json()` now falls back to the host tool registry when a tool isn't found
    in the built-in registry, so non-mutating host tools actually execute end to end
  - Mutating host tools emit the new `ETHERVOX_GOVERNOR_EVENT_TOOL_CALL_REQUESTED` progress event
    and fail closed (denied) unless a confirmation callback is registered and approves - see
    `ethervox_governor_set_tool_confirmation_callback()` in `governor.h`. No callback is currently
    registered from Android; wiring a real approve/deny UI is N5.4's job
  - Denial is fed back to the model as a plain-language tool error ("User denied this action."),
    reusing the existing tool-error continuation path rather than aborting the run
  - The XML-attribute tool-call format (`execute_tool_call`) is unchanged - its fixed attribute
    whitelist is built-in-tool-specific and isn't how host tools are called (they use the JSON
    format); this was a deliberate scope decision, not an oversight
  - `test_host_tools.c`: added `test_exists` (4/4 new assertions); full suite passes standalone
    (`ctest -R HostTools`) and `assembleDevDebug` builds clean end to end
  - Follow-up, not in this change: an Android-side confirmation callback (N5.4) and a governor-loop
    integration test that exercises a live tool call through `ethervox_governor_execute()`
- **C5.1 (complete)**: LoRA adapter loading
  - New `adapter.h` API for loading and managing LoRA adapters
  - `ethervox_adapter_load()` loads adapters from GGUF files, associated with model handles
  - `ethervox_adapter_free()` manually frees adapters (or auto-freed with model)
  - `ethervox_model_set_adapters()` applies one or more adapters to a model context with configurable scales
  - Metadata inspection functions: `ethervox_adapter_get_metadata()`, `ethervox_adapter_meta_count()`, `ethervox_adapter_meta_key_by_index()`, `ethervox_adapter_meta_value_by_index()`
  - Wraps llama.cpp's existing `llama_adapter_lora` support, zero-copy pass-through
  - Test suite: 4/4 tests pass (NULL handling, adapter loading, metadata, apply/clear)
  - Use case: Granite Libraries adapters for certainty estimation and hallucination detection (post-1.0)
- **C4.3 (complete)**: Structured logging and metrics
  - Extended `logging.h` with structured logging types: `ethervox_log_subsystem_t` (8 subsystems + unknown), `ethervox_log_field_t` (key-value pairs), `ethervox_log_entry_t` (level/subsystem/fields/timestamp)
  - `ethervox_log_set_callback()` to register structured log callback (thread-safe)
  - `ethervox_log_ex()` for subsystem-aware logging, `ethervox_log_fields()` for field-based logging
  - Updated `ethervox_log()` to invoke callback with default CORE subsystem (backward compatible)
  - `ethervox_metrics_t` struct with 20 fields: model pool metrics (loaded/evicted/bytes), generation metrics (total/succeeded/failed/tokens/time), session metrics (active/forked/kv hits+misses), grammar metrics (structured gens/avg confidence), error metrics (total/OOM/timeout)
  - `ethervox_metrics_snapshot()` for point-in-time metrics, `ethervox_metrics_reset()` for cumulative counter reset
  - Internal helper functions for metrics recording (generation, structured gen, error tracking)
  - Test suite: 5/5 tests pass (callback invocation, subsystem tagging, structured fields, metrics snapshot, metrics reset)
  - Removed conflicting old callback typedef from `config.h` (replaced with new structured callback)
- **C4.2 (complete)**: Deterministic seeding
  - Fixed `llama_backend.c` to use configured seed instead of hardcoded 0
  - Added seed=0 → time-based random seed handling in `structured_generation.c`
  - Determinism boundary documented in `structured_generation.h`
  - Test suite: 2/2 tests pass (same seed → identical output over 20 runs, seed=0 → random)
  - Note: Determinism holds for fixed (build, backend, thread count); breaks across backends/thread counts
- **C4.1 (complete)**: OS abstraction layer
  - `platform_thread.h`: Cross-platform threading primitives (mutex, thread, condition variable, static initializer)
  - `platform_time.h`: Cross-platform time primitives (monotonic time, wall clock, sleep, ISO 8601 formatting)
  - `platform_directory.h`: Cross-platform directory operations (open, read, close, create, create_recursive, remove)
  - `platform_fs.h`: Cross-platform file system operations (exists, is_file, is_dir, file_size, delete, rename, copy, absolute_path, path_join, get_extension)
  - `platform_mem.h`: Cross-platform memory operations (aligned allocation, memory mapping, system memory info)
  - Thin static inline wrappers over POSIX pthread and Win32 APIs, zero overhead
  - Updated `model_pool.c` to use new platform_thread.h (removed inline platform-specific code)
  - Test suite: 5/5 tests pass (mutex, time, file system, directory, memory)
- **C3.4 (complete)**: Memory pressure and LRU eviction
  - `ethervox_residency_class_t` enum: RESIDENT (never evicted) vs ON_DEMAND (evictable)
  - `ethervox_model_config_t` extended with `residency` and `ttl_seconds` fields
  - `ethervox_model_pool_set_pressure_callback()`: Register OS memory pressure handler
  - `ethervox_model_pool_evict_lru()`: Explicit LRU eviction with protected roles
  - `ethervox_model_pool_set_max_on_demand()`: Limit concurrent on-demand models
  - LRU tracking: timestamp-based, linear scan eviction
  - Retry-once logic: if OOM, evict and retry load once
  - Max on-demand enforcement: evict LRU before loading new on-demand
  - Test suite: 4/4 tests pass (resident, LRU order, max_on_demand, protected roles)
- **Multimodal media API** (TASK-C3.1, 2026-08-15): Media-agnostic API for vision and audio inputs via llama.cpp's mtmd library
  - `ethervox_media_kind_t`: IMAGE_RGBA8, IMAGE_PNG, IMAGE_JPEG, AUDIO_PCM16
  - `ethervox_media_t`: Container for media data with dimensions/sample rate
  - `ethervox_capabilities_t`: Query model capabilities (vision/audio support)
  - `ethervox_model_capabilities()`: Query what media types a model supports
  - `ethervox_media_prepare()`: Validate and prepare media for encoding
  - `ethervox_media_free()`: Release media resources
  - Model pool now supports optional mmproj loading via `ethervox_model_config_t.mmproj_path`
- **Structured generation with confidence scoring** (TASK-C3.3, 2026-08-15): Log-probability extraction and calibrated confidence metrics for structured JSON generation
  - `ethervox_generate_structured()`: Generate grammar-constrained JSON with confidence score [0.0, 1.0]
  - `ethervox_structured_gen_params_t`: Generation parameters (max_tokens, temperature, top_p, seed, include_logprobs)
  - Confidence calculation excludes grammar-forced structural tokens (only content choices count)
  - `ETHERVOX_EVENT_LOGPROB` fires for each token when enabled (includes token text, logprob, probability)
  - Geometric mean of content token probabilities provides interpretable confidence metric
  - Model handles track mtmd_context for multimodal support
  - Granite-Docling integration test: loads safetensors model directly (no GGUF conversion needed)
  - Documentation: `docs/MULTI_MODEL_CONCURRENT_EXECUTION.md` explains refcounted backend, per-model locking, concurrent execution
  - 6 unit tests covering validation, NULL safety, and all media types ✅
  - Integration test verifies: pool creation, model loading, capability queries, memory budget enforcement ✅
  - All 4 profiles (EDGE, MOBILE, DESKTOP, WORKSPACE) build successfully ✅
  - See `include/ethervox/media.h`, `src/llm/media.c`, `tests/unit/test_media.c`, `tests/integration/test_docling_integration.c`
  - Architecture: One refcounted llama_backend_init() per process, per-model inference_mutex for concurrent execution
  - Ready for concurrent governor + vision model use cases

- **Chat template conformance tests** (TASK-C2.5, 2026-08-15): Golden tests verifying byte-for-byte correctness of chat template formatting for all supported formats
  - Added GRANITE_3 and GRANITE_4 distinction (separate templates for 3.x vs 4.x versions)
  - CHAT_TEMPLATE_CHATML alias for QWEN (same format)
  - CHAT_TEMPLATE_GRANITE alias for GRANITE_4 (backward compatibility)
  - Auto-detection: granite-3/granite_3 → GRANITE_3, granite-4+/unversioned → GRANITE_4
  - 16 golden tests covering GRANITE_3, GRANITE_4, CHATML, LLAMA3: system messages, user messages, assistant start, tool results, multi-turn conversations, stop sequences, tool formats
  - Fixed format functions to return ETHERVOX_SUCCESS (0) instead of snprintf byte count
  - All 4 profiles (EDGE, MOBILE, DESKTOP, WORKSPACE) build successfully ✅
  - See `include/ethervox/chat_template.h`, `src/governor/chat_template.c`, `tests/unit/test_chat_templates.c`

- **Embeddings API** (TASK-C2.4, 2026-08-15): Text → float vector conversion for semantic search with configurable pooling and L2 normalization
  - `ethervox_embed_dimensions()`: Get embedding dimension count from loaded model
  - `ethervox_embed_max_batch()`: Get maximum batch size (context_size / 2)
  - `ethervox_embed_texts()`: Batch embed with pooling strategies (MEAN=1, CLS=2, LAST=3) and optional L2 normalization for cosine similarity
  - Truncates texts >512 tokens at token boundary with warning
  - Cancellation support via `ethervox_cancel_token_t`
  - Uses llama.cpp's `llama_model_n_embd()` and `llama_get_embeddings_seq()` with pooling
  - L2 normalization enables cosine similarity via dot product (unit vectors)
  - 5 unit tests: NULL safety, pooling validation, L2 norm math, cosine similarity, zero vectors
  - All 4 profiles (EDGE, MOBILE, DESKTOP, WORKSPACE) build successfully ✅
  - See `include/ethervox/embeddings.h`, `src/llm/embeddings.c`, `tests/unit/test_embeddings.c`
  - Note: Integration tests with embedding model pending; current tests verify API contracts and mathematical correctness

- **Grammar-constrained decoding - COMPLETE** (TASK-C2.3a/b, 2026-08-14): Full JSON Schema → GBNF converter and generation pipeline integration
  - **C2.3a**: 14/14 JSON Schema types: boolean, integer, number, string (plain/enum/maxLength/pattern), object (required/optional), array (unbounded/maxItems), **oneOf (union types)**, **nested objects**
  - **C2.3b**: Backend integration complete + governor API integration
  - API: `ethervox_grammar_compile()`, `ethervox_grammar_from_json_schema()`, `ethervox_grammar_free()`, `ethervox_grammar_set_lazy_mode()`, `ethervox_grammar_is_lazy()`, `ethervox_grammar_get_trigger_words()`
  - Backend: `ethervox_llm_backend_set_grammar()` - set grammar constraint for next generation
  - Grammar sampler integrated into llama.cpp sampler chain (after penalties, before dist)
  - Lazy mode: grammar only engages after trigger pattern detected (e.g., "```json"), allowing free text explanation before structured output
  - Immediate mode: grammar enforced from first token
  - `ETHERVOX_FINISH_GRAMMAR_DEADLOCK` finish reason when grammar produces empty candidate set
  - 14 golden tests + lazy mode API test, all passing
  - Pure C implementation (no llama.cpp common/, no nlohmann/json dependency)
  - Dependency gate: `nm -g libethervoxai.a | grep nlohmann` → empty ✅
  - All 4 profiles (EDGE, MOBILE, DESKTOP, WORKSPACE) build successfully ✅
  - See `include/ethervox/grammar.h`, `include/ethervox/llm.h`, `src/llm/json_schema_to_gbnf.c`, `src/llm/llama_backend.c`, `tests/unit/test_schema_to_gbnf.c`

- `cmake/EthervoxFeatures.cmake` (TASK-C1.0): `ETHERVOX_FEATURE_HTTP`, `_DOWNLOADER`,
  `_BUG_REPORT`, `_WEATHER`, `_FILE_TOOLS` options, all defaulting `ON` (no behaviour change for
  existing consumers). Selecting any of them `OFF` now excludes the corresponding subsystem from
  the build:
  - `_WEATHER`: excludes `src/plugins/weather_tools/*` and its four runtime registration call
    sites.
  - `_BUG_REPORT`: excludes `src/common/bug_reporter.c` and the `/report` CLI command.
  - `_HTTP`: excludes `src/common/platform_http.c`; `model_downloader.c` falls back to its
    existing "manual download required" path.
  - `_DOWNLOADER`: `ethervox_model_download()` returns the new `ETHERVOX_ERROR_FEATURE_DISABLED`
    before reaching the network path; `src/llm/model_manager.c`'s independent
    `USE_LIBCURL`/`USE_WININET` download path (previously ungated) is composed with this flag too.
  - `_FILE_TOOLS`: excludes `main.c`'s file-tools init/register block.
- **`ETHERVOX_PROFILE`** (TASK-C1.2): Named feature profiles for target platform classes —
  `EDGE|MOBILE|DESKTOP|WORKSPACE`. Setting a profile automatically configures all feature flags:
  - `EDGE` (ESP32): All network/tool features OFF, no llama.cpp (~200KB target)
  - `MOBILE` (iOS/Android/RPi): All features ON (voice line with network)
  - `DESKTOP` (Friend'O'Mine): Same as MOBILE (pool/vision/embeddings future work)
  - `WORKSPACE`: DESKTOP minus all network features (HTTP, downloader, bug reporter, weather,
    built-in file tools) — enforces zero-network guarantee for Workspace shells
  Profile generates `ethervox_features.h` with `ETHERVOX_HAS_*` macros and `ETHERVOX_PROFILE_*`
  definitions. Conformance test (`tests/unit/test_profile_conformance.c`) verifies feature
  composition per profile.
- `scripts/check-no-network-symbols.sh` (TASK-C1.0): scans a static library's undefined symbols
  for `socket`/`connect`/`getaddrinfo`/`gethostbyname`/`curl_*`/`CFNetwork`/`NWConnection`/
  `CFSocket`, failing if any survive. Wired into CI (`workspace-network-symbol-check` job in
  `.github/workflows/build-and-test.yml`), which configures with all five feature flags `OFF`.
- `ETHERVOX_ERROR_FEATURE_DISABLED` and `ETHERVOX_ERROR_NO_TTS_HOST` error codes.
- `ethervox_tts_host_t` (TASK-C1.0, `docs/07-BACKEND-CHANGES.md` §0.4): the TTS-as-a-host-capability
  interface (`include/ethervox/tts_host.h`, `src/tts/tts_host.c`) —
  `speak`/`stop`/`pause`/`resume`/`is_speaking`, `ethervox_tts_style_t`, `ethervox_tts_word_cb`,
  `ethervox_tts_set_host`. A missing host returns `ETHERVOX_ERROR_NO_TTS_HOST`, never crashes.
  `tests/unit/test_tts_host.c` verifies speak/stop/pause/resume/is_speaking/word-callback against
  a fake host. **UPDATED**: Now wired into `voice_conversation.c`'s `conversation_on_speak()` —
  mobile/platform-native TTS path unchanged; desktop Piper path replaced with calls to
  `ethervox_tts_host_speak()` and `ethervox_tts_host_is_speaking()` for barge-in detection.
  Phonemiser/Piper deletion in progress (removed from file system and most CMake references; some
  consumers like `settings_menu.c` still need updating).
- **Finish reason constants** (TASK-C1.1): `ETHERVOX_FINISH_STOP`, `ETHERVOX_FINISH_LENGTH`,
  `ETHERVOX_FINISH_TOOL_CALLS`, `ETHERVOX_FINISH_CONTENT_FILTER`, `ETHERVOX_FINISH_REPETITION`,
  `ETHERVOX_FINISH_EOG` in `include/ethervox/governor.h`. Generation now reports exact stop reason
  (aligned with OpenAI API).
- **Consolidated stop checker** (TASK-C1.1): `governor_should_stop()` helper function in
  `src/governor/governor.c` that checks all stop conditions (EOG, stop sequences, max tokens,
  repetition loops) in one place. Replaces three redundant stop-check locations.
- **Repetition loop detection** (TASK-C1.1): Cycles of ≤8 tokens repeated >3 times now trigger
  `ETHERVOX_FINISH_REPETITION`, preventing infinite loops like "yes yes yes..." hallucinations.
- **Per-model `ignore_eog` property** (TASK-C1.1): Added `bool ignore_eog` to `chat_template_t`
  (`include/ethervox/chat_template.h`). All templates default to `false` (respect model's EOG
  decision). Can be set `true` for creative tasks where models may EOG prematurely. Removes
  Granite-specific EOG special-casing.
- `tests/test_stop_sequences.c` (TASK-C1.1): Conformance test verifying finish reason constants,
  EOG handling, stop sequence detection, and max tokens backstop. 26 assertions covering all 5
  chat templates.
- **`ethervox_paths_t`** (TASK-C1.3): Caller-supplied path configuration API (`include/ethervox/paths.h`,
  `src/common/paths.c`) replacing hardcoded `getenv("HOME")` and `Documents/` derivations. Required for
  macOS App Sandbox, security-scoped vault access, and mobile file-access restrictions. API includes:
  - `ethervox_paths_t` struct: `data_dir`, `cache_dir`, `models_dir`, `temp_dir` (all must be absolute)
  - `ethervox_paths_validate()`: Checks paths are absolute, exist/creatable, have R/W permissions
  - `ethervox_paths_get_default()`: Platform-specific defaults (macOS/Linux/Windows; NOT Android/iOS)
  - `ethervox_path_join()`, `ethervox_ensure_directory()`: Path manipulation helpers
  - `tests/test_paths.c`: 21 assertions covering validation, defaults, joining, directory creation
  - `scripts/check-no-hardcoded-paths.sh`: Grep test warning about hardcoded path usage (45 getenv("HOME"),
    2 "Documents/", 16 "/Library/" instances remain - migration in progress)
- **`ethervox_cancel_token_t`** (TASK-C1.4): Thread-safe cancellation token for long-running operations
  (`include/ethervox/cancel_token.h`, `src/governor/cancel_token.c`). API includes:
  - `ethervox_cancel_token_create()`: Allocate a new token in non-cancelled state
  - `ethervox_cancel_token_cancel()`: Atomically set the cancelled flag (thread-safe)
  - `ethervox_cancel_token_is_cancelled()`: Atomically read the cancelled flag (thread-safe)
  - `ethervox_cancel_token_free()`: Free the token (not thread-safe - caller must synchronize)
  - Threaded through model loading and generation: added optional `ethervox_cancel_token_t*` parameter
    to `ethervox_governor_load_model()`, `ethervox_governor_load_model_with_audio()`,
    `ethervox_governor_execute()`, and `ethervox_governor_execute_with_context()`. All accept NULL
    for backwards compatibility.
  - Checked between token generation steps and in model loading progress callbacks for sub-200ms
    cancellation latency. Session remains usable after cancellation with no leaked state.
  - Uses C11 atomic_bool for lock-free thread-safety (minimal overhead)
  - `tests/test_cancel_token.c`: 8 test cases covering create/free, NULL safety, basic cancellation,
    idempotency, cross-thread cancellation, and concurrent is_cancelled checks. All tests pass.- **Structured event stream** (TASK-C1.5): Unified `ethervox_event_cb` callback interface for tokens,
  tool calls, usage, load stages, log-probs, and errors (`include/ethervox/event_stream.h`,
  `src/common/event_stream.c`). API includes:
  - `ethervox_event_type_t`: 8 event types (LOAD_STAGE, TOKEN, TOOL_CALL_REQUESTED,
    TOOL_CALL_RESULT, USAGE, LOGPROB, FINISHED, ERROR)
  - `ethervox_event_t`: Tagged union carrying event-specific data
  - `ethervox_event_cb`: Callback function signature (returns bool to continue/cancel)
  - **UTF-8 validation**: Token fragments are guaranteed to never split multi-byte sequences. Ported
    UTF-8 validation logic from iOS bridge (`EthervoxBridge.mm`) to C. Handles 1-4 byte sequences
    (ASCII, 2-byte, 3-byte, 4-byte emoji), detects incomplete sequences, validates continuation bytes.
  - `ethervox_validate_utf8()`: Validates UTF-8 string, returns length of valid prefix
  - `ethervox_create_safe_utf8()`: Creates UTF-8 safe copy (truncates incomplete sequences)
  - Threaded through governor execution: added optional `ethervox_event_cb` parameter to
    `ethervox_governor_execute()` and `ethervox_governor_execute_with_context()`. Old
    `token_callback` reimplemented on top of event stream for backwards compatibility.
  - Events delivered in order on the inference thread. String pointers valid only during callback.
  - `tests/test_event_stream.c`: 16 test cases covering UTF-8 validation (NULL, ASCII, emoji,
    incomplete 2/3/4-byte sequences, invalid continuation, mixed content), safe string creation,
    and event structure correctness. All tests pass. Event size: 32 bytes.
- **KV-cache re-keying with hash-based keys** (TASK-C1.6): Multi-model cache support via hash-based
  filenames and eviction API (`include/ethervox/kv_cache_persistence.h` version 2,
  `src/governor/kv_cache_persistence.c`). Cache keys are now computed from:
  - Model file digest (first 1MB + file size — fast but collision-resistant)
  - System prompt hash (token sequence XOR)
  - Context size, quantization type, backend version
  - **Filename format**: `<cache_dir>/kv_cache_<hex_hash>.bin` (64-char hex hash of all components)
  - API changes:
    - `ethervox_kv_cache_get_path()`: Computes hash-based path from governor state
    - `ethervox_kv_cache_save/load()`: Now take `ethervox_paths_t*` and compute path internally
    - `ethervox_kv_cache_exists()`: Updated to use hash-based lookup
    - `ethervox_kv_cache_usage()`: New function returns total bytes, count, oldest/newest timestamps
    - `ethervox_kv_cache_evict_older_than()`: New function deletes caches older than timestamp,
      returns evicted count and bytes freed
  - Hash functions: XOR-based for speed (comment notes SHA-256 would be better but avoids crypto
    dependency). File digest uses simple read of first 1MB for large models.
  - Directory scanning for usage/eviction: Uses `dirent.h` to iterate `kv_cache_*.bin` files,
    extracts size/mtime via `stat()`, accumulates statistics.
  - **Backwards incompatible**: Old cache files (system_prompt_*.kvcache) will not be found by new
    hash-based lookup. One-time cache rebuild on first run after upgrade.
  - **Multi-model support**: Different models can now coexist in the same cache directory without
    filename collisions. Cache mismatches result in silent misses (new cache generated), never
    wrong loads — llama.cpp handles validation.
  - All function signatures updated to use `ethervox_paths_t*` (from C1.3) instead of hardcoded paths.
  - Library compiles successfully (1.3MB); tests not yet added (BACKLOG).
- **Multi-model pool** (TASK-C2.1): Concurrent multi-model loading and execution with memory budget
  enforcement (`include/ethervox/model_pool.h`, `src/llm/model_pool.c`). API includes:
  - `ethervox_model_pool_t`: Pool manages multiple loaded models with a memory budget
  - `ethervox_model_pool_create(paths, budget, &pool)`: Creates pool with memory budget in bytes
    (0 = no limit)
  - `ethervox_model_pool_load(pool, config, progress_cb, ctx, &handle)`: Loads model, returns handle.
    Fails with `ETHERVOX_ERROR_OUT_OF_MEMORY` if budget exceeded. Thread-safe.
  - `ethervox_model_pool_unload(pool, handle)`: Unloads model, frees resources. Thread-safe.
  - `ethervox_model_pool_would_fit(pool, config, &fits, &required)`: Estimates memory requirements
    without loading. Calculation includes model file size + KV cache (context_size × 2048 bytes/token)
    + 128MB overhead. Must be accurate within ±10% to prevent OOM kills.
  - `ethervox_model_pool_memory_usage(pool, &used, &budget)`: Reports current usage across all models
  - **Refcounted backend init**: Single `llama_backend_init()` per process, refcounted across pools.
    First pool creation initializes, last pool destruction cleans up.
  - **Per-model mutex**: `inference_mutex` in `ethervox_model_handle_t` serializes inference on same
    model handle. Different models can run concurrently.
  - Platform-agnostic threading: Uses `pthread_mutex_t` (POSIX) or `CRITICAL_SECTION` (Windows)
  - Designed for Workspace (main LLM + vision + embeddings) and mobile model hot-swap use cases
  - `tests/unit/test_model_pool.c`: 5 test cases covering create/destroy, memory usage, budget
    enforcement, NULL safety, refcounted backend. All tests pass.
  - Library compiles with zero warnings across all 4 profiles (DESKTOP, MOBILE, EDGE, WORKSPACE)
  - Blocks TASK-C2.4 (embeddings), C3.1 (vision), C3.2 (forking), C3.4 (pressure)
- **Host-registered tools** (TASK-C2.2): API for shells/engines to register custom tools with the
  backend (`include/ethervox/host_tools.h`, `src/governor/host_tools.c`). Enforces ADR-0007
  (Preview → Approve → Apply) at the C level via the `is_mutating` refusal rule. API includes:
  - `ethervox_host_tool_t`: Tool descriptor with name, description, JSON Schema, `is_mutating` flag,
    callback function, and user_data
  - `ethervox_host_tool_fn`: Callback signature accepting JSON arguments, returning JSON result or error
  - `ethervox_tool_registry_register_host_tool(registry, tool)`: Registers a host tool, fails with
    `ETHERVOX_ERROR_ALREADY_EXISTS` if name conflicts
  - `ethervox_tool_registry_set_timeout(registry, ms)`: Sets timeout for tool invocations
  - `ethervox_tool_registry_clear_host_tools(registry)`: Removes all host tools (not built-in tools)
  - `ethervox_string_free(str)`: Frees strings allocated by backend (ABI-stable wrapper around free)
  - **`is_mutating=true` enforcement**: Tools marked mutating emit `ETHERVOX_EVENT_TOOL_CALL_REQUESTED`
    (C1.5 event stream) instead of auto-invoking. Attempting to auto-invoke returns
    `ETHERVOX_ERROR_PERMISSION_DENIED`. This makes prompt injection attacks unable to modify user data.
  - **Reentrancy protection**: Placeholder for detecting host tool callbacks that call back into the
    same model (returns `ETHERVOX_ERROR_REENTRANT` - full implementation pending)
  - Host tools stored as linked list in `tool_manifest_registry_t.host_tools` field
  - Integration with existing tool manifest system: host tools and built-in tools appear in same manifest
  - New error codes: `ETHERVOX_ERROR_ALREADY_EXISTS`, `ETHERVOX_ERROR_PERMISSION_DENIED`,
    `ETHERVOX_ERROR_INVALID_STATE`, `ETHERVOX_ERROR_REENTRANT`
  - `tests/unit/test_host_tools.c`: 10 test cases covering registration, duplicate detection, timeout,
    NULL safety, invalid fields, mutating flag, refusal enforcement, invocation (success/error), string
    freeing. All tests pass.
  - Library compiles with zero warnings across all 4 profiles (DESKTOP, MOBILE, EDGE, WORKSPACE)
  - Unblocks TASK-E6.2 (engine tool registration for search_vault, read_document, create_plan, apply_plan)

### Fixed
- **Stop-sequence infinite loop** (TASK-C1.1): Fixed the bug where sampled tokens were fed into the
  KV cache via `llama_decode()` BEFORE stop-sequence checks, causing the model to see its own stop
  markers and repeat them infinitely. `governor_should_stop()` is now called BEFORE
  `llama_decode()`, ensuring stop tokens never enter the context. Stop sequences are excluded from
  the final output, and generation terminates at exactly the right token.
- **BACKLOG-15 complete**: All 16 remaining test files in `tests/unit/` now use `CHECK()` macro
  instead of `assert()`. Created shared `test_utils.h` header providing `CHECK()` macro definition
  that works regardless of NDEBUG setting. Replaced 449 `assert()` calls across
  `test_audio_core.c`, `test_audio_integration.c`, `test_config.c`, `test_context_overflow.c`,
  `test_device_profile.c`, `test_error.c`, `test_file_tools.c`, `test_gguf_config.c`,
  `test_memory_tools.c`, `test_mobile_optimization.c`, `test_plugin_manager.c`,
  `test_schema_to_gbnf.c`, `test_secret_mode.c`, `test_voice_training.c`, and `test_wake_word.c`.
  Note: `test_media.c` uses custom `ASSERT` macro that already evaluates unconditionally (not
  affected by NDEBUG), so unchanged. All tests build and run successfully with `CHECK()` macro.
  Originally fixed `test_tts_host.c` and `test_voice_conversation.c` in C1.0; remaining files
  completed 2025-01-12. Tests now properly validate conditions even when built in Release mode with
  `-DNDEBUG`.
- `tests/unit/test_tts_host.c` and `tests/unit/test_voice_conversation.c`: replaced `assert()`
  with an explicit `CHECK()` macro — this test suite builds with `-DNDEBUG` (Release), which
  turns `assert()` into a silent no-op, so every check in these two files was passing vacuously
  regardless of correctness (see BACKLOG-15 entry above for full fix).
  Fixing this in `test_voice_conversation.c` (the barge-in test coverage) surfaced two real,
  previously-masked issues: a test-authoring bug in the grace-period exact-boundary case (the
  detector itself was correct — `ethervox_barge_in_detector_process`'s documented `>=` semantics),
  and a gap where `ethervox_conversation_init`/`start`'s background thread never reaches
  `ETHERVOX_CONV_STATE_ERROR` when local STT/TTS model files are absent (stays at
  `UNINITIALIZED` indefinitely instead) — worked around in the test (`SKIP` instead of fail), the
  underlying `voice_conversation.c` gap itself is unfixed.

### Changed
- All CC BY-NC-SA 4.0 licence references in `src/` and `include/` (132 files: SPDX identifiers,
  "Licensed under…" lines) rewritten to reference the proprietary licence, authorized by
  `LICENSE`'s "Copyright Holder Reservation of Rights" clause.
- **Piper TTS backend and phonemizer** (TASK-C1.0): Removed `src/tts/phonemizer/` directory (25 files),
  `src/tts/piper_backend.c`, and `src/tts/tts.c` (old TTS API). TTS is now handled via the
  `ethervox_tts_host_t` platform callback interface. ONNX Runtime and espeak-ng dependencies removed.
  CMakeLists.txt cleaned: removed phonemizer source references, ONNX/Piper linking, espeak dictionary
  embedding logic (5 variant checks + `ENABLE_ESPEAK_DICT` option), MSVC `/bigobj` workaround.
  Platforms must now register a TTS host via `ethervox_tts_set_host()` before speaking.
  - `voice_conversation.c`: Fully rewired to use `ethervox_tts_host_speak()` and
    `ethervox_tts_host_is_speaking()`; removed Piper initialization; barge-in detection now polls
    host interface. `ethervox_conversation_get_phonemizer()` and `ethervox_conversation_get_tts()`
    return NULL (API compat stubs).
  - `settings_menu.c`: Voice testing functions stubbed with "no longer available" message;
    pronunciation reset action disabled.
  - `language_detector.c`: TTS reload calls removed; language switching now logs platform-managed TTS.
  - Excluded from build (depend on removed phonemizer): `voice_training.c`, `global_tts.c`,
    `train_pronunciation.c`.
  - Standalone CLI app (`main.c`): Still references old TTS API (not part of library build).
- **`include/ethervox/file_tools.h`** (TASK-C1.2): Added compile-time assertion that
  `ETHERVOX_FILE_ACCESS_READ_WRITE` cannot be selected under `ETHERVOX_PROFILE=WORKSPACE` (ADR-0007,
  AGENTS.md I2: no model output can mutate the file system in Workspace shells).

### Removed
- `src/tmp.txt` (tracked debug-log dump); `main.c.bak`/`tmp.txt` added to `.gitignore`.
