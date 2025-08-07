"""
🔍 EthervoxAI Platform Detector - Python Implementation

Detects hardware capabilities, OS features, and system constraints
to optimize AI model selection and performance parameters.

This implementation follows the EthervoxAI cross-language protocol
while leveraging Python-specific libraries and optimizations.
"""

import json
import platform
import psutil
import hashlib
import time
import logging
import asyncio
from dataclasses import dataclass, asdict
from typing import Optional, List, Dict, Any
from pathlib import Path

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

@dataclass
class SystemCapabilities:
    """System capabilities following EthervoxAI protocol"""
    # Hardware
    total_memory: int          # MB
    available_memory: int      # MB  
    cpu_cores: int
    architecture: str          # x64, arm64, etc.
    
    # Platform
    platform: str              # windows, linux, darwin, other
    is_raspberry_pi: bool
    raspberry_pi_model: Optional[str] = None
    
    # AI Capabilities
    has_gpu: bool = False
    has_neural_engine: bool = False
    has_avx2: bool = False
    has_neon: bool = False
    
    # Performance tier
    performance_tier: str = "medium"  # low, medium, high, ultra
    
    # Recommended constraints
    max_model_size: int = 0        # MB
    max_context_length: int = 0    # tokens
    recommended_threads: int = 0
    use_memory_mapping: bool = False
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary following protocol format"""
        return asdict(self)

@dataclass
class ModelCompatibility:
    """Model compatibility assessment following EthervoxAI protocol"""
    model_name: str
    is_compatible: bool
    required_memory: int
    expected_performance: str  # poor, fair, good, excellent
    optimization_flags: List[str]
    warnings: List[str]
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary following protocol format"""
        return asdict(self)

