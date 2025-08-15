"""
Core modules for EthervoxAI MicroPython implementation

This package contains the core functionality modules optimized for 
microcontrollers, particularly Raspberry Pi Pico and compatible boards.

Modules:
- platform_detector: Hardware detection and capability assessment
- audio_manager: I2S audio input/output management
- model_manager: Lightweight AI model management
- inference_engine: Optimized inference for constrained environments
- privacy_manager: Privacy controls and audit logging
"""

from .platform_detector import PlatformDetector
from .audio_manager import AudioManager
from .model_manager import ModelManager
from .inference_engine import InferenceEngine
from .privacy_manager import PrivacyManager

__all__ = [
    'PlatformDetector',
    'AudioManager',
    'ModelManager', 
    'InferenceEngine',
    'PrivacyManager'
]
