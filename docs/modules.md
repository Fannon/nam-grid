# Grid modules

All effects use the fixed grid and the same eight-position editor: up to seven knobs
or switches, then Enabled in position eight. Unused positions remain empty. Every visible knob and bypass is
host-automatable; selection and settings are saved in project state. Families
share their controls within a slot. Copy/Move/Swap transfers those values, with
independent processor history at the destination.

**Dynamics > Noise Gate (Smart)** adds learned frequency-selective noise reduction and an
optional final gate. Its seven controls and Enabled fill the eight-position editor.
See [Noise Gate (Smart)](noise-gate-smart.md) for learning, controls, measurements and limitations.

## Modulation, delay and reverb

| Module | Controls | Character |
| --- | --- | --- |
| Room | Decay, Size, Damping, Pre-delay, Modulation, Low cut, Mix | Multistage diffusion, early reflections and short Householder FDN |
| Dense Plate | Decay, Size, Damping, Pre-delay, Modulation, Low cut, Mix | Four input diffusers and a cross-coupled two-branch plate tank |
| Cloud | Decay, Size, Damping, Pre-delay, Modulation, Low cut, Mix | Rotating stereo allpass loop with slow delay motion |
| Hall | Decay, Size, Damping, Pre-delay, Modulation, Low cut, Mix | Larger diffusion/Householder FDN with bass and treble decay shaping |
| Airwindows Plate | Input, Sustain, Resolution, Pre-delay, Mix, Low cut, Output | Native kPlate140 audition port |
| Airwindows Guitar Hall | Sustain, Resolution, Brightness, Early level, Position, Mix, Output | Native kGuitarHall2 audition port |
| Galactic Space | Replace, Brightness, Detune, Size, Resolution, Mix, Output | Native Galactic3 ambient-pad audition port |
| Cosmic Echo | Time, Warp, Feedback, Density, High cut, Motion, Mix; header Style | Six Valhalla Supermassive-inspired Faust modes; see [Cosmic Echo](cosmic-echo.md) |
| Shimmer | Mix, Decay, Damping, Pitch, Amount, Modulation, Freeze | Eight-line reverb with granular pitched feedback; positive Pitch climbs on each pass |
| Digital Delay | Time, Feedback, High cut, Ducking, Mix, Output | Ducking ducks repeats beneath the pick |
| Analog Echo | Time, Feedback, High cut, Age, Modulation, Mix, Output | Two-pole darkening and soft feedback saturation; Age increases wear |
| Tape Echo | Time, Feedback, High cut, Saturation, Modulation, Mix, Output | Filtered repeats, soft saturation, Modulation adds wow/flutter |
| Multi-head Tape | Time, Feedback, High cut, Saturation, Modulation, Mix, Output; header Heads | Eight head patterns including stereo dual/cascade, last-head-only feedback |
| Diffused Delay | Time, Feedback, High cut, Diffusion, Modulation, Mix, Output | Echoes smear into stereo reflection clusters |
| Chorus | Rate, Depth, Texture, Mix | Fractional-delay chorus; Texture darkens and adds asymmetric motion |
| Juno Chorus | Mode, Mix, Brightness | Dual-triangle-LFO BBD ensemble with I, I+II and II modes |
| Flanger | Rate, Depth, Feedback, Feedback sign, Mix | Short swept comb; Feedback sign moves feedback from positive to negative |
| Phaser | Rate, Depth, Feedback, Sweep center, Mix | Six allpass stages; Sweep center raises the sweep's center |
| Auto Tremolo | Rate, Depth, Pulse / feel, Mix | Rounded pulses; Pulse / feel sharpens them and adds envelope-dependent speed |
| Vibrato | Rate, Depth, Texture | Fully wet pitch movement; Texture adds drift and darkening |
| Frequency Shifter | Shift, Feedback, Delay, Sideband, High cut, Spread, Mix | Allpass Hilbert shifter in a filtered feedback delay; fixed-Hz rather than ratio pitch movement |
| LoFi Tape Machine | Speed, Wow, Flutter, Noise, Wear, Mix, Output | Tape transport motion, hiss, bandwidth loss and dropouts; separate from tape saturation |

