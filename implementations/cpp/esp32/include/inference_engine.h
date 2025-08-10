/**
 * 🧠 EthervoxAI Inference Engine Header - ESP32 Implementation
 * 
 * Mock implementation for testing and development
 * Provides the same interface as the real TensorFlow Lite implementation
 */

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <map>
#include <vector>

namespace ethervoxai {

/**
 * Model information structure
 */
struct ModelInfo {
    String name;
    uint32_t size_kb;
    String description;
    bool is_loaded;
    float expected_accuracy;
    uint32_t average_inference_time_ms;
};

/**
 * Mock model information for testing
 */
struct MockModelInfo {
    uint32_t size_kb;
    String description;
    uint32_t inference_time_ms;
    float accuracy;
};

/**
 * AI Inference Engine for ESP32
 * 
 * Currently implements mock inference for testing
 * Will be replaced with TensorFlow Lite Micro in production
 */
class InferenceEngine {
public:
    InferenceEngine();
    ~InferenceEngine();
    
    /**
     * Initialize the inference engine
     */
    bool initialize();
    
    /**
     * Load an AI model by name
     */
    bool loadModel(const String& model_name);
    
    /**
     * Run inference on input data
     * @param input_data Input tensor data (size: INPUT_TENSOR_SIZE)
     * @param output_data Output tensor data (size: OUTPUT_TENSOR_SIZE)
     * @return true if inference successful
     */
    bool runInference(float* input_data, float* output_data);
    
    /**
     * Get list of available models
     */
    std::vector<String> getAvailableModels();
    
    /**
     * Get information about a specific model
     */
    ModelInfo getModelInfo(const String& model_name);
    
    /**
     * Get performance statistics
     */
    DynamicJsonDocument getPerformanceStats();
    
    /**
     * Check if engine is initialized
     */
    bool isInitialized() const { return _initialized; }
    
    /**
     * Check if a model is loaded
     */
    bool isModelLoaded() const { return _model_loaded; }
    
    /**
     * Get current model name
     */
    String getCurrentModel() const { return _current_model; }
    
    /**
     * Get inference count
     */
    uint32_t getInferenceCount() const { return _inference_count; }

private:
    // Engine state
    bool _initialized;
    bool _model_loaded;
    String _current_model;
    
    // Performance tracking
    uint32_t _inference_count;
    uint32_t _total_inference_time_ms;
    uint32_t _last_inference_time_ms;
    
    // Memory management
    uint8_t* _tensor_arena;
    float* _input_buffer;
    float* _output_buffer;
    
    // Mock model catalog
    std::map<std::string, MockModelInfo> _mock_models;
    
    // Mock inference methods
    bool performMockInference(float* input_data, float* output_data);
    void generateKeywordDetectionOutput(float* input_data, float* output_data);
    void generateIntentClassificationOutput(float* input_data, float* output_data);
    void generateVoiceActivityOutput(float* input_data, float* output_data);
    void generateConversationOutput(float* input_data, float* output_data);
    void generateGenericOutput(float* input_data, float* output_data);
    
    // Utility methods
    void updatePerformanceStats(unsigned long inference_time_ms);
    void cleanup();
};

} // namespace ethervoxai
