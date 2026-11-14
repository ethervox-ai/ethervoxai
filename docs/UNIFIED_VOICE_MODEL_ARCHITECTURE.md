# Unified Voice Model Architecture

**Status:** Implemented and wired into `voice_conversation.c` (Mode 1). `ethervox_governor_load_model_with_audio()` / `ethervox_governor_transcribe_audio()` / `ethervox_governor_has_audio_support()` exist in `governor.c` exactly as specified below (seq 0/1/2 layout, ASR confined to seq 2). This header is stale where it says "not yet implemented" — kept for the design rationale, which still applies.

**Audience:** The engineer/agent implementing this in `ethervoxai` native
core, and the parallel Android/JNI session integrating against the new API
once it lands.

**Branch context:** This design was authored on
`agents/unified-voice-model-architecture`, rebased onto
`origin/agents/granite-speech-architecture` (tip `da849a7`) so that Granite
Speech Plus / mtmd support is present (`src/stt/granite_speech_backend.c`,
`ETHERVOX_STT_BACKEND_GRANITE_SPEECH{,_PLUS}`). That work is a prerequisite
and is assumed complete. **Do not implement on top of `main` directly** -
rebase onto that branch (or its successor once merged) first.

---

## 1. Problem statement

Product requirement: **only one `llama_model`/`llama_context` instance may
be resident in memory at a time** - never the Governor (chat/tool-calling
LLM) and a Granite Speech instance loaded simultaneously. The Android app
has two screens:

1. **Text screen** (default): Governor loaded, normal chat/tool-calling
   loop.
2. **Voice screen**: mic → VAD → speech decoded → response generated →
   response spoken via TTS - a full conversational loop, not just
   transcription (this is "Mode 1", `src/dialogue/voice_conversation.c`).

Switching Text → Voice must unload the Governor and load "the voice
capability"; switching back must do the reverse. Never both loaded at once.

**The catch:** today, Mode 1's mic→response→speech turn needs the model to
do two different jobs:

- **ASR decode**: audio in, transcript out
  (`ethervox_stt_granite_speech_*` in `src/stt/granite_speech_backend.c`,
  via `mtmd`)
- **Chat/tool generation**: transcript in as a user turn, response out (the
  Governor's normal `llama_context` decode loop in
  `src/governor/governor.c`)

These are currently **two entirely separate `llama_model`/`llama_context`
instances**, even when pointed at the identical GGUF
(`granite-speech-4.1-2b-plus.Q4_K_M.gguf` + its mmproj):

- `ethervox_conversation_init()` (`src/dialogue/voice_conversation.c`) takes
  an **already-loaded Governor** (`ethervox_governor_t*`) - it does not load
  it.
- `conversation_thread()` (same file, around its `ethervox_stt_init()` call)
  **also** lazily initializes its own, completely separate STT
  runtime/context with `stt_config.backend = ETHERVOX_STT_BACKEND_GRANITE_SPEECH`.
- The Governor's `llama_context` has zero audio-decode capability - no
  `mtmd_context` attached at all. It only ever receives plain text.

Per IBM's model card (see `granite_speech_backend.c`'s file header), Granite
Speech Plus supports ASR-only, speaker-attributed ASR, and (unused here)
word-timestamp behaviors purely by which fixed prompt string is sent to it -
no separate weights needed. This is what makes unification possible: ASR
decode does not require its own `llama_model`, only its own careful place in
the KV cache of a context that also serves chat generation.

---

## 2. Design overview

Extend `ethervox_governor_t` itself (do **not** create a second
Governor-like struct) with an optional `mtmd_context*` for audio decode,
gated so it has zero effect on existing text-only Governor use (CLI,
existing Android chat flow, all current tests). One loaded
`llama_model`/`llama_context` pair then serves:

- **Chat/tool generation** (existing, unchanged codepath) on **sequence 0**
- **System prompt master** (existing, unchanged) on **sequence 1**
- **ASR decode** (new) on a **new, dedicated sequence 2**

This requires bumping `ctx_params.n_seq_max` from 2 to 3 when the Governor
is loaded in audio-capable mode, and extracting the Granite Speech
prompt-build/tokenize/greedy-decode loop out of
`granite_speech_backend.c`'s `ethervox_stt_granite_speech_finalize()` into a
shared helper both the standalone STT backend (Mode 2, unchanged, its own
private context, `seq_id=0`) and the new Governor audio path (`seq_id=2`)
call.

