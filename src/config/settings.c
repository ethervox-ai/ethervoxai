/**
 * @file settings.c
 * @brief Settings persistence implementation using cJSON
 */

#include "ethervox/settings.h"
#include "ethervox/config.h"
#include "ethervox/logging.h"
#include "ethervox/error.h"
#include "ethervox/platform_utils.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

// Default settings path: Windows: %APPDATA%\EthervoxAI\settings.json, Unix: ~/.ethervox/settings.json
static char s_default_path[512] = {0};

const char* ethervox_settings_get_default_path(void) {
    if (s_default_path[0] == '\0') {
        char app_data_dir[512];
        if (platform_get_app_data_dir(app_data_dir, sizeof(app_data_dir)) == ETHERVOX_SUCCESS) {
            snprintf(s_default_path, sizeof(s_default_path), 
                     "%s%csettings.json", app_data_dir, 
#ifdef _WIN32
                     '\\'
#else
                     '/'
#endif
            );
        } else {
            // Fallback to current directory
            strncpy(s_default_path, "settings.json", sizeof(s_default_path) - 1);
        }
    }
    return s_default_path;
}

ethervox_persistent_settings_t ethervox_settings_get_defaults(void) {
    ethervox_persistent_settings_t settings = {0};
    
    settings.version = 1;
    
    // Whisper defaults
    strncpy(settings.whisper.model_name, "base.bin", sizeof(settings.whisper.model_name) - 1);
    strncpy(settings.whisper.language, "auto", sizeof(settings.whisper.language) - 1);
    settings.whisper.temperature = 0.0f;
    settings.whisper.beam_size = 5;
    settings.whisper.translate_to_english = false;
    settings.whisper.n_threads = -1; // Auto-detect
    settings.whisper.use_gpu = false;
    
    // TTS defaults
#ifdef __APPLE__
    strncpy(settings.tts.engine, "piper", sizeof(settings.tts.engine) - 1);
    strncpy(settings.tts.voice, "en_GB-alba-medium", sizeof(settings.tts.voice) - 1); // Deprecated, use per-language
    strncpy(settings.tts.voice_en, "en_GB-alba-medium", sizeof(settings.tts.voice_en) - 1);
    strncpy(settings.tts.voice_zh, "zh_CN-huayan-medium", sizeof(settings.tts.voice_zh) - 1);
    strncpy(settings.tts.voice_de, "de_DE-thorsten-high", sizeof(settings.tts.voice_de) - 1);
    strncpy(settings.tts.voice_es, "es_MX-ald-medium", sizeof(settings.tts.voice_es) - 1);
#elif defined(__linux__)
    strncpy(settings.tts.engine, "piper", sizeof(settings.tts.engine) - 1);
    strncpy(settings.tts.voice, "en_GB-alba-medium", sizeof(settings.tts.voice) - 1); // Deprecated, use per-language
    strncpy(settings.tts.voice_en, "en_GB-alba-medium", sizeof(settings.tts.voice_en) - 1);
    strncpy(settings.tts.voice_zh, "zh_CN-huayan-medium", sizeof(settings.tts.voice_zh) - 1);
    strncpy(settings.tts.voice_de, "de_DE-thorsten-high", sizeof(settings.tts.voice_de) - 1);
    strncpy(settings.tts.voice_es, "es_MX-ald-medium", sizeof(settings.tts.voice_es) - 1);
#else
    strncpy(settings.tts.engine, "none", sizeof(settings.tts.engine) - 1);
    settings.tts.voice[0] = '\0';
    settings.tts.voice_en[0] = '\0';
    settings.tts.voice_zh[0] = '\0';
    settings.tts.voice_de[0] = '\0';
    settings.tts.voice_es[0] = '\0';
#endif
    settings.tts.speed = 1.0f;
    settings.tts.volume = 0.8f;
    settings.tts.phoneme_variance = 0.667f;  // Default Piper noise_scale
    settings.tts.prosody_variance = 0.8f;    // Default Piper noise_w (increase to 1.0-1.2 for more expressiveness)
    settings.tts.piper_model_path[0] = '\0'; // Will auto-detect
    
    // Conversation defaults
    settings.conversation.listen_timeout_ms = 5000;
    settings.conversation.conversation_timeout_ms = 30000;
    settings.conversation.silence_timeout_ms = 2000;
    settings.conversation.audio_energy_threshold = 0.01f;
    settings.conversation.filter_hallucinations = true;
    settings.conversation.max_audio_chunk_size = 16000 * 3; // 3 seconds at 16kHz
#if defined(ETHERVOX_PLATFORM_MACOS) || defined(ETHERVOX_PLATFORM_LINUX) || defined(ETHERVOX_PLATFORM_WINDOWS)
    settings.conversation.always_listening = true;  // Desktop: sufficient resources for continuous STT
#else
    settings.conversation.always_listening = false; // Embedded: use wake word to conserve resources
#endif
    
    // AEC defaults
    settings.aec.enabled = true;  // Enable AEC by default
    strncpy(settings.aec.backend, "speex", sizeof(settings.aec.backend) - 1);
    settings.aec.suppression_level = 0.5f;  // Moderate echo suppression
    settings.aec.filter_length_ms = 64;     // 64ms echo tail (1024 samples @ 16kHz)
    
    // Wake word defaults
    strncpy(settings.wake_word.wake_phrase, "hey ethervox", sizeof(settings.wake_word.wake_phrase) - 1);
    settings.wake_word.detection_threshold = 0.4f;
    settings.wake_word.expected_syllables = 3;
    settings.wake_word.min_syllables = 2;
    settings.wake_word.max_syllables = 5;
    settings.wake_word.vad_energy_threshold = 0.01f;
    settings.wake_word.vad_zcr_min = 0.01f;
    settings.wake_word.vad_zcr_max = 0.35f;
    settings.wake_word.cooldown_ms = 3000;
    
    // LLM defaults (from config.h defines)
    settings.llm.max_tokens = ETHERVOX_LLM_MAX_TOKENS_DEFAULT;
    settings.llm.context_length = ETHERVOX_GOVERNOR_CONTEXT_SIZE;  // Use Governor context size (LLM is Governor)
    settings.llm.temperature = ETHERVOX_LLM_TEMPERATURE_DEFAULT;
    settings.llm.top_p = ETHERVOX_LLM_TOP_P_DEFAULT;
    settings.llm.seed = ETHERVOX_LLM_SEED_DEFAULT;
    settings.llm.gpu_layers = ETHERVOX_LLM_GPU_LAYERS_DEFAULT;
    settings.llm.n_threads = -1; // Auto-detect
    
    // Governor defaults (from config.h defines)
    settings.governor.max_iterations = ETHERVOX_GOVERNOR_MAX_ITERATIONS;
    settings.governor.timeout_seconds = ETHERVOX_GOVERNOR_TIMEOUT_SECONDS;
    settings.governor.temperature = ETHERVOX_GOVERNOR_TEMPERATURE;
    settings.governor.max_tokens_per_iteration = ETHERVOX_GOVERNOR_MAX_TOKENS_PER_ITERATION;
    settings.governor.confidence_threshold = ETHERVOX_GOVERNOR_CONFIDENCE_THRESHOLD;
    settings.governor.gpu_layers = ETHERVOX_GOVERNOR_GPU_LAYERS;
    settings.governor.context_size = ETHERVOX_GOVERNOR_CONTEXT_SIZE;
    settings.governor.n_threads = ETHERVOX_GOVERNOR_THREADS;
    
    return settings;
}

