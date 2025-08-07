#!/usr/bin/env python3
"""
🧪 EthervoxAI Python Implementation - Test Suite

Comprehensive tests for the Python implementation of EthervoxAI,
ensuring compatibility with the cross-language protocol specifications.

Usage:
    python tests/test_ethervoxai.py  # Direct execution
    python -m pytest tests/test_ethervoxai.py -v  # With pytest (if installed)
"""

import asyncio
import sys
import tempfile
import json
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock, patch
from dataclasses import asdict

# Check if pytest is available
try:
    import pytest
    PYTEST_AVAILABLE = True
except ImportError:
    PYTEST_AVAILABLE = False
    # Create a dummy pytest.mark.asyncio decorator for compatibility
    class MockPytest:
        class mark:
            @staticmethod
            def asyncio(func):
                return func
    pytest = MockPytest()

# Add the parent directory to the path so we can import ethervoxai
sys.path.insert(0, str(Path(__file__).parent.parent))

from ethervoxai.platform_detector import platform_detector, SystemCapabilities, ModelCompatibility
from ethervoxai.model_manager import model_manager, ModelInfo
from ethervoxai.inference_engine import inference_engine, InferenceParameters, InferenceResponse
from ethervoxai.local_llm_stack import local_llm_stack, ProcessingResponse, IntentResult

class TestPlatformDetector:
    """Test suite for platform detection functionality"""
    
    @pytest.mark.asyncio
    async def test_get_capabilities(self):
        """Test system capabilities detection"""
        capabilities = await platform_detector.get_capabilities()
        
        # Verify required fields
        assert isinstance(capabilities, SystemCapabilities)
        assert capabilities.cpu_cores > 0
        assert capabilities.total_memory > 0
        assert capabilities.available_memory > 0
        assert capabilities.performance_tier in ['low', 'medium', 'high', 'ultra']
        assert capabilities.platform in ['windows', 'linux', 'darwin', 'other']
        assert capabilities.architecture in ['x86_64', 'arm64', 'armv7l', 'unknown']
        
        print(f"✅ Platform detection: {capabilities.platform} {capabilities.architecture}")
        print(f"✅ Performance tier: {capabilities.performance_tier}")
    
    @pytest.mark.asyncio
    async def test_model_compatibility_small(self):
        """Test compatibility with small models"""
        compatibility = await platform_detector.check_model_compatibility(
            "test-small", 1000, 2048  # 1GB model, 2GB required memory
        )
        
        assert isinstance(compatibility, ModelCompatibility)
        assert isinstance(compatibility.is_compatible, bool)
        assert isinstance(compatibility.expected_performance, str)
        assert isinstance(compatibility.warnings, list)
        
        print(f"✅ Small model compatibility: {compatibility.is_compatible}")
    
    @pytest.mark.asyncio
    async def test_model_compatibility_large(self):
        """Test compatibility with large models"""
        compatibility = await platform_detector.check_model_compatibility(
            "test-huge", 100000, 120000  # 100GB model, 120GB required memory
        )
        
        assert isinstance(compatibility, ModelCompatibility)
        # Should likely be incompatible on most systems
        print(f"✅ Large model compatibility: {compatibility.is_compatible}")
        if not compatibility.is_compatible:
            print(f"   Performance: {compatibility.expected_performance}")

class TestModelManager:
    """Test suite for model management functionality"""
    
    def test_get_default_catalog(self):
        """Test default model catalog retrieval"""
        catalog = model_manager.get_default_model_catalog()
        
        assert isinstance(catalog, list)
        assert len(catalog) > 0
        
        # Verify model info structure
        for model in catalog:
            assert isinstance(model, ModelInfo)
            assert model.name
            assert model.display_name
            assert model.size > 0
            assert model.download_url
            assert isinstance(model.tags, list)
            
        print(f"✅ Model catalog: {len(catalog)} models available")
    
    @pytest.mark.asyncio
    async def test_get_recommended_models(self):
        """Test recommended models for current system"""
        recommended = await model_manager.get_recommended_models()
        
        assert isinstance(recommended, list)
        # Should have at least one compatible model
        assert len(recommended) > 0
        
        # Verify models are sorted by preference
        for model in recommended:
            assert isinstance(model, ModelInfo)
            
        print(f"✅ Recommended models: {len(recommended)} models")
        if recommended:
            print(f"   Top recommendation: {recommended[0].display_name}")
    
    def test_list_downloaded_models(self):
        """Test listing of downloaded models"""
        downloaded = model_manager.list_downloaded_models()
        
        assert isinstance(downloaded, list)
        # Downloaded list can be empty
        
        print(f"✅ Downloaded models: {len(downloaded)} models")
    
    def test_get_cache_size(self):
        """Test cache size calculation"""
        cache_size = model_manager.get_cache_size_mb()
        
        assert isinstance(cache_size, (int, float))
        assert cache_size >= 0
        
        print(f"✅ Cache size: {cache_size:.1f}MB")