### 2.1 Why sequence 2, not sequence 1

Sequence 1 is **already ambiguously reused** in the existing codebase:
`src/governor/governor.c` documents seq 1 as the permanent system-prompt
master (populated once, read via `llama_memory_seq_cp(mem, 1, 0, 0, -1)` to
refresh seq 0 after a conversation clear - see governor.c around line
3045-3061). But `src/governor/conversation_summary.c`'s
`generate_summary_with_llm()` **also** uses `temp_seq_id = 1` as scratch
space for one-shot summary generation, then calls
`llama_memory_seq_rm(mem, 1, -1, -1)` to wipe it clean afterward (see
`conversation_summary.c` lines ~314-410). This is a **pre-existing latent
bug**: if summary generation and the system-prompt-master use of seq 1 are
ever interleaved, the `llama_memory_seq_rm` wipes the system prompt master
sequence. This is *out of scope to fix* here (unrelated pre-existing issue,
flag it but do not fix as part of this task, unless it turns out to
directly interfere with the new seq 2 addition), but it is exactly why ASR
must get its own, never-shared sequence rather than trying to reuse 1: seq 1
already has one undocumented extra tenant.

### 2.2 Sequence layout after this change (audio-capable Governor only)

| Seq | Purpose | Lifetime | Existing/New |
|-----|---------|----------|---------------|
| 0 | Conversation (system prompt copy + turns) | Cleared/rebuilt per context-management cycle | Existing |
| 1 | System prompt master (permanent) + conversation-summary scratch (pre-existing overlap bug, not addressed here) | Permanent | Existing |
| 2 | **ASR decode scratch** (new) | Cleared via `llama_memory_seq_rm(mem, 2, -1, -1)` at the start of every `ethervox_governor_transcribe_audio()` call; never copied to/from seq 0/1 | **New** |

Text-only Governor use (`mtmd_ctx == NULL`) keeps `n_seq_max = 2` exactly as
today - **no behavior change** for existing callers.

---

## 3. API changes

### 3.1 `include/ethervox/governor.h`

Add near the existing `ethervox_governor_load_model` declaration:

```c
/**
 * Load the Governor model with audio (Granite Speech) decode support
 * attached, for the unified voice-model architecture (Mode 1 voice screen).
 *
 * Identical to ethervox_governor_load_model() except:
 *  - Also loads mmproj_path via mtmd_init_from_file() and attaches the
 *    resulting mtmd_context to the Governor, enabling
 *    ethervox_governor_transcribe_audio().
 *  - Uses ctx_params.n_seq_max = 3 instead of 2 (adds a dedicated ASR
 *    scratch sequence - see docs/UNIFIED_VOICE_MODEL_ARCHITECTURE.md
 *    section 2.2). Context size budget must therefore be planned per
 *    3-way split, not the existing 2-way split - see section 5.2 below for
 *    the concrete minimum-context-size revision needed.
 *  - model_path and mmproj_path must point at the SAME Granite Speech Plus
 *    GGUF pair used to load Granite Speech elsewhere (Mode 1/2/4) - this is
 *    a hard requirement, not validated at runtime beyond mtmd's own
 *    mtmd_support_audio() check.
 *
 * @param governor Governor instance
 * @param model_path Path to Granite Speech Plus GGUF (LLM decoder + Conformer weights)
 * @param mmproj_path Path to companion mmproj GGUF (QFormer audio projector)
 * @param cache_dir Directory to store/load KV cache (can be NULL to disable caching)
 * @param progress_callback Optional progress callback (can be NULL)
 * @param user_data User data for progress callback (can be NULL)
 * @return ETHERVOX_SUCCESS on success, negative error code otherwise
 */
ethervox_result_t ethervox_governor_load_model_with_audio(
    ethervox_governor_t* governor,
    const char* model_path,
    const char* mmproj_path,
    const char* cache_dir,
    ethervox_load_progress_callback progress_callback,
    void* user_data);

/**
 * Transcribe one utterance's worth of accumulated audio using the
 * Governor's own loaded model (must have been loaded via
 * ethervox_governor_load_model_with_audio()). Runs entirely on the
 * dedicated ASR scratch sequence (seq 2) - never touches the conversation
 * (seq 0) or system-prompt-master (seq 1) sequences, so this can be called
 * at any point without corrupting Governor chat state, including mid
 * multi-turn conversation.
 *
 * Internally: clears seq 2, builds the fixed ASR-only prompt (see
 * granite_speech_decode.h), tokenizes prompt+audio via mtmd, evaluates via
 * mtmd_helper_eval_chunks(..., seq_id=2, ...), then greedily decodes the
 * transcript token-by-token, all confined to seq 2.
 *
 * @param governor Governor instance (must have been loaded with
 *   ethervox_governor_load_model_with_audio - returns
 *   ETHERVOX_ERROR_NOT_INITIALIZED otherwise)
 * @param samples Raw float32 PCM audio samples, 16kHz mono (Granite
 *   Speech's fixed encoder rate)
 * @param n_samples Number of samples in `samples`
 * @param out_text Output: malloc'd transcript string (caller must free),
 *   set to NULL on failure
 * @return ETHERVOX_SUCCESS on success, negative error code otherwise
 */
ethervox_result_t ethervox_governor_transcribe_audio(
    ethervox_governor_t* governor,
    const float* samples,
    uint32_t n_samples,
    char** out_text);

/**
 * @return true if this Governor instance was loaded via
 *   ethervox_governor_load_model_with_audio() and still has its mtmd
 *   context attached (i.e. ethervox_governor_transcribe_audio() is usable).
 */
bool ethervox_governor_has_audio_support(ethervox_governor_t* governor);
```

