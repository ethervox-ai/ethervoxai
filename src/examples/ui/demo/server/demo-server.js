/**
 * EthervoxAI UI Demo Server
 * 
 * Local web server that hosts both web and mobile UI demos
 * Allows users to experience the EthervoxAI dashboard interfaces
 */

// Check dependencies first
try {
  require('express');
  require('cors');
} catch (error) {
  console.error('❌ Required dependencies not found!');
  console.error('Please install dependencies first:');
  console.error('  npm install express cors');
  console.error('');
  console.error('Or run the master launcher: launch-ui-demo-master.bat');
  process.exit(1);
}

const express = require('express');
const path = require('path');
const cors = require('cors');
const fs = require('fs');

class UIDemo {
  constructor() {
    this.app = express();
    this.port = process.env.PORT || 3000;
    this.setupMiddleware();
    this.setupRoutes();
    this.setupStaticFiles();
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
    return {
      timestamp: new Date().toISOString(),
      version: '1.0.0',
      status: 'running',
      demos: ['web', 'mobile'],
      features: [
        'Multilingual Support',
        'Local LLM Processing', 
        'Privacy Controls',
        'Real-time Updates'
      ]
    };
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

  start() {
    this.app.listen(this.port, () => {
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
