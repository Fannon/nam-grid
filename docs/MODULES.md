# Grid modules

All effects use the fixed grid and the same six-position editor: up to five knobs,
then Enabled. Unused positions remain empty. Every visible knob and bypass is
host-automatable; selection and settings are saved in project state. Families
share their controls within a slot. Copy/Move/Swap transfers those values, with
independent processor history at the destination.

## Modulation, delay and reverb

| Module | Controls | Character |
| --- | --- | --- |
| Room | Decay, Size, Damping, Pre-delay, Mix | Sparse early reflections, shorter feedback network |
| Dense Plate | Decay, Size, Damping, Pre-delay, Mix | Four input diffusers feed a dense network |
| Cloud | Decay, Size, Damping, Pre-delay, Mix | Diffused network with slow independent delay motion |
| Digital Delay | Time, Feedback, Tone, Character, Mix | Character ducks repeats beneath the pick |
| Analog Echo | Time, Feedback, Tone, Character, Mix | Two-pole darkening and soft feedback saturation; Character increases wear |
| Tape Echo | Time, Feedback, Tone, Character, Mix | Filtered repeats, soft saturation, Character adds wow/flutter |
| Chorus | Rate, Depth, Character, Mix | Fractional-delay chorus; Character darkens and adds asymmetric motion |
| Flanger | Rate, Depth, Feedback, Character, Mix | Short swept comb; Character moves feedback from positive to negative |
| Phaser | Rate, Depth, Feedback, Character, Mix | Six allpass stages; Character raises the sweep's center |
| Auto Tremolo | Rate, Depth, Character, Mix | Rounded pulses; Character sharpens them and adds envelope-dependent speed |
| Vibrato | Rate, Depth, Character | Fully wet pitch movement; Character adds drift and darkening |

This set follows the core guitar categories in the official
[Helix effect guide](https://manuals.line6.com/en/helix-stadium/live/effect-blocks)
and [Quad Cortex device list](https://neuraldsp.com/device-list). These are category
references, not code sources or claims of hardware emulation. Spring reverb,
pitch shifting, rotary, stereo/ping-pong effects and tempo sync are outside this increment.
Dense Plate is a voiced digital reverb, not a physical plate model.

Delays cover 20–1500 ms, feedback 0–82%, tone 800–12000 Hz. Time changes are smoothed
and intentionally bend pitch. Reverb decay is nominal RT60, .25–10 s;
Size is 50–150%, damping 800–12000 Hz, pre-delay 0–100 ms. Damping and interpolation
can shorten measured decay. Modulation rate is .05–10 Hz. Depth/Character/Mix
use percentages. Vibrato is always wet; the other effects can reach 100% wet for a
parallel lane. Set lane blend fully toward the processed lane for serial auditioning.

`SlotTimeEffects.h` contains original MIT implementations: fractional delay with
bounded filtered feedback; an eight-line normalized Hadamard feedback network
with optional input diffusion; and delay/allpass/amplitude modulation. Timing
scales with the actual host rate. There is no forced 44.1 kHz processing or
oversampling. The grid remains mono internally and duplicates its final signal
to stereo. Native stereo width is not implemented.

## Tone, dynamics and utility

| Module | Controls | Character |
| --- | --- | --- |
| Natural Gate | Threshold, Decay, Depth | Gentle 2:1 expansion; 1.5 ms opening, 40 ms hold |
| Tight Gate | Threshold, Decay, Depth | Deep closure; .3 ms opening, 15 ms hold |
| Smooth Compressor | Threshold, Release, Makeup | 2.5:1, 9 dB knee, 8 ms attack |
| Punch Compressor | Threshold, Release, Makeup | 4:1, 3 dB knee, 25 ms attack |
| Guitar EQ | Bass, Mid, Treble | 150 Hz shelf, broad 800 Hz bell, 3 kHz shelf |
| Cut EQ | High pass, Low pass, Presence | Rumble/fizz cuts and 3.5 kHz shelf |
| Parametric EQ | Frequency, Gain, Q, High pass, Low pass | One 80–8000 Hz bell, ±12 dB, Q .3–4 |
| Boost | Gain, Tilt, High pass | Gain 0–18 dB; opposing shelves around 800 Hz; low cut 0–300 Hz |
| Envelope Filter | Cutoff, Resonance, Envelope | Resonant low-pass, up to four octaves of input-driven opening |
| Auto Swell | Rise, Threshold, Release | Phrase envelope removes the pick and fades up; pause below threshold to re-arm |
| Airwindows Tape | Drive, Bump, Mix, Output | Tape saturation/head bump, no flutter |

Boost defaults to clean gain: Tilt 0 dB and high pass off. It adds no saturation;
use Tape when nonlinear character is wanted. Swell has no lookahead: it responds
to the incoming phrase and preserves a continuous gain envelope. NAM's Bass/Mid/
Treble controls are neutral post-capture tone EQ before output trim, not inferred
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
their tails rather than spilling them over. There is no freeze/infinite-feedback mode.

The new effects report zero **host compensation latency**. Their dry paths have
no block buffering. Delay/reverb/vibrato necessarily delay the effected signal;
vibrato has an 8 ms center delay. This intentional effect time is not compensated
away. NAM sample-rate conversion can still add its separately reported latency.
Tail bounds account for serial/parallel placement, maximum feedback and decay,
so automation cannot extend a tail beyond the declared bound. They are conservative,
not estimates of currently audible tail duration.

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

## Offline CPU checkpoint

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
