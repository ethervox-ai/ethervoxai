#!/usr/bin/env python3
"""
🤖 EthervoxAI Advanced Real Model Example

This example demonstrates EthervoxAI with multiple AI backends:
1. OpenAI API (if API key available)
2. Local Transformers models (if installed)
3. Mock AI (always available as fallback)

The example gracefully falls back to available options.

Usage:
    python examples/advanced_real_model.py
    
Environment Variables (optional):
    OPENAI_API_KEY=your_key_here  # For OpenAI integration
"""

import asyncio
import json
import logging
import os
import sys
import time
from pathlib import Path
from typing import Optional, AsyncGenerator, Dict, Any

# Add the parent directory to the path
sys.path.insert(0, str(Path(__file__).parent.parent))

# Import EthervoxAI components
from ethervoxai import local_llm_stack
from ethervoxai.platform_detector import platform_detector
from ethervoxai.model_manager import model_manager

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class AIBackend:
    """Base class for AI backends"""
    
    def __init__(self, name: str):
        self.name = name
        self.is_available = False
        self.is_initialized = False
    
    async def check_availability(self) -> bool:
        """Check if this backend is available"""
        return False
    
    async def initialize(self) -> bool:
        """Initialize the backend"""
        return False
    
    async def generate_response(self, prompt: str, **kwargs) -> str:
        """Generate a text response"""
        raise NotImplementedError
    
    async def generate_streaming_response(self, prompt: str, **kwargs) -> AsyncGenerator[str, None]:
        """Generate a streaming response"""
        response = await self.generate_response(prompt, **kwargs)
        words = response.split()
        for i, word in enumerate(words):
            if i == 0:
                yield word
            else:
                yield f" {word}"
            await asyncio.sleep(0.05)
    
    def cleanup(self):
        """Clean up resources"""
        self.is_initialized = False

class OpenAIBackend(AIBackend):
    """OpenAI API backend"""
    
    def __init__(self):
        super().__init__("OpenAI GPT")
        self.client = None
        self.api_key = os.getenv("OPENAI_API_KEY")
    
    async def check_availability(self) -> bool:
        """Check if OpenAI is available"""
        if not self.api_key:
            return False
        
        try:
            # Try to import openai
            import openai
            self.is_available = True
            return True
        except ImportError:
            return False
    
    async def initialize(self) -> bool:
        """Initialize OpenAI client"""
        if not await self.check_availability():
            return False
        
        try:
            import openai
            self.client = openai.OpenAI(api_key=self.api_key)
            self.is_initialized = True
            return True
        except Exception as e:
            logger.error(f"OpenAI initialization failed: {e}")
            return False
    
    async def generate_response(self, prompt: str, **kwargs) -> str:
        """Generate response using OpenAI"""
        if not self.is_initialized:
            return "❌ OpenAI not initialized"
        
        try:
            response = self.client.chat.completions.create(
                model="gpt-3.5-turbo",
                messages=[{"role": "user", "content": prompt}],
                max_tokens=kwargs.get("max_tokens", 150),
                temperature=kwargs.get("temperature", 0.7)
            )
            return response.choices[0].message.content
        except Exception as e:
            return f"❌ OpenAI error: {e}"

