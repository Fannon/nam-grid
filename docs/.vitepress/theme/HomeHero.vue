<script setup lang="ts">
import { useData, withBase } from 'vitepress'

// Replaces the default hero heading: the plugin's logo and name as the title, then the
// frontmatter text and tagline. The default hero still renders the action buttons.
const { frontmatter } = useData()
</script>

<template>
  <div class="nam-hero">
    <h1 class="nam-hero-title">
      <img :src="withBase('/logo.svg')" alt="" width="64" height="64" />
      <span>{{ frontmatter.hero.name }}</span>
    </h1>
    <p v-if="frontmatter.hero.text" class="nam-hero-text" v-html="frontmatter.hero.text"></p>
    <p v-if="frontmatter.hero.tagline" class="nam-hero-tagline" v-html="frontmatter.hero.tagline"></p>
  </div>
</template>
