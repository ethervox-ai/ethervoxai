/**
 * @file App.vue
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
  <div id="app" class="min-h-screen bg-secondary-50">
    <RouterView />
  </div>
</template>

<script setup>
import { RouterView } from 'vue-router'
import { onMounted } from 'vue'
import { useSystemStore } from './stores/system'

const systemStore = useSystemStore()

onMounted(() => {
  // Initialize system status monitoring
  systemStore.initializeSystem()
})
</script>