static void ensure_config_dir(const char* filepath) {
    char dir_path[512];
    strncpy(dir_path, filepath, sizeof(dir_path) - 1);
    
    // Find last slash
    char* last_slash = strrchr(dir_path, '/');
    if (!last_slash) {
        last_slash = strrchr(dir_path, '\\');
    }
    
    if (last_slash) {
        *last_slash = '\0';
        
        // Create directory if it doesn't exist
        struct stat st = {0};
        if (stat(dir_path, &st) == -1) {
            mkdir(dir_path, 0700);
        }
    }
}

ethervox_result_t ethervox_settings_save(const ethervox_persistent_settings_t* settings, const char* filepath) {
    ETHERVOX_CHECK_PTR(settings);
    
    if (!filepath) {
        filepath = ethervox_settings_get_default_path();
    }
    
    ensure_config_dir(filepath);
    
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        ETHERVOX_LOG_ERROR("Failed to create JSON root object");
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, "Failed to create JSON root object");
    }
    
    cJSON_AddNumberToObject(root, "version", settings->version);
    
    // Whisper settings
    cJSON* whisper = cJSON_CreateObject();
    cJSON_AddStringToObject(whisper, "model", settings->whisper.model_name);
    cJSON_AddStringToObject(whisper, "language", settings->whisper.language);
    cJSON_AddNumberToObject(whisper, "temperature", settings->whisper.temperature);
    cJSON_AddNumberToObject(whisper, "beam_size", settings->whisper.beam_size);
    cJSON_AddBoolToObject(whisper, "translate_to_english", settings->whisper.translate_to_english);
    cJSON_AddNumberToObject(whisper, "n_threads", settings->whisper.n_threads);
    cJSON_AddBoolToObject(whisper, "use_gpu", settings->whisper.use_gpu);
    cJSON_AddItemToObject(root, "whisper", whisper);
    
    // TTS settings
    cJSON* tts = cJSON_CreateObject();
    cJSON_AddStringToObject(tts, "engine", settings->tts.engine);
    cJSON_AddStringToObject(tts, "voice", settings->tts.voice);
    cJSON_AddStringToObject(tts, "voice_en", settings->tts.voice_en);
    cJSON_AddStringToObject(tts, "voice_zh", settings->tts.voice_zh);
    cJSON_AddStringToObject(tts, "voice_de", settings->tts.voice_de);
    cJSON_AddStringToObject(tts, "voice_es", settings->tts.voice_es);
    cJSON_AddNumberToObject(tts, "speed", settings->tts.speed);
    cJSON_AddNumberToObject(tts, "volume", settings->tts.volume);
    cJSON_AddNumberToObject(tts, "phoneme_variance", settings->tts.phoneme_variance);
    cJSON_AddNumberToObject(tts, "prosody_variance", settings->tts.prosody_variance);
    // Note: piper_model_path is NOT saved - it's auto-generated from voice_en/zh/de at load time
    cJSON_AddItemToObject(root, "tts", tts);
    
    // Conversation settings
    cJSON* conversation = cJSON_CreateObject();
    cJSON_AddNumberToObject(conversation, "listen_timeout_ms", settings->conversation.listen_timeout_ms);
    cJSON_AddNumberToObject(conversation, "conversation_timeout_ms", settings->conversation.conversation_timeout_ms);
    cJSON_AddNumberToObject(conversation, "silence_timeout_ms", settings->conversation.silence_timeout_ms);
    cJSON_AddNumberToObject(conversation, "audio_energy_threshold", settings->conversation.audio_energy_threshold);
    cJSON_AddBoolToObject(conversation, "always_listening", settings->conversation.always_listening);
    cJSON_AddBoolToObject(conversation, "filter_hallucinations", settings->conversation.filter_hallucinations);
    cJSON_AddNumberToObject(conversation, "max_audio_chunk_size", settings->conversation.max_audio_chunk_size);
    cJSON_AddItemToObject(root, "conversation", conversation);
    
    // AEC settings
    cJSON* aec = cJSON_CreateObject();
    cJSON_AddBoolToObject(aec, "enabled", settings->aec.enabled);
    cJSON_AddStringToObject(aec, "backend", settings->aec.backend);
    cJSON_AddNumberToObject(aec, "suppression_level", settings->aec.suppression_level);
    cJSON_AddNumberToObject(aec, "filter_length_ms", settings->aec.filter_length_ms);
    cJSON_AddItemToObject(root, "aec", aec);
    
    // Wake word settings
    cJSON* wake_word = cJSON_CreateObject();
    cJSON_AddStringToObject(wake_word, "wake_phrase", settings->wake_word.wake_phrase);
    cJSON_AddNumberToObject(wake_word, "detection_threshold", settings->wake_word.detection_threshold);
    cJSON_AddNumberToObject(wake_word, "expected_syllables", settings->wake_word.expected_syllables);
    cJSON_AddNumberToObject(wake_word, "min_syllables", settings->wake_word.min_syllables);
    cJSON_AddNumberToObject(wake_word, "max_syllables", settings->wake_word.max_syllables);
    cJSON_AddNumberToObject(wake_word, "vad_energy_threshold", settings->wake_word.vad_energy_threshold);
    cJSON_AddNumberToObject(wake_word, "vad_zcr_min", settings->wake_word.vad_zcr_min);
    cJSON_AddNumberToObject(wake_word, "vad_zcr_max", settings->wake_word.vad_zcr_max);
    cJSON_AddNumberToObject(wake_word, "cooldown_ms", settings->wake_word.cooldown_ms);
    cJSON_AddItemToObject(root, "wake_word", wake_word);
    
    // LLM settings
    cJSON* llm = cJSON_CreateObject();
    cJSON_AddNumberToObject(llm, "max_tokens", settings->llm.max_tokens);
    cJSON_AddNumberToObject(llm, "context_length", settings->llm.context_length);
    cJSON_AddNumberToObject(llm, "temperature", settings->llm.temperature);
    cJSON_AddNumberToObject(llm, "top_p", settings->llm.top_p);
    cJSON_AddNumberToObject(llm, "seed", settings->llm.seed);
    cJSON_AddNumberToObject(llm, "gpu_layers", settings->llm.gpu_layers);
    cJSON_AddNumberToObject(llm, "n_threads", settings->llm.n_threads);
    cJSON_AddItemToObject(root, "llm", llm);
    
    // Governor settings
    cJSON* governor = cJSON_CreateObject();
    cJSON_AddNumberToObject(governor, "max_iterations", settings->governor.max_iterations);
    cJSON_AddNumberToObject(governor, "timeout_seconds", settings->governor.timeout_seconds);
    cJSON_AddNumberToObject(governor, "temperature", settings->governor.temperature);
    cJSON_AddNumberToObject(governor, "max_tokens_per_iteration", settings->governor.max_tokens_per_iteration);
    cJSON_AddNumberToObject(governor, "confidence_threshold", settings->governor.confidence_threshold);
    cJSON_AddNumberToObject(governor, "gpu_layers", settings->governor.gpu_layers);
    cJSON_AddNumberToObject(governor, "context_size", settings->governor.context_size);
    cJSON_AddNumberToObject(governor, "n_threads", settings->governor.n_threads);
    cJSON_AddItemToObject(root, "governor", governor);
    
    // Write to file
    char* json_string = cJSON_Print(root);
    cJSON_Delete(root);
    
    if (!json_string) {
        ETHERVOX_LOG_ERROR("Failed to serialize JSON");
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_CONFIG_SAVE_FAILED, "Failed to serialize settings to JSON");
    }
    
    FILE* f = fopen(filepath, "w");
    if (!f) {
        ETHERVOX_LOG_ERROR("Failed to open %s for writing: %s", filepath, strerror(errno));
        free(json_string);
        char msg[512];
        snprintf(msg, sizeof(msg), "Cannot open settings file for writing: %s", filepath);
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_FILE_WRITE, msg);
    }
    
    fprintf(f, "%s", json_string);
    fclose(f);
    free(json_string);
    
    ETHERVOX_LOG_INFO("Settings saved to %s", filepath);
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_settings_load(ethervox_persistent_settings_t* settings, const char* filepath) {
    ETHERVOX_CHECK_PTR(settings);
    
    if (!filepath) {
        filepath = ethervox_settings_get_default_path();
    }
    
    FILE* f = fopen(filepath, "r");
    if (!f) {
        ETHERVOX_LOG_INFO("Settings file not found at %s, using defaults", filepath);
        *settings = ethervox_settings_get_defaults();
        return ETHERVOX_SUCCESS;
    }
    
    // Read file contents
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        ETHERVOX_LOG_ERROR("Failed to allocate buffer for settings file");
        fclose(f);
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, "Failed to allocate memory for settings file");
    }
    
    size_t read_size = fread(buffer, 1, size, f);
    buffer[read_size] = '\0';
    fclose(f);
    
    ethervox_result_t ret = ethervox_settings_import(settings, buffer);
    free(buffer);
    
    if (ethervox_is_success(ret)) {
        ETHERVOX_LOG_INFO("Settings loaded from %s", filepath);
    }
    
    return ret;
}