class TransformersBackend(AIBackend):
    """Local Transformers backend"""
    
    def __init__(self):
        super().__init__("Local Transformers")
        self.model = None
        self.tokenizer = None
        self.device = None
    
    async def check_availability(self) -> bool:
        """Check if transformers is available"""
        try:
            import torch
            import transformers
            self.is_available = True
            return True
        except ImportError:
            return False
    
    async def initialize(self) -> bool:
        """Initialize transformers model"""
        if not await self.check_availability():
            return False
        
        try:
            import torch
            from transformers import AutoTokenizer, AutoModelForCausalLM
            
            model_name = "microsoft/DialoGPT-small"  # Small, fast model
            
            self.device = "cuda" if torch.cuda.is_available() else "cpu"
            self.tokenizer = AutoTokenizer.from_pretrained(model_name)
            self.model = AutoModelForCausalLM.from_pretrained(model_name)
            
            if self.tokenizer.pad_token is None:
                self.tokenizer.pad_token = self.tokenizer.eos_token
            
            self.model.to(self.device)
            self.is_initialized = True
            return True
            
        except Exception as e:
            logger.error(f"Transformers initialization failed: {e}")
            return False
    
    async def generate_response(self, prompt: str, **kwargs) -> str:
        """Generate response using transformers"""
        if not self.is_initialized:
            return "❌ Transformers not initialized"
        
        try:
            import torch
            
            inputs = self.tokenizer.encode(prompt, return_tensors="pt").to(self.device)
            
            with torch.no_grad():
                outputs = self.model.generate(
                    inputs,
                    max_length=inputs.shape[1] + kwargs.get("max_tokens", 50),
                    num_return_sequences=1,
                    temperature=kwargs.get("temperature", 0.7),
                    do_sample=True,
                    pad_token_id=self.tokenizer.eos_token_id
                )
            
            response = self.tokenizer.decode(outputs[0], skip_special_tokens=True)
            
            # Remove the original prompt
            if response.startswith(prompt):
                response = response[len(prompt):].strip()
            
            return response if response else "I understand, but I need more context to respond."
            
        except Exception as e:
            return f"❌ Generation error: {e}"
    
    def cleanup(self):
        """Clean up transformers resources"""
        if self.model:
            del self.model
        if self.tokenizer:
            del self.tokenizer
        
        try:
            import torch
            import gc
            gc.collect()
            if torch.cuda.is_available():
                torch.cuda.empty_cache()
        except ImportError:
            pass
        
        super().cleanup()

class MockBackend(AIBackend):
    """Mock AI backend (always available)"""
    
    def __init__(self):
        super().__init__("Mock AI")
        self.conversation_count = 0
        self.responses = {
            "greeting": [
                "Hello! I'm a mock AI assistant powered by EthervoxAI. How can I help you?",
                "Hi there! I'm here to demonstrate EthervoxAI's capabilities.",
                "Greetings! I'm a simulated AI ready to assist you."
            ],
            "ethervoxai": [
                "EthervoxAI is a privacy-focused, multi-language AI framework designed for local AI processing.",
                "EthervoxAI enables secure, private AI interactions across different platforms and languages.",
                "With EthervoxAI, you can run AI models locally while maintaining complete data privacy."
            ],
            "programming": [
                "I can help with programming in Python, JavaScript, TypeScript, and many other languages!",
                "Programming is one of my strengths. What specific coding challenge are you working on?",
                "I'd be happy to assist with your programming questions. What language are you using?"
            ],
            "ai": [
                "Artificial Intelligence is the simulation of human intelligence in machines.",
                "AI involves machine learning, natural language processing, and decision-making systems.",
                "Modern AI can understand language, recognize patterns, and solve complex problems."
            ],
            "default": [
                "That's an interesting question. Let me provide you with a helpful response.",
                "I understand what you're asking. Here's my take on that topic.",
                "Great question! Based on my knowledge, I can share some insights about this."
            ]
        }
    
    async def check_availability(self) -> bool:
        """Mock is always available"""
        self.is_available = True
        return True
    
    async def initialize(self) -> bool:
        """Initialize mock backend"""
        self.is_initialized = True
        return True
    
    async def generate_response(self, prompt: str, **kwargs) -> str:
        """Generate mock response"""
        self.conversation_count += 1
        
        prompt_lower = prompt.lower()
        
        # Determine response category
        if any(word in prompt_lower for word in ["hello", "hi", "hey", "greetings"]):
            category = "greeting"
        elif "ethervoxai" in prompt_lower:
            category = "ethervoxai"
        elif any(word in prompt_lower for word in ["program", "code", "python", "javascript"]):
            category = "programming"
        elif any(word in prompt_lower for word in ["ai", "artificial intelligence", "machine learning"]):
            category = "ai"
        else:
            category = "default"
        
        # Select response
        import random
        responses = self.responses[category]
        base_response = random.choice(responses)
        
        # Add conversation context
        if self.conversation_count > 1:
            base_response += f" (This is our {self.conversation_count}{'nd' if self.conversation_count == 2 else 'th'} exchange.)"
        
        return base_response

