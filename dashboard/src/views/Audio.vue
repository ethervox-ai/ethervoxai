<template>
  <div class="card">
    <div class="card-header">
      <h2 class="text-xl font-semibold text-secondary-900">Audio & Speech Settings</h2>
    </div>
    <div class="card-body">
      <div class="text-center py-12">
        <MicrophoneIcon class="w-16 h-16 text-primary-500 mx-auto mb-4" />
        <h3 class="text-lg font-medium text-secondary-900 mb-2">
          Audio Configuration Panel
        </h3>
        <p class="text-secondary-600">
          Speech-to-text and text-to-speech settings will be available here.
        </p>
      </div>
    </div>
  </div>
</template>

<script setup>
import { MicrophoneIcon } from '@heroicons/vue/24/outline'
</script>