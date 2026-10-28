# NAM Grid documentation

> **Beta release:** NAM Grid is a beta (0.x) release. Preset and automation stability are goals, but we reserve the option to improve sound quality or performance in ways that may change the sonic results of existing projects and presets.

The player guides are published at **[fannon.github.io/nam-grid](https://fannon.github.io/nam-grid/)**. This page lists every guide in the repository.

## Get started

- [Getting started](getting-started.md): download, install, set up your DAW and play your first rig.
- [Troubleshooting & FAQ](troubleshooting.md): common problems, questions and how to report a bug.
- [Release notes](release-notes.md): what's new in this version. The full history is in the [changelog](../CHANGELOG.md).

## Use NAM Grid

| Guide | What it covers |
| --- | --- |
| [Editor](ui.md) | The parts of the window, adjusting modules, displays, settings and shortcuts |
| [Grid & routing](grid.md) | Signal flow, typical placement, parallel lanes, blend and the Routing page |
| [Library browser](browser.md) | Finding and auditioning modules, captures, IRs and rigs; library folders |
| [Rig presets](presets.md) | Saving, loading and sharing rigs; switching while you play |
| [Tuner](tuner.md) | Tuning without leaving your rig |
| [Modulation & scenes](performance.md) | LFO, envelope, sequencer, macros, the matrix and performance scenes |

## Modules

- [Module guide](modules/README.md): every module, its controls, starting settings and tips.
- Deep dives: [Pedal Drive](modules/pedal-drive.md), [Noise Gate (Smart)](modules/noise-gate-smart.md), [LoFi Tape Machine](modules/lofi-tape.md), [Cosmic Echo](modules/cosmic-echo.md) and [Cabinet IR processing](modules/ir-processing.md).

## Build and contribute

These pages are for developers; you do not need them to use the plugin.

- [Build and test](build.md), [development and compatibility](development.md), [module implementation](module-implementation.md) and the [release checklist](release-checklist.md).
- [On-demand FX benchmarks](fx-performance.md), the [DSP aliasing checks](dsp-aliasing.md) and the [IR convolution experiment](ir-convolution-comparison.md); the engine rules are in `AdaptiveIR.h`.
- [Airwindows outboard](airwindows-outboard.md) and [Digital Black Gate](airwindows-dynamics.md) implementation notes.
- [Screenshots](screenshots/README.md): how the gallery and module images are generated.
- [Third-party notices](../ThirdPartyNotices.txt) and [contribution guidelines](../CONTRIBUTING.md).