### 3.2 `struct ethervox_governor` (governor.c, internal)

Add, inside the existing `#if defined(ETHERVOX_WITH_LLAMA) && LLAMA_HEADER_AVAILABLE` block:

```c
  mtmd_context* mtmd_ctx;        // NULL unless loaded via
                                 // ethervox_governor_load_model_with_audio()
  int audio_sample_rate;         // From mtmd_get_audio_sample_rate(), only
                                 // meaningful when mtmd_ctx != NULL
  char* mmproj_path;             // Saved alongside model_path so
                                 // ethervox_governor_reload_model() can
                                 // re-attach mtmd on reload (NULL for
                                 // text-only Governor instances)
```

`ethervox_governor_unload_model()` must additionally free `mtmd_ctx` (via
`mtmd_free()`) and NULL it out, alongside its existing `llm_ctx`/`llm_model`
teardown, but **keep** `mmproj_path` (same pattern as `model_path`) so
`ethervox_governor_reload_model()` can restore audio support.
`ethervox_governor_reload_model()` must call
`ethervox_governor_load_model_with_audio()` instead of `load_model()` when
`governor->mmproj_path != NULL`.

### 3.3 Shared ASR-decode helper (new files)

Extract the prompt-build + `mtmd_tokenize` + greedy-decode-loop body
currently inside `ethervox_stt_granite_speech_finalize()`
(`src/stt/granite_speech_backend.c` lines ~240-378) into:

- `include/ethervox/granite_speech_decode.h`
- `src/stt/granite_speech_decode.c`

Proposed signature (parameterized on context/seq rather than assuming a
private `granite_speech_context_t`):

```c
/**
 * Shared Granite Speech ASR/SAA decode routine: builds the fixed ASR or SAA
 * prompt, tokenizes prompt+audio via mtmd, evaluates into the given
 * llama_context at the given sequence, then greedily decodes the transcript.
 * Used by both:
 *  - src/stt/granite_speech_backend.c (Mode 2 standalone context, seq_id=0)
 *  - governor.c's ethervox_governor_transcribe_audio() (shared Governor
 *    context, seq_id=2)
 *
 * Does NOT clear the target sequence itself - callers must do that first
 * (via llama_memory_seq_rm or llama_memory_clear as appropriate for their
 * context's sequence layout) since the right way to clear differs between
 * a single-sequence standalone context (Mode 2) and a multi-sequence shared
 * context (Governor).
 *
 * @param model llama_model (vocab source)
 * @param ctx llama_context to decode into
 * @param mctx mtmd_context (must support audio - see mtmd_support_audio())
 * @param tmpl Chat template (CHAT_TEMPLATE_GRANITE - shared format)
 * @param seq_id Target sequence for prompt+audio tokens and generation
 * @param samples Raw float32 PCM audio, 16kHz mono
 * @param n_samples Sample count
 * @param is_saa true for Speaker-Attributed ASA prompt (Mode 2/PLUS), false
 *   for plain ASR-only prompt (Mode 1 unified voice / Mode 2 BASE fallback
 *   n/a - Mode 1 always false)
 * @param prefix_text Optional incremental-decoding carry-over (Mode 2 only,
 *   NULL for Mode 1)
 * @param max_tokens Cap on generated transcript tokens (0 = backend default)
 * @param out_text Output: malloc'd transcript (caller must free)
 * @return ETHERVOX_SUCCESS or error code
 */
ethervox_result_t granite_speech_decode(
    struct llama_model* model,
    struct llama_context* ctx,
    mtmd_context* mctx,
    const chat_template_t* tmpl,
    llama_seq_id seq_id,
    const float* samples,
    uint32_t n_samples,
    bool is_saa,
    const char* prefix_text,
    uint32_t max_tokens,
    char** out_text);
```

