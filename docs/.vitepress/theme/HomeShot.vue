<script setup lang="ts">
import { withBase } from 'vitepress'
import grid from '../../screenshots/grid.png'
</script>

<template>
  <div class="home-shot">
    <div class="plugin-window">
      <div class="plugin-window-bar"><span></span><span></span><span></span><em>NAM Grid · Clean Default</em></div>
      <img :src="grid" width="1000" alt="NAM Grid with the bundled Clean Default rig and its amp capture selected" />
    </div>
    <div class="quick-start">
      <span class="quick-start-label">First sound</span>
      <ol>
        <li><strong>Download</strong> for your system</li>
        <li><strong>Install</strong> CLAP or VST3</li>
        <li><strong>Load</strong> Clean Default and play</li>
      </ol>
      <a class="quick-start-link" :href="withBase('/getting-started')">Getting started guide&nbsp;&rarr;</a>
    </div>
  </div>
</template>