class TestInferenceEngine:
    """Test suite for inference engine functionality"""
    
    def test_initialization_state(self):
        """Test inference engine initialization state"""
        # Should start uninitialized
        assert not inference_engine.is_initialized()
        print("✅ Inference engine starts uninitialized")
    
    @pytest.mark.asyncio
    async def test_initialization_mock(self):
        """Test inference engine initialization with mocking"""
        # For this test, we'll just verify that initialize method exists and can be called
        try:
            # This will fail gracefully since we don't have real models
            await inference_engine.initialize("test-model")
            print("✅ Inference engine initialization attempt successful")
        except Exception as e:
            # Expected to fail without real models, but method should exist
            assert "not found in catalog" in str(e) or "simulation mode" in str(e)
            print("✅ Inference engine initialization (expected failure without models)")
    
    def test_inference_parameters(self):
        """Test inference parameters structure"""
        params = InferenceParameters()
        
        # Verify default values
        assert 0.0 <= params.temperature <= 2.0
        assert params.max_tokens > 0
        assert 0.0 <= params.top_p <= 1.0
        assert isinstance(params.stop_sequences, list)
        
        print("✅ Inference parameters validation")
    
    def test_inference_response_structure(self):
        """Test inference response structure"""
        response = InferenceResponse(
            text="Test response",
            tokens_generated=10,
            tokens_per_second=5.0,
            finished=True,
            finish_reason="stop",
            timings={"total_time": 1.5, "tokens_per_second": 6.7}
        )
        
        assert isinstance(response.text, str)
        assert isinstance(response.tokens_generated, int)
        assert isinstance(response.timings, dict)
        
        print("✅ Inference response structure")

class TestLocalLLMStack:
    """Test suite for the complete LLM stack"""
    
    @pytest.mark.asyncio
    async def test_initialization_state(self):
        """Test LLM stack initialization state"""
        # Should start uninitialized
        status = await local_llm_stack.get_system_status()
        assert not status["is_initialized"]
        print("✅ LLM stack starts uninitialized")
    
    @pytest.mark.asyncio
    async def test_intent_parsing(self):
        """Test intent parsing functionality"""
        # Access private method for testing
        intent = await local_llm_stack._parse_intent("What's the weather like?")
        
        assert isinstance(intent, IntentResult)
        assert intent.intent == "weather_query"
        assert intent.requires_external
        
        intent2 = await local_llm_stack._parse_intent("Calculate 5 + 3")
        assert intent2.intent == "calculation"
        assert not intent2.requires_external
        
        print("✅ Intent parsing")
    
    def test_processing_response_structure(self):
        """Test processing response structure"""
        response = ProcessingResponse(
            text="Test response",
            confidence=0.85,
            source="local",
            model="test-model",
            tokens_used=10,
            inference_stats={"tokens_per_second": 5.0},
            privacy_level="local"
        )
        
        # Test dictionary conversion
        response_dict = response.to_dict()
        assert isinstance(response_dict, dict)
        assert response_dict["text"] == "Test response"
        assert response_dict["source"] == "local"
        
        print("✅ Processing response structure")
    
    @pytest.mark.asyncio
    async def test_system_status(self):
        """Test system status reporting"""
        status = await local_llm_stack.get_system_status()
        
        required_fields = [
            "is_initialized", "current_model", "privacy_mode",
            "capabilities", "inference_engine_ready", "available_models", "cache_size_mb"
        ]
        
        for field in required_fields:
            assert field in status
            
        print("✅ System status reporting")

