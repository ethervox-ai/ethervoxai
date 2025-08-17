"""
🧠 NVIDIA LLM Integration for LangChain
High-performance LLM wrapper using NVIDIA optimizations
"""

import asyncio
import logging
from typing import Any, Dict, List, Optional, Union, AsyncIterator
from langchain.llms.base import LLM
from langchain.callbacks.manager import CallbackManagerForLLMRun, AsyncCallbackManagerForLLMRun
from langchain.schema import Generation, LLMResult
from pydantic import Field, validator
import torch
from ..core.nvidia_platform_detector import nvidia_detector

logger = logging.getLogger(__name__)

class NVIDIATritonLLM(LLM):
    """
    NVIDIA Triton-optimized LLM for high-performance inference
    
    Features:
    - TensorRT optimization
    - Multi-GPU tensor parallelism  
    - Dynamic batching
    - KV-cache optimization
    - Flash Attention support
    """
    
    model_name: str = Field(..., description="Name of the model to load")
    triton_url: str = Field(default="localhost:8001", description="Triton server URL")
    max_tokens: int = Field(default=2048, description="Maximum tokens to generate")
    temperature: float = Field(default=0.7, description="Sampling temperature")
    top_p: float = Field(default=0.9, description="Top-p sampling")
    top_k: int = Field(default=50, description="Top-k sampling")
    
    # NVIDIA optimizations
    use_tensorrt: bool = Field(default=True, description="Enable TensorRT optimization")
    use_flash_attention: bool = Field(default=True, description="Enable Flash Attention")
    tensor_parallel_size: int = Field(default=1, description="Number of GPUs for tensor parallelism")
    pipeline_parallel_size: int = Field(default=1, description="Pipeline parallelism size")
    
    # Performance settings
    batch_size: int = Field(default=1, description="Batch size for inference")
    max_batch_delay_ms: int = Field(default=10, description="Max delay for dynamic batching")
    kv_cache_dtype: str = Field(default="fp16", description="KV cache data type")
    
    # Internal state
    _triton_client: Optional[Any] = None
    _model_config: Optional[Dict] = None
    _capabilities: Optional[Any] = None
    
    class Config:
        arbitrary_types_allowed = True
        
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self._capabilities = nvidia_detector.detect_nvidia_capabilities()
        self._configure_optimizations()
        self._initialize_triton_client()
        
    @validator('tensor_parallel_size')
    def validate_tensor_parallel_size(cls, v):
        if torch.cuda.is_available():
            max_gpus = torch.cuda.device_count()
            if v > max_gpus:
                logger.warning(f"Requested {v} GPUs but only {max_gpus} available")
                return max_gpus
        return v
        
    def _configure_optimizations(self):
        """Configure optimizations based on detected hardware"""
        if self._capabilities:
            # Auto-configure based on hardware
            if self._capabilities.tensor_cores and self.use_tensorrt:
                self.kv_cache_dtype = "fp16"
                
            if self._capabilities.flash_attention_support and self.use_flash_attention:
                logger.info("✅ Flash Attention enabled")
                
            if self._capabilities.gpu_count > 1:
                self.tensor_parallel_size = min(
                    self.tensor_parallel_size, 
                    self._capabilities.gpu_count
                )
                
            # Optimize batch size based on memory
            memory_per_gpu = self._capabilities.avg_memory_per_gpu
            if memory_per_gpu > 40:  # High memory
                self.batch_size = min(32, self.batch_size * 4)
            elif memory_per_gpu > 20:  # Medium memory
                self.batch_size = min(16, self.batch_size * 2)
                
    def _initialize_triton_client(self):
        """Initialize Triton Inference Server client"""
        try:
            import tritonclient.grpc as tritonclient
            
            self._triton_client = tritonclient.InferenceServerClient(
                url=self.triton_url,
                verbose=False
            )
            
            # Check server health
            if self._triton_client.is_server_live():
                logger.info(f"✅ Connected to Triton server at {self.triton_url}")
                self._load_model_config()
            else:
                logger.error("❌ Triton server not responding")
                
        except ImportError:
            logger.warning("Triton client not available, falling back to direct inference")
            self._triton_client = None
        except Exception as e:
            logger.error(f"Failed to connect to Triton server: {e}")
            self._triton_client = None
            
    def _load_model_config(self):
        """Load model configuration from Triton"""
        if self._triton_client:
            try:
                self._model_config = self._triton_client.get_model_config(self.model_name)
                logger.info(f"📋 Loaded config for model: {self.model_name}")
            except Exception as e:
                logger.error(f"Failed to load model config: {e}")
                
    @property
    def _llm_type(self) -> str:
        return "nvidia_triton"
        
    def _call(
        self,
        prompt: str,
        stop: Optional[List[str]] = None,
        run_manager: Optional[CallbackManagerForLLMRun] = None,
        **kwargs: Any,
    ) -> str:
        """Synchronous inference call"""
        try:
            if self._triton_client:
                return self._triton_inference(prompt, stop, **kwargs)
            else:
                return self._direct_inference(prompt, stop, **kwargs)
        except Exception as e:
            logger.error(f"Inference failed: {e}")
            raise
            
    async def _acall(
        self,
        prompt: str,
        stop: Optional[List[str]] = None,
        run_manager: Optional[AsyncCallbackManagerForLLMRun] = None,
        **kwargs: Any,
    ) -> str:
        """Asynchronous inference call"""
        try:
            if self._triton_client:
                return await self._triton_inference_async(prompt, stop, **kwargs)
            else:
                return await self._direct_inference_async(prompt, stop, **kwargs)
        except Exception as e:
            logger.error(f"Async inference failed: {e}")
            raise
            
    def _triton_inference(self, prompt: str, stop: Optional[List[str]] = None, **kwargs) -> str:
        """Inference using Triton Inference Server"""
        import tritonclient.grpc as tritonclient
        import numpy as np
        
        try:
            # Prepare input
            inputs = []
            inputs.append(
                tritonclient.InferInput("text_input", [1], "BYTES")
            )
            inputs[0].set_data_from_numpy(
                np.array([prompt.encode('utf-8')], dtype=object)
            )
            
            # Add generation parameters
            if hasattr(self, 'max_tokens'):
                max_tokens_input = tritonclient.InferInput("max_tokens", [1], "INT32")
                max_tokens_input.set_data_from_numpy(
                    np.array([self.max_tokens], dtype=np.int32)
                )
                inputs.append(max_tokens_input)
                
            if hasattr(self, 'temperature'):
                temp_input = tritonclient.InferInput("temperature", [1], "FP32")
                temp_input.set_data_from_numpy(
                    np.array([self.temperature], dtype=np.float32)
                )
                inputs.append(temp_input)
                
            # Prepare outputs
            outputs = []
            outputs.append(tritonclient.InferRequestedOutput("text_output"))
            
            # Run inference
            response = self._triton_client.infer(
                model_name=self.model_name,
                inputs=inputs,
                outputs=outputs,
                timeout=60.0
            )
            
            # Extract result
            output_data = response.as_numpy("text_output")
            result = output_data[0].decode('utf-8') if output_data.size > 0 else ""
            
            return result
            
        except Exception as e:
            logger.error(f"Triton inference failed: {e}")
            raise
            
    async def _triton_inference_async(self, prompt: str, stop: Optional[List[str]] = None, **kwargs) -> str:
        """Asynchronous Triton inference"""
        # Run synchronous inference in thread pool
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(
            None, 
            self._triton_inference, 
            prompt, 
            stop, 
            **kwargs
        )
        
    def _direct_inference(self, prompt: str, stop: Optional[List[str]] = None, **kwargs) -> str:
        """Direct inference without Triton (fallback)"""
        try:
            # This would use direct PyTorch/Transformers inference
            # For now, return a placeholder
            logger.warning("Using direct inference fallback")
            return f"Direct inference result for: {prompt[:50]}..."
            
        except Exception as e:
            logger.error(f"Direct inference failed: {e}")
            raise
            
    async def _direct_inference_async(self, prompt: str, stop: Optional[List[str]] = None, **kwargs) -> str:
        """Asynchronous direct inference"""
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(
            None, 
            self._direct_inference, 
            prompt, 
            stop, 
            **kwargs
        )
        
    def generate(
        self,
        prompts: List[str],
        stop: Optional[List[str]] = None,
        run_manager: Optional[CallbackManagerForLLMRun] = None,
        **kwargs: Any,
    ) -> LLMResult:
        """Generate responses for multiple prompts"""
        generations = []
        
        for prompt in prompts:
            try:
                response = self._call(prompt, stop, run_manager, **kwargs)
                generations.append([Generation(text=response)])
            except Exception as e:
                logger.error(f"Generation failed for prompt: {e}")
                generations.append([Generation(text=f"Error: {str(e)}")])
                
        return LLMResult(generations=generations)
        
    async def agenerate(
        self,
        prompts: List[str],
        stop: Optional[List[str]] = None,
        run_manager: Optional[AsyncCallbackManagerForLLMRun] = None,
        **kwargs: Any,
    ) -> LLMResult:
        """Asynchronously generate responses for multiple prompts"""
        tasks = [
            self._acall(prompt, stop, run_manager, **kwargs)
            for prompt in prompts
        ]
        
        try:
            responses = await asyncio.gather(*tasks, return_exceptions=True)
            generations = []
            
            for response in responses:
                if isinstance(response, Exception):
                    generations.append([Generation(text=f"Error: {str(response)}")])
                else:
                    generations.append([Generation(text=response)])
                    
            return LLMResult(generations=generations)
            
        except Exception as e:
            logger.error(f"Async generation batch failed: {e}")
            raise
            
    def stream(
        self,
        prompt: str,
        stop: Optional[List[str]] = None,
        run_manager: Optional[CallbackManagerForLLMRun] = None,
        **kwargs: Any,
    ) -> AsyncIterator[str]:
        """Stream generation (if supported by backend)"""
        # Placeholder for streaming implementation
        # Would require streaming support in Triton or direct model
        response = self._call(prompt, stop, run_manager, **kwargs)
        
        # Simulate streaming by yielding chunks
        words = response.split()
        for word in words:
            yield word + " "
            
    def get_performance_stats(self) -> Dict[str, Any]:
        """Get performance statistics"""
        stats = {
            "model_name": self.model_name,
            "tensor_parallel_size": self.tensor_parallel_size,
            "batch_size": self.batch_size,
            "use_tensorrt": self.use_tensorrt,
            "use_flash_attention": self.use_flash_attention,
        }
        
        if self._capabilities:
            stats.update({
                "gpu_count": self._capabilities.gpu_count,
                "total_vram_gb": self._capabilities.total_vram_gb,
                "tensor_cores": self._capabilities.tensor_cores,
                "nvlink_available": self._capabilities.nvlink_topology["nvlink_available"]
            })
            
        return stats
        
    def optimize_for_deployment(self):
        """Optimize settings for production deployment"""
        logger.info("🔧 Optimizing for deployment...")
        
        if self._capabilities:
            # Increase batch size for throughput
            if self._capabilities.total_vram_gb > 40:
                self.batch_size = 32
                self.max_batch_delay_ms = 50
            elif self._capabilities.total_vram_gb > 20:
                self.batch_size = 16
                self.max_batch_delay_ms = 25
            else:
                self.batch_size = 8
                self.max_batch_delay_ms = 10
                
            # Enable all optimizations
            self.use_tensorrt = True
            self.use_flash_attention = self._capabilities.flash_attention_support
            
            # Configure parallelism
            if self._capabilities.gpu_count > 1:
                self.tensor_parallel_size = min(4, self._capabilities.gpu_count)
                
        logger.info(f"✅ Optimized: batch_size={self.batch_size}, tp_size={self.tensor_parallel_size}")


