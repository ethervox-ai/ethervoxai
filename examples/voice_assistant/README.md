# Voice Assistant Demo

The voice assistant demo exercises the EthervoxAI wake word, speech-to-text, dialogue, and text-to-speech pipeline.

## Running with audio (default)

```bash
./voice_assistant_demo
```text

1. Speak the wake phrase `hey ethervox` near your microphone.
1. Continue speaking your request. The placeholder STT module emits a final result after roughly two seconds of captured audio.
1. Say `stop` to end the conversation.

If the wake word never triggers, try speaking louder or closer to the microphone. You can also lower the sensitivity threshold inside `examples/voice_assistant/main.c`.

> **No input device?** If ALSA reports `Unknown PCM default`, set the input explicitly before launching:

```bash
export ETHERVOX_ALSA_DEVICE=plughw:1,0
./voice_assistant_demo
```text

To choose a different playback device, set `ETHERVOX_ALSA_PLAYBACK` in the same fashion. The demo will automatically fall back to text mode when capture cannot start.

## Text-only fallback

When you do not have a microphone available, launch the demo with `--text` to interact from the terminal:

```bash
./voice_assistant_demo --text
```text

Type your message after the `You>` prompt and press Enter. Type `exit`, `quit`, or `stop` to leave the session.

## Language override

Use `--lang` (or `-l`) to provide a 2-letter language code. The assistant will map this to a supported STT locale where possible.

```bash
./voice_assistant_demo --lang es
```

## LLM Model Selection

The voice assistant supports local LLM inference using GGUF models. Specify a model using `--model`:

### Preset Models (Auto-Download)

Use preset names to automatically download recommended models:

```bash
# TinyLlama 1.1B (Q4_K_M, ~637 MB) - Fastest, suitable for low-resource devices
./voice_assistant_demo --model=tinyllama

# Phi-2 2.7B (Q4_K_M, ~1.6 GB) - Balanced performance
./voice_assistant_demo --model=phi2

# Mistral 7B (Q4_K_M, ~4.1 GB) - Higher quality responses
./voice_assistant_demo --model=mistral

# Llama 2 7B (Q4_K_M, ~3.8 GB) - Production-ready performance
./voice_assistant_demo --model=llama2
```

The first time you use a preset model, it will be automatically downloaded to `~/.cache/ethervox/models/` (Linux/macOS) or `%USERPROFILE%\.cache\ethervox\models\` (Windows). Progress is displayed during download. Subsequent runs will use the cached model.

### Custom Model Paths

You can also provide a local GGUF file path:

```bash
./voice_assistant_demo --model=/path/to/model.gguf
```

### Disabling Auto-Download

To prevent automatic downloads (e.g., on metered connections), the assistant will fall back to text-only mode if the model is not cached. Manual download instructions are provided.

**Note:** LLM support requires building with `WITH_LLAMA=ON` (enabled by default). Model downloads require `libcurl` (Linux/macOS) or WinINet (Windows)
