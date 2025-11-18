# LLM Backend Implementation

This directory contains the LLM (Large Language Model) backend implementations for EthervoxAI, enabling local and external AI model integration.

## Overview

The LLM backend system provides a unified interface for running language models on EthervoxAI, supporting multiple backends:

- **Llama Backend**: Full-featured backend using llama.cpp for GGUF models
- **TinyLlama Backend**: Optimized backend for resource-constrained devices
- **External Backend**: Integration with cloud APIs (OpenAI, Anthropic, etc.)

## Architecture

```
┌─────────────────────────────────────────┐
│      Dialogue Engine (dialogue.h)       │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│      LLM Backend Interface (llm.h)      │
├─────────────────────────────────────────┤
│  • Backend creation & management        │
│  • Model loading & unloading            │
│  • Text generation                      │
│  • Capabilities query                   │
└──────────────┬──────────────────────────┘
               │
       ┌───────┴────────┬──────────────┐
       ▼                ▼              ▼
┌──────────────┐ ┌─────────────┐ ┌────────────┐
│    Llama     │ │  TinyLlama  │ │  External  │
│   Backend    │ │   Backend   │ │   Backend  │
├──────────────┤ ├─────────────┤ ├────────────┤
│ llama.cpp    │ │ Optimized   │ │ REST APIs  │
│ GGUF models  │ │ for embedded│ │ OpenAI     │
│ GPU support  │ │ devices     │ │ HuggingFace│
└──────────────┘ └─────────────┘ └────────────┘
```

## Files

- **llm_core.c**: Core backend management and utilities
- **llama_backend.c**: Llama.cpp integration for local GGUF models
- **tinyllama_backend.c**: Optimized backend for TinyLlama models
- **include/ethervox/llm.h**: Public API header

## Building with LLM Support

### Prerequisites

For Llama backend support, you need llama.cpp:

```bash
# Clone llama.cpp
git clone https://github.com/ggerganov/llama.cpp.git
cd llama.cpp

# Build llama.cpp
make

# Optional: Install to system
sudo make install
```

### CMake Configuration

```bash
# Enable Llama backend
cmake -B build -DWITH_LLAMA=ON

# Enable GPU support (requires CUDA or Metal)
cmake -B build -DWITH_LLAMA=ON -DLLAMA_CUBLAS=ON

# Disable Llama backend (smaller build)
cmake -B build -DWITH_LLAMA=OFF

# Build
cmake --build build
```

## Usage

### Basic Example

```c
#include "ethervox/llm.h"

// 1. Create backend
ethervox_llm_backend_t* backend = ethervox_llm_create_llama_backend();

// 2. Configure
ethervox_llm_config_t config = ethervox_dialogue_get_default_llm_config();
config.context_length = 2048;
config.max_tokens = 256;
config.temperature = 0.7f;

// 3. Initialize
ethervox_llm_backend_init(backend, &config);

// 4. Load model
ethervox_llm_backend_load_model(backend, "models/tinyllama-1.1b-chat.gguf");

// 5. Generate
ethervox_llm_response_t response;
ethervox_llm_backend_generate(backend, "Hello!", "en", &response);

printf("Response: %s\n", response.text);

// 6. Cleanup
ethervox_llm_response_free(&response);
ethervox_llm_backend_free(backend);
```

### Integration with Dialogue Engine

```c
#include "ethervox/dialogue.h"
#include "ethervox/llm.h"

// Create dialogue engine
ethervox_dialogue_engine_t engine;
ethervox_llm_config_t llm_config = ethervox_dialogue_get_default_llm_config();
llm_config.model_path = "models/tinyllama-1.1b-chat.gguf";

ethervox_dialogue_init(&engine, &llm_config);

// Process intent
ethervox_dialogue_intent_request_t request = {
    .text = "What's the weather like today?",
    .language_code = "en"
};

ethervox_intent_t intent;
ethervox_dialogue_parse_intent(&engine, &request, &intent);

// Generate response using LLM
ethervox_llm_response_t response;
ethervox_dialogue_process_llm(&engine, &intent, NULL, &response);

printf("Response: %s\n", response.text);

// Cleanup
ethervox_intent_free(&intent);
ethervox_llm_response_free(&response);
ethervox_dialogue_cleanup(&engine);
```

## Supported Models

### Recommended GGUF Models

| Model | Size | Context | Use Case |
|-------|------|---------|----------|
| TinyLlama 1.1B | 637MB (Q4_K_M) | 2048 | Embedded devices, RPi |
| Phi-2 2.7B | 1.6GB (Q4_K_M) | 2048 | Desktop, good quality |
| Mistral 7B | 4.1GB (Q4_K_M) | 8192 | Desktop, high quality |
| Llama 2 7B | 3.8GB (Q4_K_M) | 4096 | Desktop, high quality |