`granite_speech_backend.c`'s `ethervox_stt_granite_speech_finalize()`
becomes a thin wrapper: clear seq 0 of its own private context (unchanged
- it already does this in `ethervox_stt_granite_speech_start()`, actually
review whether that clear needs to move into `finalize()` or stays in
`start()` - **keep current behavior**, only replace the tokenize/decode
body with a call to `granite_speech_decode(..., seq_id=0, ...)`), populate
`ethervox_stt_result_t` from the returned text.

`governor.c`'s new `ethervox_governor_transcribe_audio()` becomes:

```c
ethervox_result_t ethervox_governor_transcribe_audio(
    ethervox_governor_t* governor, const float* samples,
    uint32_t n_samples, char** out_text) {
  if (!governor || !samples || n_samples == 0 || !out_text)
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  if (!governor->mtmd_ctx)
    return ETHERVOX_ERROR_NOT_INITIALIZED;

  llama_memory_t mem = llama_get_memory(governor->llm_ctx);
  llama_memory_seq_rm(mem, /*seq_id=*/2, -1, -1);  // Clear ASR scratch only

  return granite_speech_decode(
      governor->llm_model, governor->llm_ctx, governor->mtmd_ctx,
      governor->chat_template, /*seq_id=*/2,
      samples, n_samples, /*is_saa=*/false, /*prefix_text=*/NULL,
      /*max_tokens=*/0, out_text);
}
```

This is the concrete mechanism that fulfills "ASR decode and chat generation
must not corrupt each other's state": every call clears exactly seq 2
first, decodes entirely within seq 2, and never touches seq 0/1.

---

## 4. `voice_conversation.c` (Mode 1) rewiring

### 4.1 What changes

Today (`src/dialogue/voice_conversation.c`, `conversation_thread()`, lines
~916-957): the thread lazily initializes its own
`ethervox_stt_runtime_t session->stt_runtime` via `ethervox_stt_init()` with
`ETHERVOX_STT_BACKEND_GRANITE_SPEECH`, entirely independent of
`session->governor`.

**New behavior:** when `ethervox_governor_has_audio_support(session->governor)`
is true, skip STT-runtime init entirely. The existing
`capture_utterance_with_vad()` call (line 1045) currently drives
`ethervox_stt_start/process/finalize/stop` internally - it needs a variant
(or an internal branch) that instead:
1. Accumulates raw audio samples locally (the RMS-energy VAD logic is
   unchanged - only the "what do I do with the accumulated audio" tail
   changes)
2. On utterance end, calls `ethervox_governor_transcribe_audio(session->governor, ...)`
   instead of `ethervox_stt_finalize()`.

When `ethervox_governor_has_audio_support()` is false (Governor loaded
text-only, or this build predates the unified path), **keep the existing
STT-runtime code path unchanged** - this is what preserves backward
compatibility for any caller that still loads a plain Governor + separate
STT, and is also what the macOS CLI / any non-migrated caller keeps using
until it's updated.

`struct ethervox_conversation_session` (`voice_conversation.c` line ~69)
keeps its `ethervox_stt_runtime_t stt_runtime` field for that fallback path
- do not remove it, just make it conditionally unused.

### 4.2 What stays exactly the same

- `ethervox_governor_execute_with_context()` call (line ~1114) - completely
  unchanged. Tool-calling, `speak`/`listen` callbacks, barge-in monitor
  (`start_barge_in_monitor`), state machine (IDLE/LISTENING/PROCESSING/
  SPEAKING) - none of this needs to change, since from Governor's
  perspective the transcript text arrives the same way regardless of which
  path produced it.
