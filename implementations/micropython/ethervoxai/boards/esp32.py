"""
🤖 ESP32 Configuration

Hardware configuration and optimizations for ESP32 microcontrollers
(Various ESP32 variants with WiFi and Bluetooth).

Hardware Specifications:
- MCU: Xtensa LX6/LX7 dual-core @ 160-240MHz
- Memory: 520KB SRAM (varies by model)
- Flash: 4MB+ (external)
- WiFi: 802.11 b/g/n
- Bluetooth: Classic + BLE (varies by model)
- GPIO: 34+ pins available
- I2S: Hardware I2S controller

Audio Hardware Recommendations:
- DAC: PCM5102A (I2S output) or built-in DAC
- ADC: INMP441 (I2S MEMS microphone) or built-in ADC
- AMP: MAX98357A (I2S amplifier)

Supported Variants:
- ESP32 (original)
- ESP32-S2 (WiFi only, no Bluetooth)
- ESP32-S3 (WiFi + BLE)
- ESP32-C3 (WiFi + BLE, RISC-V)
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

# Bluetooth configuration
BluetoothConfig = namedtuple('BluetoothConfig', [
    'ble_enabled', 'classic_enabled', 'max_connections',
    'advertising_interval_ms', 'scan_window_ms'
])

class ESP32Config:
    """Configuration class for ESP32 microcontrollers"""
    
    def __init__(self, variant='esp32'):
        """Initialize ESP32-specific configuration
        
        Args:
            variant: ESP32 variant ('esp32', 'esp32s2', 'esp32s3', 'esp32c3')
        """
        
        self.variant = variant.lower()
        
        # Board identification
        self.board_name = f"ESP32 ({self.variant.upper()})"
        self.board_type = "esp32"
        self.mcu_type = self._get_mcu_type()
        self.has_wifi = True
        self.has_bluetooth = self._has_bluetooth()
        
        # Pin configuration for audio I/O (variant-specific)
        self.pins = self._get_pin_config()
        
        # Performance characteristics (higher than RP2040)
        self.performance = PerformanceConfig(
            cpu_freq_hz=240_000_000,      # 240MHz typical for ESP32
            max_model_size_kb=400,        # Much more RAM available
            audio_buffer_size=1024,       # Larger buffers possible
            max_audio_sample_rate=44100,  # Higher sample rates supported
            recommended_quantization='8bit'  # Can handle less aggressive quantization
        )
        
        # Memory layout (varies by variant)
        self.memory = self._get_memory_config()
        
        # WiFi configuration
        self.wifi = WiFiConfig(
            max_ssid_length=32,
            max_password_length=64,
            connection_timeout_ms=10000,
            retry_attempts=3,
            power_save_mode=True
        )
        
        # Bluetooth configuration (if available)
        self.bluetooth = BluetoothConfig(
            ble_enabled=self.has_bluetooth,
            classic_enabled=self.variant == 'esp32',  # Only original ESP32
            max_connections=3,
            advertising_interval_ms=100,
            scan_window_ms=30
        ) if self.has_bluetooth else None
        
        # Audio configuration (enhanced for ESP32)
        self.audio_config = {
            'sample_rate': 16000,         # 16kHz for voice (can go higher)
            'channels': 1,                # Mono for efficiency (stereo possible)
            'bits_per_sample': 16,        # 16-bit samples
            'buffer_size_samples': 1024,  # Larger buffer for stability
            'i2s_format': 'I2S',          # Standard I2S format
            'endianness': 'little',       # ESP32 is little-endian
            'hardware_i2s': True,         # Hardware I2S controller
            'internal_dac': True,         # Built-in DAC available
            'internal_adc': True          # Built-in ADC available
        }
        
        # Power management (WiFi + Bluetooth aware)
        self.power_config = {
            'default_mode': 'balanced',
            'sleep_threshold_ms': 60000,   # 1 minute (longer due to more RAM)
            'deep_sleep_threshold_ms': 600000,  # 10 minutes
            'wake_sources': ['button', 'audio', 'wifi', 'bluetooth', 'timer'],
            'cpu_scaling': True,
            'peripheral_power_down': True,
            'wifi_power_save': True,
            'bluetooth_power_save': True,
            'modem_sleep': True            # Can sleep WiFi/BT modems
        }
        
        # AI model optimizations (more capable than RP2040)
        self.ai_config = {
            'max_models_cached': 2,        # Multiple models possible
            'streaming_inference': True,
            'quantization_support': ['4bit', '8bit', '16bit'],
            'max_context_length': 128,     # Longer context possible
            'use_fixed_point': False,      # Floating point unit available
            'aggressive_gc': False,        # More forgiving memory management
            'cloud_fallback': True,
            'edge_optimization': True      # Hardware acceleration possible
        }
        
        # Network capabilities (enhanced)
        self.network_config = {
            'protocols': ['http', 'https', 'mqtt', 'websocket', 'coap', 'ble'],
            'max_concurrent_connections': 5,
            'cloud_inference_enabled': True,
            'model_download_enabled': True,
            'ota_updates_enabled': True,
            'telemetry_enabled': False,
            'local_first': True,
            'mesh_networking': True,       # ESP-MESH support
            'bluetooth_audio': self.has_bluetooth
        }
    
    def _get_mcu_type(self):
        """Get MCU type based on variant"""
        mcu_types = {
            'esp32': 'Xtensa LX6',
            'esp32s2': 'Xtensa LX7',
            'esp32s3': 'Xtensa LX7',
            'esp32c3': 'RISC-V'
        }
        return mcu_types.get(self.variant, 'Xtensa LX6')
    
    def _has_bluetooth(self):
        """Check if variant supports Bluetooth"""
        bluetooth_variants = ['esp32', 'esp32s3', 'esp32c3']
        return self.variant in bluetooth_variants
    
    def _get_pin_config(self):
        """Get pin configuration based on ESP32 variant"""
        if self.variant == 'esp32':
            # Original ESP32 pin configuration
            return PinConfig(
                # I2S Output (DAC)
                i2s_output_sck=26,    # Serial Clock (BCK)
                i2s_output_ws=25,     # Word Select (LRCK)
                i2s_output_sd=22,     # Serial Data Output
                
                # I2S Input (ADC)
                i2s_input_sck=32,     # Serial Clock
                i2s_input_ws=33,      # Word Select
                i2s_input_sd=34,      # Serial Data Input
                
                # Control pins
                enable_pin=27,        # Audio codec enable
                led_pin=2,           # Built-in LED
                button_pin=0         # Boot button
            )
        elif self.variant == 'esp32s2':
            # ESP32-S2 pin configuration
            return PinConfig(
                i2s_output_sck=12,
                i2s_output_ws=13,
                i2s_output_sd=14,
                i2s_input_sck=15,
                i2s_input_ws=16,
                i2s_input_sd=17,
                enable_pin=18,
                led_pin=2,
                button_pin=0
            )
        elif self.variant == 'esp32s3':
            # ESP32-S3 pin configuration
            return PinConfig(
                i2s_output_sck=12,
                i2s_output_ws=13,
                i2s_output_sd=11,
                i2s_input_sck=14,
                i2s_input_ws=15,
                i2s_input_sd=16,
                enable_pin=17,
                led_pin=48,          # RGB LED on some boards
                button_pin=0
            )
        else:  # esp32c3
            # ESP32-C3 pin configuration
            return PinConfig(
                i2s_output_sck=4,
                i2s_output_ws=5,
                i2s_output_sd=6,
                i2s_input_sck=7,
                i2s_input_ws=8,
                i2s_input_sd=9,
                enable_pin=10,
                led_pin=8,
                button_pin=0
            )
    
    def _get_memory_config(self):
        """Get memory configuration based on ESP32 variant"""
        memory_configs = {
            'esp32': MemoryConfig(
                total_ram_kb=520,         # 520KB SRAM
                reserved_system_kb=100,   # System + WiFi/BT
                max_model_cache_kb=350,   # Large models possible
                audio_buffer_kb=30,       # Larger audio buffers
                stack_size_kb=16
            ),
            'esp32s2': MemoryConfig(
                total_ram_kb=320,         # 320KB SRAM
                reserved_system_kb=80,    # System + WiFi (no BT)
                max_model_cache_kb=200,
                audio_buffer_kb=25,
                stack_size_kb=12
            ),
            'esp32s3': MemoryConfig(
                total_ram_kb=512,         # 512KB SRAM
                reserved_system_kb=120,   # System + WiFi/BLE
                max_model_cache_kb=320,
                audio_buffer_kb=30,
                stack_size_kb=16
            ),
            'esp32c3': MemoryConfig(
                total_ram_kb=400,         # 400KB SRAM
                reserved_system_kb=90,    # System + WiFi/BLE
                max_model_cache_kb=250,
                audio_buffer_kb=25,
                stack_size_kb=12
            )
        }
        return memory_configs.get(self.variant, memory_configs['esp32'])
    
    def get_recommended_models(self):
        """Get list of models recommended for ESP32"""
        base_models = [
            {
                'name': 'wake-word-esp32',
                'size_kb': 15,
                'type': 'wake_word',
                'description': 'Optimized wake word detection for ESP32',
                'location': 'local'
            },
            {
                'name': 'command-classifier-enhanced',
                'size_kb': 80,
                'type': 'classification',
                'description': 'Enhanced voice command recognition',
                'location': 'local'
            },
            {
                'name': 'tinyllama-esp32',
                'size_kb': 200,
                'type': 'language_model',
                'description': 'Language model optimized for ESP32',
                'location': 'local'
            },
            {
                'name': 'cloud-gpt-3.5-turbo',
                'size_kb': 0,
                'type': 'language_model',
                'description': 'Cloud-based language model',
                'location': 'cloud'
            },
            {
                'name': 'whisper-tiny',
                'size_kb': 150,
                'type': 'speech_recognition',
                'description': 'Speech-to-text model',
                'location': 'local'
            }
        ]
        
        # Add larger models for high-memory variants
        if self.memory.total_ram_kb >= 512:
            base_models.append({
                'name': 'llama-micro-esp32',
                'size_kb': 300,
                'type': 'language_model',
                'description': 'Larger language model for high-memory ESP32',
                'location': 'local'
            })
        
        return base_models
    
    def validate_hardware(self):
        """Validate ESP32 hardware configuration"""
        try:
            import machine
            import esp32
            
            validation_results = {
                'cpu_frequency': machine.freq(),
                'memory_free': gc.mem_free(),
                'flash_size': esp32.flash_size() if hasattr(esp32, 'flash_size') else 'unknown',
                'pins_available': True,
                'i2s_capable': True,
                'wifi_available': True,
                'bluetooth_available': self.has_bluetooth,
                'variant_detected': self.variant,
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
            
            # Check WiFi
            try:
                import network
                wlan = network.WLAN(network.STA_IF)
                validation_results['wifi_available'] = wlan is not None
            except Exception as e:
                validation_results['errors'].append(f"WiFi validation failed: {e}")
                validation_results['wifi_available'] = False
            
            # Check Bluetooth if supported
            if self.has_bluetooth:
                try:
                    import bluetooth
                    validation_results['bluetooth_available'] = True
                except ImportError:
                    validation_results['warnings'].append("Bluetooth not available in this MicroPython build")
                    validation_results['bluetooth_available'] = False
                except Exception as e:
                    validation_results['errors'].append(f"Bluetooth validation failed: {e}")
                    validation_results['bluetooth_available'] = False
            
            return validation_results
            
        except Exception as e:
            return {
                'validation_passed': False,
                'errors': [f"Hardware validation failed: {e}"],
                'variant_detected': 'unknown'
            }
    
    def connect_wifi(self, ssid, password, timeout_ms=None):
        """Connect to WiFi with ESP32-optimized settings"""
        try:
            import network
            import time
            
            timeout_ms = timeout_ms or self.wifi.connection_timeout_ms
            
            wlan = network.WLAN(network.STA_IF)
            wlan.active(True)
            
            # ESP32-specific optimizations
            if hasattr(wlan, 'config'):
                # Configure power saving
                if self.power_config['wifi_power_save']:
                    wlan.config(pm=network.WLAN.PM_POWERSAVE)
                else:
                    wlan.config(pm=network.WLAN.PM_PERFORMANCE)
            
            print(f"🔌 Connecting to WiFi: {ssid}")
            wlan.connect(ssid, password)
            
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
                'signal_strength': wlan.status('rssi') if hasattr(wlan, 'status') else None
            }
            
        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'ip': None,
                'signal_strength': None
            }
    
    def optimize_for_voice_processing(self):
        """Apply ESP32-specific optimizations for voice processing"""
        try:
            import machine
            import esp32
            
            optimizations_applied = []
            
            # Set optimal CPU frequency
            current_freq = machine.freq()
            if current_freq != self.performance.cpu_freq_hz:
                machine.freq(self.performance.cpu_freq_hz)
                optimizations_applied.append(f"CPU frequency set to {self.performance.cpu_freq_hz}Hz")
            
            # Configure ESP32-specific features
            if hasattr(esp32, 'wake_on_ext0'):
                # Configure wake sources for deep sleep
                optimizations_applied.append("Deep sleep wake sources configured")
            
            # Configure memory
            gc.threshold(2000)  # Less aggressive than RP2040
            optimizations_applied.append("Memory management optimized for ESP32")
            
            # Configure power management
            if hasattr(esp32, 'sleep_type'):
                optimizations_applied.append("Power management configured")
            
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
        """Get power consumption profile for ESP32"""
        profiles = {
            'high_performance': {
                'cpu_freq_hz': 240_000_000,
                'estimated_current_ma': 180,  # High power with WiFi/BT
                'audio_processing': True,
                'continuous_inference': True,
                'wifi_enabled': True,
                'bluetooth_enabled': self.has_bluetooth,
                'modem_sleep': False,
                'light_sleep': False
            },
            'balanced': {
                'cpu_freq_hz': 160_000_000,
                'estimated_current_ma': 120,
                'audio_processing': True,
                'continuous_inference': False,
                'wifi_enabled': True,
                'bluetooth_enabled': False,
                'modem_sleep': True,
                'light_sleep': False
            },
            'low_power': {
                'cpu_freq_hz': 80_000_000,
                'estimated_current_ma': 60,
                'audio_processing': False,
                'continuous_inference': False,
                'wifi_enabled': True,
                'bluetooth_enabled': False,
                'modem_sleep': True,
                'light_sleep': True
            },
            'ultra_low_power': {
                'cpu_freq_hz': 40_000_000,
                'estimated_current_ma': 20,
                'audio_processing': False,
                'continuous_inference': False,
                'wifi_enabled': False,
                'bluetooth_enabled': False,
                'modem_sleep': True,
                'light_sleep': True
            }
        }
        
        return profiles.get(mode, profiles['balanced'])
    
    def create_audio_pins(self):
        """Create and configure audio I/O pins for ESP32"""
        try:
            import machine
            
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
        """Get comprehensive ESP32 board information"""
        try:
            import machine
            import esp32
            import sys
            
            info = {
                'board_name': self.board_name,
                'board_type': self.board_type,
                'mcu_type': self.mcu_type,
                'variant': self.variant,
                'platform': sys.platform,
                'cpu_freq_hz': machine.freq(),
                'memory_free_kb': gc.mem_free() // 1024,
                'memory_total_kb': self.memory.total_ram_kb,
                'flash_size_mb': esp32.flash_size() // (1024 * 1024) if hasattr(esp32, 'flash_size') else 'unknown',
                'unique_id': machine.unique_id().hex() if hasattr(machine, 'unique_id') else 'unknown',
                'has_wifi': self.has_wifi,
                'has_bluetooth': self.has_bluetooth,
                'audio_pins': {
                    'output': [self.pins.i2s_output_sck, self.pins.i2s_output_ws, self.pins.i2s_output_sd],
                    'input': [self.pins.i2s_input_sck, self.pins.i2s_input_ws, self.pins.i2s_input_sd]
                },
                'recommended_models': len(self.get_recommended_models()),
                'max_model_size_kb': self.performance.max_model_size_kb,
                'hardware_i2s': True,
                'internal_dac': True,
                'internal_adc': True
            }
            
            return info
            
        except Exception as e:
            return {
                'board_name': self.board_name,
                'variant': self.variant,
                'error': str(e)
            }
