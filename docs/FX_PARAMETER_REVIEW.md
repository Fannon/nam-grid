# FX parameter and visualization review — 2026-10-21

This review starts after `a45a257`. The initial 43 built-in menu choices, plus NAM/IR,
were checked against their DSP, defaults, visible controls and displays. The new
reverb/delay pass expands the built-in menu to 48 choices and Cosmic Echo to 49; see
[REVERB_DELAY_AUDITION.md](REVERB_DELAY_AUDITION.md). These
are code/measurement judgments; final musical voicing still needs matched-level
listening with real guitar, especially at low buffers and high gain.

## Changes

- Schema 25 appends seven controls for [Digital Black Gate and Pressure](AIRWINDOWS_DYNAMICS.md).
  Gate Threshold stays within −74…−10 dB of the native amplitude mapping; its
  header Mode chooses DigitalBlack or CStrip hysteresis. Its meter measures actual
  attenuation including the closing fill. Pressure keeps a shared stereo detector,
  native Compress and sine voice, with conventional outer Mix/Output. The source
  audit corrects sample-counted gate timing and Pressure's squared-rate recovery.

- Schema 24 adds the first four [Airwindows outboard modules](AIRWINDOWS_OUTBOARD.md):
  21 appended controls per slot, native 0–100% intensity/filter controls, nominal
  ±6 dB nonlinear EQ bands and genuine dB wrapper trims. Console EQ's frequency
  ranges retain upstream coefficient scales (Treble 1–16 kHz, Bass 300 Hz–16 kHz).
  The display uses signed density bars and cut settings; a linear EQ curve would
  misrepresent its harmonics. Strip/Butter use exact detector-GR taps with full
  IN/OUT histories. Colour uses matched desk levels and IN/OUT histories; a static
  shaper curve would misrepresent its dynamic convolution. All continuous controls
  smooth; Desk fades through dry and clears the small histories. CStrip2 and
  ButterComp2 retain their different native compressor designs.

- Shared presentation metadata owns effect-specific labels, audible-effect help,
  conditional visibility and layout. Unique controls use the same wording in
  host/state descriptors and the editor. Prototype compatibility is not a constraint.
- Schema 19 consolidated the FX controls, removing the separate LoFi bank and
  frozen schema-16 stride. Schema 20 extends that bank to 165 controls per slot
  for the reverb/delay audition modules. Schema 21 appends eight Cosmic Echo
  controls, for 173 FX controls per slot and 3,098 total parameters.
- Enum registration reads shared choice tables and descriptor defaults. Compile-time
  checks keep ranges consistent with those choices. CLAP explicitly reports enum
  metadata through a reproducible iPlug2 patch.
- Digital Delay Character is **Ducking**; Analog Echo is **Age**; Tape Echo is
  **Saturation**. Modulation Character is **Texture**, **Sweep center**,
  **Feedback sign**, **Pulse / feel**, or **Sweep color**, as appropriate.
- Gate Depth is **Reduction**, Tube Boost Bias is **Asymmetry**, Utility Level/
  Stereo are **Output / Width**, Rotary Acceleration is **Ramp time**, and Wah
  Range is **Register**. Wah moves both endpoints rather than changing their ratio.
  Shimmer Tone is **Damping**, consistent with other reverbs. Sub-octave labels
  explicitly say **1 oct down / 2 oct down**.
- Reverse Delay no longer offers the ineffective Ping-pong switch. Dual Delay's
  existing switch now selects a working **cross-feedback mode**: the mono sum
  enters the left tap and repeats alternate through the two different tap times.
  Dry stereo remains intact. Off retains the independent-tap behavior.
- Reverse and Dual Delay output gain now smooths across automation, rather than
  jumping immediately. Uni-Vibe's Mix is hidden in Vibrato mode, which is fully wet.
- Spring controls form a compact **Decay, Dwell, Drip, Damping, Mix, Output** row.
  Reverse Delay also uses a compact row. An exhaustive test checks every visible
  knob position across sync and alternate-mode combinations.
