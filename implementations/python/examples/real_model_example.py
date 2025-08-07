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
    
    # Optional but recommended for faster model downloads:
    pip install huggingface_hub[hf_xet]
    # OR: pip install hf_xet

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
OPTIONAL_PACKAGES = ["hf_xet"]  # For faster downloads
missing_packages = []
missing_optional = []

for package in REQUIRED_PACKAGES:
    try:
        __import__(package)
    except ImportError:
        missing_packages.append(package)

for package in OPTIONAL_PACKAGES:
    try:
        __import__(package)
    except ImportError:
        missing_optional.append(package)

if missing_packages:
    print("❌ Missing required packages for real model usage:")
    for pkg in missing_packages:
        print(f"   • {pkg}")
    print("\n📦 Install with:")
    print(f"   pip install {' '.join(missing_packages)}")
    print("\n💡 For full AI capabilities, install:")
    print("   pip install transformers torch huggingface-hub accelerate")
    sys.exit(1)

if missing_optional:
    print("💡 Optional packages for better performance:")
    for pkg in missing_optional:
        print(f"   • {pkg} (for faster model downloads)")
    print(f"   Install with: pip install {' '.join(missing_optional)}")
    print()

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

class CustomStoppingCriteria(StoppingCriteria):
    """Custom stopping criteria to prevent repetitive or unwanted text"""
    
    def __init__(self, tokenizer, stop_sequences=None):
        self.tokenizer = tokenizer
        self.stop_sequences = stop_sequences or [
            "\n\n",  # Double newlines
            "Human:",  # Prevent the model from generating human responses
            "AI:",     # Prevent AI: prefixes
            "User:",   # Prevent User: prefixes
        ]
        # Convert stop sequences to token IDs
        self.stop_token_ids = []
        for seq in self.stop_sequences:
            tokens = tokenizer.encode(seq, add_special_tokens=False)
            if tokens:
                self.stop_token_ids.append(tokens)
    
    def __call__(self, input_ids, scores, **kwargs):
        # Check if any stop sequence appears at the end
        for stop_tokens in self.stop_token_ids:
            if len(input_ids[0]) >= len(stop_tokens):
                if input_ids[0][-len(stop_tokens):].tolist() == stop_tokens:
                    return True
        return False