### Downloading Models

```bash
# Create models directory
mkdir -p models

# Download TinyLlama (recommended for testing)
wget https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF/resolve/main/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf \
    -O models/tinyllama-1.1b-chat.gguf

# Download Phi-2 (good balance)
wget https://huggingface.co/TheBloke/phi-2-GGUF/resolve/main/phi-2.Q4_K_M.gguf \
    -O models/phi-2.gguf
```

## Configuration Options

### LLM Config Structure

```c
typedef struct {
  char* model_path;           // Path to GGUF model file
  char* model_name;           // Model identifier
  uint32_t max_tokens;        // Max tokens to generate
  uint32_t context_length;    // Context window size
  float temperature;          // Sampling temperature (0.0-2.0)
  float top_p;               // Nucleus sampling threshold
  uint32_t seed;             // Random seed (0 = random)
  bool use_gpu;              // Enable GPU acceleration
  uint32_t gpu_layers;       // Number of layers on GPU
  char* language_code;       // Primary language code
} ethervox_llm_config_t;
```

### Recommended Settings

**Raspberry Pi 4/5:**
```c
config.context_length = 1024;
config.max_tokens = 128;
config.temperature = 0.7f;
config.use_gpu = false;
// Use TinyLlama 1.1B Q4_K_M
```

**Desktop (8GB RAM):**
```c
config.context_length = 2048;
config.max_tokens = 512;
config.temperature = 0.7f;
config.use_gpu = true;
config.gpu_layers = 32;
// Use Mistral 7B or Llama 2 7B Q4_K_M
```

**Desktop (16GB+ RAM):**
```c
config.context_length = 4096;
config.max_tokens = 1024;
config.temperature = 0.7f;
config.use_gpu = true;
config.gpu_layers = 99;  // All layers on GPU
// Use Mistral 7B Q5_K_M or larger
```

## Performance

### Benchmarks

| Device | Model | Speed | Memory |
|--------|-------|-------|--------|
| RPi 4 8GB | TinyLlama 1.1B Q4 | ~10 tok/s | 1.2GB |
| RPi 5 8GB | TinyLlama 1.1B Q4 | ~15 tok/s | 1.2GB |
| Desktop (CPU) | Mistral 7B Q4 | ~8 tok/s | 5GB |
| Desktop (RTX 3060) | Mistral 7B Q4 | ~40 tok/s | 6GB VRAM |
| Desktop (RTX 4090) | Mistral 7B Q5 | ~100 tok/s | 8GB VRAM |

### Optimization Tips

1. **Use quantized models**: Q4_K_M is a good balance of quality and speed
2. **Enable GPU**: Significant speedup if you have compatible hardware
3. **Adjust context**: Smaller context = faster inference
4. **Batch processing**: Process multiple requests together when possible
5. **Model caching**: Keep model loaded for repeated queries

## Troubleshooting

### Model fails to load

```
Error: Failed to load model from: models/model.gguf
```

**Solutions:**
- Verify file exists and is a valid GGUF file
- Check file permissions
- Ensure enough RAM/disk space
- Try a smaller quantization (Q2_K, Q3_K_M)

### Out of memory

```
Error: Failed to create Llama context
```

**Solutions:**
- Reduce `context_length` in config
- Use smaller model or quantization
- Close other applications
- Enable swap space (not recommended for production)

### Slow generation

**Solutions:**
- Enable GPU acceleration (`use_gpu = true`)
- Increase `gpu_layers` if using GPU
- Use smaller model (TinyLlama instead of Mistral)
- Reduce `context_length`

## Development

### Adding a New Backend

1. Create `src/llm/your_backend.c`
2. Implement backend interface:
   - `init()`, `cleanup()`
   - `load_model()`, `unload_model()`
   - `generate()`
   - `get_capabilities()`
3. Add creation function to `include/ethervox/llm.h`
4. Update CMakeLists.txt
5. Add tests and documentation

### Testing

```bash
# Build example
cmake --build build --target llm_example

# Run with TinyLlama
./build/examples/llm_example models/tinyllama-1.1b-chat.gguf

# Run with custom prompt
./build/examples/llm_example models/tinyllama-1.1b-chat.gguf "Explain quantum computing"
```

## References

- [llama.cpp](https://github.com/ggerganov/llama.cpp) - Core inference engine
- [GGUF Format](https://github.com/ggerganov/ggml/blob/master/docs/gguf.md) - Model format spec
- [HuggingFace GGUF Models](https://huggingface.co/models?library=gguf) - Pre-quantized models
- [EthervoxAI SDK](../sdk/README.md) - Full SDK documentation

## License

Copyright (c) 2024-2025 EthervoxAI Team

Licensed under CC BY-NC-SA 4.0
