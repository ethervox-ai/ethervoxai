# SPDX-License-Identifier: CC-BY-NC-SA-4.0

# Error handling tests
add_executable(test_error unit/test_error.c)
target_link_libraries(test_error ethervoxai)
target_include_directories(test_error PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME ErrorHandling COMMAND test_error)
set_tests_properties(ErrorHandling PROPERTIES TIMEOUT 30 LABELS "unit")

# TTS host tests (TASK-C1.0)
add_executable(test_tts_host unit/test_tts_host.c)
target_link_libraries(test_tts_host ethervoxai)
target_include_directories(test_tts_host PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME TtsHost COMMAND test_tts_host)
set_tests_properties(TtsHost PROPERTIES TIMEOUT 30 LABELS "unit")

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

# Stop sequence handling tests (TASK-C1.1)
add_executable(test_stop_sequences test_stop_sequences.c)
target_link_libraries(test_stop_sequences ethervoxai)
target_include_directories(test_stop_sequences PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME StopSequences COMMAND test_stop_sequences)
set_tests_properties(StopSequences PROPERTIES TIMEOUT 30 LABELS "unit;conformance")

# Path configuration tests (TASK-C1.3)
add_executable(test_paths test_paths.c)
target_link_libraries(test_paths ethervoxai)
target_include_directories(test_paths PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME Paths COMMAND test_paths)
set_tests_properties(Paths PROPERTIES TIMEOUT 30 LABELS "unit")

# Cancellation token tests (TASK-C1.4)
add_executable(test_cancel_token test_cancel_token.c)
target_link_libraries(test_cancel_token ethervoxai pthread)
target_include_directories(test_cancel_token PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME CancelToken COMMAND test_cancel_token)
set_tests_properties(CancelToken PROPERTIES TIMEOUT 30 LABELS "unit")

# Event stream and UTF-8 validation tests (TASK-C1.5)
add_executable(test_event_stream test_event_stream.c)
target_link_libraries(test_event_stream ethervoxai)
target_include_directories(test_event_stream PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME EventStream COMMAND test_event_stream)
set_tests_properties(EventStream PROPERTIES TIMEOUT 30 LABELS "unit")

# Multi-model pool tests (TASK-C2.1)
add_executable(test_model_pool unit/test_model_pool.c)
target_link_libraries(test_model_pool ethervoxai)
target_include_directories(test_model_pool PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME ModelPool COMMAND test_model_pool)
set_tests_properties(ModelPool PROPERTIES TIMEOUT 30 LABELS "unit")

# Host-registered tools tests (TASK-C2.2)
add_executable(test_host_tools unit/test_host_tools.c)
target_link_libraries(test_host_tools ethervoxai)
target_include_directories(test_host_tools PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME HostTools COMMAND test_host_tools)
set_tests_properties(HostTools PROPERTIES TIMEOUT 30 LABELS "unit")

