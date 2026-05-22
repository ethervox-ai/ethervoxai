/**
 * @file dialogue_core.c
 * @brief Core dialogue processing functionality for EthervoxAI
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 *
 * This file is part of EthervoxAI, licensed under CC BY-NC-SA 4.0.
 * You are free to share and adapt this work under the following terms:
 * - Attribution: Credit the original authors
 * - NonCommercial: Not for commercial use
 * - ShareAlike: Distribute under same license
 *
 * For full license terms, see: https://creativecommons.org/licenses/by-nc-sa/4.0/
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ethervox/error.h"
#include "ethervox/platform_utils.h"

#ifdef ETHERVOX_PLATFORM_ANDROID
#include <android/log.h>
#include <sys/system_properties.h>  // For Android properties
#endif

#include "ethervox/compute_tools.h"  // Compute tools
#include "ethervox/timer_tools.h"    // Timer tools
#include "ethervox/memory_tools.h"   // Memory tools
#include "ethervox/unit_conversion.h" // Unit conversion tools
#if HAVE_LIBCURL
#include "ethervox/weather_tools.h"  // Weather forecast tools (requires libcurl)
#endif
#include "ethervox/conversation_tools.h" // Conversation tools (speak, listen)
#include "ethervox/config.h"         // For version information
#include "ethervox/dialogue.h"
#include "ethervox/governor.h"  // Governor orchestration
#include "ethervox/tool_manifest.h"  // Tool Manifest System
#include "ethervox/llm.h"

#ifndef ETHERVOX_UNUSED
#if defined(__GNUC__)
#define ETHERVOX_UNUSED __attribute__((unused))
#else
#define ETHERVOX_UNUSED
#endif
#endif

static const float kEthervoxIntentMatchConfidence ETHERVOX_UNUSED = 0.8f;
static const float kEthervoxIntentUnknownConfidence ETHERVOX_UNUSED = 0.1f;
static const float kEthervoxResponseConfidence = 0.9f;
static const uint32_t kEthervoxResponseProcessingTimeMs = 50U;
static const uint32_t kEthervoxTokenEstimateDivisor = 4U;
static const uint32_t kEthervoxDefaultMaxContexts = 16U;
static const uint32_t kEthervoxDefaultMaxHistory = 20U;
static const float kEthervoxWakeConfidenceScale ETHERVOX_UNUSED = 2.0f;
static const size_t kEthervoxLanguageMatchPrefix = 2U;
static const size_t kEthervoxLanguageTrimIndex = 2U;

// Global memory store (set via ethervox_dialogue_set_memory_store)
static struct ethervox_memory_store_t* g_dialogue_memory_store = NULL;

static void sanitize_language_code(const char* source, char* target, size_t target_len) {
  if (!target || target_len == 0) {
    return;
  }

  const char* cursor = source;
  size_t out_pos = 0;

  while (cursor && *cursor && out_pos < target_len - 1) {
    if (*cursor == '.' || *cursor == '@') {
      break;
    }
    if (*cursor == '_' || *cursor == '-') {
      target[out_pos++] = '-';
      cursor++;
      continue;
    }
    if (isalpha((unsigned char)*cursor)) {
      target[out_pos++] = (char)tolower((unsigned char)*cursor);
    }
    cursor++;
  }

  target[out_pos] = '\0';

  if (out_pos > kEthervoxLanguageTrimIndex) {
    target[kEthervoxLanguageTrimIndex] = '\0';
  }
}

const char* ethervox_dialogue_detect_system_language(void) {
  static char cached_language[ETHERVOX_LANG_CODE_LEN] = "";
  static bool initialized = false;

  if (initialized && cached_language[0] != '\0') {
    return cached_language;
  }

  const char* env_lang = getenv("ETHERVOX_LANG");
  if (!env_lang || !*env_lang) {
    env_lang = getenv("LANG");
  }

  if (env_lang && *env_lang) {
    sanitize_language_code(env_lang, cached_language, sizeof(cached_language));
  }

  if (cached_language[0] == '\0') {
    snprintf(cached_language, sizeof(cached_language), "%s", "en");
  }

  initialized = true;
  return cached_language;
}
#include <inttypes.h>
#include <time.h>

// Forward declarations
static char* add_smart_punctuation(const char* text, const char* language_code);

// Localization string IDs
typedef enum {
  // Basic responses
  LOC_GREETING_RESPONSE,
  LOC_NAME_RESPONSE,
  LOC_CAPABILITIES_RESPONSE,
  LOC_PRIVACY_RESPONSE,
  LOC_THANK_YOU_RESPONSE,
  LOC_GOODBYE_RESPONSE,
  LOC_UNKNOWN_RESPONSE,
  LOC_CONFIRMATION_RESPONSE,
  LOC_LISTENING_RESPONSE,

  // Question-specific responses
  LOC_HOW_ARE_YOU_RESPONSE,
  LOC_OFFLINE_STATUS,
  LOC_WEATHER_NO_INTERNET,
  LOC_PRIVACY_QUESTION_RESPONSE,
  LOC_CREATOR_RESPONSE,
  LOC_PURPOSE_RESPONSE,
  LOC_LLM_UNAVAILABLE,
  LOC_COMMAND_UNDERSTOOD,

  // Math responses
  LOC_MATH_DIVISION_BY_ZERO,

  // Time/Date responses (format strings)
  LOC_WEEK_NUMBER_RESPONSE,

  LOC_STRING_COUNT
} localization_string_id_t;

// Language-specific string table
typedef struct {
  const char* lang_code;
  const char* strings[LOC_STRING_COUNT];
} language_strings_t;

static const language_strings_t LANGUAGE_STRINGS[] = {
    {"en",
     {[LOC_GREETING_RESPONSE] = "Hello! How can I help you today?",
      [LOC_NAME_RESPONSE] = "I'm EthervoxAI",
      [LOC_CAPABILITIES_RESPONSE] = "I can answer questions, provide information, and help with "
                                    "tasks. What would you like to know?",
      [LOC_PRIVACY_RESPONSE] =
          "I run completely offline on your device. I don't send any data to the cloud or "
          "internet. Everything you say stays private on your device",
      [LOC_THANK_YOU_RESPONSE] = "You're welcome! Happy to help",
      [LOC_GOODBYE_RESPONSE] = "Goodbye! Feel free to call me anytime",
      [LOC_UNKNOWN_RESPONSE] = "I'm sorry, I don't fully understand. Could you rephrase?",
      [LOC_CONFIRMATION_RESPONSE] = "Got it. What else can I help you with?",
      [LOC_LISTENING_RESPONSE] = "Yes, I'm listening. What do you need?",
      [LOC_HOW_ARE_YOU_RESPONSE] = "I'm working perfectly, thanks for asking. How can I help you?",
      [LOC_OFFLINE_STATUS] =
          "No, I work completely offline. All your information stays private on your device",
      [LOC_WEATHER_NO_INTERNET] = "I'm sorry, I don't have internet access to check the weather",
      [LOC_PRIVACY_QUESTION_RESPONSE] = "Everything stays on your device. I don't connect to the "
                                        "internet or send any data anywhere",
      [LOC_CREATOR_RESPONSE] = "I was created by the EthervoxAI team",
      [LOC_PURPOSE_RESPONSE] = "I'm here to help answer questions and assist you with various "
                               "tasks - all while keeping your data private on your device",
      [LOC_LLM_UNAVAILABLE] = "I'm sorry, I can't answer that question right now",
      [LOC_COMMAND_UNDERSTOOD] = "Understood. Executing command.",
      [LOC_MATH_DIVISION_BY_ZERO] = "Cannot divide by zero",
      [LOC_WEEK_NUMBER_RESPONSE] = "It's week %d"}},
    {"es",
     {[LOC_GREETING_RESPONSE] = "¡Hola! ¿En qué puedo ayudarte?",
      [LOC_NAME_RESPONSE] = "Soy EthervoxAI",
      [LOC_CAPABILITIES_RESPONSE] = "Puedo responder preguntas, ayudarte con información y "
                                    "controlar dispositivos. ¿En qué puedo ayudarte?",
      [LOC_PRIVACY_RESPONSE] =
          "Funciono completamente sin conexión en tu dispositivo. No envío ningún dato a la nube "
          "ni a internet. Todo lo que dices permanece privado en tu dispositivo",
      [LOC_THANK_YOU_RESPONSE] = "De nada, estoy aquí para ayudar",
      [LOC_GOODBYE_RESPONSE] = "¡Adiós! No dudes en llamarme en cualquier momento",
      [LOC_UNKNOWN_RESPONSE] = "Lo siento, no entiendo completamente. ¿Podrías reformular?",
      [LOC_CONFIRMATION_RESPONSE] = "Entendido. ¿En qué más puedo ayudarte?",
      [LOC_LISTENING_RESPONSE] = "Sí, te escucho. ¿Qué necesitas?",
      [LOC_HOW_ARE_YOU_RESPONSE] = "Estoy funcionando perfectamente, gracias por preguntar. ¿Y tú?",
      [LOC_OFFLINE_STATUS] = "No, funciono completamente sin conexión. Toda tu información "
                             "permanece privada en tu dispositivo",
      [LOC_WEATHER_NO_INTERNET] =
          "Lo siento, no tengo acceso a Internet para consultar el clima actual",
      [LOC_PRIVACY_QUESTION_RESPONSE] = "Todo permanece en tu dispositivo. No me conecto a "
                                        "internet ni envío datos a ninguna parte",
      [LOC_CREATOR_RESPONSE] = "Fui creado por el equipo de EthervoxAI",
      [LOC_PURPOSE_RESPONSE] =
          "Estoy aquí para ayudar a responder preguntas y asistirte con varias tareas, todo "
          "manteniendo tus datos privados en tu dispositivo",
      [LOC_LLM_UNAVAILABLE] = "Lo siento, no puedo responder esa pregunta en este momento",
      [LOC_COMMAND_UNDERSTOOD] = "Entendido. Ejecutando comando.",
      [LOC_MATH_DIVISION_BY_ZERO] = "No se puede dividir entre cero",
      [LOC_WEEK_NUMBER_RESPONSE] = "Estamos en la semana %d"}},
    {"zh",
     {[LOC_GREETING_RESPONSE] = "你好！我能为您做些什么？",
      [LOC_NAME_RESPONSE] = "我是EthervoxAI",
      [LOC_CAPABILITIES_RESPONSE] = "我可以回答问题、提供信息和控制设备。我能为您做什么？",
      [LOC_PRIVACY_RESPONSE] = "我完全在您的设备上离线运行。我不会向云端或互联网发送任何数据。您说"
                               "的一切都保留在您的设备上",
      [LOC_THANK_YOU_RESPONSE] = "不客气，很高兴能帮到您",
      [LOC_GOODBYE_RESPONSE] = "再见！随时欢迎您再来",
      [LOC_UNKNOWN_RESPONSE] = "抱歉，我不太理解。您能重新表述一下吗？",
      [LOC_CONFIRMATION_RESPONSE] = "好的。还有什么我可以帮您的吗？",
      [LOC_LISTENING_RESPONSE] = "是的，我在听。您需要什么？",
      [LOC_HOW_ARE_YOU_RESPONSE] = "我很好，谢谢询问。您呢？",
      [LOC_OFFLINE_STATUS] = "不，我完全离线运行。您的所有信息都保留在您的设备上",
      [LOC_WEATHER_NO_INTERNET] = "抱歉，我没有互联网访问权限来查看天气",
      [LOC_PRIVACY_QUESTION_RESPONSE] =
          "一切都保留在您的设备上。我不连接互联网，也不向任何地方发送数据",
      [LOC_CREATOR_RESPONSE] = "我由EthervoxAI团队创建",
      [LOC_PURPOSE_RESPONSE] =
          "我在这里帮助回答问题并协助您完成各种任务，同时将您的数据保密在您的设备上",
      [LOC_LLM_UNAVAILABLE] = "抱歉，我现在无法回答这个问题",
      [LOC_COMMAND_UNDERSTOOD] = "明白了。正在执行命令。",
      [LOC_MATH_DIVISION_BY_ZERO] = "不能除以零",
      [LOC_WEEK_NUMBER_RESPONSE] = "这是第%d周"}},
    {"de",
     {[LOC_GREETING_RESPONSE] = "Hallo! Wie kann ich Ihnen heute helfen?",
      [LOC_NAME_RESPONSE] = "Ich bin EthervoxAI",
      [LOC_CAPABILITIES_RESPONSE] = "Ich kann Fragen beantworten, Informationen bereitstellen und "
                                    "bei Aufgaben helfen. Womit kann ich dir helfen?",
      [LOC_PRIVACY_RESPONSE] =
          "Ich arbeite komplett offline auf Ihrem Gerät. Ich sende keine Daten in die Cloud oder "
          "ins Internet. Alles, was Sie sagen, bleibt privat auf Ihrem Gerät",
      [LOC_THANK_YOU_RESPONSE] = "Gern geschehen! Ich helfe gerne",
      [LOC_GOODBYE_RESPONSE] = "Auf Wiedersehen! Rufen Sie mich jederzeit wieder",
      [LOC_UNKNOWN_RESPONSE] =
          "Entschuldigung, ich verstehe das nicht ganz. Können Sie das umformulieren?",
      [LOC_CONFIRMATION_RESPONSE] = "Verstanden. Womit kann ich Ihnen noch helfen?",
      [LOC_LISTENING_RESPONSE] = "Ja, ich höre zu. Was brauchen Sie?",
      [LOC_HOW_ARE_YOU_RESPONSE] = "Mir geht es gut, danke der Nachfrage. Wie kann ich dir helfen?",
      [LOC_OFFLINE_STATUS] = "Nein, ich arbeite komplett offline. Alle Ihre Informationen bleiben "
                             "privat auf Ihrem Gerät",
      [LOC_WEATHER_NO_INTERNET] =
          "Entschuldigung, ich habe keinen Internetzugang, um das Wetter abzurufen",
      [LOC_PRIVACY_QUESTION_RESPONSE] = "Alles bleibt auf Ihrem Gerät. Ich verbinde mich nicht mit "
                                        "dem Internet und sende keine Daten irgendwohin",
      [LOC_CREATOR_RESPONSE] = "Ich wurde vom EthervoxAI-Team erstellt",
      [LOC_PURPOSE_RESPONSE] =
          "Ich bin hier, um Fragen zu beantworten und Sie bei verschiedenen Aufgaben zu "
          "unterstützen - alles während Ihre Daten privat auf Ihrem Gerät bleiben",
      [LOC_LLM_UNAVAILABLE] = "Es tut mir leid, ich kann diese Frage im Moment nicht beantworten",
      [LOC_COMMAND_UNDERSTOOD] = "Verstanden. Führe Befehl aus.",
      [LOC_MATH_DIVISION_BY_ZERO] = "Division durch Null ist nicht möglich",
      [LOC_WEEK_NUMBER_RESPONSE] = "Es ist Woche %d"}},
    {NULL, {NULL}}  // Sentinel
};

// Get localized string
static const char* get_localized_string(const char* lang_code, localization_string_id_t string_id) {
  if (!lang_code || string_id >= LOC_STRING_COUNT) {
    return NULL;
  }

  // Find language table
  for (int i = 0; LANGUAGE_STRINGS[i].lang_code != NULL; i++) {
    if (strcmp(LANGUAGE_STRINGS[i].lang_code, lang_code) == 0) {
      return LANGUAGE_STRINGS[i].strings[string_id];
    }
  }

  // Fallback to English
  return LANGUAGE_STRINGS[0].strings[string_id];
}

// Supported languages for MVP
static const char* SUPPORTED_LANGUAGES[] = {"en",  // English
                                            "es",  // Spanish
                                            "zh",  // Mandarin Chinese
                                            "de",  // German
                                            NULL};

// Simple intent patterns for demonstration
typedef struct {
  const char* pattern;
  ethervox_intent_type_t intent_type;
  const char* language;
  bool require_start;  // Pattern must be at start of sentence
} intent_pattern_t;

static const intent_pattern_t INTENT_PATTERNS[] = {
    // English patterns - greetings must be at start
    {"hello", ETHERVOX_INTENT_GREETING, "en", true},
    {"hi", ETHERVOX_INTENT_GREETING, "en", true},
    {"hey", ETHERVOX_INTENT_GREETING, "en", true},
    {"good morning", ETHERVOX_INTENT_GREETING, "en", true},
    {"good afternoon", ETHERVOX_INTENT_GREETING, "en", true},
    {"good evening", ETHERVOX_INTENT_GREETING, "en", true},

    // Questions can appear anywhere
    {"what is", ETHERVOX_INTENT_QUESTION, "en", false},
    {"what's", ETHERVOX_INTENT_QUESTION, "en", false},
    {"what time", ETHERVOX_INTENT_QUESTION, "en", false},
    {"what day", ETHERVOX_INTENT_QUESTION, "en", false},
    {"what date", ETHERVOX_INTENT_QUESTION, "en", false},
    {"what week", ETHERVOX_INTENT_QUESTION, "en", false},
    {"what are", ETHERVOX_INTENT_QUESTION, "en", false},
    {"how to", ETHERVOX_INTENT_QUESTION, "en", false},
    {"how do", ETHERVOX_INTENT_QUESTION, "en", false},
    {"how can", ETHERVOX_INTENT_QUESTION, "en", false},
    {"how does", ETHERVOX_INTENT_QUESTION, "en", false},
    {"where is", ETHERVOX_INTENT_QUESTION, "en", false},
    {"where are", ETHERVOX_INTENT_QUESTION, "en", false},
    {"when is", ETHERVOX_INTENT_QUESTION, "en", false},
    {"when are", ETHERVOX_INTENT_QUESTION, "en", false},
    {"who is", ETHERVOX_INTENT_QUESTION, "en", false},
    {"who are", ETHERVOX_INTENT_QUESTION, "en", false},
    {"who made", ETHERVOX_INTENT_QUESTION, "en", false},
    {"who created", ETHERVOX_INTENT_QUESTION, "en", false},
    {"who built", ETHERVOX_INTENT_QUESTION, "en", false},
    {"who developed", ETHERVOX_INTENT_QUESTION, "en", false},
    {"why is", ETHERVOX_INTENT_QUESTION, "en", false},
    {"why are", ETHERVOX_INTENT_QUESTION, "en", false},
    {"can you", ETHERVOX_INTENT_QUESTION, "en", false},
    {"do you", ETHERVOX_INTENT_QUESTION, "en", false},
    {"are you", ETHERVOX_INTENT_QUESTION, "en", false},
    {"is there", ETHERVOX_INTENT_QUESTION, "en", false},

    // Control commands - should be at start
    {"turn on", ETHERVOX_INTENT_CONTROL, "en", true},
    {"turn off", ETHERVOX_INTENT_CONTROL, "en", true},
    {"set", ETHERVOX_INTENT_COMMAND, "en", true},
    {"play", ETHERVOX_INTENT_COMMAND, "en", true},
    {"stop", ETHERVOX_INTENT_COMMAND, "en", true},
    // Goodbyes must be at start or standalone
    {"goodbye", ETHERVOX_INTENT_GOODBYE, "en", true},
    {"bye", ETHERVOX_INTENT_GOODBYE, "en", true},
    {"no", ETHERVOX_INTENT_GOODBYE, "en", true},
    {"cancel", ETHERVOX_INTENT_GOODBYE, "en", true},

    // Spanish patterns
    {"hola", ETHERVOX_INTENT_GREETING, "es", true},
    {"buenos días", ETHERVOX_INTENT_GREETING, "es", true},
    {"buenas tardes", ETHERVOX_INTENT_GREETING, "es", true},
    {"buenas noches", ETHERVOX_INTENT_GREETING, "es", true},
    {"qué es", ETHERVOX_INTENT_QUESTION, "es", false},
    {"qué hora", ETHERVOX_INTENT_QUESTION, "es", false},
    {"qué día", ETHERVOX_INTENT_QUESTION, "es", false},
    {"qué fecha", ETHERVOX_INTENT_QUESTION, "es", false},
    {"cuál es", ETHERVOX_INTENT_QUESTION, "es", false},
    {"cuándo", ETHERVOX_INTENT_QUESTION, "es", false},
    {"dónde", ETHERVOX_INTENT_QUESTION, "es", false},
    {"quién", ETHERVOX_INTENT_QUESTION, "es", false},
    {"quién creó", ETHERVOX_INTENT_QUESTION, "es", false},
    {"quién hizo", ETHERVOX_INTENT_QUESTION, "es", false},
    {"por qué", ETHERVOX_INTENT_QUESTION, "es", false},
    {"cómo", ETHERVOX_INTENT_QUESTION, "es", false},
    {"puedes", ETHERVOX_INTENT_QUESTION, "es", false},
    {"podrías", ETHERVOX_INTENT_QUESTION, "es", false},
    {"eres", ETHERVOX_INTENT_QUESTION, "es", false},
    {"estás", ETHERVOX_INTENT_QUESTION, "es", false},
    {"encender", ETHERVOX_INTENT_CONTROL, "es", true},
    {"apagar", ETHERVOX_INTENT_CONTROL, "es", true},
    {"reproducir", ETHERVOX_INTENT_COMMAND, "es", true},
    {"parar", ETHERVOX_INTENT_COMMAND, "es", true},
    {"adiós", ETHERVOX_INTENT_GOODBYE, "es", true},
    {"hasta luego", ETHERVOX_INTENT_GOODBYE, "es", true},
    {"chao", ETHERVOX_INTENT_GOODBYE, "es", true},

    // Chinese patterns (simplified)
    {"你好", ETHERVOX_INTENT_GREETING, "zh", true},
    {"早上好", ETHERVOX_INTENT_GREETING, "zh", true},
    {"下午好", ETHERVOX_INTENT_GREETING, "zh", true},
    {"晚上好", ETHERVOX_INTENT_GREETING, "zh", true},
    {"什么是", ETHERVOX_INTENT_QUESTION, "zh", false},
    {"什么时候", ETHERVOX_INTENT_QUESTION, "zh", false},
    {"几点", ETHERVOX_INTENT_QUESTION, "zh", false},
    {"现在几点", ETHERVOX_INTENT_QUESTION, "zh", false},
    {"今天几号", ETHERVOX_INTENT_QUESTION, "zh", false},
    {"哪里", ETHERVOX_INTENT_QUESTION, "zh", false},
    {"什么地方", ETHERVOX_INTENT_QUESTION, "zh", false},
    {"谁", ETHERVOX_INTENT_QUESTION, "zh", false},
    {"谁创造", ETHERVOX_INTENT_QUESTION, "zh", false},
    {"谁开发", ETHERVOX_INTENT_QUESTION, "zh", false},
    {"为什么", ETHERVOX_INTENT_QUESTION, "zh", false},
    {"怎么", ETHERVOX_INTENT_QUESTION, "zh", false},
    {"怎么样", ETHERVOX_INTENT_QUESTION, "zh", false},
    {"如何", ETHERVOX_INTENT_QUESTION, "zh", false},
    {"能不能", ETHERVOX_INTENT_QUESTION, "zh", false},
    {"可以", ETHERVOX_INTENT_QUESTION, "zh", false},
    {"你是", ETHERVOX_INTENT_QUESTION, "zh", false},
    {"打开", ETHERVOX_INTENT_CONTROL, "zh", true},
    {"关闭", ETHERVOX_INTENT_CONTROL, "zh", true},
    {"播放", ETHERVOX_INTENT_COMMAND, "zh", true},
    {"停止", ETHERVOX_INTENT_COMMAND, "zh", true},
    {"再见", ETHERVOX_INTENT_GOODBYE, "zh", true},
    {"拜拜", ETHERVOX_INTENT_GOODBYE, "zh", true},

    // German patterns
    {"hallo", ETHERVOX_INTENT_GREETING, "de", true},
    {"guten morgen", ETHERVOX_INTENT_GREETING, "de", true},
    {"guten tag", ETHERVOX_INTENT_GREETING, "de", true},
    {"guten abend", ETHERVOX_INTENT_GREETING, "de", true},
    {"was ist", ETHERVOX_INTENT_QUESTION, "de", false},
    {"was sind", ETHERVOX_INTENT_QUESTION, "de", false},
    {"wie viel", ETHERVOX_INTENT_QUESTION, "de", false},
    {"welche", ETHERVOX_INTENT_QUESTION, "de", false},
    {"wann", ETHERVOX_INTENT_QUESTION, "de", false},
    {"wo", ETHERVOX_INTENT_QUESTION, "de", false},
    {"wer", ETHERVOX_INTENT_QUESTION, "de", false},
    {"warum", ETHERVOX_INTENT_QUESTION, "de", false},
    {"wie", ETHERVOX_INTENT_QUESTION, "de", false},
    {"kannst du", ETHERVOX_INTENT_QUESTION, "de", false},
    {"können sie", ETHERVOX_INTENT_QUESTION, "de", false},
    {"einschalten", ETHERVOX_INTENT_CONTROL, "de", true},
    {"ausschalten", ETHERVOX_INTENT_CONTROL, "de", true},
    {"abspielen", ETHERVOX_INTENT_COMMAND, "de", true},
    {"stopp", ETHERVOX_INTENT_COMMAND, "de", true},
    {"auf wiedersehen", ETHERVOX_INTENT_GOODBYE, "de", true},
    {"tschüss", ETHERVOX_INTENT_GOODBYE, "de", true},

    {NULL, ETHERVOX_INTENT_UNKNOWN, NULL, false}  // Sentinel
};

// Intent type to string mapping
const char* ethervox_intent_type_to_string(ethervox_intent_type_t type) {
  switch (type) {
    case ETHERVOX_INTENT_GREETING:
      return "greeting";
    case ETHERVOX_INTENT_QUESTION:
      return "question";
    case ETHERVOX_INTENT_COMMAND:
      return "command";
    case ETHERVOX_INTENT_REQUEST:
      return "request";
    case ETHERVOX_INTENT_INFORMATION:
      return "information";
    case ETHERVOX_INTENT_CONTROL:
      return "control";
    case ETHERVOX_INTENT_GOODBYE:
      return "goodbye";
    case ETHERVOX_INTENT_UNKNOWN:
    default:
      return "unknown";
  }
}

// Entity type to string mapping
const char* ethervox_entity_type_to_string(ethervox_entity_type_t type) {
  switch (type) {
    case ETHERVOX_ENTITY_PERSON:
      return "person";
    case ETHERVOX_ENTITY_LOCATION:
      return "location";
    case ETHERVOX_ENTITY_TIME:
      return "time";
    case ETHERVOX_ENTITY_NUMBER:
      return "number";
    case ETHERVOX_ENTITY_DEVICE:
      return "device";
    case ETHERVOX_ENTITY_ACTION:
      return "action";
    default:
      return "unknown";
  }
}

// Default LLM configuration
ethervox_llm_config_t ethervox_dialogue_get_default_llm_config(void) {
  ethervox_llm_config_t config = {.model_path = NULL,  // Will be set based on platform
                                  .model_name = ETHERVOX_LLM_MODEL_NAME,
                                  .max_tokens = ETHERVOX_LLM_MAX_TOKENS_DEFAULT,
                                  .context_length = ETHERVOX_LLM_CONTEXT_LENGTH_DEFAULT,
                                  .temperature = ETHERVOX_LLM_TEMPERATURE_DEFAULT,
                                  .top_p = ETHERVOX_LLM_TOP_P_DEFAULT,
                                  .seed = ETHERVOX_LLM_SEED_DEFAULT,
                                  .use_gpu = true,  // Enable GPU acceleration by default
                                  .gpu_layers = ETHERVOX_LLM_GPU_LAYERS_DEFAULT,
                                  .language_code = NULL};

#ifdef ETHERVOX_PLATFORM_ANDROID
  config.max_tokens = ETHERVOX_LLM_MAX_TOKENS_ANDROID;
  config.context_length = ETHERVOX_LLM_CONTEXT_LENGTH_ANDROID;
  config.use_gpu = true;
  config.gpu_layers = ETHERVOX_LLM_GPU_LAYERS_ANDROID;
#elif defined(ETHERVOX_PLATFORM_DESKTOP)
  config.max_tokens = ETHERVOX_LLM_MAX_TOKENS_DESKTOP;
  config.context_length = ETHERVOX_LLM_CONTEXT_LENGTH_DESKTOP;
  config.use_gpu = true;
  config.gpu_layers = ETHERVOX_LLM_GPU_LAYERS_DESKTOP;
#endif

  return config;
}

// Check if language is supported
bool ethervox_dialogue_is_language_supported(const char* language_code) {
  if (!language_code) {
    return false;
  }

  for (int i = 0; SUPPORTED_LANGUAGES[i] != NULL; i++) {
    if (strncmp(language_code, SUPPORTED_LANGUAGES[i], kEthervoxLanguageMatchPrefix) == 0) {
      return true;
    }
  }

  return false;
}

// Get list of supported languages
const char** ethervox_dialogue_get_supported_languages(void) {
  return SUPPORTED_LANGUAGES;
}

// Generate conversation ID
static char* generate_conversation_id(void) {
  static uint32_t counter = 0;
  char* id = (char*)malloc(32);
  snprintf(id, 32, "conv_%" PRIu32 "_%lu", ++counter, (unsigned long)time(NULL));
  return id;
}

/**
 * Generate user-friendly display name from timestamp
 * 
 * Current implementation: Timestamp-based naming (Option C)
 * Format: "Apr 8, 3:45 PM"
 * 
 * Future enhancement: Can be upgraded to topic-based naming (Option A) by:
 * 1. Detecting first real user message (ignoring silent startup prompts)
 * 2. Extracting key topics or using first 30 chars of user's message
 * 3. Optionally using LLM to generate smart titles after 3-5 turns
 * 
 * @return Allocated string with display name (caller must free)
 */