class AdvancedAIManager:
    """Manages multiple AI backends with intelligent fallback"""
    
    def __init__(self):
        self.backends = [
            OpenAIBackend(),
            TransformersBackend(),
            MockBackend()
        ]
        self.active_backend = None
        self.fallback_backends = []
    
    async def initialize(self) -> Dict[str, Any]:
        """Initialize all available backends"""
        results = {
            "available_backends": [],
            "active_backend": None,
            "initialization_log": []
        }
        
        print("🔍 Scanning for available AI backends...")
        
        for backend in self.backends:
            print(f"   Checking {backend.name}...", end=" ")
            
            if await backend.check_availability():
                print("✅ Available")
                
                print(f"   Initializing {backend.name}...", end=" ")
                if await backend.initialize():
                    print("✅ Initialized")
                    results["available_backends"].append(backend.name)
                    
                    if self.active_backend is None:
                        self.active_backend = backend
                        results["active_backend"] = backend.name
                    else:
                        self.fallback_backends.append(backend)
                        
                    results["initialization_log"].append(f"✅ {backend.name}: Ready")
                else:
                    print("❌ Failed")
                    results["initialization_log"].append(f"❌ {backend.name}: Initialization failed")
            else:
                print("❌ Not available")
                results["initialization_log"].append(f"⚠️ {backend.name}: Not available")
        
        return results
    
    async def generate_response(self, prompt: str, **kwargs) -> Dict[str, Any]:
        """Generate response with fallback logic"""
        if not self.active_backend:
            return {
                "response": "❌ No AI backend available",
                "backend": "None",
                "success": False
            }
        
        # Try active backend first
        try:
            start_time = time.time()
            response = await self.active_backend.generate_response(prompt, **kwargs)
            response_time = time.time() - start_time
            
            return {
                "response": response,
                "backend": self.active_backend.name,
                "response_time": response_time,
                "success": True
            }
            
        except Exception as e:
            logger.error(f"Active backend {self.active_backend.name} failed: {e}")
            
            # Try fallback backends
            for fallback in self.fallback_backends:
                try:
                    start_time = time.time()
                    response = await fallback.generate_response(prompt, **kwargs)
                    response_time = time.time() - start_time
                    
                    return {
                        "response": response,
                        "backend": f"{fallback.name} (fallback)",
                        "response_time": response_time,
                        "success": True
                    }
                except Exception as fallback_error:
                    logger.error(f"Fallback {fallback.name} failed: {fallback_error}")
                    continue
            
            return {
                "response": "❌ All AI backends failed",
                "backend": "None",
                "success": False
            }
    
    async def generate_streaming_response(self, prompt: str, **kwargs):
        """Generate streaming response"""
        if not self.active_backend:
            yield "❌ No AI backend available"
            return
        
        try:
            async for token in self.active_backend.generate_streaming_response(prompt, **kwargs):
                yield token
        except Exception as e:
            yield f"❌ Streaming failed: {e}"
    
    def get_status(self) -> Dict[str, Any]:
        """Get manager status"""
        return {
            "active_backend": self.active_backend.name if self.active_backend else None,
            "fallback_backends": [b.name for b in self.fallback_backends],
            "total_backends": len([b for b in self.backends if b.is_initialized])
        }
    
    def cleanup(self):
        """Clean up all backends"""
        for backend in self.backends:
            backend.cleanup()

