# SPDX-License-Identifier: CC-BY-NC-SA-4.0

# Error handling tests
add_executable(test_error unit/test_error.c)
target_link_libraries(test_error ethervoxai)
target_include_directories(test_error PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME ErrorHandling COMMAND test_error)
set_tests_properties(ErrorHandling PROPERTIES TIMEOUT 30 LABELS "unit")

# Context overflow management tests
add_executable(test_context_overflow unit/test_context_overflow.c)
target_link_libraries(test_context_overflow ethervoxai)
target_include_directories(test_context_overflow PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME ContextOverflow COMMAND test_context_overflow)
set_tests_properties(ContextOverflow PROPERTIES TIMEOUT 30 LABELS "unit")

# Audio core tests
add_executable(test_audio_core unit/test_audio_core.c)
target_link_libraries(test_audio_core ethervoxai)
target_include_directories(test_audio_core PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME AudioCore COMMAND test_audio_core)
set_tests_properties(AudioCore PROPERTIES TIMEOUT 30 LABELS "unit")

# Config tests
add_executable(test_config unit/test_config.c)
target_link_libraries(test_config ethervoxai)
target_include_directories(test_config PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME Config COMMAND test_config)
set_tests_properties(Config PROPERTIES TIMEOUT 30 LABELS "unit")

# Memory tools tests
add_executable(test_memory_tools unit/test_memory_tools.c)
target_link_libraries(test_memory_tools ethervoxai)
target_include_directories(test_memory_tools PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME MemoryTools COMMAND test_memory_tools)
set_tests_properties(MemoryTools PROPERTIES TIMEOUT 30 LABELS "unit")

# Secret mode / privacy mode tests
add_executable(test_secret_mode unit/test_secret_mode.c)
target_link_libraries(test_secret_mode ethervoxai)
target_include_directories(test_secret_mode PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME SecretMode COMMAND test_secret_mode)
set_tests_properties(SecretMode PROPERTIES TIMEOUT 30 LABELS "unit")

# Adaptive memory tests (corrections and patterns)
add_executable(test_adaptive_memory unit/test_adaptive_memory.c)
target_link_libraries(test_adaptive_memory ethervoxai)
target_include_directories(test_adaptive_memory PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME AdaptiveMemory COMMAND test_adaptive_memory)
set_tests_properties(AdaptiveMemory PROPERTIES TIMEOUT 30 LABELS "unit")

# Plugin manager tests
add_executable(test_plugin_manager unit/test_plugin_manager.c)
target_link_libraries(test_plugin_manager ethervoxai)
target_include_directories(test_plugin_manager PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME PluginManager COMMAND test_plugin_manager)
set_tests_properties(PluginManager PROPERTIES TIMEOUT 30 LABELS "unit")

# KV cache management tests (conversation reset, position tracking)
add_executable(test_kv_cache_management unit/test_kv_cache_management.c)
target_link_libraries(test_kv_cache_management ethervoxai)
target_include_directories(test_kv_cache_management PRIVATE ${CMAKE_SOURCE_DIR}/include)
# This test requires a model file, so it's not added as a ctest
# Run manually: ./tests/test_kv_cache_management [model_path]

# Memory archive tests
add_executable(test_memory_archive unit/test_memory_archive.c)
target_link_libraries(test_memory_archive ethervoxai)
target_include_directories(test_memory_archive PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME MemoryArchive COMMAND test_memory_archive)
set_tests_properties(MemoryArchive PROPERTIES TIMEOUT 30 LABELS "unit")

# File tools tests (Phase 1: Binary detection, token estimation, error handling)
add_executable(test_file_tools unit/test_file_tools.c)
target_link_libraries(test_file_tools ethervoxai)
target_include_directories(test_file_tools PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME FileTools COMMAND test_file_tools)
set_tests_properties(FileTools PROPERTIES TIMEOUT 60 LABELS "unit")

# File append tool tests
add_executable(test_file_append unit/test_file_append.c)
target_link_libraries(test_file_append ethervoxai)
target_include_directories(test_file_append PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME FileAppend COMMAND test_file_append)
set_tests_properties(FileAppend PROPERTIES TIMEOUT 30 LABELS "unit;file_tools")

# Unit conversion tests
add_executable(test_unit_conversion unit/test_unit_conversion.c)
target_link_libraries(test_unit_conversion ethervoxai m)
target_include_directories(test_unit_conversion PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME UnitConversion COMMAND test_unit_conversion)
set_tests_properties(UnitConversion PROPERTIES TIMEOUT 30 LABELS "unit;unit_conversion")

