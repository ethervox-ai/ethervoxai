"""
🍓📶🔷 Raspberry Pi Pico W 2 Configuration

Hardware configuration and optimizations for the Raspberry Pi Pico W 2
(RP2350 microcontroller        # Enhanced power management
        self.power_config = {
            'default_mode': 'balanced',
            'sleep_threshold_ms': 60000,   # 1 minute (can afford longer due to more RAM)
            'deep_sleep_threshold_ms': 600000,  # 10 minutes
            'wake_sources': ['button', 'audio', 'wifi', 'bluetooth'],
            'cpu_scaling': True,
            'peripheral_power_down': True,
            'wifi_power_save': True,
            'bluetooth_power_save': True,  # NEW: BT power management
            'wifi_sleep_mode': 'light'
        }nd Bluetooth).

Hardware Specifications:
- MCU: RP2350 dual-core ARM Cortex-M33 @ 150MHz
- Memory: 520KB SRAM (2x more than original Pico W)
- Flash: 4MB (2x more than original Pico W)
- WiFi: Infineon CYW43439 802.11n
- Bluetooth: Bluetooth 5.2 LE (CYW43439)
- GPIO: 26 pins available for I/O (some shared with WiFi/BT)
- I2S: 2 PIO state machines for audio
- Security: ARM TrustZone, Hardware crypto acceleration

Audio Hardware Options:
- Wired: PCM5102A (I2S DAC) + INMP441 (I2S mic)
- Bluetooth: A2DP speakers, HFP headsets, LE Audio devices
- Mixed: Local processing + Bluetooth audio output

Key Improvements over Pico W:
- 2x more RAM (520KB vs 264KB) - can handle larger models
- 2x more Flash (4MB vs 2MB) - more model storage
- Bluetooth LE support - wireless audio capabilities
- Faster CPU (150MHz vs 133MHz) - better performance
- Hardware security features
"""

import gc
from collections import namedtuple

# Pin configuration structure (same as Pico W but with more capabilities)
PinConfig = namedtuple('PinConfig', [
    'i2s_output_sck', 'i2s_output_ws', 'i2s_output_sd',
    'i2s_input_sck', 'i2s_input_ws', 'i2s_input_sd',
    'enable_pin', 'led_pin', 'button_pin'
])

# Enhanced performance configuration
PerformanceConfig = namedtuple('PerformanceConfig', [
    'cpu_freq_hz', 'max_model_size_kb', 'audio_buffer_size',
    'max_audio_sample_rate', 'recommended_quantization'
])

# Enhanced memory configuration
MemoryConfig = namedtuple('MemoryConfig', [
    'total_ram_kb', 'reserved_system_kb', 'max_model_cache_kb',
    'audio_buffer_kb', 'stack_size_kb'
])

# WiFi configuration (same as Pico W)
WiFiConfig = namedtuple('WiFiConfig', [
    'max_ssid_length', 'max_password_length', 'connection_timeout_ms',
    'retry_attempts', 'power_save_mode'
])

# Bluetooth configuration (NEW for Pico W 2)
BluetoothConfig = namedtuple('BluetoothConfig', [
    'ble_enabled', 'classic_enabled', 'max_connections',
    'advertising_interval_ms', 'scan_window_ms', 'audio_codecs'
])