static char* generate_display_name(void) {
  char* name = (char*)malloc(64);
  if (!name) {
    return strdup("New Conversation");
  }
  
  time_t now = time(NULL);
  struct tm* tm_info = localtime(&now);
  
  if (!tm_info) {
    free(name);
    return strdup("New Conversation");
  }
  
  // Format: "Apr 8, 3:45 PM"
  strftime(name, 64, "%b %d, %I:%M %p", tm_info);
  return name;
}

// Initialize dialogue engine
ethervox_result_t ethervox_dialogue_init(ethervox_dialogue_engine_t* engine,
                           const ethervox_llm_config_t* config) {
  if (!engine) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  memset(engine, 0, sizeof(ethervox_dialogue_engine_t));

  // Copy configuration
  if (config) {
#ifdef ETHERVOX_PLATFORM_ANDROID
    ETHERVOX_LOGI("[DEBUG] dialogue_init config: model_path=%s", config->model_path ? config->model_path : "NULL");
#endif
    engine->llm_config = *config;
    if (config->model_path) {
      engine->llm_config.model_path = strdup(config->model_path);
#ifdef ETHERVOX_PLATFORM_ANDROID
      ETHERVOX_LOGI("[DEBUG] After strdup: engine->llm_config.model_path=%s", engine->llm_config.model_path);
#endif
    }
    if (config->model_name) {
      engine->llm_config.model_name = strdup(config->model_name);
    }
  } else {
    engine->llm_config = ethervox_dialogue_get_default_llm_config();
  }

  const char* language_source = NULL;
  if (config && config->language_code) {
    language_source = config->language_code;
  } else if (engine->llm_config.language_code) {
    language_source = engine->llm_config.language_code;
  } else {
    language_source = ethervox_dialogue_detect_system_language();
  }

  engine->llm_config.language_code = NULL;
  if (language_source) {
    engine->llm_config.language_code = strdup(language_source);
  }

  // Allocate context storage
  engine->max_contexts =
      kEthervoxDefaultMaxContexts;  // Support up to 16 simultaneous conversations
  engine->contexts = (ethervox_dialogue_context_t*)calloc(engine->max_contexts,
                                                          sizeof(ethervox_dialogue_context_t));
  if (!engine->contexts) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Initialize intent patterns (simplified - in production would load from files)
  engine->intent_patterns = (void*)INTENT_PATTERNS;

  // Initialize LLM backend if model path provided
  engine->llm_backend = NULL;
  engine->use_llm_for_unknown = false;

#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE
  if (config && config->model_path) {
#ifdef ETHERVOX_PLATFORM_ANDROID
    ETHERVOX_LOGI(
                        "Initializing LLM backend with model: %s", config->model_path);
#else
    printf("Initializing LLM backend with model: %s\n", config->model_path);
#endif

    // Create llama backend
    ethervox_llm_backend_t* backend = ethervox_llm_create_llama_backend();
    if (backend) {
      // Initialize backend
      ethervox_result_t result = ethervox_llm_backend_init(backend, config);
      if (ethervox_is_success(result)) {
        // Load model
        result = ethervox_llm_backend_load_model(backend, config->model_path);
        if (ethervox_is_success(result)) {
          engine->llm_backend = backend;
          engine->use_llm_for_unknown = true;
#ifdef ETHERVOX_PLATFORM_ANDROID
          ETHERVOX_LOGI(
                              "LLM backend initialized successfully");
#else
          printf("LLM backend initialized successfully\n");
#endif
        } else {
#ifdef ETHERVOX_PLATFORM_ANDROID
          ETHERVOX_LOGE( "Failed to load LLM model: %d",
                              result);
#else
          printf("Failed to load LLM model: %d\n", result);
#endif
          ethervox_llm_backend_cleanup(backend);
          ethervox_llm_backend_free(backend);
        }
      } else {
#ifdef ETHERVOX_PLATFORM_ANDROID
        ETHERVOX_LOGE(
                            "Failed to initialize LLM backend: %d", result);
#else
        printf("Failed to initialize LLM backend: %d\n", result);
#endif
        ethervox_llm_backend_free(backend);
      }
    } else {
#ifdef ETHERVOX_PLATFORM_ANDROID
      ETHERVOX_LOGE( "Failed to create LLM backend");
#else
      printf("Failed to create LLM backend\n");
#endif
    }
  }
#else
  if (config && config->model_path) {
#ifdef ETHERVOX_PLATFORM_ANDROID
    ETHERVOX_LOGW(
                        "LLM support not compiled in (LLAMA_CPP_AVAILABLE not defined)");
#else
    printf("LLM support not compiled in (LLAMA_CPP_AVAILABLE not defined)\n");
#endif
  }
#endif

  // Initialize Governor with compute tools
  engine->governor = NULL;
  engine->governor_tool_registry = NULL;
  engine->use_governor = false;

#ifdef ETHERVOX_PLATFORM_ANDROID
  ETHERVOX_LOGI(
                      "Initializing Governor with compute tools...");
#else
  printf("Initializing Governor with compute tools...\\n");
#endif

  // Create tool registry
  ethervox_tool_registry_t* registry = malloc(sizeof(ethervox_tool_registry_t));
  if (registry && ethervox_tool_registry_init(registry, 16) == 0) {
    // Register compute tools
    int tool_count = ethervox_compute_tools_register_all(registry);
    
    // Register timer tools
    ethervox_tool_registry_add(registry, ethervox_tool_timer_create());
    ethervox_tool_registry_add(registry, ethervox_tool_timer_cancel());
    ethervox_tool_registry_add(registry, ethervox_tool_timer_list());
    ethervox_tool_registry_add(registry, ethervox_tool_alarm_create());
    tool_count += 4;
    
    // Register unit conversion tool
    if (ethervox_unit_conversion_register(registry) == 0) {
      tool_count++;
#ifdef ETHERVOX_PLATFORM_ANDROID
      ETHERVOX_LOGI("Registered unit conversion tool with Governor");
#else
      printf("Registered unit conversion tool with Governor\n");
#endif
    }
    
#if HAVE_LIBCURL
    // Register weather forecast tool (requires libcurl)
#ifdef ETHERVOX_PLATFORM_ANDROID
    ETHERVOX_LOGI("Attempting to register weather tools...");
#else
    printf("Attempting to register weather tools...\n");
#endif
    ethervox_result_t weather_result = ethervox_weather_tools_register(registry);
#ifdef ETHERVOX_PLATFORM_ANDROID
    ETHERVOX_LOGI("Weather registration returned: %d", weather_result);
#else
    printf("Weather registration returned: %d\n", weather_result);
#endif
    if (weather_result == 0) {
      tool_count++;
#ifdef ETHERVOX_PLATFORM_ANDROID
      ETHERVOX_LOGI("Registered weather forecast tool with Governor");
#else
      printf("Registered weather forecast tool with Governor\n");
#endif
    } else {
#ifdef ETHERVOX_PLATFORM_ANDROID
      ETHERVOX_LOGE("Failed to register weather tool, error: %d", weather_result);
#else
      printf("Failed to register weather tool, error: %d\n", weather_result);
#endif
    }
#else
#ifdef ETHERVOX_PLATFORM_ANDROID
    ETHERVOX_LOGW("HAVE_LIBCURL not defined - weather tools skipped");
#else
    printf("HAVE_LIBCURL not defined - weather tools skipped\n");
#endif
#endif
    
    // Register conversation tools (speak, listen for LLM-controlled voice interaction)
    ethervox_result_t conv_tools_result = ethervox_conversation_tools_register(registry);
    if (ethervox_is_success(conv_tools_result)) {
      tool_count += 2;  // speak and listen
#ifdef ETHERVOX_PLATFORM_ANDROID
      ETHERVOX_LOGI("Registered conversation tools (speak, listen) with Governor");
#else
      printf("Registered conversation tools (speak, listen) with Governor\n");
#endif
    } else {
#ifdef ETHERVOX_PLATFORM_ANDROID
      ETHERVOX_LOGE("Failed to register conversation tools - error code: %d", conv_tools_result);
#else
      printf("Failed to register conversation tools - error code: %d\n", conv_tools_result);
#endif
    }
    
    // Register memory tools if memory store is available
    if (g_dialogue_memory_store) {
      ethervox_result_t memory_tools = ethervox_memory_tools_register(registry, (ethervox_memory_store_t*)g_dialogue_memory_store);
      if (ethervox_is_success(memory_tools)) {
        tool_count += 6;  // 6 memory tools registered
#ifdef ETHERVOX_PLATFORM_ANDROID
        ETHERVOX_LOGI("Registered memory tools with Governor");
#else
        printf("Registered memory tools with Governor\n");
#endif
      } else {
#ifdef ETHERVOX_PLATFORM_ANDROID
        ETHERVOX_LOGE("Failed to register memory tools");
#endif
      }
    } else {
#ifdef ETHERVOX_PLATFORM_ANDROID
      ETHERVOX_LOGW("Memory store not initialized - skipping memory tools registration");
#endif
    }

#ifdef ETHERVOX_PLATFORM_ANDROID
    ETHERVOX_LOGI( "Registered %d tools total",
                        tool_count);
#else
    printf("Registered %d tools total\\n", tool_count);
#endif

    // Initialize Governor
    ethervox_governor_t* governor = NULL;
    ethervox_governor_config_t gov_config = ethervox_governor_default_config();

    if (ethervox_governor_init(&governor, &gov_config, registry) == 0) {
      engine->governor = governor;
      engine->governor_tool_registry = registry;
      engine->use_governor = true;

#ifdef ETHERVOX_PLATFORM_ANDROID
      ETHERVOX_LOGI("Governor initialized successfully");
#else
      printf("Governor initialized successfully\\n");
#endif

      // Manifest registry will be initialized after model is loaded
      // (see loadGovernorModel JNI function for Android platform)
      engine->manifest_registry = NULL;
      
    } else {
#ifdef ETHERVOX_PLATFORM_ANDROID
      ETHERVOX_LOGE( "Failed to initialize Governor");
#endif
      ethervox_tool_registry_cleanup(registry);
      free(registry);
    }
  } else {
#ifdef ETHERVOX_PLATFORM_ANDROID
    ETHERVOX_LOGE( "Failed to create tool registry");
#endif
    if (registry)
      free(registry);
  }

  engine->is_initialized = true;
  printf("Dialogue engine initialized with %s model\n",
         engine->llm_config.model_name ? engine->llm_config.model_name : "default");

  return ETHERVOX_SUCCESS;
}

