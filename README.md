# nam-grid

[![Build](https://github.com/Fannon/nam-grid/actions/workflows/dual-prototype.yml/badge.svg)](https://github.com/Fannon/nam-grid/actions/workflows/dual-prototype.yml)

An open-source guitar effects plugin based on [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerPlugin)
and iPlug2, with native **CLAP and VST3** builds.

Sixteen slots form four serial input stages, two parallel four-slot lanes, and
four serial output stages. Load NAM captures, WAV cabinet IRs, or built-in effects
in any slot. Empty slots pass audio through. Stereo is retained through built-in
effects and IRs; a mono NAM stage averages its inputs and runs one inference pass.

The editor includes input/output gain, lane balance, a tuner, tempo sync, routing
and modulation controls, and complete rig presets saved as `.ngrid` files.
Effects include gates, compressors, EQ/filter, distortion, modulation, pitch,
delays, reverbs, [Noise Gate (Smart)](docs/NOISE_PRINT.md), and
[LoFi Tape Machine](docs/LOFI_TAPE.md).

**Release status: preview.** Windows x64 builds and automated tests are verified
locally. Real DAW listening and low-buffer sessions, VST3 host validation, remote
CI, and macOS validation remain release gates. See the
[first-release review](docs/FIRST_RELEASE_REVIEW.md) for findings and checks.

To build and test, follow [BUILD.md](docs/BUILD.md). The supported entry point is
CMake; the retained upstream installer and standalone scripts are historical.
Build outputs and release archives include third-party notices: keep them with
the plugins when distributing a build. No NAM captures or IR collection is bundled.

- [Editor and library](docs/UI.md): add folders, choose captures/IRs, and edit slots.
- [Module reference](docs/MODULES.md) and [starting settings](docs/EFFECT_DEFAULTS.md).
- [FX control and visualization audit](docs/FX_PARAMETER_REVIEW.md).
- [Recent FX layout and verification](docs/FX_USABILITY_REVIEW.md).
- [Airwindows module candidates](docs/AIRWINDOWS_CANDIDATES.md): outboard,
  console, thickener and character-compressor research.
- [Reverb and delay algorithm research](docs/REVERB_DELAY_RESEARCH.md).
- [Reverb/delay audition candidates](docs/REVERB_DELAY_AUDITION.md): new cores,
  Airwindows modules, ranges and local renders.
- [Cosmic Echo prototype](docs/COSMIC_ECHO.md): six Supermassive-inspired ambient modes.
- [Grid routing](docs/GRID.md) and [performance controls](docs/ROADMAP_EFFECTS.md).
- [Rig presets](docs/PRESETS.md): saving, recall, and external file references.
- [Verification history](docs/VERIFICATION.md) and [latest release review](docs/FIRST_RELEASE_REVIEW.md).

Captures and IRs remain external files. Saved content hashes help relink moved
files within indexed library folders; presets do not embed or copy those files.
Loading a large model can briefly occupy the editor while its DSP is prepared.

The existing CLAP/VST3 plugin IDs are retained for host identification. This
unreleased preview may change parameter IDs, names and enum ranges; prototype
project and automation compatibility is not guaranteed.

Licensed under [MIT](LICENSE).

## Acknowledgments

nam-grid is built on the work of many open-source projects:

- **[Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerPlugin)** by
  Steven Atkinson — the plugin foundation, together with the
  [NAM Core](https://github.com/sdatkinson/NeuralAmpModelerCore) and
  [AudioDSPTools](https://github.com/sdatkinson/AudioDSPTools) (NAM captures,
  cabinet IRs, EQ and compressor filter coefficients).
- **[Gateway](https://www.neuralampmodeler.com/)** — the pre-built official NAM
  plugin by Steven Atkinson, the starting point for the dual-prototype editor.
- **[iPlug2](https://github.com/iPlug2/iPlug2)** — the C++ plugin framework behind
  the CLAP and VST3 builds.
- FX algorithms adapted or informed by (all used under permissive licenses; see the
  shipped [third-party notices](NeuralAmpModeler/installer/ThirdPartyNotices.txt)
  for details and full license texts):
  - **[Airwindows](https://github.com/airwindows/airwindows)** (Chris Johnson) —
    [Tape](https://www.airwindows.com/tape/),
    [kPlate140](https://www.airwindows.com/kplate140/),
    [kGuitarHall2](https://www.airwindows.com/kguitarhall2/) and Galactic3.
  - **[Faust reverb library](https://faustlibraries.grame.fr/libs/reverbs/)**
    (GRAME) — [JPverb](https://faustlibraries.grame.fr/libs/reverbs/#rejpverb),
    Greyhole, and David Braun's MIT Faust reconstruction of the
    [Valhalla Supermassive](https://faustlibraries.grame.fr/libs/reverbs/#revalhallasupermassive)
    algorithms behind Cosmic Echo.
  - **[Signalsmith Audio basics](https://github.com/Signalsmith-Audio/basics)**
    (Geraint Luff) — the Room/Hall reverb cores.
  - **[Mutable Instruments Clouds](https://github.com/pichenettes/eurorack/tree/master/clouds)**
    (Émilie Gillet) — the Dattorro/Griesinger tank behind Dense Plate.
  - **Schwung modules** — [Shimwash](https://github.com/twinpowers/schwung-shimwash)
    (shimmer), [Junologue Chorus](https://github.com/charlesvestal/schwung-junologue-chorus),
    and [Ruminant](https://github.com/mestela/schwung-ruminant) (frequency-shifting
    delay), reimplemented as rate-scaled double-precision code.
  - **[LEAF](https://github.com/spiricom/LEAF)** — compressor dynamics.

Thanks to all contributors credited in the
[third-party notices](NeuralAmpModeler/installer/ThirdPartyNotices.txt).
