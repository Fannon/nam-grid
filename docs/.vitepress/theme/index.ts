import type { Theme } from 'vitepress'
import DefaultTheme from 'vitepress/theme'
import HomePage from './HomePage.vue'
import './custom.css'

export default {
  extends: DefaultTheme,
  enhanceApp({ app }) {
    // Keep VitePress's public homepage components inside one native main landmark.
    app.component('landing-page', HomePage)
  },
} satisfies Theme