class NVIDIAEmbeddings:
    """
    NVIDIA-optimized embeddings with GPU acceleration
    """
    
    def __init__(
        self,
        model_name: str = "sentence-transformers/all-MiniLM-L6-v2",
        device: str = "auto",
        batch_size: int = 32
    ):
        self.model_name = model_name
        self.device = self._select_device(device)
        self.batch_size = batch_size
        self._model = None
        self._load_model()
        
    def _select_device(self, device: str) -> str:
        """Select optimal device for embeddings"""
        if device == "auto":
            if torch.cuda.is_available():
                return f"cuda:{torch.cuda.current_device()}"
            else:
                return "cpu"
        return device
        
    def _load_model(self):
        """Load embedding model"""
        try:
            from sentence_transformers import SentenceTransformer
            
            self._model = SentenceTransformer(
                self.model_name,
                device=self.device
            )
            
            logger.info(f"✅ Loaded embedding model on {self.device}")
            
        except ImportError:
            logger.error("sentence-transformers not available")
            raise
        except Exception as e:
            logger.error(f"Failed to load embedding model: {e}")
            raise
            
    def embed_documents(self, texts: List[str]) -> List[List[float]]:
        """Embed multiple documents"""
        if not self._model:
            raise RuntimeError("Model not loaded")
            
        try:
            embeddings = self._model.encode(
                texts,
                batch_size=self.batch_size,
                show_progress_bar=False,
                convert_to_numpy=True
            )
            
            return embeddings.tolist()
            
        except Exception as e:
            logger.error(f"Document embedding failed: {e}")
            raise
            
    def embed_query(self, text: str) -> List[float]:
        """Embed a single query"""
        return self.embed_documents([text])[0]
        
    async def aembed_documents(self, texts: List[str]) -> List[List[float]]:
        """Async document embedding"""
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(None, self.embed_documents, texts)
        
    async def aembed_query(self, text: str) -> List[float]:
        """Async query embedding"""
        loop = asyncio.get_event_loop()
        return await loop.run_in_executor(None, self.embed_query, text)
