import React, { useState, useEffect } from 'react';
import { multilingualRuntime, LanguageProfile } from '../../../modules/multilingualRuntime';
import { localLLMStack, LLMModel, LLMResponse } from '../../../modules/localLLMStack';
import { privacyDashboard, PrivacySettings, CloudQuery, DeviceStatus } from '../../../modules/privacyDashboard';

interface DashboardState {
  languageProfiles: LanguageProfile[];
  localModels: LLMModel[];
  privacySettings: PrivacySettings;
  cloudQueries: CloudQuery[];
  deviceStatuses: DeviceStatus[];
}

export const DashboardWebSimple: React.FC = () => {
  const [activeTab, setActiveTab] = useState<'multilingual' | 'llm' | 'privacy'>('multilingual');
  const [dashboardState, setDashboardState] = useState<DashboardState>({
    languageProfiles: [],
    localModels: [],
    privacySettings: {
      cloudAccessEnabled: false,
      cloudAccessPerQuery: true,
      dataRetentionDays: 30,
      encryptionEnabled: true,
      auditLoggingEnabled: true,
      allowedServices: [],
      blockedServices: [],
    },
    cloudQueries: [],
    deviceStatuses: []
  });

  useEffect(() => {
    // Initialize dashboard data
    const loadData = async () => {
      try {
        const languages = multilingualRuntime.getLanguageProfiles();
        const models = localLLMStack.getLocalModels();
        const privacy = privacyDashboard.getPrivacySettings();
        const queries = privacyDashboard.getCloudQueryHistory(10);
        const devices = privacyDashboard.getDeviceStatuses();

        setDashboardState({
          languageProfiles: languages,
          localModels: models,
          privacySettings: privacy,
          cloudQueries: queries,
          deviceStatuses: devices,
        });
      } catch (error) {
        console.error('Failed to load dashboard data:', error);
      }
    };

    loadData();
  }, []);

  const containerStyle: React.CSSProperties = {
    minHeight: '100vh',
    background: 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)',
    fontFamily: "'Inter', -apple-system, BlinkMacSystemFont, sans-serif",
  };

  const navStyle: React.CSSProperties = {
    background: 'white',
    boxShadow: '0 2px 10px rgba(0,0,0,0.1)',
    padding: '1rem 2rem',
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center',
  };

  const titleStyle: React.CSSProperties = {
    margin: 0,
    color: '#333',
    fontSize: '1.5rem',
  };

  const tabsStyle: React.CSSProperties = {
    display: 'flex',
    gap: '1rem',
  };

  const getTabStyle = (isActive: boolean): React.CSSProperties => ({
    padding: '0.5rem 1rem',
    border: 'none',
    background: isActive ? '#667eea' : 'none',
    color: isActive ? 'white' : '#666',
    cursor: 'pointer',
    borderRadius: '6px',
    transition: 'all 0.2s',
  });

  const mainStyle: React.CSSProperties = {
    padding: '2rem',
    maxWidth: '1200px',
    margin: '0 auto',
  };

  const gridStyle: React.CSSProperties = {
    display: 'grid',
    gridTemplateColumns: 'repeat(auto-fit, minmax(350px, 1fr))',
    gap: '2rem',
    marginBottom: '2rem',
  };

  const sectionStyle: React.CSSProperties = {
    background: 'white',
    borderRadius: '12px',
    padding: '1.5rem',
    boxShadow: '0 4px 6px rgba(0,0,0,0.1)',
  };

  const sectionHeaderStyle: React.CSSProperties = {
    display: 'flex',
    alignItems: 'center',
    marginBottom: '1rem',
    paddingBottom: '0.5rem',
    borderBottom: '2px solid #f0f0f0',
  };

  const sectionTitleStyle: React.CSSProperties = {
    margin: 0,
    color: '#333',
    fontSize: '1.2rem',
  };

  const renderMultilingualTab = () => (
    <div style={gridStyle}>
      <div style={sectionStyle}>
        <div style={sectionHeaderStyle}>
          <h2 style={sectionTitleStyle}>Language Profiles</h2>
          <div style={{
            width: '8px',
            height: '8px',
            borderRadius: '50%',
            background: '#4ade80',
            marginLeft: 'auto',
          }} />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '0.75rem' }}>
          {dashboardState.languageProfiles.map((profile, index) => (
            <div key={index} style={{
              display: 'flex',
              alignItems: 'center',
              padding: '0.75rem',
              background: '#f8fafc',
              borderRadius: '8px',
              borderLeft: '4px solid #667eea',
            }}>
              <div style={{ flex: 1 }}>
                <strong style={{ color: '#333', display: 'block', marginBottom: '0.25rem' }}>
                  {profile.name} ({profile.code})
                </strong>
                <span style={{ color: '#666', fontSize: '0.9rem' }}>
                  Default: {profile.isDefault ? 'Yes' : 'No'}
                </span>
              </div>
              <div style={{
                width: '80px',
                height: '4px',
                background: '#e5e7eb',
                borderRadius: '2px',
                marginLeft: '1rem',
                position: 'relative',
              }}>
                <div style={{
                  height: '100%',
                  background: '#667eea',
                  borderRadius: '2px',
                  width: profile.isDefault ? '100%' : '60%',
                  transition: 'width 0.3s ease',
                }} />
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );

  const renderLLMTab = () => (
    <div style={gridStyle}>
      <div style={sectionStyle}>
        <div style={sectionHeaderStyle}>
          <h2 style={sectionTitleStyle}>Local Models</h2>
          <div style={{
            width: '8px',
            height: '8px',
            borderRadius: '50%',
            background: dashboardState.localModels.length > 0 ? '#4ade80' : '#f87171',
            marginLeft: 'auto',
          }} />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '0.75rem' }}>
          {dashboardState.localModels.map((model, index) => (
            <div key={index} style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              padding: '0.75rem',
              background: '#f8fafc',
              borderRadius: '8px',
              borderLeft: '4px solid #10b981',
            }}>
              <div style={{ flex: 1 }}>
                <strong style={{ color: '#333', display: 'block', marginBottom: '0.25rem' }}>
                  {model.name}
                </strong>
                <span style={{ color: '#666', fontSize: '0.9rem', marginRight: '1rem' }}>
                  Type: {model.type} • Tokens: {model.tokenLimit}
                </span>
              </div>
              <span style={{
                padding: '0.25rem 0.5rem',
                borderRadius: '4px',
                fontSize: '0.8rem',
                fontWeight: '500',
                background: model.type === 'local' ? '#dcfce7' : '#fef3c7',
                color: model.type === 'local' ? '#166534' : '#92400e',
              }}>
                {model.type.charAt(0).toUpperCase() + model.type.slice(1)}
              </span>
            </div>
          ))}
        </div>
      </div>
    </div>
  );

  const renderPrivacyTab = () => (
    <div style={gridStyle}>
      <div style={sectionStyle}>
        <div style={sectionHeaderStyle}>
          <h2 style={sectionTitleStyle}>Privacy Settings</h2>
          <div style={{
            width: '8px',
            height: '8px',
            borderRadius: '50%',
            background: dashboardState.privacySettings.auditLoggingEnabled ? '#4ade80' : '#fbbf24',
            marginLeft: 'auto',
          }} />
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: '1rem' }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            padding: '0.75rem',
            background: '#f8fafc',
            borderRadius: '8px',
          }}>
            <label style={{ color: '#333', fontWeight: '500', cursor: 'pointer' }}>
              Cloud Access Enabled
            </label>
            <input 
              type="checkbox" 
              checked={dashboardState.privacySettings.cloudAccessEnabled}
              style={{ margin: 0, transform: 'scale(1.2)' }}
              onChange={() => {}}
            />
          </div>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            padding: '0.75rem',
            background: '#f8fafc',
            borderRadius: '8px',
          }}>
            <label style={{ color: '#333', fontWeight: '500', cursor: 'pointer' }}>
              Per-Query Consent
            </label>
            <input 
              type="checkbox" 
              checked={dashboardState.privacySettings.cloudAccessPerQuery}
              style={{ margin: 0, transform: 'scale(1.2)' }}
              onChange={() => {}}
            />
          </div>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            padding: '0.75rem',
            background: '#f8fafc',
            borderRadius: '8px',
          }}>
            <label style={{ color: '#333', fontWeight: '500', cursor: 'pointer' }}>
              Audit Logging Enabled
            </label>
            <input 
              type="checkbox" 
              checked={dashboardState.privacySettings.auditLoggingEnabled}
              style={{ margin: 0, transform: 'scale(1.2)' }}
              onChange={() => {}}
            />
          </div>
        </div>
      </div>

      <div style={sectionStyle}>
        <div style={sectionHeaderStyle}>
          <h2 style={sectionTitleStyle}>Recent Cloud Queries</h2>
        </div>
        <div style={{ maxHeight: '300px', overflowY: 'auto' }}>
          {dashboardState.cloudQueries.map((query, index) => (
            <div key={index} style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              padding: '0.75rem',
              borderBottom: index === dashboardState.cloudQueries.length - 1 ? 'none' : '1px solid #e5e7eb',
            }}>
              <div style={{ flex: 1 }}>
                <div style={{ color: '#333', marginBottom: '0.25rem' }}>
                  {query.query}
                </div>
                <div style={{ color: '#666', fontSize: '0.8rem' }}>
                  {query.timestamp.toLocaleString()} • {query.service}
                </div>
              </div>
              <span style={{
                padding: '0.25rem 0.5rem',
                borderRadius: '4px',
                fontSize: '0.8rem',
                fontWeight: '500',
                background: query.userConsent ? '#dcfce7' : '#fecaca',
                color: query.userConsent ? '#166534' : '#991b1b',
              }}>
                {query.userConsent ? 'Approved' : 'Denied'}
              </span>
            </div>
          ))}
        </div>
      </div>

      <div style={sectionStyle}>
        <div style={sectionHeaderStyle}>
          <h2 style={sectionTitleStyle}>System Metrics</h2>
        </div>
        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fit, minmax(120px, 1fr))',
          gap: '1rem',
        }}>
          <div style={{
            textAlign: 'center',
            padding: '1rem',
            background: '#f8fafc',
            borderRadius: '8px',
            borderLeft: '4px solid #8b5cf6',
          }}>
            <span style={{
              display: 'block',
              fontSize: '1.5rem',
              fontWeight: 'bold',
              color: '#333',
              marginBottom: '0.25rem',
            }}>
              {dashboardState.deviceStatuses.length > 0 ? dashboardState.deviceStatuses[0].cloudQueriesCount : 0}
            </span>
            <span style={{ color: '#666', fontSize: '0.9rem' }}>Cloud Queries</span>
          </div>
          <div style={{
            textAlign: 'center',
            padding: '1rem',
            background: '#f8fafc',
            borderRadius: '8px',
            borderLeft: '4px solid #8b5cf6',
          }}>
            <span style={{
              display: 'block',
              fontSize: '1.5rem',
              fontWeight: 'bold',
              color: '#333',
              marginBottom: '0.25rem',
            }}>
              {dashboardState.deviceStatuses.length > 0 ? (dashboardState.deviceStatuses[0].isOnline ? 'Online' : 'Offline') : 'Unknown'}
            </span>
            <span style={{ color: '#666', fontSize: '0.9rem' }}>Status</span>
          </div>
          <div style={{
            textAlign: 'center',
            padding: '1rem',
            background: '#f8fafc',
            borderRadius: '8px',
            borderLeft: '4px solid #8b5cf6',
          }}>
            <span style={{
              display: 'block',
              fontSize: '1.5rem',
              fontWeight: 'bold',
              color: '#333',
              marginBottom: '0.25rem',
            }}>
              {dashboardState.deviceStatuses.length > 0 ? dashboardState.deviceStatuses[0].privacyMode : 'Unknown'}
            </span>
            <span style={{ color: '#666', fontSize: '0.9rem' }}>Privacy Mode</span>
          </div>
        </div>
      </div>
    </div>
  );

  return (
    <div style={containerStyle}>
      <nav style={navStyle}>
        <div>
          <h1 style={titleStyle}>EthervoxAI Dashboard</h1>
        </div>
        <div style={tabsStyle}>
          <button
            style={getTabStyle(activeTab === 'multilingual')}
            onClick={() => setActiveTab('multilingual')}
          >
            Multilingual
          </button>
          <button
            style={getTabStyle(activeTab === 'llm')}
            onClick={() => setActiveTab('llm')}
          >
            Local LLM
          </button>
          <button
            style={getTabStyle(activeTab === 'privacy')}
            onClick={() => setActiveTab('privacy')}
          >
            Privacy
          </button>
        </div>
      </nav>

      <main style={mainStyle}>
        {activeTab === 'multilingual' && renderMultilingualTab()}
        {activeTab === 'llm' && renderLLMTab()}
        {activeTab === 'privacy' && renderPrivacyTab()}
      </main>
    </div>
  );
};

export default DashboardWebSimple;