- `on_speak_request`/TTS path - completely unchanged.
- Mode 2 (`src/plugins/voice_tools/voice_tools.c`) - untouched, keeps its
  own separate one-shot Granite Speech Plus (SAA) context exactly as today.
  Lower-priority follow-up (not required for this task): make Mode 2 also
  check "is anything else loaded, unload it first" if the product wants the
  single-model rule to extend there too.

### 4.3 Thread-safety / lifecycle note

`ethervox_governor_unload_model()`/`ethervox_governor_load_model_with_audio()`
(screen-switch triggers, called from the Android JNI layer in the parallel
session) must not race with `conversation_thread()`'s state machine. Two
options, pick based on how the Android side sequences screen transitions
(coordinate with the parallel Android session):

- **Option A (simpler, recommended default):** require the caller to fully
  `ethervox_conversation_stop()` (which joins the background thread - verify
  this in `voice_conversation.c`'s stop implementation) *before* unloading
  the Governor / switching model instances, and `ethervox_conversation_start()`
  again only after the new instance is loaded. This matches "switch
  screens" as a discrete, sequenced operation rather than something that
  needs to be safe concurrently with an in-flight turn.
- **Option B (more resilient, more work):** add a mutex/flag Governor-side
  so `ethervox_governor_unload_model()` blocks until any in-flight
  `ethervox_governor_execute_with_context()` or
  `ethervox_governor_transcribe_audio()` call on that instance completes.
  Only pursue this if the Android team's screen-switch UX can't tolerate
  the "stop conversation session first" sequencing of Option A.

Document whichever is chosen clearly in this file's own comments once
implemented, since the parallel Android session needs to know which
contract it's coding against.

---

## 5. KV cache persistence changes

### 5.1 The reload bug (fix this - likely root cause of "trouble loading the file")

`ethervox_governor_reload_model()` (governor.c line ~2264-2283):

```c
ethervox_result_t ethervox_governor_reload_model(ethervox_governor_t* governor) {
  ...
  return ethervox_governor_load_model(governor, governor->model_path, NULL, NULL, NULL);
  //                                                                    ^^^^ cache_dir hardcoded NULL
}
```

This **always** passes `cache_dir = NULL`, meaning `ethervox_governor_load_model()`'s
entire KV-cache-check branch (`if (cache_dir) { ... ethervox_kv_cache_exists/load ... }`,
lines ~1752-1799) is **unconditionally skipped on every reload** - the
system prompt is always regenerated from scratch (slow: "~5 minutes" per
`GOV_LOG` at line 1819) instead of loaded from the cache file that was
saved on first load. This is almost certainly the Android team's "had
trouble loading the file" symptom (they likely only ever see the slow path
and assume the cache load is broken, when actually it's never attempted).

**Fix:** store the `cache_dir` used at first load on the governor struct
(new field `char* cache_dir_saved` alongside `model_path`), set it in
`ethervox_governor_load_model()` (and the new
`ethervox_governor_load_model_with_audio()`), free/reset it in
`ethervox_governor_unload_model()` analogous to `model_path`'s handling, and
have `ethervox_governor_reload_model()` pass `governor->cache_dir_saved`
instead of `NULL`:

```c
ethervox_result_t ethervox_governor_reload_model(ethervox_governor_t* governor) {
  ...
  if (governor->mmproj_path) {
    return ethervox_governor_load_model_with_audio(
        governor, governor->model_path, governor->mmproj_path,
        governor->cache_dir_saved, NULL, NULL);
  }
  return ethervox_governor_load_model(governor, governor->model_path,
                                       governor->cache_dir_saved, NULL, NULL);
}
```

Before implementing, **reproduce the reported symptom first** (per the
task's instructions): load a Governor with a valid `cache_dir`, confirm a
`.kvcache` file is written (`ethervox_kv_cache_save` success log), call
`ethervox_governor_unload_model()` then `ethervox_governor_reload_model()`,
and confirm today's build does in fact skip the cache (regenerates from
scratch, ~5 min) - this validates the root-cause theory above before
patching it. If reproduction reveals a *different* failure (e.g. the actual
`llama_state_seq_load_file()` call failing even when reached, a checksum
mismatch, a directory-permissions issue on Android specifically), adjust
the fix accordingly - the `cache_dir` bug above is the most likely and
most obviously wrong code found during this investigation, but confirm
before assuming it's the *only* issue.

### 5.2 Context-size budget under 3-way sequence split

`ethervox_governor_load_model()` currently enforces (governor.c line
~1690-1698):

```c
const uint32_t MIN_CONTEXT_FOR_SYSTEM_PROMPT = 16384;  // for n_seq_max=2
```

with the comment "n_ctx_seq = n_ctx / n_seq_max". Under
`ethervox_governor_load_model_with_audio()`'s `n_seq_max = 3`, the same
16384 total budget now yields ~5461 tokens/sequence instead of ~8192 -
tight for a ~4100-token system prompt (per the existing comment at line
580: "16384/3=5461 per seq, needed for 4101-token system prompt" - this
comment already anticipates n_seq_max=3, suggesting a previous author
partially planned for this). Recommend a **separate, higher minimum for the
audio-capable path**, e.g.:

```c
const uint32_t MIN_CONTEXT_FOR_AUDIO_MODE = 24576;  // n_seq_max=3: 8192/seq
```

Verify against the actual system prompt token count in the target build
(log via `GOV_LOG` as the existing code already does) rather than trusting
the comment's numbers blindly - tool count/manifest mode affects this (see
`ethervox_governor_system_prompt_mode_t`).

