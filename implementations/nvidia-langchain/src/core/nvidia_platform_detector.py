"""
🚀 EthervoxAI NVIDIA LangChain Implementation
Enhanced platform detection with NVIDIA-specific capabilities
"""

import torch
import subprocess
import json
from typing import Dict, List, Optional, Any
from dataclasses import dataclass
import logging
import pynvml
from packaging import version

logger = logging.getLogger(__name__)

@dataclass
class NVIDIACapabilities:
    """NVIDIA hardware and software capabilities"""
    # Hardware
    cuda_devices: List[Dict[str, Any]]
    total_gpu_memory_gb: float
    total_vram_gb: float
    nvlink_topology: Dict[str, Any]
    compute_capabilities: List[str]
    
    # Software
    cuda_version: str
    driver_version: str
    tensorrt_version: Optional[str]
    triton_available: bool
    nemo_available: bool
    riva_available: bool
    
    # Performance
    memory_bandwidth: List[float]  # GB/s per GPU
    peak_compute_tflops: List[float]  # TFLOPS per GPU
    nvlink_bandwidth: Optional[float]  # GB/s
    
    # AI/ML specific
    tensor_cores: bool
    transformer_engine: bool
    flash_attention_support: bool
    multi_instance_gpu: bool
    
    def __post_init__(self):
        """Calculate derived metrics"""
        self.total_compute_tflops = sum(self.peak_compute_tflops)
        self.gpu_count = len(self.cuda_devices)
        self.avg_memory_per_gpu = self.total_vram_gb / max(1, self.gpu_count)

