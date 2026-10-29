# Module implementation

How the built-in modules are implemented: DSP character and sources, real-time behavior,
latency and tails. For controls, ranges and starting settings see the
[module guide](modules/README.md); for the parameter contract see
[development.md](development.md#parameter-and-project-compatibility).

## Controls and state

Enabled uses a parameter-bound circle in the editor header, sharing the grid node's
automation identity. Every module uses the same eight-position dial editor (see [Shape a module](ui.md#shape-a-module)).
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

New controls use neutral or original-algorithm defaults when loading older states.
The conventional compressors expose their original ratios, plus a detector-only
low cut that leaves audible bass intact. Juno Width scales only the wet side signal;
Uni-Vibe Spread offsets the right sweep. LoFi Tape Drive uses the attributed Airwindows
Tape Spiral function inside the shared 4x oversampler, ahead of the transport. Zero Drive
bypasses saturation exactly; the full tape core, head bump and softening remain in Airwindows Tape.

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
  algorithms; see [Cosmic Echo](#cosmic-echo) below and the [player guide](modules/cosmic-echo.md).
- **Shimmer** is an eight-line reverb with granular pitched feedback (±24 semitones); Freeze
  holds the tail and stops new input. Polyphonic pitch shifting and a harmonizer are not
  implemented.
- **Juno Chorus** is an original rate-scaled implementation informed by the Juno-60 BBD
  measurements of the MIT Junologue project: fixed 0.513/0.863 Hz triangle LFOs and a
  1.66–5.35 ms delay. **Frequency Shifter** is a Bode-style single-sideband shifter (±2000 Hz)
  in a 0.05–1000 ms feedback delay with soft-saturated, bounded feedback. Its quadrature
  pair is a zero-latency polyphase IIR allpass network (Olli Niemitalo's coefficients),
  keeping the opposite sideband about 45 dB down from 30 Hz to 20 kHz at 44.1/48 kHz.
- **Distortion** offers six original waveshapers with ±100% Bias, a 5 Hz DC blocker and 20 ms
  style crossfades. It combines first-order antiderivative antialiasing with minimum-phase
  FIR oversampling (4x below 88.2 kHz, 2x below 176.4 kHz, otherwise 1x), without
  lookahead or reported delay. **Tube Boost** is an original biased soft saturator with Boost's
  Tilt/Low cut voicing and a locally oversampled saturator (4x/2x/1x at the same host-rate bands). It uses the shared minimum-phase FIR with 32 base taps; the host-rate tone and DC filters remain outside it. A collapsed identity FIR
  aligns the partial-Mix dry reference; a 20 ms fade handles the exact-dry bypass at Mix zero.
- **Pedal Drive** ([SlotPedalDrive.h](../NeuralAmpModeler/SlotPedalDrive.h),
  [DSP](../NeuralAmpModeler/dsp/PedalDrive.h)) is an original set of voiced drives
  informed by documented pedal designs. It uses fixed 4x minimum-phase FIR oversampling with zero added reported
  latency, unshifted dry/bypass and local clean-low returns, independent
  stereo histories and 20 ms style crossfades. Its centered character parameter
  changes function/label by style; derived bias normalization is cached on the
  16-sample control schedule. The FIR loops share input loads
  across interpolation phases and use independent decimation accumulators for
  portable vectorization. Settled styles use a single-voice path; only participating styles run during
  transitions; an inactive style starts with cleared filter history. Rodent adds
  first-order antiderivative antialiasing before a continuously integrated slew
  limiter/hard clip; its filter rate remains 4x. See
  [voices, signal flow and design references](modules/pedal-drive.md).
- **Pedal Comp** ([SlotPedalComp.h](../NeuralAmpModeler/SlotPedalComp.h)) is an original
  feed-forward compressor that takes the playing behaviour of guitar sustain pedals
  without modelling a circuit. One linked stereo detector feeds a soft-knee curve whose
  threshold and ratio (1.6:1 to 8:1) both follow Sustain. Automatic makeup reads the curve
  2.5 dB above the reference level, so typical played notes stay at bypass loudness. Attack
  is 1.5 ms (Fast) or 20 ms (Slow). Release is program-dependent: a fast state (150/250 ms)
  and a slow memory that charges over 300 ms toward 40% of the reduction and lets go over
  800 ms, so short peaks recover quickly and held notes ease back. Pickup only changes the
  detector: sensitivity, a 12 dB/octave low cut (70 Hz, or 150 Hz for Acoustic), and for
  Acoustic an RMS window and a 5 kHz low-pass that ignore pick spikes and piezo quack.
  Body / Pick is a first-order ±6 dB tilt around 500 Hz in the detector only, so it changes
  which notes are compressed, never the frequency response. Blend 0% with Level at zero is
  exactly dry. There is no waveshaper, so nothing aliases.

  The Sustain range, Slow attack and release were calibrated against the Yuer Dyna
  Compressor (a Ross-type pedal) in the [ToneTwist AFx dataset](https://github.com/mcomunita/tonetwist-afx-dataset)
  (CC BY-NC 4.0; recordings used for measurement only, not distributed). On the same guitar DI,
  Sustain 50% and 100% follow the pedal's noon and maximum gain-versus-level curves within
  about 1 and 4 dB, and Slow's reduction builds over 12-50 ms like the pedal's slowest attack.
  The pedal's slight treble loss (-2.5 dB at 8 kHz) is not reproduced.
- **EQs and Filter** use the MIT AudioDSPTools coefficients and a scalar implementation of the
  [RBJ/W3C equations](https://www.w3.org/TR/audio-eq-cookbook/). Dynamics were written
  independently, using AudioDSPTools and LEAF as MIT algorithm references.
- **Spring Reverb** has three dispersive tanks with driven excitation, after
  [Gamper, Parker and Välimäki](https://research.aalto.fi/en/publications/automated-calibration-of-a-parametric-spring-reverb-model/)
  and the [DAFx overview](https://dafx.de/paper-archive/details/omUNRAXkKDeEOxMkIyPwQg);
  no source implementation or calibration was copied. Its **Dispersive** tank follows the
  parametric structure of Välimäki, Parker and Abel, "Parametric spring reverberation effect"
  (JAES 58(7/8), 2010): per spring, a cascade of 100 first-order allpasses stretched to a
  ~4.4 kHz cutoff inside a feedback loop, a low-pass at the cutoff and a shorter
  plain-allpass loop for the high-frequency splash. The stage count, coefficient range,
  echo times, loop gains and levels are NAM Grid's own, measured against the chirp,
  decay and level tests; no published code or parameters were copied.
- **Wah** follows [Simper's trapezoidal SVF equations](https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf)
  for its band-pass and body voicing, without third-party code.
- **Airwindows** modules (Tape, kPlate140, kGuitarHall2, Galactic3, CStrip2, BussColors4,
  ButterComp2, DigitalBlack) are native ports of MIT Airwindows source reviewed at
  `b79c7a3f9053f408f38f740bef56ca3291a09ad4`, with licenses beside the code and in shipped
  notices. Changes remove the VST2 SDK, use an explicit sample rate, seed random state
  deterministically and add smoothed wrapper Mix/Output controls. Native intensity and size
  controls are not presented as calibrated units. See [Airwindows outboard](airwindows-outboard.md)
  and [dynamics](airwindows-dynamics.md).

Timing scales with the host rate; nothing is forced to 44.1 kHz. Pedal Drive and
Distortion and Tube Boost oversample their driven paths; Tape and Console Colour
run their cores at 2x below 88.2 kHz. Other built-in effects run at the host rate. Built-in
effects keep independent left/right histories. A mono NAM stage downmixes once and broadcasts
its result, so stereo effects belong after the last mono capture.

## Noise Gate (Smart)

The spectral gate learns a per-band noise print during Learn and applies stereo-linked band
gains, so the image does not wobble. A confidence guard relaxes suppression on strong notes and
returns to the learned profile for quiet playing. Learning transitions to dry over 5 ms and
opens the final gate; digital silence and captures shorter than about 300 ms do not replace an
existing valid print, and during a new capture the last completed print remains the recallable
snapshot. Very large or invalid samples are bounded so they cannot poison the filter histories.

The optional final broadband gate works independently of the spectral profile, also without
learning. It detects the original input's maximum L/R peak (so either channel, including
anti-phase input, can open it), applies one linked gain after the spectral mix, and closes
linearly to exact zero over Release after a 20 ms hold, with a fixed 0.5 ms opening ramp, 5 ms
peak-detector decay and 3 dB hysteresis. An enabled gate starts closed after reset or recall to
avoid an initial noise burst; turning it Off reopens it over 0.5 ms. The GR readout estimates
band gain plus gate attenuation, capped at 120 dB.

With Gate Off, untrained, settled Mix 0 and Reduction 0 pass the exact finite input. Completed
snapshots survive project recall, rig-preset save/load and Copy/Move/Swap; Learn is saved off,
suppressed on rig-preset load and excluded from scenes. Reset preserves the completed print but
clears envelopes and histories. Profiles from older builds that this version cannot read are
rejected without affecting the current sound.

## LoFi Tape Machine

An original transport and degradation effect, not a measured cassette/deck emulation or a
magnetic-hysteresis model; no Dusk/CHOWTape DSP is copied. The transport is a 10 ms wet delay
modulated by up to 5 ms of wow and 0.35 ms of flutter. Both channels share the transport and
dropout envelope; their audio histories and hiss are independent. With Noise off, a centered
mono input stays mono with no crossfeed. Slower Speed changes bandwidth and modulation rate
rather than performing a lasting playback-speed pitch shift.

The transport display derives pitch from changes in each read-head delay, so automation and the
irregular wow component are represented. History is captured at 100 Hz into fixed storage on the
audio thread and delivered through the meter queue; no UI timer invents movement when audio is
stopped. The two components are shown separately, before Mix.

## Cosmic Echo

| Style | Topology |
| --- | --- |
| Gemini | Sixteen scattering delays |
| Sagittarius | Eight serial stereo allpasses |
| Virgo | Sparse delays with both cuts inside the feedback loop |
| Centaurus | Four serial four-channel allpasses |
| Great Annihilator | A delay stage and three matrix allpasses in global feedback |
| Lyra | Four scattering delays, filters at the output |
| Aurora Hall | Eight filtered feedback loops, each with a short allpass diffuser |
| Pulsar Echo | Four serial stereo delay/diffuser stages, four output taps, end-only cross feedback |
| Binary Bloom | Two coupled eight-line FDNs with different time scales and per-line decay normalization |
| Photon Plate | Stereo input diffusion, cross-coupled allpass/delay tank, seven passive output taps per channel |

In the first six styles, Warp, Density, Feedback and modulation keep the source model's per-topology interactions. Time
uses a quadratic dial; Time/Warp automation moves allpass-interpolated reads and can bend the
sounding tail. Style changes fade and clear the old tail. The display is rendered by a private
copy of the DSP whenever a setting changes: a measured impulse response, not an RT60 estimate.
See the [Faust reference](https://faustlibraries.grame.fr/libs/reverbs/#revalhallasupermassive).

Aurora Hall is an original NAM Grid design: Density controls an orthogonal eight-way scattering network and the allpass coefficients inside the loops. Warp spreads clustered main delays; short diffusers retain irregular spacing. Loop filters damp successive repeats. All delays scale with sample rate and process every host sample. No Airwindows source or undersampling is used. The existing sixteen-line bank supplies eight main delays and eight diffusers. See [Aurora Hall settings](modules/cosmic-echo.md#aurora-hall-echoes-and-washes).

## Capture effects

Freeze's Capture is the source window. It repeats every Capture minus the effective
Crossfade, bounded to a quarter of the window: 500 ms Capture and 20 ms Crossfade repeat every
480 ms. Stutter keeps the requested Window period and fades through zero even at 100% Gate
length. Reverse Delay replaces its window each period with a short transition. Captured audio
is transient and never saved in presets.

## Cabinet IR

A mono WAV loads through NAM's IR loader and the [adaptive convolver](modules/ir-processing.md), which
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

Built-in effects have no block buffering. Pedal Drive reports 64 host samples for its
oversampling filters and aligns its dry, clean-low and bypass paths; other built-in
effects report zero host compensation latency.
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
cmake --build build/windows/release --config Release --target nam-grid-clap nam-fx-audition
& .\build\windows\release\NeuralAmpModeler\Release\nam-fx-audition.exe .\build\windows\release\out\nam-grid.clap .\tmp\cosmic-audition cosmic
```

Output goes to the directory passed to the exporter; `tmp/` is gitignored.
These renders are generated developer artifacts: they are not checked in or
copied into release archives.
