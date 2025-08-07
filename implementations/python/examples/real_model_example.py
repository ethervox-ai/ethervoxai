#!/usr/bin/env python3
"""
🤖 EthervoxAI Real Model Example

This example demonstrates the EthervoxAI Python implementation using a real
AI model downloaded from Hugging Face Hub. It showcases:

1. Real model downloading with progress tracking
2. Actual AI inference using transformers
3. Streaming responses and performance monitoring
4. Platform-optimized inference settings

Prerequisites:
    pip install transformers torch huggingface-hub accelerate

Usage:
    python examples/real_model_example.py
"""

import asyncio
import logging
import sys
import time
import warnings
from pathlib import Path

# Add the parent directory to the path so we can import ethervoxai
sys.path.insert(0, str(Path(__file__).parent.parent))

# Suppress some common warnings for cleaner output
warnings.filterwarnings("ignore", category=UserWarning)
warnings.filterwarnings("ignore", category=FutureWarning)

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Check for required dependencies
REQUIRED_PACKAGES = ["transformers", "torch", "huggingface_hub"]
missing_packages = []

for package in REQUIRED_PACKAGES:
    try:
        __import__(package)
    except ImportError:
        missing_packages.append(package)

if missing_packages:
    print("❌ Missing required packages for real model usage:")
    for pkg in missing_packages:
        print(f"   • {pkg}")
    print("\n📦 Install with:")
    print(f"   pip install {' '.join(missing_packages)}")
    print("\n💡 For full AI capabilities, install:")
    print("   pip install transformers torch huggingface-hub accelerate")
    sys.exit(1)

# Import AI libraries
try:
    import torch
    from transformers import (
        AutoTokenizer, 
        AutoModelForCausalLM, 
        TextIteratorStreamer,
        StoppingCriteria,
        StoppingCriteriaList
    )
    from huggingface_hub import hf_hub_download, snapshot_download
    from threading import Thread
    import gc
except ImportError as e:
    print(f"❌ Import error: {e}")
    sys.exit(1)

# Import EthervoxAI components
from ethervoxai import local_llm_stack
from ethervoxai.platform_detector import platform_detector
from ethervoxai.model_manager import model_manager

class RealModelInference:
    """Real AI model inference using transformers"""
    
    def __init__(self):
        self.model = None
        self.tokenizer = None
        self.device = None
        self.model_name = None
        self.is_loaded = False
        
    async def initialize(self, model_name: str = "microsoft/DialoGPT-small"):
        """Initialize with a real model from Hugging Face"""
        print(f"\n🤖 Initializing real AI model: {model_name}")
        
        # Detect optimal device
        self.device = "cuda" if torch.cuda.is_available() else "cpu"
        print(f"🔧 Using device: {self.device}")
        
        try:
            print("📥 Downloading tokenizer...")
            self.tokenizer = AutoTokenizer.from_pretrained(model_name)
            
            # Add padding token if missing
            if self.tokenizer.pad_token is None:
                self.tokenizer.pad_token = self.tokenizer.eos_token
            
            print("📥 Downloading model...")
            self.model = AutoModelForCausalLM.from_pretrained(
                model_name,
                torch_dtype=torch.float16 if self.device == "cuda" else torch.float32,
                device_map="auto" if self.device == "cuda" else None,
                low_cpu_mem_usage=True
            )
            
            if self.device == "cpu":
                self.model = self.model.to(self.device)
            
            self.model_name = model_name
            self.is_loaded = True
            
            print(f"✅ Model loaded successfully!")
            print(f"📊 Model parameters: ~{self.model.num_parameters() / 1e6:.1f}M")
            
        except Exception as e:
            print(f"❌ Failed to load model: {e}")
            return False
        
        return True
    
    def generate_response(self, prompt: str, max_length: int = 100) -> str:
        """Generate a response using the real model"""
        if not self.is_loaded:
            return "❌ Model not loaded"
        
        try:
            # Encode the input
            inputs = self.tokenizer.encode(prompt, return_tensors="pt").to(self.device)
            
            # Generate response
            with torch.no_grad():
                outputs = self.model.generate(
                    inputs,
                    max_length=inputs.shape[1] + max_length,
                    num_return_sequences=1,
                    temperature=0.7,
                    do_sample=True,
                    pad_token_id=self.tokenizer.eos_token_id
                )
            
            # Decode the response
            response = self.tokenizer.decode(outputs[0], skip_special_tokens=True)
            
            # Remove the original prompt from the response
            if response.startswith(prompt):
                response = response[len(prompt):].strip()
            
            return response
            
        except Exception as e:
            return f"❌ Generation failed: {e}"
    
    async def generate_streaming_response(self, prompt: str, max_length: int = 100):
        """Generate a streaming response"""
        if not self.is_loaded:
            yield "❌ Model not loaded"
            return
        
        try:
            # Set up streaming
            streamer = TextIteratorStreamer(
                self.tokenizer, 
                skip_prompt=True, 
                skip_special_tokens=True
            )
            
            inputs = self.tokenizer.encode(prompt, return_tensors="pt").to(self.device)
            
            # Generation parameters
            generation_kwargs = {
                "input_ids": inputs,
                "max_length": inputs.shape[1] + max_length,
                "temperature": 0.7,
                "do_sample": True,
                "pad_token_id": self.tokenizer.eos_token_id,
                "streamer": streamer
            }
            
            # Start generation in a separate thread
            thread = Thread(target=self.model.generate, kwargs=generation_kwargs)
            thread.start()
            
            # Stream the results
            for new_text in streamer:
                yield new_text
                await asyncio.sleep(0.01)  # Small delay for smooth streaming
            
            thread.join()
            
        except Exception as e:
            yield f"❌ Streaming failed: {e}"
    
    def cleanup(self):
        """Clean up model resources"""
        if self.model:
            del self.model
        if self.tokenizer:
            del self.tokenizer
        
        # Force garbage collection
        gc.collect()
        if torch.cuda.is_available():
            torch.cuda.empty_cache()
        
        self.is_loaded = False
        print("🧹 Model resources cleaned up")

