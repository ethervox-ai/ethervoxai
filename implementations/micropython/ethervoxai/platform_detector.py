"""
🔍 EthervoxAI Platform Detector - MicroPython Implementation

Minimal implementation for microcontrollers (ESP32, Raspberry Pi Pico, etc.)
Follows EthervoxAI protocol while optimizing for memory-constrained environments.

This implementation focuses on:
- Minimal memory footprint
- Hardware abstraction
- Power efficiency
- Real-time constraints
"""

import gc
import json
import time
import machine
import micropython
from collections import namedtuple

# Use namedtuple for memory efficiency instead of dataclasses
SystemCapabilities = namedtuple('SystemCapabilities', [
    'total_memory', 'available_memory', 'cpu_freq', 'board_type',
    'platform', 'has_wifi', 'has_bluetooth', 'performance_tier',
    'max_model_size', 'max_context_length', 'power_mode'
])

ModelCompatibility = namedtuple('ModelCompatibility', [
    'model_name', 'is_compatible', 'required_memory', 
    'expected_performance', 'warnings'
])

class PlatformDetector:
    """
    MicroPython implementation optimized for microcontrollers
    
    Key differences from desktop versions:
    - Memory-optimized data structures
    - Board-specific hardware detection
    - Power management considerations
    - Limited model catalog focus
    """
    
    def __init__(self):
        self._capabilities = None
        self._detection_time = 0
        self._cache_duration = 300  # 5 minutes cache (slower devices)
        
        # Force garbage collection on initialization
        gc.collect()
        
    def get_capabilities(self):
        """Get system capabilities (cached, synchronous for MicroPython)"""
        current_time = time.ticks_ms()
        
        if (self._capabilities and 
            time.ticks_diff(current_time, self._detection_time) < self._cache_duration * 1000):
            return self._capabilities
            
        print("🔍 Detecting microcontroller capabilities...")
        
        # Memory detection
        gc.collect()  # Ensure clean measurement
        free_mem = gc.mem_free()
        alloc_mem = gc.mem_alloc()
        total_mem = free_mem + alloc_mem
        
        capabilities = SystemCapabilities(
            total_memory=total_mem // 1024,  # Convert to KB for MCUs
            available_memory=free_mem // 1024,
            cpu_freq=machine.freq(),
            board_type=self._detect_board_type(),
            platform="micropython",
            has_wifi=self._detect_wifi(),
            has_bluetooth=self._detect_bluetooth(), 
            performance_tier=self._calculate_performance_tier(total_mem, machine.freq()),
            max_model_size=self._calculate_max_model_size(free_mem),
            max_context_length=self._calculate_max_context(free_mem),
            power_mode="normal"  # Can be: normal, low_power, high_performance
        )
        
        self._capabilities = capabilities
        self._detection_time = current_time
        
        self._log_capabilities(capabilities)
        
        return capabilities
    
    def check_model_compatibility(self, model_name, model_size_kb, min_memory_kb=0):
        """Check model compatibility (sizes in KB for MCUs)"""
        capabilities = self.get_capabilities()
        
        # MCUs need significant overhead for inference
        required_memory = max(model_size_kb * 3, min_memory_kb)  # 3x overhead
        is_compatible = capabilities.available_memory >= required_memory
        
        # Performance assessment for MCUs
        if capabilities.performance_tier == "high":
            expected_performance = "good"
        elif capabilities.performance_tier == "medium": 
            expected_performance = "fair"
        else:
            expected_performance = "slow"
            
        warnings = []
        
        # MCU-specific warnings
        if model_size_kb > 500:  # 500KB is large for MCU
            warnings.append("Large model for microcontroller - expect slow inference")
            
        if capabilities.available_memory < 100:  # Less than 100KB free
            warnings.append("Very limited memory - model may not fit")
            
        if not capabilities.has_wifi and model_size_kb > 50:
            warnings.append("No WiFi - large model download not possible")
            
        return ModelCompatibility(
            model_name=model_name,
            is_compatible=is_compatible,
            required_memory=required_memory,
            expected_performance=expected_performance,
            warnings=warnings
        )
    
    def get_recommended_models(self):
        """Get MCU-appropriate model recommendations"""
        capabilities = self.get_capabilities()
        recommendations = []
        
        # For MCUs, focus on tiny models or edge inference
        if capabilities.available_memory >= 200:  # 200KB+
            recommendations.append({
                "name": "tinyllama-quantized-q2",
                "size": "150KB",
                "reason": "Ultra-lightweight for microcontrollers"
            })
            
        if capabilities.available_memory >= 100:  # 100KB+
            recommendations.append({
                "name": "edge-classifier-v1",
                "size": "80KB", 
                "reason": "Intent classification for voice commands"
            })
            
        # Always include minimal options
        recommendations.append({
            "name": "keyword-detector",
            "size": "20KB",
            "reason": "Wake word detection only"
        })
        
        if capabilities.has_wifi:
            recommendations.append({
                "name": "hybrid-edge-cloud",
                "size": "50KB",
                "reason": "Edge processing with cloud fallback"
            })
            
        return recommendations
    
    def set_power_mode(self, mode):
        """Set power mode: normal, low_power, high_performance"""
        if mode == "low_power":
            # Reduce CPU frequency for power saving
            machine.freq(80_000_000)  # 80MHz
        elif mode == "high_performance":
            # Increase CPU frequency for better performance
            machine.freq(240_000_000)  # 240MHz (ESP32 max)
        else:
            # Normal balanced mode
            machine.freq(160_000_000)  # 160MHz
            
        # Update cached capabilities
        if self._capabilities:
            # Create new tuple with updated values (namedtuples are immutable)
            caps_dict = self._capabilities._asdict()
            caps_dict['cpu_freq'] = machine.freq()
            caps_dict['power_mode'] = mode
            caps_dict['performance_tier'] = self._calculate_performance_tier(
                self._capabilities.total_memory * 1024, machine.freq()
            )
            self._capabilities = SystemCapabilities(**caps_dict)
    
    # ================== Private Methods ==================
    
    def _detect_board_type(self):
        """Detect specific microcontroller board"""
        try:
            # Try to identify board from machine module
            import sys
            platform = sys.platform
            
            if "esp32" in platform:
                return "esp32"
            elif "rp2" in platform:
                return "raspberry_pi_pico" 
            elif "pyboard" in platform:
                return "pyboard"
            else:
                return "unknown_mcu"
        except:
            return "unknown_mcu"
    
    def _detect_wifi(self):
        """Check for WiFi capability"""
        try:
            import network
            sta = network.WLAN(network.STA_IF)
            return True
        except:
            return False
    
    def _detect_bluetooth(self):
        """Check for Bluetooth capability"""
        try:
            import bluetooth
            return True
        except:
            return False
    
    def _calculate_performance_tier(self, total_memory_bytes, cpu_freq):
        """Calculate performance tier for MCU"""
        # Memory score (in bytes)
        memory_score = 3 if total_memory_bytes >= 500_000 else \
                      2 if total_memory_bytes >= 100_000 else 1
        
        # CPU frequency score
        freq_score = 3 if cpu_freq >= 200_000_000 else \
                    2 if cpu_freq >= 100_000_000 else 1
        
        total_score = memory_score + freq_score
        
        if total_score >= 5:
            return "high"
        elif total_score >= 3:
            return "medium"
        else:
            return "low"
    
    def _calculate_max_model_size(self, available_memory_bytes):
        """Calculate maximum model size for MCU (in KB)"""
        # Use 30% of available memory for model
        max_size_kb = (available_memory_bytes * 0.3) // 1024
        return max(int(max_size_kb), 10)  # At least 10KB
    
    def _calculate_max_context(self, available_memory_bytes):
        """Calculate maximum context length for MCU"""
        # Very limited context for MCUs
        if available_memory_bytes >= 200_000:  # 200KB+
            return 256
        elif available_memory_bytes >= 100_000:  # 100KB+
            return 128
        else:
            return 64
    
    def _log_capabilities(self, caps):
        """Log detected capabilities (memory-efficient)"""
        print("📊 MCU Capabilities:")
        print(f"   💾 Memory: {caps.available_memory}KB free / {caps.total_memory}KB total")
        print(f"   ⚡ CPU: {caps.cpu_freq // 1_000_000}MHz ({caps.board_type})")
        print(f"   🏗️  Platform: {caps.platform}")
        print(f"   📶 Performance: {caps.performance_tier.upper()}")
        print(f"   🧠 Max Model: {caps.max_model_size}KB")
        print(f"   📝 Max Context: {caps.max_context_length} tokens")
        print(f"   🔋 Power Mode: {caps.power_mode}")
        
        features = []
        if caps.has_wifi:
            features.append("WiFi")
        if caps.has_bluetooth:
            features.append("BLE")
            
        if features:
            print(f"   📡 Connectivity: {', '.join(features)}")

# Export singleton instance  
platform_detector = PlatformDetector()

# Memory optimization: pre-allocate emergency buffer
def emergency_free_memory():
    """Free memory in emergency situations"""
    gc.collect()
    micropython.mem_info()
    
# Utility function for memory monitoring
def monitor_memory():
    """Monitor memory usage for debugging"""
    gc.collect()
    free = gc.mem_free()
    alloc = gc.mem_alloc()
    print(f"Memory: {free}B free, {alloc}B allocated")
    return free, alloc