class NVIDIAPlatformDetector:
    """Enhanced platform detector for NVIDIA AI stack"""
    
    def __init__(self):
        self.capabilities = None
        self._initialize_nvidia_libs()
        
    def _initialize_nvidia_libs(self):
        """Initialize NVIDIA libraries"""
        try:
            pynvml.nvmlInit()
            self.nvml_available = True
        except Exception as e:
            logger.warning(f"NVML initialization failed: {e}")
            self.nvml_available = False
            
    def detect_nvidia_capabilities(self) -> NVIDIACapabilities:
        """Comprehensive NVIDIA capability detection"""
        if self.capabilities is None:
            self.capabilities = self._detect_capabilities()
        return self.capabilities
        
    def _detect_capabilities(self) -> NVIDIACapabilities:
        """Internal capability detection"""
        logger.info("🔍 Detecting NVIDIA capabilities...")
        
        # CUDA device detection
        cuda_devices = self._detect_cuda_devices()
        
        # Memory detection
        total_gpu_memory, total_vram = self._detect_memory()
        
        # Topology detection
        nvlink_topology = self._detect_nvlink_topology()
        
        # Software versions
        cuda_version = self._get_cuda_version()
        driver_version = self._get_driver_version()
        tensorrt_version = self._get_tensorrt_version()
        
        # Service availability
        triton_available = self._check_triton_availability()
        nemo_available = self._check_nemo_availability()
        riva_available = self._check_riva_availability()
        
        # Performance metrics
        memory_bandwidth = self._measure_memory_bandwidth()
        compute_tflops = self._calculate_compute_performance()
        nvlink_bandwidth = self._measure_nvlink_bandwidth()
        
        # Compute capabilities
        compute_capabilities = [
            device["compute_capability"] for device in cuda_devices
        ]
        
        # Advanced features
        tensor_cores = self._detect_tensor_cores()
        transformer_engine = self._detect_transformer_engine()
        flash_attention = self._detect_flash_attention_support()
        multi_instance_gpu = self._detect_mig_support()
        
        return NVIDIACapabilities(
            cuda_devices=cuda_devices,
            total_gpu_memory_gb=total_gpu_memory,
            total_vram_gb=total_vram,
            nvlink_topology=nvlink_topology,
            compute_capabilities=compute_capabilities,
            cuda_version=cuda_version,
            driver_version=driver_version,
            tensorrt_version=tensorrt_version,
            triton_available=triton_available,
            nemo_available=nemo_available,
            riva_available=riva_available,
            memory_bandwidth=memory_bandwidth,
            peak_compute_tflops=compute_tflops,
            nvlink_bandwidth=nvlink_bandwidth,
            tensor_cores=tensor_cores,
            transformer_engine=transformer_engine,
            flash_attention_support=flash_attention,
            multi_instance_gpu=multi_instance_gpu
        )
        
    def _detect_cuda_devices(self) -> List[Dict[str, Any]]:
        """Detect CUDA devices and their properties"""
        devices = []
        
        if not torch.cuda.is_available():
            logger.warning("CUDA not available")
            return devices
            
        for i in range(torch.cuda.device_count()):
            try:
                device_props = torch.cuda.get_device_properties(i)
                
                device_info = {
                    "device_id": i,
                    "name": device_props.name,
                    "compute_capability": f"{device_props.major}.{device_props.minor}",
                    "total_memory_mb": device_props.total_memory // (1024 * 1024),
                    "multi_processor_count": device_props.multi_processor_count,
                    "clock_rate_khz": device_props.max_clock_rate,
                    "memory_clock_rate_khz": device_props.memory_clock_rate,
                    "memory_bus_width": device_props.memory_bus_width,
                    "l2_cache_size": device_props.l2_cache_size,
                    "max_threads_per_multiprocessor": device_props.max_threads_per_multiprocessor,
                    "warp_size": device_props.warp_size
                }
                
                # Add NVML data if available
                if self.nvml_available:
                    try:
                        handle = pynvml.nvmlDeviceGetHandleByIndex(i)
                        
                        # Temperature
                        temp = pynvml.nvmlDeviceGetTemperature(
                            handle, pynvml.NVML_TEMPERATURE_GPU
                        )
                        device_info["temperature_c"] = temp
                        
                        # Power
                        power = pynvml.nvmlDeviceGetPowerUsage(handle) / 1000.0  # Watts
                        device_info["power_usage_w"] = power
                        
                        # Utilization
                        util = pynvml.nvmlDeviceGetUtilizationRates(handle)
                        device_info["gpu_utilization_percent"] = util.gpu
                        device_info["memory_utilization_percent"] = util.memory
                        
                        # Memory info
                        mem_info = pynvml.nvmlDeviceGetMemoryInfo(handle)
                        device_info["memory_total"] = mem_info.total
                        device_info["memory_used"] = mem_info.used
                        device_info["memory_free"] = mem_info.free
                        
                    except Exception as e:
                        logger.debug(f"NVML query failed for device {i}: {e}")
                
                devices.append(device_info)
                
            except Exception as e:
                logger.error(f"Failed to detect device {i}: {e}")
                
        return devices
        
    def _detect_memory(self) -> tuple[float, float]:
        """Detect total GPU memory"""
        total_memory = 0
        total_vram = 0
        
        for i in range(torch.cuda.device_count()):
            try:
                props = torch.cuda.get_device_properties(i)
                device_memory = props.total_memory / (1024**3)  # GB
                total_memory += device_memory
                total_vram += device_memory  # Same for discrete GPUs
                
            except Exception as e:
                logger.error(f"Memory detection failed for device {i}: {e}")
                
        return total_memory, total_vram
        
    def _detect_nvlink_topology(self) -> Dict[str, Any]:
        """Detect NVLink topology between GPUs"""
        topology = {"nvlink_available": False, "connections": []}
        
        try:
            if self.nvml_available and torch.cuda.device_count() > 1:
                for i in range(torch.cuda.device_count()):
                    handle_i = pynvml.nvmlDeviceGetHandleByIndex(i)
                    
                    for j in range(i + 1, torch.cuda.device_count()):
                        handle_j = pynvml.nvmlDeviceGetHandleByIndex(j)
                        
                        try:
                            # Check NVLink connection
                            link_state = pynvml.nvmlDeviceGetNvLinkState(handle_i, j)
                            if link_state == pynvml.NVML_FEATURE_ENABLED:
                                topology["nvlink_available"] = True
                                topology["connections"].append({
                                    "device_a": i,
                                    "device_b": j,
                                    "link_active": True
                                })
                        except:
                            # NVLink not available between these devices
                            pass
                            
        except Exception as e:
            logger.debug(f"NVLink topology detection failed: {e}")
            
        return topology
        
    def _get_cuda_version(self) -> str:
        """Get CUDA version"""
        try:
            if torch.cuda.is_available():
                return torch.version.cuda
        except Exception as e:
            logger.debug(f"CUDA version detection failed: {e}")
        return "unknown"
        
    def _get_driver_version(self) -> str:
        """Get NVIDIA driver version"""
        try:
            if self.nvml_available:
                return pynvml.nvmlSystemGetDriverVersion()
        except Exception as e:
            logger.debug(f"Driver version detection failed: {e}")
            
        # Fallback to nvidia-smi
        try:
            result = subprocess.run(
                ["nvidia-smi", "--query-gpu=driver_version", "--format=csv,noheader,nounits"],
                capture_output=True, text=True, timeout=10
            )
            if result.returncode == 0:
                return result.stdout.strip().split('\n')[0]
        except Exception as e:
            logger.debug(f"nvidia-smi driver version query failed: {e}")
            
        return "unknown"
        
    def _get_tensorrt_version(self) -> Optional[str]:
        """Get TensorRT version if available"""
        try:
            import tensorrt as trt
            return trt.__version__
        except ImportError:
            logger.debug("TensorRT not available")
            return None
        except Exception as e:
            logger.debug(f"TensorRT version detection failed: {e}")
            return None
            
    def _check_triton_availability(self) -> bool:
        """Check if Triton Inference Server is available"""
        try:
            import tritonclient.grpc as tritonclient
            return True
        except ImportError:
            logger.debug("Triton client not available")
            return False
        except Exception as e:
            logger.debug(f"Triton availability check failed: {e}")
            return False
            
    def _check_nemo_availability(self) -> bool:
        """Check if NVIDIA NeMo is available"""
        try:
            import nemo
            return True
        except ImportError:
            logger.debug("NeMo not available")
            return False
        except Exception as e:
            logger.debug(f"NeMo availability check failed: {e}")
            return False
            
    def _check_riva_availability(self) -> bool:
        """Check if NVIDIA Riva is available"""
        try:
            import riva.client
            return True
        except ImportError:
            logger.debug("Riva client not available")
            return False
        except Exception as e:
            logger.debug(f"Riva availability check failed: {e}")
            return False
            
    def _measure_memory_bandwidth(self) -> List[float]:
        """Measure memory bandwidth for each GPU"""
        bandwidths = []
        
        for i in range(torch.cuda.device_count()):
            try:
                # Theoretical peak bandwidth calculation
                props = torch.cuda.get_device_properties(i)
                
                # Memory clock rate in Hz, bus width in bits
                memory_clock_hz = props.memory_clock_rate * 1000
                bus_width_bits = props.memory_bus_width
                
                # Theoretical bandwidth (accounting for DDR)
                bandwidth_bps = memory_clock_hz * bus_width_bits * 2 / 8
                bandwidth_gbps = bandwidth_bps / (1024**3)
                
                bandwidths.append(bandwidth_gbps)
                
            except Exception as e:
                logger.debug(f"Bandwidth calculation failed for device {i}: {e}")
                bandwidths.append(0.0)
                
        return bandwidths
        
    def _calculate_compute_performance(self) -> List[float]:
        """Calculate peak compute performance for each GPU"""
        performances = []
        
        for i in range(torch.cuda.device_count()):
            try:
                props = torch.cuda.get_device_properties(i)
                
                # Base clock rate in KHz
                base_clock_mhz = props.max_clock_rate / 1000
                
                # Estimate TFLOPS based on architecture
                # This is a simplified calculation
                cores = props.multi_processor_count * 64  # Approximate cores per SM
                peak_ops_per_second = cores * base_clock_mhz * 1e6 * 2  # FMA operations
                tflops = peak_ops_per_second / 1e12
                
                performances.append(tflops)
                
            except Exception as e:
                logger.debug(f"Performance calculation failed for device {i}: {e}")
                performances.append(0.0)
                
        return performances
        
    def _measure_nvlink_bandwidth(self) -> Optional[float]:
        """Measure NVLink bandwidth if available"""
        if not self.nvml_available:
            return None
            
        try:
            # Simplified NVLink bandwidth estimation
            # Real measurement would require actual data transfer tests
            total_bandwidth = 0.0
            
            for i in range(torch.cuda.device_count()):
                handle = pynvml.nvmlDeviceGetHandleByIndex(i)
                
                for link_id in range(6):  # Max 6 NVLink connections per GPU
                    try:
                        capability = pynvml.nvmlDeviceGetNvLinkCapability(
                            handle, link_id, pynvml.NVML_NVLINK_CAP_P2P_SUPPORTED
                        )
                        if capability:
                            # Estimate bandwidth per link (varies by generation)
                            total_bandwidth += 25.0  # GB/s per NVLink 2.0 link
                    except:
                        continue
                        
            return total_bandwidth if total_bandwidth > 0 else None
            
        except Exception as e:
            logger.debug(f"NVLink bandwidth measurement failed: {e}")
            return None
            
    def _detect_tensor_cores(self) -> bool:
        """Detect if Tensor Cores are available"""
        try:
            for i in range(torch.cuda.device_count()):
                props = torch.cuda.get_device_properties(i)
                # Tensor Cores available on compute capability 7.0+
                major, minor = props.major, props.minor
                if major > 7 or (major == 7 and minor >= 0):
                    return True
        except Exception as e:
            logger.debug(f"Tensor Core detection failed: {e}")
            
        return False
        
    def _detect_transformer_engine(self) -> bool:
        """Detect Transformer Engine support"""
        try:
            import transformer_engine
            return True
        except ImportError:
            return False
        except Exception as e:
            logger.debug(f"Transformer Engine detection failed: {e}")
            return False
            
    def _detect_flash_attention_support(self) -> bool:
        """Detect Flash Attention support"""
        try:
            import flash_attn
            return True
        except ImportError:
            return False
        except Exception as e:
            logger.debug(f"Flash Attention detection failed: {e}")
            return False
            
    def _detect_mig_support(self) -> bool:
        """Detect Multi-Instance GPU support"""
        if not self.nvml_available:
            return False
            
        try:
            for i in range(torch.cuda.device_count()):
                handle = pynvml.nvmlDeviceGetHandleByIndex(i)
                
                try:
                    # Check if MIG mode is supported
                    max_mig_device_count = pynvml.nvmlDeviceGetMaxMigDeviceCount(handle)
                    if max_mig_device_count > 0:
                        return True
                except:
                    continue
                    
        except Exception as e:
            logger.debug(f"MIG detection failed: {e}")
            
        return False
        
    def get_recommended_configuration(self) -> Dict[str, Any]:
        """Get recommended configuration based on detected hardware"""
        caps = self.detect_nvidia_capabilities()
        
        config = {
            "inference": {
                "batch_size": min(32, max(1, caps.gpu_count * 4)),
                "precision": "fp16" if caps.tensor_cores else "fp32",
                "use_tensorrt": caps.tensorrt_version is not None,
                "use_triton": caps.triton_available and caps.gpu_count > 1
            },
            "memory": {
                "kv_cache_dtype": "fp8" if caps.transformer_engine else "fp16",
                "attention_implementation": "flash_attention_2" if caps.flash_attention_support else "eager",
                "gradient_checkpointing": caps.total_vram_gb < 24
            },
            "parallelism": {
                "strategy": self._recommend_parallelism_strategy(caps),
                "num_gpus": caps.gpu_count,
                "use_nvlink": caps.nvlink_topology["nvlink_available"]
            },
            "optimization": {
                "enable_torch_compile": True,
                "use_tensor_cores": caps.tensor_cores,
                "use_transformer_engine": caps.transformer_engine,
                "quantization": "int8" if caps.total_vram_gb < 40 else "none"
            }
        }
        
        return config
        
    def _recommend_parallelism_strategy(self, caps: NVIDIACapabilities) -> str:
        """Recommend parallelism strategy based on hardware"""
        if caps.gpu_count == 1:
            return "none"
        elif caps.gpu_count <= 4 and caps.nvlink_topology["nvlink_available"]:
            return "tensor_parallel"
        elif caps.gpu_count > 4:
            return "pipeline_parallel"
        else:
            return "data_parallel"
            
    def log_capabilities(self):
        """Log detected capabilities"""
        caps = self.detect_nvidia_capabilities()
        
        logger.info("🔍 NVIDIA Platform Detection Results:")
        logger.info(f"  GPUs: {caps.gpu_count}")
        logger.info(f"  Total VRAM: {caps.total_vram_gb:.1f} GB")
        logger.info(f"  CUDA Version: {caps.cuda_version}")
        logger.info(f"  Driver Version: {caps.driver_version}")
        logger.info(f"  TensorRT: {'✅' if caps.tensorrt_version else '❌'}")
        logger.info(f"  Triton: {'✅' if caps.triton_available else '❌'}")
        logger.info(f"  NeMo: {'✅' if caps.nemo_available else '❌'}")
        logger.info(f"  Riva: {'✅' if caps.riva_available else '❌'}")
        logger.info(f"  Tensor Cores: {'✅' if caps.tensor_cores else '❌'}")
        logger.info(f"  NVLink: {'✅' if caps.nvlink_topology['nvlink_available'] else '❌'}")
        logger.info(f"  Total Compute: {caps.total_compute_tflops:.1f} TFLOPS")


# Global instance
nvidia_detector = NVIDIAPlatformDetector()