Shimmer provides pitched reverb feedback; polyphonic pitch shifting and a
harmonizer are not implemented.
The **Creative** category also contains [LoFi Tape Machine](lofi-tape.md), with
measured wow/flutter pitch-motion traces and a separate seven-control editor.
Dense Plate is a voiced digital reverb, not a physical plate model. Juno Chorus is an
original rate-scaled implementation informed by the Juno-60 BBD measurements used by
the MIT Junologue project, not a circuit emulation. Frequency Shifter is a Bode-style
single-sideband shifter, not a pitch shifter; Shift moves every partial by the same
number of hertz.

Digital Delay has no modulation control; its Ducking control lowers repeats beneath playing. Analog/Tape
separate saturation character from modulation amount. Output adjusts the blended result.
Reverb Low cut filters its input only on the wet path (0 = off); Modulation controls
network motion. Neither changes the dry signal. Compressor Attack covers .5–50 ms;
Mix blends the compressed and original signals before the slot bypass ramp.

Conventional delays cover 5–4000 ms with a quadratic dial scale, feedback 0–82%, and High cut 800–12000 Hz. Time changes are smoothed
and intentionally bend pitch. Tempo sync spans **1/32–1/1**, including triplet/dotted choices;
the same division range applies to Stutter and synced modulation.
Reverb decay is nominal RT60, .15–30 s;
Size is 25–200%, damping 800–12000 Hz, pre-delay 0–200 ms. Plate and Cloud use
larger spatial scaling; Hall expands it further. Damping and interpolation
can shorten measured decay. Modulation rate is .05–10 Hz. Depth/texture/Mix
use percentages. Vibrato is always wet; the other effects can reach 100% wet for a
parallel lane. Set lane blend fully toward the processed lane for serial auditioning.

Shimmer Decay is .5–20 s, Pitch is ±24 semitones, and Amount sets the pitched feedback.
Freeze holds the tail and stops new input; bypassing the slot drains it like the other
reverbs. Juno Chorus uses fixed 0.513/0.863 Hz triangle LFOs and a 1.66–5.35 ms delay;
Brightness moves the pre/post filters. Frequency Shifter spans ±2000 Hz, .05–1000 ms
delay and 0–100% feedback; Sideband selects lower sideband, ring modulation, or upper
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

The first [Airwindows outboard additions](airwindows-outboard.md) are:

| Module | Controls | Character |
| --- | --- | --- |
| Outboard Channel | Low cut, High cut, Compress, Comp speed, Saturate, Output, Mix | CStrip2 Capacitor → native compression → Spiral; Dynamics group |
| Console Colour | Input trim, Output trim, Mix; header Desk | Eight BussColors4 console voices, with fixed default-level matching; Distortion group |
| Console EQ | Treble, Mid, Bass, Treble freq, Bass freq, High cut, Low cut | Nonlinear ±6 dB band density and rate-compensated Capacitor; EQ / Filter group |
| Butter Comp | Compress, Output, Mix | Native ButterComp2 polarity-sensitive compression and output-dependent recovery; Dynamics group |
| Digital Black Gate | Threshold, Mix, Output; header Mode | DigitalBlack darkening gate or CStrip hysteresis; reference-time counters and release |
| Pressure | Compress, Mix, Output | Pressure6 shared stereo detector and sine colour; corrected recovery scaling |

The cuts use native percentages: low cut 0 is off, high cut 100 is open. Their
rotating ladder cannot honestly be described by a single unmeasured cutoff Hz.
Console EQ's frequency labels retain the source coefficient scales. All four
are zero-latency and preserve independent stereo histories. The strip and Butter
Comp have related but different detector/recovery math. See the linked audit for
the source decisions, ranges, defaults, GR semantics and measurements.
The [two additional dynamics modules](airwindows-dynamics.md) preserve independent
gate histories and Pressure's linked detector respectively. Digital Black meters
actual attenuation including its rectified closing fill; Pressure meters detector
reduction before makeup and sine colour. Both have live IN/OUT histories.

The **Distortion** category contains Distortion, Boost, Tube Boost, Airwindows Tape
and Bitcrusher. The general Distortion module offers **Soft clip**, **Hard clip**,
**Fuzz**, **Foldback**, **Diode** and **Rectify** styles, Drive (0–36 dB), High cut (200–12000 Hz), Bias (±100%), Mix and
Output (-24 to +6 dB). Bias changes symmetry; a 5 Hz DC blocker removes the resulting
offset. Style changes crossfade over 20 ms; continuous controls are also smoothed.
High cut filters the wet signal, and Output follows the dry/wet blend. Stereo channels
use independent histories. These are original waveshapers, not pedal emulations;
there is no oversampling, so bright/high-drive styles can alias.
Its mini display separates the static pre-tone shaper from the linear High cut/Mix/Output
response. Drive and Bias change the shaper; High cut, Mix and Output change the second curve. The highlighted span and endpoint markers track the current mono input peak,
showing which parts of the curve the signal reaches; this is a peak envelope, not an audio oscilloscope.
Auto Swell Rise spans **10–2000 ms**.

