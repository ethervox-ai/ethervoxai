/**
 * EthervoxAI UI Demo Server
 * 
 * Local web server that hosts both web and mobile UI demos
 * Allows users to experience the EthervoxAI dashboard interfaces
 * 
 * Cross-platform support: Windows, Linux, Raspberry Pi with optimizations
 */

const os = require('os');
const fs = require('fs');

// Platform detection
const isRaspberryPi = (() => {
  if (process.platform !== 'linux') return false;
  
  try {
    if (fs.existsSync('/proc/device-tree/model')) {
      const model = fs.readFileSync('/proc/device-tree/model', 'utf8');
      return model.toLowerCase().includes('raspberry pi');
    }
    
    const cpuInfo = fs.readFileSync('/proc/cpuinfo', 'utf8');
    return cpuInfo.includes('BCM') || cpuInfo.includes('ARM');
  } catch {
    return false;
  }
})();

// System information for optimization
const systemInfo = {
  platform: process.platform,
  arch: process.arch,
  isRaspberryPi,
  totalMemory: Math.round(os.totalmem() / 1024 / 1024), // MB
  nodeVersion: process.version,
  isLowMemory: os.totalmem() < 1024 * 1024 * 1024, // Less than 1GB
  isARM: process.arch.startsWith('arm')
};

// Apply Raspberry Pi optimizations
if (isRaspberryPi) {
  console.log('🍓 Raspberry Pi detected - applying optimizations...');
  console.log(`   Memory: ${systemInfo.totalMemory}MB total`);
  console.log(`   Architecture: ${systemInfo.arch}`);
  
  // Memory optimizations
  if (systemInfo.isLowMemory) {
    process.env.NODE_OPTIONS = '--max-old-space-size=512';
    console.log('   Applied low-memory optimizations');
  }
  
  // Set garbage collection to be more aggressive on RPi
  if (global.gc) {
    setInterval(() => {
      if (global.gc) global.gc();
    }, 30000); // GC every 30 seconds
  }
}

// Check dependencies first
try {
  require('express');
  require('cors');
} catch (error) {
  console.error('❌ Required dependencies not found!');
  console.error('Please install dependencies first:');
  console.error('  npm install express cors');
  console.error('');
  
  if (isRaspberryPi) {
    console.error('🍓 Raspberry Pi users:');
    console.error('  - Use: sudo npm install --unsafe-perm if needed');
    console.error('  - Ensure enough free space: df -h');
    console.error('  - Or run: ./launch-ui-demo-master.sh (auto-installs)');
  } else if (process.platform === 'win32') {
    console.error('Or run the master launcher: launch-ui-demo-master.bat');
  } else {
    console.error('Or run the master launcher: ./launch-ui-demo-master.sh');
  }
  
  process.exit(1);
}

const express = require('express');
const path = require('path');
const cors = require('cors');
const http = require('http');
const { Server } = require('socket.io');

class UIDemo {
  constructor() {
    this.app = express();
    this.server = http.createServer(this.app);
    this.io = new Server(this.server, {
      cors: {
        origin: "*",
        methods: ["GET", "POST"]
      }
    });
    this.port = process.env.PORT || 3000;
    
    // Store system info for demo purposes
    this.systemInfo = systemInfo;
    
    // Track active downloads
    this.activeDownloads = new Map();
    
    this.setupMiddleware();
    this.setupRoutes();
    this.setupStaticFiles();
    this.setupWebSocket();
    
    // Raspberry Pi specific setup
    if (isRaspberryPi) {
      this.setupRaspberryPiOptimizations();
    }
  }
  
  setupRaspberryPiOptimizations() {
    console.log('🔧 Configuring Raspberry Pi optimizations...');
    
    // Reduce keep-alive timeout for better memory management
    this.app.use((req, res, next) => {
      res.setTimeout(30000); // 30 second timeout
      next();
    });
    
    // Add RPi-specific headers
    this.app.use((req, res, next) => {
      res.set('X-Powered-By', 'EthervoxAI-RPi');
      next();
    });
  }