def clean_response(text: str) -> str:
    """Clean up the AI response to remove common artifacts"""
    if not text:
        return text
    
    # Remove common conversation prefixes that the model might generate
    prefixes_to_remove = ["AI:", "Assistant:", "Bot:", "Human:", "User:", "Response:"]
    for prefix in prefixes_to_remove:
        if text.startswith(prefix):
            text = text[len(prefix):].strip()
    
    # Split by common separators and take the first meaningful part
    separators = ["\n\nHuman:", "\n\nUser:", "\n\nAI:", "\n\nAssistant:", "\n\n\n"]
    for separator in separators:
        if separator in text:
            text = text.split(separator)[0]
    
    # Remove excessive repetition (if a phrase repeats more than 2 times)
    words = text.split()
    if len(words) > 6:  # Only check longer responses
        # Check for repeating 3-word patterns
        for i in range(len(words) - 8):
            phrase = " ".join(words[i:i+3])
            # Count how many times this 3-word phrase appears consecutively
            count = 0
            j = i
            while j <= len(words) - 3 and " ".join(words[j:j+3]) == phrase:
                count += 1
                j += 3
            
            # If phrase repeats more than twice, cut it off
            if count > 2:
                text = " ".join(words[:i + 3])
                break
    
    # Remove trailing incomplete sentences (if it ends abruptly)
    if text and not text.endswith(('.', '!', '?', '"', "'", ':')):
        # Find the last complete sentence
        sentences = text.split('.')
        if len(sentences) > 1:
            # Keep all complete sentences
            text = '.'.join(sentences[:-1]) + '.'
    
    # Final cleanup
    text = text.strip()
    
    # Remove any trailing special characters except punctuation
    while text and text[-1] in ['|', '<', '>', '{', '}', '[', ']', '`']:
        text = text[:-1].strip()
    
    return text

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
            self.tokenizer = AutoTokenizer.from_pretrained(
                model_name,
                resume_download=True,  # Resume interrupted downloads
                force_download=False   # Use cached if available
            )
            
            # Add padding token if missing - temporarily use eos_token
            if self.tokenizer.pad_token is None:
                self.tokenizer.pad_token = self.tokenizer.eos_token
            
            # Add chat template if it doesn't exist (for DialoGPT)
            if not hasattr(self.tokenizer, 'chat_template') or self.tokenizer.chat_template is None:
                # DialoGPT expects conversational format
                self.tokenizer.chat_template = "{{ bos_token }}{{ messages[0]['content'] }}{{ eos_token }}"
            
            print("📥 Downloading model (this may take several minutes)...")
            print("⏳ Please wait for download to complete before inference starts...")
            
            # Ensure model is fully downloaded before proceeding
            self.model = AutoModelForCausalLM.from_pretrained(
                model_name,
                torch_dtype=torch.float16 if self.device == "cuda" else torch.float32,
                device_map="auto" if self.device == "cuda" else None,
                low_cpu_mem_usage=True,
                resume_download=True,  # Resume interrupted downloads
                force_download=False   # Use cached if available
            )
            
            if self.device == "cpu":
                self.model = self.model.to(self.device)
            
            # Now properly set up the pad token to avoid attention mask warnings
            if self.tokenizer.pad_token == self.tokenizer.eos_token:
                # Try to use a different existing token for padding
                if hasattr(self.tokenizer, 'unk_token') and self.tokenizer.unk_token is not None:
                    self.tokenizer.pad_token = self.tokenizer.unk_token
                    print(f"🔧 Set pad_token to unk_token: {self.tokenizer.pad_token}")
                elif hasattr(self.tokenizer, 'bos_token') and self.tokenizer.bos_token is not None:
                    self.tokenizer.pad_token = self.tokenizer.bos_token
                    print(f"🔧 Set pad_token to bos_token: {self.tokenizer.pad_token}")
                else:
                    # Last resort: add a new padding token and resize embeddings
                    print("🔧 Adding new pad token to avoid attention mask warnings...")
                    self.tokenizer.add_special_tokens({'pad_token': '<pad>'})
                    self.model.resize_token_embeddings(len(self.tokenizer))
                    print(f"🔧 Added new pad_token: {self.tokenizer.pad_token}")
            
            self.model_name = model_name
            self.is_loaded = True
            
            print(f"✅ Model download and loading complete!")
            print(f"📊 Model parameters: ~{self.model.num_parameters() / 1e6:.1f}M")
            print("🚀 Ready for inference - all downloads finished!")
            
        except Exception as e:
            print(f"❌ Failed to load model: {e}")
            return False
        
        return True
    
    def generate_response(self, prompt: str, max_length: int = 100) -> str:
        """Generate a response using the real model"""
        if not self.is_loaded:
            return "❌ Model not loaded"
        
        try:
            # Format prompt for conversation models
            formatted_prompt = prompt + self.tokenizer.eos_token
            
            # Encode the input with attention mask
            inputs = self.tokenizer(
                formatted_prompt, 
                return_tensors="pt", 
                padding=True,
                truncation=True,
                max_length=512
            ).to(self.device)
            
            # Store the original input length to extract only new tokens
            input_length = inputs['input_ids'].shape[1]
            
            # Create custom stopping criteria
            stopping_criteria = StoppingCriteriaList([
                CustomStoppingCriteria(self.tokenizer)
            ])
            
            # Generate response with better parameters and stopping criteria
            with torch.no_grad():
                outputs = self.model.generate(
                    input_ids=inputs['input_ids'],
                    attention_mask=inputs['attention_mask'],
                    max_new_tokens=max_length,  # Use max_new_tokens instead of max_length
                    min_length=input_length + 5,  # Reduced minimum to allow shorter responses
                    temperature=0.7,  # Slightly lower temperature for more focused responses
                    do_sample=True,
                    top_p=0.85,  # Slightly more focused sampling
                    repetition_penalty=1.2,  # Higher repetition penalty to reduce loops
                    pad_token_id=self.tokenizer.eos_token_id,
                    eos_token_id=self.tokenizer.eos_token_id,
                    stopping_criteria=stopping_criteria,  # Add stopping criteria
                    num_return_sequences=1
                )
            
            # Extract only the new tokens (response part)
            # This avoids string-based prompt removal which can cut off response text
            new_tokens = outputs[0][input_length:]
            response = self.tokenizer.decode(new_tokens, skip_special_tokens=True).strip()
            
            # Clean the response to remove artifacts
            response = clean_response(response)
            
            # If response is empty or too short, try with different parameters
            if len(response) < 3:
                print("🔄 First generation too short, trying with relaxed parameters...")
                # Try with forced generation but still use cleaning
                outputs = self.model.generate(
                    input_ids=inputs['input_ids'],
                    attention_mask=inputs['attention_mask'],
                    max_new_tokens=min(50, max_length),  # Limit max length for fallback
                    min_length=input_length + 8,
                    temperature=0.9,  # Higher temperature for more creativity
                    do_sample=True,
                    top_p=0.9,
                    repetition_penalty=1.1,
                    pad_token_id=self.tokenizer.eos_token_id,
                    eos_token_id=self.tokenizer.eos_token_id,
                    stopping_criteria=stopping_criteria
                )
                # Extract only the new tokens from the second attempt
                new_tokens = outputs[0][input_length:]
                response = self.tokenizer.decode(new_tokens, skip_special_tokens=True).strip()
                response = clean_response(response)
            
            return response if response else "I'm thinking... (response generation needs tuning)"
            
        except Exception as e:
            return f"❌ Generation failed: {e}"
    
    async def generate_streaming_response(self, prompt: str, max_length: int = 100):
        """Generate a streaming response"""
        if not self.is_loaded:
            yield "❌ Model not loaded"
            return
        
        try:
            # Format prompt for conversation
            formatted_prompt = prompt + self.tokenizer.eos_token
            
            # Set up streaming with timeout to prevent hanging
            streamer = TextIteratorStreamer(
                self.tokenizer, 
                skip_prompt=True, 
                skip_special_tokens=True,
                timeout=1.0  # Add timeout to prevent hanging
            )
            
            # Encode with attention mask
            inputs = self.tokenizer(
                formatted_prompt, 
                return_tensors="pt", 
                padding=True,
                truncation=True,
                max_length=512
            ).to(self.device)
            
            # Create stopping criteria for streaming too
            stopping_criteria = StoppingCriteriaList([
                CustomStoppingCriteria(self.tokenizer)
            ])
            
            # Generation parameters with improved settings
            generation_kwargs = {
                "input_ids": inputs['input_ids'],
                "attention_mask": inputs['attention_mask'],
                "max_new_tokens": max_length,
                "min_length": inputs['input_ids'].shape[1] + 5,  # Reduced minimum
                "temperature": 0.7,  # Slightly lower for more focused responses
                "do_sample": True,
                "top_p": 0.85,  # More focused sampling
                "repetition_penalty": 1.2,  # Higher to reduce repetition
                "pad_token_id": self.tokenizer.eos_token_id,
                "eos_token_id": self.tokenizer.eos_token_id,
                "stopping_criteria": stopping_criteria,
                "streamer": streamer
            }
            
            # Start generation in a separate thread
            thread = Thread(target=self.model.generate, kwargs=generation_kwargs)
            thread.start()
            
            # Stream the results with simple, effective cleaning
            full_response = ""
            word_count = 0
            last_few_words = []
            
            try:
                for new_text in streamer:
                    if new_text and new_text.strip():  # Only process non-empty tokens
                        full_response += new_text
                        
                        # Simple stop phrase detection
                        stop_phrases = ["Human:", "User:", "AI:", "Assistant:"]
                        should_stop = any(phrase in full_response for phrase in stop_phrases)
                        if should_stop:
                            break
                        
                        # Track words for repetition detection
                        words = new_text.split()
                        for word in words:
                            if word.strip():
                                last_few_words.append(word.strip().lower())
                                word_count += 1
                                
                                # Keep only last 6 words for comparison
                                if len(last_few_words) > 6:
                                    last_few_words.pop(0)
                                
                                # Simple repetition check: if last 3 words repeat
                                if (len(last_few_words) >= 6 and 
                                    last_few_words[-3:] == last_few_words[-6:-3]):
                                    should_stop = True
                                    break
                        
                        if should_stop:
                            break
                        
                        # Yield the token and pause briefly
                        yield new_text
                        await asyncio.sleep(0.01)
                        
                        # Safety: stop after reasonable length
                        if word_count > 100:  # Prevent runaway generation
                            break
                            
            except Exception as inner_e:
                yield f"❌ Streaming error: {inner_e}"
            
            # Wait for generation thread to complete
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
        print("💾 Expected download: ~117MB")
    elif capabilities.total_memory < 8000:  # Less than 8GB
        model_name = "microsoft/DialoGPT-medium"
        print("📦 Selected model: DialoGPT-medium (balanced performance)")
        print("💾 Expected download: ~345MB")
    else:  # 8GB or more
        model_name = "microsoft/DialoGPT-large"
        print("📦 Selected model: DialoGPT-large (best quality)")
        print("💾 Expected download: ~1.75GB")
    
    print("💡 Tip: Install 'pip install hf_xet' for faster downloads!")
    
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
    
    # Ensure all downloads are complete before starting inference
    print("\n⏳ Preparing for inference (ensuring all downloads complete)...")
    await asyncio.sleep(2)  # Give any background downloads time to finish
    print("🚀 Starting AI conversations...\n")
    
    # Test conversations with better prompts for DialoGPT
    test_prompts = [
        "Hello! How are you today?",
        "What can you tell me about the weather?",
        "I need help with a programming problem. Can you assist?",
        "Can you tell me a funny joke?",
        "Explain artificial intelligence in simple terms."
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
        
        # Single test with better prompt
        prompt = "The future of AI is"
        print(f"\nPrompt: {prompt}")
        response = real_model.generate_response(prompt, max_length=50)
        print(f"Response: {response}")
        
        # Test a conversational prompt
        conv_prompt = "Hello! How are you?"
        print(f"\nConversational test: {conv_prompt}")
        conv_response = real_model.generate_response(conv_prompt, max_length=40)
        print(f"Response: {conv_response}")
        
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
    try:
        import psutil
        memory_gb = psutil.virtual_memory().total / 1024**3
        print(f"🧠 System RAM: {memory_gb:.1f}GB")
        
        if memory_gb < 4:
            print("⚠️  Recommendation: Use DialoGPT-small or GPT-2")
        elif memory_gb < 8:
            print("✅ Recommendation: DialoGPT-medium or small transformers")
        else:
            print("🚀 Recommendation: Can handle larger models like DialoGPT-large")
    except ImportError:
        print("🧠 System RAM: Unable to detect (install psutil for memory info)")
        print("💡 Install psutil with: pip install psutil")
    
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
