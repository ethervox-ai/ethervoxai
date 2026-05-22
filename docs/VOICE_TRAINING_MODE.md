# Voice Training Mode

Interactive pronunciation training system that learns from your voice to improve TTS quality.

## Overview

The `/voice_training` command launches an interactive session where:
1. **LLM generates training sentences** - AI creates phrases focusing on commonly mispronounced words
2. **System provides reference pronunciation** - TTS speaks the phrase so you hear correct pronunciation
3. **You speak the phrase** - System records your audio
4. **System learns corrections** - Compares your pronunciation to variants and stores improvements

## Usage

```bash
> /voice_training
```

This starts an interactive training session with on-screen prompts.

## Training Workflow

### Step 1: Phrase Generation
```
🤖 Generating training phrase...

📝 Practice this phrase:
   "The integrated circuit requires accurate calibration."
```

The LLM generates sentences that:
- Focus on commonly mispronounced words (integrate, circuit, accurate)
- Use natural, conversational language
- Are 5-10 words long for manageable practice
- Can be customized to focus on specific pronunciation challenges

### Step 2: Reference Audio
```
🔊 Playing reference pronunciation...
  (Audio synthesis successful, playback not yet implemented)
```

The system:
- Synthesizes the phrase using Piper TTS
- Plays it back so you hear proper pronunciation
- Saves reference audio for comparison

### Step 3: User Recording
```
🎤 Now you speak it!
   Press Enter when ready to record...

  🎤 Recording for 5 seconds...
```

You:
- Press Enter to start recording
- Read the phrase clearly
- Recording automatically stops after 5 seconds
- Audio saved to `~/.ethervox/voice_training/`

### Step 4: Analysis & Learning
```
🔍 Analyzing pronunciation...
  Training: 'integrated'
  ✓ Learned pronunciation: ˈɪntəˌɡɹeɪt → ɪnˈtɛɡɹeɪtɪd (similarity: 0.82)
  Training: 'circuit'
  ✓ Learned pronunciation: ˈsɝkɪt → sɝˈkɪt (similarity: 0.91)
  Training: 'accurate'
  ✓ Learned pronunciation: ˈækjəɹət → ˈækjʊɹət (similarity: 0.88)

✨ Session complete! Trained 3 words

Press Enter for next phrase, or type 'quit' to exit:
```

For each word, the system:
1. Generates 15-20 phoneme variants
2. Synthesizes each variant to audio
3. Compares your audio to each variant using mel spectrograms
4. Selects best match (similarity score)
5. Stores correction in pronunciation overrides

## Interactive Commands

During training, you can type:

- **`[Enter]`** - Continue to next phrase
- **`stats`** - Show training statistics
- **`quit`** or `q` - Exit training mode

Example stats output:
```
📊 Training Statistics:
   Sessions completed: 5
   Words trained: 23
   Phrases practiced: 5
```

## File Structure

Training data is saved to `~/.ethervox/voice_training/`:

```
~/.ethervox/voice_training/
├── reference_1.wav      # TTS reference audio (session 1)
├── user_1.wav          # Your recorded audio (session 1)
├── reference_2.wav      # Session 2 reference
├── user_2.wav          # Session 2 recording
└── ...
```

Pronunciation corrections are stored in:
- `~/.ethervox/pronunciation_overrides.json` - Personal corrections
- Auto-promoted to community after 50+ uses (see [PRONUNCIATION_TRAINING.md](PRONUNCIATION_TRAINING.md))

## Behind the Scenes

### LLM Phrase Generation

The system uses your Governor LLM to generate training sentences:

```
Prompt: "Generate a short, clear sentence (5-10 words) for pronunciation training.
        Focus on: common mispronunciations.
        The sentence should be natural and contain common words that people often mispronounce.
        Respond with ONLY the sentence, no extra text."

Response: "The architect designed an efficient building."
```

Benefits:
- Infinite variety of training phrases
- Can focus on specific problem areas
- Natural, conversational sentences
- Adapts to your progress

### Pronunciation Analysis

For each word in the phrase:

1. **Variant Generation** (15-20 variants per word)
   ```
   Word: "integrate"
   Base:     ˈɪntəˌɡɹeɪt
   Variant1: ɪntəɡɹeɪt          (no stress)
   Variant2: ɪˈntəɡɹeɪt         (different stress)
   Variant3: ˈintəˌɡɹeɪt        (i vs ɪ)
   ...
   ```