  setupMiddleware() {
    // Enable CORS for all routes
    this.app.use(cors());
    
    // Parse JSON bodies
    this.app.use(express.json());
    
    // Parse URL-encoded bodies  
    this.app.use(express.urlencoded({ extended: true }));
    
    // Logging middleware
    this.app.use((req, res, next) => {
      console.log(`${new Date().toISOString()} - ${req.method} ${req.path}`);
      next();
    });
  }

  setupStaticFiles() {
    // Serve static files from the demo directories
    this.app.use('/web', express.static(path.join(__dirname, '../web')));
    this.app.use('/mobile', express.static(path.join(__dirname, '../mobile')));
    this.app.use('/assets', express.static(path.join(__dirname, 'assets')));
    
    // Serve React build files if they exist
    const buildPath = path.join(__dirname, '../../../../../../dist/ui');
    if (fs.existsSync(buildPath)) {
      this.app.use('/dist', express.static(buildPath));
    }
  }

  setupRoutes() {
    // Main demo selector page
    this.app.get('/', (req, res) => {
      res.send(this.generateHomePage());
    });

    // Web dashboard demo
    this.app.get('/web-demo', (req, res) => {
      res.sendFile(path.join(__dirname, '../web/index.html'));
    });

    // Mobile dashboard demo (web-based simulator)
    this.app.get('/mobile-demo', (req, res) => {
      res.sendFile(path.join(__dirname, '../mobile/index.html'));
    });

    // API endpoints for demo data
    this.app.get('/api/demo-data', (req, res) => {
      res.json(this.generateDemoData());
    });

    // EthervoxAI core module endpoints
    this.app.get('/api/languages', (req, res) => {
      res.json(this.getLanguageProfiles());
    });

    this.app.get('/api/models', (req, res) => {
      res.json(this.getLocalModels());
    });

    this.app.get('/api/privacy', (req, res) => {
      res.json(this.getPrivacySettings());
    });

    this.app.post('/api/query', (req, res) => {
      const { query, language } = req.body;
      res.json(this.processQuery(query, language));
    });

    // Model management endpoints
    this.app.get('/api/models/available', (req, res) => {
      res.json(this.getAvailableModels());
    });

    this.app.get('/api/models/recommended', (req, res) => {
      res.json(this.getRecommendedModels());
    });

    this.app.post('/api/models/download', (req, res) => {
      const { modelName } = req.body;
      this.startModelDownload(modelName, res);
    });

    this.app.get('/api/models/download-status/:modelName', (req, res) => {
      const { modelName } = req.params;
      const status = this.activeDownloads.get(modelName) || { status: 'not_found' };
      res.json(status);
    });

    // 404 handler
    this.app.use((req, res) => {
      res.status(404).send(`
        <html>
          <head><title>EthervoxAI UI Demo - Not Found</title></head>
          <body style="font-family: Arial, sans-serif; text-align: center; padding: 50px;">
            <h1>🤖 Page Not Found</h1>
            <p>The requested page was not found in the EthervoxAI UI Demo.</p>
            <a href="/" style="color: #007bff; text-decoration: none;">← Back to Demo Home</a>
          </body>
        </html>
      `);
    });
  }