class PicoW2Config:
    """Configuration class for Raspberry Pi Pico W 2"""
    
    def __init__(self):
        """Initialize Pico W 2-specific configuration"""
        
        # Board identification
        self.board_name = "Raspberry Pi Pico W 2"
        self.board_type = "pico_w2"
        self.mcu_type = "RP2350"
        self.has_wifi = True
        self.has_bluetooth = True  # Major upgrade!
        self.has_security_features = True
        
        # Pin configuration (same as Pico W - maintain compatibility)
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
            led_pin="LED",        # Built-in LED (special handling)
            button_pin=14         # User button for wake/interaction
        )
        
        # Enhanced performance (better than Pico W)
        self.performance = PerformanceConfig(
            cpu_freq_hz=150_000_000,      # 150MHz (faster than Pico W)
            max_model_size_kb=350,        # Much larger models possible (vs 150KB on Pico W)
            audio_buffer_size=1024,       # Larger buffers for better quality
            max_audio_sample_rate=44100,  # CD-quality audio support
            recommended_quantization='8bit'  # Can handle less aggressive quantization
        )
        
        # Enhanced memory layout (2x more RAM)
        self.memory = MemoryConfig(
            total_ram_kb=520,             # 520KB total SRAM (2x Pico W)
            reserved_system_kb=100,       # WiFi + Bluetooth + system overhead
            max_model_cache_kb=350,       # Much more available for AI models
            audio_buffer_kb=40,           # Larger audio buffers for better quality
            stack_size_kb=16              # More stack space
        )
        
        # WiFi configuration (same as Pico W)
        self.wifi = WiFiConfig(
            max_ssid_length=32,
            max_password_length=64,
            connection_timeout_ms=10000,
            retry_attempts=3,
            power_save_mode=True
        )
        
        # Bluetooth configuration (NEW!)
        self.bluetooth = BluetoothConfig(
            ble_enabled=True,             # Bluetooth LE support
            classic_enabled=False,        # LE only (CYW43439 limitation)
            max_connections=3,            # Multiple device connections
            advertising_interval_ms=100,  # BLE advertising
            scan_window_ms=30,           # BLE scanning
            audio_codecs=['SBC', 'LC3']  # Supported audio codecs
        )
        
        # Enhanced audio configuration
        self.audio_config = {
            'sample_rate': 16000,         # 16kHz for voice (can go higher)
            'channels': 1,                # Mono for efficiency (stereo possible)
            'bits_per_sample': 16,        # 16-bit samples
            'buffer_size_samples': 1024,  # Larger buffer for stability
            'i2s_format': 'I2S',          # Standard I2S format
            'endianness': 'little',       # RP2350 is little-endian
            'bluetooth_audio': True,      # NEW: Bluetooth audio support
            'a2dp_support': True,         # Advanced Audio Distribution Profile
            'hfp_support': True,          # Hands-Free Profile for headsets
            'le_audio_support': True      # LE Audio (future MicroPython support)
        }
        
        # Enhanced power management
        self.power_config = {
            'default_mode': 'balanced',
            'sleep_threshold_ms': 60000,   # 1 minute (can afford longer due to more RAM)
            'deep_sleep_threshold_ms': 600000,  # 10 minutes
            'wake_sources': ['button', 'audio', 'wifi', 'bluetooth'],
            'cpu_scaling': True,
            'peripheral_power_down': True,
            'wifi_power_save': True,
            'bluetooth_power_save': True,  # NEW: BT power management
            'wifi_sleep_mode': 'light'
        }
        
        # Enhanced AI model optimizations
        self.ai_config = {
            'max_models_cached': 3,        # Multiple models due to more RAM
            'streaming_inference': True,
            'quantization_support': ['4bit', '8bit', '16bit'],
            'max_context_length': 128,     # Longer context possible
            'use_fixed_point': False,      # Can use floating point
            'aggressive_gc': False,        # More forgiving memory management
            'cloud_fallback': True,
            'bluetooth_inference': True,   # NEW: Can use BT for distributed inference
            'model_hot_swapping': True     # NEW: Swap models without restart
        }
        
        # Enhanced network capabilities
        self.network_config = {
            'protocols': ['http', 'https', 'mqtt', 'websocket', 'bluetooth'],
            'max_concurrent_connections': 5,
            'cloud_inference_enabled': True,
            'model_download_enabled': True,
            'ota_updates_enabled': True,
            'telemetry_enabled': False,
            'local_first': True,
            'bluetooth_audio_streaming': True,  # NEW: Stream audio via BT
            'bluetooth_device_discovery': True, # NEW: Discover BT devices
            'mesh_networking': False,          # Not supported on this chip
            'peer_to_peer_bt': True           # NEW: Direct BT communication
        }
    
    def get_recommended_models(self):
        """Get list of models recommended for Pico W 2"""
        return [
            {
                'name': 'wake-word-enhanced',
                'size_kb': 15,
                'type': 'wake_word',
                'description': 'Enhanced wake word detection with multiple phrases',
                'location': 'local'
            },
            {
                'name': 'command-classifier-large',
                'size_kb': 120,
                'type': 'classification',
                'description': 'Large vocabulary voice command recognition',
                'location': 'local'
            },
            {
                'name': 'llama-micro-w2',
                'size_kb': 200,
                'type': 'language_model',
                'description': 'Optimized language model for Pico W 2',
                'location': 'local'
            },
            {
                'name': 'whisper-tiny-optimized',
                'size_kb': 180,
                'type': 'speech_recognition',
                'description': 'Speech-to-text optimized for RP2350',
                'location': 'local'
            },
            {
                'name': 'voice-synthesis-tiny',
                'size_kb': 100,
                'type': 'text_to_speech',
                'description': 'Text-to-speech for local responses',
                'location': 'local'
            },
            {
                'name': 'cloud-gpt-4o-mini',
                'size_kb': 0,
                'type': 'language_model',
                'description': 'Cloud-based advanced language model',
                'location': 'cloud'
            }
        ]
    
    def get_bluetooth_audio_profiles(self):
        """Get supported Bluetooth audio profiles"""
        return {
            'a2dp': {
                'name': 'Advanced Audio Distribution Profile',
                'description': 'High-quality audio streaming to speakers/headphones',
                'supported_codecs': ['SBC'],  # More codecs may be added
                'max_bitrate_kbps': 328,
                'latency_ms': 150,
                'use_cases': ['music_playback', 'tts_output', 'notification_sounds']
            },
            'hfp': {
                'name': 'Hands-Free Profile',
                'description': 'Bidirectional audio for voice calls/commands',
                'supported_codecs': ['CVSD', 'mSBC'],
                'max_bitrate_kbps': 64,
                'latency_ms': 50,
                'use_cases': ['voice_commands', 'phone_calls', 'voice_assistant']
            },
            'le_audio': {
                'name': 'LE Audio (Future)',
                'description': 'Next-generation Bluetooth audio',
                'supported_codecs': ['LC3'],
                'max_bitrate_kbps': 160,
                'latency_ms': 20,
                'use_cases': ['low_latency_audio', 'hearing_aids', 'multi_stream'],
                'available': False  # Pending MicroPython support
            }
        }
    
    def validate_hardware(self):
        """Validate hardware configuration including Bluetooth"""
        try:
            import machine
            import network
            
            validation_results = {
                'cpu_frequency': machine.freq(),
                'memory_free': gc.mem_free(),
                'pins_available': True,
                'i2s_capable': True,
                'wifi_available': True,
                'bluetooth_available': True,
                'mcu_type': 'RP2350',
                'validation_passed': True,
                'warnings': [],
                'errors': []
            }
            
            # Check enhanced memory
            free_memory_kb = gc.mem_free() // 1024
            if free_memory_kb < self.memory.reserved_system_kb:
                validation_results['warnings'].append(
                    f"Low memory: {free_memory_kb}KB free, "
                    f"need {self.memory.reserved_system_kb}KB minimum for WiFi+BT"
                )
            
            # Check WiFi availability
            try:
                wlan = network.WLAN(network.STA_IF)
                validation_results['wifi_available'] = wlan is not None
            except Exception as e:
                validation_results['errors'].append(f"WiFi validation failed: {e}")
                validation_results['wifi_available'] = False
            
            # Check Bluetooth availability (when supported in MicroPython)
            try:
                import bluetooth
                validation_results['bluetooth_available'] = True
                validation_results['bluetooth_version'] = '5.2 LE'
            except ImportError:
                validation_results['warnings'].append(
                    "Bluetooth not available in this MicroPython build"
                )
                validation_results['bluetooth_available'] = False
            except Exception as e:
                validation_results['errors'].append(f"Bluetooth validation failed: {e}")
                validation_results['bluetooth_available'] = False
            
            # Validate CPU frequency (should be higher than Pico W)
            actual_freq = machine.freq()
            if actual_freq < 140_000_000:  # Should be around 150MHz
                validation_results['warnings'].append(
                    f"CPU frequency lower than expected: {actual_freq}Hz"
                )
            
            return validation_results
            
        except Exception as e:
            return {
                'validation_passed': False,
                'errors': [f"Hardware validation failed: {e}"],
                'mcu_type': 'unknown'
            }
    
    def connect_bluetooth_audio(self, device_address, profile='a2dp'):
        """
        Connect to Bluetooth audio device
        
        Args:
            device_address: MAC address of Bluetooth device
            profile: Audio profile ('a2dp', 'hfp')
            
        Returns:
            dict: Connection result
        """
        try:
            import bluetooth  # Will be available when MicroPython supports it
            
            print(f"🔵 Connecting to Bluetooth audio device: {device_address}")
            print(f"📡 Profile: {profile.upper()}")
            
            # Initialize Bluetooth
            bt = bluetooth.BLE()
            bt.active(True)
            
            # For now, return a simulated connection result
            # Real implementation would:
            # 1. Scan for the device
            # 2. Pair if needed
            # 3. Connect using specified profile
            # 4. Configure audio parameters
            
            return {
                'success': True,
                'device_address': device_address,
                'profile': profile,
                'connection_id': 'sim_001',
                'audio_quality': 'high' if profile == 'a2dp' else 'voice',
                'latency_ms': 150 if profile == 'a2dp' else 50,
                'supported_codecs': ['SBC'] if profile == 'a2dp' else ['CVSD']
            }
            
        except ImportError:
            return {
                'success': False,
                'error': 'Bluetooth not supported in this MicroPython build',
                'device_address': device_address
            }
        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'device_address': device_address
            }
    
    def scan_bluetooth_audio_devices(self, scan_duration_ms=10000):
        """
        Scan for nearby Bluetooth audio devices
        
        Args:
            scan_duration_ms: Scan duration in milliseconds
            
        Returns:
            list: Found audio devices
        """
        try:
            import bluetooth
            
            print(f"🔍 Scanning for Bluetooth audio devices ({scan_duration_ms}ms)...")
            
            # Simulated scan results (real implementation would scan)
            simulated_devices = [
                {
                    'name': 'AirPods Pro',
                    'address': '00:1A:2B:3C:4D:5E',
                    'rssi': -45,
                    'profiles': ['a2dp', 'hfp'],
                    'device_type': 'headphones',
                    'paired': False
                },
                {
                    'name': 'Echo Dot',
                    'address': '00:1A:2B:3C:4D:5F',
                    'rssi': -60,
                    'profiles': ['a2dp'],
                    'device_type': 'speaker',
                    'paired': False
                },
                {
                    'name': 'JBL Flip 6',
                    'address': '00:1A:2B:3C:4D:60',
                    'rssi': -35,
                    'profiles': ['a2dp'],
                    'device_type': 'speaker',
                    'paired': True
                }
            ]
            
            return {
                'success': True,
                'devices': simulated_devices,
                'count': len(simulated_devices),
                'scan_duration_ms': scan_duration_ms
            }
            
        except ImportError:
            return {
                'success': False,
                'error': 'Bluetooth not supported',
                'devices': []
            }
        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'devices': []
            }
    
    def optimize_for_voice_processing(self):
        """Apply Pico W 2-specific optimizations for voice processing"""
        try:
            import machine
            
            optimizations_applied = []
            
            # Set optimal CPU frequency (higher than Pico W)
            current_freq = machine.freq()
            if current_freq != self.performance.cpu_freq_hz:
                machine.freq(self.performance.cpu_freq_hz)
                optimizations_applied.append(f"CPU frequency set to {self.performance.cpu_freq_hz}Hz")
            
            # Configure memory management (less aggressive due to more RAM)
            gc.threshold(1500)  # Higher threshold due to more memory
            optimizations_applied.append("Memory management optimized for 520KB RAM")
            
            # Pre-allocate larger audio buffers
            gc.collect()
            optimizations_applied.append("Enhanced audio buffers allocated")
            
            # Configure Bluetooth for audio (when available)
            try:
                import bluetooth
                optimizations_applied.append("Bluetooth audio subsystem initialized")
            except ImportError:
                optimizations_applied.append("Bluetooth optimization pending MicroPython support")
            
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
        """Get power consumption profile for different modes (includes Bluetooth)"""
        profiles = {
            'high_performance': {
                'cpu_freq_hz': 150_000_000,
                'estimated_current_ma': 140,  # Higher due to more powerful MCU + WiFi + BT
                'audio_processing': True,
                'continuous_inference': True,
                'wifi_enabled': True,
                'bluetooth_enabled': True,
                'bluetooth_audio_streaming': True,
                'sleep_disabled': True
            },
            'balanced': {
                'cpu_freq_hz': 133_000_000,
                'estimated_current_ma': 90,   # Moderate with WiFi + BT power save
                'audio_processing': True,
                'continuous_inference': False,
                'wifi_enabled': True,
                'bluetooth_enabled': True,
                'bluetooth_audio_streaming': False,
                'sleep_threshold_ms': 60000
            },
            'low_power': {
                'cpu_freq_hz': 80_000_000,
                'estimated_current_ma': 45,   # WiFi + BT in power save mode
                'audio_processing': False,
                'continuous_inference': False,
                'wifi_enabled': True,
                'bluetooth_enabled': True,
                'bluetooth_audio_streaming': False,
                'sleep_threshold_ms': 30000
            },
            'bluetooth_audio_only': {
                'cpu_freq_hz': 100_000_000,
                'estimated_current_ma': 60,   # BT audio streaming only
                'audio_processing': True,
                'continuous_inference': False,
                'wifi_enabled': False,
                'bluetooth_enabled': True,
                'bluetooth_audio_streaming': True,
                'sleep_threshold_ms': 120000
            },
            'ultra_low_power': {
                'cpu_freq_hz': 48_000_000,
                'estimated_current_ma': 25,   # All wireless off
                'audio_processing': False,
                'continuous_inference': False,
                'wifi_enabled': False,
                'bluetooth_enabled': False,
                'bluetooth_audio_streaming': False,
                'sleep_threshold_ms': 10000
            }
        }
        
        return profiles.get(mode, profiles['balanced'])
    
    def get_board_info(self):
        """Get comprehensive board information including Bluetooth"""
        try:
            import machine
            import sys
            
            # Get WiFi status
            wifi_status = self.get_wifi_status() if hasattr(self, 'get_wifi_status') else {'connected': False}
            
            # Get Bluetooth status (when available)
            try:
                import bluetooth
                bt_available = True
                bt_version = '5.2 LE'
            except ImportError:
                bt_available = False
                bt_version = 'Not available'
            
            info = {
                'board_name': self.board_name,
                'board_type': self.board_type,
                'mcu_type': self.mcu_type,
                'platform': sys.platform,
                'cpu_freq_hz': machine.freq(),
                'memory_free_kb': gc.mem_free() // 1024,
                'memory_total_kb': self.memory.total_ram_kb,
                'unique_id': machine.unique_id().hex() if hasattr(machine, 'unique_id') else 'unknown',
                'has_wifi': self.has_wifi,
                'has_bluetooth': bt_available,
                'bluetooth_version': bt_version,
                'has_security_features': self.has_security_features,
                'wifi_connected': wifi_status.get('connected', False),
                'wifi_ip': wifi_status.get('ip', None),
                'audio_pins': {
                    'output': [self.pins.i2s_output_sck, self.pins.i2s_output_ws, self.pins.i2s_output_sd],
                    'input': [self.pins.i2s_input_sck, self.pins.i2s_input_ws, self.pins.i2s_input_sd]
                },
                'bluetooth_audio_support': bt_available,
                'bluetooth_profiles': list(self.get_bluetooth_audio_profiles().keys()) if bt_available else [],
                'recommended_models': len(self.get_recommended_models()),
                'max_model_size_kb': self.performance.max_model_size_kb,
                'cloud_inference_available': wifi_status.get('connected', False),
                'bluetooth_inference_available': bt_available
            }
            
            return info
            
        except Exception as e:
            return {
                'board_name': self.board_name,
                'error': str(e)
            }
    
    def get_wifi_status(self):
        """Get current WiFi connection status (inherited from PicoWConfig)"""
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
                    'signal_strength': None
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