// Cleanup dialogue engine
void ethervox_dialogue_cleanup(ethervox_dialogue_engine_t* engine) {
  if (!engine) {
    return;
  }

  // Cleanup Governor
  if (engine->governor) {
    ethervox_governor_cleanup((ethervox_governor_t*)engine->governor);
    engine->governor = NULL;
  }
  
  // Cleanup tool registry
  if (engine->governor_tool_registry) {
    ethervox_tool_registry_cleanup((ethervox_tool_registry_t*)engine->governor_tool_registry);
    free(engine->governor_tool_registry);
    engine->governor_tool_registry = NULL;
  }
  
  // Cleanup manifest registry
  if (engine->manifest_registry) {
    ethervox_tool_manifest_cleanup((tool_manifest_registry_t*)engine->manifest_registry);
    free(engine->manifest_registry);
    engine->manifest_registry = NULL;
  }

  // Cleanup contexts
  if (engine->contexts) {
    for (uint32_t i = 0; i < engine->max_contexts; i++) {
      ethervox_dialogue_context_t* ctx = &engine->contexts[i];
      if (ctx->conversation_id) {
        free(ctx->conversation_id);
      }
      if (ctx->display_name) {
        free(ctx->display_name);
      }
      free(ctx->user_id);

      if (ctx->conversation_history) {
        for (uint32_t j = 0; j < ctx->history_count; j++) {
          ethervox_intent_free(&ctx->conversation_history[j]);
        }
        free(ctx->conversation_history);
      }
    }
    free(engine->contexts);
  }

  // Cleanup LLM config
  if (engine->llm_config.model_path) {
    free(engine->llm_config.model_path);
  }
  if (engine->llm_config.model_name) {
    free(engine->llm_config.model_name);
  }
  if (engine->llm_config.language_code) {
    free(engine->llm_config.language_code);
    engine->llm_config.language_code = NULL;
  }

  // Cleanup LLM backend
#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE
  if (engine->llm_backend) {
    ethervox_llm_backend_t* backend = (ethervox_llm_backend_t*)engine->llm_backend;
    ethervox_llm_backend_cleanup(backend);
    ethervox_llm_backend_free(backend);
    engine->llm_backend = NULL;
  }
#else
  engine->llm_backend = NULL;
#endif

  engine->is_initialized = false;
  printf("Dialogue engine cleaned up\n");
}

