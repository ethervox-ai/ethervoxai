/**
 * 🧠 EthervoxAI Inference Engine - ESP32 Implementation
 * 
 * Mock implementation for testing and development
 * Will be replaced with real TensorFlow Lite Micro integration
 */

#include "inference_engine.h"
#include "config.h"
#include <Arduino.h>

namespace ethervoxai {

InferenceEngine::InferenceEngine() : 
    _initialized(false),
    _model_loaded(false),
    _current_model(""),
    _inference_count(0),
    _total_inference_time_ms(0),
    _last_inference_time_ms(0) {
}

InferenceEngine::~InferenceEngine() {
    if (_initialized) {
        cleanup();
    }
}

bool InferenceEngine::initialize() {
    LOG_INFO("🧠 Initializing AI Inference Engine...");
    
    // Check available memory
    uint32_t free_memory = esp_get_free_heap_size() / 1024; // KB
    if (free_memory < MIN_MEMORY_THRESHOLD_KB) {
        LOG_ERROR("Insufficient memory for AI engine: %d KB available", free_memory);
        return false;
    }
    
    // Initialize mock tensor arena
    _tensor_arena = (uint8_t*)malloc(TENSOR_ARENA_SIZE_KB * 1024);
    if (!_tensor_arena) {
        LOG_ERROR("Failed to allocate tensor arena");
        return false;
    }
    
    // Initialize mock input/output buffers
    _input_buffer = (float*)malloc(INPUT_TENSOR_SIZE * sizeof(float));
    _output_buffer = (float*)malloc(OUTPUT_TENSOR_SIZE * sizeof(float));
    
    if (!_input_buffer || !_output_buffer) {
        LOG_ERROR("Failed to allocate input/output buffers");
        cleanup();
        return false;
    }
    
    // Mock model metadata initialization
    _mock_models = {
        {"keyword_detector", {20, "keyword detection", 50, 0.95}},
        {"intent_classifier", {80, "intent classification", 120, 0.88}},
        {"voice_activity", {15, "voice activity detection", 30, 0.92}},
        {"tiny_llama", {150, "basic conversation", 800, 0.75}}
    };
    
    _initialized = true;
    LOG_INFO("✅ AI Inference Engine initialized successfully");
    LOG_INFO("📊 Tensor arena: %d KB, Free memory: %d KB", 
             TENSOR_ARENA_SIZE_KB, free_memory);
    
    return true;
}

bool InferenceEngine::loadModel(const String& model_name) {
    if (!_initialized) {
        LOG_ERROR("Engine not initialized");
        return false;
    }
    
    LOG_INFO("🔄 Loading model: %s", model_name.c_str());
    
    // Check if model exists in mock catalog
    auto it = _mock_models.find(model_name.c_str());
    if (it == _mock_models.end()) {
        LOG_ERROR("Model not found: %s", model_name.c_str());
        return false;
    }
    
    MockModelInfo& model = it->second;
    
    // Check memory requirements
    uint32_t free_memory = esp_get_free_heap_size() / 1024;
    if (model.size_kb > free_memory * 0.6) { // Use max 60% of free memory
        LOG_WARN("Model may be too large: %d KB required, %d KB available", 
                 model.size_kb, free_memory);
    }
    
    // Simulate model loading time
    delay(100 + (model.size_kb / 10)); // Realistic loading delay
    
    _current_model = model_name;
    _model_loaded = true;
    
    LOG_INFO("✅ Model loaded: %s (%d KB, %s)", 
             model_name.c_str(), model.size_kb, model.description.c_str());
    
    return true;
}

bool InferenceEngine::runInference(float* input_data, float* output_data) {
    if (!_model_loaded) {
        LOG_ERROR("No model loaded");
        return false;
    }
    
    unsigned long start_time = millis();
    
    // Mock inference processing
    bool success = performMockInference(input_data, output_data);
    
    unsigned long inference_time = millis() - start_time;
    updatePerformanceStats(inference_time);
    
    if (success) {
        LOG_DEBUG("🧠 Inference completed in %lu ms", inference_time);
    } else {
        LOG_ERROR("❌ Inference failed");
    }
    
    return success;
}

bool InferenceEngine::performMockInference(float* input_data, float* output_data) {
    auto it = _mock_models.find(_current_model.c_str());
    if (it == _mock_models.end()) {
        return false;
    }
    
    MockModelInfo& model = it->second;
    
    // Simulate processing time based on model complexity
    delay(model.inference_time_ms);
    
    // Generate mock outputs based on model type
    if (_current_model == "keyword_detector") {
        generateKeywordDetectionOutput(input_data, output_data);
    }
    else if (_current_model == "intent_classifier") {
        generateIntentClassificationOutput(input_data, output_data);
    }
    else if (_current_model == "voice_activity") {
        generateVoiceActivityOutput(input_data, output_data);
    }
    else if (_current_model == "tiny_llama") {
        generateConversationOutput(input_data, output_data);
    }
    else {
        // Generic classification output
        generateGenericOutput(input_data, output_data);
    }
    
    return true;
}

void InferenceEngine::generateKeywordDetectionOutput(float* input_data, float* output_data) {
    // Mock keyword detection: "hey", "ethervox", "stop", "help", etc.
    const char* keywords[] = {"silence", "hey", "ethervox", "stop", "help", "yes", "no"};
    int num_keywords = sizeof(keywords) / sizeof(keywords[0]);
    
    // Simulate realistic keyword detection
    float energy = 0.0;
    for (int i = 0; i < INPUT_TENSOR_SIZE; i++) {
        energy += abs(input_data[i]);
    }
    energy /= INPUT_TENSOR_SIZE;
    
    // Initialize all outputs to low probability
    for (int i = 0; i < OUTPUT_TENSOR_SIZE && i < num_keywords; i++) {
        output_data[i] = 0.01 + (random(0, 20) / 1000.0); // 0.01-0.03 base probability
    }
    
    // If there's significant energy, boost a random keyword
    if (energy > 0.1) {
        int detected_keyword = random(1, min(OUTPUT_TENSOR_SIZE, num_keywords));
        output_data[detected_keyword] = 0.7 + (random(0, 25) / 100.0); // 0.70-0.95
        LOG_DEBUG("🎯 Mock detected: %s (%.2f confidence)", 
                  keywords[detected_keyword], output_data[detected_keyword]);
    }
}

void InferenceEngine::generateIntentClassificationOutput(float* input_data, float* output_data) {
    // Mock intent classification: "lights", "music", "weather", "timer", etc.
    const char* intents[] = {"unknown", "lights", "music", "weather", "timer", "question", "greeting"};
    int num_intents = sizeof(intents) / sizeof(intents[0]);
    
    // Initialize with low probabilities
    for (int i = 0; i < OUTPUT_TENSOR_SIZE && i < num_intents; i++) {
        output_data[i] = 0.02 + (random(0, 15) / 1000.0);
    }
    
    // Randomly select a likely intent
    int predicted_intent = random(1, min(OUTPUT_TENSOR_SIZE, num_intents));
    output_data[predicted_intent] = 0.6 + (random(0, 30) / 100.0); // 0.60-0.90
    
    LOG_DEBUG("🎯 Mock intent: %s (%.2f confidence)", 
              intents[predicted_intent], output_data[predicted_intent]);
}

void InferenceEngine::generateVoiceActivityOutput(float* input_data, float* output_data) {
    // Voice Activity Detection: [silence, speech]
    float energy = 0.0;
    for (int i = 0; i < INPUT_TENSOR_SIZE; i++) {
        energy += abs(input_data[i]);
    }
    energy /= INPUT_TENSOR_SIZE;
    
    if (energy > 0.05) {
        output_data[0] = 0.1 + (random(0, 20) / 100.0);  // silence: 0.1-0.3
        output_data[1] = 0.7 + (random(0, 25) / 100.0);  // speech: 0.7-0.95
    } else {
        output_data[0] = 0.8 + (random(0, 15) / 100.0);  // silence: 0.8-0.95
        output_data[1] = 0.05 + (random(0, 10) / 100.0); // speech: 0.05-0.15
    }
    
    LOG_DEBUG("🎙️ Voice activity: %.2f speech, %.2f silence", 
              output_data[1], output_data[0]);
}

void InferenceEngine::generateConversationOutput(float* input_data, float* output_data) {
    // Mock conversation model - simplified token probabilities
    // For demo purposes, just generate some reasonable distribution
    
    float total_prob = 0.0;
    for (int i = 0; i < OUTPUT_TENSOR_SIZE; i++) {
        output_data[i] = random(1, 100) / 1000.0; // 0.001-0.100
        total_prob += output_data[i];
    }
    
    // Normalize to make it a proper probability distribution
    for (int i = 0; i < OUTPUT_TENSOR_SIZE; i++) {
        output_data[i] /= total_prob;
    }
    
    // Find the most likely token
    int max_token = 0;
    for (int i = 1; i < OUTPUT_TENSOR_SIZE; i++) {
        if (output_data[i] > output_data[max_token]) {
            max_token = i;
        }
    }
    
    LOG_DEBUG("💬 Conversation token %d (%.3f probability)", 
              max_token, output_data[max_token]);
}

void InferenceEngine::generateGenericOutput(float* input_data, float* output_data) {
    // Generic classifier output
    for (int i = 0; i < OUTPUT_TENSOR_SIZE; i++) {
        output_data[i] = random(0, 100) / 1000.0; // 0.000-0.100
    }
    
    // Make one class more likely
    int predicted_class = random(0, OUTPUT_TENSOR_SIZE);
    output_data[predicted_class] = 0.5 + (random(0, 40) / 100.0); // 0.50-0.90
}

std::vector<String> InferenceEngine::getAvailableModels() {
    std::vector<String> models;
    for (const auto& pair : _mock_models) {
        models.push_back(String(pair.first.c_str()));
    }
    return models;
}

ModelInfo InferenceEngine::getModelInfo(const String& model_name) {
    ModelInfo info;
    
    auto it = _mock_models.find(model_name.c_str());
    if (it != _mock_models.end()) {
        MockModelInfo& mock = it->second;
        info.name = model_name;
        info.size_kb = mock.size_kb;
        info.description = mock.description;
        info.is_loaded = (_current_model == model_name);
        info.expected_accuracy = mock.accuracy;
        info.average_inference_time_ms = mock.inference_time_ms;
    } else {
        info.name = "unknown";
        info.size_kb = 0;
        info.description = "Model not found";
        info.is_loaded = false;
        info.expected_accuracy = 0.0;
        info.average_inference_time_ms = 0;
    }
    
    return info;
}

DynamicJsonDocument InferenceEngine::getPerformanceStats() {
    DynamicJsonDocument doc(1024);
    
    doc["model_loaded"] = _model_loaded;
    doc["current_model"] = _current_model;
    doc["inference_count"] = _inference_count;
    doc["total_inference_time_ms"] = _total_inference_time_ms;
    doc["last_inference_time_ms"] = _last_inference_time_ms;
    
    if (_inference_count > 0) {
        doc["average_inference_time_ms"] = _total_inference_time_ms / _inference_count;
    } else {
        doc["average_inference_time_ms"] = 0;
    }
    
    doc["memory_usage_kb"] = (TENSOR_ARENA_SIZE_KB + 
                             ((INPUT_TENSOR_SIZE + OUTPUT_TENSOR_SIZE) * sizeof(float)) / 1024);
    
    return doc;
}

void InferenceEngine::updatePerformanceStats(unsigned long inference_time_ms) {
    _inference_count++;
    _total_inference_time_ms += inference_time_ms;
    _last_inference_time_ms = inference_time_ms;
}

void InferenceEngine::cleanup() {
    if (_tensor_arena) {
        free(_tensor_arena);
        _tensor_arena = nullptr;
    }
    
    if (_input_buffer) {
        free(_input_buffer);
        _input_buffer = nullptr;
    }
    
    if (_output_buffer) {
        free(_output_buffer);
        _output_buffer = nullptr;
    }
    
    _initialized = false;
    _model_loaded = false;
    _current_model = "";
    
    LOG_INFO("🧠 AI Inference Engine cleaned up");
}

} // namespace ethervoxai