  generateHomePage() {
    return `
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>EthervoxAI UI Demo</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
        }
        .container {
            text-align: center;
            max-width: 800px;
            padding: 40px;
            background: rgba(255, 255, 255, 0.1);
            border-radius: 20px;
            backdrop-filter: blur(10px);
            box-shadow: 0 20px 40px rgba(0, 0, 0, 0.1);
        }
        h1 {
            font-size: 3em;
            margin-bottom: 20px;
            text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.3);
        }
        .subtitle {
            font-size: 1.2em;
            margin-bottom: 40px;
            opacity: 0.9;
        }
        .demo-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 30px;
            margin: 40px 0;
        }
        .demo-card {
            background: rgba(255, 255, 255, 0.15);
            border-radius: 15px;
            padding: 30px;
            text-decoration: none;
            color: white;
            transition: all 0.3s ease;
            border: 1px solid rgba(255, 255, 255, 0.2);
        }
        .demo-card:hover {
            transform: translateY(-5px);
            background: rgba(255, 255, 255, 0.2);
            box-shadow: 0 15px 30px rgba(0, 0, 0, 0.2);
        }
        .demo-icon {
            font-size: 3em;
            margin-bottom: 15px;
            display: block;
        }
        .demo-title {
            font-size: 1.5em;
            margin-bottom: 10px;
            font-weight: 600;
        }
        .demo-description {
            opacity: 0.8;
            line-height: 1.5;
        }
        .features {
            margin-top: 40px;
            text-align: left;
        }
        .feature-list {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-top: 20px;
        }
        .feature {
            display: flex;
            align-items: center;
            opacity: 0.9;
        }
        .feature-icon {
            margin-right: 10px;
            font-size: 1.2em;
        }
        .footer {
            margin-top: 40px;
            opacity: 0.7;
            font-size: 0.9em;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🧠 EthervoxAI UI Demo</h1>
        <p class="subtitle">Experience the Privacy-First Voice Intelligence Interface</p>
        
        <div class="demo-grid">
            <a href="/web-demo" class="demo-card">
                <span class="demo-icon">🖥️</span>
                <div class="demo-title">Web Dashboard</div>
                <div class="demo-description">
                    Experience the full desktop dashboard with all features including
                    multilingual support, privacy controls, and AI model management.
                </div>
            </a>
            
            <a href="/mobile-demo" class="demo-card">
                <span class="demo-icon">📱</span>
                <div class="demo-title">Mobile Interface</div>
                <div class="demo-description">
                    Try the mobile-optimized interface designed for touch interactions
                    and on-the-go privacy-first AI assistance.
                </div>
            </a>
        </div>

        <div class="features">
            <h3 style="text-align: center; margin-bottom: 20px;">✨ Demo Features</h3>
            <div class="feature-list">
                <div class="feature">
                    <span class="feature-icon">🌐</span>
                    <span>Multilingual Language Profiles</span>
                </div>
                <div class="feature">
                    <span class="feature-icon">🧠</span>
                    <span>Local LLM Model Management</span>
                </div>
                <div class="feature">
                    <span class="feature-icon">🔐</span>
                    <span>Privacy Dashboard & Controls</span>
                </div>
                <div class="feature">
                    <span class="feature-icon">📊</span>
                    <span>Real-time System Monitoring</span>
                </div>
                <div class="feature">
                    <span class="feature-icon">🎤</span>
                    <span>Voice Query Processing</span>
                </div>
                <div class="feature">
                    <span class="feature-icon">🏠</span>
                    <span>Smart Device Integration</span>
                </div>
            </div>
        </div>

        <div class="footer">
            <p>🚀 Running on localhost:${this.port} | Built with ❤️ for privacy-conscious users</p>
            <p style="margin-top: 10px; font-size: 0.8em; opacity: 0.6;">
                Platform: ${this.systemInfo.isRaspberryPi ? '🍓 Raspberry Pi' : '💻 ' + this.systemInfo.platform} | 
                ${this.systemInfo.arch} | 
                Node.js ${this.systemInfo.nodeVersion}
            </p>
        </div>
    </div>
    
    <script>
        // Add some interactivity
        document.querySelectorAll('.demo-card').forEach(card => {
            card.addEventListener('mouseenter', () => {
                card.style.transform = 'translateY(-8px) scale(1.02)';
            });
            
            card.addEventListener('mouseleave', () => {
                card.style.transform = 'translateY(0) scale(1)';
            });
        });
    </script>
</body>
</html>
    `;
  }

  generateDemoData() {
    const data = {
      timestamp: new Date().toISOString(),
      version: '1.0.0',
      status: 'running',
      demos: ['web', 'mobile'],
      features: [
        'Multilingual Support',
        'Local LLM Processing', 
        'Privacy Controls',
        'Real-time Updates'
      ],
      system: {
        platform: this.systemInfo.platform,
        architecture: this.systemInfo.arch,
        nodeVersion: this.systemInfo.nodeVersion,
        totalMemory: this.systemInfo.totalMemory,
        isRaspberryPi: this.systemInfo.isRaspberryPi
      }
    };
    
    if (this.systemInfo.isRaspberryPi) {
      data.system.raspberryPi = {
        optimized: true,
        memoryConstrained: this.systemInfo.isLowMemory,
        gcEnabled: !!global.gc
      };
      
      // Add RPi model if available
      try {
        if (fs.existsSync('/proc/device-tree/model')) {
          data.system.raspberryPi.model = fs.readFileSync('/proc/device-tree/model', 'utf8').replace(/\0/g, '');
        }
      } catch {}
    }
    
    return data;
  }