2. **Audio Synthesis** - Each variant synthesized to WAV

3. **Mel Spectrogram Extraction**
   ```
   Your audio:   [mel features: 80 bands × N frames]
   Variant audio: [mel features: 80 bands × M frames]
   ```

4. **DTW Comparison** - Dynamic Time Warping finds best alignment
   ```
   Distance: 2.14 → Similarity: 0.82 (exp(-distance))
   ```

5. **Best Match Selection** - Highest similarity above threshold (0.70)

6. **Storage** - Saved to pronunciation overrides with usage tracking

### Storage & Promotion

Corrections are managed through a 3-tier system:

**Personal Overrides** (`~/.ethervox/pronunciation_overrides.json`)
```json
{
  "overrides": [
    {
      "word": "integrate",
      "ipa": "ɪnˈtɛɡɹeɪt",
      "usage_count": 1,
      "confidence": 0.82,
      "trained_speaker_id": 0
    }
  ]
}
```

**Auto-Promotion** (50+ uses, 85%+ confidence)
- Moves to `~/.ethervox/community_overrides.json`
- Indicates high-quality, frequently-used correction

**Core Integration** (100+ community uses)
- Exported to `overrides_learned.c`
- Compiled into phonemizer for zero overhead
- Included in releases

## Typical Training Session

```
> /voice_training

╭───────────────────────────────────────────────────────────────╮
│           🎙️  Voice Pronunciation Training Mode              │
╰───────────────────────────────────────────────────────────────╯

This mode helps improve TTS pronunciation by learning from your voice.

How it works:
  1. LLM generates a training sentence
  2. Read it aloud (system records)
  3. System analyzes pronunciation
  4. Learns corrections from your speech

Commands:
  [Enter]    Generate and practice next phrase
  'stats'    Show training statistics
  'quit'     Exit training mode

Press Enter to start...
[user presses Enter]

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Training Session #1
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

🤖 Generating training phrase...

📝 Practice this phrase:
   "The schedule requires proper pronunciation guidance."

🔊 Playing reference pronunciation...

🎤 Now you speak it!
   Press Enter when ready to record...
[user reads phrase and presses Enter]

  🎤 Recording for 5 seconds...

🔍 Analyzing pronunciation...
  Training: 'schedule'
  ✓ Learned pronunciation: ˈskɛdʒul → ˈʃɛdjul (similarity: 0.79)
  Training: 'requires'
  ✓ Learned pronunciation: ɹɪˈkwaɪɹz (similarity: 0.94)
  Training: 'proper'
  ✓ Learned pronunciation: ˈpɹɑpɚ (similarity: 0.89)
  Training: 'pronunciation'
  ✓ Learned pronunciation: pɹəˌnʌnsiˈeɪʃən (similarity: 0.76)
  Training: 'guidance'
  ✓ Learned pronunciation: ˈɡaɪdəns (similarity: 0.91)

✨ Session complete! Trained 5 words

Press Enter for next phrase, or type 'quit' to exit:
stats

📊 Training Statistics:
   Sessions completed: 1
   Words trained: 5
   Phrases practiced: 1

Press Enter for next phrase, or type 'quit' to exit:
quit

╭───────────────────────────────────────────────────────────────╮
│                  Training Session Complete                    │
╰───────────────────────────────────────────────────────────────╯

📊 Final Statistics:
   Sessions completed: 1
   Words trained: 5
   Phrases practiced: 1

Training data saved to: ~/.ethervox/voice_training

✓ Training session ended. Returning to main prompt.
```

## Prerequisites

### Required
- Governor LLM initialized (for phrase generation)

### Optional (Recommended)
- Voice conversation session active (`/convon`)
  - Provides TTS for reference audio
  - Provides STT for transcription verification
  - Enables full training workflow

Without voice conversation:
```
  ⚠️  Voice conversation not initialized.
     Training will work but without audio feedback.
     Use /convon to initialize voice systems first.
```

You can still train, but:
- No reference audio playback
- No automatic transcription verification
- Manual word extraction from phrases

## Tips for Effective Training

### 1. Start with Problem Words
Focus on words you notice being mispronounced:
- Technical terms ("integrate", "schedule", "cache")
- Names ("EthervoxAI", proper nouns)
- Regional variations (British vs American English)

### 2. Multiple Sessions
- Train 10-20 phrases per session
- Take breaks between sessions
- Consistency matters more than volume

### 3. Clear Recording Environment
- Quiet room (minimize background noise)
- Good microphone placement
- Consistent volume and pace