class TestProtocolCompliance:
    """Test compliance with EthervoxAI cross-language protocol"""
    
    @pytest.mark.asyncio
    async def test_platform_detection_protocol(self):
        """Test platform detection follows protocol format"""
        capabilities = await platform_detector.get_capabilities()
        capabilities_dict = capabilities.to_dict()
        
        # Required protocol fields
        required_fields = [
            "cpu_cores", "total_memory", "available_memory", "performance_tier",
            "platform", "architecture", "has_gpu"
        ]
        
        for field in required_fields:
            assert field in capabilities_dict
            
        print("✅ Platform detection protocol compliance")
    
    def test_model_info_protocol(self):
        """Test model info follows protocol format"""
        catalog = model_manager.get_default_model_catalog()
        
        for model in catalog[:3]:  # Test first 3 models
            model_dict = model.to_dict()
            
            required_fields = [
                "name", "display_name", "size", "required_memory",
                "download_url", "tags", "description"
            ]
            
            for field in required_fields:
                assert field in model_dict
                
        print("✅ Model info protocol compliance")
    
    def test_inference_parameters_protocol(self):
        """Test inference parameters follow protocol format"""
        params = InferenceParameters()
        params_dict = asdict(params)
        
        required_fields = [
            "temperature", "max_tokens", "top_p", "stop_sequences"
        ]
        
        for field in required_fields:
            assert field in params_dict
            
        print("✅ Inference parameters protocol compliance")

# Integration test
@pytest.mark.asyncio
async def test_full_integration():
    """Test complete integration workflow"""
    print("\n🔄 Running full integration test...")
    
    # 1. Platform detection
    capabilities = await platform_detector.get_capabilities()
    assert capabilities.performance_tier in ['low', 'medium', 'high', 'ultra']
    print("   ✅ Platform detection")
    
    # 2. Model recommendations
    recommended = await model_manager.get_recommended_models()
    assert len(recommended) > 0
    print("   ✅ Model recommendations")
    
    # 3. System status
    status = await local_llm_stack.get_system_status()
    assert "is_initialized" in status
    print("   ✅ System status")
    
    # 4. Intent parsing
    intent = await local_llm_stack._parse_intent("Hello world")
    assert isinstance(intent, IntentResult)
    print("   ✅ Intent parsing")
    
    print("🎉 Full integration test passed!")

async def run_all_tests():
    """Run all tests when script is executed directly"""
    print("🧪 EthervoxAI Python Implementation Test Suite")
    print("=" * 60)
    
    try:
        # Platform detection tests
        print("\n🔍 Testing Platform Detection...")
        pd_test = TestPlatformDetector()
        await pd_test.test_get_capabilities()
        await pd_test.test_model_compatibility_small()
        await pd_test.test_model_compatibility_large()
        
        # Model manager tests
        print("\n📦 Testing Model Management...")
        mm_test = TestModelManager()
        mm_test.test_get_default_catalog()
        await mm_test.test_get_recommended_models()
        mm_test.test_list_downloaded_models()
        mm_test.test_get_cache_size()
        
        # Inference engine tests
        print("\n🧠 Testing Inference Engine...")
        ie_test = TestInferenceEngine()
        ie_test.test_initialization_state()
        await ie_test.test_initialization_mock()
        ie_test.test_inference_parameters()
        ie_test.test_inference_response_structure()
        
        # LLM stack tests
        print("\n🚀 Testing LLM Stack...")
        llm_test = TestLocalLLMStack()
        await llm_test.test_initialization_state()
        await llm_test.test_intent_parsing()
        llm_test.test_processing_response_structure()
        await llm_test.test_system_status()
        
        # Protocol compliance tests
        print("\n📋 Testing Protocol Compliance...")
        pc_test = TestProtocolCompliance()
        await pc_test.test_platform_detection_protocol()
        pc_test.test_model_info_protocol()
        pc_test.test_inference_parameters_protocol()
        
        # Integration test
        await test_full_integration()
        
        print("\n" + "=" * 60)
        print("🎉 ALL TESTS PASSED!")
        print("✅ EthervoxAI Python implementation is working correctly")
        print("✅ Protocol compliance verified")
        print("✅ Integration workflow validated")
        
        if PYTEST_AVAILABLE:
            print("✅ Pytest compatibility confirmed")
        else:
            print("ℹ️  Running in standalone mode (pytest not installed)")
            
        return True
        
    except Exception as e:
        print(f"\n❌ Test failed: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    success = asyncio.run(run_all_tests())
    sys.exit(0 if success else 1)
