# 🧠 EthervoxAI Python Implementation

Python reference implementation of EthervoxAI following the cross-language protocol specifications.

## 🚀 Quick Start

### Prerequisites

- Python 3.11 or higher
- 4GB+ RAM (8GB+ recommended)
- Internet connection for model downloads

### Installation

1. **Clone the repository** (if not already done):
   ```bash
   git clone <repository-url>
   cd ethervoxai/implementations/python
   ```

2. **Install dependencies**:
   ```bash
   pip install -r requirements.txt
   ```

3. **Run the demonstration**:
   ```bash
   python examples/basic_usage.py
   ```

4. **Run tests**:
   ```bash
   python tests/test_ethervoxai.py
   # OR with pytest:
   pytest tests/test_ethervoxai.py -v
   ```

## Dependencies

- `psutil` - Advanced system information
- `aiohttp` - Async HTTP client for model downloads
- `aiofiles` - Async file operations
- `numpy` - Numerical computing (optional, for inference optimizations)
- `tqdm` - Progress bars
- `pydantic` - Data validation and serialization

## Development Dependencies

- `pytest` - Testing framework
- `pytest-asyncio` - Async test support
- `black` - Code formatting
- `mypy` - Type checking
- `flake8` - Linting

## Usage

```python
import asyncio
from ethervoxai import platform_detector, model_manager, inference_engine

async def main():
    # Detect system capabilities
    capabilities = await platform_detector.get_capabilities()
    print(f"System: {capabilities.performance_tier} ({capabilities.total_memory}MB)")
    
    # Get recommended models
    models = await platform_detector.get_recommended_models()
    print(f"Recommended models: {[m['name'] for m in models]}")
    
    # Download and use a model
    if models:
        model_name = models[0]['name']
        model_path = await model_manager.get_model_path(model_name)
        
        await inference_engine.initialize(model_name)
        response = await inference_engine.complete("Hello, how are you?")
        print(f"AI: {response.text}")

if __name__ == "__main__":
    asyncio.run(main())
```

## Platform-Specific Features

### Raspberry Pi Optimizations
- ARM NEON instruction detection
- Memory constraint handling
- GPIO integration support
- Power management awareness

### Linux Server Features
- Advanced CPU feature detection
- NUMA awareness
- Container environment detection
- High-performance async I/O

### Development Features
- Jupyter notebook examples
- Performance profiling tools
- Model conversion utilities
- Cross-platform testing
