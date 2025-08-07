# 🧪 Cross-Language Test Framework for EthervoxAI

## Overview
This framework validates that all EthervoxAI implementations (Node.js, Python, C++, MicroPython) follow the same protocol and produce compatible results.

## Test Categories

### 1. 📊 Platform Detection Tests
Ensure all implementations detect system capabilities consistently:

```json
{
  "testName": "platform_detection_consistency",
  "description": "All implementations should detect similar system capabilities",
  "testData": {
    "expectedFields": [
      "total_memory", "available_memory", "cpu_cores", "architecture",
      "platform", "performance_tier", "max_model_size"
    ],
    "validationRules": {
      "total_memory": {"type": "number", "min": 0},
      "performance_tier": {"type": "string", "enum": ["low", "medium", "high", "ultra"]},
      "architecture": {"type": "string", "enum": ["x64", "arm64", "arm32", "riscv"]}
    }
  },
  "tolerance": {
    "memory_variance_percent": 5,
    "cpu_cores_exact": true,
    "performance_tier_exact": false
  }
}
```

### 2. 🗃️ Model Compatibility Tests
Verify model compatibility assessments are consistent:

```json
{
  "testName": "model_compatibility_consistency", 
  "description": "Model compatibility should be assessed similarly across implementations",
  "testCases": [
    {
      "model_name": "tinyllama-1.1b-q4",
      "model_size_mb": 669,
      "expected_compatibility": true,
      "expected_performance": ["fair", "good", "excellent"]
    },
    {
      "model_name": "llama2-13b-chat-q4",
      "model_size_mb": 7300,
      "min_memory_requirement": 8192,
      "platform_requirements": ["high", "ultra"]
    }
  ]
}
```

### 3. 🚀 Inference Protocol Tests
Validate inference request/response format compatibility:

```json
{
  "testName": "inference_protocol_consistency",
  "description": "Inference requests and responses should follow same format",
  "testCases": [
    {
      "request": {
        "prompt": "Hello, how are you?",
        "max_tokens": 50,
        "temperature": 0.7,
        "model_id": "tinyllama-1.1b-q4"
      },
      "expected_response_fields": [
        "text", "tokens_generated", "tokens_per_second", 
        "finished", "finish_reason", "timings"
      ]
    }
  ]
}
```

## Test Execution Framework

### Test Runner Configuration
```json
{
  "implementations": {
    "nodejs": {
      "command": "node test-runner.js",
      "working_directory": "implementations/nodejs",
      "timeout_seconds": 30
    },
    "python": {
      "command": "python test_runner.py", 
      "working_directory": "implementations/python",
      "timeout_seconds": 30
    },
    "cpp": {
      "command": "./test_runner",
      "working_directory": "implementations/cpp/build",
      "timeout_seconds": 15
    },
    "micropython": {
      "command": "micropython test_runner.py",
      "working_directory": "implementations/micropython", 
      "timeout_seconds": 60,
      "platform_filter": ["embedded", "microcontroller"]
    }
  },
  "test_environments": {
    "desktop_high_memory": {
      "memory_gb": 16,
      "cpu_cores": 8,
      "architecture": "x64",
      "expected_tier": "ultra"
    },
    "raspberry_pi_4": {
      "memory_gb": 4,
      "cpu_cores": 4, 
      "architecture": "arm64",
      "expected_tier": "medium",
      "is_raspberry_pi": true
    },
    "esp32": {
      "memory_kb": 320,
      "cpu_freq_mhz": 240,
      "architecture": "esp32",
      "expected_tier": "low",
      "platform": "micropython"
    }
  }
}
```

### Validation Rules

#### Memory Detection Tolerance
```python
def validate_memory_detection(results):
    """Validate memory detection across implementations"""
    baseline = results['nodejs']['total_memory']
    tolerance_percent = 5
    
    for impl_name, result in results.items():
        detected_memory = result['total_memory']
        variance = abs(detected_memory - baseline) / baseline * 100
        
        assert variance <= tolerance_percent, \
            f"{impl_name} memory detection variance {variance}% exceeds {tolerance_percent}%"
```

