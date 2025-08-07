#!/usr/bin/env python3
"""
🧠 EthervoxAI Python Implementation - Complete Example

This example demonstrates the full EthervoxAI Python implementation,
showcasing platform detection, model management, and AI inference
following the cross-language protocol specifications.

Usage:
    python examples/basic_usage.py
"""

import asyncio
import logging
import sys
import time
from pathlib import Path

# Add the parent directory to the path so we can import ethervoxai
sys.path.insert(0, str(Path(__file__).parent.parent))

from ethervoxai import local_llm_stack
from ethervoxai.platform_detector import platform_detector
from ethervoxai.model_manager import model_manager

# Configure logging for demo
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler('ethervoxai_demo.log')
    ]
)

logger = logging.getLogger(__name__)

async def demonstrate_platform_detection():
    """Demonstrate platform detection capabilities"""
    print("\n" + "="*60)
    print("🔍 PLATFORM DETECTION DEMONSTRATION")
    print("="*60)
    
    logger.info("Getting system capabilities...")
    capabilities = await platform_detector.get_capabilities()
    
    print(f"🖥️  CPU: {capabilities.cpu_cores} cores ({capabilities.architecture})")
    print(f"🧠 Memory: {capabilities.total_memory:,}MB ({capabilities.available_memory:,}MB available)")
    print(f"⚡ Performance Tier: {capabilities.performance_tier.upper()}")
    print(f"🏗️  Architecture: {capabilities.architecture}")
    print(f"💻 Platform: {capabilities.platform}")
    
    if capabilities.has_gpu:
        print(f"🎮 GPU: Available")
    else:
        print("🎮 GPU: Not available")
    
    # Show AI acceleration features
    accelerations = []
    if capabilities.has_neural_engine:
        accelerations.append("Neural Engine")
    if capabilities.has_avx2:
        accelerations.append("AVX2")
    if capabilities.has_neon:
        accelerations.append("NEON")
    
    if accelerations:
        print(f"🚀 AI Acceleration: {', '.join(accelerations)}")
    else:
        print("🚀 AI Acceleration: None detected")
    
    # Test model compatibility
    print("\n📋 Testing model compatibility...")
    test_models = [
        ("tinyllama-1.1b", 1100, 2048),
        ("llama2-7b", 7000, 8192),
        ("llama2-13b", 13000, 16384),
        ("llama2-70b", 70000, 80000)
    ]
    
    for model_name, size_mb, required_memory in test_models:
        compatibility = await platform_detector.check_model_compatibility(
            model_name, size_mb, required_memory
        )
        
        status = "✅" if compatibility.is_compatible else "❌"
        performance = compatibility.expected_performance
        print(f"{status} {model_name}: {performance} performance expected")
        if compatibility.warnings:
            for warning in compatibility.warnings:
                print(f"   ⚠️  {warning}")
        if compatibility.optimization_flags:
            print(f"   🚀 Optimizations: {', '.join(compatibility.optimization_flags)}")

async def demonstrate_model_management():
    """Demonstrate model management capabilities"""
    print("\n" + "="*60)
    print("📦 MODEL MANAGEMENT DEMONSTRATION")
    print("="*60)
    
    # Show recommended models
    print("🎯 Getting recommended models for this system...")
    recommended = await model_manager.get_recommended_models()
    
    for i, model in enumerate(recommended[:3], 1):
        print(f"{i}. {model.display_name}")
        print(f"   📊 Size: {model.size}MB")
        print(f"   🏷️  Tags: {', '.join(model.tags)}")
        print(f"   📝 Description: {model.description}")
        print()
    
    # Check what's already downloaded
    print("💾 Checking downloaded models...")
    downloaded = model_manager.list_downloaded_models()
    
    if downloaded:
        print("📦 Downloaded models:")
        for model in downloaded:
            print(f"   • {model}")
    else:
        print("📭 No models downloaded yet")
    
    # Show cache info
    cache_size = model_manager.get_cache_size_mb()
    print(f"💽 Cache size: {cache_size:.1f}MB")
    
    # For demo, we'll simulate downloading the smallest recommended model
    if recommended and not downloaded:
        smallest_model = min(recommended, key=lambda m: m.size)
        print(f"\n⬇️ Simulating download of {smallest_model.display_name}...")
        print("   (In real usage, this would download the actual model)")
        print(f"   📊 Model size: {smallest_model.size}MB")
        print(f"   🔗 URL: {smallest_model.download_url}")

