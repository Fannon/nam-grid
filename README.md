# NAM Grid

[![Build Native](https://github.com/Fannon/nam-grid/actions/workflows/build-native.yml/badge.svg)](https://github.com/Fannon/nam-grid/actions/workflows/build-native.yml)

An open-source guitar rig in one **CLAP / VST3 plugin**. Combine
[Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerPlugin) captures,
cabinet impulse responses and built-in effects, then blend two parallel chains
for anything from a simple amp sound to layered ambient textures.

**[Download NAM Grid](https://github.com/Fannon/nam-grid/releases/latest)** ·
**[Read the manual](https://fannon.github.io/nam-grid/)** · [Getting started](https://fannon.github.io/nam-grid/getting-started) ·
[See it in action](https://fannon.github.io/nam-grid/#see-it-in-action)

![NAM Grid with a clean amp-and-cab capture, Tube Boost, supporting effects, and the amp capture editor](docs/screenshots/grid.png)

*The Clean Default rig that comes with the plugin: a gate, compression, a clean
amp-and-cab capture and ambient effects, with the capture selected. Find more
captures and IRs for your own rigs on
[TONE3000](https://www.tone3000.com), which hosts free NAM captures and IRs.*

## About this project

NAM Grid stands on the shoulders of a lot of excellent open-source work. The
neural amp modeling is [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerPlugin)
by Steven Atkinson, the plugin framework is [iPlug2](https://github.com/iPlug2/iPlug2),
and many effects are adapted from or informed by Airwindows, the Faust reverb
library, Signalsmith Audio, Mutable Instruments and others (see
[acknowledgments](#acknowledgments)). NAM Grid ties these together into the guitar
plugin I wanted: several captures, cabinets and effects in one flexible rig.

NAM Grid was developed with AI coding agents. I directed the design, reviewed the
changes and tested the results, supported by an extensive automated test suite.

## Why NAM Grid

- **[A free grid instead of a fixed chain](https://fannon.github.io/nam-grid/grid):** 16 slots in four rows
  (Pre, two parallel lanes A and B, Post). Any slot takes any module, so you can run
  several NAM captures, cabinet IRs or the same effect more than once, and blend the
  two lanes with **A/B**.
- **[55 built-in effects](https://fannon.github.io/nam-grid/modules/):** gates, compressors and outboard-style
  dynamics, EQs and filters, [guitar and bass drives](https://fannon.github.io/nam-grid/modules/pedal-drive), modulation, pitch effects, delays and reverbs,
  including [Cosmic Echo](https://fannon.github.io/nam-grid/modules/cosmic-echo) and the
  [LoFi Tape Machine](https://fannon.github.io/nam-grid/modules/lofi-tape).
- **[Visualizations that explain each effect](https://fannon.github.io/nam-grid/ui#displays-and-meters):** curves,
  live spectra and measured responses show what every control does, and level meters
  in each display footer compare the module's input and output.
- **Built for low latency:** zero added latency for most effects and all cabinet IRs.
  Pedal Drive adds 64 samples for antialiasing. Cabinet IRs switch
  between direct and FFT convolution automatically, and the DSP is tuned for small
  buffers with no allocation on the audio thread.
- **[Browse while you listen](https://fannon.github.io/nam-grid/browser):** audition modules, captures and IRs in
  a slot before keeping them, and save complete rigs as `.ngrid` presets.
- **[Performance controls](https://fannon.github.io/nam-grid/performance):** tuner, tempo sync, an eight-row
  modulation matrix with an LFO, envelope follower, step sequencer and four macros,
  lane routing and three one-click scenes.

## Download and install

Download the archive for your platform from
[GitHub Releases](https://github.com/Fannon/nam-grid/releases/latest): Windows x64,
macOS Apple Silicon (macOS 11 or newer) or Linux x64 (early preview). Each contains
both CLAP and VST3. Copy the plugin into your DAW's plugin folder, rescan, and insert
**NAM Grid** on an audio track. NAM Grid comes with a clean amp-and-cab capture and
two starter rigs, so you can play right away.

[Getting started](https://fannon.github.io/nam-grid/getting-started) has the plugin folders for each system, the
one-time step that macOS needs for the unsigned build, DAW setup and a walk-through of
your first rig. If something does not work, see
[troubleshooting](https://fannon.github.io/nam-grid/troubleshooting).

## Feedback and contributions

Feedback, bug reports and ideas are welcome as
[GitHub issues](https://github.com/Fannon/nam-grid/issues); see
[what to include in a bug report](https://fannon.github.io/nam-grid/troubleshooting#report-a-problem).

Pull requests are welcome too. NAM Grid is a side project, so please be aware that
replies, reviews and releases happen in spare time and may take a while. Open an
issue first for larger changes, and see [CONTRIBUTING.md](CONTRIBUTING.md).

## For developers

NAM Grid is based on NAM, NAM Core, AudioDSPTools and iPlug2. The supported build
entry point is **CMake**: [build.md](docs/build.md) covers dependencies, submodule
patches, build commands, CTest and packaging. The
[documentation index](docs/README.md) lists every guide, including implementation,
compatibility, verification and DSP-source notes.

New controls, modules and modes follow the append-only
[compatibility contract](docs/development.md#parameter-and-project-compatibility).
Between releases, the
[Build Native](https://github.com/Fannon/nam-grid/actions/workflows/build-native.yml)
workflow provides development builds of `main` as GitHub Actions artifacts (sign-in
required).

## License and credits

Licensed under [MIT](LICENSE). Adapted algorithms, dependencies and the
[Roboto](https://github.com/googlefonts/roboto) font (Apache 2.0) retain their own notices; see [ThirdPartyNotices.txt](ThirdPartyNotices.txt)
and the accompanying notice files in distributed builds.

### Acknowledgments

NAM Grid is built on the work of many open-source projects:

- **[Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerPlugin)** by
  Steven Atkinson — the plugin foundation, together with the
  [NAM Core](https://github.com/sdatkinson/NeuralAmpModelerCore) and
  [AudioDSPTools](https://github.com/sdatkinson/AudioDSPTools) (NAM captures,
  cabinet IRs, EQ and compressor filter coefficients).
- **[Gateway](https://www.neuralampmodeler.com/)** — the official pre-built NAM
  plugin by Steven Atkinson, the starting point for NAM Grid's editor.
- **[iPlug2](https://github.com/iPlug2/iPlug2)** — the C++ plugin framework behind
  the CLAP and VST3 builds.
- FX algorithms adapted or informed by (all used under permissive licenses; see the
  shipped [third-party notices](ThirdPartyNotices.txt)
  for details and full license texts):
  - **[Airwindows](https://github.com/airwindows/airwindows)** (Chris Johnson) —
    [Tape](https://www.airwindows.com/tape/),
    [kPlate140](https://www.airwindows.com/kplate140/),
    [kGuitarHall2](https://www.airwindows.com/kguitarhall2/), Galactic3,
    CStrip2, BussColors4, ButterComp2, DigitalBlack and CStrip's gate.
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
[third-party notices](ThirdPartyNotices.txt).
