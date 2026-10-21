# nam-grid

[nam-grid](https://github.com/Fannon/nam-grid) is an open-source guitar plugin based on the
official [Neural Amp Modeler plugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin).
This branch experiments with a fixed 16-slot NAM grid: four serial input slots, two parallel
four-slot lanes, and four serial output slots. It retains native CLAP/VST3 and iPlug2.
Start with the [grid experiment](docs/GRID.md), [build instructions](docs/BUILD.md),
[repository assessment](docs/PHASE0.md), and [verification report](docs/VERIFICATION.md).
Slots offer [gates, compressors, EQ, Boost, Tape, Filter, Swell, four reverbs, three delays, Wah, five modulation effects and stereo Utility](docs/MODULES.md), with at most seven knobs and bypass.
Ping-pong delay, chorus spread, stereo reverb and a centered Utility Stereo control are included.
A compact input tuner and host tempo sync are included; mono NAM stages still run one inference pass.
Additional [effects and grid controls](docs/ROADMAP_EFFECTS.md) include Rotary, Uni-Vibe,
Limiter, Octaver, Doubler, reverse/dual delays, Freeze, Stutter, Bitcrusher, Ring Mod,
fine timing, crossover routing, shared modulation and performance scenes.
[Rig presets](docs/PRESETS.md) save complete setups to `.ngrid` files. Global IN,
OUT and lane balance controls sit beside their positions in the signal flow.
[UI and library](docs/UI.md) explain the flat editor, selected-module panel and multiple NAM/IR folders.
[Slot controls](docs/SLOTS.md) cover EQ, cabinet IRs, metering, Copy/Move/Swap and A2 Full/Lite.
Windows builds and the automated harness are verified; Bitwig and macOS testing remain outstanding.

[![Build](https://github.com/Fannon/nam-grid/actions/workflows/dual-prototype.yml/badge.svg)](https://github.com/Fannon/nam-grid/actions/workflows/dual-prototype.yml)

The following notes are inherited from upstream; use the nam-grid build instructions above
for this fork. Upstream installers do not include the grid.

A VST3/AudioUnit plug-in\* for [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler), built with [iPlug2](https://iplug2.github.io).

- https://www.youtube.com/user/RunawayThumbtack
- https://github.com/sdatkinson/neural-amp-modeler

## Building and Installation

To build the app or plugin, there are build scripts in [NeuralAmpModeler/scripts/](https://github.com/sdatkinson/NeuralAmpModelerPlugin/tree/main/NeuralAmpModeler/scripts).
The [workflows](https://github.com/sdatkinson/NeuralAmpModelerPlugin/tree/main/.github/workflows) can show you how to do this.

### Pre-built installers

If you want a pre-built installer from this repo without having to , I've made "Gateway", a fork of this repo, availble at https://neuralampmodeler.com/users!

## Supported Platforms

The Neural Amp Modeler plugin currently supports Windows 10 (64bit) or later, and macOS 10.15 (Catalina) or later.

For Linux support, there is an LV2 plugin available: https://github.com/mikeoliphant/neural-amp-modeler-lv2.

## About

This is a cleaned up version of [the original iPlug2-based NAM plugin](https://github.com/sdatkinson/iPlug2) with some refactoring to adopt better practices recommended by the developers of iPlug2.
(Thanks [Oli](https://github.com/olilarkin) for your generous suggestions!)

\*could also support AAX, CLAP, Linux, iOS soon.

## Rough edges

### Standalone I/O
The I/O for the standalone doesn't inherit the stability of most plugin hosts (DAWs), so it's a bit sparser on features. For complex routing, the plugin (VST3/AU) inside a plugin host is still the most reliable option.

### Graphics backend
If you're having trouble with NAM crashing before the GUI comes up, then you might have an unsupported graphics configuration. Usually, this is when you have a dedicated graphics card (like an nVIDIA GPU) and you're using the integrated (CPU) graphics on a Windows system. To fix this, Go to the control panel, pick NAM (or your DAW), and make sure that it uses your graphics card. (If you know more and can help fix this, please make an Issue and let me know more!)