  getLanguageProfiles() {
    return [
      {
        id: 'en-US',
        name: 'English (US)',
        confidence: 0.95,
        status: 'active',
        features: ['speech-recognition', 'text-to-speech', 'nlp']
      },
      {
        id: 'es-LA',
        name: 'Spanish (Latin America)',
        confidence: 0.88,
        status: 'active',
        features: ['speech-recognition', 'text-to-speech']
      },
      {
        id: 'zh-CN',
        name: 'Mandarin (Simplified)',
        confidence: 0.82,
        status: 'loading',
        features: ['speech-recognition']
      }
    ];
  }

  getLocalModels() {
    return [
      {
        id: 'mistral-lite',
        name: 'Mistral Lite',
        size: '4.1GB',
        status: 'loaded',
        capabilities: ['conversation', 'qa', 'summarization'],
        performance: { speed: 'fast', accuracy: 'high' }
      },
      {
        id: 'tinyllama',
        name: 'TinyLlama',
        size: '1.2GB',
        status: 'loaded',
        capabilities: ['simple-qa', 'classification'],
        performance: { speed: 'very-fast', accuracy: 'medium' }
      },
      {
        id: 'codellama-7b',
        name: 'CodeLlama 7B',
        size: '6.8GB',
        status: 'available',
        capabilities: ['code-generation', 'code-analysis'],
        performance: { speed: 'medium', accuracy: 'very-high' }
      }
    ];
  }

  getPrivacySettings() {
    return {
      mode: 'balanced',
      cloudFallback: false,
      dataRetention: '7-days',
      auditLogging: true,
      encryptionEnabled: true,
      consentRequired: true,
      allowedDomains: [],
      blockedDomains: ['example-tracker.com'],
      lastAudit: new Date(Date.now() - 24 * 60 * 60 * 1000).toISOString()
    };
  }

  processQuery(query, language = 'en-US') {
    // Simulate AI processing
    const responses = [
      {
        text: `I understand you asked: "${query}". This is a demo response showing how EthervoxAI processes queries locally while maintaining your privacy.`,
        confidence: 0.94,
        source: 'mistral-lite',
        processingTime: Math.random() * 1000 + 200
      },
      {
        text: `Query processed in ${language}. In a real implementation, this would use local AI models to provide intelligent responses without sending data to the cloud.`,
        confidence: 0.89,
        source: 'tinyllama',
        processingTime: Math.random() * 500 + 100
      }
    ];

    return responses[Math.floor(Math.random() * responses.length)];
  }

  setupWebSocket() {
    this.io.on('connection', (socket) => {
      console.log(`🔌 Client connected: ${socket.id}`);
      
      socket.on('subscribe_download_progress', (modelName) => {
        console.log(`📡 Client ${socket.id} subscribed to download progress for ${modelName}`);
        socket.join(`download_${modelName}`);
      });
      
      socket.on('disconnect', () => {
        console.log(`🔌 Client disconnected: ${socket.id}`);
      });
    });
  }

