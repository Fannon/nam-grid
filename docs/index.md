---
layout: home

hero:
  name: NAM Grid
  text: Your whole guitar rig. In a 4×4 grid.
  tagline: Pair Neural Amp Modeler captures and cabinet IRs with 55 built-in effects, then route them your way. Free and open source, for Windows, macOS and Linux, in CLAP and VST3.
  actions:
    - theme: brand
      text: Download NAM Grid
      link: https://github.com/Fannon/nam-grid/releases/latest
    - theme: alt
      text: Get started
      link: /getting-started
    - theme: alt
      text: Explore the modules
      link: /modules/

features:
  - title: Route it your way
    details: Sixteen slots across Pre, two parallel lanes and Post. Put any capture, cabinet or effect in any slot, and blend the lanes.
    link: /grid
    linkText: See the signal flow
  - title: Audition while you play
    details: Browse captures, cabinet IRs, effects and rigs while you play. Nothing changes until you apply your choice.
    link: /browser
    linkText: Browse your library
  - title: 55 effects built in
    details: Gates, compressors, EQs, guitar and bass drives, modulation, pitch, delays and reverbs. No extra plugins needed.
    link: /modules/
    linkText: Browse the effects
  - title: See what you hear
    details: Every module has a display, from EQ curves to echo patterns, and level meters for fair before-and-after comparisons.
    link: /ui#displays-and-meters
    linkText: Read the displays
  - title: Made for playing live
    details: A tuner, an LFO, envelope follower, step sequencer and four macros, plus three scenes to switch sounds with one click.
    link: /performance
    linkText: Modulation & scenes
  - title: Yours to keep
    details: Free and open source, with a clean capture and starter rigs included. No account or activation required.
    link: https://github.com/Fannon/nam-grid
    linkText: View the source
---

<script setup>
import nam from './screenshots/nam.png'
import ir from './screenshots/ir.png'
import delay from './screenshots/delay.png'
import browser from './screenshots/browser-modules.png'
import control from './screenshots/control.png'
import tuner from './screenshots/tuner.png'

const shots = [
  { src: nam, title: 'NAM capture', text: 'The measured input/output curve of the loaded capture' },
  { src: ir, title: 'Cabinet IR', text: 'The frequency response of the loaded impulse response' },
  { src: delay, title: 'Tape Echo', text: 'Echo pattern, tone and level of every repeat' },
  { src: browser, title: 'Module browser', text: 'Audition effects in a slot while you browse' },
  { src: control, title: 'Modulation', text: 'Sources, macros and the modulation matrix' },
  { src: tuner, title: 'Tuner', text: 'Cents, reference pitch and pitch history' },
]
</script>

::: warning Beta release
NAM Grid is a beta (0.x) release. Preset and automation stability are goals, but we reserve the option to improve sound quality or performance in ways that may change the sonic results of existing projects and presets.
:::

## Pick your platform

Each release has one archive per platform, with both **CLAP and VST3** inside.

| Platform | Download |
| --- | --- |
| Windows x64 | `nam-grid-windows-x64.zip` |
| macOS Apple Silicon | `nam-grid-macos-arm64.zip` |
| Linux x64 | `nam-grid-linux-x64.zip` |

[Download the latest release](https://github.com/Fannon/nam-grid/releases/latest) · [Requirements and installation](getting-started.md)

The Linux build has not been tested in a real DAW environment. If you use it, let me know how well it works.

You can play right away: NAM Grid includes the **RiffSmith Clean** amp-and-cab capture and the **Clean Default** rig. When you want more, [TONE3000](https://www.tone3000.com/) hosts a large collection of free NAM captures and cabinet IRs to add to your [library](browser.md).

## See it in action

<div class="shot-gallery">
  <figure v-for="shot in shots" :key="shot.title">
    <a :href="shot.src" target="_blank" rel="noopener"><img :src="shot.src" :alt="shot.title + ': ' + shot.text" loading="lazy" /></a>
    <figcaption><strong>{{ shot.title }}</strong><span>{{ shot.text }}</span></figcaption>
  </figure>
</div>

## Built on open source

NAM Grid brings together [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerPlugin) by Steven Atkinson, the [iPlug2](https://github.com/iPlug2/iPlug2) plugin framework, and effects adapted from Airwindows, the Faust reverb library, Signalsmith Audio, Mutable Instruments and others. See the [acknowledgments](https://github.com/Fannon/nam-grid#acknowledgments) for every project it builds on.

Found a bug or have an idea? [Open an issue](https://github.com/Fannon/nam-grid/issues) or read [troubleshooting](troubleshooting.md) first.