- Each selected module has a short sound explanation beneath its title. All
  utility controls have audible-effect tooltips. Bypassed modules are identified
  in the explanation so a setup sketch is not confused with active processing.

## Parameter audit

Most existing ranges and insertion defaults already cover restrained
electric-guitar starting points and useful creative extremes. The subsequent
reverb/delay pass expands and documents their time/size ranges and adjusts Size
defaults for the new cores. Thresholds, gain
and makeup depend on input calibration; more controls would not solve that.
The seven-knob limit remains. The following table covers every offered built-in.

| Module | Musical control judgment | Feedback / visualization |
| --- | --- | --- |
| Distortion | Six styles: Soft clip, Hard clip, Fuzz, Foldback, Diode and Rectify. Diode adds asymmetric even harmonics; Rectify adds an octave-like buzz. Drive/Bias shape harmonics; Tone/Mix/Output control harshness, attack and level. | Shared-DSP transfer curve with input-range markers; separate tone/mix/output response. Selected-style sound explanation. |
| Boost | Clean gain, tilt and low cut are sufficient for pushing a following capture. | Live IN/OUT levels, explicit gain/tilt explanation. |
| Tube Boost | Drive, tilt, low cut, asymmetry, mix and output cover a softer asymmetric alternative. | Live IN/OUT; asymmetry and output tooltips. |
| Airwindows Tape | Drive/Bump/Mix/Output suit a saturation module; transport controls belong in LoFi Tape. | Live IN/OUT; saturation and bass-emphasis guidance. |
| Bitcrusher | Bits and rate should remain separate; Tone/Mix/Output tame the creative extremes. | Live IN/OUT; quantization, aliasing and high-cut guidance. |
| Noise Gate | Threshold/Release/Reduction plus Natural/Tight are appropriate. Ratio/hold are voiced by mode. | Input/output history, threshold and measured reduction. |
| Noise Gate (Smart) | Learn/Sensitivity/Reduction/Attack/Release/final Gate/Mix cover learning and sustain preservation. | Measured reduction and history; learn and optional-gate guidance. |
| Smooth Compressor | Fixed gentle ratio/knee plus Threshold/Attack/Release/Makeup/Mix keep adjustment manageable. | Measured reduction and threshold history. |
| Punch Compressor | Firmer fixed ratio/knee with slower attack and dry blend serves a distinct role. | Measured reduction and threshold history. |
| Peak Limiter | Ceiling and Release suffice for the current zero-lookahead design; a true-peak promise would require a different processor. | Measured reduction and ceiling. |
| Guitar EQ | Three broad bands are sufficient for tone shaping after a capture. | Combined coefficient-based frequency response. |
| Cut EQ | High/low cuts and presence provide rumble/fizz control. | Combined response including bypassed cuts. |
| Parametric EQ | Frequency/Gain/Q plus cuts and Notch mode cover problem bands. Gain is hidden for Notch. | Coefficient-based bell/notch/cut response. |
| Filter | LP/HP/BP, cutoff, resonance and upward envelope are useful; zero envelope provides static filtering. | Actual applied cutoff/Q plus coefficient response. |
| Wah | Position/Q/Register/Mix/Output cover expression use; Register is the correct name for moving both endpoints. | Live IN/OUT; explicit heel/toe and controller guidance. |
| Chorus | Rate/Depth/Texture/Mix/Spread are sufficient; no unused feedback knob. | Left/right delay-motion sketch. |
| Juno Chorus | I/II/I+II and Mix/Brightness keep the intended simple voice selection. | Both configured voice trajectories. |
| Flanger | Rate/Depth/Feedback/Feedback sign/Mix cover comb motion and polarity. | Small-signal comb response at sweep positions. |
| Phaser | Rate/Depth/Feedback/Sweep center/Mix cover the fixed six-stage design. | Six-stage small-signal response. |
| Tremolo | Rate/Depth/Pulse-feel/Mix are useful, but the feel control deliberately couples shape and envelope speed when unsynced. | Gain curves and envelope-speed range; synced speed stays fixed. |
| Vibrato | Fully wet pitch motion needs Rate/Depth/Texture; hiding Mix is appropriate. | Configured delay excursion. |
| Rotary | Slow/Fast speeds, selector, ramp, drive, spread and mix cover performance needs within seven knobs. | Horn/drum speed and stereo-motion setup. |
| Uni-Vibe | Rate/Depth/Sweep color/Mix and chorus/vibrato mode cover the uneven four-stage voice. | Stage sweep sketch; ineffective Mix hidden in Vibrato mode. |
| Ring Mod | Frequency/Depth/Tone/Mix are useful, though Depth and Mix overlap when Tone is effectively flat. Retaining both preserves dry-tone versus wet-filter control. | Bipolar carrier/gain sketch. |
| Octaver | Independent dry/-1/-2 octave levels, Tone and Output are appropriate for monophonic lines. | Layer-level bars with octave labels and tone frequency. |
| Doubler | Detune/Delay/Spread/Mix/Output cover a subtle second-take effect. | Opposite-detune voice sketch with cents and delay. |
| Frequency Shifter | Shift/Feedback/Delay/Sideband/Tone/Spread/Mix expose its unusual but useful inharmonic repeats. | Fixed-Hz sideband/repeat sketch on a labeled harmonic reference. |
| Digital Delay | Time/Feedback/Tone/Ducking/Mix/Output are useful; no inert pitch-modulation knob. | Time/feedback/mix/output stems, treble survival and L/R routing. |
| Analog Echo | Age couples darkening and saturation; Modulation remains independent. | Repeat/treble sketch; explicit Age explanation. |
| Tape Echo | Saturation and Modulation are separate, along with Time/Feedback/Tone/Mix/Output. | Repeat/treble sketch; tape-motion guidance. |
| Reverse Delay | Window length/Tone/Mix/Output are sufficient for the existing fresh-window reverse algorithm. It is not a conventional feedback delay. | Reversed-window/seam sketch; no ineffective Ping-pong switch. |
| Dual Delay | Time/Feedback/Tone/Ratio/Tap balance/Mix/Output already fill seven knobs. Header Ping-pong adds meaningful cross-feedback without another parameter bank. | Two tap times/balance, routing and repeats; first arrival times checked against DSP. |
| Room Reverb | Size/Decay/Damping/Pre-delay/Modulation/Low cut/Mix are appropriate for compact spaces. | Configured space, decay and damping. |
| Plate Reverb | Same controls suit a denser, smoother tank; defaults distinguish it from Room. | Tank and diffusion/tail sketch. |
| Cloud Reverb | Longer decay and stronger movement suit a wash; bass/treble shaping remains important. | Large-space/tail sketch. |
| Hall Reverb | Large reflection spacing and pre-delay distinguish the hall voice. | Space/pre-delay/decay sketch. |
| Spring Reverb | Dwell and Drip are meaningful spring-specific controls; Damping/Mix/Output complete the row. | Spring geometry and decay/damping sketch. |
| Shimmer Reverb | Mix/Decay/Damping/Pitch/Amount/Modulation/Freeze are a useful seven-control set. | Pitch direction, feedback amount, decay and freeze setup. |
| Freeze | Hold/Capture/Crossfade/Tone/Held level are appropriate; Held level is additive, not a dry/wet crossfade. | Window/overlap and shortened-loop-period sketch. |
| Stutter | Window/Gate length/Edge fade/Mix plus sync cover rhythmic slicing. | Repeat/gate/fade sketch; period remains the full window. |
| LoFi Tape | Speed/Wow/Flutter/Noise/Wear/Mix/Output cover tone, transport instability and dropouts without duplicating saturation. | Measured wow/flutter cents traces; setup explanation. |
| Utility | Output/Balance/Width/polarity/fine delay cover existing-stereo adjustment and alignment. | Mid/side routing, polarity, width and offset setup. |

