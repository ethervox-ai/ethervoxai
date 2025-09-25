#include "ethervox/dialogue.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// Supported languages for MVP
static const char* SUPPORTED_LANGUAGES[] = {
    "en",  // English
    "es",  // Spanish
    "zh",  // Mandarin Chinese
    NULL
};

// Simple intent patterns for demonstration
typedef struct {
    const char* pattern;
    ethervox_intent_type_t intent_type;
    const char* language;
} intent_pattern_t;

static const intent_pattern_t INTENT_PATTERNS[] = {
    // English patterns
    {"hello", ETHERVOX_INTENT_GREETING, "en"},
    {"hi", ETHERVOX_INTENT_GREETING, "en"},
    {"good morning", ETHERVOX_INTENT_GREETING, "en"},
    {"what is", ETHERVOX_INTENT_QUESTION, "en"},
    {"how to", ETHERVOX_INTENT_QUESTION, "en"},
    {"turn on", ETHERVOX_INTENT_CONTROL, "en"},
    {"turn off", ETHERVOX_INTENT_CONTROL, "en"},
    {"set", ETHERVOX_INTENT_COMMAND, "en"},
    {"play", ETHERVOX_INTENT_COMMAND, "en"},
    {"stop", ETHERVOX_INTENT_COMMAND, "en"},
    {"goodbye", ETHERVOX_INTENT_GOODBYE, "en"},
    {"bye", ETHERVOX_INTENT_GOODBYE, "en"},
    
    // Spanish patterns
    {"hola", ETHERVOX_INTENT_GREETING, "es"},
    {"buenos días", ETHERVOX_INTENT_GREETING, "es"},
    {"qué es", ETHERVOX_INTENT_QUESTION, "es"},
    {"cómo", ETHERVOX_INTENT_QUESTION, "es"},
    {"encender", ETHERVOX_INTENT_CONTROL, "es"},
    {"apagar", ETHERVOX_INTENT_CONTROL, "es"},
    {"reproducir", ETHERVOX_INTENT_COMMAND, "es"},
    {"parar", ETHERVOX_INTENT_COMMAND, "es"},
    {"adiós", ETHERVOX_INTENT_GOODBYE, "es"},
    
    // Chinese patterns (simplified)
    {"你好", ETHERVOX_INTENT_GREETING, "zh"},
    {"早上好", ETHERVOX_INTENT_GREETING, "zh"},
    {"什么是", ETHERVOX_INTENT_QUESTION, "zh"},
    {"怎么", ETHERVOX_INTENT_QUESTION, "zh"},
    {"打开", ETHERVOX_INTENT_CONTROL, "zh"},
    {"关闭", ETHERVOX_INTENT_CONTROL, "zh"},
    {"播放", ETHERVOX_INTENT_COMMAND, "zh"},
    {"停止", ETHERVOX_INTENT_COMMAND, "zh"},
    {"再见", ETHERVOX_INTENT_GOODBYE, "zh"},
    
    {NULL, ETHERVOX_INTENT_UNKNOWN, NULL}  // Sentinel
};

// Intent type to string mapping
const char* ethervox_intent_type_to_string(ethervox_intent_type_t type) {
    switch (type) {
        case ETHERVOX_INTENT_GREETING: return "greeting";
        case ETHERVOX_INTENT_QUESTION: return "question";
        case ETHERVOX_INTENT_COMMAND: return "command";
        case ETHERVOX_INTENT_REQUEST: return "request";
        case ETHERVOX_INTENT_INFORMATION: return "information";
        case ETHERVOX_INTENT_CONTROL: return "control";
        case ETHERVOX_INTENT_GOODBYE: return "goodbye";
        case ETHERVOX_INTENT_UNKNOWN:
        default: return "unknown";
    }
}

// Entity type to string mapping
const char* ethervox_entity_type_to_string(ethervox_entity_type_t type) {
    switch (type) {
        case ETHERVOX_ENTITY_PERSON: return "person";
        case ETHERVOX_ENTITY_LOCATION: return "location";
        case ETHERVOX_ENTITY_TIME: return "time";
        case ETHERVOX_ENTITY_NUMBER: return "number";
        case ETHERVOX_ENTITY_DEVICE: return "device";
        case ETHERVOX_ENTITY_ACTION: return "action";
        default: return "unknown";
    }
}