# JSON Schema → GBNF converter tests (TASK-C2.3a)
add_executable(test_schema_to_gbnf unit/test_schema_to_gbnf.c)
target_link_libraries(test_schema_to_gbnf ethervoxai)
target_include_directories(test_schema_to_gbnf PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME SchemaToGbnf COMMAND test_schema_to_gbnf WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(SchemaToGbnf PROPERTIES TIMEOUT 30 LABELS "unit;grammar")

# Embeddings API tests (TASK-C2.4)
add_executable(test_embeddings unit/test_embeddings.c)
target_link_libraries(test_embeddings ethervoxai m)
target_include_directories(test_embeddings PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME Embeddings COMMAND test_embeddings)
set_tests_properties(Embeddings PROPERTIES TIMEOUT 30 LABELS "unit;embeddings")

# Chat template conformance tests (TASK-C2.5)
add_executable(test_chat_templates unit/test_chat_templates.c)
target_link_libraries(test_chat_templates ethervoxai)
target_include_directories(test_chat_templates PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME ChatTemplates COMMAND test_chat_templates)
set_tests_properties(ChatTemplates PROPERTIES TIMEOUT 30 LABELS "unit;chat_templates;conformance")

# Role-structured prompt rendering for structured generation (TASK-C6.1a), model-free
add_executable(test_structured_generation_roles unit/test_structured_generation_roles.c)
target_link_libraries(test_structured_generation_roles ethervoxai)
target_include_directories(test_structured_generation_roles PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME StructuredGenerationRoles COMMAND test_structured_generation_roles)
set_tests_properties(StructuredGenerationRoles PROPERTIES TIMEOUT 30 LABELS "unit;chat_templates")

# Object member order and comma placement in schema -> GBNF (BACKLOG-74), model-free
add_executable(test_schema_object_members unit/test_schema_object_members.c)
target_link_libraries(test_schema_object_members ethervoxai)
target_include_directories(test_schema_object_members PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME SchemaObjectMembers COMMAND test_schema_object_members)
set_tests_properties(SchemaObjectMembers PROPERTIES TIMEOUT 30 LABELS "unit;grammar")

# Multimodal media API tests (TASK-C3.1)
add_executable(test_media unit/test_media.c)
target_link_libraries(test_media ethervoxai)
target_include_directories(test_media PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME Media COMMAND test_media)
set_tests_properties(Media PROPERTIES TIMEOUT 30 LABELS "unit;media;multimodal")

# Granite-Docling integration test (TASK-C3.1 acceptance test)
# Manual test only - requires granite-docling-258M model and test image
add_executable(test_docling_integration integration/test_docling_integration.c)
target_link_libraries(test_docling_integration ethervoxai)
target_include_directories(test_docling_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)
# Not added to ctest - run manually:
# ./tests/test_docling_integration <model_path> <image_path>

# Minimal mmproj loading test (debug C3.1)
add_executable(test_mmproj_load integration/test_mmproj_load.c)
target_link_libraries(test_mmproj_load ethervoxai)
target_include_directories(test_mmproj_load PRIVATE ${CMAKE_SOURCE_DIR}/include)
# Not added to ctest - run manually:
# ./tests/test_mmproj_load <text_model.gguf> <mmproj.gguf>

# mmproj WITH context test (debug C3.1)
add_executable(test_mmproj_with_context integration/test_mmproj_with_context.c)
target_link_libraries(test_mmproj_with_context ethervoxai)
target_include_directories(test_mmproj_with_context PRIVATE ${CMAKE_SOURCE_DIR}/include)

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
# File tools exist only in profiles with ETHERVOX_FEATURE_FILE_TOOLS (not EDGE/WORKSPACE).
if(ETHERVOX_FEATURE_FILE_TOOLS)
add_executable(test_file_tools unit/test_file_tools.c)
target_link_libraries(test_file_tools ethervoxai)
target_include_directories(test_file_tools PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME FileTools COMMAND test_file_tools)
set_tests_properties(FileTools PROPERTIES TIMEOUT 60 LABELS "unit")
endif()

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

# Lookahead buffer filtering tests (token streaming, JSON leak prevention)
add_executable(test_lookahead_filtering test_lookahead_filtering.c)
target_link_libraries(test_lookahead_filtering ethervoxai)
target_include_directories(test_lookahead_filtering PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME LookaheadFiltering COMMAND test_lookahead_filtering)
set_tests_properties(LookaheadFiltering PROPERTIES TIMEOUT 10 LABELS "unit;governor;streaming")

# Microphone direct tests (manual testing, requires microphone)
add_executable(test_mic_direct unit/test_mic_direct.c)
target_link_libraries(test_mic_direct ethervoxai)
target_include_directories(test_mic_direct PRIVATE ${CMAKE_SOURCE_DIR}/include)
# Not added to CTest - requires microphone and user interaction
# Run manually: ./tests/test_mic_direct

# The phonemizer, Piper TTS and voice-training tests were removed with those features (TASK-C1.0).
# Text-to-speech now goes through ethervox_tts_host_t; see unit/test_tts_host.c.

# Tool descriptions tests
add_executable(test_tool_descriptions unit/test_tool_descriptions.c)
target_link_libraries(test_tool_descriptions ethervoxai)
target_include_directories(test_tool_descriptions PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME ToolDescriptions COMMAND test_tool_descriptions)
set_tests_properties(ToolDescriptions PROPERTIES TIMEOUT 30 LABELS "unit;tools")


# Profile conformance tests (TASK-C1.2) - verify feature composition per profile
add_executable(test_profile_conformance unit/test_profile_conformance.c)
target_link_libraries(test_profile_conformance ethervoxai)
target_include_directories(test_profile_conformance PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/include)
add_test(NAME ProfileConformance COMMAND test_profile_conformance)
set_tests_properties(ProfileConformance PROPERTIES TIMEOUT 10 LABELS "conformance;profile")

# Grammar generation stress test (TASK-C2.3c) - 500 schemas × 20 iterations
add_executable(test_grammar_generation test_grammar_generation.c)
target_link_libraries(test_grammar_generation ethervoxai)
target_include_directories(test_grammar_generation PRIVATE ${CMAKE_SOURCE_DIR}/include)
# Not added to ctest (requires model file and takes ~30-60 minutes)
# Generate schemas first: python3 scripts/generate_grammar_test_schemas.py
# Run manually: ./tests/test_grammar_generation <model.gguf> <schema_dir> [iterations]
# Example: ./tests/test_grammar_generation ~/.ethervox/models/governor/granite-4.0-h-1b-Q4_K_M.gguf tests/schemas/grammar_stress 20

# Simple generation test for debugging setup/grammar issues
add_executable(test_simple_generation test_simple_generation.c)
target_link_libraries(test_simple_generation ethervoxai)
target_include_directories(test_simple_generation PRIVATE ${CMAKE_SOURCE_DIR}/include)
# Run: ./tests/test_simple_generation ~/.ethervox/models/governor/granite-4.0-h-1b-Q4_K_M.gguf

# Session forking benchmark (TASK-C3.2) - measures ≥4× throughput improvement
# Uses fork(), which Windows does not have
if(NOT WIN32)
add_executable(bench_fork bench_fork.c)
target_link_libraries(bench_fork ethervoxai)
target_include_directories(bench_fork PRIVATE ${CMAKE_SOURCE_DIR}/include)
endif()
# Not added to ctest (requires model file and is a benchmark, not a test)
# Run manually: ./tests/bench_fork [model.gguf]
# Example: ./tests/bench_fork ~/models/granite-3.0-8b-instruct-Q4_K_M.gguf
# Target: ≥4× speedup for 300-document classification workload

# Session forking leak test (TASK-C3.2) - verify 300 forks don't leak
# Uses fork(), which Windows does not have
if(NOT WIN32)
add_executable(test_session_leak test_session_leak.c)
target_link_libraries(test_session_leak ethervoxai)
target_include_directories(test_session_leak PRIVATE ${CMAKE_SOURCE_DIR}/include)
endif()
# Run with ASan/LSan: ./tests/test_session_leak [model.gguf]
# Should complete with zero leaks reported by sanitizers

# Structured generation with confidence scoring (TASK-C3.3)
add_executable(test_structured_confidence test_structured_confidence.c)
target_link_libraries(test_structured_confidence ethervoxai)
target_include_directories(test_structured_confidence PRIVATE ${CMAKE_SOURCE_DIR}/include)
# Run: ./tests/test_structured_confidence <model.gguf>
# Example: ./tests/test_structured_confidence ~/.ethervox/models/governor/granite-4.0-h-1b-Q4_K_M.gguf
# Tests confidence calculation and log-prob extraction

# system_prompt role plumbing, real-model smoke test (BACKLOG-80)
add_executable(test_system_prompt_role test_system_prompt_role.c)
target_link_libraries(test_system_prompt_role ethervoxai)
target_include_directories(test_system_prompt_role PRIVATE ${CMAKE_SOURCE_DIR}/include)
# Run: ./tests/test_system_prompt_role <model.gguf>
# Example: ./tests/test_system_prompt_role ~/.ethervox/models/governor/granite-4.0-h-1b-Q4_K_M.gguf

# LRU eviction and memory pressure (TASK-C3.4)
add_executable(test_lru_eviction test_lru_eviction.c)
target_link_libraries(test_lru_eviction ethervoxai)
target_include_directories(test_lru_eviction PRIVATE ${CMAKE_SOURCE_DIR}/include)
# Run: ./tests/test_lru_eviction <model.gguf>
# Example: ./tests/test_lru_eviction ~/.ethervox/models/governor/granite-4.0-h-1b-Q4_K_M.gguf
# Tests: resident never evicted, LRU order, max_on_demand, protected roles

# Platform abstraction layer (TASK-C4.1)
add_executable(test_platform_abstraction test_platform_abstraction.c)
target_link_libraries(test_platform_abstraction ethervoxai)
target_include_directories(test_platform_abstraction PRIVATE ${CMAKE_SOURCE_DIR}/include)
# Run: ./tests/test_platform_abstraction
# Tests: mutex, time, file system, directory, memory operations

# Deterministic seeding (TASK-C4.2)
add_executable(test_deterministic_seeding test_deterministic_seeding.c)
target_link_libraries(test_deterministic_seeding ethervoxai)
target_include_directories(test_deterministic_seeding PRIVATE ${CMAKE_SOURCE_DIR}/include)
# Run: ./tests/test_deterministic_seeding <model.gguf>
# Example: ./tests/test_deterministic_seeding ~/.ethervox/models/governor/granite-4.0-h-1b-Q4_K_M.gguf
# Tests: same seed → identical output (20 runs), different seeds → different output, seed=0 → random

# Structured logging and metrics (TASK-C4.3)
add_executable(test_logging_metrics test_logging_metrics.c)
target_link_libraries(test_logging_metrics ethervoxai)
target_include_directories(test_logging_metrics PRIVATE ${CMAKE_SOURCE_DIR}/include)
# Run: ./tests/test_logging_metrics
# Tests: callback invocation, subsystem tagging, structured fields, metrics snapshot, metrics reset

# LoRA adapter loading (TASK-C5.1)
add_executable(test_adapter test_adapter.c)
target_link_libraries(test_adapter ethervoxai)
target_include_directories(test_adapter PRIVATE ${CMAKE_SOURCE_DIR}/include)
# Run: ./tests/test_adapter [<model.gguf> <adapter.gguf>]
# Tests: adapter loading, metadata reading, apply/clear adapters, NULL handling
# Note: Without adapter file, runs API contract tests only

# Tool catalogue: contracts as data (TASK-C2.6a)
add_executable(test_tool_catalogue test_tool_catalogue.c)
target_link_libraries(test_tool_catalogue ethervoxai)
target_include_directories(test_tool_catalogue PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests ${ETHERVOX_CATALOGUE_GENERATED_DIR})
add_test(NAME test_tool_catalogue COMMAND test_tool_catalogue)
# Run: ./tests/test_tool_catalogue

# Streaming transcription events (TASK-C3.5)
add_executable(test_streaming_transcription test_streaming_transcription.c)
target_link_libraries(test_streaming_transcription ethervoxai)
target_include_directories(test_streaming_transcription PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests)
add_test(NAME test_streaming_transcription COMMAND test_streaming_transcription)
# Run: ./tests/test_streaming_transcription
# Tests: event struct shape, callback register/clear/NULL-safety, callback invocation fields
# Note: full pipeline (real segments arriving mid-recording) needs a live Granite Speech Plus model

# Streaming transcription, live model (TASK-C3.5) - not add_test'd: needs real
# model/mmproj GGUF paths and real audio, unlike the unit test above.
add_executable(test_streaming_transcription_live test_streaming_transcription_live.c)
target_link_libraries(test_streaming_transcription_live ethervoxai)
target_include_directories(test_streaming_transcription_live PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests)
# Run: ./tests/test_streaming_transcription_live <model.gguf> <mmproj.gguf> <chunk1.wav> <chunk2.wav>
# Tests: real two-chunk decode with prefix_text carry-forward, UTF-8/non-Latin script survival
# Set C35_NO_PREFIX=1 to isolate BACKLOG-31 (prefix_text carry-forward degrades chunk 2 quality)

# Shared governor/STT model (TASK-C3.6b) - not add_test'd: needs real model/mmproj
# GGUF paths and real audio.
add_executable(test_c36b_shared_governor_stt integration/test_c36b_shared_governor_stt.c)
target_link_libraries(test_c36b_shared_governor_stt ethervoxai)
target_include_directories(test_c36b_shared_governor_stt PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests)
# Run: ./tests/test_c36b_shared_governor_stt <model.gguf> <mmproj.gguf> <speech.wav>
# Tests: governor-shaped load (role=main) then STT load (role=speech) against the
# same model_path shares weights (memory delta < 700MB, not a second full model),
# and STT still produces a real transcript through the shared model + its own context

# Governor context integrity + ctx_mutex concurrency (TASK-C3.6b) - not add_test'd:
# needs real Granite Speech Plus model/mmproj GGUF paths and real audio.
add_executable(test_c36b_threading_and_context integration/test_c36b_threading_and_context.c)
target_link_libraries(test_c36b_threading_and_context ethervoxai)
target_include_directories(test_c36b_threading_and_context PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests)
# Run: ./tests/test_c36b_threading_and_context <model.gguf> <mmproj.gguf> <speech.wav>
# Tests: conversation (seq 0) is byte-identical (kv_pos + decoded contents) before
# and after a real ASR pass on seq 2; concurrent ethervox_governor_execute() +
# ethervox_governor_transcribe_audio() from separate threads completes cleanly
# now that ctx_mutex serializes access to the shared llama_context
# Tests: golden byte-identical contracts for the migrated compute_tools group,
# loader NOT_FOUND/NOT_SUPPORTED/INVALID_ARGUMENT/success paths
