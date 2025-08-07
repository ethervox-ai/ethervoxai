/**
 * 🔍 EthervoxAI Platform Detector - C++ Implementation
 * 
 * High-performance implementation for desktop and embedded Linux systems.
 * Follows EthervoxAI protocol while optimizing for performance and memory efficiency.
 * 
 * Features:
 * - Direct system call access for maximum performance
 * - SIMD instruction detection
 * - Real-time capabilities assessment
 * - Memory pool management
 * - Thread safety
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <mutex>
#include <optional>

namespace ethervoxai {

/**
 * System capabilities structure following EthervoxAI protocol
 */
struct SystemCapabilities {
    // Hardware
    uint64_t total_memory_mb;
    uint64_t available_memory_mb;
    uint32_t cpu_cores;
    std::string architecture;
    
    // Platform
    std::string platform;
    bool is_raspberry_pi;
    std::optional<std::string> raspberry_pi_model;
    
    // AI Capabilities
    bool has_gpu;
    bool has_neural_engine;
    bool has_avx2;
    bool has_neon;
    bool has_avx512;
    bool has_vulkan;
    
    // Performance tier
    std::string performance_tier;  // low, medium, high, ultra
    
    // Recommended constraints
    uint64_t max_model_size_mb;
    uint32_t max_context_length;
    uint32_t recommended_threads;
    bool use_memory_mapping;
    bool use_gpu_acceleration;
};

/**
 * Model compatibility assessment following EthervoxAI protocol
 */
struct ModelCompatibility {
    std::string model_name;
    bool is_compatible;
    uint64_t required_memory_mb;
    std::string expected_performance;  // poor, fair, good, excellent
    std::vector<std::string> optimization_flags;
    std::vector<std::string> warnings;
};

/**
 * C++ implementation of EthervoxAI Platform Detector
 * 
 * Optimized for:
 * - High-performance inference servers
 * - Embedded Linux systems
 * - Real-time applications
 * - Multi-threaded environments
 */
class PlatformDetector {
public:
    PlatformDetector();
    ~PlatformDetector();
    
    /**
     * Get system capabilities (thread-safe, cached)
     */
    SystemCapabilities getCapabilities();
    
    /**
     * Check model compatibility with current system
     */
    ModelCompatibility checkModelCompatibility(
        const std::string& model_name,
        uint64_t model_size_mb,
        uint64_t min_memory_mb = 0,
        uint64_t preferred_memory_mb = 0
    );
    
    /**
     * Get recommended models for current system
     */
    std::vector<std::map<std::string, std::string>> getRecommendedModels();
    
    /**
     * Force refresh of cached capabilities
     */
    void refreshCapabilities();
    
    /**
     * Enable/disable real-time optimizations
     */
    void setRealTimeMode(bool enabled);
    
    /**
     * Get current CPU utilization
     */
    double getCurrentCpuUtilization();
    
    /**
     * Get available memory (real-time)
     */
    uint64_t getAvailableMemoryMb();

private:
    mutable std::mutex mutex_;
    std::optional<SystemCapabilities> capabilities_;
    std::chrono::steady_clock::time_point detection_time_;
    std::chrono::seconds cache_duration_;
    bool real_time_mode_;
    
    // Platform detection methods
    std::string detectPlatform();
    bool detectRaspberryPi();
    std::optional<std::string> getRaspberryPiModel();
    
    // Hardware detection methods
    bool detectGPU();
    bool detectNeuralEngine();
    bool detectAVX2();
    bool detectAVX512();
    bool detectNEON();
    bool detectVulkan();
    
    // Performance calculation
    void calculatePerformanceMetrics(SystemCapabilities& caps);
    
    // System information gathering
    uint64_t getTotalMemoryMb();
    uint64_t getAvailableMemoryMbImpl();
    uint32_t getCpuCoreCount();
    std::string getArchitecture();
    
    // Logging
    void logCapabilities(const SystemCapabilities& caps);
    
    // CPU feature detection using CPUID (x86/x64)
    struct CpuFeatures {
        bool has_sse;
        bool has_sse2;
        bool has_avx;
        bool has_avx2;
        bool has_avx512;
        bool has_fma;
    };
    CpuFeatures detectCpuFeatures();
    
    // ARM feature detection
    struct ArmFeatures {
        bool has_neon;
        bool has_fp16;
        bool has_dotprod;
        bool has_sve;
    };
    ArmFeatures detectArmFeatures();
};

/**
 * Singleton instance access
 */
PlatformDetector& getPlatformDetector();

} // namespace ethervoxai