// Parse intent from text
ethervox_result_t ethervox_dialogue_parse_intent(ethervox_dialogue_engine_t* engine,
                                   const ethervox_dialogue_intent_request_t* request,
                                   ethervox_intent_t* intent) {
  if (!engine || !request || !request->text || !intent) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  const char* language_code = request->language_code;
  if (!language_code || language_code[0] == '\0') {
    language_code = engine->llm_config.language_code ? engine->llm_config.language_code : "en";
  }

  // Normalize language code to just first 2 characters (en-US -> en)
  char lang_normalized[8] = {0};
  size_t lang_copy_len = strlen(language_code);
  if (lang_copy_len > 2) {
    lang_copy_len = 2;  // Only take first 2 chars
  }
  strncpy(lang_normalized, language_code, lang_copy_len);
  lang_normalized[lang_copy_len] = '\0';

  // Convert to lowercase
  for (size_t i = 0; i < lang_copy_len; i++) {
    lang_normalized[i] = (char)tolower((unsigned char)lang_normalized[i]);
  }

  const char* text = request->text;

  memset(intent, 0, sizeof(ethervox_intent_t));

  // Add smart punctuation if missing (helps LLM understand context)
  char* punctuated_text = add_smart_punctuation(text, lang_normalized);
  const char* final_text = punctuated_text ? punctuated_text : text;

  // Copy input text (use punctuated version)
  intent->raw_text = strdup(final_text);

  // Normalize text to lowercase for pattern matching
  size_t text_len = strlen(final_text);
  char* normalized = (char*)malloc(text_len + 1);
  for (size_t i = 0; i < text_len; i++) {
    normalized[i] = (char)tolower((unsigned char)final_text[i]);
  }
  normalized[text_len] = '\0';
  intent->normalized_text = normalized;

  snprintf(intent->language_code, sizeof(intent->language_code), "%s", lang_normalized);

  // Simple pattern matching for intent detection
  intent->type = ETHERVOX_INTENT_UNKNOWN;
  intent->confidence = 0.0f;

  const intent_pattern_t* patterns = (const intent_pattern_t*)engine->intent_patterns;

  for (int i = 0; patterns[i].pattern != NULL; i++) {
    // Check if pattern matches language (use normalized language code)
    if (strcmp(patterns[i].language, lang_normalized) != 0) {
      continue;
    }

    // Check pattern position requirement
    const char* match_pos = strstr(normalized, patterns[i].pattern);
    if (match_pos != NULL) {
      // If pattern requires start of sentence, check position
      if (patterns[i].require_start) {
        // Pattern must be at the very beginning (after trimming whitespace)
        if (match_pos != normalized) {
          continue;  // Not at start, skip this pattern
        }
      }
      // Pattern matched!
      intent->type = patterns[i].intent_type;
      intent->confidence = 0.8f;  // Fixed confidence for demo
      break;
    }
  }

  // If no pattern matched, use unknown intent with low confidence
  if (intent->type == ETHERVOX_INTENT_UNKNOWN) {
    intent->confidence = 0.1f;
  }

  printf("Intent parsed: %s (confidence: %.2f) for text: '%s' [lang: %s]\n",
         ethervox_intent_type_to_string(intent->type), intent->confidence, final_text,
         lang_normalized);

  // Free punctuated text if it was allocated
  if (punctuated_text) {
    free(punctuated_text);
  }

  return ETHERVOX_SUCCESS;
}

// Infer punctuation for unpunctuated STT output
static char* add_smart_punctuation(const char* text, const char* language_code) {
  if (!text || !text[0]) {
    return NULL;
  }

  size_t len = strlen(text);
  // Allocate buffer (original + potential punctuation + null terminator)
  char* punctuated = (char*)malloc(len + 10);
  if (!punctuated) {
    return NULL;
  }

  // Copy original text
  strcpy(punctuated, text);

  // Check if already has ending punctuation (ASCII)
  char last_char = text[len - 1];
  if (last_char == '.' || last_char == '?' || last_char == '!') {
    free(punctuated);
    return NULL;  // Already punctuated, return NULL to use original
  }

  // Check for multi-byte Chinese punctuation at end
  if (len >= 3) {
    const char* end = text + len - 3;
    // Chinese period 。 (E3 80 82), question mark ？ (EF BC 9F), exclamation ！ (EF BC 81)
    if (memcmp(end, "\xE3\x80\x82", 3) == 0 || memcmp(end, "\xEF\xBC\x9F", 3) == 0 ||
        memcmp(end, "\xEF\xBC\x81", 3) == 0) {
      free(punctuated);
      return NULL;  // Already has Chinese punctuation
    }
  }

  // Convert to lowercase for pattern matching
  char* lower = (char*)malloc(len + 1);
  if (!lower) {
    free(punctuated);
    return NULL;
  }
  for (size_t i = 0; i < len; i++) {
    lower[i] = (char)tolower((unsigned char)text[i]);
  }
  lower[len] = '\0';

  // Detect questions by interrogative words at the start
  bool is_question = false;

  // English question words
  if (strncmp(lower, "what ", 5) == 0 || strncmp(lower, "what's ", 7) == 0 ||
      strncmp(lower, "where ", 6) == 0 || strncmp(lower, "where's ", 8) == 0 ||
      strncmp(lower, "when ", 5) == 0 || strncmp(lower, "when's ", 7) == 0 ||
      strncmp(lower, "who ", 4) == 0 || strncmp(lower, "who's ", 6) == 0 ||
      strncmp(lower, "why ", 4) == 0 || strncmp(lower, "how ", 4) == 0 ||
      strncmp(lower, "which ", 6) == 0 || strncmp(lower, "whose ", 6) == 0 ||
      strncmp(lower, "can you ", 8) == 0 || strncmp(lower, "could you ", 10) == 0 ||
      strncmp(lower, "would you ", 10) == 0 || strncmp(lower, "will you ", 9) == 0 ||
      strncmp(lower, "do you ", 7) == 0 || strncmp(lower, "does ", 6) == 0 ||
      strncmp(lower, "did you ", 8) == 0 || strncmp(lower, "are you ", 8) == 0 ||
      strncmp(lower, "is ", 3) == 0 || strncmp(lower, "was ", 4) == 0 ||
      strncmp(lower, "were ", 5) == 0 || strncmp(lower, "have you ", 9) == 0 ||
      strncmp(lower, "has ", 4) == 0 || strncmp(lower, "should ", 7) == 0) {
    is_question = true;
  }

  // Spanish question words
  if (strcmp(language_code, "es") == 0) {
    if (strncmp(lower, "qué ", 5) == 0 || strncmp(lower, "que ", 4) == 0 ||
        strncmp(lower, "dónde ", 7) == 0 || strncmp(lower, "donde ", 6) == 0 ||
        strncmp(lower, "cuándo ", 8) == 0 || strncmp(lower, "cuando ", 7) == 0 ||
        strncmp(lower, "quién ", 7) == 0 || strncmp(lower, "quien ", 6) == 0 ||
        strncmp(lower, "por qué ", 9) == 0 || strncmp(lower, "cómo ", 6) == 0 ||
        strncmp(lower, "como ", 5) == 0 || strncmp(lower, "cuál ", 6) == 0 ||
        strncmp(lower, "puedes ", 7) == 0 || strncmp(lower, "podrías ", 9) == 0) {
      is_question = true;
    }
  }

  // German question words
  if (strcmp(language_code, "de") == 0) {
    if (strncmp(lower, "was ", 4) == 0 || strncmp(lower, "wer ", 4) == 0 ||
        strncmp(lower, "wo ", 3) == 0 || strncmp(lower, "wann ", 5) == 0 ||
        strncmp(lower, "warum ", 6) == 0 || strncmp(lower, "wie ", 4) == 0 ||
        strncmp(lower, "welche ", 7) == 0 || strncmp(lower, "welcher ", 8) == 0 ||
        strncmp(lower, "welches ", 8) == 0 || strncmp(lower, "können ", 8) == 0 ||
        strncmp(lower, "kannst ", 7) == 0 || strncmp(lower, "hast ", 5) == 0 ||
        strncmp(lower, "haben ", 6) == 0 || strncmp(lower, "bist ", 5) == 0 ||
        strncmp(lower, "sind ", 5) == 0 || strncmp(lower, "ist ", 4) == 0) {
      is_question = true;
    }
  }

  // Chinese question particles (usually at end, but STT might miss them)
  if (strcmp(language_code, "zh") == 0) {
    if (strstr(lower, "什么") != NULL || strstr(lower, "哪里") != NULL ||
        strstr(lower, "什么时候") != NULL || strstr(lower, "谁") != NULL ||
        strstr(lower, "为什么") != NULL || strstr(lower, "怎么") != NULL ||
        strstr(lower, "如何") != NULL || strstr(lower, "哪个") != NULL ||
        strstr(lower, "能不能") != NULL || strstr(lower, "可以") != NULL ||
        strstr(lower, "是不是") != NULL || strstr(lower, "吗") != NULL) {
      is_question = true;
    }
  }

  free(lower);

  // Add appropriate punctuation
  if (is_question) {
    if (strcmp(language_code, "zh") == 0) {
      strcat(punctuated, "\xEF\xBC\x9F");  // Chinese full-width question mark ？
    } else {
      strcat(punctuated, "?");
    }
  } else {
    // Default to period for statements
    if (strcmp(language_code, "zh") == 0) {
      strcat(punctuated, "\xE3\x80\x82");  // Chinese period 。
    } else {
      strcat(punctuated, ".");
    }
  }

  return punctuated;
}