async def demonstrate_llm_stack():
    """Demonstrate the complete LLM stack"""
    print("\n" + "="*60)
    print("🧠 LLM STACK DEMONSTRATION")
    print("="*60)
    
    # Initialize the stack
    print("🚀 Initializing EthervoxAI Local LLM Stack...")
    try:
        await local_llm_stack.initialize(privacy_mode="strict")
        print("✅ LLM Stack initialized successfully!")
    except Exception as e:
        print(f"❌ Failed to initialize LLM Stack: {e}")
        print("📝 Note: This is expected in demo mode without actual models")
        return
    
    # Show system status
    status = await local_llm_stack.get_system_status()
    print("\n📊 System Status:")
    print(f"   🔧 Initialized: {status['is_initialized']}")
    print(f"   🤖 Current Model: {status['current_model']}")
    print(f"   🔐 Privacy Mode: {status['privacy_mode']}")
    print(f"   ⚙️  Engine Ready: {status['inference_engine_ready']}")
    
    # Test queries
    test_queries = [
        "What is 2 + 2?",
        "Tell me about artificial intelligence",
        "What's the weather like today?",
        "Calculate the square root of 144"
    ]
    
    print("\n💬 Testing query processing...")
    for query in test_queries:
        print(f"\n❓ Query: '{query}'")
        try:
            response = await local_llm_stack.process_query(query)
            print(f"🤖 Response: {response.text[:100]}{'...' if len(response.text) > 100 else ''}")
            print(f"📊 Tokens: {response.tokens_used}, Confidence: {response.confidence:.2f}")
            print(f"🔒 Privacy: {response.privacy_level}, Source: {response.source}")
        except Exception as e:
            print(f"❌ Error: {e}")
            print("📝 Note: This is expected in demo mode without actual models")

async def demonstrate_streaming():
    """Demonstrate streaming inference"""
    print("\n" + "="*60)
    print("🌊 STREAMING INFERENCE DEMONSTRATION")
    print("="*60)
    
    print("🔄 Testing streaming response...")
    query = "Explain how machine learning works"
    print(f"❓ Query: '{query}'")
    print("🤖 Streaming response:")
    
    try:
        async for token in local_llm_stack.process_streaming(query):
            print(token, end='', flush=True)
            await asyncio.sleep(0.05)  # Simulate realistic streaming delay
        print("\n✅ Streaming completed")
    except Exception as e:
        print(f"❌ Streaming error: {e}")
        print("📝 Note: This is expected in demo mode without actual models")

async def main():
    """Main demonstration function"""
    print("🚀 EthervoxAI Python Implementation Demo")
    print("=" * 60)
    print("This demonstration showcases the complete EthervoxAI Python")
    print("implementation following the cross-language protocol specifications.")
    print()
    
    try:
        # Run demonstrations
        await demonstrate_platform_detection()
        await demonstrate_model_management()
        await demonstrate_llm_stack()
        await demonstrate_streaming()
        
        print("\n" + "="*60)
        print("🎉 DEMONSTRATION COMPLETED")
        print("="*60)
        print("✅ Platform detection: Working")
        print("✅ Model management: Working")
        print("✅ LLM stack integration: Framework ready")
        print("✅ Streaming inference: Framework ready")
        print()
        print("📝 Next steps:")
        print("   1. Install dependencies: pip install -r requirements.txt")
        print("   2. Download actual models to test inference")
        print("   3. Configure model paths and URLs")
        print("   4. Test with real AI models")
        
    except Exception as e:
        logger.error(f"Demo error: {e}", exc_info=True)
        print(f"\n❌ Demo encountered an error: {e}")
    finally:
        # Clean up
        try:
            await local_llm_stack.cleanup()
        except:
            pass

if __name__ == "__main__":
    # Run the demonstration
    asyncio.run(main())
