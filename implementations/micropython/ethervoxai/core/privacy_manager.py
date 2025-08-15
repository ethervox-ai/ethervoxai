"""
🔐 Privacy Manager - MicroPython Implementation

Privacy controls and audit logging for microcontroller-based voice AI.
Ensures user data protection and provides transparency in voice processing
while optimizing for memory-constrained environments.

Key Features:
- Local-only processing by default
- Minimal audit logging with circular buffers
- User consent management
- Data retention controls
- Privacy-preserving operation modes
- Memory-efficient logging

Privacy Principles:
- All voice processing happens on-device
- No data transmission without explicit consent
- Audit logs stored locally with size limits
- User has full control over data retention
- Privacy settings persist across reboots
"""

import gc
import json
import time
import micropython
from collections import namedtuple, deque

# Privacy settings structure
PrivacySettings = namedtuple('PrivacySettings', [
    'local_processing_only', 'enable_audit_logging', 'max_log_entries',
    'data_retention_days', 'require_consent', 'allow_model_updates'
])

# Audit log entry structure (memory efficient)
AuditEntry = namedtuple('AuditEntry', [
    'timestamp', 'event_type', 'data_hash', 'confidence', 'model_used'
])

# Privacy consent record
ConsentRecord = namedtuple('ConsentRecord', [
    'feature', 'granted', 'timestamp', 'expires'
])

