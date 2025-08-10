#include <unity.h>
#include "inference_engine.h"
#include "config.h"

using namespace ethervoxai;

InferenceEngine engine;

void setUp(void) {
    // Set up before each test
}

void tearDown(void) {
    // Clean up after each test
}

void test_engine_initialization() {
    // Test engine initialization
    TEST_ASSERT_TRUE(engine.initialize());
    TEST_ASSERT_TRUE(engine.isInitialized());
    TEST_ASSERT_FALSE(engine.isModelLoaded()); // No model loaded yet
    TEST_ASSERT_EQUAL(0, engine.getInferenceCount());
}

void test_model_catalog() {
    // Test getting available models
    std::vector<String> models = engine.getAvailableModels();
    TEST_ASSERT_GREATER_THAN(0, models.size());
    
    // Check that expected mock models are present
    bool found_keyword_detector = false;
    bool found_intent_classifier = false;
    
    for (const String& model : models) {
        if (model == "keyword_detector") found_keyword_detector = true;
        if (model == "intent_classifier") found_intent_classifier = true;
    }
    
    TEST_ASSERT_TRUE(found_keyword_detector);
    TEST_ASSERT_TRUE(found_intent_classifier);
}

void test_model_loading() {
    // Test loading a valid model
    TEST_ASSERT_TRUE(engine.loadModel("keyword_detector"));
    TEST_ASSERT_TRUE(engine.isModelLoaded());
    TEST_ASSERT_EQUAL_STRING("keyword_detector", engine.getCurrentModel().c_str());
    
    // Test model info
    ModelInfo info = engine.getModelInfo("keyword_detector");
    TEST_ASSERT_EQUAL_STRING("keyword_detector", info.name.c_str());
    TEST_ASSERT_TRUE(info.is_loaded);
    TEST_ASSERT_GREATER_THAN(0, info.size_kb);
    TEST_ASSERT_GREATER_THAN(0.0, info.expected_accuracy);
    
    // Test loading non-existent model
    TEST_ASSERT_FALSE(engine.loadModel("non_existent_model"));
}

void test_inference_execution() {
    // Load a model first
    TEST_ASSERT_TRUE(engine.loadModel("keyword_detector"));
    
    // Prepare test data
    float input_data[INPUT_TENSOR_SIZE];
    float output_data[OUTPUT_TENSOR_SIZE];
    
    // Fill input with test pattern
    for (int i = 0; i < INPUT_TENSOR_SIZE; i++) {
        input_data[i] = sin(i * 0.1) * 0.5; // Sine wave pattern
    }
    
    // Run inference
    unsigned long start_time = millis();
    TEST_ASSERT_TRUE(engine.runInference(input_data, output_data));
    unsigned long inference_time = millis() - start_time;
    
    // Check that inference completed in reasonable time
    TEST_ASSERT_LESS_THAN(1000, inference_time); // Less than 1 second
    
    // Check that outputs are valid probabilities
    for (int i = 0; i < OUTPUT_TENSOR_SIZE; i++) {
        TEST_ASSERT_GREATER_OR_EQUAL(0.0, output_data[i]);
        TEST_ASSERT_LESS_OR_EQUAL(1.0, output_data[i]);
    }
    
    // Check that inference count increased
    TEST_ASSERT_EQUAL(1, engine.getInferenceCount());
}

void test_multiple_inferences() {
    // Load model
    TEST_ASSERT_TRUE(engine.loadModel("intent_classifier"));
    
    float input_data[INPUT_TENSOR_SIZE];
    float output_data[OUTPUT_TENSOR_SIZE];
    
    // Run multiple inferences
    const int num_inferences = 5;
    for (int iter = 0; iter < num_inferences; iter++) {
        // Create different input patterns
        for (int i = 0; i < INPUT_TENSOR_SIZE; i++) {
            input_data[i] = sin((i + iter * 10) * 0.1) * 0.5;
        }
        
        TEST_ASSERT_TRUE(engine.runInference(input_data, output_data));
    }
    
    // Check inference count
    TEST_ASSERT_EQUAL(num_inferences, engine.getInferenceCount());
    
    // Check performance stats
    JsonDocument stats = engine.getPerformanceStats();
    TEST_ASSERT_EQUAL(num_inferences, stats["inference_count"].as<uint32_t>());
    TEST_ASSERT_GREATER_THAN(0, stats["total_inference_time_ms"].as<uint32_t>());
    TEST_ASSERT_GREATER_THAN(0, stats["average_inference_time_ms"].as<uint32_t>());
}