async def demonstrate_real_model():
    """Demonstrate real model inference"""
    print("🎯 EthervoxAI Real Model Demonstration")
    print("=" * 60)
    
    # Initialize EthervoxAI platform detection
    print("🔍 Analyzing system capabilities...")
    capabilities = await platform_detector.get_capabilities()
    
    print(f"💻 System: {capabilities.platform} {capabilities.architecture}")
    print(f"🧠 Memory: {capabilities.total_memory}MB (Performance: {capabilities.performance_tier})")
    print(f"⚡ AI Acceleration: {'NEON' if capabilities.has_neon else 'None detected'}")
    
    # Determine optimal model based on system
    if capabilities.total_memory < 4000:  # Less than 4GB
        model_name = "microsoft/DialoGPT-small"
        print("📦 Selected model: DialoGPT-small (optimized for low memory)")
    elif capabilities.total_memory < 8000:  # Less than 8GB
        model_name = "microsoft/DialoGPT-medium"
        print("📦 Selected model: DialoGPT-medium (balanced performance)")
    else:  # 8GB or more
        model_name = "microsoft/DialoGPT-large"
        print("📦 Selected model: DialoGPT-large (best quality)")
    
    # Initialize real model inference
    real_model = RealModelInference()
    
    print(f"\n🚀 Loading model: {model_name}")
    print("⏳ This may take a few minutes for first-time download...")
    
    start_time = time.time()
    success = await real_model.initialize(model_name)
    load_time = time.time() - start_time
    
    if not success:
        print("❌ Failed to initialize real model")
        return
    
    print(f"⚡ Model loaded in {load_time:.1f} seconds")
    
    # Test conversations
    test_prompts = [
        "Hello! How are you?",
        "What's the weather like?",
        "Can you help me with programming?",
        "Tell me a joke",
        "What is artificial intelligence?"
    ]
    
    print("\n💬 Testing Real AI Conversations")
    print("-" * 40)
    
    for i, prompt in enumerate(test_prompts, 1):
        print(f"\n[{i}] Human: {prompt}")
        
        # Generate response with timing
        start_time = time.time()
        response = real_model.generate_response(prompt, max_length=50)
        response_time = time.time() - start_time
        
        print(f"🤖 AI: {response}")
        print(f"⏱️  Response time: {response_time:.2f}s")
        
        # Small delay between conversations
        await asyncio.sleep(0.5)
    
    # Demonstrate streaming
    print("\n🌊 Demonstrating Streaming Response")
    print("-" * 40)
    
    streaming_prompt = "Tell me about the future of artificial intelligence"
    print(f"Human: {streaming_prompt}")
    print("🤖 AI: ", end="", flush=True)
    
    start_time = time.time()
    full_response = ""
    
    async for token in real_model.generate_streaming_response(streaming_prompt, max_length=100):
        print(token, end="", flush=True)
        full_response += token
        await asyncio.sleep(0.02)  # Simulate typing delay
    
    stream_time = time.time() - start_time
    token_count = len(full_response.split())
    tokens_per_second = token_count / stream_time if stream_time > 0 else 0
    
    print(f"\n⚡ Streaming stats: {token_count} tokens in {stream_time:.2f}s ({tokens_per_second:.1f} tok/s)")
    
    # Performance analysis
    print("\n📊 Performance Analysis")
    print("-" * 40)
    
    # Memory usage
    if torch.cuda.is_available():
        memory_used = torch.cuda.memory_allocated() / 1024**2  # MB
        print(f"🔧 GPU Memory: {memory_used:.1f}MB")
    
    print(f"🎯 Model: {model_name}")
    print(f"🔧 Device: {real_model.device}")
    print(f"⚡ Average response time: {response_time:.2f}s")
    print(f"🌊 Streaming speed: {tokens_per_second:.1f} tokens/second")
    
    # Integration with EthervoxAI stack
    print("\n🔗 EthervoxAI Integration Test")
    print("-" * 40)
    
    try:
        # Get system status from LLM stack
        status = await local_llm_stack.get_system_status()
        print(f"🔧 LLM Stack Status: {'Initialized' if status['is_initialized'] else 'Ready for initialization'}")
        
        # Test intent parsing
        intent = await local_llm_stack._parse_intent("What's 2 + 2?")
        print(f"🧠 Intent Detection: {intent.intent} (confidence: {intent.confidence:.2f})")
        
        print("✅ EthervoxAI integration working correctly")
        
    except Exception as e:
        print(f"⚠️ EthervoxAI integration test failed: {e}")
    
    # Cleanup
    print("\n🧹 Cleaning up...")
    real_model.cleanup()
    
    print("\n" + "=" * 60)
    print("🎉 Real Model Demonstration Complete!")
    print("✅ Successfully downloaded and used a real AI model")
    print("✅ Demonstrated text generation and streaming")
    print("✅ Measured performance and system integration")
    print("\n💡 This proves EthervoxAI can work with real AI models!")

