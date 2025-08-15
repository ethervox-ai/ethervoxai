"""
🔬 EthervoxAI MicroPython Implementation

Privacy-first voice AI for microcontrollers, optimized for Raspberry Pi Pico
and compatible boards. Provides core voice processing capabilities in 
memory-constrained environments.

Key Features:
- Hardware abstraction for multiple MCU platforms
- Memory-optimized audio processing
- Lightweight AI model inference
- Power management for battery operation
- Protocol compatibility with other EthervoxAI implementations

Example Usage:
    ```python
    from ethervoxai import EthervoxAI
    
    # Initialize for Raspberry Pi Pico
    ai = EthervoxAI(board_type='pico')
    ai.initialize()
    
    # Start voice processing
    ai.start_listening()
    ```
"""

from .core.platform_detector import PlatformDetector
from .core.audio_manager import AudioManager
from .core.model_manager import ModelManager
from .core.inference_engine import InferenceEngine
from .core.privacy_manager import PrivacyManager

import gc
import time
import json
from collections import namedtuple

# Version info
__version__ = "1.0.0-micropython"
__author__ = "EthervoxAI Team"
__license__ = "MIT"

# Configuration tuple for memory efficiency
EthervoxConfig = namedtuple('EthervoxConfig', [
    'board_type', 'audio_sample_rate', 'audio_buffer_size',
    'model_name', 'power_mode', 'enable_wifi', 'debug_mode'
])