// Simple question answering for common queries
static const char* answer_simple_question(const char* normalized_text, const char* language_code) {
  if (!normalized_text || !language_code) {
    return NULL;
  }

  // Math calculations - addition, subtraction, multiplication, division
  // Pattern: "what is X plus/minus/times/divided by Y" or German "was ist X plus/minus/mal/geteilt
  // durch Y"
  static char math_response[256];
  const char* operators[] = {
      "plus", "minus", "times", "multiplied by", "divided by",   "+", "-",
      "*",    "x",     "/",     "mal",           "geteilt durch"  // German operators
  };

  for (int i = 0; i < 12; i++) {
    const char* op_pos = strstr(normalized_text, operators[i]);
    if (op_pos != NULL) {
      // Try to extract numbers before and after operator
      double num1 = 0, num2 = 0;
      const char* start = normalized_text;

      // Find "what is" or similar prefix (English and German)
      const char* what_is = strstr(start, "what is");
      const char* what_s = strstr(start, "what's");
      const char* calculate = strstr(start, "calculate");
      const char* was_ist = strstr(start, "was ist");
      const char* rechne = strstr(start, "rechne");

      if (what_is)
        start = what_is + 7;
      else if (what_s)
        start = what_s + 6;
      else if (calculate)
        start = calculate + 9;
      else if (was_ist)
        start = was_ist + 7;
      else if (rechne)
        start = rechne + 6;

      // Parse first number
      while (*start && !isdigit(*start) && *start != '-')
        start++;
      if (sscanf(start, "%lf", &num1) == 1) {
        // Parse second number (after operator)
        const char* after_op = op_pos + strlen(operators[i]);
        while (*after_op && !isdigit(*after_op) && *after_op != '-')
          after_op++;
        if (sscanf(after_op, "%lf", &num2) == 1) {
          double result = 0;
          bool valid = true;

          // Perform calculation
          if (strcmp(operators[i], "plus") == 0 || strcmp(operators[i], "+") == 0) {
            result = num1 + num2;
          } else if (strcmp(operators[i], "minus") == 0 || strcmp(operators[i], "-") == 0) {
            result = num1 - num2;
          } else if (strcmp(operators[i], "times") == 0 ||
                     strcmp(operators[i], "multiplied by") == 0 || strcmp(operators[i], "*") == 0 ||
                     strcmp(operators[i], "x") == 0 || strcmp(operators[i], "mal") == 0) {
            result = num1 * num2;
          } else if (strcmp(operators[i], "divided by") == 0 || strcmp(operators[i], "/") == 0 ||
                     strcmp(operators[i], "geteilt durch") == 0) {
            if (num2 != 0) {
              result = num1 / num2;
            } else {
              valid = false;
              return get_localized_string(language_code, LOC_MATH_DIVISION_BY_ZERO);
            }
          }

          if (valid) {
            // Format response - just return the number, language-agnostic
            snprintf(math_response, sizeof(math_response), "%.2f", result);
            return math_response;
          }
        }
      }
      break;  // Found an operator, don't check others
    }
  }

  // Time-related questions
  if (strstr(normalized_text, "what time is it") != NULL ||
      strstr(normalized_text, "what's the time") != NULL ||
      strstr(normalized_text, "tell me the time") != NULL ||
      strstr(normalized_text, "wie spät") != NULL ||
      strstr(normalized_text, "wie viel uhr") != NULL) {
    time_t now = time(NULL);
    struct tm* local = localtime(&now);
    static char time_response[128];

    if (strcmp(language_code, "es") == 0) {
      snprintf(time_response, sizeof(time_response), "Son las %d:%02d", local->tm_hour,
               local->tm_min);
    } else if (strcmp(language_code, "zh") == 0) {
      snprintf(time_response, sizeof(time_response), "现在是%d点%02d分", local->tm_hour,
               local->tm_min);
    } else if (strcmp(language_code, "de") == 0) {
      snprintf(time_response, sizeof(time_response), "Es ist %d:%02d Uhr", local->tm_hour,
               local->tm_min);
    } else {
      int hour = local->tm_hour;
      const char* period = "AM";
      if (hour >= 12) {
        period = "PM";
        if (hour > 12)
          hour -= 12;
      }
      if (hour == 0)
        hour = 12;
      snprintf(time_response, sizeof(time_response), "It's %d:%02d %s", hour, local->tm_min,
               period);
    }
    return time_response;
  }

  // Week number questions
  if (strstr(normalized_text, "what week") != NULL ||
      strstr(normalized_text, "which week") != NULL ||
      strstr(normalized_text, "week number") != NULL ||
      strstr(normalized_text, "welche woche") != NULL) {
    time_t now = time(NULL);
    struct tm* local = localtime(&now);
    static char week_response[128];

    // Calculate ISO 8601 week number
    char week_str[8];
    strftime(week_str, sizeof(week_str), "%V", local);
    int week_num = atoi(week_str);

    if (strcmp(language_code, "es") == 0) {
      snprintf(week_response, sizeof(week_response), "Estamos en la semana %d", week_num);
    } else if (strcmp(language_code, "zh") == 0) {
      snprintf(week_response, sizeof(week_response), "这是第%d周", week_num);
    } else if (strcmp(language_code, "de") == 0) {
      snprintf(week_response, sizeof(week_response), "Es ist Woche %d", week_num);
    } else {
      snprintf(week_response, sizeof(week_response), "It's week %d", week_num);
    }
    return week_response;
  }

  // Date-related questions
  if (strstr(normalized_text, "what date") != NULL ||
      strstr(normalized_text, "what's the date") != NULL ||
      strstr(normalized_text, "what day is it") != NULL ||
      strstr(normalized_text, "what day") != NULL ||
      strstr(normalized_text, "what's today") != NULL ||
      strstr(normalized_text, "today's date") != NULL ||
      strstr(normalized_text, "welches datum") != NULL ||
      strstr(normalized_text, "welcher tag") != NULL) {
    time_t now = time(NULL);
    struct tm* local = localtime(&now);
    static char date_response[128];

    const char* months[] = {"January", "February", "March",     "April",   "May",      "June",
                            "July",    "August",   "September", "October", "November", "December"};
    const char* days[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                          "Thursday", "Friday", "Saturday"};

    if (strcmp(language_code, "es") == 0) {
      const char* months_es[] = {"enero",      "febrero", "marzo",     "abril",
                                 "mayo",       "junio",   "julio",     "agosto",
                                 "septiembre", "octubre", "noviembre", "diciembre"};
      snprintf(date_response, sizeof(date_response), "Hoy es %d de %s de %d", local->tm_mday,
               months_es[local->tm_mon], 1900 + local->tm_year);
    } else if (strcmp(language_code, "zh") == 0) {
      snprintf(date_response, sizeof(date_response), "今天是%d年%d月%d日", 1900 + local->tm_year,
               local->tm_mon + 1, local->tm_mday);
    } else if (strcmp(language_code, "de") == 0) {
      const char* months_de[] = {"Januar",    "Februar", "März",     "April",
                                 "Mai",       "Juni",    "Juli",     "August",
                                 "September", "Oktober", "November", "Dezember"};
      const char* days_de[] = {"Sonntag",    "Montag",  "Dienstag", "Mittwoch",
                               "Donnerstag", "Freitag", "Samstag"};
      snprintf(date_response, sizeof(date_response), "Heute ist %s, der %d. %s %d",
               days_de[local->tm_wday], local->tm_mday, months_de[local->tm_mon],
               1900 + local->tm_year);
    } else {
      snprintf(date_response, sizeof(date_response), "Today is %s, %s %d, %d", days[local->tm_wday],
               months[local->tm_mon], local->tm_mday, 1900 + local->tm_year);
    }
    return date_response;
  }

  // Name/identity questions
  if (strstr(normalized_text, "what's your name") != NULL ||
      strstr(normalized_text, "what is your name") != NULL ||
      strstr(normalized_text, "who are you") != NULL ||
      strstr(normalized_text, "cómo te llamas") != NULL ||
      strstr(normalized_text, "cuál es tu nombre") != NULL ||
      strstr(normalized_text, "quién eres") != NULL ||
      strstr(normalized_text, "你叫什么") != NULL || strstr(normalized_text, "你的名字") != NULL ||
      strstr(normalized_text, "你是谁") != NULL ||
      strstr(normalized_text, "wie heißt du") != NULL ||
      strstr(normalized_text, "wie heissen sie") != NULL ||
      strstr(normalized_text, "wer bist du") != NULL) {
    return get_localized_string(language_code, LOC_NAME_RESPONSE);
  }

  // Capability questions
  if (strstr(normalized_text, "what can you do") != NULL ||
      strstr(normalized_text, "what can you help") != NULL ||
      strstr(normalized_text, "how can you help") != NULL ||
      strstr(normalized_text, "qué puedes hacer") != NULL ||
      strstr(normalized_text, "en qué puedes ayudar") != NULL ||
      strstr(normalized_text, "cómo puedes ayudar") != NULL ||
      strstr(normalized_text, "你能做什么") != NULL ||
      strstr(normalized_text, "你可以帮我") != NULL ||
      strstr(normalized_text, "你会什么") != NULL ||
      strstr(normalized_text, "was kannst du") != NULL ||
      strstr(normalized_text, "was können sie") != NULL ||
      strstr(normalized_text, "wobei kannst du helfen") != NULL) {
    return get_localized_string(language_code, LOC_CAPABILITIES_RESPONSE);
  }

  // How are you questions
  if (strstr(normalized_text, "how are you") != NULL ||
      strstr(normalized_text, "how are you doing") != NULL ||
      strstr(normalized_text, "cómo estás") != NULL ||
      strstr(normalized_text, "cómo te va") != NULL || strstr(normalized_text, "qué tal") != NULL ||
      strstr(normalized_text, "你好吗") != NULL || strstr(normalized_text, "你怎么样") != NULL ||
      strstr(normalized_text, "wie geht es dir") != NULL ||
      strstr(normalized_text, "wie geht's") != NULL) {
    return get_localized_string(language_code, LOC_HOW_ARE_YOU_RESPONSE);
  }

  // Creator questions
  if (strstr(normalized_text, "who made you") != NULL ||
      strstr(normalized_text, "who created you") != NULL ||
      strstr(normalized_text, "who built you") != NULL ||
      strstr(normalized_text, "who developed you") != NULL ||
      strstr(normalized_text, "quién te creó") != NULL ||
      strstr(normalized_text, "quién te hizo") != NULL ||
      strstr(normalized_text, "谁创造了你") != NULL ||
      strstr(normalized_text, "谁做的你") != NULL ||
      strstr(normalized_text, "谁开发了你") != NULL ||
      strstr(normalized_text, "wer hat dich") != NULL) {
    return get_localized_string(language_code, LOC_CREATOR_RESPONSE);
  }

  // Purpose questions
  if (strstr(normalized_text, "what are you for") != NULL ||
      strstr(normalized_text, "why were you made") != NULL ||
      strstr(normalized_text, "why were you created") != NULL ||
      strstr(normalized_text, "what's your purpose") != NULL ||
      strstr(normalized_text, "what is your purpose") != NULL ||
      strstr(normalized_text, "para qué fuiste") != NULL ||
      strstr(normalized_text, "cuál es tu propósito") != NULL ||
      strstr(normalized_text, "para qué sirves") != NULL ||
      strstr(normalized_text, "你是干什么的") != NULL ||
      strstr(normalized_text, "你的目的是什么") != NULL ||
      strstr(normalized_text, "你有什么用") != NULL ||
      strstr(normalized_text, "warum wurdest du") != NULL ||
      strstr(normalized_text, "wofür bist du") != NULL) {
    return get_localized_string(language_code, LOC_PURPOSE_RESPONSE);
  }

  // Weather questions (we can't actually answer these without internet)
  if (strstr(normalized_text, "weather") != NULL ||
      strstr(normalized_text, "temperature") != NULL || strstr(normalized_text, "clima") != NULL ||
      strstr(normalized_text, "tiempo") != NULL || strstr(normalized_text, "temperatura") != NULL ||
      strstr(normalized_text, "天气") != NULL || strstr(normalized_text, "气温") != NULL ||
      strstr(normalized_text, "温度") != NULL || strstr(normalized_text, "wetter") != NULL ||
      strstr(normalized_text, "temperatur") != NULL) {
    return get_localized_string(language_code, LOC_WEATHER_NO_INTERNET);
  }

  // Thank you (not really a question but common response)
  if (strstr(normalized_text, "thank") != NULL || strstr(normalized_text, "thanks") != NULL ||
      strstr(normalized_text, "gracias") != NULL ||
      strstr(normalized_text, "muchas gracias") != NULL ||
      strstr(normalized_text, "谢谢") != NULL || strstr(normalized_text, "谢了") != NULL ||
      strstr(normalized_text, "感谢") != NULL || strstr(normalized_text, "danke") != NULL ||
      strstr(normalized_text, "vielen dank") != NULL) {
    return get_localized_string(language_code, LOC_THANK_YOU_RESPONSE);
  }

  // Online/Internet status questions
  if (strstr(normalized_text, "are you online") != NULL ||
      strstr(normalized_text, "do you have internet") != NULL ||
      strstr(normalized_text, "are you connected") != NULL ||
      strstr(normalized_text, "do you need internet") != NULL ||
      strstr(normalized_text, "estás en línea") != NULL ||
      strstr(normalized_text, "tienes internet") != NULL ||
      strstr(normalized_text, "estás conectado") != NULL ||
      strstr(normalized_text, "necesitas internet") != NULL ||
      strstr(normalized_text, "你在线吗") != NULL || strstr(normalized_text, "你有网络") != NULL ||
      strstr(normalized_text, "你联网了") != NULL || strstr(normalized_text, "需要网络") != NULL ||
      strstr(normalized_text, "bist du online") != NULL ||
      strstr(normalized_text, "hast du internet") != NULL) {
    return get_localized_string(language_code, LOC_OFFLINE_STATUS);
  }

  // Version/System info questions
  if (strstr(normalized_text, "what version") != NULL ||
      strstr(normalized_text, "what's your version") != NULL ||
      strstr(normalized_text, "version are you") != NULL ||
      strstr(normalized_text, "welche version") != NULL) {
    static char version_response[256];

#ifdef ETHERVOX_PLATFORM_ANDROID
    // Get Android version info
    char android_version[PROP_VALUE_MAX] = {0};
    char device_model[PROP_VALUE_MAX] = {0};
    char sdk_version[PROP_VALUE_MAX] = {0};

    __system_property_get("ro.build.version.release", android_version);
    __system_property_get("ro.product.model", device_model);
    __system_property_get("ro.build.version.sdk", sdk_version);

    if (strcmp(language_code, "es") == 0) {
      snprintf(version_response, sizeof(version_response),
               "EthervoxAI versión %s, compilación %s. Ejecutándose en Android %s, SDK %s, "
               "dispositivo %s",
               ETHERVOX_VERSION_STRING, ETHERVOX_BUILD_TYPE, android_version, sdk_version,
               device_model);
    } else if (strcmp(language_code, "zh") == 0) {
      snprintf(version_response, sizeof(version_response),
               "EthervoxAI版本%s，%s版本。运行在Android %s，SDK %s，设备%s",
               ETHERVOX_VERSION_STRING, ETHERVOX_BUILD_TYPE, android_version, sdk_version,
               device_model);
    } else if (strcmp(language_code, "de") == 0) {
      snprintf(version_response, sizeof(version_response),
               "EthervoxAI Version %s, %s Build. Läuft auf Android %s, SDK %s, Gerät %s",
               ETHERVOX_VERSION_STRING, ETHERVOX_BUILD_TYPE, android_version, sdk_version,
               device_model);
    } else {
      snprintf(version_response, sizeof(version_response),
               "EthervoxAI version %s, %s build. Running on Android %s, SDK %s, device %s",
               ETHERVOX_VERSION_STRING, ETHERVOX_BUILD_TYPE, android_version, sdk_version,
               device_model);
    }
#else
    // Non-Android platform - use cross-platform utility
    char os_name[64];
    char os_version[64];
    platform_get_os_version(os_name, sizeof(os_name), os_version, sizeof(os_version));
    
    if (strcmp(language_code, "es") == 0) {
      snprintf(version_response, sizeof(version_response),
               "EthervoxAI versión %s, compilación %s. Ejecutándose en %s %s",
               ETHERVOX_VERSION_STRING, ETHERVOX_BUILD_TYPE, os_name, os_version);
    } else if (strcmp(language_code, "zh") == 0) {
      snprintf(version_response, sizeof(version_response),
               "EthervoxAI版本%s，%s版本。运行在%s %s", ETHERVOX_VERSION_STRING,
               ETHERVOX_BUILD_TYPE, os_name, os_version);
    } else if (strcmp(language_code, "de") == 0) {
      snprintf(version_response, sizeof(version_response),
               "EthervoxAI Version %s, %s Build. Läuft auf %s %s", ETHERVOX_VERSION_STRING,
               ETHERVOX_BUILD_TYPE, os_name, os_version);
    } else {
      snprintf(version_response, sizeof(version_response),
               "EthervoxAI version %s, %s build. Running on %s %s", ETHERVOX_VERSION_STRING,
               ETHERVOX_BUILD_TYPE, os_name, os_version);
    }
    
    // Note: Removed fallback - platform_get_os_version always returns something
#endif

    return version_response;
  }

  // Platform/Device questions
  if (strstr(normalized_text, "what device") != NULL ||
      strstr(normalized_text, "what platform") != NULL ||
      strstr(normalized_text, "what system") != NULL ||
      strstr(normalized_text, "welches gerät") != NULL ||
      strstr(normalized_text, "welche plattform") != NULL) {
    static char device_response[256];

#ifdef ETHERVOX_PLATFORM_ANDROID
    char device_model[PROP_VALUE_MAX] = {0};
    char manufacturer[PROP_VALUE_MAX] = {0};
    char android_version[PROP_VALUE_MAX] = {0};

    __system_property_get("ro.product.model", device_model);
    __system_property_get("ro.product.manufacturer", manufacturer);
    __system_property_get("ro.build.version.release", android_version);

    if (strcmp(language_code, "es") == 0) {
      snprintf(device_response, sizeof(device_response),
               "Estoy ejecutándome en un %s %s con Android %s, completamente en tu dispositivo",
               manufacturer, device_model, android_version);
    } else if (strcmp(language_code, "zh") == 0) {
      snprintf(device_response, sizeof(device_response),
               "我在%s %s上运行，Android %s，完全在您的设备上", manufacturer, device_model,
               android_version);
    } else if (strcmp(language_code, "de") == 0) {
      snprintf(device_response, sizeof(device_response),
               "Ich laufe auf einem %s %s mit Android %s, vollständig auf deinem Gerät",
               manufacturer, device_model, android_version);
    } else {
      snprintf(device_response, sizeof(device_response),
               "I'm running on a %s %s with Android %s, entirely on your device", manufacturer,
               device_model, android_version);
    }
#else
    // Non-Android platform - use cross-platform utility
    char os_name[64];
    char os_version[64];
    char architecture[64];
    platform_get_os_version(os_name, sizeof(os_name), os_version, sizeof(os_version));
    platform_get_architecture(architecture, sizeof(architecture));
    
    if (strcmp(language_code, "es") == 0) {
      snprintf(device_response, sizeof(device_response), "Estoy ejecutándome en %s %s, %s",
               os_name, os_version, architecture);
    } else if (strcmp(language_code, "zh") == 0) {
      snprintf(device_response, sizeof(device_response), "我在%s %s上运行，%s", os_name,
               os_version, architecture);
    } else if (strcmp(language_code, "de") == 0) {
      snprintf(device_response, sizeof(device_response), "Ich laufe auf %s %s, %s Architektur",
               os_name, os_version, architecture);
    } else {
      snprintf(device_response, sizeof(device_response), "I'm running on %s %s, %s architecture",
               os_name, os_version, architecture);
    }
#endif
    
    return device_response;
  }

  // Privacy questions
  if (strstr(normalized_text, "is my data safe") != NULL ||
      strstr(normalized_text, "are you private") != NULL ||
      strstr(normalized_text, "do you track me") != NULL ||
      strstr(normalized_text, "is this private") != NULL ||
      strstr(normalized_text, "ist das privat") != NULL ||
      strstr(normalized_text, "verfolgst du mich") != NULL) {
    return get_localized_string(language_code, LOC_PRIVACY_QUESTION_RESPONSE);
  }

  // No simple answer found
  return NULL;
}