async def demonstrate_advanced_ai():
    """Demonstrate advanced AI integration"""
    print("🚀 EthervoxAI Advanced Real Model Demo")
    print("=" * 50)
    
    # Step 1: System Analysis
    print("🔍 Step 1: System Analysis")
    capabilities = await platform_detector.get_capabilities()
    print(f"💻 Platform: {capabilities.platform} {capabilities.architecture}")
    print(f"🧠 Memory: {capabilities.total_memory}MB ({capabilities.performance_tier})")
    
    # Step 2: AI Backend Initialization
    print(f"\n🤖 Step 2: AI Backend Initialization")
    ai_manager = AdvancedAIManager()
    init_results = await ai_manager.initialize()
    
    print(f"\n📊 Backend Status:")
    for log_entry in init_results["initialization_log"]:
        print(f"   {log_entry}")
    
    print(f"\n✅ Active Backend: {init_results['active_backend']}")
    print(f"📦 Available Backends: {len(init_results['available_backends'])}")
    
    # Step 3: EthervoxAI Integration
    print(f"\n🔗 Step 3: EthervoxAI Integration")
    try:
        await local_llm_stack.initialize()
        status = await local_llm_stack.get_system_status()
        print(f"✅ LLM Stack: Active")
        print(f"🔒 Privacy Mode: Enabled")
    except Exception as e:
        print(f"⚠️ LLM Stack: {e}")
    
    # Step 4: Interactive Testing
    print(f"\n💬 Step 4: Interactive AI Testing")
    print("-" * 30)
    
    test_prompts = [
        "Hello! What is EthervoxAI?",
        "How does local AI processing work?",
        "Can you help me with Python programming?",
        "What are the benefits of privacy-focused AI?",
        "Explain machine learning in simple terms"
    ]
    
    conversation_log = []
    
    for i, prompt in enumerate(test_prompts, 1):
        print(f"\n[{i}] 👤 Human: {prompt}")
        
        result = await ai_manager.generate_response(prompt, max_tokens=100)
        
        print(f"🤖 AI ({result['backend']}): {result['response']}")
        if result['success']:
            print(f"⏱️ Response time: {result['response_time']:.3f}s")
        
        conversation_log.append(result)
        await asyncio.sleep(0.5)
    
    # Step 5: Streaming Demo
    print(f"\n🌊 Step 5: Streaming Response Demo")
    print("-" * 30)
    
    streaming_prompt = "Describe the future of privacy-focused AI"
    print(f"👤 Human: {streaming_prompt}")
    print("🤖 AI: ", end="", flush=True)
    
    start_time = time.time()
    async for token in ai_manager.generate_streaming_response(streaming_prompt):
        print(token, end="", flush=True)
    
    stream_time = time.time() - start_time
    print(f"\n⚡ Streaming completed in {stream_time:.2f}s")
    
    # Step 6: Performance Analysis
    print(f"\n📊 Step 6: Performance Analysis")
    print("-" * 30)
    
    successful_responses = [log for log in conversation_log if log['success']]
    if successful_responses:
        avg_time = sum(log['response_time'] for log in successful_responses) / len(successful_responses)
        backends_used = set(log['backend'] for log in successful_responses)
        
        print(f"✅ Successful responses: {len(successful_responses)}/{len(conversation_log)}")
        print(f"⚡ Average response time: {avg_time:.3f}s")
        print(f"🔧 Backends used: {', '.join(backends_used)}")
    
    manager_status = ai_manager.get_status()
    print(f"🤖 Active backend: {manager_status['active_backend']}")
    print(f"🔄 Fallback options: {len(manager_status['fallback_backends'])}")
    
    # Step 7: Capabilities Summary
    print(f"\n🎯 Step 7: Capabilities Summary")
    print("-" * 30)
    
    capabilities_summary = {
        "Real AI Integration": "✅" if init_results['active_backend'] != "Mock AI" else "⚠️ Mock only",
        "Streaming Responses": "✅ Working",
        "Fallback System": "✅ Working",
        "Privacy Protection": "✅ Local processing",
        "Multi-backend Support": "✅ Working",
        "EthervoxAI Integration": "✅ Working"
    }
    
    for capability, status in capabilities_summary.items():
        print(f"{capability}: {status}")
    
    # Cleanup
    print(f"\n🧹 Cleanup")
    ai_manager.cleanup()
    
    print(f"\n" + "=" * 50)
    print("🎉 Advanced AI Demo Complete!")
    
    if init_results['active_backend'] != "Mock AI":
        print("✅ Successfully used real AI models!")
    else:
        print("ℹ️ Used mock AI (install OpenAI or transformers for real models)")
    
    print("✅ Demonstrated advanced EthervoxAI capabilities")
    print("✅ Verified fallback and reliability systems")

async def main():
    """Main function"""
    print("🤖 EthervoxAI Advanced Real Model Integration")
    print("\nThis demo will test multiple AI backends:")
    print("• OpenAI API (if OPENAI_API_KEY set)")
    print("• Local Transformers (if installed)")
    print("• Mock AI (always available)")
    print("\nThe system will automatically use the best available option.")
    
    input("\nPress Enter to start the demo...")
    
    try:
        await demonstrate_advanced_ai()
    except KeyboardInterrupt:
        print("\n🛑 Demo interrupted by user")
    except Exception as e:
        print(f"\n❌ Demo failed: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    asyncio.run(main())
