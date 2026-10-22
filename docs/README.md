# Documentation

Start with the [main README](../README.md) for downloads, installation and a first
guitar rig. The pages below describe usage and implementation. Dated reviews,
prototype plans and old measurements live in the [archive](archive/README.md).

## Playing and building rigs

| Guide | Contents |
| --- | --- |
| [Editor](UI.md) | Grid cards, controls, meters and visualizations |
| [Library browser](BROWSER.md) | Find effects, NAM captures, cabinet IRs and presets |
| [Modules](MODULES.md) | Effect controls and processing behavior |
| [Starting settings](EFFECT_DEFAULTS.md) | Electric guitar defaults and acoustic/bass adjustments |
| [Routing](GRID.md) | Pre/Post stages, parallel lanes, stereo and latency |
| [Presets](PRESETS.md) | Save, recall and share complete rigs |
| [Tuner](TUNER.md) | Tuning, reference pitch, mute and history |
| [Performance controls](ROADMAP_EFFECTS.md) | Macros, modulation, scenes and lane controls |
| [Noise Gate (Smart)](NOISE_PRINT.md) | Learn and reduce a noise profile |
| [LoFi Tape Machine](LOFI_TAPE.md) | Tape transport and degradation effects |
| [Cosmic Echo](COSMIC_ECHO.md) | Ambient echo styles and controls |
| [Stereo effects](STEREO_GUITAR_EFFECTS.md) | Ping-pong, chorus spread, spring and wah |

## Development and release

- [Build and test](BUILD.md) and [contribution guidance](../CONTRIBUTING.md).
- [Release checklist and known validation limits](RELEASE_CHECKLIST.md).
  [Release notes](RELEASE_NOTES.md) are used by the release workflow.
- [Slot editing and processing](SLOTS.md), [FX parameter audit](FX_PARAMETER_REVIEW.md)
  and [visualization audit](FX_VISUALIZATION_REVIEW.md).
- [On-demand FX benchmarks](FX_PERFORMANCE.md) and
  [further optimization candidates](FX_OPTIMIZATION_CANDIDATES.md).

## DSP sources and research

- [Airwindows candidates](AIRWINDOWS_CANDIDATES.md),
  [outboard implementation](AIRWINDOWS_OUTBOARD.md) and
  [dynamics implementation](AIRWINDOWS_DYNAMICS.md).
- [Reverb and delay research](REVERB_DELAY_RESEARCH.md) and
  [Surge source/license review](SURGE_SOURCE_REVIEW.md).
- [Third-party notices](../NeuralAmpModeler/installer/ThirdPartyNotices.txt)
  and [screenshot provenance](screenshots/README.md).

## Maintaining these docs

Keep usage, build instructions and release requirements in the guides above.
Put completed reviews, one-off measurements and development checkpoints in
`archive/`, with their date or source commit and a link to the relevant guide.
Historical test results establish what passed at that checkpoint; they are not
the current build status. Check GitHub Actions for a specific commit.