NAM now shows live input/output level history instead of a decorative processor
icon; gear badges remain in the header. Cabinet IR retains live input/output
history. These levels describe amplitude, not spectrum, clipping waveform or stereo
correlation. Their mono-sum metering can cancel opposite-polarity stereo.

## Modes and release policy

Dual Delay's cross-feedback is the mode added in this round. No new knob bank is
needed and the existing off setting preserves prior behavior. Projects that had
its previously ineffective switch on will now hear the requested routing.

Distortion adds **Diode** and **Rectify** before the first release. Diode uses an
original asymmetric soft shaper with an earlier negative shoulder. Rectify uses
full-wave soft saturation. Both share the existing DC rejection, tone filter,
mix, output control and 20 ms style crossfade. They are original transfer
functions, not circuit emulations; distortion remains unoversampled.

Do not expose 16 choices merely to reserve future modes. iPlug2 maps an enum index
to `index / (count - 1)` for VST3. A permanently fixed count of 16 would preserve
those positions, but all unused choices would remain host-selectable. Expose the
six working styles. This is different from reserving internal IDs/storage, which
does not reserve normalized host positions. See the bundled
`iPlug2/IPlug/IPlugParameter.cpp` and `IPlug/VST3/IPlugVST3_Parameter.h`.

Prototype parameter IDs, enum ranges and several saved names intentionally change.
Schema 19 distinguished the consolidated bank from older readers; the current
reverb/delay audition build uses schema 21. Older named-state
readers remain best effort; neither old project recall nor old automation is a
release requirement. Before a public stable release, freeze parameter IDs/ranges
or define an explicit versioning strategy for later enum expansion.

