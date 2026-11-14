

## 2026-08-10 Session — voice_conversation.c rewired, phonemiser/Piper deleted

### voice_conversation.c changes
- Removed `#include "ethervox/tts.h"`, added `#include "ethervox/tts_host.h"`
- Removed g_global_tts extern and references
- Removed tts_context/tts_initialized fields from session struct  
- Replaced desktop Piper path in conversation_on_speak() with ethervox_tts_host_speak() + is_speaking() loop
- Removed Piper initialization from ethervox_conversation_init()
- Stubbed get_phonemizer()/get_tts() to return NULL

### Physical deletions
- rm -rf src/tts/phonemizer (entire directory)
- rm -f src/tts/piper_backend.c src/tts/tts.c

### CMakeLists.txt cleanup
- Removed all phonemizer/*.c sources (both Windows and non-Windows)
- Deleted espeak dictionary section (lines 560-625)
- Removed MSVC /bigobj workaround for phonemizer.c
- Replaced ONNX/Piper linking section with removal comment

### settings_menu.c (partial)
- Fixed includes, stubbed action_reset_pronunciation()
- Lines ~584 and ~813 still fail (ethervox_tts_create calls) - need fixing next session

### Updated CHANGELOG.md with above changes

### Remaining work
1. Fix settings_menu.c test-voice functions
2. Check other consumers (dialogue_core.c, voice_training.c, etc.)
3. Test barge-in end-to-end
4. FILE_ACCESS_READ_WRITE assertion (needs C1.2)
5. Cross-platform verification (Android/iOS/Linux)
6. CI verification (deferred per user)
