# Grid modules

All effects use the fixed grid and the same eight-position editor: up to seven knobs
or switches, then Enabled in position eight. Unused positions remain empty. Every visible knob and bypass is
host-automatable; selection and settings are saved in project state. Families
share their controls within a slot. Copy/Move/Swap transfers those values, with
independent processor history at the destination.

**Dynamics > Noise Gate (Smart)** adds learned frequency-selective noise reduction and an
optional final gate. Its seven controls and Enabled fill the eight-position editor.
See [Noise Gate (Smart)](NOISE_PRINT.md) for learning, controls, measurements and limitations.

## Modulation, delay and reverb

| Module | Controls | Character |
| --- | --- | --- |
| Room | Decay, Size, Damping, Pre-delay, Modulation, Low cut, Mix | Sparse early reflections, shorter feedback network |
| Dense Plate | Decay, Size, Damping, Pre-delay, Modulation, Low cut, Mix | Four input diffusers feed a dense network |
| Cloud | Decay, Size, Damping, Pre-delay, Modulation, Low cut, Mix | Diffused network with slow independent delay motion |
| Shimmer | Mix, Decay, Tone, Pitch, Amount, Modulation, Freeze | Eight-line reverb with granular pitched feedback; positive Pitch climbs on each pass |
| Digital Delay | Time, Feedback, Tone, Character, Mix, Output | Character ducks repeats beneath the pick |
| Analog Echo | Time, Feedback, Tone, Character, Modulation, Mix, Output | Two-pole darkening and soft feedback saturation; Character increases wear |
| Tape Echo | Time, Feedback, Tone, Character, Modulation, Mix, Output | Filtered repeats, soft saturation, Modulation adds wow/flutter |
| Chorus | Rate, Depth, Character, Mix | Fractional-delay chorus; Character darkens and adds asymmetric motion |
| Juno Chorus | Mode, Mix, Brightness | Dual-triangle-LFO BBD ensemble with I, I+II and II modes |
| Flanger | Rate, Depth, Feedback, Character, Mix | Short swept comb; Character moves feedback from positive to negative |
| Phaser | Rate, Depth, Feedback, Character, Mix | Six allpass stages; Character raises the sweep's center |
| Auto Tremolo | Rate, Depth, Character, Mix | Rounded pulses; Character sharpens them and adds envelope-dependent speed |
| Vibrato | Rate, Depth, Character | Fully wet pitch movement; Character adds drift and darkening |
| Frequency Shifter | Shift, Feedback, Delay, Side, Tone, Stereo, Mix | Allpass Hilbert shifter in a filtered feedback delay; fixed-Hz rather than ratio pitch movement |
| LoFi Tape Machine | Speed, Wow, Flutter, Noise, Wear, Mix, Output | Tape transport motion, hiss, bandwidth loss and dropouts; separate from tape saturation |

