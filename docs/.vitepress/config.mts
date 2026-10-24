import { defineConfig } from 'vitepress'

export default defineConfig({
  title: 'NAM Grid',
  description:
    'Open-source guitar rig in one CLAP/VST3 plugin: NAM captures, cab IRs and effects on a 16-slot grid',
  base: '/nam-grid/',
  // Dark theme by default (like the plugin editor); readers can switch to light.
  appearance: { initialValue: 'dark' },
  ignoreDeadLinks: true,
  // Directory indexes live in README.md files (GitHub convention); serve them as the section root.
  rewrites: {
    'modules/README.md': 'modules/index.md',
  },
  head: [
    ['link', { rel: 'preconnect', href: 'https://fonts.googleapis.com' }],
    ['link', { rel: 'preconnect', href: 'https://fonts.gstatic.com', crossorigin: '' }],
    [
      'link',
      {
        rel: 'stylesheet',
        href: 'https://fonts.googleapis.com/css2?family=Roboto:ital,wght@0,400;0,500;0,700;1,400&display=swap',
      },
    ],
  ],
  themeConfig: {
    siteTitle: 'NAM Grid',
    nav: [
      { text: 'Overview', link: '/' },
      { text: 'Modules', link: '/modules/' },
      { text: 'All guides', link: '/README' },
      { text: 'Download', link: 'https://github.com/Fannon/nam-grid/releases/latest' },
    ],
    sidebar: [
      {
        text: 'Start here',
        items: [
          { text: 'Overview', link: '/' },
          { text: 'All guides', link: '/README' },
          { text: 'Grid & routing', link: '/grid' },
          { text: 'Editor & library', link: '/ui' },
          { text: 'Library browser', link: '/browser' },
          { text: 'Presets', link: '/presets' },
          { text: 'Performance controls', link: '/performance' },
          { text: 'Starting settings', link: '/effect-defaults' },
          { text: 'Tuner', link: '/tuner' },
        ],
      },
      {
        text: 'Modules',
        items: [
          { text: 'Module guide', link: '/modules/' },
          { text: 'Captures & cabinets', link: '/modules/captures' },
          { text: 'Distortion', link: '/modules/distortion' },
          { text: 'Dynamics', link: '/modules/dynamics' },
          { text: 'EQ / Filter', link: '/modules/eq-filter' },
          { text: 'Modulation', link: '/modules/modulation' },
          { text: 'Pitch / Frequency', link: '/modules/pitch-frequency' },
          { text: 'Delay', link: '/modules/delay' },
          { text: 'Reverb', link: '/modules/reverb' },
          { text: 'Creative', link: '/modules/creative' },
          { text: 'Utility / Volume', link: '/modules/utility-volume' },
        ],
      },
      {
        text: 'Deep dives',
        items: [
          { text: 'Noise Gate (Smart)', link: '/noise-gate-smart' },
          { text: 'LoFi Tape Machine', link: '/lofi-tape' },
          { text: 'Cosmic Echo', link: '/cosmic-echo' },
          { text: 'Module implementation', link: '/module-implementation' },
          { text: 'Adaptive cabinet convolution', link: '/adaptive-ir' },
        ],
      },
      {
        text: 'Contributing',
        items: [
          { text: 'Build & test', link: '/build' },
          { text: 'Development', link: '/development' },
          { text: 'Release checklist', link: '/release-checklist' },
          { text: 'Release notes', link: '/release-notes' },
        ],
      },
    ],
    socialLinks: [{ icon: 'github', link: 'https://github.com/Fannon/nam-grid' }],
    footer: {
      message: 'Free and open source. Keep ThirdPartyNotices.txt with the plugins when redistributing.',
      copyright: 'Copyright 2026 Simon Heimler',
    },
    search: { provider: 'local' },
  },
})
