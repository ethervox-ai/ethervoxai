"""
Board-specific configurations for EthervoxAI MicroPython implementation

This package contains board-specific configurations and optimizations
for different microcontroller platforms.

Supported Boards:
- Raspberry Pi Pico (RP2040)
- Raspberry Pi Pico W (RP2040 + WiFi)
- Raspberry Pi Pico W 2 (RP2350 + WiFi + Bluetooth)
- ESP32 family
- Future: STM32, Teensy 4.x

Each board module provides:
- Pin configuration for audio I/O
- Hardware-specific optimizations
- Performance tuning parameters
- Power management settings
- Connectivity features (WiFi, Bluetooth)
"""

# Lazy import to avoid issues when MicroPython modules aren't available
_board_configs = {}

def _lazy_import_board_config(board_type):
    """Lazy import board configuration to avoid MicroPython import errors"""
    if board_type in _board_configs:
        return _board_configs[board_type]
    
    try:
        if board_type == 'pico':
            from .pico import PicoConfig
            _board_configs[board_type] = PicoConfig
            return PicoConfig
        elif board_type == 'pico_w':
            from .pico_w import PicoWConfig
            _board_configs[board_type] = PicoWConfig
            return PicoWConfig
        elif board_type == 'pico_w2':
            from .pico_w2 import PicoW2Config
            _board_configs[board_type] = PicoW2Config
            return PicoW2Config
        elif board_type == 'esp32':
            from .esp32 import ESP32Config
            _board_configs[board_type] = ESP32Config
            return ESP32Config
        else:
            raise ValueError(f"Unsupported board type: {board_type}")
    except ImportError as e:
        print(f"⚠️  Warning: Could not import {board_type} config: {e}")
        return None

# Board detection and auto-configuration
def detect_board():
    """Automatically detect the current board type"""
    try:
        import sys
        
        # Check platform string first
        platform = sys.platform
        
        if 'esp32' in platform:
            return 'esp32'
        elif 'rp2' in platform:
            # Try to differentiate between Pico, Pico W, and Pico W 2
            try:
                import machine
                import network
                
                # Check for unique ID to differentiate RP2040 vs RP2350
                uid = machine.unique_id()
                if len(uid) >= 8:
                    # Check if this is RP2350 (Pico W 2) vs RP2040 (Pico/Pico W)
                    # RP2350 has different silicon revision patterns
                    uid_hex = uid.hex()
                    
                    # If network module is available and supports WLAN
                    wlan = network.WLAN(network.STA_IF)
                    if wlan is not None:
                        # Check for RP2350 characteristics (more sophisticated detection needed)
                        # For now, assume any RP2040 with WiFi is Pico W
                        # Real Pico W 2 detection would need firmware version checks
                        
                        # Temporary detection: check memory size
                        import gc
                        total_memory = gc.mem_free() + gc.mem_alloc()
                        
                        # Pico W 2 has ~520KB SRAM vs Pico W's ~264KB
                        if total_memory > 400000:  # > 400KB suggests Pico W 2
                            return 'pico_w2'
                        else:
                            return 'pico_w'
                    else:
                        # No WiFi, regular Pico
                        return 'pico'
            except (ImportError, AttributeError):
                # Fallback: no network, assume regular Pico
                return 'pico'
        else:
            print(f"⚠️  Unknown platform: {platform}, defaulting to pico")
            return 'pico'
            
    except Exception as e:
        print(f"⚠️  Board detection failed: {e}, defaulting to pico")
        return 'pico'

def get_board_config(board_type=None):
    """Get configuration for specified or detected board"""
    if board_type is None:
        board_type = detect_board()
    
    config_class = _lazy_import_board_config(board_type)
    if config_class is None:
        # Try fallback to pico config
        fallback_config = _lazy_import_board_config('pico')
        if fallback_config:
            print(f"⚠️ Using Pico config as fallback for {board_type}")
            return fallback_config()
        return None
        
    try:
        if board_type == 'esp32':
            # ESP32 may need variant detection
            return config_class()  # Will auto-detect variant
        else:
            return config_class()
    except Exception as e:
        print(f"❌ Failed to create {board_type} config: {e}")
        return None

def list_supported_boards():
    """Get list of supported board types"""
    return ['pico', 'pico_w', 'esp32']

def validate_board_support(board_type):
    """Check if a board type is supported"""
    return board_type in list_supported_boards()

__all__ = [
    'detect_board',
    'get_board_config',
    'list_supported_boards',
    'validate_board_support'
]
