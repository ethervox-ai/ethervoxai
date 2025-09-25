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