Further reverb/delay candidates are researched in
[REVERB_DELAY_RESEARCH.md](REVERB_DELAY_RESEARCH.md).

## Display meaning and verification

Parameter sketches redraw as controls change; they do not fabricate live LFO
phase or claim to measure a room or input spectrum. Delay stems omit nonlinear
saturation, dynamic ducking and wobble; treble stems isolate feedback filtering.
Repeat heights now use a fixed -60 to +12 dB scale, so wet mix and output gain
change the visible heights; bright inner stems show surviving 5 kHz content.
Doubler separates pitch offset from stereo placement: Width does not
retune its voices, and Mix changes prominence rather than pitch.
Frequency-shift stems use a labeled 220 Hz harmonic reference, not detected notes.
Distortion's left plot is the static pre-tone shaper; the right plot isolates the
linear wet filters, dry/wet phase sum and output gain, excluding nonlinear gain.
The subsequent [visualization audit](FX_VISUALIZATION_REVIEW.md) covers every
current module, adds Wah/Bitcrusher views, corrects Flanger/Ring Mod response
details and standardizes chart rows, labels and legends.

Tests cover every offered effect's control positions and help, including sync,
notch and vibrato conditions. Dual Delay tests verify actual impulse arrivals,
dry stereo, output/routing automation partition invariance and no allocations
at 44.1/48/96/192 kHz. The distortion tone preview is compared against actual
small-signal DSP at several frequencies and mix settings.

The six distortion styles are checked for distinct outputs, DC rejection,
sample-rate scaling, automation partition invariance and zero audio allocations.
Spectral assertions distinguish Diode's even harmonics from symmetric clipping,
and Rectify's doubled fundamental. Host tests enumerate all 16 FX banks, verify
choice ranges/defaults/text conversion, automate every choice in the first/last
slots, and recall both new styles.

Before the subsequent reverb/delay additions, Windows Release CLAP/VST3 rebuilt
successfully. All five CTest suites passed at that checkpoint:
the four focused suites in 15.48 s, and the native CLAP host/editor suite in
350.97 s. The native suite covers the full module catalog, hidden Reverse
Ping-pong, conditional Uni-Vibe Mix, enum metadata/automation and new-style recall.
Smart Gate's transient Learn control retains its separate native test. The
native suite timeout is 600 s to give the expanded matrix headroom on CI.

Native capture generation completed for the ten focused effects and all six
Distortion styles. Diode and Rectify were visually inspected after the final
cleanup; earlier focused inspection covered Dual Delay, Spring, Freeze, Stutter,
Frequency Shifter, Doubler, Octaver, Utility and Swell. This verifies
layout/rendering, not audible quality or complete DAW behavior. Dependency patch
application was checked for clean, idempotent setup, including the CLAP enum flag.
