/**
 * @file stores.js
 * @brief Vuex store module for managing application state in EthervoxAI Dashboard
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * 
 * This file is part of EthervoxAI, licensed under CC BY-NC-SA 4.0.
 * You are free to share and adapt this work under the following terms:
 * - Attribution: Credit the original authors
 * - NonCommercial: Not for commercial use
 * - ShareAlike: Distribute under same license
 * 
 * For full license terms, see: https://creativecommons.org/licenses/by-nc-sa/4.0/
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

import { ref, computed } from 'vue'
import { defineStore } from 'pinia'
import axios from 'axios'

export const useSystemStore = defineStore('system', () => {
  // State
  const status = ref('initializing')
  const platform = ref('unknown')
  const version = ref('0.1.0')
  const uptime = ref(0)
  const lastUpdateTime = ref(null)
  
  // Audio system state
  const audioStatus = ref('offline')
  const microphoneActive = ref(false)
  const speakerActive = ref(false)
  const currentLanguage = ref('en')
  const supportedLanguages = ref(['en', 'es', 'zh'])
  
  // Privacy settings
  const localProcessingEnabled = ref(true)
  const cloudQueriesEnabled = ref(false)
  const dataRetentionDays = ref(7)
  const auditLogEnabled = ref(true)
  
  // Performance metrics
  const cpuUsage = ref(0)
  const memoryUsage = ref(0)
  const processedQueries = ref(0)
  const averageResponseTime = ref(0)
  
  // Error tracking
  const errors = ref([])
  const warnings = ref([])
  
  // Computed
  const systemHealth = computed(() => {
    if (status.value === 'error' || errors.value.length > 5) {
      return 'critical'
    }
    if (status.value === 'degraded' || warnings.value.length > 10) {
      return 'warning'
    }
    if (status.value === 'online' && audioStatus.value === 'online') {
      return 'healthy'
    }
    return 'unknown'
  })
  
  const uptimeFormatted = computed(() => {
    const hours = Math.floor(uptime.value / 3600)
    const minutes = Math.floor((uptime.value % 3600) / 60)
    return `${hours}h ${minutes}m`
  })
  
  // Actions
  async function initializeSystem() {
    try {
      status.value = 'initializing'
      
      // Fetch system information
      const response = await axios.get('/api/system/status')
      
      status.value = response.data.status
      platform.value = response.data.platform
      version.value = response.data.version
      uptime.value = response.data.uptime
      
      // Start monitoring intervals
      startSystemMonitoring()
      
    } catch (error) {
      console.error('Failed to initialize system:', error)
      status.value = 'error'
      addError('Failed to connect to EthervoxAI core system')
    }
  }
  
  function startSystemMonitoring() {
    // Update system metrics every 5 seconds
    setInterval(async () => {
      try {
        await updateSystemMetrics()
      } catch (error) {
        console.warn('Failed to update system metrics:', error)
      }
    }, 5000)
    
    // Update uptime every minute
    setInterval(() => {
      if (status.value === 'online') {
        uptime.value += 60
      }
    }, 60000)
  }
  
  async function updateSystemMetrics() {
    try {
      const response = await axios.get('/api/system/metrics')
      
      cpuUsage.value = response.data.cpu_usage
      memoryUsage.value = response.data.memory_usage
      processedQueries.value = response.data.processed_queries
      averageResponseTime.value = response.data.avg_response_time
      
      audioStatus.value = response.data.audio_status
      microphoneActive.value = response.data.microphone_active
      speakerActive.value = response.data.speaker_active
      
      lastUpdateTime.value = new Date()
      
    } catch (error) {
      if (status.value === 'online') {
        status.value = 'degraded'
        addWarning('System metrics update failed')
      }
    }
  }
  
  async function updateAudioSettings(settings) {
    try {
      await axios.post('/api/audio/settings', settings)
      
      // Update local state
      if (Object.prototype.hasOwnProperty.call(settings, 'microphoneActive')) {
        microphoneActive.value = settings.microphoneActive
      }
      if (Object.prototype.hasOwnProperty.call(settings, 'speakerActive')) {
        speakerActive.value = settings.speakerActive
      }
      if (Object.prototype.hasOwnProperty.call(settings, 'currentLanguage')) {
        currentLanguage.value = settings.currentLanguage
      }
      
    } catch (error) {
      addError('Failed to update audio settings')
      throw error
    }
  }
  
  async function updatePrivacySettings(settings) {
    try {
      await axios.post('/api/privacy/settings', settings)
      
      // Update local state
      localProcessingEnabled.value = settings.localProcessingEnabled ?? localProcessingEnabled.value
      cloudQueriesEnabled.value = settings.cloudQueriesEnabled ?? cloudQueriesEnabled.value
      dataRetentionDays.value = settings.dataRetentionDays ?? dataRetentionDays.value
      auditLogEnabled.value = settings.auditLogEnabled ?? auditLogEnabled.value
      
    } catch (error) {
      addError('Failed to update privacy settings')
      throw error
    }
  }
  
  async function restartSystem() {
    try {
      status.value = 'restarting'
      await axios.post('/api/system/restart')
      
      // Wait for system to come back online
      setTimeout(() => {
        initializeSystem()
      }, 5000)
      
    } catch (error) {
      addError('Failed to restart system')
      status.value = 'error'
    }
  }
  
  function addError(message) {
    errors.value.unshift({
      id: Date.now(),
      message,
      timestamp: new Date(),
      type: 'error'
    })
    
    // Keep only last 50 errors
    if (errors.value.length > 50) {
      errors.value = errors.value.slice(0, 50)
    }
  }
  
  function addWarning(message) {
    warnings.value.unshift({
      id: Date.now(),
      message,
      timestamp: new Date(),
      type: 'warning'
    })
    
    // Keep only last 100 warnings
    if (warnings.value.length > 100) {
      warnings.value = warnings.value.slice(0, 100)
    }
  }
  
  function clearErrors() {
    errors.value = []
  }
  
  function clearWarnings() {
    warnings.value = []
  }
  
  return {
    // State
    status,
    platform,
    version,
    uptime,
    lastUpdateTime,
    audioStatus,
    microphoneActive,
    speakerActive,
    currentLanguage,
    supportedLanguages,
    localProcessingEnabled,
    cloudQueriesEnabled,
    dataRetentionDays,
    auditLogEnabled,
    cpuUsage,
    memoryUsage,
    processedQueries,
    averageResponseTime,
    errors,
    warnings,
    
    // Computed
    systemHealth,
    uptimeFormatted,
    
    // Actions
    initializeSystem,
    updateSystemMetrics,
    updateAudioSettings,
    updatePrivacySettings,
    restartSystem,
    addError,
    addWarning,
    clearErrors,
    clearWarnings
  }
})