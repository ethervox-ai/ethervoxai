"""
🧠 EthervoxAI Local LLM Stack - Python Implementation

Integrates platform detection, model management, and inference engine
to provide a complete local AI processing pipeline.

This implementation follows the EthervoxAI cross-language protocol while
providing a high-level Python interface for local AI processing.
"""

import asyncio
import logging
from typing import Optional, Dict, Any, List
from dataclasses import dataclass, asdict

from .platform_detector import platform_detector, SystemCapabilities
from .model_manager import model_manager, ModelInfo
from .inference_engine import inference_engine, InferenceParameters, InferenceResponse

logger = logging.getLogger(__name__)

@dataclass
class ProcessingResponse:
    """Complete processing response following EthervoxAI protocol"""
    text: str
    confidence: float
    source: str  # local, external, hybrid
    model: str
    tokens_used: int
    inference_stats: Dict[str, Any]
    privacy_level: str  # local, cloud, external
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary following protocol format"""
        return asdict(self)

@dataclass
class IntentResult:
    """Intent parsing result"""
    intent: str
    confidence: float
    entities: Dict[str, Any]
    requires_external: bool

class LocalLLMStack:
    """
    Python implementation of EthervoxAI Local LLM Stack
    
    Features:
    - Intelligent model selection based on system capabilities
    - Local-first processing with optional cloud fallback
    - Intent parsing and routing
    - Privacy-aware processing
    - Performance monitoring and optimization
    """
    
    def __init__(self):
        self._capabilities: Optional[SystemCapabilities] = None
        self._current_model: Optional[str] = None
        self._is_initialized: bool = False
        self._preferred_model: Optional[str] = None
        self._privacy_mode: str = "strict"  # strict, balanced, permissive
        
    async def initialize(self, preferred_model: Optional[str] = None, privacy_mode: str = "strict") -> None:
        """Initialize the LLM stack with optimal configuration"""
        logger.info("🧠 Initializing EthervoxAI Local LLM Stack...")
        
        self._privacy_mode = privacy_mode
        self._preferred_model = preferred_model
        
        # Get system capabilities
        self._capabilities = await platform_detector.get_capabilities()
        logger.info(f"📊 System detected: {self._capabilities.performance_tier} tier "
                   f"({self._capabilities.total_memory}MB RAM, {self._capabilities.cpu_cores} cores)")
        
        # Select optimal model
        if preferred_model:
            # Verify preferred model is compatible
            catalog = model_manager.get_default_model_catalog()
            model_info = next((m for m in catalog if m.name == preferred_model), None)
            
            if model_info:
                compatibility = await platform_detector.check_model_compatibility(
                    preferred_model, model_info.size, model_info.required_memory
                )
                
                if compatibility.is_compatible:
                    self._current_model = preferred_model
                    logger.info(f"✅ Using preferred model: {preferred_model}")
                else:
                    logger.warning(f"⚠️ Preferred model {preferred_model} not compatible, selecting alternative")
                    self._current_model = await self._select_optimal_model()
            else:
                logger.warning(f"⚠️ Preferred model {preferred_model} not found, selecting alternative")
                self._current_model = await self._select_optimal_model()
        else:
            self._current_model = await self._select_optimal_model()
        
        # Initialize inference engine
        await inference_engine.initialize(self._current_model)
        
        self._is_initialized = True
        logger.info(f"🚀 LLM Stack initialized successfully with model: {self._current_model}")
        
        # Log privacy and performance info
        logger.info(f"🔐 Privacy mode: {self._privacy_mode}")
        if self._capabilities.performance_tier in ['low', 'medium']:
            logger.info("💡 Consider upgrading hardware for better performance with larger models")
    
    async def process_query(self, text: str, context: Optional[Dict[str, Any]] = None) -> ProcessingResponse:
        """Process a text query through the complete LLM stack"""
        if not self._is_initialized:
            raise RuntimeError("LLM Stack not initialized. Call initialize() first.")
        
        logger.info(f"🔍 Processing query: '{text[:100]}{'...' if len(text) > 100 else ''}'")
        
        # Parse intent
        intent_result = await self._parse_intent(text)
        logger.debug(f"📝 Detected intent: {intent_result.intent} (confidence: {intent_result.confidence:.2f})")
        
        # Determine processing strategy
        if self._privacy_mode == "strict" or not intent_result.requires_external:
            # Local processing only
            response = await self._process_locally(text, intent_result, context)
        elif self._privacy_mode == "balanced" and intent_result.requires_external:
            # Try local first, fallback to external if needed
            response = await self._process_with_fallback(text, intent_result, context)
        elif self._privacy_mode == "permissive" and intent_result.requires_external:
            # Use external services for better quality when appropriate
            response = await self._process_with_external(text, intent_result, context)
        else:
            # Default to local processing
            response = await self._process_locally(text, intent_result, context)
        
        logger.info(f"✅ Query processed: {response.tokens_used} tokens, "
                   f"{response.inference_stats.get('tokens_per_second', 0):.1f} tok/s")
        
        return response
    
    async def process_streaming(self, text: str, context: Optional[Dict[str, Any]] = None):
        """Process a query with streaming response"""
        if not self._is_initialized:
            raise RuntimeError("LLM Stack not initialized. Call initialize() first.")
        
        logger.info(f"🔍 Starting streaming processing for: '{text[:50]}{'...' if len(text) > 50 else ''}'")
        
        # For streaming, we currently only support local processing
        intent_result = await self._parse_intent(text)
        
        # Configure inference parameters based on intent
        parameters = self._get_inference_parameters(intent_result)
        
        # Stream response
        async for token in inference_engine.complete_streaming(text, parameters):
            yield token
    
    async def get_system_status(self) -> Dict[str, Any]:
        """Get current system status and performance metrics"""
        return {
            "is_initialized": self._is_initialized,
            "current_model": self._current_model,
            "privacy_mode": self._privacy_mode,
            "capabilities": self._capabilities.to_dict() if self._capabilities else None,
            "inference_engine_ready": inference_engine.is_initialized(),
            "available_models": model_manager.list_downloaded_models(),
            "cache_size_mb": model_manager.get_cache_size_mb()
        }
    
    async def switch_model(self, model_name: str) -> None:
        """Switch to a different model"""
        logger.info(f"🔄 Switching to model: {model_name}")
        
        # Verify model compatibility
        catalog = model_manager.get_default_model_catalog()
        model_info = next((m for m in catalog if m.name == model_name), None)
        
        if not model_info:
            raise ValueError(f"Model '{model_name}' not found in catalog")
        
        compatibility = await platform_detector.check_model_compatibility(
            model_name, model_info.size, model_info.required_memory
        )
        
        if not compatibility.is_compatible:
            raise RuntimeError(f"Model '{model_name}' is not compatible: {compatibility.warnings}")
        
        # Unload current model and load new one
        await inference_engine.unload_model()
        await inference_engine.initialize(model_name)
        
        self._current_model = model_name
        logger.info(f"✅ Successfully switched to model: {model_name}")
    
    async def cleanup(self) -> None:
        """Clean up resources"""
        if self._is_initialized:
            logger.info("🧹 Cleaning up LLM Stack resources...")
            await inference_engine.unload_model()
            self._is_initialized = False
            logger.info("✅ Cleanup completed")
    
    # ================== Private Methods ==================
    
    async def _select_optimal_model(self) -> str:
        """Select the optimal model based on system capabilities"""
        recommended_models = await model_manager.get_recommended_models()
        
        if not recommended_models:
            raise RuntimeError("No compatible models found for this system")
        
        # Select the best model for the system
        selected_model = recommended_models[0]  # First is highest priority
        
        logger.info(f"🎯 Selected optimal model: {selected_model.display_name} "
                   f"({selected_model.size}MB)")
        
        return selected_model.name
    
    async def _parse_intent(self, text: str) -> IntentResult:
        """Parse intent from input text"""
        # Simple rule-based intent parsing for demo
        # In production, this could use a dedicated intent classification model
        
        text_lower = text.lower()
        
        # Determine intent based on keywords
        if any(word in text_lower for word in ['weather', 'temperature', 'rain', 'sunny']):
            return IntentResult(
                intent='weather_query',
                confidence=0.8,
                entities={},
                requires_external=True  # Weather requires real-time data
            )
        elif any(word in text_lower for word in ['news', 'current events', 'today']):
            return IntentResult(
                intent='news_query',
                confidence=0.8,
                entities={},
                requires_external=True  # News requires real-time data
            )
        elif any(word in text_lower for word in ['calculate', 'math', 'compute']):
            return IntentResult(
                intent='calculation',
                confidence=0.9,
                entities={},
                requires_external=False  # Math can be done locally
            )
        else:
            return IntentResult(
                intent='general_conversation',
                confidence=0.7,
                entities={},
                requires_external=False  # General chat can be done locally
            )
    
    async def _process_locally(
        self, 
        text: str, 
        intent_result: IntentResult, 
        context: Optional[Dict[str, Any]]
    ) -> ProcessingResponse:
        """Process query using only local resources"""
        # Configure inference parameters based on intent
        parameters = self._get_inference_parameters(intent_result)
        
        # Generate response using local model
        inference_response = await inference_engine.complete(text, parameters)
        
        return ProcessingResponse(
            text=inference_response.text,
            confidence=0.85,  # Local processing confidence
            source='local',
            model=self._current_model,
            tokens_used=inference_response.tokens_generated,
            inference_stats=inference_response.timings,
            privacy_level='local'
        )
    
    async def _process_with_fallback(
        self, 
        text: str, 
        intent_result: IntentResult, 
        context: Optional[Dict[str, Any]]
    ) -> ProcessingResponse:
        """Try local processing first, fallback to external if needed"""
        try:
            # Try local processing first
            response = await self._process_locally(text, intent_result, context)
            
            # Check if local response is satisfactory
            if response.confidence >= 0.7:
                return response
            else:
                logger.info("🔄 Local response confidence low, would fallback to external (simulation)")
                # In production, would call external service here
                return response
        except Exception as e:
            logger.warning(f"⚠️ Local processing failed: {e}, would fallback to external (simulation)")
            # Return local response anyway for demo
            return await self._process_locally(text, intent_result, context)
    
    async def _process_with_external(
        self, 
        text: str, 
        intent_result: IntentResult, 
        context: Optional[Dict[str, Any]]
    ) -> ProcessingResponse:
        """Process using external services when appropriate"""
        # For demo purposes, we'll still use local processing
        # In production, this would integrate with external APIs
        logger.info("🌐 Would use external processing (simulation)")
        
        response = await self._process_locally(text, intent_result, context)
        response.source = 'hybrid'  # Indicate hybrid processing
        response.privacy_level = 'cloud'
        
        return response
    
    def _get_inference_parameters(self, intent_result: IntentResult) -> InferenceParameters:
        """Get inference parameters optimized for the detected intent"""
        base_params = InferenceParameters()
        
        if intent_result.intent == 'calculation':
            # Math queries need precision, less creativity
            base_params.temperature = 0.1
            base_params.max_tokens = 200
        elif intent_result.intent == 'general_conversation':
            # Conversation can be more creative
            base_params.temperature = 0.7
            base_params.max_tokens = 150
        elif intent_result.intent in ['weather_query', 'news_query']:
            # Information queries need balance
            base_params.temperature = 0.3
            base_params.max_tokens = 100
        
        return base_params

# Export singleton instance
local_llm_stack = LocalLLMStack()
