#!/usr/bin/env python3
"""
🤖 EthervoxAI Examples Showcase

This script provides an interactive menu to run all available Python examples
and demonstrates the progression from sim    print("🌐 Online Resources:")
    print("   • GitHub: https://github.com/ethervox-ai/ethervoxai")
    print("   • Documentation: See docs/ folder") mock AI to real AI models.

Usage:
    python showcase_examples.py
"""

import asyncio
import sys
import subprocess
from pathlib import Path

# Add the parent directory to the path
sys.path.insert(0, str(Path(__file__).parent.parent))

def print_banner():
    """Print the showcase banner"""
    print("🤖 EthervoxAI Python Examples Showcase")
    print("=" * 50)
    print("Explore AI with Privacy - Choose Your Adventure!")
    print("=" * 50)

def print_examples_menu():
    """Print the examples menu"""
    print("\n📋 Available Examples:")
    print()
    
    examples = [
        {
            "number": "1",
            "name": "Simple Real Model",
            "file": "simple_real_model.py",
            "time": "~30 seconds",
            "level": "🟢 Beginner",
            "deps": "None required",
            "description": "Quick demo with mock AI - perfect for first-time users"
        },
        {
            "number": "2", 
            "name": "Complete Real Model",
            "file": "complete_real_model.py", 
            "time": "~5 minutes",
            "level": "🟡 Intermediate",
            "deps": "None required",
            "description": "Full AI pipeline with neural processing simulation"
        },
        {
            "number": "3",
            "name": "Advanced Multi-Backend",
            "file": "advanced_real_model.py",
            "time": "Variable",
            "level": "🟠 Advanced", 
            "deps": "Optional AI libs",
            "description": "Production-ready with multiple AI backends"
        },
        {
            "number": "4",
            "name": "Heavy Real Models",
            "file": "real_model_example.py",
            "time": "~10+ minutes",
            "level": "🔴 Expert",
            "deps": "PyTorch + 4GB RAM",
            "description": "Real AI models from Hugging Face (requires downloads)"
        }
    ]
    
    for example in examples:
        print(f"[{example['number']}] {example['name']}")
        print(f"    📁 File: {example['file']}")
        print(f"    ⏱️  Time: {example['time']}")
        print(f"    🎯 Level: {example['level']}")
        print(f"    📦 Dependencies: {example['deps']}")
        print(f"    💡 {example['description']}")
        print()
    
    print("[5] 🔍 System Requirements Check")
    print("[6] 📚 View Documentation")
    print("[7] 🧪 Run All Tests")
    print("[0] ❌ Exit")

def check_system_requirements():
    """Check system requirements and AI capabilities"""
    print("\n🔍 System Requirements Analysis")
    print("=" * 40)
    
    # Check Python version
    python_version = sys.version_info
    print(f"🐍 Python: {python_version.major}.{python_version.minor}.{python_version.micro}", end="")
    
    if python_version >= (3, 11):
        print(" ✅")
    else:
        print(" ⚠️ (Recommended: 3.11+)")
    
    # Check memory
    try:
        import psutil
        memory_gb = psutil.virtual_memory().total / 1024**3
        print(f"🧠 RAM: {memory_gb:.1f}GB", end="")
        
        if memory_gb >= 8:
            print(" ✅ (Can run all examples)")
        elif memory_gb >= 4:
            print(" 🟡 (Can run most examples)")
        else:
            print(" ⚠️ (Limited to basic examples)")
    except ImportError:
        print("🧠 RAM: Unknown (psutil not installed)")
    
    # Check disk space
    try:
        import shutil
        disk_gb = shutil.disk_usage(Path.cwd()).free / 1024**3
        print(f"💾 Free Disk: {disk_gb:.1f}GB", end="")
        
        if disk_gb >= 10:
            print(" ✅ (Can download large models)")
        elif disk_gb >= 2:
            print(" 🟡 (Can run basic models)")
        else:
            print(" ⚠️ (Limited disk space)")
    except:
        print("💾 Free Disk: Unknown")
    
    # Check AI dependencies
    print("\n🤖 AI Dependencies:")
    
    dependencies = [
        ("PyTorch", "torch"),
        ("Transformers", "transformers"), 
        ("Hugging Face Hub", "huggingface_hub"),
        ("OpenAI", "openai"),
        ("ONNX Runtime", "onnxruntime")
    ]
    
    for name, module in dependencies:
        try:
            __import__(module)
            print(f"   {name}: ✅ Installed")
        except ImportError:
            print(f"   {name}: ❌ Not installed")
    
    # Recommendations
    print("\n💡 Recommendations:")
    
    if memory_gb < 4:
        print("   • Start with examples 1-2 (lightweight)")
        print("   • Consider upgrading RAM for real AI models")
    elif memory_gb < 8:
        print("   • Examples 1-3 recommended")
        print("   • Example 4 may be slow")
    else:
        print("   • All examples supported!")
        print("   • Perfect for real AI model exploration")
    
    print("\n📦 Installation Commands:")
    print("   Basic AI: pip install transformers torch huggingface-hub")
    print("   Full AI:  pip install transformers torch huggingface-hub accelerate openai")