| Module | Controls | Character |
| --- | --- | --- |
| Noise Gate | Threshold, Release, Reduction, Mode | Natural: gentle 2:1 expansion, 1.5 ms opening, 40 ms hold. Tight: deep closure, .3 ms opening, 15 ms hold |
| Smooth Compressor | Threshold, Release, Makeup, Attack, Mix | 2.5:1, 9 dB knee, 8 ms default attack |
| Punch Compressor | Threshold, Release, Makeup, Attack, Mix | 4:1, 3 dB knee, 25 ms default attack |
| Guitar EQ | Bass, Mid, Treble | 150 Hz shelf, broad 800 Hz bell, 3 kHz shelf |
| Cut EQ | High pass, Low pass, Presence | Rumble/fizz cuts and 3.5 kHz shelf |
| Parametric EQ | Frequency, Gain, Q, High pass, Low pass | One 20–20000 Hz bell, ±12 dB, Q .1–20, optional Notch mode |
| Boost | Gain, Tilt, High pass | Gain 0–18 dB; opposing shelves around 800 Hz; low cut 0–300 Hz |
| Tube Boost | Drive, Tilt, High pass, Asymmetry, Mix, Output | Tube-inspired saturation with adjustable asymmetry and DC removal |
| Filter | Mode, Cutoff, Resonance, Envelope | Resonant low-pass, high-pass or band-pass; optional input envelope shifts cutoff up to four octaves |
| Auto Swell | Rise, Threshold, Release | Phrase envelope removes the pick and fades up; pause below threshold to re-arm |
| Airwindows Tape | Drive, Bump, Mix, Output | Tape saturation/head bump, no flutter |
| Utility | Output, L/R balance, Width, Invert L, Invert R, Fine delay | Width −100% mono / 0% unchanged / +100% wider |
| Spring Reverb | Decay, Dwell, Damping, Drip, Mix, Output | Three dispersive tanks with driven excitation; stereo wet switch |
| Wah | Position, Resonance, Register, Mix, Output | Automatable log sweep with resonant band-pass and body |

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

Current native mappings are documented in [Airwindows outboard](airwindows-outboard.md)
and [dynamics](airwindows-dynamics.md); source licenses accompany the implementation.

## Real-time behavior and verification

Preparation, buffer allocation and destruction occur outside the audio callback
through the existing prepared-slot exchange. Audio uses fixed storage, smoothed
controls and bounded feedback. Existing 5 ms fades cover bypass and replacement.
Temporal histories drain with silence after bypass and then suspend; bypass mutes
their tails rather than spilling them over. Once a delay is fully muted, its feedback
write is disabled; roughly nine seconds clears the full 8.1-second buffer plus filter settling
(with the existing bypass fade and at most one additional drain block).
Reverbs retain a 30-second drain and modulation 70 ms. This uses existing buffers
and never allocates or clears a whole delay ring in one callback. Standard delay
feedback is bounded; Freeze and Shimmer hold behavior is described in the
[effects roadmap](performance.md).

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
the build and CTest commands in [build.md](build.md).

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
circuit emulation. Strong drive can alias without oversampling.

## Tempo sync and stereo controls

Supported delays and modulation modules offer **Tempo sync**. Division replaces
Time/Rate while sync is enabled; manual values remain stored. Divisions are
1/32, 1/16, 1/8T, 1/8, 1/8D, 1/4, 1/4D, 1/2, 1/2D and 1/1. T means triplet,
D dotted. Missing tempo falls back to 120 BPM. Modulation is rate-synced and
free-running, rather than locked to host song position. Time changes can bend pitch.

Digital, Analog and Tape Delay's **Ping-pong** folds the wet send to mono and
alternates repeats between channels; dry stereo stays intact. Dual Delay uses
cross-feedback through its two different tap times: the first right arrival is
left time plus right time. Reverse Delay does not have this routing switch.

**Chorus Spread** offsets its L/R LFOs: 0% is coincident, 100% is half a cycle.
Reverb **Stereo wet** folds the wet side to mono when off, without moving dry audio.
Place spatial effects after the last mono NAM capture to retain their stereo width.

