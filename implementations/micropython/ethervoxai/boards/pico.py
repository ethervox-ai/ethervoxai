"""
🍓 Raspberry Pi Pico Configuration

Hardware configuration and optimizations for the Raspberry Pi Pico
(RP2040 microcontroller without WiFi).

Hardware Specifications:
- MCU: RP2040 dual-core ARM Cortex-M0+ @ 133MHz
- Memory: 264KB SRAM
- Flash: 2MB (can be larger on some boards)
- GPIO: 26 pins available for I/O
- I2S: 2 PIO state machines for audio

Audio Hardware Recommendations:
- DAC: PCM5102A (I2S output)
- ADC: INMP441 (I2S MEMS microphone)
- AMP: MAX98357A (I2S amplifier)
"""

import gc
from collections import namedtuple

# Pin configuration structure
PinConfig = namedtuple('PinConfig', [
    'i2s_output_sck', 'i2s_output_ws', 'i2s_output_sd',
    'i2s_input_sck', 'i2s_input_ws', 'i2s_input_sd',
    'enable_pin', 'led_pin', 'button_pin'
])

# Performance configuration
PerformanceConfig = namedtuple('PerformanceConfig', [
    'cpu_freq_hz', 'max_model_size_kb', 'audio_buffer_size',
    'max_audio_sample_rate', 'recommended_quantization'
])

# Memory configuration
MemoryConfig = namedtuple('MemoryConfig', [
    'total_ram_kb', 'reserved_system_kb', 'max_model_cache_kb',
    'audio_buffer_kb', 'stack_size_kb'
])