// Helper function to check if user is confirming/acknowledging
static bool is_confirmation(const char* normalized_text) {
  if (!normalized_text) {
    return false;
  }

  // Check for common confirmation words
  return (strcmp(normalized_text, "yes") == 0 || strcmp(normalized_text, "yeah") == 0 ||
          strcmp(normalized_text, "yep") == 0 || strcmp(normalized_text, "sure") == 0 ||
          strcmp(normalized_text, "okay") == 0 || strcmp(normalized_text, "ok") == 0 ||
          strcmp(normalized_text, "alright") == 0 || strcmp(normalized_text, "correct") == 0 ||
          strcmp(normalized_text, "right") == 0 || strcmp(normalized_text, "exactly") == 0 ||
          // Spanish
          strcmp(normalized_text, "sí") == 0 || strcmp(normalized_text, "si") == 0 ||
          strcmp(normalized_text, "vale") == 0 || strcmp(normalized_text, "bueno") == 0 ||
          strcmp(normalized_text, "correcto") == 0 ||
          // Chinese
          strcmp(normalized_text, "是") == 0 || strcmp(normalized_text, "对") == 0 ||
          strcmp(normalized_text, "好") == 0 || strcmp(normalized_text, "可以") == 0);
}

// Helper function to check if user is asking for listening confirmation
static bool is_listening_check(const char* normalized_text) {
  if (!normalized_text) {
    return false;
  }

  return (strstr(normalized_text, "can you hear") != NULL ||
          strstr(normalized_text, "are you listening") != NULL ||
          strstr(normalized_text, "do you hear me") != NULL ||
          strstr(normalized_text, "are you there") != NULL ||
          strstr(normalized_text, "hello") == normalized_text ||  // "hello" at start
          // Spanish
          strstr(normalized_text, "me escuchas") != NULL ||
          strstr(normalized_text, "estás ahí") != NULL ||
          strstr(normalized_text, "me oyes") != NULL ||
          // Chinese
          strstr(normalized_text, "能听到") != NULL || strstr(normalized_text, "在吗") != NULL ||
          strstr(normalized_text, "听得到") != NULL);
}