Seq 2 (ASR scratch) itself needs enough per-sequence capacity for one
utterance's audio-embedding tokens + prompt + generated transcript - the
existing standalone Granite Speech context in `granite_speech_backend.c`
uses `ctx_params.n_ctx = 4096` for this same job with its own private
context (i.e. all 4096 belongs to ASR alone there). With a shared 3-way
split as above (8192/seq at 24576 total), seq 2 gets 8192 - comfortably
more than the standalone path's 4096, so this should be safe, but validate
empirically once implemented (long utterances, worst-case audio length).

### 5.3 Extend save/load to also persist seq 0 (conversation continuity)

Current `ethervox_kv_cache_save()`/`load()`
(`src/governor/kv_cache_persistence.c`) always operate on **sequence 1**
only (`llama_state_seq_save_file(ctx, cache_path, 1, ...)` /
`llama_state_seq_load_file(ctx, cache_path, 1, ...)`) - i.e. only the
system-prompt master. To preserve conversation continuity across a
Text↔Voice screen switch (unload → reload) without losing turns or
reprocessing, extend this to *optionally* also snapshot/restore seq 0:

**Recommended approach (additive, not a forked file):**

1. Add a new pair of functions (do not change existing signatures used
   elsewhere - `ethervox_kv_cache_save`/`load` are called from `governor.c`
   in 6+ places and `conversation_summary.c`; changing their signature
   would ripple everywhere for no benefit to those call sites, which only
   ever want the seq-1 system-prompt-master behavior):

   ```c
   // New, in kv_cache_persistence.h/.c - separate cache file, separate
   // sequence, so a missing/corrupt conversation cache never blocks the
   // system-prompt cache load path (independent failure domains).
   ethervox_result_t ethervox_kv_cache_save_conversation(
       struct ethervox_governor* governor, const char* cache_path);
   ethervox_result_t ethervox_kv_cache_load_conversation(
       struct ethervox_governor* governor, const char* cache_path);
   ```

   Internally these call `llama_state_seq_save_file(ctx, cache_path, /*seq_id=*/0, ...)`
   / `llama_state_seq_load_file(ctx, cache_path, /*seq_id=*/0, ...)` -
   mirroring the existing seq-1 functions almost exactly (expect to share
   most of the fsync/verification boilerplate via a small internal static
   helper rather than copy-pasting the ~170-line body twice).

2. Wire these into `ethervox_governor_unload_model()` (save seq 0, if
   `current_kv_pos > system_prompt_token_count` i.e. there's an actual
   conversation beyond the system prompt worth preserving) and into
   `ethervox_governor_load_model_with_audio()`/`load_model()`'s cache-check
   branch (load seq 0 *after* seq 1 loads successfully, only if a
   conversation cache file exists - treat its absence as normal/expected,
   not an error, since first-ever load has no prior conversation).

3. Cache path convention: reuse `ethervox_kv_cache_get_path()`'s pattern but
   with a distinct suffix, e.g.
   `<cache_dir>/cache/conversation_<model>.kvcache` (vs. existing
   `system_prompt_<model>.kvcache`), so the two never collide and can be
   independently invalidated (e.g. clearing conversation history should
   delete only the conversation cache file, not the system-prompt one).