ethervox_result_t ethervox_settings_import(ethervox_persistent_settings_t* settings, const char* json_string) {
    ETHERVOX_CHECK_PTR(settings);
    ETHERVOX_CHECK_PTR(json_string);
    
    // Start with defaults
    *settings = ethervox_settings_get_defaults();
    
    cJSON* root = cJSON_Parse(json_string);
    if (!root) {
        ETHERVOX_LOG_ERROR("Failed to parse JSON: %s", cJSON_GetErrorPtr());
        char msg[256];
        snprintf(msg, sizeof(msg), "JSON parse error: %s", cJSON_GetErrorPtr());
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_CONFIG_PARSE_ERROR, msg);
    }
    
    // Parse version
    cJSON* version = cJSON_GetObjectItem(root, "version");
    if (cJSON_IsNumber(version)) {
        settings->version = version->valueint;
    }
    
    // Parse whisper settings
    cJSON* whisper = cJSON_GetObjectItem(root, "whisper");
    if (cJSON_IsObject(whisper)) {
        cJSON* model = cJSON_GetObjectItem(whisper, "model");
        if (cJSON_IsString(model)) {
            strncpy(settings->whisper.model_name, model->valuestring, 
                    sizeof(settings->whisper.model_name) - 1);
        }
        
        cJSON* language = cJSON_GetObjectItem(whisper, "language");
        if (cJSON_IsString(language)) {
            strncpy(settings->whisper.language, language->valuestring, 
                    sizeof(settings->whisper.language) - 1);
        }
        
        cJSON* temp = cJSON_GetObjectItem(whisper, "temperature");
        if (cJSON_IsNumber(temp)) {
            settings->whisper.temperature = (float)temp->valuedouble;
        }
        
        cJSON* beam = cJSON_GetObjectItem(whisper, "beam_size");
        if (cJSON_IsNumber(beam)) {
            settings->whisper.beam_size = beam->valueint;
        }
        
        cJSON* translate = cJSON_GetObjectItem(whisper, "translate_to_english");
        if (cJSON_IsBool(translate)) {
            settings->whisper.translate_to_english = cJSON_IsTrue(translate);
        }
        
        cJSON* threads = cJSON_GetObjectItem(whisper, "n_threads");
        if (cJSON_IsNumber(threads)) {
            settings->whisper.n_threads = threads->valueint;
        }
        
        cJSON* gpu = cJSON_GetObjectItem(whisper, "use_gpu");
        if (cJSON_IsBool(gpu)) {
            settings->whisper.use_gpu = cJSON_IsTrue(gpu);
        }
    }
    
    // Parse conversation settings
    cJSON* conversation = cJSON_GetObjectItem(root, "conversation");
    if (cJSON_IsObject(conversation)) {
        cJSON* item;
        
        item = cJSON_GetObjectItem(conversation, "listen_timeout_ms");
        if (cJSON_IsNumber(item)) settings->conversation.listen_timeout_ms = item->valueint;
        
        item = cJSON_GetObjectItem(conversation, "conversation_timeout_ms");
        if (cJSON_IsNumber(item)) settings->conversation.conversation_timeout_ms = item->valueint;
        
        item = cJSON_GetObjectItem(conversation, "silence_timeout_ms");
        if (cJSON_IsNumber(item)) settings->conversation.silence_timeout_ms = item->valueint;
        
        item = cJSON_GetObjectItem(conversation, "audio_energy_threshold");
        if (cJSON_IsNumber(item)) settings->conversation.audio_energy_threshold = (float)item->valuedouble;
        
        item = cJSON_GetObjectItem(conversation, "always_listening");
        if (cJSON_IsBool(item)) settings->conversation.always_listening = cJSON_IsTrue(item);
        
        item = cJSON_GetObjectItem(conversation, "filter_hallucinations");
        if (cJSON_IsBool(item)) settings->conversation.filter_hallucinations = cJSON_IsTrue(item);
        
        item = cJSON_GetObjectItem(conversation, "max_audio_chunk_size");
        if (cJSON_IsNumber(item)) settings->conversation.max_audio_chunk_size = item->valueint;
    }
    
    // Parse TTS settings
    cJSON* tts = cJSON_GetObjectItem(root, "tts");
    if (cJSON_IsObject(tts)) {
        cJSON* item;
        
        item = cJSON_GetObjectItem(tts, "engine");
        if (cJSON_IsString(item)) {
            strncpy(settings->tts.engine, item->valuestring,
                    sizeof(settings->tts.engine) - 1);
        }
        
        item = cJSON_GetObjectItem(tts, "voice");
        if (cJSON_IsString(item)) {
            strncpy(settings->tts.voice, item->valuestring,
                    sizeof(settings->tts.voice) - 1);
        }
        
        item = cJSON_GetObjectItem(tts, "voice_en");
        if (cJSON_IsString(item)) {
            strncpy(settings->tts.voice_en, item->valuestring,
                    sizeof(settings->tts.voice_en) - 1);
        }
        
        item = cJSON_GetObjectItem(tts, "voice_zh");
        if (cJSON_IsString(item)) {
            strncpy(settings->tts.voice_zh, item->valuestring,
                    sizeof(settings->tts.voice_zh) - 1);
        }
        
        item = cJSON_GetObjectItem(tts, "voice_de");
        if (cJSON_IsString(item)) {
            strncpy(settings->tts.voice_de, item->valuestring,
                    sizeof(settings->tts.voice_de) - 1);
        }
        
        item = cJSON_GetObjectItem(tts, "voice_es");
        if (cJSON_IsString(item)) {
            strncpy(settings->tts.voice_es, item->valuestring,
                    sizeof(settings->tts.voice_es) - 1);
        }
        
        item = cJSON_GetObjectItem(tts, "speed");
        if (cJSON_IsNumber(item)) settings->tts.speed = (float)item->valuedouble;
        
        item = cJSON_GetObjectItem(tts, "volume");
        if (cJSON_IsNumber(item)) settings->tts.volume = (float)item->valuedouble;
        
        item = cJSON_GetObjectItem(tts, "phoneme_variance");
        if (cJSON_IsNumber(item)) settings->tts.phoneme_variance = (float)item->valuedouble;
        
        item = cJSON_GetObjectItem(tts, "prosody_variance");
        if (cJSON_IsNumber(item)) settings->tts.prosody_variance = (float)item->valuedouble;
        
        // Always auto-generate piper_model_path from voice_en setting
        // This ensures voice selection persists correctly across restarts
        if (strlen(settings->tts.voice_en) > 0) {
            const char* home = getenv("HOME");
            if (home) {
                snprintf(settings->tts.piper_model_path, sizeof(settings->tts.piper_model_path),
                        "%s/.ethervox/models/piper/%s.onnx", home, settings->tts.voice_en);
                ETHERVOX_LOG_DEBUG("[Settings] Auto-generated piper_model_path from voice_en: %s",
                                   settings->tts.piper_model_path);
            }
        }
    }
    
    // Parse AEC settings
    cJSON* aec = cJSON_GetObjectItem(root, "aec");
    if (cJSON_IsObject(aec)) {
        cJSON* item;
        
        item = cJSON_GetObjectItem(aec, "enabled");
        if (cJSON_IsBool(item)) settings->aec.enabled = cJSON_IsTrue(item);
        
        item = cJSON_GetObjectItem(aec, "backend");
        if (cJSON_IsString(item)) {
            strncpy(settings->aec.backend, item->valuestring,
                    sizeof(settings->aec.backend) - 1);
        }
        
        item = cJSON_GetObjectItem(aec, "suppression_level");
        if (cJSON_IsNumber(item)) settings->aec.suppression_level = (float)item->valuedouble;
        
        item = cJSON_GetObjectItem(aec, "filter_length_ms");
        if (cJSON_IsNumber(item)) settings->aec.filter_length_ms = item->valueint;
    }
    
    // Parse wake word settings
    cJSON* wake_word = cJSON_GetObjectItem(root, "wake_word");
    if (cJSON_IsObject(wake_word)) {
        cJSON* item;
        
        item = cJSON_GetObjectItem(wake_word, "wake_phrase");
        if (cJSON_IsString(item)) {
            strncpy(settings->wake_word.wake_phrase, item->valuestring,
                    sizeof(settings->wake_word.wake_phrase) - 1);
        }
        
        item = cJSON_GetObjectItem(wake_word, "detection_threshold");
        if (cJSON_IsNumber(item)) settings->wake_word.detection_threshold = (float)item->valuedouble;
        
        item = cJSON_GetObjectItem(wake_word, "expected_syllables");
        if (cJSON_IsNumber(item)) settings->wake_word.expected_syllables = item->valueint;
        
        item = cJSON_GetObjectItem(wake_word, "min_syllables");
        if (cJSON_IsNumber(item)) settings->wake_word.min_syllables = item->valueint;
        
        item = cJSON_GetObjectItem(wake_word, "max_syllables");
        if (cJSON_IsNumber(item)) settings->wake_word.max_syllables = item->valueint;
        
        item = cJSON_GetObjectItem(wake_word, "vad_energy_threshold");
        if (cJSON_IsNumber(item)) settings->wake_word.vad_energy_threshold = (float)item->valuedouble;
        
        item = cJSON_GetObjectItem(wake_word, "vad_zcr_min");
        if (cJSON_IsNumber(item)) settings->wake_word.vad_zcr_min = (float)item->valuedouble;
        
        item = cJSON_GetObjectItem(wake_word, "vad_zcr_max");
        if (cJSON_IsNumber(item)) settings->wake_word.vad_zcr_max = (float)item->valuedouble;
        
        item = cJSON_GetObjectItem(wake_word, "cooldown_ms");
        if (cJSON_IsNumber(item)) settings->wake_word.cooldown_ms = item->valueint;
    }
    
    // Parse LLM settings (optional for backwards compatibility)
    cJSON* llm = cJSON_GetObjectItem(root, "llm");
    if (cJSON_IsObject(llm)) {
        cJSON* item;
        
        item = cJSON_GetObjectItem(llm, "max_tokens");
        if (cJSON_IsNumber(item)) settings->llm.max_tokens = item->valueint;
        
        item = cJSON_GetObjectItem(llm, "context_length");
        if (cJSON_IsNumber(item)) settings->llm.context_length = item->valueint;
        
        item = cJSON_GetObjectItem(llm, "temperature");
        if (cJSON_IsNumber(item)) settings->llm.temperature = (float)item->valuedouble;
        
        item = cJSON_GetObjectItem(llm, "top_p");
        if (cJSON_IsNumber(item)) settings->llm.top_p = (float)item->valuedouble;
        
        item = cJSON_GetObjectItem(llm, "seed");
        if (cJSON_IsNumber(item)) settings->llm.seed = item->valueint;
        
        item = cJSON_GetObjectItem(llm, "gpu_layers");
        if (cJSON_IsNumber(item)) settings->llm.gpu_layers = item->valueint;
        
        item = cJSON_GetObjectItem(llm, "n_threads");
        if (cJSON_IsNumber(item)) settings->llm.n_threads = item->valueint;
    }
    
    // Parse Governor settings (optional for backwards compatibility)
    cJSON* governor = cJSON_GetObjectItem(root, "governor");
    if (cJSON_IsObject(governor)) {
        cJSON* item;
        
        item = cJSON_GetObjectItem(governor, "max_iterations");
        if (cJSON_IsNumber(item)) settings->governor.max_iterations = item->valueint;
        
        item = cJSON_GetObjectItem(governor, "timeout_seconds");
        if (cJSON_IsNumber(item)) settings->governor.timeout_seconds = item->valueint;
        
        item = cJSON_GetObjectItem(governor, "temperature");
        if (cJSON_IsNumber(item)) settings->governor.temperature = (float)item->valuedouble;
        
        item = cJSON_GetObjectItem(governor, "max_tokens_per_iteration");
        if (cJSON_IsNumber(item)) settings->governor.max_tokens_per_iteration = item->valueint;
        
        item = cJSON_GetObjectItem(governor, "confidence_threshold");
        if (cJSON_IsNumber(item)) settings->governor.confidence_threshold = (float)item->valuedouble;
        
        item = cJSON_GetObjectItem(governor, "gpu_layers");
        if (cJSON_IsNumber(item)) settings->governor.gpu_layers = item->valueint;
        
        item = cJSON_GetObjectItem(governor, "context_size");
        if (cJSON_IsNumber(item)) settings->governor.context_size = item->valueint;
        
        item = cJSON_GetObjectItem(governor, "n_threads");
        if (cJSON_IsNumber(item)) settings->governor.n_threads = item->valueint;
    }
    
    cJSON_Delete(root);
    return ETHERVOX_SUCCESS;
}