Utility **Width** is −100% mono, 0% unchanged and +100% double side. It cannot
create stereo from identical L/R. Balance attenuates the opposite channel rather
than using constant-power panning. **Fine delay** adds 0–20 ms of intentional
timing; it does not change reported model-conversion latency. See
[performance controls](performance.md) for lane timing and modulation,
and [Tuner](tuner.md) for tuning and mute.

## Spring and wah

Spring Reverb has Decay, Dwell, Drip, Damping, Mix and Output. Dwell drives its
excitation; Drip changes dispersion rather than adding generic modulation.
Its three dispersive tanks are spring-inspired, not a measured emulation of a
named tank. Strong Dwell can alias without oversampling. Mathematical references:
[Gamper, Parker and Välimäki](https://research.aalto.fi/en/publications/automated-calibration-of-a-parametric-spring-reverb-model/)
and the [DAFx overview](https://dafx.de/paper-archive/details/omUNRAXkKDeEOxMkIyPwQg).
No source implementation or calibration procedure was copied.

Wah has Position, Resonance, Register, Mix and Output. Map Position to an expression
controller through your DAW or draw automation; there is no separate MIDI-learn
layer or automatic pedal bypass. Register shifts both heel/toe frequencies.
The original band-pass/body voicing follows
[Simper's trapezoidal SVF equations](https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf),
without importing third-party code.

## Capture effects

Freeze's Capture is the source-window length. Its repeat period is Capture minus
the effective Fade, bounded to one quarter of the window: 500 ms Capture and
20 ms Fade repeat every 480 ms. Stutter retains the requested Window period,
with a fade through zero even at 100% Gate. Reverse replaces its captured window
each period with a short transition. Captured audio is transient, not embedded in presets.

## Cabinet IR

**Choose cabinet IR (.wav)** loads a mono WAV into any slot, using the existing NAM IR
loader/convolver. The card shows the filename and `WAV / IR`; its bottom editor has one **Level** knob (-36 to +18 dB).
Each slot holds either a NAM or an IR. Place a NAM cabinet capture using the normal NAM
selector. Full amp+cab captures typically do not need another cabinet stage.

The default Level is -18 dB to match the original global cabinet's headroom convention.
At 0 dB, the slot cancels the reused convolver's fixed -18 dB gain, so a unit impulse has
unity convolution gain at its native 48 kHz rate. Its existing sample-rate-dependent IR
resampling gain is retained. Nothing auto-normalizes IRs. Recalled projects can still have a
legacy global IR; its compatibility toggle appears in Settings. It is disabled in new instances.

The reused processor has zero block buffering latency and caps the resampled IR at 8192
taps. It is intended for cabinet impulses, rather than long reverbs. WAV parsing, resampling,
history allocation, and prewarming occur off audio. Reset reconstructs and primes the
convolver even when the sample rate is unchanged. Variable host blocks use reserved capacity.
Invalid loads preserve the current selection; recalled missing/invalid files retain their
paths and IR type for relinking, while that slot passes audio.

## A2 Full / Lite

The module menu offers **A2 Lite** when Core reports a slimmable model. A2 captures packaged
with both submodels select them from the same `.nam` file. Fixed-size captures have no quality
toggle; a smaller model cannot be synthesized from an arbitrary fixed model.
The supported target is A2. No A1-specific implementation was added.

The [Core size API](https://github.com/sdatkinson/NeuralAmpModelerCore/blob/main/NAM/slimmable.h)
explicitly states that changing size is not real-time safe. Size changes now reload/prepare
the requested model outside audio, then use the slot's existing publication/retirement and
switching fade. Core's setter is never called by `NAMStage::Process`. A short dropout is
allowed during changes. UI toggles and host automation requests are applied on the idle/control
thread; rapid requests can coalesce. If the selected file disappears, the old processor stays
active and the quality parameter reverts to the prepared value.

Other slots append boolean Full Quality parameters. Pre 2 reuses the original `Slim` parameter
(0 = Lite, 1 = Full), preserving its automation ID and old state. New projects default to
Full. Saved quality travels with Copy/Move/Swap and is applied before audio is activated.

All new effects add zero buffering latency. Existing NAM rate conversion still reports its
actual delay when capture/host rates differ; use matching rates for a zero-latency NAM chain.
Typical captures are 48 kHz. Parallel lane compensation continues to follow actual delays.