class PlatformDetector:
    """
    Python implementation of EthervoxAI Platform Detector
    
    Follows the cross-language protocol while leveraging Python libraries
    like psutil for enhanced system information gathering.
    """
    
    def __init__(self):
        self._capabilities: Optional[SystemCapabilities] = None
        self._detection_time: float = 0
        self._cache_duration: float = 60.0  # 60 seconds cache
        
    async def get_capabilities(self) -> SystemCapabilities:
        """Get system capabilities (cached after first detection)"""
        current_time = time.time()
        
        if (self._capabilities and 
            current_time - self._detection_time < self._cache_duration):
            return self._capabilities
            
        logger.info("🔍 Detecting system capabilities...")
        
        # Basic system info using psutil (more accurate than built-ins)
        memory = psutil.virtual_memory()
        
        capabilities = SystemCapabilities(
            # Hardware detection
            total_memory=int(memory.total / 1024 / 1024),
            available_memory=int(memory.available / 1024 / 1024),
            cpu_cores=psutil.cpu_count(logical=False) or psutil.cpu_count(),
            architecture=platform.machine().lower(),
            
            # Platform detection  
            platform=self._detect_platform(),
            is_raspberry_pi=self._detect_raspberry_pi(),
            raspberry_pi_model=self._get_raspberry_pi_model(),
            
            # Hardware acceleration
            has_gpu=self._detect_gpu(),
            has_neural_engine=self._detect_neural_engine(),
            has_avx2=self._detect_avx2(),
            has_neon=self._detect_neon()
        )
        
        # Calculate performance metrics
        self._calculate_performance_metrics(capabilities)
        
        self._capabilities = capabilities
        self._detection_time = current_time
        
        self._log_capabilities(capabilities)
        
        return capabilities
    
    async def check_model_compatibility(
        self,
        model_name: str,
        model_size_mb: int,
        min_memory_mb: int = 0,
        preferred_memory_mb: int = 0
    ) -> ModelCompatibility:
        """Check model compatibility with current system"""
        capabilities = await self.get_capabilities()
        
        required_memory = max(int(model_size_mb * 1.5), min_memory_mb)
        is_compatible = capabilities.available_memory >= required_memory
        
        # Determine expected performance
        performance_map = {
            "ultra": "excellent",
            "high": "good", 
            "medium": "fair",
            "low": "poor"
        }
        expected_performance = performance_map.get(capabilities.performance_tier, "fair")
        
        optimization_flags = []
        warnings = []
        
        # Add platform-specific optimizations
        if capabilities.has_avx2:
            optimization_flags.append("AVX2")
        if capabilities.has_neon:
            optimization_flags.append("NEON")
        if capabilities.use_memory_mapping:
            optimization_flags.append("MMAP")
            
        # Add warnings for potential issues
        if required_memory > capabilities.available_memory * 0.8:
            warnings.append("High memory usage - may cause system slowdown")
            
        if capabilities.is_raspberry_pi and model_size_mb > 1000:
            warnings.append("Large model on Raspberry Pi - expect slower performance")
            
        if capabilities.total_memory < 4096 and model_size_mb > 500:
            warnings.append("Limited system memory - consider smaller model variant")
            
        return ModelCompatibility(
            model_name=model_name,
            is_compatible=is_compatible,
            required_memory=required_memory,
            expected_performance=expected_performance,
            optimization_flags=optimization_flags,
            warnings=warnings
        )
    
    async def get_recommended_models(self) -> List[Dict[str, str]]:
        """Get recommended models for current system"""
        capabilities = await self.get_capabilities()
        recommendations = []
        
        if capabilities.performance_tier == "ultra" and capabilities.total_memory >= 16384:
            recommendations.append({
                "name": "llama2-13b-chat-q4",
                "size": "7.3GB", 
                "reason": "High-performance system can handle larger models"
            })
            
        if capabilities.performance_tier in ["high", "ultra"] and capabilities.total_memory >= 8192:
            recommendations.append({
                "name": "llama2-7b-chat-q4",
                "size": "3.9GB",
                "reason": "Good balance of capability and performance"
            })
            
        if capabilities.performance_tier in ["medium", "high", "ultra"] or capabilities.total_memory >= 4096:
            recommendations.append({
                "name": "mistral-7b-instruct-v0.1-q4", 
                "size": "4.1GB",
                "reason": "Excellent instruction following, efficient"
            })
            
        # Always include lightweight options
        recommendations.append({
            "name": "tinyllama-1.1b-chat-q4",
            "size": "669MB",
            "reason": "Lightweight, works on any system"
        })
        
        if capabilities.is_raspberry_pi:
            recommendations.append({
                "name": "phi-2-2.7b-q4",
                "size": "1.6GB", 
                "reason": "Optimized for ARM processors, good for RPi"
            })
            
        return recommendations
    
    # ================== Private Methods ==================
    
    def _detect_platform(self) -> str:
        """Detect platform following EthervoxAI protocol"""
        system = platform.system().lower()
        if system == "windows":
            return "windows"
        elif system == "linux":
            return "linux"
        elif system == "darwin":
            return "darwin"
        else:
            return "other"
            
    def _detect_raspberry_pi(self) -> bool:
        """Detect if running on Raspberry Pi"""
        if platform.system().lower() != "linux":
            return False
            
        try:
            # Check device tree model
            model_file = Path("/proc/device-tree/model")
            if model_file.exists():
                model = model_file.read_text(encoding="utf-8", errors="ignore")
                if "raspberry pi" in model.lower():
                    return True
                    
            # Check CPU info
            cpuinfo_file = Path("/proc/cpuinfo")
            if cpuinfo_file.exists():
                cpuinfo = cpuinfo_file.read_text()
                if "BCM" in cpuinfo or "Raspberry Pi" in cpuinfo:
                    return True
                    
        except Exception:
            pass
            
        return False
    
    def _get_raspberry_pi_model(self) -> Optional[str]:
        """Get specific Raspberry Pi model"""
        if not self._detect_raspberry_pi():
            return None
            
        try:
            model_file = Path("/proc/device-tree/model")
            if model_file.exists():
                model = model_file.read_text(encoding="utf-8", errors="ignore")
                return model.replace("\x00", "").strip()
        except Exception:
            pass
            
        return "Unknown Raspberry Pi"
    
    def _detect_gpu(self) -> bool:
        """Detect GPU presence - basic implementation"""
        try:
            import GPUtil
            gpus = GPUtil.getGPUs()
            return len(gpus) > 0
        except ImportError:
            # Fallback detection methods
            return False
    
    def _detect_neural_engine(self) -> bool:
        """Detect Apple Silicon Neural Engine"""
        if platform.system().lower() == "darwin" and platform.machine().lower() == "arm64":
            # Check if it's Apple Silicon
            try:
                import subprocess
                result = subprocess.run(
                    ["sysctl", "-n", "machdep.cpu.brand_string"],
                    capture_output=True, text=True
                )
                return "Apple" in result.stdout
            except Exception:
                pass
        return False
    
    def _detect_avx2(self) -> bool:
        """Detect x86/x64 AVX2 support"""
        arch = platform.machine().lower()
        if arch in ["x86_64", "amd64", "i386", "i686"]:
            try:
                import cpufeature
                return cpufeature.CPUFeature["AVX2"]
            except ImportError:
                # Fallback: assume modern x64 has AVX2
                return arch in ["x86_64", "amd64"]
        return False
    
    def _detect_neon(self) -> bool:
        """Detect ARM NEON support"""
        arch = platform.machine().lower()
        if "arm" in arch or "aarch64" in arch:
            # Most modern ARM processors have NEON
            return True
        return False
    
    def _calculate_performance_metrics(self, caps: SystemCapabilities) -> None:
        """Calculate performance tier and constraints"""
        # Performance scoring
        memory_score = 4 if caps.total_memory >= 16384 else \
                      3 if caps.total_memory >= 8192 else \
                      2 if caps.total_memory >= 4096 else 1
                      
        core_score = 4 if caps.cpu_cores >= 8 else \
                    3 if caps.cpu_cores >= 4 else \
                    2 if caps.cpu_cores >= 2 else 1
                    
        arch_score = 4 if caps.has_neural_engine else \
                    3 if caps.has_avx2 else \
                    2 if caps.has_neon else 1
                    
        total_score = memory_score + core_score + arch_score
        
        if total_score >= 10:
            caps.performance_tier = "ultra"
        elif total_score >= 8:
            caps.performance_tier = "high" 
        elif total_score >= 6:
            caps.performance_tier = "medium"
        else:
            caps.performance_tier = "low"
            
        # Calculate constraints
        memory_for_models = int(caps.available_memory * 0.6)
        caps.max_model_size = max(memory_for_models, 512)
        
        caps.max_context_length = min(
            int(caps.available_memory / 4),  # Rough estimate
            8192
        )
        
        caps.recommended_threads = max(
            int(caps.cpu_cores * 0.75),
            1
        )
        
        caps.use_memory_mapping = caps.total_memory >= 4096 and not caps.is_raspberry_pi
    
    def _log_capabilities(self, caps: SystemCapabilities) -> None:
        """Log detected capabilities"""
        logger.info("📊 System Capabilities Detected:")
        logger.info(f"   💾 Memory: {caps.available_memory}MB available / {caps.total_memory}MB total")
        logger.info(f"   🖥️  CPU: {caps.cpu_cores} cores ({caps.architecture})")
        logger.info(f"   🏗️  Platform: {caps.platform}" + 
                   (f" ({caps.raspberry_pi_model})" if caps.is_raspberry_pi else ""))
        logger.info(f"   ⚡ Performance Tier: {caps.performance_tier.upper()}")
        logger.info(f"   🧠 Max Model Size: {caps.max_model_size}MB")
        logger.info(f"   📝 Max Context: {caps.max_context_length} tokens")
        logger.info(f"   🧵 Recommended Threads: {caps.recommended_threads}")
        
        features = []
        if caps.has_gpu:
            features.append("GPU")
        if caps.has_neural_engine:
            features.append("Neural Engine")
        if caps.has_avx2:
            features.append("AVX2")
        if caps.has_neon:
            features.append("NEON")
            
        if features:
            logger.info(f"   🚀 Hardware Acceleration: {', '.join(features)}")

# Export singleton instance
platform_detector = PlatformDetector()
