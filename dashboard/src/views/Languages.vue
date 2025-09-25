<template>
  <div class="card">
    <div class="card-header">
      <h2 class="text-xl font-semibold text-secondary-900">Language Support</h2>
    </div>
    <div class="card-body">
      <div class="text-center py-12">
        <LanguageIcon class="w-16 h-16 text-primary-500 mx-auto mb-4" />
        <h3 class="text-lg font-medium text-secondary-900 mb-2">
          Multilingual Configuration
        </h3>
        <p class="text-secondary-600">
          Language model and translation settings will be available here.
        </p>
      </div>
    </div>
  </div>
</template>

<script setup>
import { LanguageIcon } from '@heroicons/vue/24/outline'
</script>