4. **Turn counter / conversation_history_t**: `ethervox_governor_unload_model()`
   today unconditionally calls `cleanup_conversation_history()` +
   `init_conversation_history()` (governor.c lines ~2250-2253), wiping the
   in-memory turn tracking regardless of whether KV state was preserved.
   If seq 0 is preserved across unload/reload, `conversation_history_t`
   (turn metadata used for context-window management, summarization
   triggers, etc.) needs to survive too, or Governor's context-health
   tracking (`context_manager_state_t`) will disagree with what's actually
   in the KV cache after reload. Options: (a) persist
   `conversation_history_t` alongside the KV cache blob (simplest: add it
   to the same file format, or a third small sidecar file), or (b) skip
   `cleanup_conversation_history()`/`init_conversation_history()` when a
   conversation-cache save succeeded, leaving the in-memory struct
   untouched across the unload/reload boundary (only correct if the
   Governor object itself isn't destroyed and recreated across the screen
   switch - confirm this is true for the Android integration plan, since
   if the JNI layer calls `ethervox_governor_cleanup()` + re-`init()`
   instead of just unload/reload, this in-memory state is lost regardless
   of what's on disk). **Flag this specifically for the Android-integration
   session to confirm**, since it directly depends on how they've built
   the screen-switch call sequence.

---

## 6. Files to create/change (summary)

| File | Change |
|------|--------|
| `include/ethervox/granite_speech_decode.h` | **New.** Shared ASR-decode helper declaration (section 3.3) |
| `src/stt/granite_speech_decode.c` | **New.** Shared ASR-decode helper implementation, extracted from `granite_speech_backend.c` |
| `src/stt/granite_speech_backend.c` | Replace `ethervox_stt_granite_speech_finalize()`'s inline tokenize/decode body with a call to `granite_speech_decode(..., seq_id=0, ...)` |
| `include/ethervox/governor.h` | Add `ethervox_governor_load_model_with_audio()`, `ethervox_governor_transcribe_audio()`, `ethervox_governor_has_audio_support()` declarations |
| `src/governor/governor.c` | Add `mtmd_ctx`/`audio_sample_rate`/`mmproj_path`/`cache_dir_saved` fields to `struct ethervox_governor`; implement the 3 new functions; fix `ethervox_governor_reload_model()`'s `cache_dir` bug; extend `ethervox_governor_unload_model()` to free `mtmd_ctx` and optionally save conversation KV; extend context-size minimum for audio mode (section 5.2) |
| `include/ethervox/kv_cache_persistence.h` | Add `ethervox_kv_cache_save_conversation()`/`load_conversation()` declarations |
| `src/governor/kv_cache_persistence.c` | Implement the above, sharing fsync/verification logic with the existing seq-1 functions via a small internal helper |
| `src/dialogue/voice_conversation.c` | Branch on `ethervox_governor_has_audio_support()`: skip STT-runtime init/use and call `ethervox_governor_transcribe_audio()` instead, when true; keep existing STT-runtime path when false |
| `tests/test_governor_helpers.c`, `tests/test_governor_streaming.c` | Add coverage for the new load/transcribe/unload/reload audio-mode path (mock or skip actual model loading as existing tests do - review current patterns first) |
| `tests/unit/test_voice_conversation.c` | Add coverage for the new branch in `conversation_thread()`/`capture_utterance_with_vad()` (likely needs a fake/mock Governor with audio support flagged, matching existing test patterns in this file) |
| `docs/UNIFIED_VOICE_MODEL_ARCHITECTURE.md` | This document - keep updated as implementation reveals adjustments |

---

## 7. Explicitly out of scope (per task instructions)

- **Android/JNI/Kotlin** (`src/platform/ethervox_android_core.c`, etc.) -
  handled by a parallel Android-focused session. This document is what that
  session should integrate against once the native API above lands. Key
  things that session will need from here:
  - Call `ethervox_governor_load_model_with_audio()` (voice screen enter)
    vs. `ethervox_governor_load_model()` (text screen enter) instead of
    always using the latter.
  - Call `ethervox_governor_unload_model()` before switching screens either
    direction (see section 4.3 - confirm which lifecycle option is
    implemented).
  - No changes needed to `ethervox_conversation_*` calls themselves -
    `voice_conversation.c`'s new internal branch is transparent to callers.
- **Mode 2 unification** (`src/plugins/voice_tools/voice_tools.c`,
  speaker-attributed transcription-only) - stays exactly as-is, its own
  separate one-shot context. Lower-priority future follow-up: make it also
  respect "unload whatever else is loaded first" for the strict
  single-model rule; not required now.
- Fixing the pre-existing `conversation_summary.c` seq-1 double-use bug
  (section 2.1) - flagged, not fixed, unless it turns out to actively
  interfere with the new seq 2 addition during implementation (it
  shouldn't, since they use different sequence numbers).
- Fixing the pre-existing macOS/iOS build break (see section 8) - flagged
  for awareness only, since it currently blocks any linked-executable
  validation on macOS in this sandbox.

---

## 8. Validation blockers encountered during design (for whoever implements)

Two environment/build issues were found while preparing this document,
**neither caused by this design** - both pre-date it and should be
mentioned to whoever picks up implementation, since they'll hit the same
wall:

1. **No Android NDK available in this sandbox.** Could not do a real
   Android cross-compile to validate against the actual target. The macOS
   CLI (`src/main.c`) was intended as the fast iteration loop per the task
   instructions instead.
2. **macOS CLI/test executables currently fail to link** (pre-existing,
   introduced by the recent iOS-platform-support commits, e.g.
   `b657a8f`/`da849a7` on the rebased branch): `src/governor/governor_manifest_init.c`
   guards iOS-only externs (`ethervox_ios_get_files_dir()`,
   `ethervox_ios_get_bundle_resource_path()`) with `#elif defined(__APPLE__)`,
   which also matches plain macOS builds (not just iOS) - so linking any
   executable that pulls in `ethervox_governor_init_with_manifest()`
   (basically everything, since `governor.c` needs it) fails on macOS with
   undefined symbols, because those functions are meant to be supplied by
   iOS app-side Objective-C bridge code (see the comment at
   `src/platform/ios_hal.c:383`, "see EthervoxBridge.mm in the ios app
   repo") that doesn't exist in this native-core repo at all. Same root
   cause also breaks `src/plugins/weather_tools/weather_core.c`'s
   `ios_http_get_request` extern. **The static library itself
   (`libethervoxai.a`) compiles cleanly** - only final linking of an
   executable (the CLI app or any test binary) is affected. This blocks
   compiling and running the test coverage mentioned in section 6 on
   macOS until fixed (likely fix: guard those externs on
   `ETHERVOX_PLATFORM_IOS` instead of blanket `__APPLE__`/`TARGET_OS_IPHONE`,
   matching the pattern `weather_core.c` already uses correctly at its own
   `#elif defined(ETHERVOX_PLATFORM_IOS) || defined(TARGET_OS_IPHONE)`
   guard one file over - `governor_manifest_init.c` is the one that's
   wrong). Recommend fixing this separately (own small PR, unrelated to
   this feature) before or alongside implementing the above, purely so
   `cmake --build build --target ethervoxai_app` /
   `--target test_voice_conversation` etc. become usable again for
   validation.

---

## 9. Suggested implementation order

1. Fix the macOS/iOS build break (section 8, item 2) as an independent
   first commit, so all subsequent work can actually be built and tested
   locally.
2. Reproduce and confirm the `ethervox_governor_reload_model()` cache_dir
   bug (section 5.1) against current `main`/pre-unification code, to
   validate the root-cause theory before other changes land on top and
   make that harder to isolate.
3. Extract `granite_speech_decode()` (section 3.3) with **no behavior
   change** to `granite_speech_backend.c`/Mode 2 - land and test this in
   isolation first (existing Mode 2 tests/manual flow should be unaffected).
4. Add the `mtmd_ctx`/`mmproj_path`/`cache_dir_saved` fields and
   `ethervox_governor_load_model_with_audio()`/`transcribe_audio()`/
   `has_audio_support()` to governor.c/.h, gated so existing text-only
   Governor behavior is provably unchanged (run existing governor tests
   after this step, before touching voice_conversation.c).
5. Fix the reload cache_dir bug (section 5.1) and extend context-size
   minimum (section 5.2).
6. Implement conversation KV persistence extension (section 5.3) - this is
   the most speculative/complex piece; consider landing it as a
   separately-toggleable improvement after the core unification works
   without conversation continuity, if time-boxing is a concern.
7. Rewire `voice_conversation.c` (section 4) behind the
   `ethervox_governor_has_audio_support()` branch.
8. Add/update tests (section 6).
9. Update this document with any deviations discovered during
   implementation, before handing off to the Android integration session.
