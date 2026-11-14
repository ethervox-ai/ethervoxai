# Multimodal models (`mtmd`) — how to integrate one

**Read this before touching any vision, audio or document-understanding model.**

This document exists because an investigation in August 2026 concluded — wrongly, and in writing —
that `granite-docling-258M` could not be supported because "llama.cpp does not support Idefics3".
It does, in seven places, and it has a special case named after that exact model. The report was
deleted. The rules below are what would have prevented it.

---

## 1. The mental model

A multimodal model in llama.cpp is **two GGUF files**:

| File | Contains | Loaded by |
|---|---|---|
| **text model** | the LLM | `llama_model_load_from_file()` |
| **mmproj** | vision or audio encoder + projector | `mtmd_init_from_file(mmproj, model, params)` |

`mtmd` is the multimodal layer. It takes the *already loaded* text model plus the projector file and
returns an `mtmd_context`. Media is fed in as an `mtmd_bitmap` — images via `mtmd_bitmap_init()`,
audio via `mtmd_bitmap_init_from_audio()` — and becomes `mtmd_input_chunks` interleaved with text.

**This is the same mechanism for audio and images.** Granite Speech works exactly this way and has
shipped in `ethervoxai-android` for months. If you are integrating a vision model, you are not
inventing anything — you are following `src/stt/granite_speech_backend.c`.

The split is a **packaging** convention, not a property of the model's architecture. A model shipped
by its authors as one checkpoint is split *by the converter*. "It's a unified checkpoint, so it
can't work" is a non-sequitur — that is what `--mmproj` is for.

---

## 2. How to check whether a model is supported — in this order

**Never answer this from prose documentation.** `tools/mtmd/README.md` and `docs/multimodal.md` are
incomplete and lag the code. Check the code, in this order.

### Step 1 — is there a projector type?

```bash
grep -n "PROJECTOR_TYPE_" external/llama.cpp/tools/mtmd/clip-impl.h
```

There are **60+** as of `core-v0.3.x`, including `IDEFICS3`, `GRANITE_SPEECH`, `GRANITE4_VISION`,
`QWEN3VL`, `MINICPMV4_6`, `GEMMA4V`, `PARAKEET`, `VOXTRAL`, `PADDLEOCR`, `DEEPSEEKOCR`.

### Step 2 — can the converter emit it?

Model classes are **no longer in `convert_hf_to_gguf.py`**. Recent llama.cpp moved them into a
`conversion/` package; the top-level script is a ~13 KB shim. Grepping the old monolithic script
finds nothing and proves nothing — this is exactly the mistake that produced the deleted report.

```bash
grep -rn "<ArchitectureName>ForConditionalGeneration" external/llama.cpp/conversion/
grep -rn "<model-name>" external/llama.cpp/conversion/
```

For example, `Idefics3ForConditionalGeneration` maps to `conversion/smolvlm.py`, which registers
both Idefics3 and SmolVLM and writes `VisionProjectorType.IDEFICS3`. And `conversion/base.py`
carries a case named for `granite-docling-258M` specifically.

### Step 3 — is there a preprocessor and a chat template?

```bash
grep -rn "mtmd_image_preprocessor_" external/llama.cpp/tools/mtmd/mtmd-image.h
grep -rn "MTMD_SLICE_TMPL_"        external/llama.cpp/tools/mtmd/mtmd.cpp
```

A projector type without a preprocessor will load and produce nonsense. Check all three.

**Only if all three are absent** is the architecture genuinely unsupported. Then the options are an
upstream PR, a different model, or waiting — and that conclusion needs the three greps above pasted
into the packet as evidence.

---

## 3. Converting a model

```bash
# 1. text model
python3 convert_hf_to_gguf.py <model-dir> --outfile text-model.gguf --outtype f16

# 2. projector — the SAME script, with --mmproj
python3 convert_hf_to_gguf.py <model-dir> --mmproj --outfile mmproj-model.gguf --outtype f16
```

Running the converter twice **is the correct modern workflow**, not a workaround. Two files is the
expected outcome.

`llava_surgery_v2.py` is for **legacy LLaVA-derived** models only. Running it on a modern
architecture and getting `Found 0 tensors to extract out of 0 tensors` means you used the wrong
tool. It is not evidence about the model.

### Verify the output before blaming the runtime