// Process with LLM
ethervox_result_t ethervox_dialogue_process_llm(ethervox_dialogue_engine_t* engine,
                                  const ethervox_intent_t* intent, const char* context_id,
                                  ethervox_llm_response_t* response) {
  if (!engine || !intent || !response) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  memset(response, 0, sizeof(ethervox_llm_response_t));

  // Declare variables
  const char* response_text = NULL;
  bool conversation_ended = false;

  // Try Governor first for ALL intent types (if enabled)
  if (engine->use_governor && engine->governor) {
#ifdef ETHERVOX_PLATFORM_ANDROID
    ETHERVOX_LOGI( 
                       "Using Governor for intent: %s", intent->raw_text);
#endif
    
    char* gov_response = NULL;
    char* gov_error = NULL;
    ethervox_confidence_metrics_t metrics = {0};
    
    ethervox_governor_status_t status = ethervox_governor_execute(
        (ethervox_governor_t*)engine->governor,
        intent->raw_text,
        &gov_response,
        &gov_error,
        &metrics,
        NULL,  // Progress callback
        NULL,  // Token callback (not streaming)
        NULL   // User data
    );
    
    if (status == ETHERVOX_GOVERNOR_SUCCESS && gov_response) {
      response->text = strdup(gov_response);
      response->confidence = metrics.confidence;
      response->processing_time_ms = 100;
      response->conversation_ended = true;
      free(gov_response);
      if (gov_error) free(gov_error);
      return ETHERVOX_SUCCESS;
    }
    if (gov_error) free(gov_error);
    if (gov_response) free(gov_response);
  }
  
  // Try LLM backend second for ALL intent types (if available)
#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE
  if (engine->llm_backend && engine->use_llm_for_unknown) {
    ethervox_llm_backend_t* backend = (ethervox_llm_backend_t*)engine->llm_backend;
    if (backend->is_loaded) {
#ifdef ETHERVOX_PLATFORM_ANDROID
      ETHERVOX_LOGI(
                          "Using LLM backend for intent: %s", intent->raw_text);
#endif
      ethervox_result_t llm_result = ethervox_llm_backend_generate(backend, intent->raw_text,
                                                     intent->language_code, response);
      if (ethervox_is_success(llm_result) && response->text && response->text[0] != '\0') {
        response->confidence = 0.7f;
        response->processing_time_ms = kEthervoxResponseProcessingTimeMs;
        response->conversation_ended = (intent->type != ETHERVOX_INTENT_UNKNOWN);
        return ETHERVOX_SUCCESS;
      }
    }
  }
#endif

  // Fallback to deterministic responses only if Governor and LLM unavailable/failed
  if (is_confirmation(intent->normalized_text)) {
    conversation_ended = false;
    response_text = get_localized_string(intent->language_code, LOC_CONFIRMATION_RESPONSE);
  }
  else if (is_listening_check(intent->normalized_text)) {
    conversation_ended = false;
    response_text = get_localized_string(intent->language_code, LOC_LISTENING_RESPONSE);
  }
  else {
    switch (intent->type) {
      case ETHERVOX_INTENT_GREETING:
        response_text = get_localized_string(intent->language_code, LOC_GREETING_RESPONSE);
        break;

      case ETHERVOX_INTENT_QUESTION:
      // Route to Governor if enabled
      if (engine->use_governor && engine->governor) {
        
#ifdef ETHERVOX_PLATFORM_ANDROID
        ETHERVOX_LOGI( 
                           "Using Governor for query: %s", intent->raw_text);
#else
        printf("Using Governor for query: %s\\n", intent->raw_text);
#endif
        
        char* gov_response = NULL;
        char* gov_error = NULL;
        ethervox_confidence_metrics_t metrics = {0};
        
        ethervox_governor_status_t status = ethervox_governor_execute(
            (ethervox_governor_t*)engine->governor,
            intent->raw_text,
            &gov_response,
            &gov_error,
            &metrics,
            NULL,  // TODO: Add progress callback for UI updates
            NULL,  // Token callback (not streaming)
            NULL   // User data
        );
        
        if (status == ETHERVOX_GOVERNOR_SUCCESS && gov_response) {
          // Governor succeeded
          response_text = gov_response;  // Will be copied below
          snprintf(response->text, sizeof(response->text), "%s", gov_response);
          response->confidence = metrics.confidence;
          response->processing_time_ms = 100;  // Governor is fast
          response->conversation_ended = true;
          free(gov_response);
          if (gov_error) free(gov_error);
          
#ifdef ETHERVOX_PLATFORM_ANDROID
          ETHERVOX_LOGI( 
                             "Governor response: %s (conf=%.2f)", response->text, metrics.confidence);
#endif
          return ETHERVOX_SUCCESS;
        } else if (gov_error) {
#ifdef ETHERVOX_PLATFORM_ANDROID
          ETHERVOX_LOGW( 
                             "Governor error: %s", gov_error);
#endif
          free(gov_error);
        }
        if (gov_response) free(gov_response);
        
        // If Governor failed, fall through to normal LLM
      }
        
        // Try LLM backend first (preferred over simple deterministic answers)
#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE
        if (engine->llm_backend && engine->use_llm_for_unknown) {
          ethervox_llm_backend_t* backend = (ethervox_llm_backend_t*)engine->llm_backend;

          if (backend->is_loaded) {
#ifdef ETHERVOX_PLATFORM_ANDROID
            ETHERVOX_LOGI(
                                "Using LLM backend for question: %s", intent->raw_text);
#else
            printf("Using LLM backend for question: %s\n", intent->raw_text);
#endif

            // Generate response with LLM
            ethervox_result_t llm_result = ethervox_llm_backend_generate(backend, intent->raw_text,
                                                           intent->language_code, response);
            if (ethervox_is_success(llm_result) && response->text && response->text[0] != '\0') {
              response->confidence = 0.7f;
              response->processing_time_ms = kEthervoxResponseProcessingTimeMs;
              response->conversation_ended = true;  // LLM questions also end conversation
              return ETHERVOX_SUCCESS;
            }
          }
        }
#endif

        // Fallback to simple deterministic answers only if LLM unavailable/failed
        response_text = answer_simple_question(intent->normalized_text, intent->language_code);

        if (response_text) {
          // We found a simple answer, use it as last resort
          conversation_ended = true;  // Questions should end conversation (no follow-up)
#ifdef ETHERVOX_PLATFORM_ANDROID
          ETHERVOX_LOGI(
                              "Answered simple question directly (fallback): %s", response_text);
#else
          printf("Answered simple question directly (fallback): %s\n", response_text);
#endif
          break;  // Use the direct answer
        }

        // Final fallback if everything failed
        conversation_ended = true;  // Even failed questions should end conversation
        response_text = get_localized_string(intent->language_code, LOC_LLM_UNAVAILABLE);
        break;

      case ETHERVOX_INTENT_COMMAND:
      case ETHERVOX_INTENT_CONTROL:
        conversation_ended = true;  // Commands should not auto-restart microphone
        response_text = get_localized_string(intent->language_code, LOC_COMMAND_UNDERSTOOD);
        break;

      case ETHERVOX_INTENT_GOODBYE:
        conversation_ended = true;  // Signal that conversation should end
        response_text = get_localized_string(intent->language_code, LOC_GOODBYE_RESPONSE);
        break;

      default:
      case ETHERVOX_INTENT_UNKNOWN:
        // Try LLM backend if available
#ifdef ETHERVOX_PLATFORM_ANDROID
        ETHERVOX_LOGI( "UNKNOWN intent received: %s",
                            intent->raw_text);
#else
        printf("UNKNOWN intent received: %s\n", intent->raw_text);
#endif

#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE
#ifdef ETHERVOX_PLATFORM_ANDROID
        ETHERVOX_LOGI( "LLM compile flags are defined");
        ETHERVOX_LOGI(
                            "engine->llm_backend = %p, engine->use_llm_for_unknown = %d",
                            engine->llm_backend, engine->use_llm_for_unknown);
#else
        printf("LLM compile flags are defined\n");
        printf("engine->llm_backend = %p, engine->use_llm_for_unknown = %d\n", engine->llm_backend,
               engine->use_llm_for_unknown);
#endif

        if (engine->llm_backend && engine->use_llm_for_unknown) {
          ethervox_llm_backend_t* backend = (ethervox_llm_backend_t*)engine->llm_backend;

#ifdef ETHERVOX_PLATFORM_ANDROID
          ETHERVOX_LOGI(
                              "LLM backend exists, checking if loaded...");
          ETHERVOX_LOGI( "backend->is_loaded = %d",
                              backend->is_loaded);
#else
          printf("LLM backend exists, checking if loaded...\n");
          printf("backend->is_loaded = %d\n", backend->is_loaded);
#endif

          // Check if backend is loaded
          if (backend->is_loaded) {
            // Create conversational prompt
            char prompt[2048];
            snprintf(prompt, sizeof(prompt), "User: %s\nAssistant:", intent->raw_text);

            // Generate response with LLM
            int llm_result =
                ethervox_llm_backend_generate(backend, prompt, intent->language_code, response);
            if (llm_result == 0 && response->text && response->text[0] != '\0') {
              // LLM generated response successfully
              response->confidence = 0.7f;  // LLM confidence
              response->processing_time_ms = kEthervoxResponseProcessingTimeMs;
              response->conversation_ended = false;  // Don't end conversation
#ifdef ETHERVOX_PLATFORM_ANDROID
              ETHERVOX_LOGI(
                                  "LLM backend generated response: %s", response->text);
#else
              printf("LLM backend generated response: %s\n", response->text);
#endif
              return ETHERVOX_SUCCESS;
            } else {
#ifdef ETHERVOX_PLATFORM_ANDROID
              ETHERVOX_LOGW(
                                  "LLM backend generation failed or empty response: %d",
                                  llm_result);
#else
              printf("LLM backend generation failed or empty response: %d\n", llm_result);
#endif
            }
          } else {
#ifdef ETHERVOX_PLATFORM_ANDROID
            ETHERVOX_LOGI(
                                "LLM backend not loaded (is_loaded=%d), using fallback response",
                                backend->is_loaded);
#else
            printf("LLM backend not loaded (is_loaded=%d), using fallback response\n",
                   backend->is_loaded);
#endif
          }
        } else {
#ifdef ETHERVOX_PLATFORM_ANDROID
          ETHERVOX_LOGI(
                              "LLM backend not available: llm_backend=%p, use_llm_for_unknown=%d",
                              engine->llm_backend, engine->use_llm_for_unknown);
#else
          printf("LLM backend not available: llm_backend=%p, use_llm_for_unknown=%d\n",
                 engine->llm_backend, engine->use_llm_for_unknown);
#endif
        }
#else
#ifdef ETHERVOX_PLATFORM_ANDROID
        ETHERVOX_LOGI(
                            "LLM compile flags NOT defined - ETHERVOX_WITH_LLAMA and/or "
                            "LLAMA_CPP_AVAILABLE not set");
#else
        printf(
            "LLM compile flags NOT defined - ETHERVOX_WITH_LLAMA and/or LLAMA_CPP_AVAILABLE not "
            "set\n");
#endif
#endif

        // Fallback to pattern-based response if LLM not available or failed
        response->requires_external_llm = true;
        response->external_llm_prompt = strdup(intent->raw_text);

        response_text = get_localized_string(intent->language_code, LOC_UNKNOWN_RESPONSE);
        break;
    }
  }

  response->text = strdup(response_text);
  response->user_prompt_punctuated = intent->raw_text ? strdup(intent->raw_text) : NULL;
  snprintf(response->language_code, sizeof(response->language_code), "%s", intent->language_code);
  response->confidence = intent->confidence;                         // Use actual intent confidence
  response->processing_time_ms = kEthervoxResponseProcessingTimeMs;  // Simulated processing time
  response->token_count =
      strlen(response_text) / kEthervoxTokenEstimateDivisor;  // Rough token estimate
  response->conversation_ended = conversation_ended;          // Set flag to disable follow-up

  printf("LLM response generated: %s (confidence: %.0f%%, conversation_ended: %s)\n",
         response->text, response->confidence * 100.0f, conversation_ended ? "true" : "false");

  return ETHERVOX_SUCCESS;
}