void test_different_model_types() {
    float input_data[INPUT_TENSOR_SIZE];
    float output_data[OUTPUT_TENSOR_SIZE];
    
    // Fill input with voice-like pattern
    for (int i = 0; i < INPUT_TENSOR_SIZE; i++) {
        input_data[i] = (random(-100, 100) / 100.0) * 0.3; // Random noise
    }
    
    // Test keyword detection model
    TEST_ASSERT_TRUE(engine.loadModel("keyword_detector"));
    TEST_ASSERT_TRUE(engine.runInference(input_data, output_data));
    
    // Test intent classification model
    TEST_ASSERT_TRUE(engine.loadModel("intent_classifier"));
    TEST_ASSERT_TRUE(engine.runInference(input_data, output_data));
    
    // Test voice activity detection model
    TEST_ASSERT_TRUE(engine.loadModel("voice_activity"));
    TEST_ASSERT_TRUE(engine.runInference(input_data, output_data));
    
    // Each should produce different outputs (can't test exact values with mock)
    // But we can test that inference succeeds
}

void test_model_switching() {
    float input_data[INPUT_TENSOR_SIZE];
    float output_data[OUTPUT_TENSOR_SIZE];
    
    // Load first model
    TEST_ASSERT_TRUE(engine.loadModel("keyword_detector"));
    TEST_ASSERT_EQUAL_STRING("keyword_detector", engine.getCurrentModel().c_str());
    
    // Switch to different model
    TEST_ASSERT_TRUE(engine.loadModel("intent_classifier"));
    TEST_ASSERT_EQUAL_STRING("intent_classifier", engine.getCurrentModel().c_str());
    
    // Should still be able to run inference
    for (int i = 0; i < INPUT_TENSOR_SIZE; i++) {
        input_data[i] = 0.5; // Constant input
    }
    TEST_ASSERT_TRUE(engine.runInference(input_data, output_data));
}

void test_inference_without_model() {
    // Create a fresh engine instance for this test
    InferenceEngine test_engine;
    TEST_ASSERT_TRUE(test_engine.initialize());
    
    float input_data[INPUT_TENSOR_SIZE];
    float output_data[OUTPUT_TENSOR_SIZE];
    
    // Try to run inference without loading a model
    TEST_ASSERT_FALSE(test_engine.runInference(input_data, output_data));
    TEST_ASSERT_FALSE(test_engine.isModelLoaded());
}

void test_performance_consistency() {
    // Load model
    TEST_ASSERT_TRUE(engine.loadModel("keyword_detector"));
    
    float input_data[INPUT_TENSOR_SIZE];
    float output_data[OUTPUT_TENSOR_SIZE];
    
    // Fill with consistent input
    for (int i = 0; i < INPUT_TENSOR_SIZE; i++) {
        input_data[i] = 0.1; // Constant low-level input
    }
    
    // Run several inferences and check timing consistency
    std::vector<unsigned long> inference_times;
    
    for (int i = 0; i < 10; i++) {
        unsigned long start_time = millis();
        TEST_ASSERT_TRUE(engine.runInference(input_data, output_data));
        unsigned long inference_time = millis() - start_time;
        inference_times.push_back(inference_time);
    }
    
    // Check that all inference times are within reasonable bounds
    for (unsigned long time : inference_times) {
        TEST_ASSERT_LESS_THAN(500, time); // Less than 500ms each
        TEST_ASSERT_GREATER_THAN(0, time); // Should take some time
    }
}

void test_memory_usage() {
    // Get initial memory
    uint32_t initial_memory = esp_get_free_heap_size();
    
    // Initialize engine
    InferenceEngine test_engine;
    TEST_ASSERT_TRUE(test_engine.initialize());
    
    // Load model
    TEST_ASSERT_TRUE(test_engine.loadModel("tiny_llama"));
    
    // Check memory usage
    uint32_t after_init_memory = esp_get_free_heap_size();
    uint32_t memory_used = initial_memory - after_init_memory;
    
    // Should use some memory but not excessive
    TEST_ASSERT_GREATER_THAN(1000, memory_used);        // At least 1KB used
    TEST_ASSERT_LESS_THAN(100000, memory_used);         // Less than 100KB used
    
    // Memory usage should be reasonable for the tensor arena and buffers
    uint32_t expected_usage = (TENSOR_ARENA_SIZE_KB * 1024) + 
                             (INPUT_TENSOR_SIZE + OUTPUT_TENSOR_SIZE) * sizeof(float);
    TEST_ASSERT_LESS_THAN(expected_usage * 2, memory_used); // Within 2x expected
}

void setup() {
    delay(2000); // Wait for serial monitor
    
    UNITY_BEGIN();
    
    // Initialize the inference engine
    TEST_ASSERT_TRUE(engine.initialize());
    
    // Run tests
    RUN_TEST(test_engine_initialization);
    RUN_TEST(test_model_catalog);
    RUN_TEST(test_model_loading);
    RUN_TEST(test_inference_execution);
    RUN_TEST(test_multiple_inferences);
    RUN_TEST(test_different_model_types);
    RUN_TEST(test_model_switching);
    RUN_TEST(test_inference_without_model);
    RUN_TEST(test_performance_consistency);
    RUN_TEST(test_memory_usage);
    
    UNITY_END();
}

void loop() {
    // Empty - tests run once in setup()
}