class EthervoxAI:
    """
    Main EthervoxAI class for microcontrollers
    
    Provides a unified interface for voice AI functionality while
    optimizing for memory constraints and real-time processing.
    """
    
    def __init__(self, board_type='auto', **kwargs):
        """
        Initialize EthervoxAI for microcontroller
        
        Args:
            board_type (str): Target board ('pico', 'pico_w', 'esp32', 'auto')
            **kwargs: Additional configuration options
        """
        # Force garbage collection before initialization
        gc.collect()
        
        # Create configuration
        self.config = EthervoxConfig(
            board_type=board_type,
            audio_sample_rate=kwargs.get('audio_sample_rate', 16000),
            audio_buffer_size=kwargs.get('audio_buffer_size', 1024),
            model_name=kwargs.get('model_name', 'tinyllama-pico'),
            power_mode=kwargs.get('power_mode', 'normal'),
            enable_wifi=kwargs.get('enable_wifi', False),
            debug_mode=kwargs.get('debug_mode', False)
        )
        
        # Initialize components (lazy loading)
        self._platform_detector = None
        self._audio_manager = None
        self._model_manager = None
        self._inference_engine = None
        self._privacy_manager = None
        
        # State tracking
        self._initialized = False
        self._listening = False
        self._last_activity = 0
        
        # Memory monitoring
        self._init_memory = gc.mem_free()
        
        if self.config.debug_mode:
            print(f"🔬 EthervoxAI v{__version__} initializing...")
            print(f"   Board: {self.config.board_type}")
            print(f"   Memory: {self._init_memory // 1024}KB free")
    
    def initialize(self):
        """Initialize the EthervoxAI system"""
        if self._initialized:
            return
            
        start_time = time.ticks_ms()
        
        try:
            # Initialize platform detection first
            self._platform_detector = PlatformDetector()
            capabilities = self._platform_detector.get_capabilities()
            
            if self.config.debug_mode:
                print(f"✅ Platform detected: {capabilities.board_type}")
                print(f"   Performance tier: {capabilities.performance_tier}")
                print(f"   Available memory: {capabilities.available_memory}KB")
            
            # Initialize components based on capabilities
            self._init_audio_manager(capabilities)
            self._init_model_manager(capabilities)
            self._init_inference_engine(capabilities)
            self._init_privacy_manager(capabilities)
            
            self._initialized = True
            self._last_activity = time.ticks_ms()
            
            init_time = time.ticks_diff(time.ticks_ms(), start_time)
            memory_used = self._init_memory - gc.mem_free()
            
            if self.config.debug_mode:
                print(f"🎤 EthervoxAI initialized in {init_time}ms")
                print(f"   Memory used: {memory_used // 1024}KB")
                print(f"   Remaining: {gc.mem_free() // 1024}KB")
                
        except Exception as e:
            print(f"❌ Initialization failed: {e}")
            raise
    
    def _init_audio_manager(self, capabilities):
        """Initialize audio manager with board-specific configuration"""
        try:
            self._audio_manager = AudioManager(
                board_type=capabilities.board_type,
                sample_rate=self.config.audio_sample_rate,
                buffer_size=self.config.audio_buffer_size,
                debug=self.config.debug_mode
            )
            
            if self.config.debug_mode:
                print("✅ Audio manager initialized")
                
        except Exception as e:
            print(f"⚠️ Audio manager initialization failed: {e}")
            self._audio_manager = None
    
    def _init_model_manager(self, capabilities):
        """Initialize model manager with memory constraints"""
        try:
            self._model_manager = ModelManager(
                max_model_size=capabilities.max_model_size,
                performance_tier=capabilities.performance_tier,
                debug=self.config.debug_mode
            )
            
            if self.config.debug_mode:
                print("✅ Model manager initialized")
                
        except Exception as e:
            print(f"⚠️ Model manager initialization failed: {e}")
            self._model_manager = None
    
    def _init_inference_engine(self, capabilities):
        """Initialize inference engine"""
        try:
            self._inference_engine = InferenceEngine(
                max_context_length=capabilities.max_context_length,
                performance_tier=capabilities.performance_tier,
                debug=self.config.debug_mode
            )
            
            if self.config.debug_mode:
                print("✅ Inference engine initialized")
                
        except Exception as e:
            print(f"⚠️ Inference engine initialization failed: {e}")
            self._inference_engine = None
    
    def _init_privacy_manager(self, capabilities):
        """Initialize privacy manager"""
        try:
            self._privacy_manager = PrivacyManager(
                enable_logging=capabilities.platform != "micropython_minimal",
                debug=self.config.debug_mode
            )
            
            if self.config.debug_mode:
                print("✅ Privacy manager initialized")
                
        except Exception as e:
            print(f"⚠️ Privacy manager initialization failed: {e}")
            self._privacy_manager = None
    
    def start_listening(self):
        """Start voice activity detection and processing"""
        if not self._initialized:
            self.initialize()
            
        if not self._audio_manager:
            raise RuntimeError("Audio manager not available")
            
        self._listening = True
        self._last_activity = time.ticks_ms()
        
        if self.config.debug_mode:
            print("🎤 Started listening for voice input...")
            
        try:
            # Start audio capture
            self._audio_manager.start_capture()
            
            # Main processing loop
            while self._listening:
                audio_data = self._audio_manager.read_audio()
                
                if audio_data:
                    self._process_audio(audio_data)
                    self._last_activity = time.ticks_ms()
                
                # Check for power management
                self._check_power_management()
                
                # Yield to other tasks
                time.sleep_ms(10)
                
        except KeyboardInterrupt:
            print("⏹️ Stopping voice processing...")
            self.stop_listening()
        except Exception as e:
            print(f"❌ Error during voice processing: {e}")
            self.stop_listening()
    
    def stop_listening(self):
        """Stop voice processing"""
        self._listening = False
        
        if self._audio_manager:
            self._audio_manager.stop_capture()
            
        if self.config.debug_mode:
            print("⏹️ Voice processing stopped")
    
    def _process_audio(self, audio_data):
        """Process audio data through AI pipeline"""
        try:
            # Voice activity detection (simple energy-based)
            if not self._has_voice_activity(audio_data):
                return
                
            if self.config.debug_mode:
                print("🗣️ Voice activity detected")
            
            # Run inference if we have an engine
            if self._inference_engine:
                result = self._inference_engine.process_audio(audio_data)
                
                if result:
                    self._handle_inference_result(result)
                    
        except Exception as e:
            if self.config.debug_mode:
                print(f"⚠️ Audio processing error: {e}")
    
    def _has_voice_activity(self, audio_data):
        """Simple voice activity detection based on energy"""
        # Calculate RMS energy
        energy = sum(sample ** 2 for sample in audio_data) / len(audio_data)
        
        # Simple threshold-based VAD
        threshold = 1000  # Adjust based on microphone sensitivity
        return energy > threshold
    
    def _handle_inference_result(self, result):
        """Handle inference results"""
        try:
            if result.get('command'):
                if self.config.debug_mode:
                    print(f"🤖 Command recognized: {result['command']}")
                    
                # Log to privacy manager
                if self._privacy_manager:
                    self._privacy_manager.log_interaction(result)
                    
                # Generate response if needed
                if result.get('response'):
                    self._generate_response(result['response'])
                    
        except Exception as e:
            if self.config.debug_mode:
                print(f"⚠️ Result handling error: {e}")
    
    def _generate_response(self, response_text):
        """Generate audio response"""
        try:
            if self._audio_manager:
                # Simple TTS or pre-recorded responses
                self._audio_manager.play_response(response_text)
                
        except Exception as e:
            if self.config.debug_mode:
                print(f"⚠️ Response generation error: {e}")
    
    def _check_power_management(self):
        """Check if power management actions are needed"""
        if self.config.power_mode == 'low_power':
            current_time = time.ticks_ms()
            inactive_time = time.ticks_diff(current_time, self._last_activity)
            
            # Enter sleep mode after 30 seconds of inactivity
            if inactive_time > 30000:
                self._enter_low_power_mode()
    
    def _enter_low_power_mode(self):
        """Enter low power mode"""
        if self.config.debug_mode:
            print("💤 Entering low power mode...")
            
        # Stop non-essential components
        if self._audio_manager:
            self._audio_manager.enter_low_power()
            
        # Force garbage collection
        gc.collect()
    
    def get_status(self):
        """Get current system status"""
        status = {
            'initialized': self._initialized,
            'listening': self._listening,
            'memory_free': gc.mem_free(),
            'memory_used': self._init_memory - gc.mem_free() if self._init_memory else 0,
            'uptime': time.ticks_ms(),
            'components': {
                'platform_detector': self._platform_detector is not None,
                'audio_manager': self._audio_manager is not None,
                'model_manager': self._model_manager is not None,
                'inference_engine': self._inference_engine is not None,
                'privacy_manager': self._privacy_manager is not None
            }
        }
        
        if self._platform_detector:
            capabilities = self._platform_detector.get_capabilities()
            status['capabilities'] = {
                'board_type': capabilities.board_type,
                'performance_tier': capabilities.performance_tier,
                'total_memory': capabilities.total_memory,
                'available_memory': capabilities.available_memory
            }
            
        return status
    
    def cleanup(self):
        """Clean up resources"""
        self.stop_listening()
        
        # Clean up components
        if self._audio_manager:
            self._audio_manager.cleanup()
            
        if self._model_manager:
            self._model_manager.cleanup()
            
        # Force garbage collection
        gc.collect()
        
        if self.config.debug_mode:
            print("🧹 EthervoxAI cleanup completed")

# Convenience functions for quick setup
def create_pico_assistant(**kwargs):
    """Create EthervoxAI instance optimized for Raspberry Pi Pico"""
    return EthervoxAI(board_type='pico', **kwargs)

def create_pico_w_assistant(**kwargs):
    """Create EthervoxAI instance optimized for Raspberry Pi Pico W"""
    return EthervoxAI(board_type='pico_w', enable_wifi=True, **kwargs)

# Export main classes and functions
__all__ = [
    'EthervoxAI',
    'PlatformDetector',
    'AudioManager', 
    'ModelManager',
    'InferenceEngine',
    'PrivacyManager',
    'create_pico_assistant',
    'create_pico_w_assistant',
    'EthervoxConfig'
]
