# Documentation

Start with the [main README](../README.md) for downloads, installation and a first
guitar rig. These guides describe current usage, implementation and release checks.

## Playing and building rigs

| Guide | Contents |
| --- | --- |
| [Editor](ui.md) | Grid cards, controls, meters and visualizations |
| [Library browser](browser.md) | Find effects, NAM captures, cabinet IRs and presets |
| [Modules](modules.md) | Effect controls and processing behavior |
| [Starting settings](effect-defaults.md) | Electric guitar defaults and acoustic/bass adjustments |
| [Routing](grid.md) | Pre/Post stages, parallel lanes, stereo and latency |
| [Presets](presets.md) | Save, recall and share complete rigs |
| [Tuner](tuner.md) | Tuning, reference pitch, mute and history |
| [Performance controls](performance.md) | Macros, modulation, scenes and lane controls |
| [Noise Gate (Smart)](noise-gate-smart.md) | Learn and reduce a noise profile |
| [LoFi Tape Machine](lofi-tape.md) | Tape transport and degradation effects |
| [Cosmic Echo](cosmic-echo.md) | Ambient echo styles and controls |

## Development and release

- [Build and test](build.md) and [contribution guidance](../CONTRIBUTING.md).
- [Release checklist and known validation limits](release-checklist.md).
  [Release notes](release-notes.md) are used by the release workflow.
- [Implementation and compatibility](development.md) and
  [on-demand FX benchmarks](fx-performance.md).
- [Adaptive cabinet convolution](adaptive-ir.md) and the
  [standalone IR engine comparison](ir-convolution-comparison.md).

## DSP implementation sources

- [Airwindows outboard implementation](airwindows-outboard.md) and
  [dynamics implementation](airwindows-dynamics.md).
- [Reverb core adaptations](../NeuralAmpModeler/dsp/reverb/README.md).
- [Third-party notices](../ThirdPartyNotices.txt)
  and [screenshot provenance](screenshots/README.md).

## Maintaining these docs

Keep lasting usage, build and implementation information in the guides above.
Use lowercase kebab-case filenames, keeping conventional names such as `README.md`.
Put reviews, audits, measurements and handoffs in gitignored `tmp/`, or in the
relevant PR description/comments. Git history preserves removed reports.
Check GitHub Actions for the status of a specific commit; old test timings are
not a current validation claim. See [AGENTS.md](../AGENTS.md).
