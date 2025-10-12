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
```text
