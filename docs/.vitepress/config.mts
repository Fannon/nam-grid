import path from 'node:path'
import { defineConfig } from 'vitepress'

const repoBlob = 'https://github.com/Fannon/nam-grid/blob/main/'
// Directory indexes live in README.md files (GitHub convention); serve them as the section root.
const rewrites: Record<string, string> = { 'modules/README.md': 'modules/index.md' }

export default defineConfig({
  title: 'NAM Grid',
  description:
    'Open-source guitar rig in one CLAP/VST3 plugin: NAM captures, cab IRs and effects on a 16-slot grid',
  base: '/nam-grid/',
  // Match the plugin on first visit; readers can switch to light.
  appearance: { initialValue: 'dark' },
  rewrites,
  markdown: {
    languageAlias: { gitignore: 'shellscript' },
    // The guides also read well on GitHub, so they link files relatively. On the site, send links
    // to source files (../NeuralAmpModeler/...) to GitHub and links to rewritten pages to their route.
    config(md) {
      md.core.ruler.after('inline', 'repo-links', (state) => {
        const source = state.env.relativePath as string | undefined
        if (!source) return
        for (const block of state.tokens) {
          for (const token of block.children ?? []) {
            if (token.type !== 'link_open') continue
            const href = token.attrGet('href')
            if (!href || /^([a-z]+:|#|\/)/i.test(href)) continue
            const [file, hash] = href.split('#')
            const target = path.posix.normalize(path.posix.join('docs', path.posix.dirname(source), file))
            const suffix = hash === undefined ? '' : '#' + hash
            if (!target.startsWith('docs/')) token.attrSet('href', repoBlob + target + suffix)
            else if (rewrites[target.slice(5)]) {
              const routed = path.posix.relative(path.posix.dirname(source), rewrites[target.slice(5)])
              token.attrSet('href', (routed.startsWith('.') ? routed : './' + routed) + suffix)
            }
          }
        }
      })
    },
  },
  head: [
    // head entries are not rebased automatically, so they carry the base path.
    ['link', { rel: 'icon', type: 'image/svg+xml', href: '/nam-grid/logo.svg' }],
    ['link', { rel: 'apple-touch-icon', href: '/nam-grid/apple-touch-icon.png' }],
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
    logo: '/logo.svg',
    siteTitle: 'NAM Grid',
    nav: [
      {
        text: 'Guide',
        link: '/getting-started',
        activeMatch: '^/(getting-started|ui|grid|browser|presets|tuner|performance)',
      },
      {
        text: 'Modules',
        link: '/modules/',
        activeMatch: '^/modules/',
      },
      { text: 'Help', link: '/troubleshooting', activeMatch: '^/(troubleshooting|release-notes)' },
      { text: 'Download', link: 'https://github.com/Fannon/nam-grid/releases/latest' },
    ],
    sidebar: [
      {
        text: 'Getting started',
        link: '/getting-started',
      },
      {
        text: 'Use NAM Grid',
        items: [
          { text: 'Editor', link: '/ui' },
          { text: 'Grid & routing', link: '/grid' },
          { text: 'Library browser', link: '/browser' },
          { text: 'Rig presets', link: '/presets' },
          { text: 'Tuner', link: '/tuner' },
          { text: 'Modulation & scenes', link: '/performance' },
        ],
      },
      {
        text: 'Module guide',
        link: '/modules/',
        items: [
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
        text: 'Module deep dives',
        items: [
          { text: 'Pedal Drive', link: '/modules/pedal-drive' },
          { text: 'Noise Gate (Smart)', link: '/modules/noise-gate-smart' },
          { text: 'LoFi Tape Machine', link: '/modules/lofi-tape' },
          { text: 'Cosmic Echo', link: '/modules/cosmic-echo' },
          { text: 'Cabinet IR processing', link: '/modules/ir-processing' },
        ],
      },
      {
        text: 'Help',
        items: [
          { text: 'Troubleshooting & FAQ', link: '/troubleshooting' },
          { text: 'Release notes', link: '/release-notes' },
        ],
      },
      {
        text: 'Build & contribute',
        collapsed: true,
        items: [
          { text: 'All documentation', link: '/README' },
          { text: 'Build & test', link: '/build' },
          { text: 'Development', link: '/development' },
          { text: 'Module implementation', link: '/module-implementation' },
          { text: 'Release checklist', link: '/release-checklist' },
          { text: 'FX benchmarks', link: '/fx-performance' },
          { text: 'DSP aliasing checks', link: '/dsp-aliasing' },
          { text: 'IR convolution experiment', link: '/ir-convolution-comparison' },
          { text: 'Airwindows outboard', link: '/airwindows-outboard' },
          { text: 'Digital Black Gate', link: '/airwindows-dynamics' },
          { text: 'Screenshots', link: '/screenshots/README' },
        ],
      },
    ],
    socialLinks: [{ icon: 'github', link: 'https://github.com/Fannon/nam-grid' }],
    footer: {
      message:
        'Free and <a href="https://github.com/Fannon/nam-grid">open source</a>. Keep ' +
        '<a href="https://github.com/Fannon/nam-grid/blob/main/ThirdPartyNotices.txt">ThirdPartyNotices.txt</a> ' +
        'with the plugins when redistributing.',
      copyright: 'Copyright 2026 Simon Heimler',
    },
    search: { provider: 'local' },
  },
})
