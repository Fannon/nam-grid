---
layout: home

hero:
  name: NAM Grid
  text: A guitar rig plugin for Neural Amp Modeler captures, cabinet IRs and <a href="./modules/">60+ built-in effects</a>.
  tagline: Place them in 16 slots and run two lanes in series or in parallel. Free and open source, for Windows, macOS and Linux, as CLAP and VST3.
  actions:
    - theme: brand
      text: Download
      link: https://github.com/Fannon/nam-grid/releases/latest
    - theme: alt
      text: Getting started
      link: /getting-started

features:
  - title: Two lanes, in series or parallel
    details: "Sixteen slots in four rows: Pre, lanes A and B, and Post. Run A and B one after the other, or side by side with a blend knob between them. Any slot can hold a capture, a cabinet IR or an effect."
    link: /grid
    linkText: Grid & routing
  - title: Hear it before you apply it
    details: Select a capture, cabinet IR or effect in the browser and you hear it in the slot as you play. Apply to keep it, or cancel to go back to what was there.
    link: /browser
    linkText: Library browser
  - title: 60+ built-in effects
    details: Gates, compressors, EQs, guitar and bass drives, modulation, pitch, delays and reverbs.
    link: /modules/
    linkText: Module guide
  - title: A display on every module
    details: EQ curves, echo repeats, capture and IR responses, and input and output meters, so you can match levels before you compare with the module bypassed.
    link: /ui#displays-and-meters
    linkText: Displays and meters
  - title: Modulation, scenes and a tuner
    details: Move knobs with two LFOs, two envelope followers, two step sequencers or four macros. Store three scenes per rig and switch between them with one click.
    link: /performance
    linkText: Modulation & scenes
  - title: Free and open source
    details: No account or activation. A clean amp capture and starter rigs are included, so you can play right after installing.
    link: https://github.com/Fannon/nam-grid
    linkText: Source on GitHub
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

Each release has one archive per platform. Every archive contains the CLAP and the VST3 plugin.

| Platform | Archive |
| --- | --- |
| Windows x64 | `nam-grid-windows-x64.zip` |
| macOS Apple Silicon | `nam-grid-macos-arm64.zip` |
| Linux x64 | `nam-grid-linux-x64.zip` |

[Latest release on GitHub](https://github.com/Fannon/nam-grid/releases/latest) · [Requirements and installation](getting-started.md)

NAM Grid is tested in Bitwig Studio on Windows and macOS, as CLAP and VST3. The Linux build passes the automated tests but has not been tried in a DAW yet. If something does not work in your DAW, please [report it](troubleshooting.md#report-a-problem).

::: warning Beta
NAM Grid is in beta (version 0.x). Projects, automation and presets from earlier versions keep loading, but an update that improves a module's sound or CPU use can change how an existing rig sounds. The [release notes](release-notes.md) list these changes.
:::

## Sounds to start with

NAM Grid includes the **RiffSmith Clean** amp-and-cabinet capture and the **Clean Default** rig, so you can play right away. [TONE3000](https://www.tone3000.com/) has a large collection of free NAM captures and cabinet IRs; add the folders you download to your [library](browser.md#library-folders).

## Screenshots

<div class="shot-gallery">
  <figure v-for="shot in shots" :key="shot.title">
    <a :href="shot.src" target="_blank" rel="noopener"><img :src="shot.src" :alt="shot.title + ': ' + shot.text" loading="lazy" /></a>
    <figcaption><strong>{{ shot.title }}</strong><span>{{ shot.text }}</span></figcaption>
  </figure>
</div>

## Credits

NAM Grid is built on [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerPlugin) by Steven Atkinson and the [iPlug2](https://github.com/iPlug2/iPlug2) plugin framework. Many effects are adapted from Airwindows, the Faust reverb library, Signalsmith Audio, Mutable Instruments and other projects; the [acknowledgments](https://github.com/Fannon/nam-grid#acknowledgments) list all of them.

Report bugs and suggest features in [GitHub issues](https://github.com/Fannon/nam-grid/issues). [Troubleshooting](troubleshooting.md) covers common problems.
