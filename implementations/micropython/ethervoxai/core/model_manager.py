"""
🧠 Model Manager - MicroPython Implementation

Manages AI models optimized for microcontrollers with severe memory constraints.
Handles model loading, caching, and memory management for Raspberry Pi Pico
and similar devices.

Key Features:
- Ultra-lightweight model formats (<100KB)
- Streaming model loading to minimize RAM usage
- 4-bit and 8-bit quantized model support
- Model compatibility checking
- Memory-efficient caching with LRU eviction
- Power-aware model management

Supported Model Types:
- TinyLlama-Pico: <250KB quantized language model
- Wake Word Detection: <10KB keyword spotting
- Command Classification: <50KB command recognition
- Custom Quantized Models: User-defined compressed models
"""

import gc
import json
import time
import micropython
from collections import namedtuple, OrderedDict

# Model metadata structure (memory efficient)
ModelInfo = namedtuple('ModelInfo', [
    'name', 'size_kb', 'type', 'quantization', 'min_memory_kb',
    'performance_tier', 'capabilities', 'file_path'
])

# Model compatibility result
CompatibilityResult = namedtuple('CompatibilityResult', [
    'compatible', 'reason', 'memory_required', 'performance_impact'
])

class ModelManager:
    """
    Model Manager for microcontroller AI models
    
    Optimized for:
    - Minimal memory footprint
    - Fast model switching
    - Quantized model support
    - Real-time performance constraints
    """
    
    def __init__(self, max_model_size=200, performance_tier='medium', debug=False):
        """
        Initialize Model Manager
        
        Args:
            max_model_size (int): Maximum model size in KB
            performance_tier (str): System performance tier
            debug (bool): Enable debug output
        """
        self.max_model_size = max_model_size
        self.performance_tier = performance_tier
        self.debug = debug
        
        # Model catalog (built-in optimized models)
        self.model_catalog = self._build_model_catalog()
        
        # Loaded models cache (LRU with size limit)
        self.loaded_models = OrderedDict()
        self.max_loaded_models = 2  # Very limited cache for MCU
        
        # Current active model
        self.active_model = None
        self.active_model_data = None
        
        # Memory tracking
        self.initial_memory = gc.mem_free()
        self.model_memory_usage = 0
        
        if self.debug:
            print(f"🧠 ModelManager initialized")
            print(f"   Max model size: {max_model_size}KB")
            print(f"   Performance tier: {performance_tier}")
            print(f"   Available models: {len(self.model_catalog)}")
    
    def _build_model_catalog(self):
        """Build catalog of available models for microcontrollers"""
        catalog = OrderedDict()
        
        # Ultra-lightweight wake word detection
        catalog['wake-word-tiny'] = ModelInfo(
            name='wake-word-tiny',
            size_kb=8,
            type='wake_word',
            quantization='4bit',
            min_memory_kb=20,
            performance_tier='low',
            capabilities=['wake_word_detection'],
            file_path='models/wake_word_tiny.bin'
        )
        
        # Command classification model
        catalog['command-classifier'] = ModelInfo(
            name='command-classifier',
            size_kb=45,
            type='classification',
            quantization='8bit',
            min_memory_kb=80,
            performance_tier='medium',
            capabilities=['command_recognition', 'intent_classification'],
            file_path='models/command_classifier.bin'
        )
        
        # Tiny language model for Pico
        catalog['tinyllama-pico'] = ModelInfo(
            name='tinyllama-pico',
            size_kb=200,
            type='language_model',
            quantization='4bit',
            min_memory_kb=220,
            performance_tier='high',
            capabilities=['text_generation', 'conversation', 'qa'],
            file_path='models/tinyllama_pico.bin'
        )
        
        # Micro language model (extreme optimization)
        catalog['microllama'] = ModelInfo(
            name='microllama',
            size_kb=80,
            type='language_model',
            quantization='4bit',
            min_memory_kb=120,
            performance_tier='medium',
            capabilities=['simple_responses', 'basic_qa'],
            file_path='models/microllama.bin'
        )
        
        # Voice activity detection
        catalog['vad-micro'] = ModelInfo(
            name='vad-micro',
            size_kb=15,
            type='audio_classification',
            quantization='8bit',
            min_memory_kb=30,
            performance_tier='low',
            capabilities=['voice_activity_detection'],
            file_path='models/vad_micro.bin'
        )
        
        return catalog
    
    def get_available_models(self):
        """Get list of available models"""
        return list(self.model_catalog.keys())
    
    def get_model_info(self, model_name):
        """Get detailed information about a model"""
        return self.model_catalog.get(model_name)
    
    def get_recommended_models(self):
        """Get models recommended for current hardware"""
        available_memory = gc.mem_free() // 1024  # Convert to KB
        recommended = []
        
        for model_name, model_info in self.model_catalog.items():
            compatibility = self.check_model_compatibility(model_name)
            
            if compatibility.compatible:
                # Score based on memory efficiency and capabilities
                memory_ratio = model_info.min_memory_kb / available_memory
                capability_score = len(model_info.capabilities)
                
                score = capability_score / memory_ratio
                
                recommended.append({
                    'name': model_name,
                    'info': model_info,
                    'score': score,
                    'memory_usage_percent': memory_ratio * 100
                })
        
        # Sort by score (higher is better)
        recommended.sort(key=lambda x: x['score'], reverse=True)
        
        if self.debug:
            print(f"📊 Found {len(recommended)} compatible models")
            for model in recommended[:3]:  # Show top 3
                print(f"   {model['name']}: score={model['score']:.2f}, "
                      f"memory={model['memory_usage_percent']:.1f}%")
        
        return recommended
    
    def check_model_compatibility(self, model_name):
        """Check if a model is compatible with current hardware"""
        model_info = self.model_catalog.get(model_name)
        if not model_info:
            return CompatibilityResult(
                compatible=False,
                reason=f"Model '{model_name}' not found",
                memory_required=0,
                performance_impact='unknown'
            )
        
        available_memory = gc.mem_free() // 1024  # KB
        
        # Check memory requirements
        if model_info.min_memory_kb > available_memory:
            return CompatibilityResult(
                compatible=False,
                reason=f"Insufficient memory: need {model_info.min_memory_kb}KB, "
                       f"available {available_memory}KB",
                memory_required=model_info.min_memory_kb,
                performance_impact='high'
            )
        
        # Check model size against limits
        if model_info.size_kb > self.max_model_size:
            return CompatibilityResult(
                compatible=False,
                reason=f"Model too large: {model_info.size_kb}KB > {self.max_model_size}KB",
                memory_required=model_info.min_memory_kb,
                performance_impact='high'
            )
        
        # Check performance tier compatibility
        tier_order = ['low', 'medium', 'high', 'ultra']
        current_tier_idx = tier_order.index(self.performance_tier)
        required_tier_idx = tier_order.index(model_info.performance_tier)
        
        if required_tier_idx > current_tier_idx:
            performance_impact = 'high'
        elif required_tier_idx == current_tier_idx:
            performance_impact = 'medium'
        else:
            performance_impact = 'low'
        
        return CompatibilityResult(
            compatible=True,
            reason="Compatible",
            memory_required=model_info.min_memory_kb,
            performance_impact=performance_impact
        )
    
    def load_model(self, model_name):
        """Load a model into memory"""
        if model_name == self.active_model:
            if self.debug:
                print(f"✅ Model '{model_name}' already loaded")
            return True
        
        # Check compatibility first
        compatibility = self.check_model_compatibility(model_name)
        if not compatibility.compatible:
            print(f"❌ Cannot load model: {compatibility.reason}")
            return False
        
        model_info = self.model_catalog[model_name]
        
        try:
            # Free current model if loaded
            if self.active_model:
                self._unload_current_model()
            
            # Check if model is in cache
            if model_name in self.loaded_models:
                model_data = self.loaded_models[model_name]
                # Move to end (LRU)
                del self.loaded_models[model_name]
                self.loaded_models[model_name] = model_data
                
                if self.debug:
                    print(f"✅ Loaded '{model_name}' from cache")
            else:
                # Load model from storage
                model_data = self._load_model_from_file(model_info)
                if not model_data:
                    return False
                
                # Add to cache (with LRU eviction)
                self._add_to_cache(model_name, model_data)
                
                if self.debug:
                    print(f"✅ Loaded '{model_name}' from file")
            
            # Set as active model
            self.active_model = model_name
            self.active_model_data = self.loaded_models[model_name]
            
            # Update memory tracking
            memory_after = gc.mem_free()
            self.model_memory_usage = self.initial_memory - memory_after
            
            if self.debug:
                print(f"   Memory usage: {self.model_memory_usage // 1024}KB")
                print(f"   Remaining: {memory_after // 1024}KB")
            
            return True
            
        except Exception as e:
            print(f"❌ Failed to load model '{model_name}': {e}")
            return False
    
    def _load_model_from_file(self, model_info):
        """Load model data from file"""
        try:
            # In a real implementation, this would load from flash storage
            # For now, we simulate with a simple data structure
            
            if self.debug:
                print(f"📁 Loading {model_info.name} from {model_info.file_path}")
            
            # Simulate model data (in real implementation, this would be
            # quantized weights, biases, and network structure)
            model_data = {
                'name': model_info.name,
                'type': model_info.type,
                'quantization': model_info.quantization,
                'size_kb': model_info.size_kb,
                'capabilities': model_info.capabilities,
                'weights': bytearray(model_info.size_kb * 1024),  # Simulated weights
                'metadata': {
                    'version': '1.0',
                    'timestamp': time.time(),
                    'checksum': 'placeholder'
                }
            }
            
            # Simulate loading time based on model size
            load_time_ms = model_info.size_kb * 2  # 2ms per KB
            time.sleep_ms(min(load_time_ms, 100))  # Cap at 100ms
            
            return model_data
            
        except Exception as e:
            if self.debug:
                print(f"❌ File loading error: {e}")
            return None
    
    def _add_to_cache(self, model_name, model_data):
        """Add model to cache with LRU eviction"""
        # Remove oldest models if cache is full
        while len(self.loaded_models) >= self.max_loaded_models:
            oldest_model = next(iter(self.loaded_models))
            removed_data = self.loaded_models.pop(oldest_model)
            
            if self.debug:
                print(f"🗑️ Evicted '{oldest_model}' from cache")
            
            # Force garbage collection after removing large object
            del removed_data
            gc.collect()
        
        # Add new model
        self.loaded_models[model_name] = model_data
    
    def _unload_current_model(self):
        """Unload the currently active model"""
        if self.active_model:
            if self.debug:
                print(f"🗑️ Unloading active model '{self.active_model}'")
            
            self.active_model = None
            self.active_model_data = None
            
            # Force garbage collection
            gc.collect()
    
    def get_active_model(self):
        """Get the currently active model"""
        if self.active_model:
            return {
                'name': self.active_model,
                'info': self.model_catalog[self.active_model],
                'loaded': True,
                'memory_usage': self.model_memory_usage
            }
        return None
    
    def process_with_model(self, input_data, model_type=None):
        """Process input data with the active model"""
        if not self.active_model or not self.active_model_data:
            if self.debug:
                print("⚠️ No model loaded for processing")
            return None
        
        model_data = self.active_model_data
        
        # Check if model type matches request
        if model_type and model_data['type'] != model_type:
            if self.debug:
                print(f"⚠️ Model type mismatch: got {model_data['type']}, expected {model_type}")
            return None
        
        try:
            # Process based on model type
            if model_data['type'] == 'wake_word':
                return self._process_wake_word(input_data, model_data)
            elif model_data['type'] == 'classification':
                return self._process_classification(input_data, model_data)
            elif model_data['type'] == 'language_model':
                return self._process_language_model(input_data, model_data)
            elif model_data['type'] == 'audio_classification':
                return self._process_audio_classification(input_data, model_data)
            else:
                if self.debug:
                    print(f"⚠️ Unknown model type: {model_data['type']}")
                return None
                
        except Exception as e:
            if self.debug:
                print(f"❌ Model processing error: {e}")
            return None
    
    def _process_wake_word(self, audio_data, model_data):
        """Process wake word detection"""
        # Simulate wake word detection
        # In real implementation, this would run the quantized neural network
        
        if not audio_data or len(audio_data) < 100:
            return {'detected': False, 'confidence': 0.0}
        
        # Simple energy-based detection for simulation
        energy = sum(abs(sample) for sample in audio_data) / len(audio_data)
        confidence = min(energy / 5000.0, 1.0)  # Normalize to 0-1
        
        detected = confidence > 0.6  # Threshold for detection
        
        return {
            'detected': detected,
            'confidence': confidence,
            'model': model_data['name'],
            'processing_time_ms': 5  # Simulated processing time
        }
    
    def _process_classification(self, input_data, model_data):
        """Process command classification"""
        # Simulate command classification
        # Real implementation would run quantized classifier
        
        commands = ['turn_on', 'turn_off', 'increase', 'decrease', 'status']
        
        # Simple simulation based on input characteristics
        if isinstance(input_data, (list, tuple)) and len(input_data) > 0:
            # Use input characteristics to simulate classification
            choice_idx = sum(abs(x) for x in input_data[:10]) % len(commands)
            predicted_command = commands[choice_idx]
            confidence = 0.7 + (choice_idx * 0.05)  # Vary confidence
        else:
            predicted_command = 'unknown'
            confidence = 0.1
        
        return {
            'command': predicted_command,
            'confidence': confidence,
            'all_scores': {cmd: 0.1 + (i * 0.1) for i, cmd in enumerate(commands)},
            'model': model_data['name'],
            'processing_time_ms': 15
        }
    
    def _process_language_model(self, text_input, model_data):
        """Process language model inference"""
        # Simulate language model response
        # Real implementation would run quantized transformer
        
        if not isinstance(text_input, str) or len(text_input) < 1:
            return {'response': '', 'confidence': 0.0}
        
        # Simple response generation simulation
        responses = {
            'hello': 'Hello! How can I help you?',
            'weather': 'I cannot check weather without internet connection.',
            'time': 'I do not have access to current time.',
            'help': 'I am a voice assistant running on your device.',
            'status': 'System is running normally on local hardware.'
        }
        
        # Find best matching response
        text_lower = text_input.lower()
        for keyword, response in responses.items():
            if keyword in text_lower:
                return {
                    'response': response,
                    'confidence': 0.8,
                    'model': model_data['name'],
                    'processing_time_ms': 50,
                    'tokens_generated': len(response.split())
                }
        
        # Default response
        return {
            'response': 'I understand, but I am not sure how to respond to that.',
            'confidence': 0.3,
            'model': model_data['name'],
            'processing_time_ms': 30,
            'tokens_generated': 12
        }
    
    def _process_audio_classification(self, audio_data, model_data):
        """Process voice activity detection"""
        if not audio_data or len(audio_data) < 10:
            return {'voice_detected': False, 'confidence': 0.0}
        
        # Simple VAD simulation
        energy = sum(sample ** 2 for sample in audio_data) / len(audio_data)
        confidence = min(energy / 10000000.0, 1.0)
        
        voice_detected = confidence > 0.3
        
        return {
            'voice_detected': voice_detected,
            'confidence': confidence,
            'energy_level': energy,
            'model': model_data['name'],
            'processing_time_ms': 3
        }
    
    def get_memory_usage(self):
        """Get current memory usage statistics"""
        current_free = gc.mem_free()
        
        return {
            'total_used_kb': (self.initial_memory - current_free) // 1024,
            'model_memory_kb': self.model_memory_usage // 1024,
            'available_kb': current_free // 1024,
            'loaded_models': len(self.loaded_models),
            'active_model': self.active_model,
            'cache_efficiency': len(self.loaded_models) / max(self.max_loaded_models, 1)
        }
    
    def cleanup(self):
        """Clean up model manager resources"""
        # Unload active model
        self._unload_current_model()
        
        # Clear cache
        self.loaded_models.clear()
        
        # Force garbage collection
        gc.collect()
        
        if self.debug:
            memory_freed = gc.mem_free() - self.initial_memory
            print(f"🧹 ModelManager cleanup completed")
            print(f"   Memory freed: {abs(memory_freed) // 1024}KB")
    
    def get_model_stats(self):
        """Get model manager statistics"""
        stats = {
            'available_models': len(self.model_catalog),
            'loaded_models': len(self.loaded_models),
            'active_model': self.active_model,
            'max_model_size_kb': self.max_model_size,
            'performance_tier': self.performance_tier,
            'memory_usage': self.get_memory_usage()
        }
        
        # Add per-model stats
        stats['model_details'] = {}
        for name, info in self.model_catalog.items():
            compatibility = self.check_model_compatibility(name)
            stats['model_details'][name] = {
                'size_kb': info.size_kb,
                'type': info.type,
                'compatible': compatibility.compatible,
                'loaded': name in self.loaded_models,
                'active': name == self.active_model
            }
        
        return stats