char* ethervox_settings_export(const ethervox_persistent_settings_t* settings) {
    if (!settings) {
        return NULL;
    }
    
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    
    cJSON_AddNumberToObject(root, "version", settings->version);
    
    // Whisper settings
    cJSON* whisper = cJSON_CreateObject();
    cJSON_AddStringToObject(whisper, "model", settings->whisper.model_name);
    cJSON_AddStringToObject(whisper, "language", settings->whisper.language);
    cJSON_AddNumberToObject(whisper, "temperature", settings->whisper.temperature);
    cJSON_AddNumberToObject(whisper, "beam_size", settings->whisper.beam_size);
    cJSON_AddBoolToObject(whisper, "translate_to_english", settings->whisper.translate_to_english);
    cJSON_AddNumberToObject(whisper, "n_threads", settings->whisper.n_threads);
    cJSON_AddBoolToObject(whisper, "use_gpu", settings->whisper.use_gpu);
    cJSON_AddItemToObject(root, "whisper", whisper);
    
    // Conversation settings
    cJSON* conversation = cJSON_CreateObject();
    cJSON_AddNumberToObject(conversation, "listen_timeout_ms", settings->conversation.listen_timeout_ms);
    cJSON_AddNumberToObject(conversation, "conversation_timeout_ms", settings->conversation.conversation_timeout_ms);
    cJSON_AddNumberToObject(conversation, "silence_timeout_ms", settings->conversation.silence_timeout_ms);
    cJSON_AddNumberToObject(conversation, "audio_energy_threshold", settings->conversation.audio_energy_threshold);
    cJSON_AddBoolToObject(conversation, "always_listening", settings->conversation.always_listening);
    cJSON_AddBoolToObject(conversation, "filter_hallucinations", settings->conversation.filter_hallucinations);
    cJSON_AddNumberToObject(conversation, "max_audio_chunk_size", settings->conversation.max_audio_chunk_size);
    cJSON_AddItemToObject(root, "conversation", conversation);
    
    // AEC settings
    cJSON* aec = cJSON_CreateObject();
    cJSON_AddBoolToObject(aec, "enabled", settings->aec.enabled);
    cJSON_AddStringToObject(aec, "backend", settings->aec.backend);
    cJSON_AddNumberToObject(aec, "suppression_level", settings->aec.suppression_level);
    cJSON_AddNumberToObject(aec, "filter_length_ms", settings->aec.filter_length_ms);
    cJSON_AddItemToObject(root, "aec", aec);
    
    // Wake word settings
    cJSON* wake_word = cJSON_CreateObject();
    cJSON_AddStringToObject(wake_word, "wake_phrase", settings->wake_word.wake_phrase);
    cJSON_AddNumberToObject(wake_word, "detection_threshold", settings->wake_word.detection_threshold);
    cJSON_AddNumberToObject(wake_word, "expected_syllables", settings->wake_word.expected_syllables);
    cJSON_AddNumberToObject(wake_word, "min_syllables", settings->wake_word.min_syllables);
    cJSON_AddNumberToObject(wake_word, "max_syllables", settings->wake_word.max_syllables);
    cJSON_AddNumberToObject(wake_word, "vad_energy_threshold", settings->wake_word.vad_energy_threshold);
    cJSON_AddNumberToObject(wake_word, "vad_zcr_min", settings->wake_word.vad_zcr_min);
    cJSON_AddNumberToObject(wake_word, "vad_zcr_max", settings->wake_word.vad_zcr_max);
    cJSON_AddNumberToObject(wake_word, "cooldown_ms", settings->wake_word.cooldown_ms);
    cJSON_AddItemToObject(root, "wake_word", wake_word);
    
    // LLM settings
    cJSON* llm = cJSON_CreateObject();
    cJSON_AddNumberToObject(llm, "max_tokens", settings->llm.max_tokens);
    cJSON_AddNumberToObject(llm, "context_length", settings->llm.context_length);
    cJSON_AddNumberToObject(llm, "temperature", settings->llm.temperature);
    cJSON_AddNumberToObject(llm, "top_p", settings->llm.top_p);
    cJSON_AddNumberToObject(llm, "seed", settings->llm.seed);
    cJSON_AddNumberToObject(llm, "gpu_layers", settings->llm.gpu_layers);
    cJSON_AddNumberToObject(llm, "n_threads", settings->llm.n_threads);
    cJSON_AddItemToObject(root, "llm", llm);
    
    // Governor settings
    cJSON* governor = cJSON_CreateObject();
    cJSON_AddNumberToObject(governor, "max_iterations", settings->governor.max_iterations);
    cJSON_AddNumberToObject(governor, "timeout_seconds", settings->governor.timeout_seconds);
    cJSON_AddNumberToObject(governor, "temperature", settings->governor.temperature);
    cJSON_AddNumberToObject(governor, "max_tokens_per_iteration", settings->governor.max_tokens_per_iteration);
    cJSON_AddNumberToObject(governor, "confidence_threshold", settings->governor.confidence_threshold);
    cJSON_AddNumberToObject(governor, "gpu_layers", settings->governor.gpu_layers);
    cJSON_AddNumberToObject(governor, "context_size", settings->governor.context_size);
    cJSON_AddNumberToObject(governor, "n_threads", settings->governor.n_threads);
    cJSON_AddItemToObject(root, "governor", governor);
    
    char* json_string = cJSON_Print(root);
    cJSON_Delete(root);
    
    return json_string;
}

