"use strict";
/**
 * 🔐 Privacy Dashboard Module
 *
 * Provides users with full control over how EthervoxAI interacts with
 * cloud services, stores data, and responds to voice input.
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.privacyDashboard = exports.PrivacyDashboard = void 0;
class PrivacyDashboard {
    constructor() {
        this.cloudQueryHistory = [];
        this.auditLog = [];
        this.deviceStatuses = new Map();
        this.privacySettings = this.getDefaultSettings();
        this.initializeDeviceStatuses();
    }
    /**
     * Get default privacy settings
     */
    getDefaultSettings() {
        return {
            cloudAccessEnabled: false,
            cloudAccessPerQuery: true,
            dataRetentionDays: 30,
            encryptionEnabled: true,
            auditLoggingEnabled: true,
            allowedServices: [],
            blockedServices: []
        };
    }
    /**
     * Initialize device statuses
     */
    initializeDeviceStatuses() {
        // Placeholder device for MVP
        const defaultDevice = {
            deviceId: 'local-device-001',
            name: 'Local EthervoxAI',
            isOnline: true,
            lastActivity: new Date(),
            privacyMode: 'balanced',
            cloudQueriesCount: 0
        };
        this.deviceStatuses.set(defaultDevice.deviceId, defaultDevice);
    }
    /**
     * Update privacy settings
     */
    updatePrivacySettings(settings) {
        this.privacySettings = { ...this.privacySettings, ...settings };
        this.logAuditEvent({
            event: 'privacy_settings_updated',
            details: settings,
            severity: 'info'
        });
    }
    /**
     * Get current privacy settings
     */
    getPrivacySettings() {
        return { ...this.privacySettings };
    }
    /**
     * Check if cloud access is allowed for a query
     */
    isCloudAccessAllowed(service, requiresConsent = false) {
        if (!this.privacySettings.cloudAccessEnabled) {
            return false;
        }
        if (this.privacySettings.blockedServices.includes(service)) {
            return false;
        }
        if (this.privacySettings.allowedServices.length > 0 &&
            !this.privacySettings.allowedServices.includes(service)) {
            return false;
        }
        if (requiresConsent && this.privacySettings.cloudAccessPerQuery) {
            return false; // Requires explicit user consent
        }
        return true;
    }
    /**
     * Request user consent for cloud query
     */
    async requestCloudConsent(query, service, dataToShare) {
        // In a real implementation, this would show a UI prompt
        // For now, return based on privacy settings
        if (!this.privacySettings.cloudAccessPerQuery) {
            return this.privacySettings.cloudAccessEnabled;
        }
        // Placeholder for user consent logic
        // This would integrate with the UI to show a consent dialog
        const userConsent = await this.showConsentDialog(query, service, dataToShare);
        this.logAuditEvent({
            event: 'cloud_consent_requested',
            details: {
                query: query.substring(0, 50) + '...',
                service,
                dataToShare,
                consent: userConsent
            },
            severity: 'info'
        });
        return userConsent;
    }
    /**
     * Show consent dialog (placeholder)
     */
    async showConsentDialog(query, service, dataToShare) {
        // This would be implemented in the UI layer
        // Return false by default for privacy
        return false;
    }
    /**
     * Log a cloud query
     */
    logCloudQuery(query, service, encrypted, dataShared, userConsent) {
        const cloudQuery = {
            id: this.generateQueryId(),
            timestamp: new Date(),
            query: encrypted ? '[ENCRYPTED]' : query,
            service,
            encrypted,
            dataShared,
            userConsent
        };
        this.cloudQueryHistory.push(cloudQuery);
        // Update device status
        const device = this.deviceStatuses.get('local-device-001');
        if (device) {
            device.cloudQueriesCount++;
            device.lastActivity = new Date();
        }
        // Log audit event
        this.logAuditEvent({
            event: 'cloud_query_executed',
            details: {
                queryId: cloudQuery.id,
                service,
                encrypted,
                dataShared: dataShared.length
            },
            severity: 'info'
        });
        // Clean up old queries based on retention policy
        this.cleanupOldQueries();
        return cloudQuery.id;
    }
    /**
     * Get cloud query history
     */
    getCloudQueryHistory(limit = 100) {
        return this.cloudQueryHistory
            .sort((a, b) => b.timestamp.getTime() - a.timestamp.getTime())
            .slice(0, limit);
    }
    /**
     * Delete specific cloud query
     */
    deleteCloudQuery(queryId) {
        const index = this.cloudQueryHistory.findIndex(q => q.id === queryId);
        if (index === -1) {
            return false;
        }
        this.cloudQueryHistory.splice(index, 1);
        this.logAuditEvent({
            event: 'cloud_query_deleted',
            details: { queryId },
            severity: 'info'
        });
        return true;
    }
    /**
     * Delete all cloud queries
     */
    deleteAllCloudQueries() {
        const count = this.cloudQueryHistory.length;
        this.cloudQueryHistory = [];
        this.logAuditEvent({
            event: 'all_cloud_queries_deleted',
            details: { count },
            severity: 'warning'
        });
    }
    /**
     * Export user data
     */
    exportUserData() {
        this.logAuditEvent({
            event: 'user_data_exported',
            details: {},
            severity: 'info'
        });
        return {
            settings: this.privacySettings,
            cloudQueries: this.cloudQueryHistory,
            auditLog: this.auditLog,
            devices: Array.from(this.deviceStatuses.values())
        };
    }
    /**
     * Get device statuses
     */
    getDeviceStatuses() {
        return Array.from(this.deviceStatuses.values());
    }
    /**
     * Update device status
     */
    updateDeviceStatus(deviceId, updates) {
        const device = this.deviceStatuses.get(deviceId);
        if (device) {
            Object.assign(device, updates);
            device.lastActivity = new Date();
            this.logAuditEvent({
                event: 'device_status_updated',
                details: { deviceId, updates },
                severity: 'info'
            });
        }
    }
    /**
     * Get audit log
     */
    getAuditLog(limit = 100) {
        return this.auditLog
            .sort((a, b) => b.timestamp.getTime() - a.timestamp.getTime())
            .slice(0, limit);
    }
    /**
     * Log audit event
     */
    logAuditEvent(event) {
        if (!this.privacySettings.auditLoggingEnabled) {
            return;
        }
        const auditEntry = {
            id: this.generateAuditId(),
            timestamp: new Date(),
            ...event
        };
        this.auditLog.push(auditEntry);
        // Keep only recent audit entries
        if (this.auditLog.length > 1000) {
            this.auditLog = this.auditLog.slice(-1000);
        }
    }
    /**
     * Clean up old queries based on retention policy
     */
    cleanupOldQueries() {
        const cutoffDate = new Date();
        cutoffDate.setDate(cutoffDate.getDate() - this.privacySettings.dataRetentionDays);
        const originalLength = this.cloudQueryHistory.length;
        this.cloudQueryHistory = this.cloudQueryHistory.filter(query => query.timestamp > cutoffDate);
        const deletedCount = originalLength - this.cloudQueryHistory.length;
        if (deletedCount > 0) {
            this.logAuditEvent({
                event: 'automatic_query_cleanup',
                details: { deletedCount },
                severity: 'info'
            });
        }
    }
    /**
     * Generate unique query ID
     */
    generateQueryId() {
        return `query_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
    }
    /**
     * Generate unique audit ID
     */
    generateAuditId() {
        return `audit_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
    }
    /**
     * Encrypt data (placeholder)
     */
    encryptData(data) {
        // Placeholder for actual encryption
        // In production, use proper encryption libraries
        return btoa(data); // Simple base64 encoding for demo
    }
    /**
     * Decrypt data (placeholder)
     */
    decryptData(encryptedData) {
        // Placeholder for actual decryption
        try {
            return atob(encryptedData); // Simple base64 decoding for demo
        }
        catch {
            return '[DECRYPTION_FAILED]';
        }
    }
}
exports.PrivacyDashboard = PrivacyDashboard;
exports.privacyDashboard = new PrivacyDashboard();
//# sourceMappingURL=privacyDashboard.js.map