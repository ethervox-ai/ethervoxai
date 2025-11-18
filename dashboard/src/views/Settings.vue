/**
 * @file Settings.vue
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
  <div class="card">
    <div class="card-header">
      <h2 class="text-xl font-semibold text-secondary-900">System Settings</h2>
    </div>
    <div class="card-body">
      <div class="text-center py-12">
        <CogIcon class="w-16 h-16 text-primary-500 mx-auto mb-4" />
        <h3 class="text-lg font-medium text-secondary-900 mb-2">
          System Configuration
        </h3>
        <p class="text-secondary-600">
          Advanced system settings and diagnostics will be available here.
        </p>
      </div>
    </div>
  </div>
</template>

<script setup>
import { CogIcon } from '@heroicons/vue/24/outline'
</script>