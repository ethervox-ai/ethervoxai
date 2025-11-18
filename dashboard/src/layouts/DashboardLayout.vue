/**
 * @file DashboardLayout.vue
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
  <div class="min-h-screen bg-secondary-50">
    <!-- Sidebar -->
    <div class="fixed inset-y-0 left-0 w-64 bg-white shadow-lg border-r border-secondary-200 z-30">
      <!-- Logo -->
      <div class="flex items-center h-16 px-6 border-b border-secondary-200">
        <div class="flex items-center">
          <div class="w-8 h-8 bg-primary-600 rounded-md flex items-center justify-center">
            <span class="text-white font-bold text-lg">E</span>
          </div>
          <span class="ml-3 text-xl font-semibold text-secondary-900">EthervoxAI</span>
        </div>
      </div>
      
      <!-- Navigation -->
      <nav class="mt-6">
        <div class="px-3">
          <RouterLink
            v-for="item in navigationItems"
            :key="item.name"
            :to="item.href"
            class="group flex items-center px-3 py-2 text-sm font-medium rounded-md transition-colors duration-150"
            :class="[
              $route.path === item.href
                ? 'bg-primary-50 text-primary-700 border-r-2 border-primary-600'
                : 'text-secondary-600 hover:text-secondary-900 hover:bg-secondary-50'
            ]"
          >
            <component
              :is="item.icon"
              class="mr-3 h-5 w-5 flex-shrink-0"
              :class="[
                $route.path === item.href
                  ? 'text-primary-500'
                  : 'text-secondary-400 group-hover:text-secondary-500'
              ]"
            />
            {{ item.name }}
            <span
              v-if="item.badge"
              class="ml-auto inline-block py-0.5 px-2 text-xs rounded-full"
              :class="item.badgeClass"
            >
              {{ item.badge }}
            </span>
          </RouterLink>
        </div>
        
        <!-- System Status -->
        <div class="mt-8 px-3">
          <div class="bg-secondary-50 rounded-lg p-4">
            <div class="flex items-center justify-between mb-2">
              <span class="text-sm font-medium text-secondary-700">System Status</span>
              <span
                class="inline-flex items-center px-2 py-1 rounded-full text-xs font-medium"
                :class="systemStatusClass"
              >
                <span class="w-2 h-2 rounded-full mr-1.5" :class="systemStatusDotClass"></span>
                {{ systemStore.status }}
              </span>
            </div>
            <div class="text-xs text-secondary-500">
              Uptime: {{ systemStore.uptimeFormatted }}
            </div>
          </div>
        </div>
      </nav>
    </div>
    
    <!-- Main Content -->
    <div class="pl-64">
      <!-- Top Header -->
      <header class="bg-white shadow-sm border-b border-secondary-200">
        <div class="px-6 py-4">
          <div class="flex items-center justify-between">
            <h1 class="text-2xl font-semibold text-secondary-900">
              {{ $route.meta.title }}
            </h1>
            
            <!-- Header Actions -->
            <div class="flex items-center space-x-4">
              <!-- Language Selector -->
              <select
                v-model="systemStore.currentLanguage"
                class="form-input text-sm py-1"
                @change="updateLanguage"
              >
                <option value="en">English</option>
                <option value="es">Español</option>
                <option value="zh">中文</option>
              </select>
              
              <!-- System Health Indicator -->
              <div class="flex items-center">
                <div
                  class="w-3 h-3 rounded-full mr-2"
                  :class="healthIndicatorClass"
                ></div>
                <span class="text-sm text-secondary-600 capitalize">
                  {{ systemStore.systemHealth }}
                </span>
              </div>
            </div>
          </div>
        </div>
      </header>
      
      <!-- Page Content -->
      <main class="p-6">
        <RouterView />
      </main>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { RouterLink, RouterView } from 'vue-router'
import { useSystemStore } from '../stores/system'
import {
  HomeIcon,
  MicrophoneIcon,
  ShieldCheckIcon,
  LanguageIcon,
  PuzzlePieceIcon,
  CogIcon
} from '@heroicons/vue/24/outline'

const systemStore = useSystemStore()

const navigationItems = [
  { name: 'Overview', href: '/', icon: HomeIcon },
  { name: 'Audio & Speech', href: '/audio', icon: MicrophoneIcon },
  { name: 'Privacy & Data', href: '/privacy', icon: ShieldCheckIcon },
  { name: 'Languages', href: '/languages', icon: LanguageIcon },
  { 
    name: 'Plugins & LLMs', 
    href: '/plugins', 
    icon: PuzzlePieceIcon,
    badge: systemStore.cloudQueriesEnabled ? 'Cloud' : 'Local',
    badgeClass: systemStore.cloudQueriesEnabled 
      ? 'bg-warning-100 text-warning-800'
      : 'bg-success-100 text-success-800'
  },
  { name: 'Settings', href: '/settings', icon: CogIcon }
]

const systemStatusClass = computed(() => {
  switch (systemStore.status) {
    case 'online':
      return 'bg-success-100 text-success-800'
    case 'offline':
      return 'bg-danger-100 text-danger-800'
    case 'degraded':
      return 'bg-warning-100 text-warning-800'
    default:
      return 'bg-secondary-100 text-secondary-800'
  }
})

const systemStatusDotClass = computed(() => {
  switch (systemStore.status) {
    case 'online':
      return 'bg-success-500'
    case 'offline':
      return 'bg-danger-500'
    case 'degraded':
      return 'bg-warning-500'
    default:
      return 'bg-secondary-500'
  }
})

const healthIndicatorClass = computed(() => {
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

async function updateLanguage() {
  try {
    await systemStore.updateAudioSettings({
      currentLanguage: systemStore.currentLanguage
    })
  } catch (error) {
    console.error('Failed to update language:', error)
  }
}
</script>