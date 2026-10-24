---
layout: home

hero:
  name: NAM Grid
  text: Open-source guitar rig in one plugin
  tagline: NAM captures, cabinet IRs and effects on a 16-slot grid. Free, in CLAP and VST3.
  actions:
    - theme: brand
      text: Download
      link: https://github.com/Fannon/nam-grid/releases/latest
    - theme: alt
      text: How it works
      link: /grid
    - theme: alt
      text: Module guide
      link: /modules/

features:
  - title: A grid, not a fixed chain
    details: 16 slots in four rows — Pre, two parallel lanes with A/B blend, Post. Any slot takes any module.
    link: /grid
  - title: Captures, cabinets and effects
    details: Neural Amp Modeler captures, cabinet impulse responses and built-in effects, with a library browser and rig presets.
    link: /modules/
  - title: Playable live
    details: Tuner, macros, modulation and performance scenes, with an editor that shows what each module is doing.
    link: /performance
  - title: Free and open source
    details: Make sound right away with the bundled clean capture and starter rigs, then dig into the code.
    link: https://github.com/Fannon/nam-grid
---

<div class="plugin-window">
  <div class="plugin-window-bar"><span></span><span></span><span></span><em>NAM Grid — Clean Default rig</em></div>
  <img src="/screenshots/grid.png" alt="The NAM Grid editor showing the Clean Default rig" />
</div>

## Download

One archive per platform from [GitHub Releases](https://github.com/Fannon/nam-grid/releases/latest), each with **CLAP and VST3**:

| Platform | Download | Notes |
| --- | --- | --- |
| Windows x64 | `nam-grid-windows-x64.zip` | 64-bit Windows |
| macOS Apple Silicon | `nam-grid-macos-arm64.zip` | macOS 11 or newer; confirm once, the build isn't signed yet |
| Linux x64 | `nam-grid-linux-x64.zip` | Ubuntu 22.04 or newer works well; still an early preview |

Everything is included to make sound right away: one clean amp-and-cab capture (RiffSmith Clean) and two starter rigs, Empty and Clean Default. Add your own NAM captures and IRs anytime through the library browser.

## Learn more

Read [how it works](/grid) to see how a rig fits together, or browse the [module guide](/modules/) for every module's controls.