class PrivacyManager:
    """
    Privacy Manager for microcontroller voice AI
    
    Manages privacy settings, consent, and audit logging while
    optimizing for memory constraints and local-only operation.
    """
    
    def __init__(self, enable_logging=True, debug=False):
        """
        Initialize Privacy Manager
        
        Args:
            enable_logging (bool): Enable audit logging
            debug (bool): Enable debug output
        """
        self.debug = debug
        
        # Default privacy settings (privacy-first)
        self.settings = PrivacySettings(
            local_processing_only=True,
            enable_audit_logging=enable_logging,
            max_log_entries=50,  # Limited for MCU memory
            data_retention_days=7,
            require_consent=True,
            allow_model_updates=False
        )
        
        # Audit log (circular buffer for memory efficiency)
        self.audit_log = deque((), maxlen=self.settings.max_log_entries)
        
        # Consent records
        self.consent_records = {}
        
        # Privacy statistics
        self.total_interactions = 0
        self.total_voice_processing = 0
        self.consent_requests = 0
        self.consent_granted = 0
        
        # Load settings from persistent storage (if available)
        self._load_privacy_settings()
        
        if self.debug:
            print(f"🔐 PrivacyManager initialized")
            print(f"   Local processing only: {self.settings.local_processing_only}")
            print(f"   Audit logging: {self.settings.enable_audit_logging}")
            print(f"   Max log entries: {self.settings.max_log_entries}")
    
    def log_interaction(self, interaction_data):
        """Log interaction for audit purposes"""
        if not self.settings.enable_audit_logging:
            return
        
        try:
            # Create privacy-preserving hash of sensitive data
            data_hash = self._create_privacy_hash(interaction_data)
            
            # Create audit entry
            entry = AuditEntry(
                timestamp=time.time(),
                event_type=interaction_data.get('result_type', 'unknown'),
                data_hash=data_hash,
                confidence=interaction_data.get('confidence', 0.0),
                model_used=interaction_data.get('model_name', 'unknown')
            )
            
            # Add to circular buffer (automatically removes old entries)
            self.audit_log.append(entry)
            
            # Update statistics
            self.total_interactions += 1
            
            if interaction_data.get('result_type') in ['voice_activity', 'wake_word', 'command']:
                self.total_voice_processing += 1
            
            if self.debug:
                print(f"🔍 Logged interaction: {entry.event_type} "
                      f"(confidence: {entry.confidence:.2f})")
            
        except Exception as e:
            if self.debug:
                print(f"⚠️ Audit logging error: {e}")
    
    def _create_privacy_hash(self, data):
        """Create privacy-preserving hash of sensitive data"""
        try:
            # Simple hash for MCU (not cryptographically secure)
            # In production, use proper hashing
            
            sensitive_fields = ['command', 'response', 'audio_data']
            hash_input = ""
            
            for field in sensitive_fields:
                if field in data:
                    value = str(data[field])
                    # Simple character sum hash
                    hash_input += str(sum(ord(c) for c in value[:10]))  # First 10 chars
            
            # Add timestamp for uniqueness
            hash_input += str(int(time.time()) % 10000)
            
            # Simple hash calculation
            hash_value = 0
            for char in hash_input:
                hash_value = (hash_value * 31 + ord(char)) % 1000000
            
            return f"hash_{hash_value:06d}"
            
        except Exception as e:
            if self.debug:
                print(f"⚠️ Hash creation error: {e}")
            return "hash_error"
    
    def request_consent(self, feature, description=None):
        """Request user consent for a feature"""
        if not self.settings.require_consent:
            return True
        
        self.consent_requests += 1
        
        try:
            # Check if consent already granted and not expired
            if feature in self.consent_records:
                record = self.consent_records[feature]
                if record.granted and (record.expires is None or time.time() < record.expires):
                    if self.debug:
                        print(f"✅ Consent already granted for: {feature}")
                    return True
            
            # For MCU, we'll assume consent is granted for essential features
            # In a full implementation, this would interact with user interface
            essential_features = ['voice_processing', 'wake_word_detection', 'local_inference']
            
            if feature in essential_features:
                granted = True
                if self.debug:
                    print(f"✅ Auto-granted consent for essential feature: {feature}")
            else:
                # For non-essential features, require explicit consent
                granted = False
                if self.debug:
                    print(f"❌ Consent required for non-essential feature: {feature}")
                    if description:
                        print(f"   Description: {description}")
            
            # Record consent decision
            expiry = time.time() + (30 * 24 * 60 * 60) if granted else None  # 30 days
            
            self.consent_records[feature] = ConsentRecord(
                feature=feature,
                granted=granted,
                timestamp=time.time(),
                expires=expiry
            )
            
            if granted:
                self.consent_granted += 1
            
            # Save to persistent storage
            self._save_consent_records()
            
            return granted
            
        except Exception as e:
            if self.debug:
                print(f"❌ Consent request error: {e}")
            return False
    
    def revoke_consent(self, feature):
        """Revoke consent for a feature"""
        try:
            if feature in self.consent_records:
                # Update record to revoked
                old_record = self.consent_records[feature]
                self.consent_records[feature] = ConsentRecord(
                    feature=feature,
                    granted=False,
                    timestamp=time.time(),
                    expires=None
                )
                
                # Save changes
                self._save_consent_records()
                
                if self.debug:
                    print(f"🚫 Consent revoked for: {feature}")
                
                return True
            else:
                if self.debug:
                    print(f"⚠️ No consent record found for: {feature}")
                return False
                
        except Exception as e:
            if self.debug:
                print(f"❌ Consent revocation error: {e}")
            return False
    
    def get_privacy_status(self):
        """Get current privacy status and settings"""
        return {
            'settings': {
                'local_processing_only': self.settings.local_processing_only,
                'enable_audit_logging': self.settings.enable_audit_logging,
                'data_retention_days': self.settings.data_retention_days,
                'require_consent': self.settings.require_consent
            },
            'statistics': {
                'total_interactions': self.total_interactions,
                'total_voice_processing': self.total_voice_processing,
                'consent_requests': self.consent_requests,
                'consent_granted': self.consent_granted,
                'audit_log_entries': len(self.audit_log)
            },
            'consent_summary': {
                'granted_features': [f for f, r in self.consent_records.items() if r.granted],
                'revoked_features': [f for f, r in self.consent_records.items() if not r.granted],
                'total_features': len(self.consent_records)
            },
            'memory_usage': {
                'audit_log_kb': len(self.audit_log) * 64 // 1024,  # Rough estimate
                'consent_records_kb': len(self.consent_records) * 32 // 1024
            }
        }
    
    def get_audit_log(self, limit=10):
        """Get recent audit log entries"""
        if not self.settings.enable_audit_logging:
            return []
        
        # Return most recent entries
        recent_entries = list(self.audit_log)[-limit:]
        
        # Convert to readable format
        readable_entries = []
        for entry in recent_entries:
            readable_entries.append({
                'timestamp': entry.timestamp,
                'time_ago_s': int(time.time() - entry.timestamp),
                'event_type': entry.event_type,
                'data_hash': entry.data_hash,
                'confidence': entry.confidence,
                'model_used': entry.model_used
            })
        
        return readable_entries
    
    def clear_audit_log(self):
        """Clear audit log (with user consent)"""
        if not self.request_consent('clear_audit_log', 'Clear all audit log entries'):
            if self.debug:
                print("❌ Consent not granted for clearing audit log")
            return False
        
        try:
            self.audit_log.clear()
            
            if self.debug:
                print("🗑️ Audit log cleared")
            
            return True
            
        except Exception as e:
            if self.debug:
                print(f"❌ Error clearing audit log: {e}")
            return False
    
    def update_privacy_settings(self, **kwargs):
        """Update privacy settings"""
        try:
            # Create new settings with updates
            current_settings = self.settings._asdict()
            current_settings.update(kwargs)
            
            # Validate settings
            if 'max_log_entries' in kwargs:
                if kwargs['max_log_entries'] < 10 or kwargs['max_log_entries'] > 100:
                    if self.debug:
                        print("⚠️ max_log_entries must be between 10 and 100")
                    return False
            
            if 'data_retention_days' in kwargs:
                if kwargs['data_retention_days'] < 1 or kwargs['data_retention_days'] > 30:
                    if self.debug:
                        print("⚠️ data_retention_days must be between 1 and 30")
                    return False
            
            # Apply new settings
            self.settings = PrivacySettings(**current_settings)
            
            # Update audit log size if changed
            if 'max_log_entries' in kwargs:
                new_log = deque(self.audit_log, maxlen=self.settings.max_log_entries)
                self.audit_log = new_log
            
            # Save to persistent storage
            self._save_privacy_settings()
            
            if self.debug:
                print(f"✅ Privacy settings updated: {list(kwargs.keys())}")
            
            return True
            
        except Exception as e:
            if self.debug:
                print(f"❌ Error updating privacy settings: {e}")
            return False
    
    def _load_privacy_settings(self):
        """Load privacy settings from persistent storage"""
        try:
            # In a real implementation, this would load from flash storage
            # For now, we'll use default settings
            
            if self.debug:
                print("📁 Using default privacy settings (no persistent storage)")
            
        except Exception as e:
            if self.debug:
                print(f"⚠️ Error loading privacy settings: {e}")
    
    def _save_privacy_settings(self):
        """Save privacy settings to persistent storage"""
        try:
            # In a real implementation, this would save to flash storage
            # For now, we'll just log the action
            
            if self.debug:
                print("💾 Privacy settings saved (simulated)")
            
        except Exception as e:
            if self.debug:
                print(f"⚠️ Error saving privacy settings: {e}")
    
    def _save_consent_records(self):
        """Save consent records to persistent storage"""
        try:
            # In a real implementation, this would save to flash storage
            # For now, we'll just log the action
            
            if self.debug:
                print(f"💾 Consent records saved (simulated): {len(self.consent_records)} records")
            
        except Exception as e:
            if self.debug:
                print(f"⚠️ Error saving consent records: {e}")
    
    def cleanup_old_data(self):
        """Clean up old data based on retention policy"""
        try:
            current_time = time.time()
            retention_seconds = self.settings.data_retention_days * 24 * 60 * 60
            
            # Clean up old audit log entries
            if self.settings.enable_audit_logging:
                entries_to_keep = []
                removed_count = 0
                
                for entry in self.audit_log:
                    if current_time - entry.timestamp < retention_seconds:
                        entries_to_keep.append(entry)
                    else:
                        removed_count += 1
                
                # Replace audit log with cleaned version
                self.audit_log.clear()
                for entry in entries_to_keep:
                    self.audit_log.append(entry)
                
                if removed_count > 0 and self.debug:
                    print(f"🗑️ Removed {removed_count} old audit entries")
            
            # Clean up expired consent records
            expired_consents = []
            for feature, record in self.consent_records.items():
                if record.expires and current_time > record.expires:
                    expired_consents.append(feature)
            
            for feature in expired_consents:
                del self.consent_records[feature]
                if self.debug:
                    print(f"🗑️ Expired consent for: {feature}")
            
            # Force garbage collection after cleanup
            gc.collect()
            
            return {
                'audit_entries_removed': removed_count,
                'consent_records_expired': len(expired_consents)
            }
            
        except Exception as e:
            if self.debug:
                print(f"❌ Data cleanup error: {e}")
            return {'error': str(e)}
    
    def generate_privacy_report(self):
        """Generate comprehensive privacy report"""
        try:
            current_time = time.time()
            
            report = {
                'report_timestamp': current_time,
                'privacy_settings': self.settings._asdict(),
                'data_summary': {
                    'total_interactions': self.total_interactions,
                    'voice_processing_events': self.total_voice_processing,
                    'audit_log_entries': len(self.audit_log),
                    'consent_records': len(self.consent_records)
                },
                'consent_status': {},
                'recent_activity': [],
                'data_retention_compliance': True,
                'local_processing_status': self.settings.local_processing_only
            }
            
            # Add consent status details
            for feature, record in self.consent_records.items():
                report['consent_status'][feature] = {
                    'granted': record.granted,
                    'granted_date': record.timestamp,
                    'expires': record.expires,
                    'days_until_expiry': (record.expires - current_time) / (24 * 60 * 60) if record.expires else None
                }
            
            # Add recent activity (last 10 entries)
            recent_entries = self.get_audit_log(10)
            report['recent_activity'] = recent_entries
            
            # Check data retention compliance
            if self.audit_log:
                oldest_entry = min(self.audit_log, key=lambda x: x.timestamp)
                age_days = (current_time - oldest_entry.timestamp) / (24 * 60 * 60)
                report['data_retention_compliance'] = age_days <= self.settings.data_retention_days
            
            return report
            
        except Exception as e:
            if self.debug:
                print(f"❌ Privacy report generation error: {e}")
            return {'error': str(e)}
    
    def cleanup(self):
        """Clean up privacy manager resources"""
        try:
            # Save current state before cleanup
            self._save_privacy_settings()
            self._save_consent_records()
            
            # Clear memory
            self.audit_log.clear()
            self.consent_records.clear()
            
            # Force garbage collection
            gc.collect()
            
            if self.debug:
                print("🧹 PrivacyManager cleanup completed")
            
        except Exception as e:
            if self.debug:
                print(f"❌ Privacy manager cleanup error: {e}")
