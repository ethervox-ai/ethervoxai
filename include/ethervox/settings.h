/**
 * @file settings.h
 * @brief Persistent settings management for EthervoxAI
 *
 * Provides JSON-based configuration persistence for Whisper STT,
 * conversation parameters, and other runtime settings.
 */

#ifndef ETHERVOX_SETTINGS_H
#define ETHERVOX_SETTINGS_H

#include <stdint.h>
#include <stdbool.h>
#include "ethervox/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Whisper STT configuration settings
 */
typedef struct {
    char model_name[64];           /**< Model filename (e.g., "base.bin") */
    char language[8];              /**< Language code ("auto", "en", "es", etc.) */
    float temperature;             /**< Sampling temperature (0.0-1.0) */
    int beam_size;                 /**< Beam search width (1-10) */
    bool translate_to_english;     /**< Translate to English if true */
    int n_threads;                 /**< Number of threads (-1 = auto) */
    bool use_gpu;                  /**< Enable GPU acceleration if available */
} ethervox_whisper_settings_t;

/**
 * @brief Text-to-Speech (TTS) configuration settings
 */
typedef struct {
    char engine[32];               /**< TTS engine: "system", "piper", "none" */
    char voice[64];                /**< Default voice name/ID (deprecated, use per-language voices) */
    char voice_en[64];             /**< English voice ID */
    char voice_zh[64];             /**< Chinese voice ID */
    char voice_de[64];             /**< German voice ID */
    char voice_es[64];             /**< Spanish voice ID */
    float speed;                   /**< Speech rate (0.5-2.0, 1.0 = normal) */
    float volume;                  /**< Volume (0.0-1.0) */
    float phoneme_variance;        /**< Phoneme duration variance (0.0-1.0, 0.667 = default, higher = more natural rhythm) */
    float prosody_variance;        /**< Pitch/intonation variance (0.0-1.5, 0.8 = default, higher = more expressive) */
    char piper_model_path[256];    /**< Path to Piper model (if using Piper) */
} ethervox_tts_settings_t;

/**
 * @brief Acoustic Echo Cancellation (AEC) settings
 */
typedef struct {
    bool enabled;                  /**< Enable/disable AEC */
    char backend[16];              /**< AEC backend: "none", "speex", "webrtc" */
    float suppression_level;       /**< Echo suppression strength (0.0-1.0, 0.5 = moderate) */
    int filter_length_ms;          /**< Echo tail length in milliseconds (32-128ms) */
} ethervox_aec_settings_t;

/**
 * @brief Voice conversation configuration settings
 */
typedef struct {
    uint32_t listen_timeout_ms;        /**< Max listening duration (ms) */
    uint32_t conversation_timeout_ms;  /**< Max conversation duration (ms) */
    uint32_t silence_timeout_ms;       /**< Silence threshold to stop listening (ms) */
    float audio_energy_threshold;      /**< Minimum audio energy to process (0.0-1.0) */
    bool always_listening;             /**< Continuous STT without wake word (desktop only) */
    bool filter_hallucinations;        /**< Filter known Whisper hallucinations */
    int max_audio_chunk_size;          /**< Audio chunk size for STT (samples) */
} ethervox_conversation_settings_t;

/**
 * @brief Wake word detection settings
 */
typedef struct {
    char wake_phrase[64];          /**< Wake phrase (e.g., "hey ethervox") */
    float detection_threshold;     /**< Correlation threshold (0.0-1.0) */
    int expected_syllables;        /**< Expected syllable count */
    int min_syllables;             /**< Minimum syllables to accept */
    int max_syllables;             /**< Maximum syllables to accept */
    float vad_energy_threshold;    /**< Voice activity energy threshold */
    float vad_zcr_min;             /**< Zero-crossing rate minimum */
    float vad_zcr_max;             /**< Zero-crossing rate maximum */
    uint32_t cooldown_ms;          /**< Cooldown period after detection (ms) */
} ethervox_wake_word_settings_t;

/**
 * @brief LLM (Language Model) runtime configuration
 */
typedef struct {
    uint32_t max_tokens;           /**< Maximum tokens to generate (response length) */
    uint32_t context_length;       /**< Context window size (affects memory) */
    float temperature;             /**< Sampling temperature (0.0-2.0, higher = more creative) */
    float top_p;                   /**< Nucleus sampling threshold (0.0-1.0) */
    uint32_t seed;                 /**< Random seed for reproducibility (-1 = random) */
    uint32_t gpu_layers;           /**< Number of layers to offload to GPU (0 = CPU only) */
    int n_threads;                 /**< Number of CPU threads (-1 = auto-detect) */
} ethervox_llm_settings_t;

/**
 * @brief Governor (Tool Orchestration) configuration
 */
typedef struct {
    uint32_t max_iterations;       /**< Maximum reasoning loop iterations */
    uint32_t timeout_seconds;      /**< Maximum execution time before abort */
    float temperature;             /**< Sampling temperature for tool calls */
    uint32_t max_tokens_per_iteration; /**< Max tokens per reasoning step */
    float confidence_threshold;    /**< Minimum confidence for tool execution (0.0-1.0) */
    uint32_t gpu_layers;           /**< Number of layers to offload to GPU */
    uint32_t context_size;         /**< Context window for tool orchestration */
    int n_threads;                 /**< Number of CPU threads (-1 = auto-detect) */
} ethervox_governor_settings_t;

/**
 * @brief Complete application settings
 */
typedef struct {
    uint32_t version;                              /**< Settings format version */
    ethervox_whisper_settings_t whisper;           /**< Whisper STT settings */
    ethervox_tts_settings_t tts;                   /**< TTS engine settings */
    ethervox_aec_settings_t aec;                   /**< Acoustic echo cancellation settings */
    ethervox_conversation_settings_t conversation; /**< Conversation settings */
    ethervox_wake_word_settings_t wake_word;       /**< Wake word settings */
    ethervox_llm_settings_t llm;                   /**< LLM runtime configuration */
    ethervox_governor_settings_t governor;         /**< Governor/tool orchestration config */
} ethervox_persistent_settings_t;

/**
 * @brief Get default settings with recommended values
 * @return Default settings structure
 */
ethervox_persistent_settings_t ethervox_settings_get_defaults(void);

/**
 * @brief Load settings from JSON file
 * @param settings Output settings structure
 * @param filepath Path to JSON settings file (NULL = default location)
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_settings_load(ethervox_persistent_settings_t* settings, const char* filepath);

/**
 * @brief Save settings to JSON file
 * @param settings Settings to save
 * @param filepath Path to JSON settings file (NULL = default location)
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_settings_save(const ethervox_persistent_settings_t* settings, const char* filepath);

/**
 * @brief Get default settings file path
 * @return Path string (static buffer, do not free)
 */
const char* ethervox_settings_get_default_path(void);

/**
 * @brief Import settings from JSON string
 * @param settings Output settings structure
 * @param json_string JSON string containing settings
 * @return ETHERVOX_SUCCESS on success, error code on failure
 */
ethervox_result_t ethervox_settings_import(ethervox_persistent_settings_t* settings, const char* json_string);

/**
 * @brief Export settings to JSON string
 * @param settings Settings to export
 * @return JSON string (caller must free), or NULL on error
 */
char* ethervox_settings_export(const ethervox_persistent_settings_t* settings);

/**
 * @brief Print settings to console in human-readable format
 * @param settings Settings to display
 */
void ethervox_settings_print(const ethervox_persistent_settings_t* settings);

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_SETTINGS_H
