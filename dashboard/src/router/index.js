/**
 * @file index.js
 * @brief Router Vue.js application entry point for EthervoxAI Dashboard
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

import { createRouter, createWebHistory } from 'vue-router'
import DashboardLayout from '../layouts/DashboardLayout.vue'
import Overview from '../views/Overview.vue'
import Audio from '../views/Audio.vue'
import Privacy from '../views/Privacy.vue'
import Languages from '../views/Languages.vue'
import Plugins from '../views/Plugins.vue'
import Settings from '../views/Settings.vue'

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    {
      path: '/',
      component: DashboardLayout,
      children: [
        {
          path: '',
          name: 'overview',
          component: Overview,
          meta: {
            title: 'Overview'
          }
        },
        {
          path: '/audio',
          name: 'audio',
          component: Audio,
          meta: {
            title: 'Audio & Speech'
          }
        },
        {
          path: '/privacy',
          name: 'privacy',
          component: Privacy,
          meta: {
            title: 'Privacy & Data'
          }
        },
        {
          path: '/languages',
          name: 'languages',
          component: Languages,
          meta: {
            title: 'Languages'
          }
        },
        {
          path: '/plugins',
          name: 'plugins',
          component: Plugins,
          meta: {
            title: 'Plugins & LLMs'
          }
        },
        {
          path: '/settings',
          name: 'settings',
          component: Settings,
          meta: {
            title: 'System Settings'
          }
        }
      ]
    }
  ]
})

// Update document title on route change
router.beforeEach((to) => {
  document.title = `${to.meta.title} - EthervoxAI Dashboard`
})

export default router