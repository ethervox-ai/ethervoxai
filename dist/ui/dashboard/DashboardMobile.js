"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.DashboardMobile = void 0;
const react_1 = __importStar(require("react"));
const react_native_1 = require("react-native");
const multilingualRuntime_1 = require("../../modules/multilingualRuntime");
const localLLMStack_1 = require("../../modules/localLLMStack");
const privacyDashboard_1 = require("../../modules/privacyDashboard");
const DashboardMobile = () => {
    const [state, setState] = (0, react_1.useState)({
        languageProfiles: [],
        localModels: [],
        privacySettings: privacyDashboard_1.privacyDashboard.getPrivacySettings(),
        cloudQueries: [],
        deviceStatuses: [],
        currentQuery: '',
        isProcessing: false,
        lastResponse: null
    });
    const [activeTab, setActiveTab] = (0, react_1.useState)('overview');
    (0, react_1.useEffect)(() => {
        loadDashboardData();
    }, []);
    const loadDashboardData = () => {
        setState(prev => ({
            ...prev,
            languageProfiles: multilingualRuntime_1.multilingualRuntime.getLanguageProfiles(),
            localModels: localLLMStack_1.localLLMStack.getLocalModels(),
            privacySettings: privacyDashboard_1.privacyDashboard.getPrivacySettings(),
            cloudQueries: privacyDashboard_1.privacyDashboard.getCloudQueryHistory(10),
            deviceStatuses: privacyDashboard_1.privacyDashboard.getDeviceStatuses()
        }));
    };
    const handleQuerySubmit = async () => {
        if (!state.currentQuery.trim())
            return;
        setState(prev => ({ ...prev, isProcessing: true }));
        try {
            const response = await localLLMStack_1.localLLMStack.processQuery(state.currentQuery, state.privacySettings.cloudAccessEnabled);
            setState(prev => ({
                ...prev,
                lastResponse: response,
                isProcessing: false,
                currentQuery: ''
            }));
            loadDashboardData();
        }
        catch (error) {
            console.error('Query processing failed:', error);
            setState(prev => ({ ...prev, isProcessing: false }));
        }
    };
    const handlePrivacySettingsUpdate = (updates) => {
        privacyDashboard_1.privacyDashboard.updatePrivacySettings(updates);
        loadDashboardData();
    };
    const renderOverviewTab = () => (<react_native_1.ScrollView style={styles.tabContent}>
      <react_native_1.Text style={styles.tabTitle}>🏠 EthervoxAI Dashboard</react_native_1.Text>
      
      <react_native_1.View style={styles.statsGrid}>
        <react_native_1.View style={styles.statCard}>
          <react_native_1.Text style={styles.statIcon}>🌐</react_native_1.Text>
          <react_native_1.Text style={styles.statTitle}>Languages</react_native_1.Text>
          <react_native_1.Text style={styles.statValue}>{state.languageProfiles.length}</react_native_1.Text>
        </react_native_1.View>
        
        <react_native_1.View style={styles.statCard}>
          <react_native_1.Text style={styles.statIcon}>🧠</react_native_1.Text>
          <react_native_1.Text style={styles.statTitle}>Local Models</react_native_1.Text>
          <react_native_1.Text style={styles.statValue}>{state.localModels.length}</react_native_1.Text>
        </react_native_1.View>
        
        <react_native_1.View style={styles.statCard}>
          <react_native_1.Text style={styles.statIcon}>☁️</react_native_1.Text>
          <react_native_1.Text style={styles.statTitle}>Cloud Queries</react_native_1.Text>
          <react_native_1.Text style={styles.statValue}>{state.cloudQueries.length}</react_native_1.Text>
        </react_native_1.View>
        
        <react_native_1.View style={styles.statCard}>
          <react_native_1.Text style={styles.statIcon}>📱</react_native_1.Text>
          <react_native_1.Text style={styles.statTitle}>Devices Online</react_native_1.Text>
          <react_native_1.Text style={styles.statValue}>
            {state.deviceStatuses.filter(d => d.isOnline).length}
          </react_native_1.Text>
        </react_native_1.View>
      </react_native_1.View>

      <react_native_1.View style={styles.querySection}>
        <react_native_1.Text style={styles.sectionTitle}>💬 Test Query</react_native_1.Text>
        <react_native_1.View style={styles.queryInput}>
          <react_native_1.TextInput style={styles.textInput} value={state.currentQuery} onChangeText={(text) => setState(prev => ({ ...prev, currentQuery: text }))} placeholder="Enter a test query..." editable={!state.isProcessing} multiline/>
          <react_native_1.TouchableOpacity style={[styles.sendButton, (!state.currentQuery.trim() || state.isProcessing) && styles.sendButtonDisabled]} onPress={handleQuerySubmit} disabled={state.isProcessing || !state.currentQuery.trim()}>
            <react_native_1.Text style={styles.sendButtonText}>
              {state.isProcessing ? '⏳ Processing...' : '🚀 Send'}
            </react_native_1.Text>
          </react_native_1.TouchableOpacity>
        </react_native_1.View>
        
        {state.lastResponse && (<react_native_1.View style={styles.responseCard}>
            <react_native_1.View style={styles.responseHeader}>
              <react_native_1.View style={[styles.sourceBadge,
                state.lastResponse.source === 'local' ? styles.sourceBadgeLocal : styles.sourceBadgeExternal
            ]}>
                <react_native_1.Text style={styles.sourceBadgeText}>
                  {state.lastResponse.source === 'local' ? '🏠 Local' : '☁️ External'}
                </react_native_1.Text>
              </react_native_1.View>
              <react_native_1.Text style={styles.modelInfo}>{state.lastResponse.model}</react_native_1.Text>
              <react_native_1.Text style={styles.confidence}>
                {Math.round(state.lastResponse.confidence * 100)}% confidence
              </react_native_1.Text>
            </react_native_1.View>
            <react_native_1.Text style={styles.responseText}>{state.lastResponse.text}</react_native_1.Text>
            <react_native_1.View style={styles.responseMeta}>
              <react_native_1.Text style={styles.responseMetaText}>
                Tokens used: {state.lastResponse.tokensUsed}
              </react_native_1.Text>
            </react_native_1.View>
          </react_native_1.View>)}
      </react_native_1.View>
    </react_native_1.ScrollView>);
    const renderLanguageTab = () => (<react_native_1.ScrollView style={styles.tabContent}>
      <react_native_1.Text style={styles.tabTitle}>🌐 Multilingual Runtime</react_native_1.Text>
      
      <react_native_1.View style={styles.section}>
        <react_native_1.Text style={styles.sectionTitle}>Supported Languages</react_native_1.Text>
        {state.languageProfiles.map(profile => (<react_native_1.View key={profile.code} style={[styles.languageCard, profile.isDefault && styles.languageCardDefault]}>
            <react_native_1.View style={styles.languageHeader}>
              <react_native_1.Text style={styles.languageCode}>{profile.code}</react_native_1.Text>
              {profile.isDefault && (<react_native_1.View style={styles.defaultBadge}>
                  <react_native_1.Text style={styles.defaultBadgeText}>Default</react_native_1.Text>
                </react_native_1.View>)}
            </react_native_1.View>
            <react_native_1.Text style={styles.languageName}>{profile.name}</react_native_1.Text>
            {profile.dialect && (<react_native_1.Text style={styles.languageDialect}>Dialect: {profile.dialect}</react_native_1.Text>)}
            <react_native_1.Text style={styles.vocabularyCount}>
              Custom vocabulary: {profile.customVocabulary.length} words
            </react_native_1.Text>
          </react_native_1.View>))}
      </react_native_1.View>

      <react_native_1.View style={styles.section}>
        <react_native_1.Text style={styles.sectionTitle}>Settings</react_native_1.Text>
        <react_native_1.View style={styles.settingsGroup}>
          <react_native_1.View style={styles.settingItem}>
            <react_native_1.Text style={styles.settingText}>Auto-detect language from speech</react_native_1.Text>
            <react_native_1.Switch value={true}/>
          </react_native_1.View>
          <react_native_1.View style={styles.settingItem}>
            <react_native_1.Text style={styles.settingText}>Switch languages mid-conversation</react_native_1.Text>
            <react_native_1.Switch value={true}/>
          </react_native_1.View>
          <react_native_1.View style={styles.settingItem}>
            <react_native_1.Text style={styles.settingText}>Enable dialect adaptation</react_native_1.Text>
            <react_native_1.Switch value={false}/>
          </react_native_1.View>
        </react_native_1.View>
      </react_native_1.View>
    </react_native_1.ScrollView>);
    const renderLLMTab = () => (<react_native_1.ScrollView style={styles.tabContent}>
      <react_native_1.Text style={styles.tabTitle}>🧠 Local LLM Stack</react_native_1.Text>
      
      <react_native_1.View style={styles.section}>
        <react_native_1.Text style={styles.sectionTitle}>Available Models</react_native_1.Text>
        {state.localModels.map(model => (<react_native_1.View key={model.name} style={styles.modelCard}>
            <react_native_1.View style={styles.modelHeader}>
              <react_native_1.Text style={styles.modelName}>{model.name}</react_native_1.Text>
              <react_native_1.View style={[styles.typeBadge,
                model.type === 'local' ? styles.typeBadgeLocal : styles.typeBadgeExternal
            ]}>
                <react_native_1.Text style={styles.typeBadgeText}>
                  {model.type === 'local' ? '🏠 Local' : '☁️ External'}
                </react_native_1.Text>
              </react_native_1.View>
            </react_native_1.View>
            <react_native_1.View style={styles.modelSpecs}>
              <react_native_1.Text style={styles.specText}>Memory: {model.memoryLimit}MB</react_native_1.Text>
              <react_native_1.Text style={styles.specText}>Tokens: {model.tokenLimit}</react_native_1.Text>
            </react_native_1.View>
            <react_native_1.Text style={styles.capabilitiesTitle}>Capabilities:</react_native_1.Text>
            <react_native_1.View style={styles.capabilityTags}>
              {model.capabilities.map(cap => (<react_native_1.View key={cap} style={styles.capabilityTag}>
                  <react_native_1.Text style={styles.capabilityText}>{cap}</react_native_1.Text>
                </react_native_1.View>))}
            </react_native_1.View>
          </react_native_1.View>))}
      </react_native_1.View>

      <react_native_1.View style={styles.section}>
        <react_native_1.Text style={styles.sectionTitle}>Routing Configuration</react_native_1.Text>
        <react_native_1.View style={styles.routingRules}>
          <react_native_1.View style={styles.ruleItem}>
            <react_native_1.Text style={styles.ruleCondition}>Confidence &lt; 70%</react_native_1.Text>
            <react_native_1.Text style={styles.ruleArrow}>→</react_native_1.Text>
            <react_native_1.Text style={styles.ruleAction}>External LLM</react_native_1.Text>
          </react_native_1.View>
          <react_native_1.View style={styles.ruleItem}>
            <react_native_1.Text style={styles.ruleCondition}>Unknown Intent</react_native_1.Text>
            <react_native_1.Text style={styles.ruleArrow}>→</react_native_1.Text>
            <react_native_1.Text style={styles.ruleAction}>Fallback</react_native_1.Text>
          </react_native_1.View>
          <react_native_1.View style={styles.ruleItem}>
            <react_native_1.Text style={styles.ruleCondition}>Default</react_native_1.Text>
            <react_native_1.Text style={styles.ruleArrow}>→</react_native_1.Text>
            <react_native_1.Text style={styles.ruleAction}>Local Model</react_native_1.Text>
          </react_native_1.View>
        </react_native_1.View>
      </react_native_1.View>
    </react_native_1.ScrollView>);
    const renderPrivacyTab = () => (<react_native_1.ScrollView style={styles.tabContent}>
      <react_native_1.Text style={styles.tabTitle}>🔐 Privacy Dashboard</react_native_1.Text>
      
      <react_native_1.View style={styles.section}>
        <react_native_1.Text style={styles.sectionTitle}>Privacy Settings</react_native_1.Text>
        <react_native_1.View style={styles.settingsGroup}>
          <react_native_1.View style={styles.settingItem}>
            <react_native_1.Text style={styles.settingText}>Enable cloud access</react_native_1.Text>
            <react_native_1.Switch value={state.privacySettings.cloudAccessEnabled} onValueChange={(value) => handlePrivacySettingsUpdate({
            cloudAccessEnabled: value
        })}/>
          </react_native_1.View>
          
          <react_native_1.View style={styles.settingItem}>
            <react_native_1.Text style={styles.settingText}>Request consent per query</react_native_1.Text>
            <react_native_1.Switch value={state.privacySettings.cloudAccessPerQuery} onValueChange={(value) => handlePrivacySettingsUpdate({
            cloudAccessPerQuery: value
        })}/>
          </react_native_1.View>
          
          <react_native_1.View style={styles.settingItem}>
            <react_native_1.Text style={styles.settingText}>End-to-end encryption</react_native_1.Text>
            <react_native_1.Switch value={state.privacySettings.encryptionEnabled} onValueChange={(value) => handlePrivacySettingsUpdate({
            encryptionEnabled: value
        })}/>
          </react_native_1.View>
          
          <react_native_1.View style={styles.settingItem}>
            <react_native_1.Text style={styles.settingText}>Audit logging</react_native_1.Text>
            <react_native_1.Switch value={state.privacySettings.auditLoggingEnabled} onValueChange={(value) => handlePrivacySettingsUpdate({
            auditLoggingEnabled: value
        })}/>
          </react_native_1.View>
        </react_native_1.View>

        <react_native_1.View style={styles.retentionSetting}>
          <react_native_1.Text style={styles.settingText}>
            Data retention: {state.privacySettings.dataRetentionDays} days
          </react_native_1.Text>
        </react_native_1.View>
      </react_native_1.View>

      <react_native_1.View style={styles.section}>
        <react_native_1.Text style={styles.sectionTitle}>Recent Cloud Queries</react_native_1.Text>
        {state.cloudQueries.length === 0 ? (<react_native_1.Text style={styles.noQueries}>No cloud queries recorded</react_native_1.Text>) : (<react_native_1.View style={styles.queryList}>
            {state.cloudQueries.map(query => (<react_native_1.View key={query.id} style={styles.queryItem}>
                <react_native_1.View style={styles.queryItemHeader}>
                  <react_native_1.Text style={styles.timestamp}>
                    {query.timestamp.toLocaleString()}
                  </react_native_1.Text>
                  <react_native_1.View style={styles.serviceBadge}>
                    <react_native_1.Text style={styles.serviceBadgeText}>{query.service}</react_native_1.Text>
                  </react_native_1.View>
                  {query.encrypted && <react_native_1.Text style={styles.encryptedBadge}>🔒</react_native_1.Text>}
                </react_native_1.View>
                <react_native_1.Text style={styles.queryText} numberOfLines={3}>
                  {query.query.length > 100
                    ? query.query.substring(0, 100) + '...'
                    : query.query}
                </react_native_1.Text>
                <react_native_1.View style={styles.queryMeta}>
                  <react_native_1.Text style={styles.queryMetaText}>
                    Data shared: {query.dataShared.join(', ')}
                  </react_native_1.Text>
                  <react_native_1.Text style={[styles.queryMetaText,
                    query.userConsent ? styles.consentYes : styles.consentNo
                ]}>
                    {query.userConsent ? '✅ Consented' : '❌ No consent'}
                  </react_native_1.Text>
                </react_native_1.View>
              </react_native_1.View>))}
          </react_native_1.View>)}
      </react_native_1.View>

      <react_native_1.View style={styles.section}>
        <react_native_1.Text style={styles.sectionTitle}>Device Status</react_native_1.Text>
        {state.deviceStatuses.map(device => (<react_native_1.View key={device.deviceId} style={styles.deviceCard}>
            <react_native_1.View style={styles.deviceHeader}>
              <react_native_1.Text style={styles.deviceName}>{device.name}</react_native_1.Text>
              <react_native_1.Text style={[styles.statusIndicator,
                device.isOnline ? styles.statusOnline : styles.statusOffline
            ]}>
                {device.isOnline ? '🟢 Online' : '🔴 Offline'}
              </react_native_1.Text>
            </react_native_1.View>
            <react_native_1.View style={styles.deviceInfo}>
              <react_native_1.Text style={styles.deviceInfoText}>
                Privacy mode: <react_native_1.Text style={styles.privacyMode}>{device.privacyMode}</react_native_1.Text>
              </react_native_1.Text>
              <react_native_1.Text style={styles.deviceInfoText}>Cloud queries: {device.cloudQueriesCount}</react_native_1.Text>
              <react_native_1.Text style={styles.deviceInfoText}>
                Last activity: {device.lastActivity.toLocaleString()}
              </react_native_1.Text>
            </react_native_1.View>
          </react_native_1.View>))}
      </react_native_1.View>
    </react_native_1.ScrollView>);
    return (<react_native_1.View style={styles.container}>
      <react_native_1.View style={styles.header}>
        <react_native_1.Text style={styles.headerTitle}>🎤 EthervoxAI</react_native_1.Text>
      </react_native_1.View>

      <react_native_1.View style={styles.tabBar}>
        <react_native_1.TouchableOpacity style={[styles.tabButton, activeTab === 'overview' && styles.tabButtonActive]} onPress={() => setActiveTab('overview')}>
          <react_native_1.Text style={[styles.tabButtonText, activeTab === 'overview' && styles.tabButtonTextActive]}>
            📊 Overview
          </react_native_1.Text>
        </react_native_1.TouchableOpacity>
        <react_native_1.TouchableOpacity style={[styles.tabButton, activeTab === 'language' && styles.tabButtonActive]} onPress={() => setActiveTab('language')}>
          <react_native_1.Text style={[styles.tabButtonText, activeTab === 'language' && styles.tabButtonTextActive]}>
            🌐 Languages
          </react_native_1.Text>
        </react_native_1.TouchableOpacity>
        <react_native_1.TouchableOpacity style={[styles.tabButton, activeTab === 'llm' && styles.tabButtonActive]} onPress={() => setActiveTab('llm')}>
          <react_native_1.Text style={[styles.tabButtonText, activeTab === 'llm' && styles.tabButtonTextActive]}>
            🧠 LLM
          </react_native_1.Text>
        </react_native_1.TouchableOpacity>
        <react_native_1.TouchableOpacity style={[styles.tabButton, activeTab === 'privacy' && styles.tabButtonActive]} onPress={() => setActiveTab('privacy')}>
          <react_native_1.Text style={[styles.tabButtonText, activeTab === 'privacy' && styles.tabButtonTextActive]}>
            🔐 Privacy
          </react_native_1.Text>
        </react_native_1.TouchableOpacity>
      </react_native_1.View>

      <react_native_1.View style={styles.content}>
        {activeTab === 'overview' && renderOverviewTab()}
        {activeTab === 'language' && renderLanguageTab()}
        {activeTab === 'llm' && renderLLMTab()}
        {activeTab === 'privacy' && renderPrivacyTab()}
      </react_native_1.View>
    </react_native_1.View>);
};
exports.DashboardMobile = DashboardMobile;
const styles = react_native_1.StyleSheet.create({
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
//# sourceMappingURL=DashboardMobile.js.map