class PicoConfig:
    """Configuration class for Raspberry Pi Pico"""
    
    def __init__(self):
        """Initialize Pico-specific configuration"""
        
        # Board identification
        self.board_name = "Raspberry Pi Pico"
        self.board_type = "pico"
        self.mcu_type = "RP2040"
        self.has_wifi = False
        self.has_bluetooth = False
        
        # Pin configuration for audio I/O
        self.pins = PinConfig(
            # I2S Output (DAC) - Using PIO0
            i2s_output_sck=16,    # Serial Clock (BCK)
            i2s_output_ws=17,     # Word Select (LRCK/WS)
            i2s_output_sd=18,     # Serial Data Output (DIN)
            
            # I2S Input (ADC) - Using PIO1
            i2s_input_sck=19,     # Serial Clock
            i2s_input_ws=20,      # Word Select
            i2s_input_sd=21,      # Serial Data Input (SD)
            
            # Control pins
            enable_pin=22,        # Audio codec enable/shutdown
            led_pin=25,          # Built-in LED
            button_pin=14        # User button for wake/interaction
        )
        
        # Performance characteristics
        self.performance = PerformanceConfig(
            cpu_freq_hz=133_000_000,      # 133MHz default
            max_model_size_kb=180,        # Conservative limit for 264KB RAM
            audio_buffer_size=512,        # Smaller buffer for memory efficiency
            max_audio_sample_rate=16000,  # 16kHz for voice processing
            recommended_quantization='4bit'  # Aggressive quantization needed
        )
        
        # Memory layout
        self.memory = MemoryConfig(
            total_ram_kb=264,             # 264KB total SRAM
            reserved_system_kb=50,        # MicroPython + system overhead
            max_model_cache_kb=150,       # Available for AI models
            audio_buffer_kb=20,           # Audio processing buffers
            stack_size_kb=8               # Call stack allocation
        )
        
        # Audio configuration
        self.audio_config = {
            'sample_rate': 16000,         # 16kHz for voice
            'channels': 1,                # Mono for efficiency
            'bits_per_sample': 16,        # 16-bit samples
            'buffer_size_samples': 512,   # Small buffer for low latency
            'i2s_format': 'I2S',          # Standard I2S format
            'endianness': 'little'        # RP2040 is little-endian
        }
        
        # Power management
        self.power_config = {
            'default_mode': 'balanced',
            'sleep_threshold_ms': 30000,   # 30 seconds to sleep
            'deep_sleep_threshold_ms': 300000,  # 5 minutes to deep sleep
            'wake_sources': ['button', 'audio'],
            'cpu_scaling': True,           # Dynamic frequency scaling
            'peripheral_power_down': True  # Power down unused peripherals
        }
        
        # AI model optimizations
        self.ai_config = {
            'max_models_cached': 1,        # Only one model in memory
            'streaming_inference': True,   # Process in chunks
            'quantization_support': ['4bit', '8bit'],
            'max_context_length': 64,      # Very limited context
            'use_fixed_point': True,       # Fixed-point math for speed
            'aggressive_gc': True          # Frequent garbage collection
        }
    
    def get_recommended_models(self):
        """Get list of models recommended for Pico"""
        return [
            {
                'name': 'wake-word-tiny',
                'size_kb': 8,
                'type': 'wake_word',
                'description': 'Ultra-lightweight wake word detection'
            },
            {
                'name': 'command-classifier',
                'size_kb': 45,
                'type': 'classification', 
                'description': 'Basic voice command recognition'
            },
            {
                'name': 'microllama',
                'size_kb': 80,
                'type': 'language_model',
                'description': 'Tiny language model for simple responses'
            }
        ]
    
    def validate_hardware(self):
        """Validate hardware configuration"""
        try:
            import machine
            
            validation_results = {
                'cpu_frequency': machine.freq(),
                'memory_free': gc.mem_free(),
                'pins_available': True,
                'i2s_capable': True,  # RP2040 PIO can handle I2S
                'validation_passed': True,
                'warnings': [],
                'errors': []
            }
            
            # Check memory
            free_memory_kb = gc.mem_free() // 1024
            if free_memory_kb < self.memory.reserved_system_kb:
                validation_results['warnings'].append(
                    f"Low memory: {free_memory_kb}KB free, "
                    f"need {self.memory.reserved_system_kb}KB minimum"
                )
            
            # Check CPU frequency
            actual_freq = machine.freq()
            if actual_freq != self.performance.cpu_freq_hz:
                validation_results['warnings'].append(
                    f"CPU frequency mismatch: {actual_freq}Hz vs "
                    f"expected {self.performance.cpu_freq_hz}Hz"
                )
            
            # Validate pins (basic check)
            try:
                # Test if we can create Pin objects for audio pins
                test_pin = machine.Pin(self.pins.led_pin, machine.Pin.OUT)
                test_pin.off()
                validation_results['pins_available'] = True
            except Exception as e:
                validation_results['errors'].append(f"Pin validation failed: {e}")
                validation_results['validation_passed'] = False
            
            return validation_results
            
        except Exception as e:
            return {
                'validation_passed': False,
                'errors': [f"Hardware validation failed: {e}"],
                'warnings': [],
                'cpu_frequency': 0,
                'memory_free': 0,
                'pins_available': False,
                'i2s_capable': False
            }
    
    def optimize_for_voice_processing(self):
        """Apply optimizations for voice processing workload"""
        try:
            import machine
            
            optimizations_applied = []
            
            # Set CPU frequency for optimal performance/power balance
            current_freq = machine.freq()
            if current_freq != self.performance.cpu_freq_hz:
                machine.freq(self.performance.cpu_freq_hz)
                optimizations_applied.append(f"CPU frequency set to {self.performance.cpu_freq_hz}Hz")
            
            # Configure garbage collection for real-time processing
            gc.threshold(1000)  # More frequent GC
            optimizations_applied.append("Garbage collection threshold lowered")
            
            # Pre-allocate audio buffers to avoid allocation during processing
            gc.collect()  # Clean up before allocation
            optimizations_applied.append("Memory optimized for audio processing")
            
            return {
                'success': True,
                'optimizations': optimizations_applied
            }
            
        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'optimizations': []
            }
    
    def get_power_profile(self, mode='balanced'):
        """Get power consumption profile for different modes"""
        profiles = {
            'high_performance': {
                'cpu_freq_hz': 133_000_000,
                'estimated_current_ma': 80,
                'audio_processing': True,
                'continuous_inference': True,
                'sleep_disabled': True
            },
            'balanced': {
                'cpu_freq_hz': 125_000_000,
                'estimated_current_ma': 50,
                'audio_processing': True,
                'continuous_inference': False,
                'sleep_threshold_ms': 30000
            },
            'low_power': {
                'cpu_freq_hz': 48_000_000,
                'estimated_current_ma': 20,
                'audio_processing': False,
                'continuous_inference': False,
                'sleep_threshold_ms': 10000
            },
            'ultra_low_power': {
                'cpu_freq_hz': 12_000_000,
                'estimated_current_ma': 5,
                'audio_processing': False,
                'continuous_inference': False,
                'sleep_threshold_ms': 5000
            }
        }
        
        return profiles.get(mode, profiles['balanced'])
    
    def create_audio_pins(self):
        """Create and configure audio I/O pins"""
        try:
            import machine
            
            # Create pin objects for I2S
            pins = {
                'output_sck': machine.Pin(self.pins.i2s_output_sck),
                'output_ws': machine.Pin(self.pins.i2s_output_ws),
                'output_sd': machine.Pin(self.pins.i2s_output_sd),
                'input_sck': machine.Pin(self.pins.i2s_input_sck),
                'input_ws': machine.Pin(self.pins.i2s_input_ws),
                'input_sd': machine.Pin(self.pins.i2s_input_sd),
                'enable': machine.Pin(self.pins.enable_pin, machine.Pin.OUT),
                'led': machine.Pin(self.pins.led_pin, machine.Pin.OUT),
                'button': machine.Pin(self.pins.button_pin, machine.Pin.IN, machine.Pin.PULL_UP)
            }
            
            # Enable audio codec
            pins['enable'].on()
            
            return pins
            
        except Exception as e:
            print(f"❌ Failed to create audio pins: {e}")
            return None
    
    def get_board_info(self):
        """Get comprehensive board information"""
        try:
            import machine
            import sys
            
            info = {
                'board_name': self.board_name,
                'board_type': self.board_type,
                'mcu_type': self.mcu_type,
                'platform': sys.platform,
                'cpu_freq_hz': machine.freq(),
                'memory_free_kb': gc.mem_free() // 1024,
                'unique_id': machine.unique_id().hex() if hasattr(machine, 'unique_id') else 'unknown',
                'has_wifi': self.has_wifi,
                'has_bluetooth': self.has_bluetooth,
                'audio_pins': {
                    'output': [self.pins.i2s_output_sck, self.pins.i2s_output_ws, self.pins.i2s_output_sd],
                    'input': [self.pins.i2s_input_sck, self.pins.i2s_input_ws, self.pins.i2s_input_sd]
                },
                'recommended_models': len(self.get_recommended_models()),
                'max_model_size_kb': self.performance.max_model_size_kb
            }
            
            return info
            
        except Exception as e:
            return {
                'board_name': self.board_name,
                'error': str(e)
            }
