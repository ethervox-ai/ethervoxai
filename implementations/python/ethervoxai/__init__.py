"""
🐍 EthervoxAI Python Implementation

A high-performance Python implementation of EthervoxAI optimized for:
- Scientific computing and research
- Raspberry Pi and embedded Linux systems  
- Server deployment and production use
- ML ecosystem integration

This implementation follows the EthervoxAI cross-language protocol while
leveraging Python's rich ecosystem and platform-specific optimizations.
"""

__version__ = "1.0.0"
__author__ = "EthervoxAI Team"
__license__ = "Apache-2.0"

from .platform_detector import PlatformDetector, SystemCapabilities, ModelCompatibility
from .model_manager import ModelManager, ModelInfo, DownloadProgress
from .inference_engine import InferenceEngine, InferenceResponse
from .local_llm_stack import LocalLLMStack

# Export main classes
__all__ = [
    'PlatformDetector', 'SystemCapabilities', 'ModelCompatibility',
    'ModelManager', 'ModelInfo', 'DownloadProgress', 
    'InferenceEngine', 'InferenceResponse',
    'LocalLLMStack'
]

# Singleton instances for easy access
platform_detector = PlatformDetector()
model_manager = ModelManager()
inference_engine = InferenceEngine()
local_llm_stack = LocalLLMStack()

def get_platform_detector():
    """Get the singleton PlatformDetector instance"""
    return platform_detector

def get_model_manager():
    """Get the singleton ModelManager instance"""
    return model_manager

def get_inference_engine():
    """Get the singleton InferenceEngine instance"""
    return inference_engine

def get_local_llm_stack():
    """Get the singleton LocalLLMStack instance"""
    return local_llm_stack
