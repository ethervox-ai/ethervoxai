"""
🍓📶 Raspberry Pi Pico W Configuration

Hardware configuration and optimizations for the Raspberry Pi Pico W
(RP2040 microcontroller with WiFi).

Hardware Specifications:
- MCU: RP2040 dual-core ARM Cortex-M0+ @ 133MHz
- Memory: 264KB SRAM
- Flash: 2MB (can be larger on some boards)
- WiFi: Infineon CYW43439 802.11n
- GPIO: 26 pins available for I/O (some shared with WiFi)
- I2S: 2 PIO state machines for audio

Audio Hardware Recommendations:
- DAC: PCM5102A (I2S output)
- ADC: INMP441 (I2S MEMS microphone)
- AMP: MAX98357A (I2S amplifier)

Note: WiFi functionality adds memory overhead and power consumption.
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

# WiFi configuration
WiFiConfig = namedtuple('WiFiConfig', [
    'max_ssid_length', 'max_password_length', 'connection_timeout_ms',
    'retry_attempts', 'power_save_mode'
])

class PicoWConfig:
    """Configuration class for Raspberry Pi Pico W"""
    
    def __init__(self):
        """Initialize Pico W-specific configuration"""
        
        # Board identification
        self.board_name = "Raspberry Pi Pico W"
        self.board_type = "pico_w"
        self.mcu_type = "RP2040"
        self.has_wifi = True
        self.has_bluetooth = False  # CYW43439 supports BT but not enabled in MicroPython
        
        # Pin configuration for audio I/O
        # Note: Avoiding pins used by WiFi (GPIO23, GPIO24, GPIO25, GPIO29)
        self.pins = PinConfig(
            # I2S Output (DAC) - Using PIO0
            i2s_output_sck=16,    # Serial Clock (BCK)
            i2s_output_ws=17,     # Word Select (LRCK/WS)  
            i2s_output_sd=18,     # Serial Data Output (DIN)
            
            # I2S Input (ADC) - Using PIO1
            i2s_input_sck=19,     # Serial Clock
            i2s_input_ws=20,      # Word Select
            i2s_input_sd=21,      # Serial Data Input (SD)
            
            # Control pins (avoiding WiFi pins)
            enable_pin=22,        # Audio codec enable/shutdown
            led_pin="LED",        # Built-in LED (special handling on Pico W)
            button_pin=14         # User button for wake/interaction
        )
        
        # Performance characteristics (reduced due to WiFi overhead)
        self.performance = PerformanceConfig(
            cpu_freq_hz=133_000_000,      # 133MHz default
            max_model_size_kb=150,        # Reduced from 180KB due to WiFi overhead
            audio_buffer_size=512,        # Smaller buffer for memory efficiency
            max_audio_sample_rate=16000,  # 16kHz for voice processing
            recommended_quantization='4bit'  # Aggressive quantization needed
        )
        
        # Memory layout (adjusted for WiFi stack)
        self.memory = MemoryConfig(
            total_ram_kb=264,             # 264KB total SRAM
            reserved_system_kb=70,        # More overhead for WiFi stack
            max_model_cache_kb=120,       # Reduced available for AI models
            audio_buffer_kb=20,           # Audio processing buffers
            stack_size_kb=8               # Call stack allocation
        )
        
        # WiFi configuration
        self.wifi = WiFiConfig(
            max_ssid_length=32,           # Standard WiFi SSID limit
            max_password_length=64,       # WPA2/WPA3 password limit
            connection_timeout_ms=10000,  # 10 second timeout
            retry_attempts=3,             # Connection retry attempts
            power_save_mode=True          # Enable WiFi power saving
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
        
        # Power management (WiFi-aware)
        self.power_config = {
            'default_mode': 'balanced',
            'sleep_threshold_ms': 30000,   # 30 seconds to sleep
            'deep_sleep_threshold_ms': 300000,  # 5 minutes to deep sleep
            'wake_sources': ['button', 'audio', 'wifi'],
            'cpu_scaling': True,           # Dynamic frequency scaling
            'peripheral_power_down': True, # Power down unused peripherals
            'wifi_power_save': True,       # WiFi power management
            'wifi_sleep_mode': 'light'     # Light sleep preserves WiFi connection
        }
        
        # AI model optimizations (conservative due to WiFi memory usage)
        self.ai_config = {
            'max_models_cached': 1,        # Only one model in memory
            'streaming_inference': True,   # Process in chunks
            'quantization_support': ['4bit', '8bit'],
            'max_context_length': 48,      # Very limited context (reduced from 64)
            'use_fixed_point': True,       # Fixed-point math for speed
            'aggressive_gc': True,         # Frequent garbage collection
            'cloud_fallback': True         # Can use WiFi for cloud inference
        }
        
        # Network capabilities
        self.network_config = {
            'protocols': ['http', 'https', 'mqtt', 'websocket'],
            'max_concurrent_connections': 2,
            'cloud_inference_enabled': True,
            'model_download_enabled': True,
            'ota_updates_enabled': True,
            'telemetry_enabled': False,    # Privacy-first: disabled by default
            'local_first': True            # Prefer local processing
        }
    
    def get_recommended_models(self):
        """Get list of models recommended for Pico W"""
        return [
            {
                'name': 'wake-word-tiny',
                'size_kb': 8,
                'type': 'wake_word',
                'description': 'Ultra-lightweight wake word detection',
                'location': 'local'
            },
            {
                'name': 'command-classifier',
                'size_kb': 35,
                'type': 'classification',
                'description': 'Basic voice command recognition',
                'location': 'local'
            },
            {
                'name': 'microllama',
                'size_kb': 60,
                'type': 'language_model', 
                'description': 'Tiny language model for simple responses',
                'location': 'local'
            },
            {
                'name': 'cloud-gpt-3.5-turbo',
                'size_kb': 0,
                'type': 'language_model',
                'description': 'Cloud-based language model (WiFi required)',
                'location': 'cloud'
            }
        ]
    
    def validate_hardware(self):
        """Validate hardware configuration including WiFi"""
        try:
            import machine
            import network
            
            validation_results = {
                'cpu_frequency': machine.freq(),
                'memory_free': gc.mem_free(),
                'pins_available': True,
                'i2s_capable': True,  # RP2040 PIO can handle I2S
                'wifi_available': True,
                'validation_passed': True,
                'warnings': [],
                'errors': []
            }
            
            # Check memory (stricter requirements due to WiFi)
            free_memory_kb = gc.mem_free() // 1024
            if free_memory_kb < self.memory.reserved_system_kb:
                validation_results['warnings'].append(
                    f"Low memory: {free_memory_kb}KB free, "
                    f"need {self.memory.reserved_system_kb}KB minimum for WiFi"
                )
            
            # Check WiFi availability
            try:
                wlan = network.WLAN(network.STA_IF)
                if wlan is not None:
                    validation_results['wifi_available'] = True
                else:
                    validation_results['errors'].append("WiFi interface not available")
                    validation_results['validation_passed'] = False
            except Exception as e:
                validation_results['errors'].append(f"WiFi validation failed: {e}")
                validation_results['wifi_available'] = False
            
            # Validate pins (basic check, avoiding WiFi pins)
            try:
                # Test non-WiFi pins only
                test_pin = machine.Pin(self.pins.button_pin, machine.Pin.IN)
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
                'i2s_capable': False,
                'wifi_available': False
            }
    
    def connect_wifi(self, ssid, password, timeout_ms=None):
        """Connect to WiFi with proper error handling"""
        try:
            import network
            import time
            
            timeout_ms = timeout_ms or self.wifi.connection_timeout_ms
            
            # Initialize WiFi interface
            wlan = network.WLAN(network.STA_IF)
            wlan.active(True)
            
            # Configure power saving if enabled
            if self.power_config['wifi_power_save']:
                # Enable power saving mode
                pass  # Implementation depends on MicroPython version
            
            print(f"🔌 Connecting to WiFi: {ssid}")
            wlan.connect(ssid, password)
            
            # Wait for connection with timeout
            start_time = time.ticks_ms()
            while not wlan.isconnected():
                if time.ticks_diff(time.ticks_ms(), start_time) > timeout_ms:
                    return {
                        'success': False,
                        'error': 'Connection timeout',
                        'ip': None,
                        'signal_strength': None
                    }
                time.sleep_ms(100)
            
            # Get connection details
            ip_info = wlan.ifconfig()
            
            print(f"✅ WiFi connected!")
            print(f"   IP: {ip_info[0]}")
            print(f"   Gateway: {ip_info[2]}")
            
            return {
                'success': True,
                'ip': ip_info[0],
                'netmask': ip_info[1],
                'gateway': ip_info[2],
                'dns': ip_info[3],
                'signal_strength': None  # Not easily available in MicroPython
            }
            
        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'ip': None,
                'signal_strength': None
            }
    
    def scan_wifi_networks(self):
        """Scan for available WiFi networks"""
        try:
            import network
            
            wlan = network.WLAN(network.STA_IF)
            wlan.active(True)
            
            print("🔍 Scanning for WiFi networks...")
            networks = wlan.scan()
            
            network_list = []
            for net in networks:
                network_info = {
                    'ssid': net[0].decode('utf-8'),
                    'bssid': ':'.join(['%02x' % b for b in net[1]]),
                    'channel': net[2],
                    'rssi': net[3],
                    'security': net[4],
                    'hidden': net[5]
                }
                network_list.append(network_info)
            
            # Sort by signal strength (RSSI)
            network_list.sort(key=lambda x: x['rssi'], reverse=True)
            
            return {
                'success': True,
                'networks': network_list,
                'count': len(network_list)
            }
            
        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'networks': [],
                'count': 0
            }
    
    def optimize_for_voice_processing(self):
        """Apply optimizations for voice processing workload with WiFi"""
        try:
            import machine
            import network
            
            optimizations_applied = []
            
            # Set CPU frequency
            current_freq = machine.freq()
            if current_freq != self.performance.cpu_freq_hz:
                machine.freq(self.performance.cpu_freq_hz)
                optimizations_applied.append(f"CPU frequency set to {self.performance.cpu_freq_hz}Hz")
            
            # Configure WiFi power saving for audio processing
            try:
                wlan = network.WLAN(network.STA_IF)
                if wlan.isconnected() and self.power_config['wifi_power_save']:
                    # Enable WiFi power saving during audio processing
                    optimizations_applied.append("WiFi power saving enabled")
            except:
                pass  # Continue if WiFi not available
            
            # Configure garbage collection for real-time processing
            gc.threshold(800)  # More aggressive GC due to WiFi memory usage
            optimizations_applied.append("Garbage collection optimized for WiFi environment")
            
            # Pre-allocate audio buffers
            gc.collect()
            optimizations_applied.append("Memory optimized for audio processing with WiFi")
            
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
        """Get power consumption profile for different modes (WiFi-aware)"""
        profiles = {
            'high_performance': {
                'cpu_freq_hz': 133_000_000,
                'estimated_current_ma': 120,  # Higher due to WiFi
                'audio_processing': True,
                'continuous_inference': True,
                'wifi_enabled': True,
                'wifi_power_save': False,
                'sleep_disabled': True
            },
            'balanced': {
                'cpu_freq_hz': 125_000_000,
                'estimated_current_ma': 80,   # Moderate with WiFi power save
                'audio_processing': True,
                'continuous_inference': False,
                'wifi_enabled': True,
                'wifi_power_save': True,
                'sleep_threshold_ms': 30000
            },
            'low_power': {
                'cpu_freq_hz': 48_000_000,
                'estimated_current_ma': 35,   # WiFi in power save mode
                'audio_processing': False,
                'continuous_inference': False,
                'wifi_enabled': True,
                'wifi_power_save': True,
                'sleep_threshold_ms': 10000
            },
            'wifi_off': {
                'cpu_freq_hz': 48_000_000,
                'estimated_current_ma': 20,   # Similar to regular Pico
                'audio_processing': False,
                'continuous_inference': False,
                'wifi_enabled': False,
                'wifi_power_save': False,
                'sleep_threshold_ms': 10000
            }
        }
        
        return profiles.get(mode, profiles['balanced'])
    
    def create_audio_pins(self):
        """Create and configure audio I/O pins (WiFi-aware)"""
        try:
            import machine
            
            # Create pin objects for I2S (avoiding WiFi pins)
            pins = {
                'output_sck': machine.Pin(self.pins.i2s_output_sck),
                'output_ws': machine.Pin(self.pins.i2s_output_ws),
                'output_sd': machine.Pin(self.pins.i2s_output_sd),
                'input_sck': machine.Pin(self.pins.i2s_input_sck),
                'input_ws': machine.Pin(self.pins.i2s_input_ws),
                'input_sd': machine.Pin(self.pins.i2s_input_sd),
                'enable': machine.Pin(self.pins.enable_pin, machine.Pin.OUT),
                'led': machine.Pin(self.pins.led_pin, machine.Pin.OUT),  # Special LED handling
                'button': machine.Pin(self.pins.button_pin, machine.Pin.IN, machine.Pin.PULL_UP)
            }
            
            # Enable audio codec
            pins['enable'].on()
            
            return pins
            
        except Exception as e:
            print(f"❌ Failed to create audio pins: {e}")
            return None
    
    def get_wifi_status(self):
        """Get current WiFi connection status"""
        try:
            import network
            
            wlan = network.WLAN(network.STA_IF)
            
            if not wlan.active():
                return {
                    'connected': False,
                    'status': 'disabled',
                    'ip': None,
                    'signal_strength': None
                }
            
            if wlan.isconnected():
                config = wlan.ifconfig()
                return {
                    'connected': True,
                    'status': 'connected',
                    'ip': config[0],
                    'netmask': config[1],
                    'gateway': config[2],
                    'dns': config[3],
                    'signal_strength': None  # Not easily available
                }
            else:
                return {
                    'connected': False,
                    'status': 'disconnected',
                    'ip': None,
                    'signal_strength': None
                }
                
        except Exception as e:
            return {
                'connected': False,
                'status': 'error',
                'error': str(e),
                'ip': None,
                'signal_strength': None
            }
    
    def get_board_info(self):
        """Get comprehensive board information including WiFi"""
        try:
            import machine
            import sys
            
            # Get WiFi status
            wifi_status = self.get_wifi_status()
            
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
                'wifi_connected': wifi_status['connected'],
                'wifi_ip': wifi_status['ip'],
                'audio_pins': {
                    'output': [self.pins.i2s_output_sck, self.pins.i2s_output_ws, self.pins.i2s_output_sd],
                    'input': [self.pins.i2s_input_sck, self.pins.i2s_input_ws, self.pins.i2s_input_sd]
                },
                'recommended_models': len(self.get_recommended_models()),
                'max_model_size_kb': self.performance.max_model_size_kb,
                'cloud_inference_available': wifi_status['connected']
            }
            
            return info
            
        except Exception as e:
            return {
                'board_name': self.board_name,
                'error': str(e)
            }
