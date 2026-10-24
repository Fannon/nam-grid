# Module implementation

How the built-in modules are implemented: DSP character and sources, real-time behavior,
latency and tails. For controls, ranges and starting settings see the
[module guide](modules/README.md); for the parameter contract see
[development.md](development.md#parameter-and-project-compatibility).

## Controls and state

Every module uses the same eight-position editor (see the [module guide](modules/README.md#the-module-editor)).
Each control is a host parameter per slot, described in
[EffectParameters.h](../NeuralAmpModeler/EffectParameters.h); its editor position and
per-module label are in [EffectPresentation.h](../NeuralAmpModeler/EffectPresentation.h),
whose layout rule (controls packed from the left, Output before Mix, modes in the header) is checked for
every module by `EffectPresentationTests.h`. Starting settings live in
[SlotDefaults.h](../NeuralAmpModeler/SlotDefaults.h).

Modules of one family share their controls within a slot: the delays share Time, Feedback,
High cut, Character, Modulation, Mix and Output; the classic modulation effects share Rate,
Depth, Feedback, Character and Mix; Room, Dense Plate, Cloud and Hall share the reverb
controls. Copy/Move/Swap transfers these values; the destination gets its own processor
history.

## DSP character and sources

- **Delays** ([SlotTimeEffects.h](../NeuralAmpModeler/SlotTimeEffects.h)) are original MIT
  implementations: fractional delay with bounded filtered feedback (0–82%), 5–4000 ms with a
  quadratic dial and 800–12000 Hz High cut. Time changes are smoothed and bend pitch. Analog
  Echo adds two-pole darkening and soft feedback saturation; Tape Echo adds saturation and
  wow/flutter; Digital Delay's Ducking lowers repeats beneath playing. Multi-head Tape feeds
  back only from the last head. Diffused Delay sends repeats through rotating allpasses.
- **Reverbs:** Room and Hall use Signalsmith-derived multistage diffusion and Householder
  networks with exact per-band decay; Dense Plate uses the Mutable Instruments Clouds
  Dattorro/Griesinger tank; Cloud is a rotating stereo allpass loop. Decay is nominal RT60,
  0.15–30 s; Size 25–200%; Damping 800–12000 Hz; Pre-delay 0–200 ms; Low cut filters only
  the wet input. See [reverb core adaptations](../NeuralAmpModeler/dsp/reverb/README.md).
- **Cosmic Echo** uses David Braun's MIT Faust reconstruction of the Valhalla Supermassive
  algorithms; see [Cosmic Echo](cosmic-echo.md).
- **Shimmer** is an eight-line reverb with granular pitched feedback (±24 semitones); Freeze
  holds the tail and stops new input. Polyphonic pitch shifting and a harmonizer are not
  implemented.
- **Juno Chorus** is an original rate-scaled implementation informed by the Juno-60 BBD
  measurements of the MIT Junologue project: fixed 0.513/0.863 Hz triangle LFOs and a
  1.66–5.35 ms delay. **Frequency Shifter** is a Bode-style single-sideband shifter (±2000 Hz)
  in a 0.05–1000 ms feedback delay with soft-saturated, bounded feedback.
- **Distortion** offers six original waveshapers with ±100% Bias, a 5 Hz DC blocker and 20 ms
  style crossfades. **Tube Boost** is an original biased soft saturator with Boost's
  Tilt/Low cut voicing. Neither oversamples, so bright, high-drive settings can alias.
- **EQs and Filter** use the MIT AudioDSPTools coefficients and a scalar implementation of the
  [RBJ/W3C equations](https://www.w3.org/TR/audio-eq-cookbook/). Dynamics were written
  independently, using AudioDSPTools and LEAF as MIT algorithm references.
- **Spring Reverb** has three dispersive tanks with driven excitation, after
  [Gamper, Parker and Välimäki](https://research.aalto.fi/en/publications/automated-calibration-of-a-parametric-spring-reverb-model/)
  and the [DAFx overview](https://dafx.de/paper-archive/details/omUNRAXkKDeEOxMkIyPwQg);
  no source implementation or calibration was copied.
- **Wah** follows [Simper's trapezoidal SVF equations](https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf)
  for its band-pass and body voicing, without third-party code.
- **Airwindows** modules (Tape, kPlate140, kGuitarHall2, Galactic3, CStrip2, BussColors4,
  ButterComp2, DigitalBlack) are native ports of MIT Airwindows source reviewed at
  `b79c7a3f9053f408f38f740bef56ca3291a09ad4`, with licenses beside the code and in shipped
  notices. Changes remove the VST2 SDK, use an explicit sample rate, seed random state
  deterministically and add smoothed wrapper Mix/Output controls. Native intensity and size
  controls are not presented as calibrated units. See [Airwindows outboard](airwindows-outboard.md)
  and [dynamics](airwindows-dynamics.md).

Timing scales with the host rate; nothing is forced to 44.1 kHz or oversampled. Built-in
effects keep independent left/right histories. A mono NAM stage downmixes once and broadcasts
its result, so stereo effects belong after the last mono capture.

## Capture effects

Freeze's Capture is the source window. It repeats every Capture minus the effective
Crossfade, bounded to a quarter of the window: 500 ms Capture and 20 ms Crossfade repeat every
480 ms. Stutter keeps the requested Window period and fades through zero even at 100% Gate
length. Reverse Delay replaces its window each period with a short transition. Captured audio
is transient and never saved in presets.

## Cabinet IR

A mono WAV loads through NAM's IR loader and the [adaptive convolver](adaptive-ir.md), which
switches between direct and FFT convolution with zero added latency. Output 0 dB gives unity
gain for a unit impulse at the IR's native rate; the -18 dB default is the upstream NAM
cabinet headroom convention. IRs are not normalized. WAV parsing, resampling, history
allocation and prewarming happen off the audio thread; an invalid load keeps the current
selection, and a missing file keeps its path for relinking while the slot passes audio.

## A2 Full / Lite

Captures that Core reports as slimmable offer **A2 Lite**. Changing the size reloads and
prepares the model outside the audio thread and switches through the slot's normal fade, as
the [Core size API](https://github.com/sdatkinson/NeuralAmpModelerCore/blob/main/NAM/slimmable.h)
is not real-time safe. Every slot's `Full Quality` parameter stores the choice; requests from
the editor or host automation apply on the idle thread and may coalesce. Fixed-size captures
have no Lite mode.

## Real-time behavior, latency and tails

Preparation, allocation and destruction happen outside the audio callback through the
prepared-slot exchange. Audio uses fixed storage, smoothed controls and bounded feedback.
5 ms fades cover bypass and replacement. Delay, reverb and modulation histories drain with
silence after bypass and then suspend: a muted delay stops writing feedback and clears its
8.1-second buffer in about nine seconds, reverbs drain for 30 seconds, modulation for 70 ms.
No callback clears a whole delay ring at once.

Built-in effects report zero host compensation latency and have no block buffering.
Delay, reverb and vibrato (8 ms center delay) delay the effected signal on purpose; that
effect time is not compensated. NAM sample-rate conversion reports its own latency, and
parallel lanes are aligned to the longer one. Host tail reporting follows serial and parallel
placement but caps the automatic tail at 60 seconds for the whole plugin; very long synced
echoes at high feedback can ring past it, so render a longer range for those.

Tests cover 44.1–192 kHz, variable blocks down to one frame, parameter extremes, finite
output, block-size independence, zero audio-thread allocation, echo timing, reverb decay,
bypass draining and reset, and project recall and editor gestures for every module in the
native editor. See [build.md](build.md) for the commands. Listening and host validation are
tracked in the [release checklist](release-checklist.md).

## Audition renders

The `nam-fx-audition` target renders one `.ngrid` preset and paired WAVs per
effect style, plus `measurements.csv`, for example:

```powershell
cmake --build build-grid --config Release --target nam-grid-clap nam-fx-audition
& .\build-grid\NeuralAmpModeler\nam-fx-audition.exe .\build-grid\out\nam-grid.clap .\tmp\cosmic-audition cosmic
```

Output goes to the directory passed to the exporter; `tmp/` is gitignored.
These renders are generated developer artifacts: they are not checked in or
copied into release archives.
