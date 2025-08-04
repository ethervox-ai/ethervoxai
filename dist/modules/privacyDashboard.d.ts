/**
 * 🔐 Privacy Dashboard Module
 *
 * Provides users with full control over how EthervoxAI interacts with
 * cloud services, stores data, and responds to voice input.
 */
export interface CloudQuery {
    id: string;
    timestamp: Date;
    query: string;
    service: string;
    encrypted: boolean;
    dataShared: string[];
    userConsent: boolean;
}
export interface PrivacySettings {
    cloudAccessEnabled: boolean;
    cloudAccessPerQuery: boolean;
    dataRetentionDays: number;
    encryptionEnabled: boolean;
    auditLoggingEnabled: boolean;
    allowedServices: string[];
    blockedServices: string[];
}
export interface DeviceStatus {
    deviceId: string;
    name: string;
    isOnline: boolean;
    lastActivity: Date;
    privacyMode: 'strict' | 'balanced' | 'permissive';
    cloudQueriesCount: number;
}
export interface AuditLogEntry {
    id: string;
    timestamp: Date;
    event: string;
    details: Record<string, any>;
    severity: 'info' | 'warning' | 'error';
}
export declare class PrivacyDashboard {
    private privacySettings;
    private cloudQueryHistory;
    private auditLog;
    private deviceStatuses;
    constructor();
    /**
     * Get default privacy settings
     */
    private getDefaultSettings;
    /**
     * Initialize device statuses
     */
    private initializeDeviceStatuses;
    /**
     * Update privacy settings
     */
    updatePrivacySettings(settings: Partial<PrivacySettings>): void;
    /**
     * Get current privacy settings
     */
    getPrivacySettings(): PrivacySettings;
    /**
     * Check if cloud access is allowed for a query
     */
    isCloudAccessAllowed(service: string, requiresConsent?: boolean): boolean;
    /**
     * Request user consent for cloud query
     */
    requestCloudConsent(query: string, service: string, dataToShare: string[]): Promise<boolean>;
    /**
     * Show consent dialog (placeholder)
     */
    private showConsentDialog;
    /**
     * Log a cloud query
     */
    logCloudQuery(query: string, service: string, encrypted: boolean, dataShared: string[], userConsent: boolean): string;
    /**
     * Get cloud query history
     */
    getCloudQueryHistory(limit?: number): CloudQuery[];
    /**
     * Delete specific cloud query
     */
    deleteCloudQuery(queryId: string): boolean;
    /**
     * Delete all cloud queries
     */
    deleteAllCloudQueries(): void;
    /**
     * Export user data
     */
    exportUserData(): {
        settings: PrivacySettings;
        cloudQueries: CloudQuery[];
        auditLog: AuditLogEntry[];
        devices: DeviceStatus[];
    };
    /**
     * Get device statuses
     */
    getDeviceStatuses(): DeviceStatus[];
    /**
     * Update device status
     */
    updateDeviceStatus(deviceId: string, updates: Partial<DeviceStatus>): void;
    /**
     * Get audit log
     */
    getAuditLog(limit?: number): AuditLogEntry[];
    /**
     * Log audit event
     */
    private logAuditEvent;
    /**
     * Clean up old queries based on retention policy
     */
    private cleanupOldQueries;
    /**
     * Generate unique query ID
     */
    private generateQueryId;
    /**
     * Generate unique audit ID
     */
    private generateAuditId;
    /**
     * Encrypt data (placeholder)
     */
    private encryptData;
    /**
     * Decrypt data (placeholder)
     */
    private decryptData;
}
export declare const privacyDashboard: PrivacyDashboard;
//# sourceMappingURL=privacyDashboard.d.ts.map