  getAvailableModels() {
    return [
      {
        id: 'tinyllama-1.1b-chat-q4',
        name: 'TinyLlama 1.1B Chat (Q4)',
        displayName: 'TinyLlama 1.1B',
        size: '669MB',
        sizeBytes: 701685760,
        description: 'Lightweight model perfect for any system, including Raspberry Pi',
        status: 'available',
        capabilities: ['conversation', 'simple-qa'],
        performance: { speed: 'very-fast', accuracy: 'good' },
        tags: ['lightweight', 'raspberry-pi', 'beginner-friendly'],
        requirements: {
          minMemory: '1GB',
          minStorage: '1GB',
          recommendedFor: ['raspberry-pi', 'low-end-systems']
        }
      },
      {
        id: 'phi-2-2.7b-q4',
        name: 'Microsoft Phi-2 2.7B (Q4)',
        displayName: 'Microsoft Phi-2 2.7B',
        size: '1.6GB',
        sizeBytes: 1717986918,
        description: 'High-quality small model from Microsoft, optimized for ARM devices',
        status: 'available',
        capabilities: ['conversation', 'instruction-following', 'reasoning'],
        performance: { speed: 'fast', accuracy: 'very-high' },
        tags: ['microsoft', 'arm-optimized', 'efficient'],
        requirements: {
          minMemory: '3GB',
          minStorage: '2GB',
          recommendedFor: ['arm-devices', 'mid-range-systems']
        }
      },
      {
        id: 'mistral-7b-instruct-v0.1-q4',
        name: 'Mistral 7B Instruct v0.1 (Q4)',
        displayName: 'Mistral 7B Instruct',
        size: '4.1GB',
        sizeBytes: 4398046511,
        description: 'Excellent instruction following model with high-quality responses',
        status: 'available',
        capabilities: ['conversation', 'instruction-following', 'qa', 'summarization'],
        performance: { speed: 'medium', accuracy: 'excellent' },
        tags: ['instruction-tuned', 'high-quality', 'general-purpose'],
        requirements: {
          minMemory: '6GB',
          minStorage: '5GB',
          recommendedFor: ['high-end-systems', 'workstations']
        }
      },
      {
        id: 'llama2-7b-chat-q4',
        name: 'Llama 2 7B Chat (Q4)',
        displayName: 'Llama 2 7B Chat',
        size: '3.9GB',
        sizeBytes: 4181721088,
        description: 'Popular general-purpose chat model from Meta',
        status: 'available',
        capabilities: ['conversation', 'qa', 'creative-writing'],
        performance: { speed: 'medium', accuracy: 'high' },
        tags: ['meta', 'popular', 'chat-optimized'],
        requirements: {
          minMemory: '5GB',
          minStorage: '4GB',
          recommendedFor: ['mid-to-high-end-systems']
        }
      },
      {
        id: 'llama2-13b-chat-q4',
        name: 'Llama 2 13B Chat (Q4)',
        displayName: 'Llama 2 13B Chat',
        size: '7.3GB',
        sizeBytes: 7836082816,
        description: 'High-capability model for systems with sufficient memory',
        status: 'available',
        capabilities: ['conversation', 'qa', 'reasoning', 'creative-writing'],
        performance: { speed: 'slow', accuracy: 'excellent' },
        tags: ['meta', 'large-model', 'high-capability'],
        requirements: {
          minMemory: '10GB',
          minStorage: '8GB',
          recommendedFor: ['high-end-systems', 'servers']
        }
      }
    ];
  }

  getRecommendedModels() {
    const systemMemoryGB = Math.round(this.systemInfo.totalMemory / 1024);
    const models = this.getAvailableModels();
    const recommended = [];

    // Ultra performance tier (16GB+)
    if (systemMemoryGB >= 16) {
      recommended.push({
        ...models.find(m => m.id === 'llama2-13b-chat-q4'),
        reason: 'High-performance system can handle larger models',
        priority: 1
      });
    }
    
    // High/Medium performance tier (8GB+)
    if (systemMemoryGB >= 8) {
      recommended.push({
        ...models.find(m => m.id === 'mistral-7b-instruct-v0.1-q4'),
        reason: 'Excellent instruction following, efficient for your system',
        priority: 2
      });
      recommended.push({
        ...models.find(m => m.id === 'llama2-7b-chat-q4'),
        reason: 'Good balance of capability and performance',
        priority: 3
      });
    }
    
    // Medium performance tier (4GB+)
    if (systemMemoryGB >= 4) {
      recommended.push({
        ...models.find(m => m.id === 'phi-2-2.7b-q4'),
        reason: 'Microsoft\'s efficient model, great for ARM processors',
        priority: 4
      });
    }
    
    // Always include lightweight option
    recommended.push({
      ...models.find(m => m.id === 'tinyllama-1.1b-chat-q4'),
      reason: 'Lightweight option that works on any system',
      priority: 5
    });

    return recommended.sort((a, b) => a.priority - b.priority);
  }

