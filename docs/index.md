---
layout: landing-page
sidebar: false
pageClass: landing-page

hero:
  name: NAM Grid
  text: A guitar rig plugin for Neural Amp Modeler captures, cabinet impulse responses and <a href="./modules/">built-in effects</a>.
  tagline: Free and open source. CLAP and VST3 for Windows, macOS and Linux.
  actions:
    - theme: brand
      text: Download NAM Grid
      link: '#download'
    - theme: alt
      text: Getting started
      link: /getting-started

features:
  - title: Two lanes, in series or parallel
    details: "Sixteen slots in four rows: Pre, lanes A and B, and Post. Run A and B one after the other, or side by side with a blend knob between them. Any slot can hold a capture, a cabinet IR or an effect."
    link: /grid
    linkText: Grid & routing
  - title: Browser with audio preview
    details: Try a capture, cabinet IR or effect in the slot while you play. Apply to keep it, or cancel to restore the previous choice.
    link: /browser
    linkText: Library browser
  - title: 60+ built-in effects
    details: Gates, compressors, EQs, guitar and bass drives, modulation, pitch, delays and reverbs.
    link: /modules/
    linkText: Module guide
  - title: A display on every module
    details: Inspect EQ curves, echo repeats, capture and IR responses. Input and output meters help you match levels when comparing an effect with its bypassed sound.
    link: /ui#displays-and-meters
    linkText: Displays and meters
  - title: Modulation, scenes and a tuner
    details: Modulate controls with LFOs, envelope followers, step sequencers and four macros. Store three scenes per rig and switch between them with one click.
    link: /performance
    linkText: Modulation & scenes
  - title: Rig presets
    details: Save the complete grid and its settings as a rig preset. NAM captures and cabinet IRs stay as separate files.
    link: /presets
    linkText: Saving and loading rigs
---

<script setup>
import nam from './screenshots/nam.png'
import ir from './screenshots/ir.png'
import delay from './screenshots/delay.png'
import browser from './screenshots/browser-modules.png'
import control from './screenshots/control.png'
import tuner from './screenshots/tuner.png'

const shots = [
  { src: nam, title: 'NAM capture', text: 'Input/output curve of the loaded capture' },
  { src: ir, title: 'Cabinet IR', text: 'Frequency response of the loaded impulse response' },
  { src: delay, title: 'Tape Echo', text: 'Timing, tone and level of each repeat' },
  { src: browser, title: 'Module browser', text: 'Try effects in a slot before you apply one' },
  { src: control, title: 'Modulation', text: 'Sources, macros and the modulation matrix' },
  { src: tuner, title: 'Tuner', text: 'Cents, reference pitch and pitch history' },
]
</script>

## Download

Each ZIP contains both CLAP and VST3. Choose your platform, then follow the [installation instructions](getting-started.md).

| Platform | Download |
| --- | --- |
| Windows x64 | [Download for Windows](https://github.com/Fannon/nam-grid/releases/latest/download/nam-grid-windows-x64.zip) |
| macOS Apple Silicon | [Download for macOS](https://github.com/Fannon/nam-grid/releases/latest/download/nam-grid-macos-arm64.zip) |
| Linux x64 | [Download for Linux](https://github.com/Fannon/nam-grid/releases/latest/download/nam-grid-linux-x64.zip) |

[Release notes](release-notes.md) · [All releases on GitHub](https://github.com/Fannon/nam-grid/releases)

Both formats have been tested in Bitwig Studio and REAPER on Windows. macOS and Linux pass automated tests; DAW checks for the current release are still pending. If something does not work in your DAW, please [report it](troubleshooting.md#report-a-problem).

::: warning Beta
NAM Grid is in beta (version 0.x). Projects, automation and presets saved with version 0.1.0 or later keep loading. Improvements to a module's sound or CPU use can change how an existing rig sounds; the [release notes](release-notes.md) describe these changes.
:::

## Sounds to start with

Bring your own NAM captures and cabinet IRs. Add their folders to your [library](browser.md#library-folders). [TONE3000](https://www.tone3000.com/) is one place to find captures and IRs.

<span id="see-it-in-action"></span>

## Screenshots

<div class="shot-gallery">
  <figure v-for="shot in shots" :key="shot.title">
    <a :href="shot.src" :aria-label="'View ' + shot.title + ' screenshot at full size'"><img :src="shot.src" :alt="shot.title + ': ' + shot.text" width="1080" height="720" loading="lazy" decoding="async" /></a>
    <figcaption><strong>{{ shot.title }}</strong><span>{{ shot.text }}</span></figcaption>
  </figure>
</div>

## Credits

NAM Grid is built on [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerPlugin) by Steven Atkinson and the [iPlug2](https://github.com/iPlug2/iPlug2) plugin framework. Many effects are adapted from Airwindows, the Faust reverb library, Signalsmith Audio, Mutable Instruments and other projects; the [acknowledgments](https://github.com/Fannon/nam-grid#acknowledgments) list all of them.

Report bugs and suggest features in [GitHub issues](https://github.com/Fannon/nam-grid/issues). [Troubleshooting](troubleshooting.md) covers common problems.