// Default LLM configuration
ethervox_llm_config_t ethervox_dialogue_get_default_llm_config(void) {
    ethervox_llm_config_t config = {
        .model_path = NULL,  // Will be set based on platform
        .model_name = "ethervox-lite",
        .max_tokens = 512,
        .context_length = 2048,
        .temperature = 0.7f,
        .top_p = 0.9f,
        .seed = 42,
        .use_gpu = false,  // Default to CPU for compatibility
        .gpu_layers = 0
    };
    
    #ifdef ETHERVOX_PLATFORM_DESKTOP
        config.max_tokens = 1024;
        config.context_length = 4096;
        config.use_gpu = true;
        config.gpu_layers = 10;
    #endif
    
    return config;
}

// Check if language is supported
bool ethervox_dialogue_is_language_supported(const char* language_code) {
    if (!language_code) return false;
    
    for (int i = 0; SUPPORTED_LANGUAGES[i] != NULL; i++) {
        if (strncmp(language_code, SUPPORTED_LANGUAGES[i], 2) == 0) {
            return true;
        }
    }
    return false;
}

// Generate conversation ID
static char* generate_conversation_id(void) {
    static uint32_t counter = 0;
    char* id = (char*)malloc(32);
    snprintf(id, 32, "conv_%u_%lu", ++counter, (unsigned long)time(NULL));
    return id;
}

// Initialize dialogue engine
int ethervox_dialogue_init(ethervox_dialogue_engine_t* engine, const ethervox_llm_config_t* config) {
    if (!engine) return -1;
    
    memset(engine, 0, sizeof(ethervox_dialogue_engine_t));
    
    // Copy configuration
    if (config) {
        engine->llm_config = *config;
        if (config->model_path) {
            engine->llm_config.model_path = strdup(config->model_path);
        }
        if (config->model_name) {
            engine->llm_config.model_name = strdup(config->model_name);
        }
    } else {
        engine->llm_config = ethervox_dialogue_get_default_llm_config();
    }
    
    // Allocate context storage
    engine->max_contexts = 16;  // Support up to 16 simultaneous conversations
    engine->contexts = (ethervox_dialogue_context_t*)calloc(engine->max_contexts, 
                                                           sizeof(ethervox_dialogue_context_t));
    if (!engine->contexts) {
        return -1;
    }
    
    // Initialize intent patterns (simplified - in production would load from files)
    engine->intent_patterns = (void*)INTENT_PATTERNS;
    
    engine->is_initialized = true;
    printf("Dialogue engine initialized with %s model\n", 
           engine->llm_config.model_name ? engine->llm_config.model_name : "default");
    
    return 0;
}