  async startModelDownload(modelName, res) {
    if (this.activeDownloads.has(modelName)) {
      return res.status(409).json({ 
        error: 'Download already in progress',
        status: this.activeDownloads.get(modelName)
      });
    }

    const model = this.getAvailableModels().find(m => m.id === modelName);
    if (!model) {
      return res.status(404).json({ error: 'Model not found' });
    }

    // Initialize download status
    const downloadStatus = {
      modelId: modelName,
      modelName: model.displayName,
      status: 'initializing',
      progress: 0,
      downloadedBytes: 0,
      totalBytes: model.sizeBytes,
      speed: 0,
      eta: 0,
      startTime: Date.now()
    };

    this.activeDownloads.set(modelName, downloadStatus);
    
    // Start the simulated download
    this.simulateModelDownload(modelName, model);
    
    res.json({
      message: 'Download started',
      modelId: modelName,
      modelName: model.displayName,
      size: model.size
    });
  }

  async simulateModelDownload(modelId, model) {
    const downloadStatus = this.activeDownloads.get(modelId);
    const totalBytes = model.sizeBytes;
    const chunkSize = Math.floor(totalBytes / 200); // 200 updates for smooth progress
    let downloadedBytes = 0;
    const startTime = Date.now();

    downloadStatus.status = 'downloading';
    this.io.to(`download_${modelId}`).emit('download_progress', downloadStatus);

    const downloadInterval = setInterval(() => {
      // Simulate variable download speed
      const speedVariation = 0.7 + Math.random() * 0.6; // 0.7x to 1.3x speed
      const currentChunkSize = Math.floor(chunkSize * speedVariation);
      downloadedBytes = Math.min(downloadedBytes + currentChunkSize, totalBytes);

      const elapsed = (Date.now() - startTime) / 1000;
      const progress = (downloadedBytes / totalBytes) * 100;
      const speed = downloadedBytes / elapsed; // bytes per second
      const remainingBytes = totalBytes - downloadedBytes;
      const eta = remainingBytes > 0 ? remainingBytes / speed : 0;

      downloadStatus.progress = progress;
      downloadStatus.downloadedBytes = downloadedBytes;
      downloadStatus.speed = speed;
      downloadStatus.eta = eta;

      // Emit progress to subscribed clients
      this.io.to(`download_${modelId}`).emit('download_progress', downloadStatus);

      if (downloadedBytes >= totalBytes) {
        clearInterval(downloadInterval);
        downloadStatus.status = 'completed';
        downloadStatus.progress = 100;
        downloadStatus.eta = 0;
        
        this.io.to(`download_${modelId}`).emit('download_progress', downloadStatus);
        this.io.to(`download_${modelId}`).emit('download_complete', {
          modelId,
          modelName: model.displayName,
          totalTime: elapsed
        });

        // Clean up after a delay
        setTimeout(() => {
          this.activeDownloads.delete(modelId);
        }, 5000);
      }
    }, 100); // Update every 100ms for smooth progress
  }

  start() {
    this.server.listen(this.port, () => {
      console.log('🚀 EthervoxAI UI Demo Server Started');
      console.log('=====================================');
      console.log(`📍 Demo Home: http://localhost:${this.port}`);
      console.log(`🖥️  Web Demo: http://localhost:${this.port}/web-demo`);
      console.log(`📱 Mobile Demo: http://localhost:${this.port}/mobile-demo`);
      console.log('=====================================');
      console.log('Press Ctrl+C to stop the server');
    });

    // Graceful shutdown
    process.on('SIGINT', () => {
      console.log('\n🛑 Shutting down EthervoxAI UI Demo Server...');
      process.exit(0);
    });
  }
}

// Start the demo server if this file is run directly
if (require.main === module) {
  const demo = new UIDemo();
  demo.start();
}

module.exports = UIDemo;