void ethervox_settings_print(const ethervox_persistent_settings_t* settings) {
    if (!settings) {
        return;
    }
    
    printf("\n╔═══════════════════════════════════════╗\n");
    printf("║       EthervoxAI Settings (v%d)       ║\n", settings->version);
    printf("╠═══════════════════════════════════════╣\n");
    
    printf("║ Whisper STT:                          ║\n");
    printf("║   Model: %-28s ║\n", settings->whisper.model_name);
    printf("║   Language: %-25s ║\n", settings->whisper.language);
    printf("║   Temperature: %-22.2f ║\n", settings->whisper.temperature);
    printf("║   Beam Size: %-24d ║\n", settings->whisper.beam_size);
    printf("║   Threads: %-26d ║\n", settings->whisper.n_threads);
    printf("║   GPU: %-31s ║\n", settings->whisper.use_gpu ? "enabled" : "disabled");
    
    printf("╠═══════════════════════════════════════╣\n");
    printf("║ Conversation:                         ║\n");
    printf("║   Listen Timeout: %-18d ms ║\n", settings->conversation.listen_timeout_ms);
    printf("║   Silence Timeout: %-17d ms ║\n", settings->conversation.silence_timeout_ms);
    printf("║   Energy Threshold: %-17.3f ║\n", settings->conversation.audio_energy_threshold);
    printf("║   Filter Hallucinations: %-12s ║\n", 
           settings->conversation.filter_hallucinations ? "yes" : "no");
    
    printf("╠═══════════════════════════════════════╣\n");
    printf("║ Wake Word:                            ║\n");
    printf("║   Phrase: %-27s ║\n", settings->wake_word.wake_phrase);
    printf("║   Threshold: %-24.2f ║\n", settings->wake_word.detection_threshold);
    printf("║   Syllables: %d-%d (expect %d)          ║\n",
           settings->wake_word.min_syllables,
           settings->wake_word.max_syllables,
           settings->wake_word.expected_syllables);
    printf("║   VAD Energy: %-23.3f ║\n", settings->wake_word.vad_energy_threshold);
    printf("║   Cooldown: %-24d ms ║\n", settings->wake_word.cooldown_ms);
    
    printf("╚═══════════════════════════════════════╝\n\n");
}