```bash
python3 gguf-py/gguf/scripts/gguf_dump.py mmproj-model.gguf | grep -i "projector_type\|clip"
```

The projector type in the metadata must match a `PROJECTOR_TYPE_*` the runtime knows. If it says
`unknown` or is absent, the **conversion** failed — the runtime is not the problem.

Sanity-check sizes too. Weights do not appear from nowhere: if text + mmproj greatly exceeds the
source checkpoint, something upcast a tensor and the conversion is wrong.

---

## 4. Loading through our API

Never call `mtmd_*` directly from feature code. Everything goes through the model pool
(`include/ethervox/model_pool.h`), which owns budget accounting:

```c
ethervox_model_config_t cfg = {
    .model_path   = "/abs/path/text-model.gguf",
    .mmproj_path  = "/abs/path/mmproj-model.gguf",  /* NULL for text-only models */
    .context_size = 4096,
    .n_threads    = 4,
    .use_gpu      = true,
    .role         = "vision",                        /* "main" | "vision" | "embed" | "speech" */
};
```

The pool loads the text model, then calls `mtmd_init_from_file()` when `mmproj_path` is set, and
returns `ETHERVOX_ERROR_FILE_READ` if the projector fails to load. It estimates weights **plus KV
cache** against the budget and refuses a load that would not fit
(`ethervox_model_pool_would_fit()`).

After loading, interrogate the context rather than assuming:

```c
mtmd_support_vision(ctx);          /* does this projector do images?  */
mtmd_support_audio(ctx);           /* does it do audio?               */
mtmd_get_audio_sample_rate(ctx);   /* resample to this, do not guess  */
mtmd_get_marker(ctx);              /* the media marker for the prompt */
```

**Rules**

- **Absolute paths only.** Nothing in C expands `~`. A literal `~/...` reaches `fopen()`, fails, and
  the resulting `NULL` is dereferenced somewhere less obvious. Expand with `getenv("HOME")` at the
  boundary, once. *(This caused several segfaults that were then misattributed to the model
  architecture.)*
- **Check every return.** `mtmd_init_from_file()` returns `NULL` on failure and must free the
  `llama_context` and `llama_model` before returning an error.
- **One `mtmd_context` per model handle**, freed by the pool, never by feature code.
- **Media models are load-on-demand** on every tier except L — see `12-PLATFORM-MATRIX.md` §3. Do
  not hold a vision context resident on an 8 GB machine.

---

## 5. Designing a packet that touches this

**In the packet, before writing code:**

1. Paste the output of the three greps in §2. Support is a fact you demonstrate, not an assumption.
2. Name the projector type the model will produce.
3. State the file sizes you expect, and check them after conversion.

**Debug in this order** when a multimodal model fails to load. It is roughly cheapest-first, and the
top two account for most failures:

| # | Check | Command |
|---|---|---|
| 1 | Paths absolute and files exist | `ls -la <path>` |
| 2 | Files are GGUF, not safetensors | `head -c4 <file>` → `GGUF` |
| 3 | Projector metadata is a known type | `gguf_dump.py … \| grep projector_type` |
| 4 | Runtime has that projector type | `grep PROJECTOR_TYPE_… clip-impl.h` |
| 5 | Preprocessor and slice template exist | `grep … mtmd-image.h mtmd.cpp` |
| 6 | Only now: suspect the architecture | all three greps, pasted |

**Never conclude "unsupported" from a segfault.** A segfault is a null pointer, and the usual null
pointer is a file that did not open.

**Never conclude "unsupported" from prose docs.** Read `clip-impl.h` and `conversion/`.

**If you do conclude it, the evidence is three greps returning nothing** — not a README omission and
not a surgery script failing.

---

## 6. Known-good references

| Model | Projector type | Status |
|---|---|---|
| `granite-speech-4.1-2b-plus` | `GRANITE_SPEECH` | **Shipped**, `ethervoxai-android`. Read `src/stt/granite_speech_backend.c` first |
| `granite-docling-258M` | `IDEFICS3` | Supported by runtime and converter; end-to-end test outstanding (C3.1) |
| `granite-vision` | `GRANITE4_VISION` | Supported by runtime |

Note that our vendored llama.cpp carries a **local patch** to
`clip_graph_granite_speech::build()` fixing an upstream shape mismatch. Any llama.cpp bump must
re-verify or re-apply it — see ADR-0023 and the consumer-impact rule.
