/**
 * @file Overview.vue
 * @brief Dashboard component for EthervoxAI
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
<template>
  <div class="space-y-6">
    <!-- System Health Overview -->
    <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
      <div class="card">
        <div class="card-body">
          <div class="flex items-center">
            <div class="flex-shrink-0">
              <div
                class="w-8 h-8 rounded-full flex items-center justify-center"
                :class="healthIndicatorBg"
              >
                <component :is="healthIcon" class="w-5 h-5 text-white" />
              </div>
            </div>
            <div class="ml-4">
              <p class="text-sm font-medium text-secondary-600">System Health</p>
              <p class="text-lg font-semibold capitalize" :class="healthTextColor">
                {{ systemStore.systemHealth }}
              </p>
            </div>
          </div>
        </div>
      </div>
      
      <div class="card">
        <div class="card-body">
          <div class="flex items-center">
            <div class="flex-shrink-0">
              <div class="w-8 h-8 bg-primary-500 rounded-full flex items-center justify-center">
                <ClockIcon class="w-5 h-5 text-white" />
              </div>
            </div>
            <div class="ml-4">
              <p class="text-sm font-medium text-secondary-600">Uptime</p>
              <p class="text-lg font-semibold text-secondary-900">
                {{ systemStore.uptimeFormatted }}
              </p>
            </div>
          </div>
        </div>
      </div>
      
      <div class="card">
        <div class="card-body">
          <div class="flex items-center">
            <div class="flex-shrink-0">
              <div class="w-8 h-8 bg-success-500 rounded-full flex items-center justify-center">
                <MicrophoneIcon class="w-5 h-5 text-white" />
              </div>
            </div>
            <div class="ml-4">
              <p class="text-sm font-medium text-secondary-600">Audio Status</p>
              <p class="text-lg font-semibold capitalize" :class="audioStatusColor">
                {{ systemStore.audioStatus }}
              </p>
            </div>
          </div>
        </div>
      </div>
      
      <div class="card">
        <div class="card-body">
          <div class="flex items-center">
            <div class="flex-shrink-0">
              <div class="w-8 h-8 bg-warning-500 rounded-full flex items-center justify-center">
                <ChatBubbleBottomCenterTextIcon class="w-5 h-5 text-white" />
              </div>
            </div>
            <div class="ml-4">
              <p class="text-sm font-medium text-secondary-600">Queries Processed</p>
              <p class="text-lg font-semibold text-secondary-900">
                {{ systemStore.processedQueries.toLocaleString() }}
              </p>
            </div>
          </div>
        </div>
      </div>
    </div>
    
    <!-- Performance Metrics -->
    <div class="grid grid-cols-1 lg:grid-cols-2 gap-6">
      <div class="card">
        <div class="card-header">
          <h3 class="text-lg font-medium text-secondary-900">System Performance</h3>
        </div>
        <div class="card-body">
          <div class="space-y-6">
            <!-- CPU Usage -->
            <div>
              <div class="flex justify-between mb-2">
                <span class="text-sm font-medium text-secondary-700">CPU Usage</span>
                <span class="text-sm text-secondary-600">{{ systemStore.cpuUsage }}%</span>
              </div>
              <div class="w-full bg-secondary-200 rounded-full h-2">
                <div
                  class="h-2 rounded-full transition-all duration-300"
                  :class="cpuUsageColor"
                  :style="{ width: `${systemStore.cpuUsage}%` }"
                ></div>
              </div>
            </div>
            
            <!-- Memory Usage -->
            <div>
              <div class="flex justify-between mb-2">
                <span class="text-sm font-medium text-secondary-700">Memory Usage</span>
                <span class="text-sm text-secondary-600">{{ systemStore.memoryUsage }}%</span>
              </div>
              <div class="w-full bg-secondary-200 rounded-full h-2">
                <div
                  class="h-2 rounded-full transition-all duration-300"
                  :class="memoryUsageColor"
                  :style="{ width: `${systemStore.memoryUsage}%` }"
                ></div>
              </div>
            </div>
            
            <!-- Average Response Time -->
            <div class="flex justify-between items-center">
              <span class="text-sm font-medium text-secondary-700">Avg Response Time</span>
              <span class="text-sm font-semibold text-secondary-900">
                {{ systemStore.averageResponseTime }}ms
              </span>
            </div>
          </div>
        </div>
      </div>
      
      <!-- Privacy Status -->
      <div class="card">
        <div class="card-header">
          <h3 class="text-lg font-medium text-secondary-900">Privacy Status</h3>
        </div>
        <div class="card-body">
          <div class="space-y-4">
            <div class="flex items-center justify-between">
              <span class="text-sm font-medium text-secondary-700">Local Processing</span>
              <span
                class="inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium"
                :class="systemStore.localProcessingEnabled 
                  ? 'bg-success-100 text-success-800' 
                  : 'bg-danger-100 text-danger-800'"
              >
                {{ systemStore.localProcessingEnabled ? 'Enabled' : 'Disabled' }}
              </span>
            </div>
            
            <div class="flex items-center justify-between">
              <span class="text-sm font-medium text-secondary-700">Cloud Queries</span>
              <span
                class="inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium"
                :class="systemStore.cloudQueriesEnabled 
                  ? 'bg-warning-100 text-warning-800' 
                  : 'bg-success-100 text-success-800'"
              >
                {{ systemStore.cloudQueriesEnabled ? 'Enabled' : 'Disabled' }}
              </span>
            </div>
            
            <div class="flex items-center justify-between">
              <span class="text-sm font-medium text-secondary-700">Data Retention</span>
              <span class="text-sm text-secondary-600">
                {{ systemStore.dataRetentionDays }} days
              </span>
            </div>
            
            <div class="flex items-center justify-between">
              <span class="text-sm font-medium text-secondary-700">Audit Logging</span>
              <span
                class="inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium"
                :class="systemStore.auditLogEnabled 
                  ? 'bg-success-100 text-success-800' 
                  : 'bg-danger-100 text-danger-800'"
              >
                {{ systemStore.auditLogEnabled ? 'Enabled' : 'Disabled' }}
              </span>
            </div>
          </div>
        </div>
      </div>
    </div>
    
    <!-- Recent Activity & Errors -->
    <div class="grid grid-cols-1 lg:grid-cols-2 gap-6">
      <!-- Recent Errors -->
      <div class="card" v-if="systemStore.errors.length > 0">
        <div class="card-header">
          <div class="flex items-center justify-between">
            <h3 class="text-lg font-medium text-secondary-900">Recent Errors</h3>
            <button
              @click="systemStore.clearErrors"
              class="text-sm text-danger-600 hover:text-danger-900"
            >
              Clear All
            </button>
          </div>
        </div>
        <div class="card-body">
          <div class="space-y-3 max-h-64 overflow-y-auto">
            <div
              v-for="error in systemStore.errors.slice(0, 5)"
              :key="error.id"
              class="flex items-start space-x-3 p-3 bg-danger-50 rounded-md"
            >
              <ExclamationTriangleIcon class="w-5 h-5 text-danger-500 mt-0.5" />
              <div class="flex-1 min-w-0">
                <p class="text-sm text-danger-800">{{ error.message }}</p>
                <p class="text-xs text-danger-600 mt-1">
                  {{ new Date(error.timestamp).toLocaleString() }}
                </p>
              </div>
            </div>
          </div>
        </div>
      </div>
      
      <!-- System Information -->
      <div class="card">
        <div class="card-header">
          <h3 class="text-lg font-medium text-secondary-900">System Information</h3>
        </div>
        <div class="card-body">
          <div class="space-y-3">
            <div class="flex justify-between">
              <span class="text-sm font-medium text-secondary-700">Version</span>
              <span class="text-sm text-secondary-600">{{ systemStore.version }}</span>
            </div>
            <div class="flex justify-between">
              <span class="text-sm font-medium text-secondary-700">Platform</span>
              <span class="text-sm text-secondary-600 capitalize">{{ systemStore.platform }}</span>
            </div>
            <div class="flex justify-between">
              <span class="text-sm font-medium text-secondary-700">Current Language</span>
              <span class="text-sm text-secondary-600">{{ languageDisplay }}</span>
            </div>
            <div class="flex justify-between">
              <span class="text-sm font-medium text-secondary-700">Supported Languages</span>
              <span class="text-sm text-secondary-600">{{ systemStore.supportedLanguages.length }}</span>
            </div>
            <div class="flex justify-between">
              <span class="text-sm font-medium text-secondary-700">Last Update</span>
              <span class="text-sm text-secondary-600">
                {{ lastUpdateDisplay }}
              </span>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useSystemStore } from '../stores/system'
import {
  CheckCircleIcon,
  ExclamationTriangleIcon,
  XCircleIcon,
  ClockIcon,
  MicrophoneIcon,
  ChatBubbleBottomCenterTextIcon
} from '@heroicons/vue/24/outline'

const systemStore = useSystemStore()

// Computed properties for UI state
const healthIcon = computed(() => {
  switch (systemStore.systemHealth) {
    case 'healthy':
      return CheckCircleIcon
    case 'warning':
      return ExclamationTriangleIcon
    case 'critical':
      return XCircleIcon
    default:
      return ExclamationTriangleIcon
  }
})

const healthIndicatorBg = computed(() => {
  switch (systemStore.systemHealth) {
    case 'healthy':
      return 'bg-success-500'
    case 'warning':
      return 'bg-warning-500'
    case 'critical':
      return 'bg-danger-500'
    default:
      return 'bg-secondary-400'
  }
})

const healthTextColor = computed(() => {
  switch (systemStore.systemHealth) {
    case 'healthy':
      return 'text-success-700'
    case 'warning':
      return 'text-warning-700'
    case 'critical':
      return 'text-danger-700'
    default:
      return 'text-secondary-700'
  }
})

const audioStatusColor = computed(() => {
  switch (systemStore.audioStatus) {
    case 'online':
      return 'text-success-700'
    case 'offline':
      return 'text-danger-700'
    default:
      return 'text-secondary-700'
  }
})

const cpuUsageColor = computed(() => {
  if (systemStore.cpuUsage > 80) return 'bg-danger-500'
  if (systemStore.cpuUsage > 60) return 'bg-warning-500'
  return 'bg-success-500'
})

const memoryUsageColor = computed(() => {
  if (systemStore.memoryUsage > 85) return 'bg-danger-500'
  if (systemStore.memoryUsage > 70) return 'bg-warning-500'
  return 'bg-primary-500'
})

const languageDisplay = computed(() => {
  const langMap = {
    'en': 'English',
    'es': 'Español',
    'zh': '中文'
  }
  return langMap[systemStore.currentLanguage] || systemStore.currentLanguage
})

const lastUpdateDisplay = computed(() => {
  if (!systemStore.lastUpdateTime) return 'Never'
  return new Date(systemStore.lastUpdateTime).toLocaleTimeString()
})
</script>