// Cleanup dialogue engine
void ethervox_dialogue_cleanup(ethervox_dialogue_engine_t* engine) {
    if (!engine) return;
    
    // Cleanup contexts
    if (engine->contexts) {
        for (uint32_t i = 0; i < engine->max_contexts; i++) {
            ethervox_dialogue_context_t* ctx = &engine->contexts[i];
            if (ctx->conversation_id) {
                free(ctx->conversation_id);
            }
            if (ctx->user_id) {
                free(ctx->user_id);
            }
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
    
    engine->is_initialized = false;
    printf("Dialogue engine cleaned up\n");
}

// Parse intent from text
int ethervox_dialogue_parse_intent(ethervox_dialogue_engine_t* engine, const char* text, 
                                  const char* language_code, ethervox_intent_t* intent) {
    if (!engine || !text || !intent) return -1;
    
    memset(intent, 0, sizeof(ethervox_intent_t));
    
    // Copy input text
    intent->raw_text = strdup(text);
    intent->normalized_text = strdup(text);  // TODO: Implement normalization
    strncpy(intent->language_code, language_code ? language_code : "en", 7);
    
    // Simple pattern matching for intent detection
    intent->type = ETHERVOX_INTENT_UNKNOWN;
    intent->confidence = 0.0f;
    
    const intent_pattern_t* patterns = (const intent_pattern_t*)engine->intent_patterns;
    
    for (int i = 0; patterns[i].pattern != NULL; i++) {
        // Check if pattern matches language
        if (language_code && strcmp(patterns[i].language, language_code) != 0) {
            continue;
        }
        
        // Simple substring matching (in production, would use more sophisticated NLP)
        if (strstr(text, patterns[i].pattern) != NULL) {
            intent->type = patterns[i].intent_type;
            intent->confidence = 0.8f;  // Fixed confidence for demo
            break;
        }
    }
    
    // If no pattern matched, use unknown intent with low confidence
    if (intent->type == ETHERVOX_INTENT_UNKNOWN) {
        intent->confidence = 0.1f;
    }
    
    printf("Intent parsed: %s (confidence: %.2f)\n", 
           ethervox_intent_type_to_string(intent->type), intent->confidence);
    
    return 0;
}

// Process with LLM
int ethervox_dialogue_process_llm(ethervox_dialogue_engine_t* engine, const ethervox_intent_t* intent,
                                 const char* context_id, ethervox_llm_response_t* response) {
    if (!engine || !intent || !response) return -1;
    
    memset(response, 0, sizeof(ethervox_llm_response_t));
    
    // For demo purposes, generate simple responses based on intent type
    const char* response_text = NULL;
    
    switch (intent->type) {
        case ETHERVOX_INTENT_GREETING:
            if (strcmp(intent->language_code, "es") == 0) {
                response_text = "¡Hola! ¿En qué puedo ayudarte?";
            } else if (strcmp(intent->language_code, "zh") == 0) {
                response_text = "你好！我能为您做些什么？";
            } else {
                response_text = "Hello! How can I help you today?";
            }
            break;
            
        case ETHERVOX_INTENT_QUESTION:
            if (strcmp(intent->language_code, "es") == 0) {
                response_text = "Déjame pensar en eso. ¿Puedes ser más específico?";
            } else if (strcmp(intent->language_code, "zh") == 0) {
                response_text = "让我想想。您能更具体一些吗？";
            } else {
                response_text = "Let me think about that. Can you be more specific?";
            }
            break;
            
        case ETHERVOX_INTENT_COMMAND:
        case ETHERVOX_INTENT_CONTROL:
            if (strcmp(intent->language_code, "es") == 0) {
                response_text = "Entendido. Ejecutando comando.";
            } else if (strcmp(intent->language_code, "zh") == 0) {
                response_text = "明白了。正在执行命令。";
            } else {
                response_text = "Understood. Executing command.";
            }
            break;
            
        case ETHERVOX_INTENT_GOODBYE:
            if (strcmp(intent->language_code, "es") == 0) {
                response_text = "¡Hasta luego! Que tengas un buen día.";
            } else if (strcmp(intent->language_code, "zh") == 0) {
                response_text = "再见！祝您有美好的一天。";
            } else {
                response_text = "Goodbye! Have a great day.";
            }
            break;
            
        default:
            // For complex queries, indicate external LLM might be needed
            response->requires_external_llm = true;
            response->external_llm_prompt = strdup(intent->raw_text);
            
            if (strcmp(intent->language_code, "es") == 0) {
                response_text = "Lo siento, no entiendo completamente. ¿Podrías reformular?";
            } else if (strcmp(intent->language_code, "zh") == 0) {
                response_text = "抱歉，我不太理解。您能重新表述一下吗？";
            } else {
                response_text = "I'm sorry, I don't fully understand. Could you rephrase?";
            }
            break;
    }
    
    response->text = strdup(response_text);
    strncpy(response->language_code, intent->language_code, 7);
    response->confidence = 0.9f;
    response->processing_time_ms = 50;  // Simulated processing time
    response->token_count = strlen(response_text) / 4;  // Rough token estimate
    
    printf("LLM response generated: %s\n", response->text);
    
    return 0;
}

// Create dialogue context
int ethervox_dialogue_create_context(ethervox_dialogue_engine_t* engine, const char* user_id, 
                                    const char* language_code, char** context_id) {
    if (!engine || !user_id || !context_id) return -1;
    
    // Find available context slot
    for (uint32_t i = 0; i < engine->max_contexts; i++) {
        ethervox_dialogue_context_t* ctx = &engine->contexts[i];
        if (!ctx->conversation_id) {  // Empty slot
            ctx->conversation_id = generate_conversation_id();
            ctx->user_id = strdup(user_id);
            strncpy(ctx->current_language, language_code ? language_code : "en", 7);
            ctx->max_history = 20;  // Keep last 20 interactions
            ctx->conversation_history = (ethervox_intent_t*)calloc(ctx->max_history, 
                                                                  sizeof(ethervox_intent_t));
            ctx->last_interaction_time = time(NULL);
            
            *context_id = strdup(ctx->conversation_id);
            engine->active_contexts++;
            
            printf("Created dialogue context: %s for user: %s\n", ctx->conversation_id, user_id);
            return 0;
        }
    }
    
    return -1;  // No available slots
}

// Free intent structure
void ethervox_intent_free(ethervox_intent_t* intent) {
    if (!intent) return;
    
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
    if (!response) return;
    
    if (response->text) {
        free(response->text);
        response->text = NULL;
    }
    if (response->external_llm_prompt) {
        free(response->external_llm_prompt);
        response->external_llm_prompt = NULL;
    }
}

// Set external LLM callback
void ethervox_dialogue_set_external_llm_callback(ethervox_dialogue_engine_t* engine,
                                                ethervox_external_llm_callback_t callback, void* user_data) {
    if (engine) {
        engine->external_llm_callback = callback;
        engine->external_llm_user_data = user_data;
    }
}