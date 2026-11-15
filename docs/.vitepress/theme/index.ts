import { h } from 'vue'
import DefaultTheme from 'vitepress/theme'
import HomeHero from './HomeHero.vue'
import HomeShot from './HomeShot.vue'
import './custom.css'

export default {
  extends: DefaultTheme,
  // The hero leads with the logo and name; the rig screenshot sits directly under it.
  Layout: () =>
    h(DefaultTheme.Layout, null, {
      'home-hero-info': () => h(HomeHero),
      'home-hero-after': () => h(HomeShot),
    }),
}