#### Performance Tier Consistency
```python
def validate_performance_tier(results):
    """Ensure performance tiers are logically consistent"""
    tier_order = ['low', 'medium', 'high', 'ultra']
    
    for impl_name, result in results.items():
        tier = result['performance_tier']
        memory = result['total_memory']
        cores = result['cpu_cores']
        
        # High memory + many cores should not result in 'low' tier
        if memory >= 8192 and cores >= 4:
            assert tier in ['medium', 'high', 'ultra'], \
                f"{impl_name} performance tier '{tier}' too low for {memory}MB/{cores} cores"
```

#### Model Compatibility Logic
```python
def validate_model_compatibility(results, model_test_case):
    """Validate model compatibility assessment logic"""
    model_size = model_test_case['model_size_mb']
    
    for impl_name, result in results.items():
        compatibility = result['model_compatibility']
        system_memory = result['available_memory']
        
        # Basic rule: model should be compatible if enough memory
        required_memory = model_size * 1.5  # 1.5x overhead
        expected_compatible = system_memory >= required_memory
        
        assert compatibility['is_compatible'] == expected_compatible, \
            f"{impl_name} compatibility mismatch for {model_size}MB model"
```

## Cross-Language Test Runner

### Command Line Interface
```bash
# Run all tests on all implementations
./run_cross_language_tests.sh

# Run specific test category
./run_cross_language_tests.sh --category platform_detection

# Run on specific implementations
./run_cross_language_tests.sh --implementations nodejs,python

# Run with specific environment simulation
./run_cross_language_tests.sh --environment raspberry_pi_4

# Generate compatibility report
./run_cross_language_tests.sh --report compatibility_report.html
```

### Test Results Format
```json
{
  "test_run": {
    "timestamp": "2025-08-06T10:30:00Z",
    "test_framework_version": "1.0.0",
    "implementations_tested": ["nodejs", "python", "cpp"],
    "environment": "desktop_high_memory"
  },
  "results": {
    "platform_detection_consistency": {
      "status": "PASSED",
      "implementation_results": {
        "nodejs": {
          "total_memory": 16384,
          "cpu_cores": 8,
          "performance_tier": "ultra",
          "execution_time_ms": 45
        },
        "python": {
          "total_memory": 16384,
          "cpu_cores": 8, 
          "performance_tier": "ultra",
          "execution_time_ms": 78
        },
        "cpp": {
          "total_memory": 16384,
          "cpu_cores": 8,
          "performance_tier": "ultra", 
          "execution_time_ms": 12
        }
      },
      "validation_results": {
        "memory_variance_check": "PASSED",
        "performance_tier_check": "PASSED",
        "architecture_check": "PASSED"
      }
    }
  },
  "summary": {
    "total_tests": 15,
    "passed": 14,
    "failed": 1,
    "skipped": 0,
    "overall_status": "MOSTLY_PASSED"
  }
}
```

## CI/CD Integration

### GitHub Actions Workflow
```yaml
name: Cross-Language Compatibility Tests

on: [push, pull_request]

jobs:
  cross-language-tests:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        environment: [desktop_high_memory, raspberry_pi_simulation]
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Setup Node.js
      uses: actions/setup-node@v3
      with:
        node-version: '18'
        
    - name: Setup Python
      uses: actions/setup-python@v4
      with:
        python-version: '3.11'
        
    - name: Setup C++ Build Environment
      run: |
        sudo apt-get update
        sudo apt-get install build-essential cmake
        
    - name: Install Dependencies
      run: |
        cd implementations/nodejs && npm install
        cd implementations/python && pip install -r requirements.txt
        cd implementations/cpp && mkdir build && cd build && cmake .. && make
        
    - name: Run Cross-Language Tests
      run: |
        ./implementations/shared/tests/run_cross_language_tests.sh \
          --environment ${{ matrix.environment }} \
          --implementations nodejs,python,cpp \
          --report test_results_${{ matrix.environment }}.json
          
    - name: Upload Test Results
      uses: actions/upload-artifact@v3
      with:
        name: test-results-${{ matrix.environment }}
        path: test_results_*.json
```

## Benefits

### ✅ **Quality Assurance**
- Ensures all implementations follow the same protocol
- Catches compatibility issues early
- Validates performance characteristics

### 🔄 **Development Workflow**
- Automated testing prevents regressions
- Clear success criteria for new implementations
- Performance benchmarking across languages

### 📊 **Documentation**
- Test results serve as living documentation
- Compatibility matrices for users
- Performance comparisons guide platform choice

This framework ensures that EthervoxAI maintains consistency and quality across all language implementations while allowing each to optimize for its target platform.