This set follows the core guitar categories in the official
[Helix effect guide](https://manuals.line6.com/en/helix-stadium/live/effect-blocks)
and [Quad Cortex device list](https://neuraldsp.com/device-list). These are category
references, not code sources or claims of hardware emulation. Spring, Wah and stereo
effects are now described in [STEREO_GUITAR_EFFECTS.md](STEREO_GUITAR_EFFECTS.md).
Shimmer provides pitched reverb feedback, but polyphonic pitch shifting and a
harmonizer remain future work. The implementation notes are in
[FX_SOURCE_REVIEW_20261020.md](FX_SOURCE_REVIEW_20261020.md).
The **Creative** category also contains [LoFi Tape Machine](LOFI_TAPE.md), with
measured wow/flutter pitch-motion traces and a separate seven-control editor.
Dense Plate is a voiced digital reverb, not a physical plate model. Juno Chorus is an
original rate-scaled implementation informed by the Juno-60 BBD measurements used by
the MIT Junologue project, not a circuit emulation. Frequency Shifter is a Bode-style
single-sideband shifter, not a pitch shifter; Shift moves every partial by the same
number of hertz.

Digital Delay has no modulation control; its Character ducks the repeats. Analog/Tape
separate saturation character from modulation amount. Output adjusts the blended result.
Reverb Low cut filters its input only on the wet path (0 = off); Modulation controls
network motion. Neither changes the dry signal. Compressor Attack covers .5–50 ms;
Mix blends the compressed and original signals before the slot bypass ramp.

Delays cover 20–1500 ms, feedback 0–82%, tone 800–12000 Hz. Time changes are smoothed
and intentionally bend pitch. Reverb decay is nominal RT60, .25–10 s;
Size is 50–150%, damping 800–12000 Hz, pre-delay 0–100 ms. Damping and interpolation
can shorten measured decay. Modulation rate is .05–10 Hz. Depth/Character/Mix
use percentages. Vibrato is always wet; the other effects can reach 100% wet for a
parallel lane. Set lane blend fully toward the processed lane for serial auditioning.

Shimmer Decay is .5–20 s, Pitch is ±24 semitones, and Amount sets the pitched feedback.
Freeze holds the tail and stops new input; bypassing the slot drains it like the other
reverbs. Juno Chorus uses fixed 0.513/0.863 Hz triangle LFOs and a 1.66–5.35 ms delay;
Brightness moves the pre/post filters. Frequency Shifter spans ±2000 Hz, .05–1000 ms
delay and 0–100% feedback; Side selects lower sideband, ring modulation, or upper
sideband. Feedback is soft-saturated and bounded, and the shifter reports zero
compensation latency.

`SlotTimeEffects.h` contains original MIT implementations: fractional delay with
bounded filtered feedback; an eight-line normalized Hadamard feedback network
with optional input diffusion; and delay/allpass/amplitude modulation. Timing
scales with the actual host rate. There is no forced 44.1 kHz processing or
oversampling. The grid preserves L/R with independent built-in histories. Mono NAM stages explicitly
downmix once and broadcast their wet result, avoiding duplicate NAM inference. Utility
provides balance, width and polarity. These effects currently preserve stereo placement;
Chorus Spread and stereo reverb generate spatial movement from a mono source.

## Tone, dynamics and utility

The **Distortion** category contains Distortion, Boost, Tube Boost, Airwindows Tape
and Bitcrusher. The general Distortion module offers **Soft**, **Hard**, **Fuzz**
and **Foldback** styles, Drive (0–36 dB), Tone (200–12000 Hz), Bias (±100%), Mix and
Output (-24 to +6 dB). Bias changes symmetry; a 5 Hz DC blocker removes the resulting
offset. Style changes crossfade over 20 ms; continuous controls are also smoothed.
Tone filters the wet signal, and Output follows the dry/wet blend. Stereo channels
use independent histories. These are original waveshapers, not pedal emulations;
there is no oversampling, so bright/high-drive styles can alias.

| Module | Controls | Character |
| --- | --- | --- |
| Noise Gate | Threshold, Release, Depth, Mode | Natural: gentle 2:1 expansion, 1.5 ms opening, 40 ms hold. Tight: deep closure, .3 ms opening, 15 ms hold |
| Smooth Compressor | Threshold, Release, Makeup, Attack, Mix | 2.5:1, 9 dB knee, 8 ms default attack |
| Punch Compressor | Threshold, Release, Makeup, Attack, Mix | 4:1, 3 dB knee, 25 ms default attack |
| Guitar EQ | Bass, Mid, Treble | 150 Hz shelf, broad 800 Hz bell, 3 kHz shelf |
| Cut EQ | High pass, Low pass, Presence | Rumble/fizz cuts and 3.5 kHz shelf |
| Parametric EQ | Frequency, Gain, Q, High pass, Low pass | One 80–8000 Hz bell, ±12 dB, Q .3–4 |
| Boost | Gain, Tilt, High pass | Gain 0–18 dB; opposing shelves around 800 Hz; low cut 0–300 Hz |
| Tube Boost | Drive, Tilt, High pass, Bias, Mix, Output | Tube-inspired saturation with adjustable asymmetry and DC removal |
| Filter | Mode, Cutoff, Resonance, Envelope | Resonant low-pass, high-pass or band-pass; optional input envelope shifts cutoff up to four octaves |
| Auto Swell | Rise, Threshold, Release | Phrase envelope removes the pick and fades up; pause below threshold to re-arm |
| Airwindows Tape | Drive, Bump, Mix, Output | Tape saturation/head bump, no flutter |
| Utility | Level, L/R balance, Stereo, Invert L, Invert R | Stereo −100% mono / 0% unchanged / +100% wider |
| Spring Reverb | Decay, Dwell, Damping, Drip, Mix, Output | Three dispersive tanks with driven excitation; stereo wet switch |
| Wah | Position, Resonance, Range, Mix, Output | Automatable log sweep with resonant band-pass and body |

Boost defaults to clean gain: Tilt 0 dB and high pass off. It adds no saturation;
use Tape when nonlinear character is wanted. Swell has no lookahead: it responds
to the incoming phrase and preserves a continuous gain envelope. NAM's Bass/Mid/
Treble and HP/LP controls are neutral post-capture processing before output trim, not inferred
controls of the captured amplifier.

The existing MIT AudioDSPTools coefficients and a new scalar implementation of
the [RBJ/W3C cut-filter equations](https://www.w3.org/TR/audio-eq-cookbook/)
serve EQ, Boost and Envelope Filter. Dynamics were independently written using
AudioDSPTools/LEAF as MIT algorithm references. Full notices remain in
`NeuralAmpModeler/installer/ThirdPartyNotices.txt`.

## Airwindows choice and licensing

The plugin remains MIT. Airwindows source reviewed at
`b79c7a3f9053f408f38f740bef56ca3291a09ad4` also uses
[MIT](https://github.com/airwindows/airwindows/blob/b79c7a3f9053f408f38f740bef56ca3291a09ad4/LICENSE).
Only the double-precision **Tape** processing path was adapted, in
`dsp/airwindows/AirwindowsTape.h`, with its license beside it and in shipped notices.
Changes remove the VST2 SDK, expose explicit sample rate, seed local random state
deterministically, and add smoothed wrapper Mix/Output controls. Original stereo
math is retained with dual-mono input; the grid uses the left result. Upstream's
tiny denormal noise is retained. No CHOWTape/GPL source is incorporated.

[Tape](https://www.airwindows.com/tape/) was selected for its two-control,
no-flutter design. Wow/flutter lives in the separate Tape Echo rather than
forcing delayed pitch modulation into the saturation module.

Focused alternatives reviewed, not imported:

- **ToTape9:** nine controls and more extensive modulation/clip processing;
  too much for this first small tape module.
- **Galactic3:** an interesting ambient stereo reverb. Its code quantizes its
  internal derez step (`1 / int(1 / derez)`) and uses fixed delay lengths. That
  needs explicit sample-rate and stereo evaluation before a faithful port.

Airwindows is not universally restricted to 44.1 kHz. Tape explicitly scales
coefficients by host-rate/44100. Some constants/heuristics remain sample-based,
so this is not a promise of identical tone at every rate. The new original time
effects scale their delay times directly; they do not adopt Galactic3's derez behavior.

## Real-time behavior and verification

Preparation, buffer allocation and destruction occur outside the audio callback
through the existing prepared-slot exchange. Audio uses fixed storage, smoothed
controls and bounded feedback. Existing 5 ms fades cover bypass and replacement.
Temporal histories drain with silence after bypass and then suspend; bypass mutes
their tails rather than spilling them over. Once a delay is fully muted, its feedback
write is disabled; roughly nine seconds clears the full 8.1-second buffer plus filter settling
(with the existing bypass fade and at most one additional drain block).
Reverbs retain a 30-second drain and modulation 70 ms. This uses existing buffers
and never allocates or clears a whole delay ring in one callback. There is no
freeze/infinite-feedback mode.

The new effects report zero **host compensation latency**. Their dry paths have
no block buffering. Delay/reverb/vibrato necessarily delay the effected signal;
vibrato has an 8 ms center delay. This intentional effect time is not compensated
away. NAM sample-rate conversion can still add its separately reported latency.
Host tail reporting accounts for serial/parallel placement but caps the automatic
render/suspend budget at **60 seconds** for the entire plugin (including the legacy
IR allowance). This policy avoids minutes or hours of automatic bounce padding.
Continuous playback keeps the natural decay: very long synced echoes at maximum
feedback may remain audible past the reported budget. For those settings, render
an explicit longer time range. This is a render limit, not an exact decay estimate.

Tests exercise 44.1/48/88.2/96/192 kHz, variable blocks including one frame, parameter
extremes, finite output, deterministic block partitioning, zero audio allocation/
deletion, echo onset timing, distinct decaying reverb responses, swell envelope,
bypass history drainage and reset. CLAP tests cover project recall of every type,
visible controls and bypass gestures in an open native editor. Reproduce with
the build and CTest commands in [BUILD.md](BUILD.md).

Manual acceptance still required:

1. Restart Bitwig, load the updated CLAP, audition clean/dirty/acoustic captures.
   Match bypass loudness before judging saturation, compression or EQ.
2. Test each effect before/after amp and cab, and on one parallel lane. Check
   100%-wet behavior, quiet tails, sustained notes and fast pick transients.
3. Sweep time/rate/feedback/damping, automate controls and toggle bypass during
   tails. Check for excessive resonances and audible discontinuities.
4. Save/reopen a project containing all families. Change host rate/block size,
   exchange modules during playback, and check CPU at your actual low buffer size.

No listening, Bitwig, macOS or VST3-host validation is claimed. Offline tests
establish numerical behavior and lifecycle safety, not final musical voicing.

Tube Boost uses an original biased soft saturator, the same Tilt/High pass voicing as
Boost, and a 5 Hz DC blocker. Drive adds gain and saturation; Bias adds even-harmonic
character; Mix includes an unprocessed dry path; Output sets final level. It is not a
circuit emulation. Strong drive can alias without oversampling. See the
[focused Hotone/TONEX comparison](EFFECT_COVERAGE.md) for current coverage and next priorities.

## Historical offline CPU checkpoint (`64ac242`)

These measurements precede the expanded controls and Tube Boost. See the updated
checkpoint below for the current increment.


Windows x64 Release, Ryzen 9 5950X, 128-frame blocks, one effect in Pre 1 and
all other slots empty. Fastest of three timed 128,000-frame renders after warmup.
Numbers include the complete plugin/grid overhead; they are one-core elapsed-time
percentages, not a DAW meter or an underrun guarantee. All cases report zero latency.

| Effect | 48 kHz | 96 kHz |
| --- | ---: | ---: |
| Empty grid | 0.19% | 0.37% |
| Envelope Filter | 0.26% | 0.52% |
| Auto Swell | 0.23% | 0.47% |
| Digital Delay | 0.31% | 0.63% |
| Analog Echo | 0.34% | 0.68% |
| Tape Echo | 0.43% | 0.86% |
| Room | 0.61% | 1.27% |
| Dense Plate | 0.71% | 1.42% |
| Cloud | 1.04% | 2.14% |
| Chorus | 0.39% | 0.77% |
| Flanger | 0.41% | 0.82% |
| Phaser | 0.35% | 0.69% |
| Auto Tremolo | 0.38% | 0.75% |
| Vibrato | 0.38% | 0.77% |
| Airwindows Tape | 0.49% | 0.99% |

Reproduce from `build-grid`: `NeuralAmpModeler/nam-clap-bench.exe out/nam-grid.clap
fx-17 1 - -` (Cloud). Use `empty` for baseline, or `fx-10` through `fx-23` for
Filter through Tape. Raw local results are in `build-grid/effects-cpu.csv`.

## State and layout checkpoint

Schema 13 contains 2346 parameters (286 inherited + 16 × 126 family controls + 44
performance controls). Earlier supported projects retain named values and fill
new controls from defaults despite the changed parameter stride.
Older unreleased format compatibility remains best effort. New controls participate
in Copy/Move/Swap, automation and project recall. Seven controls plus Enabled share
eight equal cells across the module editor. A smaller effect does not need to fill them.

Additional tests check Tube Boost's even harmonics with Bias, near-zero steady-state
DC and exact settled dry blend; compressor attack and blend; NAM cuts in the processing
reference; all eight editor positions; and schema-6 later-slot parameter recall.

## Previous mono offline CPU checkpoint

Same host and benchmark method as above. Values include the full grid; all cases
report zero compensation latency. Defaults are used, so this does not measure all
parameter extremes. Raw local results: `build-grid/complete-modules-cpu.csv`.

| Effect | 48 kHz | 96 kHz |
| --- | ---: | ---: |
| Empty grid | 0.20% | 0.40% |
| Smooth Compressor | 0.35% | 0.70% |
| Analog Echo | 0.47% | 0.93% |
| Cloud | 1.09% | 2.17% |
| Tube Boost | 0.39% | 0.79% |

## Tempo sync, tuner and stereo Utility

All three delays and all five modulation modules have **Tempo sync**. Division replaces
Time/Rate in position one: 1/16, 1/8T, 1/8, 1/8D, 1/4, 1/4D, 1/2, 1/2D, 1/1. T means
triplet and D dotted. Manual settings remain intact when sync is off. Delay timing uses
60 × beats / BPM; modulation uses its reciprocal. Missing/invalid tempo defaults to
120 BPM; valid values are bounded to 30–300 BPM. The longest synced delay is eight
seconds (whole note at 30 BPM). Modulation is rate-synced and free-running, not locked
to the host song position. Auto Tremolo disables envelope-driven acceleration while
synced, retaining its waveform Character. Tempo/time changes use existing smoothing
and can bend pitch; there is no mandatory lookahead or oversampling.

Delay buffers now reserve about 6 MiB per stereo slot at 48 kHz, 24 MiB at 192 kHz.
Bypassed delays stop feedback after their fade and suspend after roughly nine
seconds of cleanup. The host tail budget is capped at 60 seconds across the grid;
continuous-playback echoes are not truncated. No new compensation latency.

**Utility** has level −60…+12 dB, balance −100…+100%, Stereo −100…+100%, independent L/R
polarity and Enabled. Center balance is unity, extremes attenuate the opposite channel;
this is not constant-power panning. Gains/polarity/width changes ramp to avoid jumps.
Stereo is centered at 0 (unity), −100 folds to mono, +100 doubles side. For phase cancellation between captures, polarity can help, but there is no automatic
phase alignment or fine-delay control yet.

**Tuner** is a separate panel beside Settings, not a grid slot. One C++ MPM detector
was adapted from the MIT sibling trace-tuner revision
`2e1ff89243698b8f4fca33dc4505a2cd749d9e67`; its complete MIT notice is shipped.
No Rust/FFT runtime or extra plugin framework is added. The audio tap selects the
stronger input channel per block, low-passes/decimates to 11–12 kHz, and writes to a
bounded SPSC queue. A worker performs normalized autocorrelation, key-peak selection
and parabolic refinement over 1536 samples. Intended monophonic range is 35–1600 Hz;
analysis takes about 130–140 ms and does not delay processed audio. Quiet/uncertain
signals clear the display. The worker sleeps on a condition variable when the panel is
closed; only UI/control code takes that mutex. A-reference and mute preference are
automatable/saved, while panel visibility is not recalled.

New tests cover harmonic-rich guitar/bass pitches (41–659 Hz, under two cents in
synthetic fixtures), silence/nonfinite input, right-only capture at all five host rates,
audio allocation guards, stereo empty paths/Utility/A2 downmix, synced echo divisions
and Auto Tremolo beat rate, real CLAP transport changes, schema-7 migration, native
tuner controls, Escape unmute and editor-destruction unmute. Real guitar listening and
Bitwig/macOS validation remain outstanding.

Manual additions: tune every guitar string and low bass notes; compare A=440/442;
check quiet decay, right-only input and reopening the panel. Set Bitwig to 60/90/120 BPM,
compare dotted/triplet echoes and automate tempo. Put Utilities after each parallel amp,
pan the lanes apart and invert either polarity; save/reopen and test a mono host output.

Current stereo build, same offline benchmark method (128-frame blocks; fastest of
three measured renders; total cost as % of one core; tuner closed):

| Effect | 48 kHz | 96 kHz |
| --- | ---: | ---: |
| Empty grid | 0.28% | 0.54% |
| Smooth Compressor | 0.54% | 1.06% |
| Analog Echo | 0.76% | 1.51% |
| Cloud | 1.87% | 3.80% |
| Tube Boost | 0.63% | 1.26% |
| Utility | 0.35% | 0.70% |
| Two synthetic A2 models | 7.26% | 8.72% |

Built-in cost rises with the independently processed right channel; NAM inference
is unchanged. New built-ins still report zero latency; the two-model fixture
reports 0/54/84 samples at 48/44.1/96 kHz. Raw effect timings are in ignored
`build-grid/stereo-modules-cpu.csv`. These are offline figures, not Bitwig CPU meters.

The spatial and guitar increment adds Ping-pong to all delays, Chorus Spread,
stereo wet output to Room/Plate/Cloud, Spring Reverb and automatable Wah. Utility's
Stereo scale is centered at zero. See [STEREO_GUITAR_EFFECTS.md](STEREO_GUITAR_EFFECTS.md)
for controls, original algorithm references, migration and the manual checklist.
