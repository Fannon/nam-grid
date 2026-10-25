import { h } from 'vue'
import DefaultTheme from 'vitepress/theme'
import HomeShot from './HomeShot.vue'
import './custom.css'

export default {
  extends: DefaultTheme,
  // The rig screenshot is the strongest first impression, so it sits directly under the hero.
  Layout: () => h(DefaultTheme.Layout, null, { 'home-hero-after': () => h(HomeShot) }),
}