// Process LLM with streaming token generation
ethervox_result_t ethervox_dialogue_process_llm_stream(ethervox_dialogue_engine_t* engine,
                                         const ethervox_intent_t* intent,
                                         const ethervox_dialogue_context_t* context,
                                         void (*token_callback)(const char* token, void* user_data),
                                         void* user_data,
                                         bool* conversation_ended,
                                         ethervox_governor_progress_callback governor_progress_callback) {
  if (!engine || !intent || !token_callback) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Route to Governor for ALL intents if enabled (not just questions/unknown)
  // This ensures tools like memory_store, timers, etc. are available
  if (engine->use_governor && engine->governor) {
    
#ifdef ETHERVOX_PLATFORM_ANDROID
    ETHERVOX_LOGI( 
                       "Using Governor (streaming) for: %s", intent->raw_text);
#endif
    
    char* gov_response = NULL;
    char* gov_error = NULL;
    ethervox_confidence_metrics_t metrics = {0};
    
    // Use the provided governor progress callback (from JNI layer)
    ethervox_governor_status_t status = ethervox_governor_execute(
        (ethervox_governor_t*)engine->governor,
        intent->raw_text,
        &gov_response,
        &gov_error,
        &metrics,
        governor_progress_callback,  // Now properly wired from JNI!
        token_callback,  // Token callback for streaming
        user_data
    );
    
    if (status == ETHERVOX_GOVERNOR_SUCCESS && gov_response) {
      // Response was already streamed via token_callback
      // Just set conversation_ended flag
      if (conversation_ended) {
        *conversation_ended = true;
      }
      
      free(gov_response);
      if (gov_error) free(gov_error);
      return ETHERVOX_SUCCESS;
    } else if (gov_error) {
#ifdef ETHERVOX_PLATFORM_ANDROID
      ETHERVOX_LOGW( 
                         "Governor failed: %s", gov_error);
#endif
      free(gov_error);
    }
    if (gov_response) free(gov_response);
    
    // If Governor failed, fall through to regular LLM
  }

#if defined(ETHERVOX_WITH_LLAMA) && defined(LLAMA_CPP_AVAILABLE) && LLAMA_CPP_AVAILABLE
  ethervox_llm_backend_t* backend = (ethervox_llm_backend_t*)engine->llm_backend;
  // Prefer LLM backend over simple deterministic answers when available
  if (backend && backend->is_loaded && backend->generate_stream && engine->use_llm_for_unknown) {
    bool use_llm_streaming = false;

    // Use streaming for UNKNOWN intents
    if (intent->type == ETHERVOX_INTENT_UNKNOWN) {
      use_llm_streaming = true;
    }
    // Use streaming for QUESTION intents when LLM is preferred
    else if (intent->type == ETHERVOX_INTENT_QUESTION) {
      // When LLM backend is available and enabled, use it for questions
      use_llm_streaming = true;
    }

    if (use_llm_streaming) {
      ETHERVOX_LOGI(
                          "Using LLM backend streaming for: %s", intent->raw_text);

      return backend->generate_stream(backend, intent->raw_text, intent->language_code,
                                      token_callback, user_data);
    }
  }
#endif

  // Fallback: process with regular dialogue engine and stream the result
#ifdef ETHERVOX_PLATFORM_ANDROID
  ETHERVOX_LOGI(
                      "LLM streaming not available, using regular dialogue processing");
#else
  printf("LLM streaming not available, using regular dialogue processing\n");
#endif

  ethervox_llm_response_t response;
  memset(&response, 0, sizeof(ethervox_llm_response_t));

  ethervox_result_t result = ethervox_dialogue_process_llm(engine, intent, NULL, &response);
  if (ethervox_is_success(result) && response.text) {
    // Stream the complete response as one token
    if (token_callback) {
      token_callback(response.text, user_data);
    }

    // Set conversation_ended flag if provided
    if (conversation_ended) {
      *conversation_ended = response.conversation_ended;
    }

    ethervox_llm_response_free(&response);
    return ETHERVOX_SUCCESS;
  }

  ethervox_llm_response_free(&response);
  return result;
}

// Create dialogue context
ethervox_result_t ethervox_dialogue_create_context(ethervox_dialogue_engine_t* engine,
                                     const ethervox_dialogue_context_request_t* request,
                                     char** context_id) {
  if (!engine || !request || !request->user_id || !context_id) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  const char* language_code = request->language_code;
  if (!language_code || language_code[0] == '\0') {
    language_code = engine->llm_config.language_code ? engine->llm_config.language_code : "en";
  }

  // Find available context slot
  for (uint32_t i = 0; i < engine->max_contexts; i++) {
    ethervox_dialogue_context_t* ctx = &engine->contexts[i];
    if (!ctx->conversation_id) {  // Empty slot
      ctx->conversation_id = generate_conversation_id();
      ctx->display_name = generate_display_name();
      ctx->user_id = strdup(request->user_id);
      snprintf(ctx->current_language, sizeof(ctx->current_language), "%s", language_code);
      ctx->max_history = kEthervoxDefaultMaxHistory;
      ctx->conversation_history =
          (ethervox_intent_t*)calloc(ctx->max_history, sizeof(ethervox_intent_t));
      ctx->last_interaction_time = time(NULL);

      *context_id = strdup(ctx->conversation_id);
      engine->active_contexts++;

      printf("Created dialogue context: %s ('%s') for user: %s\n", 
             ctx->conversation_id, ctx->display_name, request->user_id);
      return ETHERVOX_SUCCESS;
    }
  }

  return ETHERVOX_ERROR_INVALID_ARGUMENT;  // No available slots
}

// Get display name for a conversation context
const char* ethervox_dialogue_get_display_name(ethervox_dialogue_engine_t* engine, 
                                               const char* context_id) {
  if (!engine || !context_id) {
    return NULL;
  }
  
  for (uint32_t i = 0; i < engine->max_contexts; i++) {
    ethervox_dialogue_context_t* ctx = &engine->contexts[i];
    if (ctx->conversation_id && strcmp(ctx->conversation_id, context_id) == 0) {
      return ctx->display_name;
    }
  }
  
  return NULL;
}

// Rename a conversation context
ethervox_result_t ethervox_dialogue_rename_context(ethervox_dialogue_engine_t* engine,
                                                   const char* context_id,
                                                   const char* new_name) {
  ETHERVOX_CHECK_PTR(engine);
  ETHERVOX_CHECK_PTR(context_id);
  ETHERVOX_CHECK_PTR(new_name);
  
  for (uint32_t i = 0; i < engine->max_contexts; i++) {
    ethervox_dialogue_context_t* ctx = &engine->contexts[i];
    if (ctx->conversation_id && strcmp(ctx->conversation_id, context_id) == 0) {
      // Free old display name
      if (ctx->display_name) {
        free(ctx->display_name);
      }
      
      // Set new display name
      ctx->display_name = strdup(new_name);
      if (!ctx->display_name) {
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_OUT_OF_MEMORY, 
                             "Failed to allocate memory for new conversation name");
      }
      
      printf("Renamed conversation %s to '%s'\n", context_id, new_name);
      return ETHERVOX_SUCCESS;
    }
  }
  
  ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_NOT_FOUND, "Conversation context not found");
}

ethervox_result_t ethervox_dialogue_set_language(ethervox_dialogue_engine_t* engine, const char* language_code) {
  if (!engine || !language_code) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  char normalized[8] = {0};
  sanitize_language_code(language_code, normalized, sizeof(normalized));

  if (normalized[0] == '\0') {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  if (!ethervox_dialogue_is_language_supported(normalized)) {
    fprintf(stderr, "Dialogue language '%s' not supported; keeping current setting\n", normalized);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  if (engine->llm_config.language_code &&
      strcmp(engine->llm_config.language_code, normalized) == 0) {
    return ETHERVOX_SUCCESS;
  }

  if (engine->llm_config.language_code) {
    free(engine->llm_config.language_code);
    engine->llm_config.language_code = NULL;
  }

  engine->llm_config.language_code = strdup(normalized);
  if (!engine->llm_config.language_code) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  for (uint32_t i = 0; i < engine->max_contexts; i++) {
    ethervox_dialogue_context_t* ctx = &engine->contexts[i];
    if (ctx->conversation_id) {
      snprintf(ctx->current_language, sizeof(ctx->current_language), "%s", normalized);
      ctx->current_language[sizeof(ctx->current_language) - 1] = '\0';
    }
  }

  return ETHERVOX_SUCCESS;
}

const char* ethervox_dialogue_get_language(const ethervox_dialogue_engine_t* engine) {
  if (!engine || !engine->llm_config.language_code) {
    return NULL;
  }
  return engine->llm_config.language_code;
}

// Free intent structure
void ethervox_intent_free(ethervox_intent_t* intent) {
  if (!intent) {
    return;
  }

  if (intent->raw_text) {
    free(intent->raw_text);
    intent->raw_text = NULL;
  }
  if (intent->normalized_text) {
    free(intent->normalized_text);
    intent->normalized_text = NULL;
  }
  if (intent->entities) {
    for (uint32_t i = 0; i < intent->entity_count; i++) {
      if (intent->entities[i].value) {
        free(intent->entities[i].value);
      }
      if (intent->entities[i].normalized_value) {
        free(intent->entities[i].normalized_value);
      }
    }
    free(intent->entities);
    intent->entities = NULL;
  }
}

// Free LLM response structure
void ethervox_llm_response_free(ethervox_llm_response_t* response) {
  if (!response) {
    return;
  }

  if (response->text) {
    free(response->text);
    response->text = NULL;
  }

  if (response->user_prompt_punctuated) {
    free(response->user_prompt_punctuated);
    response->user_prompt_punctuated = NULL;
  }

  if (response->model_name) {
    free(response->model_name);
    response->model_name = NULL;
  }

  if (response->finish_reason) {
    free(response->finish_reason);
    response->finish_reason = NULL;
  }
}

// Set external LLM callback
void ethervox_dialogue_set_external_llm_callback(ethervox_dialogue_engine_t* engine,
                                                 ethervox_external_llm_callback_t callback,
                                                 void* user_data) {
  if (engine) {
    engine->external_llm_callback = callback;
    engine->external_llm_user_data = user_data;
  }
}

// Set memory store for dialogue engine tools
void ethervox_dialogue_set_memory_store(struct ethervox_memory_store_t* store) {
  g_dialogue_memory_store = store;
#ifdef ETHERVOX_PLATFORM_ANDROID
  ETHERVOX_LOGI("Memory store set for dialogue engine");
#else
  printf("Memory store set for dialogue engine\n");
#endif
}

// Reload manifest registry (for model switching)
ethervox_result_t ethervox_dialogue_reload_manifest(ethervox_dialogue_engine_t* engine, const char* model_path) {
  if (!engine || !model_path) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }
  
  if (!engine->governor || !engine->use_governor) {
#ifdef ETHERVOX_PLATFORM_ANDROID
    ETHERVOX_LOGW("Governor not initialized - cannot reload manifest");
#else
    printf("Governor not initialized - cannot reload manifest\n");
#endif
    return -2;
  }
  
  // Cleanup existing manifest if any
  if (engine->manifest_registry) {
    ethervox_tool_manifest_cleanup((tool_manifest_registry_t*)engine->manifest_registry);
    free(engine->manifest_registry);
    engine->manifest_registry = NULL;
  }
  
  // Allocate new manifest
  tool_manifest_registry_t* manifest = malloc(sizeof(tool_manifest_registry_t));
  if (!manifest) {
#ifdef ETHERVOX_PLATFORM_ANDROID
    ETHERVOX_LOGE("Failed to allocate manifest registry");
#else
    printf("Failed to allocate manifest registry\n");
#endif
    return -3;
  }
  
  memset(manifest, 0, sizeof(tool_manifest_registry_t));
  
  // Initialize with new model
#ifdef ETHERVOX_PLATFORM_ANDROID
  ETHERVOX_LOGI("Reloading manifest for model: %s", model_path);
#else
  printf("Reloading manifest for model: %s\n", model_path);
#endif
  
  if (ethervox_governor_init_with_manifest(
          (ethervox_governor_t*)engine->governor, 
          model_path, 
          manifest) == 0) {
    engine->manifest_registry = manifest;
    
#ifdef ETHERVOX_PLATFORM_ANDROID
    ETHERVOX_LOGI("Manifest reloaded successfully (fallback level: %u)", 
                 manifest->fallback_level);
#else
    printf("Manifest reloaded successfully (fallback level: %u)\n", 
           manifest->fallback_level);
#endif
    return ETHERVOX_SUCCESS;
  } else {
    // Failed to reload - free and return error
    free(manifest);
#ifdef ETHERVOX_PLATFORM_ANDROID
    ETHERVOX_LOGW("Failed to reload manifest - continuing without it");
#else
    printf("Failed to reload manifest - continuing without it\n");
#endif
    return -4;
  }
}