# Tool Manifest System tests
add_executable(test_tool_manifest test_tool_manifest.c)
target_link_libraries(test_tool_manifest ethervoxai)
target_include_directories(test_tool_manifest PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME ToolManifest COMMAND test_tool_manifest)
set_tests_properties(ToolManifest PROPERTIES TIMEOUT 30 LABELS "unit;tool_manifest")

# Device profiling tests (adaptive hardware detection)
add_executable(test_device_profile unit/test_device_profile.c)
target_link_libraries(test_device_profile ethervoxai)
target_include_directories(test_device_profile PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME DeviceProfile COMMAND test_device_profile)
set_tests_properties(DeviceProfile PROPERTIES TIMEOUT 30 LABELS "unit;adaptive")

# GGUF config helper tests (model metadata extraction and optimal config)
add_executable(test_gguf_config unit/test_gguf_config.c)
target_link_libraries(test_gguf_config ethervoxai)
target_include_directories(test_gguf_config PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME GGUFConfig COMMAND test_gguf_config)
set_tests_properties(GGUFConfig PROPERTIES TIMEOUT 30 LABELS "unit;adaptive")

# GGUF metadata extraction test (requires model file, manual test)
add_executable(test_gguf_metadata_real test_gguf_metadata_real.c)
target_link_libraries(test_gguf_metadata_real ethervoxai)
target_include_directories(test_gguf_metadata_real PRIVATE ${CMAKE_SOURCE_DIR}/include)
# Run manually: ./tests/test_gguf_metadata_real [model_path.gguf]
# Example: ./tests/test_gguf_metadata_real ~/.ethervox/models/governor/granite-4.0-h-1b-Q4_K_M.gguf

