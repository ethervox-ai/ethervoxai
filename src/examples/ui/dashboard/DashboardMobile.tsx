import React, { useState, useEffect } from 'react';
import { View, Text, ScrollView, TouchableOpacity, Switch, TextInput, StyleSheet } from 'react-native';
import { multilingualRuntime, LanguageProfile } from '../../../modules/multilingualRuntime';
import { localLLMStack, LLMModel, LLMResponse } from '../../../modules/localLLMStack';
import { privacyDashboard, PrivacySettings, CloudQuery, DeviceStatus } from '../../../modules/privacyDashboard';

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

export const DashboardMobile: React.FC = () => {
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
    <ScrollView style={styles.tabContent}>
      <Text style={styles.tabTitle}>🏠 EthervoxAI Dashboard</Text>
      
      <View style={styles.statsGrid}>
        <View style={styles.statCard}>
          <Text style={styles.statIcon}>🌐</Text>
          <Text style={styles.statTitle}>Languages</Text>
          <Text style={styles.statValue}>{state.languageProfiles.length}</Text>
        </View>
        
        <View style={styles.statCard}>
          <Text style={styles.statIcon}>🧠</Text>
          <Text style={styles.statTitle}>Local Models</Text>
          <Text style={styles.statValue}>{state.localModels.length}</Text>
        </View>
        
        <View style={styles.statCard}>
          <Text style={styles.statIcon}>☁️</Text>
          <Text style={styles.statTitle}>Cloud Queries</Text>
          <Text style={styles.statValue}>{state.cloudQueries.length}</Text>
        </View>
        
        <View style={styles.statCard}>
          <Text style={styles.statIcon}>📱</Text>
          <Text style={styles.statTitle}>Devices Online</Text>
          <Text style={styles.statValue}>
            {state.deviceStatuses.filter(d => d.isOnline).length}
          </Text>
        </View>
      </View>

      <View style={styles.querySection}>
        <Text style={styles.sectionTitle}>💬 Test Query</Text>
        <View style={styles.queryInput}>
          <TextInput
            style={styles.textInput}
            value={state.currentQuery}
            onChangeText={(text) => setState(prev => ({ ...prev, currentQuery: text }))}
            placeholder="Enter a test query..."
            editable={!state.isProcessing}
            multiline
          />
          <TouchableOpacity 
            style={[styles.sendButton, (!state.currentQuery.trim() || state.isProcessing) && styles.sendButtonDisabled]}
            onPress={handleQuerySubmit}
            disabled={state.isProcessing || !state.currentQuery.trim()}
          >
            <Text style={styles.sendButtonText}>
              {state.isProcessing ? '⏳ Processing...' : '🚀 Send'}
            </Text>
          </TouchableOpacity>
        </View>
        
        {state.lastResponse && (
          <View style={styles.responseCard}>
            <View style={styles.responseHeader}>
              <View style={[styles.sourceBadge, 
                state.lastResponse.source === 'local' ? styles.sourceBadgeLocal : styles.sourceBadgeExternal
              ]}>
                <Text style={styles.sourceBadgeText}>
                  {state.lastResponse.source === 'local' ? '🏠 Local' : '☁️ External'}
                </Text>
              </View>
              <Text style={styles.modelInfo}>{state.lastResponse.model}</Text>
              <Text style={styles.confidence}>
                {Math.round(state.lastResponse.confidence * 100)}% confidence
              </Text>
            </View>
            <Text style={styles.responseText}>{state.lastResponse.text}</Text>
            <View style={styles.responseMeta}>
              <Text style={styles.responseMetaText}>
                Tokens used: {state.lastResponse.tokensUsed}
              </Text>
            </View>
          </View>
        )}
      </View>
    </ScrollView>
  );

  const renderLanguageTab = () => (
    <ScrollView style={styles.tabContent}>
      <Text style={styles.tabTitle}>🌐 Multilingual Runtime</Text>
      
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Supported Languages</Text>
        {state.languageProfiles.map(profile => (
          <View key={profile.code} style={[styles.languageCard, profile.isDefault && styles.languageCardDefault]}>
            <View style={styles.languageHeader}>
              <Text style={styles.languageCode}>{profile.code}</Text>
              {profile.isDefault && (
                <View style={styles.defaultBadge}>
                  <Text style={styles.defaultBadgeText}>Default</Text>
                </View>
              )}
            </View>
            <Text style={styles.languageName}>{profile.name}</Text>
            {profile.dialect && (
              <Text style={styles.languageDialect}>Dialect: {profile.dialect}</Text>
            )}
            <Text style={styles.vocabularyCount}>
              Custom vocabulary: {profile.customVocabulary.length} words
            </Text>
          </View>
        ))}
      </View>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Settings</Text>
        <View style={styles.settingsGroup}>
          <View style={styles.settingItem}>
            <Text style={styles.settingText}>Auto-detect language from speech</Text>
            <Switch value={true} />
          </View>
          <View style={styles.settingItem}>
            <Text style={styles.settingText}>Switch languages mid-conversation</Text>
            <Switch value={true} />
          </View>
          <View style={styles.settingItem}>
            <Text style={styles.settingText}>Enable dialect adaptation</Text>
            <Switch value={false} />
          </View>
        </View>
      </View>
    </ScrollView>
  );

  const renderLLMTab = () => (
    <ScrollView style={styles.tabContent}>
      <Text style={styles.tabTitle}>🧠 Local LLM Stack</Text>
      
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Available Models</Text>
        {state.localModels.map(model => (
          <View key={model.name} style={styles.modelCard}>
            <View style={styles.modelHeader}>
              <Text style={styles.modelName}>{model.name}</Text>
              <View style={[styles.typeBadge, 
                model.type === 'local' ? styles.typeBadgeLocal : styles.typeBadgeExternal
              ]}>
                <Text style={styles.typeBadgeText}>
                  {model.type === 'local' ? '🏠 Local' : '☁️ External'}
                </Text>
              </View>
            </View>
            <View style={styles.modelSpecs}>
              <Text style={styles.specText}>Memory: {model.memoryLimit}MB</Text>
              <Text style={styles.specText}>Tokens: {model.tokenLimit}</Text>
            </View>
            <Text style={styles.capabilitiesTitle}>Capabilities:</Text>
            <View style={styles.capabilityTags}>
              {model.capabilities.map(cap => (
                <View key={cap} style={styles.capabilityTag}>
                  <Text style={styles.capabilityText}>{cap}</Text>
                </View>
              ))}
            </View>
          </View>
        ))}
      </View>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Routing Configuration</Text>
        <View style={styles.routingRules}>
          <View style={styles.ruleItem}>
            <Text style={styles.ruleCondition}>Confidence &lt; 70%</Text>
            <Text style={styles.ruleArrow}>→</Text>
            <Text style={styles.ruleAction}>External LLM</Text>
          </View>
          <View style={styles.ruleItem}>
            <Text style={styles.ruleCondition}>Unknown Intent</Text>
            <Text style={styles.ruleArrow}>→</Text>
            <Text style={styles.ruleAction}>Fallback</Text>
          </View>
          <View style={styles.ruleItem}>
            <Text style={styles.ruleCondition}>Default</Text>
            <Text style={styles.ruleArrow}>→</Text>
            <Text style={styles.ruleAction}>Local Model</Text>
          </View>
        </View>
      </View>
    </ScrollView>
  );

  const renderPrivacyTab = () => (
    <ScrollView style={styles.tabContent}>
      <Text style={styles.tabTitle}>🔐 Privacy Dashboard</Text>
      
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Privacy Settings</Text>
        <View style={styles.settingsGroup}>
          <View style={styles.settingItem}>
            <Text style={styles.settingText}>Enable cloud access</Text>
            <Switch
              value={state.privacySettings.cloudAccessEnabled}
              onValueChange={(value) => handlePrivacySettingsUpdate({ 
                cloudAccessEnabled: value 
              })}
            />
          </View>
          
          <View style={styles.settingItem}>
            <Text style={styles.settingText}>Request consent per query</Text>
            <Switch
              value={state.privacySettings.cloudAccessPerQuery}
              onValueChange={(value) => handlePrivacySettingsUpdate({ 
                cloudAccessPerQuery: value 
              })}
            />
          </View>
          
          <View style={styles.settingItem}>
            <Text style={styles.settingText}>End-to-end encryption</Text>
            <Switch
              value={state.privacySettings.encryptionEnabled}
              onValueChange={(value) => handlePrivacySettingsUpdate({ 
                encryptionEnabled: value 
              })}
            />
          </View>
          
          <View style={styles.settingItem}>
            <Text style={styles.settingText}>Audit logging</Text>
            <Switch
              value={state.privacySettings.auditLoggingEnabled}
              onValueChange={(value) => handlePrivacySettingsUpdate({ 
                auditLoggingEnabled: value 
              })}
            />
          </View>
        </View>

        <View style={styles.retentionSetting}>
          <Text style={styles.settingText}>
            Data retention: {state.privacySettings.dataRetentionDays} days
          </Text>
        </View>
      </View>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Recent Cloud Queries</Text>
        {state.cloudQueries.length === 0 ? (
          <Text style={styles.noQueries}>No cloud queries recorded</Text>
        ) : (
          <View style={styles.queryList}>
            {state.cloudQueries.map(query => (
              <View key={query.id} style={styles.queryItem}>
                <View style={styles.queryItemHeader}>
                  <Text style={styles.timestamp}>
                    {query.timestamp.toLocaleString()}
                  </Text>
                  <View style={styles.serviceBadge}>
                    <Text style={styles.serviceBadgeText}>{query.service}</Text>
                  </View>
                  {query.encrypted && <Text style={styles.encryptedBadge}>🔒</Text>}
                </View>
                <Text style={styles.queryText} numberOfLines={3}>
                  {query.query.length > 100 
                    ? query.query.substring(0, 100) + '...' 
                    : query.query}
                </Text>
                <View style={styles.queryMeta}>
                  <Text style={styles.queryMetaText}>
                    Data shared: {query.dataShared.join(', ')}
                  </Text>
                  <Text style={[styles.queryMetaText, 
                    query.userConsent ? styles.consentYes : styles.consentNo
                  ]}>
                    {query.userConsent ? '✅ Consented' : '❌ No consent'}
                  </Text>
                </View>
              </View>
            ))}
          </View>
        )}
      </View>

      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Device Status</Text>
        {state.deviceStatuses.map(device => (
          <View key={device.deviceId} style={styles.deviceCard}>
            <View style={styles.deviceHeader}>
              <Text style={styles.deviceName}>{device.name}</Text>
              <Text style={[styles.statusIndicator, 
                device.isOnline ? styles.statusOnline : styles.statusOffline
              ]}>
                {device.isOnline ? '🟢 Online' : '🔴 Offline'}
              </Text>
            </View>
            <View style={styles.deviceInfo}>
              <Text style={styles.deviceInfoText}>
                Privacy mode: <Text style={styles.privacyMode}>{device.privacyMode}</Text>
              </Text>
              <Text style={styles.deviceInfoText}>Cloud queries: {device.cloudQueriesCount}</Text>
              <Text style={styles.deviceInfoText}>
                Last activity: {device.lastActivity.toLocaleString()}
              </Text>
            </View>
          </View>
        ))}
      </View>
    </ScrollView>
  );

  return (
    <View style={styles.container}>
      <View style={styles.header}>
        <Text style={styles.headerTitle}>🎤 EthervoxAI</Text>
      </View>

      <View style={styles.tabBar}>
        <TouchableOpacity 
          style={[styles.tabButton, activeTab === 'overview' && styles.tabButtonActive]}
          onPress={() => setActiveTab('overview')}
        >
          <Text style={[styles.tabButtonText, activeTab === 'overview' && styles.tabButtonTextActive]}>
            📊 Overview
          </Text>
        </TouchableOpacity>
        <TouchableOpacity 
          style={[styles.tabButton, activeTab === 'language' && styles.tabButtonActive]}
          onPress={() => setActiveTab('language')}
        >
          <Text style={[styles.tabButtonText, activeTab === 'language' && styles.tabButtonTextActive]}>
            🌐 Languages
          </Text>
        </TouchableOpacity>
        <TouchableOpacity 
          style={[styles.tabButton, activeTab === 'llm' && styles.tabButtonActive]}
          onPress={() => setActiveTab('llm')}
        >
          <Text style={[styles.tabButtonText, activeTab === 'llm' && styles.tabButtonTextActive]}>
            🧠 LLM
          </Text>
        </TouchableOpacity>
        <TouchableOpacity 
          style={[styles.tabButton, activeTab === 'privacy' && styles.tabButtonActive]}
          onPress={() => setActiveTab('privacy')}
        >
          <Text style={[styles.tabButtonText, activeTab === 'privacy' && styles.tabButtonTextActive]}>
            🔐 Privacy
          </Text>
        </TouchableOpacity>
      </View>

      <View style={styles.content}>
        {activeTab === 'overview' && renderOverviewTab()}
        {activeTab === 'language' && renderLanguageTab()}
        {activeTab === 'llm' && renderLLMTab()}
        {activeTab === 'privacy' && renderPrivacyTab()}
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#f5f5f5',
  },
  header: {
    backgroundColor: '#667eea',
    paddingTop: 50,
    paddingBottom: 20,
    paddingHorizontal: 20,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 5,
  },
  headerTitle: {
    fontSize: 24,
    fontWeight: 'bold',
    color: 'white',
    textAlign: 'center',
  },
  tabBar: {
    flexDirection: 'row',
    backgroundColor: 'white',
    paddingVertical: 10,
    paddingHorizontal: 5,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1,
    shadowRadius: 2,
    elevation: 3,
  },
  tabButton: {
    flex: 1,
    paddingVertical: 8,
    paddingHorizontal: 4,
    borderRadius: 6,
    marginHorizontal: 2,
  },
  tabButtonActive: {
    backgroundColor: '#667eea',
  },
  tabButtonText: {
    fontSize: 10,
    textAlign: 'center',
    color: '#666',
    fontWeight: '500',
  },
  tabButtonTextActive: {
    color: 'white',
  },
  content: {
    flex: 1,
  },
  tabContent: {
    flex: 1,
    padding: 16,
  },
  tabTitle: {
    fontSize: 24,
    fontWeight: 'bold',
    color: '#333',
    marginBottom: 20,
    textAlign: 'center',
    backgroundColor: 'white',
    padding: 16,
    borderRadius: 8,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  statsGrid: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'space-between',
    marginBottom: 20,
  },
  statCard: {
    width: '48%',
    backgroundColor: 'white',
    padding: 16,
    borderRadius: 8,
    alignItems: 'center',
    marginBottom: 10,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  statIcon: {
    fontSize: 24,
    marginBottom: 8,
  },
  statTitle: {
    fontSize: 12,
    color: '#666',
    marginBottom: 4,
    textAlign: 'center',
  },
  statValue: {
    fontSize: 20,
    fontWeight: 'bold',
    color: '#333',
  },
  querySection: {
    backgroundColor: 'white',
    padding: 16,
    borderRadius: 8,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  sectionTitle: {
    fontSize: 18,
    fontWeight: 'bold',
    color: '#333',
    marginBottom: 16,
  },
  queryInput: {
    marginBottom: 16,
  },
  textInput: {
    borderWidth: 1,
    borderColor: '#ddd',
    borderRadius: 6,
    padding: 12,
    fontSize: 16,
    minHeight: 80,
    textAlignVertical: 'top',
    marginBottom: 10,
  },
  sendButton: {
    backgroundColor: '#667eea',
    padding: 12,
    borderRadius: 6,
    alignItems: 'center',
  },
  sendButtonDisabled: {
    opacity: 0.6,
  },
  sendButtonText: {
    color: 'white',
    fontSize: 16,
    fontWeight: '600',
  },
  responseCard: {
    backgroundColor: '#f8f9ff',
    padding: 12,
    borderRadius: 6,
    borderLeftWidth: 4,
    borderLeftColor: '#667eea',
  },
  responseHeader: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    alignItems: 'center',
    marginBottom: 12,
    gap: 8,
  },
  sourceBadge: {
    paddingHorizontal: 8,
    paddingVertical: 4,
    borderRadius: 4,
  },
  sourceBadgeLocal: {
    backgroundColor: '#e8f5e8',
  },
  sourceBadgeExternal: {
    backgroundColor: '#fff3cd',
  },
  sourceBadgeText: {
    fontSize: 12,
    fontWeight: 'bold',
  },
  modelInfo: {
    fontFamily: 'monospace',
    backgroundColor: '#e0e0e0',
    paddingHorizontal: 8,
    paddingVertical: 4,
    borderRadius: 4,
    fontSize: 12,
  },
  confidence: {
    color: '#666',
    fontSize: 14,
  },
  responseText: {
    fontSize: 16,
    lineHeight: 24,
    marginBottom: 12,
  },
  responseMeta: {
    borderTopWidth: 1,
    borderTopColor: '#e0e0e0',
    paddingTop: 8,
  },
  responseMetaText: {
    color: '#888',
    fontSize: 12,
  },
  section: {
    backgroundColor: 'white',
    padding: 16,
    borderRadius: 8,
    marginBottom: 16,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
  },
  languageCard: {
    backgroundColor: '#f8f9ff',
    padding: 12,
    borderRadius: 6,
    marginBottom: 12,
  },
  languageCardDefault: {
    borderWidth: 2,
    borderColor: '#667eea',
  },
  languageHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  languageCode: {
    fontFamily: 'monospace',
    fontSize: 14,
    color: '#666',
  },
  defaultBadge: {
    backgroundColor: '#667eea',
    paddingHorizontal: 8,
    paddingVertical: 2,
    borderRadius: 4,
  },
  defaultBadgeText: {
    color: 'white',
    fontSize: 10,
    fontWeight: 'bold',
  },
  languageName: {
    fontSize: 16,
    fontWeight: '600',
    color: '#333',
    marginBottom: 4,
  },
  languageDialect: {
    fontSize: 14,
    color: '#666',
    marginBottom: 4,
  },
  vocabularyCount: {
    fontSize: 12,
    color: '#888',
  },
  settingsGroup: {
    gap: 16,
  },
  settingItem: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: 8,
  },
  settingText: {
    fontSize: 16,
    color: '#333',
    flex: 1,
  },
  modelCard: {
    backgroundColor: '#f8f9ff',
    padding: 12,
    borderRadius: 6,
    marginBottom: 12,
  },
  modelHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  modelName: {
    fontSize: 16,
    fontWeight: '600',
    color: '#333',
  },
  typeBadge: {
    paddingHorizontal: 8,
    paddingVertical: 4,
    borderRadius: 4,
  },
  typeBadgeLocal: {
    backgroundColor: '#e8f5e8',
  },
  typeBadgeExternal: {
    backgroundColor: '#fff3cd',
  },
  typeBadgeText: {
    fontSize: 12,
    fontWeight: 'bold',
  },
  modelSpecs: {
    flexDirection: 'row',
    gap: 16,
    marginBottom: 8,
  },
  specText: {
    fontSize: 14,
    color: '#666',
  },
  capabilitiesTitle: {
    fontSize: 14,
    fontWeight: '600',
    color: '#333',
    marginBottom: 8,
  },
  capabilityTags: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 8,
  },
  capabilityTag: {
    backgroundColor: '#f0f0f0',
    paddingHorizontal: 8,
    paddingVertical: 4,
    borderRadius: 4,
  },
  capabilityText: {
    fontSize: 12,
    color: '#666',
  },
  routingRules: {
    gap: 12,
  },
  ruleItem: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
    paddingVertical: 8,
    borderBottomWidth: 1,
    borderBottomColor: '#f0f0f0',
  },
  ruleCondition: {
    flex: 1,
    backgroundColor: '#f8f9ff',
    padding: 8,
    borderRadius: 4,
    fontFamily: 'monospace',
    fontSize: 12,
  },
  ruleArrow: {
    color: '#667eea',
    fontWeight: 'bold',
    fontSize: 16,
  },
  ruleAction: {
    flex: 1,
    backgroundColor: '#e8f5e8',
    padding: 8,
    borderRadius: 4,
    fontSize: 12,
    textAlign: 'center',
  },
  retentionSetting: {
    marginTop: 16,
    paddingTop: 16,
    borderTopWidth: 1,
    borderTopColor: '#f0f0f0',
  },
  noQueries: {
    textAlign: 'center',
    color: '#666',
    fontStyle: 'italic',
    padding: 32,
  },
  queryList: {
    gap: 12,
  },
  queryItem: {
    backgroundColor: '#f8f9ff',
    padding: 12,
    borderRadius: 6,
    borderLeftWidth: 4,
    borderLeftColor: '#667eea',
  },
  queryItemHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  timestamp: {
    fontSize: 12,
    color: '#666',
  },
  serviceBadge: {
    backgroundColor: '#e0e0e0',
    paddingHorizontal: 8,
    paddingVertical: 2,
    borderRadius: 4,
  },
  serviceBadgeText: {
    fontSize: 10,
    color: '#333',
  },
  encryptedBadge: {
    color: '#28a745',
    fontSize: 14,
  },
  queryText: {
    fontSize: 14,
    lineHeight: 20,
    marginBottom: 8,
    color: '#333',
  },
  queryMeta: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    paddingTop: 8,
    borderTopWidth: 1,
    borderTopColor: '#e0e0e0',
  },
  queryMetaText: {
    fontSize: 12,
    color: '#666',
  },
  consentYes: {
    color: '#28a745',
  },
  consentNo: {
    color: '#dc3545',
  },
  deviceCard: {
    backgroundColor: '#f8f9ff',
    padding: 12,
    borderRadius: 6,
    marginBottom: 12,
  },
  deviceHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  deviceName: {
    fontSize: 16,
    fontWeight: '600',
    color: '#333',
  },
  statusIndicator: {
    fontSize: 12,
    fontWeight: '600',
  },
  statusOnline: {
    color: '#28a745',
  },
  statusOffline: {
    color: '#dc3545',
  },
  deviceInfo: {
    gap: 4,
  },
  deviceInfoText: {
    fontSize: 14,
    color: '#666',
  },
  privacyMode: {
    fontWeight: 'bold',
    textTransform: 'capitalize',
  },
});