### 4. Natural Speech
- Speak naturally, not over-enunciated
- Use your normal speaking voice
- Match the reference audio's prosody

### 5. Monitor Statistics
Check `stats` periodically:
- High similarity scores (>0.85) indicate good training
- Low scores (<0.70) may need re-recording
- Track improvement over time

## Current Limitations

### Placeholder Features
1. **Audio Recording** - Not yet implemented
   - Currently creates empty placeholder files
   - Need to integrate microphone capture

2. **Audio Playback** - Reference audio not played
   - Synthesis works, playback pending
   - Need audio output integration

3. **STT Verification** - Transcription not used
   - Whisper integration needed
   - Would verify correct word spoken

4. **Context Access** - Can't get TTS/STT from conversation session
   - API needs to expose contexts
   - Currently uses NULL pointers

### Workarounds

Until audio integration is complete, you can:
1. Check synthesized audio files manually
   ```bash
   cd ~/.ethervox/voice_training
   ls *.wav
   # Play in external app
   ```

2. Record audio separately and provide path
   ```bash
   # Record with external tool
   arecord -d 5 -f cd user_recording.wav
   # Training will use this file
   ```

3. Focus on phoneme analysis
   - System still learns phoneme variants
   - Pronunciation overrides still stored
   - Benefits will show in TTS output

## Future Enhancements

### Short-term
1. **Audio Recording Integration**
   - Use existing audio capture code from wake word
   - Add manual start/stop controls
   - Support multiple recording devices

2. **Audio Playback**
   - Play reference audio before recording
   - Allow replay of user audio
   - Compare side-by-side

3. **Whisper Transcription**
   - Verify user said correct words
   - Prevent training on wrong words
   - Detect pronunciation errors

### Medium-term
1. **Progress Tracking**
   - Per-word improvement metrics
   - Historical similarity scores
   - Identify problem words

2. **Targeted Training**
   - Focus on low-scoring words
   - Phoneme-specific practice
   - Customizable training plans

3. **Batch Processing**
   - Train on multiple recordings at once
   - Import external audio samples
   - Bulk phoneme analysis

### Long-term
1. **Active Learning**
   - System suggests words to practice
   - Prioritize high-frequency words
   - Adaptive difficulty

2. **Voice Cloning**
   - Learn your voice characteristics
   - Generate TTS that sounds like you
   - Personalized speech synthesis

3. **Multi-Speaker Support**
   - Train different voices separately
   - Gender/accent-aware training
   - Family or team training modes

## Troubleshooting

### "Governor not initialized"
Wait for system startup to complete, or check:
```bash
> /tools
# Should show loaded Governor tools
```

### "Voice conversation not initialized"
Enable conversation systems first:
```bash
> /convon
# Then try /voice_training again
```

### Low Similarity Scores (<0.70)
Possible causes:
- Background noise
- Microphone too far/close
- Speaking too fast/slow
- Different accent than training data

Solutions:
- Re-record in quieter environment
- Adjust microphone placement
- Speak at moderate pace
- Multiple attempts for problem words

### No Audio Playback
Known limitation - audio synthesis works but playback not implemented. Check files manually:
```bash
ls ~/.ethervox/voice_training/*.wav
```

## Integration with Other Features

### Memory System
Training sessions are logged to conversation memory:
```
[Training] Practiced 5 phrases, trained 23 words
```

### TTS Improvements
After training, TTS automatically uses learned pronunciations:
```
> Tell me about integrated circuits
# TTS now pronounces "integrated" correctly
```

### Pronunciation Dashboard
View all trained words:
```bash
cat ~/.ethervox/pronunciation_overrides.json | jq '.overrides[] | {word, ipa, confidence}'
```

### Model Management
Training data contributes to model improvement:
- Personal overrides: immediate effect
- Community promotion: shared learning
- Core integration: permanent improvements

## See Also

- [PRONUNCIATION_TRAINING.md](PRONUNCIATION_TRAINING.md) - Pronunciation system architecture
- [ADDING_NEW_COMMAND.md](ADDING_NEW_COMMAND.md) - How this feature was built
- [conversation.h](../include/ethervox/conversation.h) - Voice conversation API
- [pronunciation_trainer.h](../include/ethervox/pronunciation_trainer.h) - Training API

## License

Copyright (c) 2024-2025 EthervoxAI Team  
Licensed under CC BY-NC-SA 4.0
