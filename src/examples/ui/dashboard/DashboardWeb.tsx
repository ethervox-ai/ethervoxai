import React, { useState, useEffect } from 'react';
import { multilingualRuntime, LanguageProfile } from '../../modules/multilingualRuntime';
import { localLLMStack, LLMModel, LLMResponse } from '../../modules/localLLMStack';
import { privacyDashboard, PrivacySettings, CloudQuery, DeviceStatus } from '../../modules/privacyDashboard';

interface DashboardState {
  languageProfiles: LanguageProfile[];
  localModels: LLMModel[];
  privacySettings: PrivacySettings;
  cloudQueries: CloudQuery[];
  deviceStatuses: DeviceStatus[];
  currentQuery: string;
  isProcessing: boolean;
  lastResponse: LLMResponse | null;
}

export const DashboardWeb: React.FC = () => {
  const [state, setState] = useState<DashboardState>({
    languageProfiles: [],
    localModels: [],
    privacySettings: privacyDashboard.getPrivacySettings(),
    cloudQueries: [],
    deviceStatuses: [],
    currentQuery: '',
    isProcessing: false,
    lastResponse: null
  });

  const [activeTab, setActiveTab] = useState<'overview' | 'language' | 'llm' | 'privacy'>('overview');

  useEffect(() => {
    loadDashboardData();
  }, []);

  const loadDashboardData = () => {
    setState(prev => ({
      ...prev,
      languageProfiles: multilingualRuntime.getLanguageProfiles(),
      localModels: localLLMStack.getLocalModels(),
      privacySettings: privacyDashboard.getPrivacySettings(),
      cloudQueries: privacyDashboard.getCloudQueryHistory(10),
      deviceStatuses: privacyDashboard.getDeviceStatuses()
    }));
  };

  const handleQuerySubmit = async () => {
    if (!state.currentQuery.trim()) return;

    setState(prev => ({ ...prev, isProcessing: true }));

    try {
      const response = await localLLMStack.processQuery(
        state.currentQuery,
        state.privacySettings.cloudAccessEnabled
      );

      setState(prev => ({
        ...prev,
        lastResponse: response,
        isProcessing: false,
        currentQuery: ''
      }));

      // Refresh data
      loadDashboardData();
    } catch (error) {
      console.error('Query processing failed:', error);
      setState(prev => ({ ...prev, isProcessing: false }));
    }
  };

  const handlePrivacySettingsUpdate = (updates: Partial<PrivacySettings>) => {
    privacyDashboard.updatePrivacySettings(updates);
    loadDashboardData();
  };

  const renderOverviewTab = () => (
    <div className="overview-tab">
      <h2>🏠 EthervoxAI Dashboard</h2>
      
      <div className="stats-grid">
        <div className="stat-card">
          <h3>🌐 Languages</h3>
          <p>{state.languageProfiles.length} configured</p>
        </div>
        
        <div className="stat-card">
          <h3>🧠 Local Models</h3>
          <p>{state.localModels.length} available</p>
        </div>
        
        <div className="stat-card">
          <h3>☁️ Cloud Queries</h3>
          <p>{state.cloudQueries.length} recent</p>
        </div>
        
        <div className="stat-card">
          <h3>📱 Devices</h3>
          <p>{state.deviceStatuses.filter(d => d.isOnline).length} online</p>
        </div>
      </div>

      <div className="query-section">
        <h3>💬 Test Query</h3>
        <div className="query-input">
          <input
            type="text"
            value={state.currentQuery}
            onChange={(e) => setState(prev => ({ ...prev, currentQuery: e.target.value }))}
            placeholder="Enter a test query..."
            disabled={state.isProcessing}
          />
          <button 
            onClick={handleQuerySubmit}
            disabled={state.isProcessing || !state.currentQuery.trim()}
          >
            {state.isProcessing ? '⏳ Processing...' : '🚀 Send'}
          </button>
        </div>
        
        {state.lastResponse && (
          <div className="response-card">
            <div className="response-header">
              <span className={`source-badge ${state.lastResponse.source}`}>
                {state.lastResponse.source === 'local' ? '🏠 Local' : '☁️ External'}
              </span>
              <span className="model-info">{state.lastResponse.model}</span>
              <span className="confidence">
                {Math.round(state.lastResponse.confidence * 100)}% confidence
              </span>
            </div>
            <p className="response-text">{state.lastResponse.text}</p>
            <div className="response-meta">
              <small>Tokens used: {state.lastResponse.tokensUsed}</small>
            </div>
          </div>
        )}
      </div>
    </div>
  );

  const renderLanguageTab = () => (
    <div className="language-tab">
      <h2>🌐 Multilingual Runtime</h2>
      
      <div className="supported-languages">
        <h3>Supported Languages</h3>
        <div className="language-grid">
          {state.languageProfiles.map(profile => (
            <div key={profile.code} className={`language-card ${profile.isDefault ? 'default' : ''}`}>
              <div className="language-header">
                <span className="language-code">{profile.code}</span>
                {profile.isDefault && <span className="default-badge">Default</span>}
              </div>
              <h4>{profile.name}</h4>
              {profile.dialect && <p className="dialect">Dialect: {profile.dialect}</p>}
              <p className="vocab-count">
                Custom vocabulary: {profile.customVocabulary.length} words
              </p>
            </div>
          ))}
        </div>
      </div>

      <div className="language-settings">
        <h3>Settings</h3>
        <div className="settings-group">
          <label>
            <input type="checkbox" defaultChecked />
            Auto-detect language from speech
          </label>
          <label>
            <input type="checkbox" defaultChecked />
            Switch languages mid-conversation
          </label>
          <label>
            <input type="checkbox" />
            Enable dialect adaptation
          </label>
        </div>
      </div>
    </div>
  );

  const renderLLMTab = () => (
    <div className="llm-tab">
      <h2>🧠 Local LLM Stack</h2>
      
      <div className="models-section">
        <h3>Available Models</h3>
        <div className="models-grid">
          {state.localModels.map(model => (
            <div key={model.name} className={`model-card ${model.type}`}>
              <div className="model-header">
                <h4>{model.name}</h4>
                <span className={`type-badge ${model.type}`}>
                  {model.type === 'local' ? '🏠 Local' : '☁️ External'}
                </span>
              </div>
              <div className="model-specs">
                <p>Memory: {model.memoryLimit}MB</p>
                <p>Tokens: {model.tokenLimit}</p>
              </div>
              <div className="capabilities">
                <h5>Capabilities:</h5>
                <div className="capability-tags">
                  {model.capabilities.map(cap => (
                    <span key={cap} className="capability-tag">{cap}</span>
                  ))}
                </div>
              </div>
            </div>
          ))}
        </div>
      </div>

      <div className="routing-settings">
        <h3>Routing Configuration</h3>
        <div className="routing-rules">
          <div className="rule-item">
            <span className="rule-condition">Confidence &lt; 70%</span>
            <span className="rule-arrow">→</span>
            <span className="rule-action">External LLM</span>
          </div>
          <div className="rule-item">
            <span className="rule-condition">Unknown Intent</span>
            <span className="rule-arrow">→</span>
            <span className="rule-action">Fallback</span>
          </div>
          <div className="rule-item">
            <span className="rule-condition">Default</span>
            <span className="rule-arrow">→</span>
            <span className="rule-action">Local Model</span>
          </div>
        </div>
      </div>
    </div>
  );

  const renderPrivacyTab = () => (
    <div className="privacy-tab">
      <h2>🔐 Privacy Dashboard</h2>
      
      <div className="privacy-controls">
        <h3>Privacy Settings</h3>
        <div className="settings-group">
          <label className="toggle-setting">
            <input
              type="checkbox"
              checked={state.privacySettings.cloudAccessEnabled}
              onChange={(e) => handlePrivacySettingsUpdate({ 
                cloudAccessEnabled: e.target.checked 
              })}
            />
            <span className="toggle-slider"></span>
            Enable cloud access
          </label>
          
          <label className="toggle-setting">
            <input
              type="checkbox"
              checked={state.privacySettings.cloudAccessPerQuery}
              onChange={(e) => handlePrivacySettingsUpdate({ 
                cloudAccessPerQuery: e.target.checked 
              })}
            />
            <span className="toggle-slider"></span>
            Request consent per query
          </label>
          
          <label className="toggle-setting">
            <input
              type="checkbox"
              checked={state.privacySettings.encryptionEnabled}
              onChange={(e) => handlePrivacySettingsUpdate({ 
                encryptionEnabled: e.target.checked 
              })}
            />
            <span className="toggle-slider"></span>
            End-to-end encryption
          </label>
          
          <label className="toggle-setting">
            <input
              type="checkbox"
              checked={state.privacySettings.auditLoggingEnabled}
              onChange={(e) => handlePrivacySettingsUpdate({ 
                auditLoggingEnabled: e.target.checked 
              })}
            />
            <span className="toggle-slider"></span>
            Audit logging
          </label>
        </div>

        <div className="retention-setting">
          <label>
            Data retention period:
            <select
              value={state.privacySettings.dataRetentionDays}
              onChange={(e) => handlePrivacySettingsUpdate({ 
                dataRetentionDays: parseInt(e.target.value) 
              })}
            >
              <option value={7}>7 days</option>
              <option value={30}>30 days</option>
              <option value={90}>90 days</option>
              <option value={365}>1 year</option>
            </select>
          </label>
        </div>
      </div>

      <div className="query-history">
        <h3>Recent Cloud Queries</h3>
        {state.cloudQueries.length === 0 ? (
          <p className="no-queries">No cloud queries recorded</p>
        ) : (
          <div className="query-list">
            {state.cloudQueries.map(query => (
              <div key={query.id} className="query-item">
                <div className="query-header">
                  <span className="timestamp">
                    {query.timestamp.toLocaleString()}
                  </span>
                  <span className={`service-badge ${query.service}`}>
                    {query.service}
                  </span>
                  {query.encrypted && <span className="encrypted-badge">🔒</span>}
                </div>
                <p className="query-text">
                  {query.query.length > 100 
                    ? query.query.substring(0, 100) + '...' 
                    : query.query}
                </p>
                <div className="query-meta">
                  <span>Data shared: {query.dataShared.join(', ')}</span>
                  <span className={query.userConsent ? 'consent-yes' : 'consent-no'}>
                    {query.userConsent ? '✅ Consented' : '❌ No consent'}
                  </span>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>

      <div className="device-status">
        <h3>Device Status</h3>
        <div className="device-grid">
          {state.deviceStatuses.map(device => (
            <div key={device.deviceId} className={`device-card ${device.isOnline ? 'online' : 'offline'}`}>
              <div className="device-header">
                <h4>{device.name}</h4>
                <span className={`status-indicator ${device.isOnline ? 'online' : 'offline'}`}>
                  {device.isOnline ? '🟢 Online' : '🔴 Offline'}
                </span>
              </div>
              <div className="device-info">
                <p>Privacy mode: <span className="privacy-mode">{device.privacyMode}</span></p>
                <p>Cloud queries: {device.cloudQueriesCount}</p>
                <p>Last activity: {device.lastActivity.toLocaleString()}</p>
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );

  return (
    <div className="dashboard-web">
      <nav className="dashboard-nav">
        <div className="nav-brand">
          <h1>🎤 EthervoxAI</h1>
        </div>
        <div className="nav-tabs">
          <button 
            className={activeTab === 'overview' ? 'active' : ''}
            onClick={() => setActiveTab('overview')}
          >
            📊 Overview
          </button>
          <button 
            className={activeTab === 'language' ? 'active' : ''}
            onClick={() => setActiveTab('language')}
          >
            🌐 Languages
          </button>
          <button 
            className={activeTab === 'llm' ? 'active' : ''}
            onClick={() => setActiveTab('llm')}
          >
            🧠 LLM Stack
          </button>
          <button 
            className={activeTab === 'privacy' ? 'active' : ''}
            onClick={() => setActiveTab('privacy')}
          >
            🔐 Privacy
          </button>
        </div>
      </nav>

      <main className="dashboard-main">
        {activeTab === 'overview' && renderOverviewTab()}
        {activeTab === 'language' && renderLanguageTab()}
        {activeTab === 'llm' && renderLLMTab()}
        {activeTab === 'privacy' && renderPrivacyTab()}
      </main>

      <style jsx>{`
        .dashboard-web {
          min-height: 100vh;
          background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
          font-family: 'Inter', -apple-system, BlinkMacSystemFont, sans-serif;
        }

        .dashboard-nav {
          background: white;
          box-shadow: 0 2px 10px rgba(0,0,0,0.1);
          padding: 1rem 2rem;
          display: flex;
          justify-content: space-between;
          align-items: center;
        }

        .nav-brand h1 {
          margin: 0;
          color: #333;
          font-size: 1.5rem;
        }

        .nav-tabs {
          display: flex;
          gap: 1rem;
        }

        .nav-tabs button {
          padding: 0.5rem 1rem;
          border: none;
          background: none;
          color: #666;
          cursor: pointer;
          border-radius: 6px;
          transition: all 0.2s;
        }

        .nav-tabs button:hover {
          background: #f0f0f0;
        }

        .nav-tabs button.active {
          background: #667eea;
          color: white;
        }

        .dashboard-main {
          padding: 2rem;
          max-width: 1200px;
          margin: 0 auto;
        }

        .stats-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
          gap: 1rem;
          margin: 2rem 0;
        }

        .stat-card {
          background: white;
          padding: 1.5rem;
          border-radius: 10px;
          box-shadow: 0 4px 6px rgba(0,0,0,0.1);
          text-align: center;
        }

        .stat-card h3 {
          margin: 0 0 0.5rem 0;
          color: #333;
          font-size: 1.1rem;
        }

        .stat-card p {
          margin: 0;
          color: #666;
          font-size: 1.5rem;
          font-weight: bold;
        }

        .query-section {
          background: white;
          padding: 2rem;
          border-radius: 10px;
          box-shadow: 0 4px 6px rgba(0,0,0,0.1);
          margin-top: 2rem;
        }

        .query-input {
          display: flex;
          gap: 1rem;
          margin: 1rem 0;
        }

        .query-input input {
          flex: 1;
          padding: 0.75rem;
          border: 2px solid #e0e0e0;
          border-radius: 6px;
          font-size: 1rem;
        }

        .query-input button {
          padding: 0.75rem 1.5rem;
          background: #667eea;
          color: white;
          border: none;
          border-radius: 6px;
          cursor: pointer;
          font-size: 1rem;
          transition: background 0.2s;
        }

        .query-input button:hover:not(:disabled) {
          background: #5a6fd8;
        }

        .query-input button:disabled {
          opacity: 0.6;
          cursor: not-allowed;
        }

        .response-card {
          background: #f8f9ff;
          padding: 1.5rem;
          border-radius: 8px;
          border-left: 4px solid #667eea;
          margin-top: 1rem;
        }

        .response-header {
          display: flex;
          gap: 1rem;
          align-items: center;
          margin-bottom: 1rem;
        }

        .source-badge {
          padding: 0.25rem 0.5rem;
          border-radius: 4px;
          font-size: 0.8rem;
          font-weight: bold;
        }

        .source-badge.local {
          background: #e8f5e8;
          color: #2d5a2d;
        }

        .source-badge.external {
          background: #fff3cd;
          color: #856404;
        }

        .model-info {
          font-family: monospace;
          background: #e0e0e0;
          padding: 0.25rem 0.5rem;
          border-radius: 4px;
          font-size: 0.8rem;
        }

        .confidence {
          color: #666;
          font-size: 0.9rem;
        }

        .response-text {
          margin: 1rem 0;
          line-height: 1.6;
        }

        .response-meta {
          border-top: 1px solid #e0e0e0;
          padding-top: 0.5rem;
          color: #888;
        }

        .language-grid, .models-grid, .device-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
          gap: 1rem;
          margin: 1rem 0;
        }

        .language-card, .model-card, .device-card {
          background: white;
          padding: 1.5rem;
          border-radius: 8px;
          box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }

        .language-card.default {
          border: 2px solid #667eea;
        }

        .language-header, .model-header, .device-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 1rem;
        }

        .default-badge, .type-badge, .status-indicator {
          padding: 0.25rem 0.5rem;
          border-radius: 4px;
          font-size: 0.75rem;
          font-weight: bold;
        }

        .default-badge {
          background: #667eea;
          color: white;
        }

        .type-badge.local {
          background: #e8f5e8;
          color: #2d5a2d;
        }

        .type-badge.external {
          background: #fff3cd;
          color: #856404;
        }

        .status-indicator.online {
          color: #28a745;
        }

        .status-indicator.offline {
          color: #dc3545;
        }

        .capability-tags {
          display: flex;
          flex-wrap: wrap;
          gap: 0.5rem;
          margin-top: 0.5rem;
        }

        .capability-tag {
          background: #f0f0f0;
          padding: 0.25rem 0.5rem;
          border-radius: 4px;
          font-size: 0.75rem;
        }

        .routing-rules {
          background: white;
          padding: 1.5rem;
          border-radius: 8px;
          box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }

        .rule-item {
          display: flex;
          align-items: center;
          gap: 1rem;
          padding: 0.75rem 0;
          border-bottom: 1px solid #f0f0f0;
        }

        .rule-item:last-child {
          border-bottom: none;
        }

        .rule-condition {
          background: #f8f9ff;
          padding: 0.5rem;
          border-radius: 4px;
          font-family: monospace;
          flex: 1;
        }

        .rule-arrow {
          color: #667eea;
          font-weight: bold;
        }

        .rule-action {
          background: #e8f5e8;
          padding: 0.5rem;
          border-radius: 4px;
          color: #2d5a2d;
          flex: 1;
        }

        .privacy-controls {
          background: white;
          padding: 2rem;
          border-radius: 8px;
          box-shadow: 0 2px 4px rgba(0,0,0,0.1);
          margin-bottom: 2rem;
        }

        .settings-group {
          display: flex;
          flex-direction: column;
          gap: 1rem;
        }

        .toggle-setting {
          display: flex;
          align-items: center;
          gap: 1rem;
          cursor: pointer;
        }

        .toggle-slider {
          width: 50px;
          height: 24px;
          background: #ccc;
          border-radius: 12px;
          position: relative;
          transition: background 0.3s;
        }

        .toggle-slider::after {
          content: '';
          position: absolute;
          width: 20px;
          height: 20px;
          background: white;
          border-radius: 50%;
          top: 2px;
          left: 2px;
          transition: transform 0.3s;
        }

        input[type="checkbox"]:checked + .toggle-slider {
          background: #667eea;
        }

        input[type="checkbox"]:checked + .toggle-slider::after {
          transform: translateX(26px);
        }

        input[type="checkbox"] {
          display: none;
        }

        .retention-setting {
          margin-top: 1rem;
        }

        .retention-setting select {
          margin-left: 1rem;
          padding: 0.5rem;
          border: 1px solid #ddd;
          border-radius: 4px;
        }

        .query-history {
          background: white;
          padding: 2rem;
          border-radius: 8px;
          box-shadow: 0 2px 4px rgba(0,0,0,0.1);
          margin-bottom: 2rem;
        }

        .query-list {
          display: flex;
          flex-direction: column;
          gap: 1rem;
        }

        .query-item {
          background: #f8f9ff;
          padding: 1rem;
          border-radius: 6px;
          border-left: 4px solid #667eea;
        }

        .query-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 0.5rem;
        }

        .timestamp {
          font-size: 0.8rem;
          color: #666;
        }

        .service-badge {
          background: #e0e0e0;
          padding: 0.25rem 0.5rem;
          border-radius: 4px;
          font-size: 0.75rem;
        }

        .encrypted-badge {
          color: #28a745;
        }

        .query-meta {
          display: flex;
          justify-content: space-between;
          font-size: 0.8rem;
          color: #666;
          margin-top: 0.5rem;
        }

        .consent-yes {
          color: #28a745;
        }

        .consent-no {
          color: #dc3545;
        }

        .privacy-mode {
          font-weight: bold;
          text-transform: capitalize;
        }

        .no-queries {
          text-align: center;
          color: #666;
          font-style: italic;
          padding: 2rem;
        }

        h2, h3 {
          color: #333;
          margin-bottom: 1rem;
        }

        h2 {
          background: white;
          padding: 1rem 2rem;
          border-radius: 8px;
          box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
      `}</style>
    </div>
  );
};