async def quick_model_test():
    """Quick test with a very small model for faster demo"""
    print("🚀 Quick Real Model Test (Small Model)")
    print("=" * 50)
    
    # Use the smallest possible model for demo
    model_name = "gpt2"  # ~500MB, very fast to download
    
    real_model = RealModelInference()
    
    print(f"📦 Loading {model_name} (small, fast download)...")
    success = await real_model.initialize(model_name)
    
    if success:
        print("✅ Model loaded successfully!")
        
        # Single test
        prompt = "The future of AI is"
        print(f"\nPrompt: {prompt}")
        response = real_model.generate_response(prompt, max_length=30)
        print(f"Response: {response}")
        
        real_model.cleanup()
        print("🎉 Quick test complete!")
    else:
        print("❌ Quick test failed")

def check_system_requirements():
    """Check if system can handle real models"""
    print("🔍 Checking System Requirements for Real Models")
    print("=" * 55)
    
    # Check Python version
    python_version = sys.version_info
    print(f"🐍 Python: {python_version.major}.{python_version.minor}.{python_version.micro}")
    
    # Check PyTorch
    print(f"🔥 PyTorch: {torch.__version__}")
    print(f"🔧 CUDA Available: {torch.cuda.is_available()}")
    
    if torch.cuda.is_available():
        print(f"🎮 GPU: {torch.cuda.get_device_name(0)}")
        print(f"💾 GPU Memory: {torch.cuda.get_device_properties(0).total_memory / 1024**3:.1f}GB")
    
    # Memory recommendation
    import psutil
    memory_gb = psutil.virtual_memory().total / 1024**3
    print(f"🧠 System RAM: {memory_gb:.1f}GB")
    
    if memory_gb < 4:
        print("⚠️  Recommendation: Use DialoGPT-small or GPT-2")
    elif memory_gb < 8:
        print("✅ Recommendation: DialoGPT-medium or small transformers")
    else:
        print("🚀 Recommendation: Can handle larger models like DialoGPT-large")
    
    print()

async def main():
    """Main demonstration"""
    
    try:
        # Check requirements first
        check_system_requirements()
        
        # Ask user what they want to do
        print("Choose demonstration mode:")
        print("1. Quick test with GPT-2 (fastest)")
        print("2. Full demonstration with optimized model")
        print("3. System requirements only")
        
        try:
            choice = input("\nEnter choice (1-3) [default: 1]: ").strip()
            if not choice:
                choice = "1"
        except (EOFError, KeyboardInterrupt):
            choice = "1"
        
        if choice == "1":
            await quick_model_test()
        elif choice == "2":
            await demonstrate_real_model()
        elif choice == "3":
            print("✅ System requirements check complete!")
        else:
            print("❌ Invalid choice, running quick test...")
            await quick_model_test()
            
    except KeyboardInterrupt:
        print("\n🛑 Demonstration interrupted by user")
    except Exception as e:
        print(f"\n❌ Demonstration failed: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    print("🤖 EthervoxAI Real Model Example")
    print("Demonstrating real AI model integration with EthervoxAI")
    print()
    
    asyncio.run(main())