def view_documentation():
    """Display documentation information"""
    print("\n📚 EthervoxAI Documentation")
    print("=" * 40)
    
    docs = [
        ("Examples README", "examples/README.md"),
        ("Python Implementation", "../README.md"),
        ("Privacy Dashboard", "../../docs/modules/privacy-dashboard.md"),
        ("Multi-language Runtime", "../../docs/modules/multilingual-runtime.md"),
        ("Local LLM Stack", "../../docs/modules/local-llm-stack.md"),
        ("Project Overview", "../../README.md")
    ]
    
    for name, path in docs:
        full_path = Path(__file__).parent / path
        if full_path.exists():
            print(f"✅ {name}: {path}")
        else:
            print(f"📝 {name}: {path}")
    
    print("\n🌐 Online Resources:")
    print("   • GitHub: https://github.com/[your-username]/ethervoxai")
    print("   • Documentation: See docs/ folder") 
    print("   • Contributing: CONTRIBUTING.md")

def run_tests():
    """Run the test suite"""
    print("\n🧪 Running EthervoxAI Test Suite")
    print("=" * 40)
    
    try:
        # Run the setup script which includes validation
        test_path = Path(__file__).parent.parent / "setup.py"
        result = subprocess.run([sys.executable, str(test_path)], 
                              capture_output=True, text=True, cwd=test_path.parent)
        
        if result.returncode == 0:
            print("✅ All tests passed!")
            print("\nTest output:")
            print(result.stdout)
        else:
            print("❌ Some tests failed!")
            print("\nError output:")
            print(result.stderr)
            
    except Exception as e:
        print(f"❌ Failed to run tests: {e}")

def run_example(example_number: str):
    """Run a specific example"""
    examples = {
        "1": "simple_real_model.py",
        "2": "complete_real_model.py", 
        "3": "advanced_real_model.py",
        "4": "real_model_example.py"
    }
    
    if example_number not in examples:
        print("❌ Invalid example number")
        return
    
    example_file = examples[example_number]
    # Fix path - run from examples directory
    example_path = Path(__file__).parent / example_file
    
    if not example_path.exists():
        print(f"❌ Example file not found: {example_file}")
        return
    
    print(f"\n🚀 Running {example_file}")
    print("=" * 50)
    
    try:
        # Run from the examples directory
        result = subprocess.run([sys.executable, str(example_path)], 
                              cwd=Path(__file__).parent)
        
        if result.returncode == 0:
            print(f"\n✅ {example_file} completed successfully!")
        else:
            print(f"\n❌ {example_file} failed with return code {result.returncode}")
            
    except KeyboardInterrupt:
        print(f"\n🛑 {example_file} interrupted by user")
    except Exception as e:
        print(f"\n❌ Failed to run {example_file}: {e}")

def main():
    """Main showcase function"""
    print_banner()
    
    while True:
        print_examples_menu()
        
        try:
            choice = input("\n🎯 Enter your choice (0-7): ").strip()
            
            if choice == "0":
                print("\n👋 Thanks for exploring EthervoxAI!")
                print("🔒 Remember: Your privacy is our priority!")
                break
            elif choice in ["1", "2", "3", "4"]:
                run_example(choice)
            elif choice == "5":
                check_system_requirements()
            elif choice == "6":
                view_documentation()
            elif choice == "7":
                run_tests()
            else:
                print("❌ Invalid choice. Please enter 0-7.")
            
            input("\n⏸️  Press Enter to continue...")
            
        except (EOFError, KeyboardInterrupt):
            print("\n\n👋 Goodbye!")
            break
        except Exception as e:
            print(f"\n❌ Error: {e}")
            input("\n⏸️  Press Enter to continue...")

if __name__ == "__main__":
    main()
