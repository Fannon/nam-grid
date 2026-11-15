<script setup lang="ts">
import grid from '../../screenshots/grid.png'
</script>

<template>
  <figure class="home-shot">
    <a :href="grid" aria-label="View the Clean Default rig screenshot at full size">
      <img :src="grid" width="1080" height="720" fetchpriority="high" alt="NAM Grid with the bundled Clean Default rig and its amp capture selected" />
    </a>
    <figcaption>The included Clean Default rig. Select the screenshot to view it at full size.</figcaption>
  </figure>
</template>