# Tool Manifest System benchmarks (not a test, run manually)
add_executable(benchmark_tool_manifest benchmark_tool_manifest.c)
target_link_libraries(benchmark_tool_manifest ethervoxai)
target_include_directories(benchmark_tool_manifest PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Mobile optimization features tests (minimal mode, secret mode)
add_executable(test_mobile_optimization unit/test_mobile_optimization.c)
target_link_libraries(test_mobile_optimization ethervoxai)
target_include_directories(test_mobile_optimization PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME MobileOptimization COMMAND test_mobile_optimization)

# Language detection tests (heuristic fallback for multilingual TTS)
add_executable(test_language_detector unit/test_language_detector.c)
target_link_libraries(test_language_detector ethervoxai)
target_include_directories(test_language_detector PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME LanguageDetector COMMAND test_language_detector)
set_tests_properties(LanguageDetector PROPERTIES TIMEOUT 30 LABELS "unit;language_detection")
set_tests_properties(MobileOptimization PROPERTIES TIMEOUT 30 LABELS "unit;mobile")

# Wake word detection tests
add_executable(test_wake_word unit/test_wake_word.c)
target_link_libraries(test_wake_word ethervoxai)
target_include_directories(test_wake_word PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME WakeWord COMMAND test_wake_word)
set_tests_properties(WakeWord PROPERTIES TIMEOUT 30 LABELS "unit;wake_word")

# Voice conversation tests
add_executable(test_voice_conversation unit/test_voice_conversation.c)
target_link_libraries(test_voice_conversation ethervoxai)
target_include_directories(test_voice_conversation PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME VoiceConversation COMMAND test_voice_conversation)
set_tests_properties(VoiceConversation PROPERTIES TIMEOUT 30 LABELS "unit;conversation")

# Audio integration tests
add_executable(test_audio_integration unit/test_audio_integration.c)
target_link_libraries(test_audio_integration ethervoxai)
target_include_directories(test_audio_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME AudioIntegration COMMAND test_audio_integration)
set_tests_properties(AudioIntegration PROPERTIES TIMEOUT 30 LABELS "integration;audio")

# Settings persistence tests (JSON-based configuration)
add_executable(test_settings_persistence unit/test_settings_persistence.c)
target_link_libraries(test_settings_persistence ethervoxai)
target_include_directories(test_settings_persistence PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME SettingsPersistence COMMAND test_settings_persistence)
set_tests_properties(SettingsPersistence PROPERTIES TIMEOUT 30 LABELS "unit;settings")

# Cache summarization tests (KV cache management)
add_executable(test_cache_summarization unit/test_cache_summarization.c)
target_link_libraries(test_cache_summarization ethervoxai)
target_include_directories(test_cache_summarization PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME CacheSummarization COMMAND test_cache_summarization)
set_tests_properties(CacheSummarization PROPERTIES TIMEOUT 30 LABELS "unit;governor;cache")

# AEC infrastructure tests (reference buffer + Speex AEC)
add_executable(test_aec_infrastructure test_aec_infrastructure.c)
target_link_libraries(test_aec_infrastructure ethervoxai)
target_include_directories(test_aec_infrastructure PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME AECInfrastructure COMMAND test_aec_infrastructure)
set_tests_properties(AECInfrastructure PROPERTIES TIMEOUT 30 LABELS "unit;audio;aec")

# TTS text-to-phoneme integration tests (English, Chinese, German)
add_executable(test_tts_text_to_phoneme unit/test_tts_text_to_phoneme.c)
target_link_libraries(test_tts_text_to_phoneme ethervoxai m)
target_include_directories(test_tts_text_to_phoneme PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME TTSTextToPhoneme COMMAND test_tts_text_to_phoneme)
set_tests_properties(TTSTextToPhoneme PROPERTIES TIMEOUT 30 LABELS "unit;tts;phonemizer")

# TTS end-to-end synthesis tests (requires model file, not added to ctest)
add_executable(test_tts_synthesis unit/test_tts_synthesis.c)
target_link_libraries(test_tts_synthesis ethervoxai m)
target_include_directories(test_tts_synthesis PRIVATE ${CMAKE_SOURCE_DIR}/include)
# Run manually: ./tests/test_tts_synthesis [model_path.onnx]
# Example: ./tests/test_tts_synthesis ~/.ethervox/models/piper/en_US-lessac-medium.onnx

# Voice training system tests (pronunciation training, audio recording)
add_executable(test_voice_training unit/test_voice_training.c)
target_link_libraries(test_voice_training ethervoxai m)
target_include_directories(test_voice_training PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Lookahead buffer filtering tests (token streaming, JSON leak prevention)
add_executable(test_lookahead_filtering test_lookahead_filtering.c)
target_link_libraries(test_lookahead_filtering ethervoxai)
target_include_directories(test_lookahead_filtering PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME LookaheadFiltering COMMAND test_lookahead_filtering)
set_tests_properties(LookaheadFiltering PROPERTIES TIMEOUT 10 LABELS "unit;governor;streaming")
add_test(NAME VoiceTraining COMMAND test_voice_training)
set_tests_properties(VoiceTraining PROPERTIES TIMEOUT 60 LABELS "unit;voice;training")

# Voice training integration tests (requires microphone, manual test only)
add_executable(test_voice_training_integration integration/test_voice_training_integration.c)
target_link_libraries(test_voice_training_integration ethervoxai m)
target_include_directories(test_voice_training_integration PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
# Not added to CTest - requires microphone and user interaction
# Run manually: ./tests/test_voice_training_integration

# Microphone direct tests (manual testing, requires microphone)
add_executable(test_mic_direct unit/test_mic_direct.c)
target_link_libraries(test_mic_direct ethervoxai)
target_include_directories(test_mic_direct PRIVATE ${CMAKE_SOURCE_DIR}/include)
# Not added to CTest - requires microphone and user interaction
# Run manually: ./tests/test_mic_direct

# Phonemizer tests (English)
add_executable(test_phonemizer unit/test_phonemizer.c)
target_link_libraries(test_phonemizer ethervoxai)
target_include_directories(test_phonemizer PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME Phonemizer COMMAND test_phonemizer)
set_tests_properties(Phonemizer PROPERTIES TIMEOUT 30 LABELS "unit;phonemizer")

# Phonemizer tests (Chinese)
add_executable(test_phonemizer_chinese unit/test_phonemizer_chinese.c)
target_link_libraries(test_phonemizer_chinese ethervoxai)
target_include_directories(test_phonemizer_chinese PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME PhonemizerChinese COMMAND test_phonemizer_chinese)
set_tests_properties(PhonemizerChinese PROPERTIES TIMEOUT 30 LABELS "unit;phonemizer;chinese")

# Phonemizer tests (German)
add_executable(test_phonemizer_german unit/test_phonemizer_german.c)
target_link_libraries(test_phonemizer_german ethervoxai)
target_include_directories(test_phonemizer_german PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME PhonemizerGerman COMMAND test_phonemizer_german)
set_tests_properties(PhonemizerGerman PROPERTIES TIMEOUT 30 LABELS "unit;phonemizer;german")

# Piper phonemizers comprehensive tests
add_executable(test_piper_phonemizers unit/test_piper_phonemizers.c)
target_link_libraries(test_piper_phonemizers ethervoxai)
target_include_directories(test_piper_phonemizers PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME PiperPhonemizers COMMAND test_piper_phonemizers)
set_tests_properties(PiperPhonemizers PROPERTIES TIMEOUT 60 LABELS "unit;phonemizer;comprehensive")

# Tool descriptions tests
add_executable(test_tool_descriptions unit/test_tool_descriptions.c)
target_link_libraries(test_tool_descriptions ethervoxai)
target_include_directories(test_tool_descriptions PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME ToolDescriptions COMMAND test_tool_descriptions)
set_tests_properties(ToolDescriptions PROPERTIES TIMEOUT 30 LABELS "unit;tools")
