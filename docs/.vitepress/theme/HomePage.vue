<script setup lang="ts">
import { VPHomeContent, VPHomeFeatures, VPHomeHero } from 'vitepress/theme'
import HomeHero from './HomeHero.vue'
import HomeShot from './HomeShot.vue'
</script>

<template>
  <main class="VPHome">
    <VPHomeHero>
      <template #home-hero-info><HomeHero /></template>
    </VPHomeHero>
    <HomeShot />
    <VPHomeFeatures />
    <VPHomeContent><Content /></VPHomeContent>
  </main>
</template>
