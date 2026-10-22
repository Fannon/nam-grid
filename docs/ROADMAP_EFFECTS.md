# Effects and performance controls

This increment implements the recommendations from the 2026-10-20 effects review.
It builds on the existing stereo, Spring and Wah modules and the fixes on `main`.
DSP is original C++ with no new runtime dependency or copied commercial algorithms.

## New modules

Each module is available from a slot's Choose menu. The editor retains a maximum
of seven controls plus Enabled. Changes are automatable and saved in project state.

| Module | Controls / behavior |
| --- | --- |
| Rotary | Slow and Fast speeds, Fast switch, acceleration, drive, spread and mix. Independent low-band amplitude motion and modulated horn delay create the rotating character; speed changes slew rather than jump. |
| Uni-Vibe | Rate, depth, character, mix and vibrato mode. Four unequal allpass stages follow an asymmetric lamp-style sweep; chorus mode combines the dry and phased paths. Rate can sync to the host. |
| Peak Limiter | Ceiling and release with the existing gain-reduction meter. Instant attack, shared L/R gain, no lookahead. This limits sample peaks at the module output; it is not a true-peak limiter, and downstream gain/processing can exceed its ceiling. |
| Mono Octaver | Dry, octave down, two octaves down, tone and output. A filtered, hysteretic zero-crossing divider adds sub-octaves from a single-note input. The detector averages L/R; dry stereo remains independent. This is not a polyphonic transposer. |
| Stereo Doubler | Detune, base delay, spread, mix and output. Opposing pitch offsets use overlapping fractional-delay read heads. Stereo width is not a substitute for recording a second performance. |
| Reverse Delay | Time, tone, mix and output, with optional host division. Captured windows play backward. It is a windowed creative effect; the wet signal needs a capture window before it can begin. |
| Dual Delay | Time, feedback, tone, second-tap ratio, tap balance, mix and output. L/R have distinct times; mono operation combines both taps. The first time can sync to the host; the ratio derives the second. |
| Freeze / Hold | Hold, capture length, loop fade, tone and held level. Adds a looping held layer beneath the continuing unity dry signal. A restored Hold waits for fresh input history instead of immediately latching an empty buffer. Captured audio is transient and is not embedded in state. |
| Stutter | Window, gate, fade and mix, optionally host-synced. Captures a window and repeats it four times before refreshing. Gate and seam fades soften hard edges. |
| Bitcrusher | Bits, sample-hold rate, tone, mix and output. Intentional sample/bit reduction, with a reconstruction low-pass. |
| Ring Modulator | Carrier frequency, depth, tone and mix. Sine-carrier multiplication for metallic sidebands. No automatic pitch tracking is claimed. |

Freeze's Capture control specifies the source-window length. The seam overlaps
the beginning and end, so the held repeat period is Capture minus the effective
Fade (bounded to one quarter of the window). For example, 500 ms Capture and
20 ms Fade repeat every 480 ms without resampling or changing pitch. Stutter
keeps the requested Window period, using a gate fade through zero even at 100%
Gate; Reverse replaces its window each period with a 5 ms capture transition.

The menu also contains a **Slapback preset**: a 90 ms digital echo with zero
feedback, an 8 kHz tone setting and 25% mix. It uses the existing Digital Delay
module rather than another processor type.

Dual Delay's **Ping-pong** switch selects cross-feedback routing. With it off,
the taps receive their own input channels. With it on, the mono sum enters the
left tap and feedback alternates through the two different delay times; the
first right arrival is left time plus right time. Dry stereo remains intact.
Reverse Delay has no Ping-pong switch because its captured-window algorithm
does not implement that routing. Both delays smooth Output automation.

See [FX_PARAMETER_REVIEW.md](FX_PARAMETER_REVIEW.md) for current labels and
visualization semantics across the complete catalog.

## Utility and EQ

Utility gains **Fine delay**, from 0 to 20 ms with 0.01 ms parameter resolution
and an additional sample-count readout.
Fractional reads interpolate adjacent samples; zero is exact passthrough at
neutral controls. Polarity, level, balance and Stereo remain available. This is
intentional audible timing and does not change the reported model conversion latency.

Parametric EQ now spans 20 Hz–20 kHz with Q 0.1–20 and has a **Notch mode**.
Notch uses the same frequency/Q controls; its gain cell is hidden because the
notch itself sets attenuation. Existing high/low cuts remain in the module.
All frequencies are bounded appropriately at the processing sample rate.

## Grid / Control

Open **Grid / Control** in the header. This full-page panel has Routing,
Modulation and Scenes tabs and closes with Close or Escape.

### Routing

- Independent A/B levels, mute and solo. Solo preserves the current blend level;
  enabling both solos passes both lanes, while mute takes precedence.
- **Lane timing**: positive values delay A, negative values delay B, up to 20 ms.
  It never advances live audio. This is additional to the existing conversion-delay
  compensation and helps audition relative capture timing, not correct every phase difference.
- Optional complementary low/high frequency split: A receives the low band and
  B the residual high band. At centered Blend, neutral empty lanes reconstruct the
  original signal, including while the crossover fades on/off. Blend away from
  center changes the relative band levels. The split is a first-order complementary
  filter, not a steep multiband crossover.
- Two Macro knobs with assignments on the Modulation page.

### Shared modulation

One sine LFO, one clean-input envelope follower and one eight-step sequencer can
each target a slot/parameter. Each has signed depth. The LFO and sequencer support
free rate or the existing host-tempo divisions. The follower exposes sensitivity,
attack and release. The eight step values are edited below the source controls.

Targets are Wah position, envelope-filter cutoff, Utility balance, delay mix,
reverb mix (including Spring), NAM trim, parallel Blend and ring frequency.
Choose a relevant target for the selected module; a Wah assignment does not
turn another module into a Wah. Two macros provide additional manual/host-automated
assignments using the same target list. Multiple assignments add and are bounded
to the destination's musical range. Depth zero or target None disables an assignment.

The engine processes bounded 16-sample control intervals and preserves that clock
across variable host blocks. Destinations use their existing smoothing. Shared
sources run while assignments are active, and reset on host reactivation. Tempo
sync controls periods; these sources are not PPQ-position-locked sequencers.
Host parameter values remain the base settings; modulation does not write host automation.

### Performance scenes

Store and recall three sets of controls. Scenes include gains, bypass, EQ,
dynamics, effects controls, routing and modulation assignments. They retain
the current model paths, module types, quality settings and tuner preferences.
Hold capture switches/audio are excluded. Store overwrites the chosen scene.
Scene values persist with the project and malformed scene data rejects before
changing state. Recall informs the host of changed parameters.

Scenes are performance settings, not alternate complete rigs. This avoids synchronous
model replacement and gives the existing effect histories continuity during recall.

## Processing and state

Storage is allocated/reset outside audio processing. Capture processors swap
preallocated banks at boundaries rather than copy entire multi-second windows.
Stereo-linked processors run once on both channels; the mono NAM policy still
uses one downmixed inference pass. New module preparation uses the existing
publish/retire handoff. No audio-thread mutex or new runtime dependency was added.

Main's finite host render-tail policy remains capped at 60 seconds. An enabled
Freeze with Hold active instead reports an infinite tail; releasing Hold restores
the finite policy. Bypassing Freeze retains the held pad for re-enabling. Bypassed
delays stop feedback while draining; Dual Delay reserves 17 seconds to clear its
maximum 16-second history. Intentional delay/pitch-window behavior is separate
from the conversion latency reported to the host.

Cabinet IR drains 8,192 silent frames after its bypass fade, covering the
convolver's maximum resampled FIR length before processing suspends. Re-enabling
after drainage cannot replay earlier input. Re-enabling sooner can resume its
remaining history during the ordinary fade. Stutter's 33-second drain covers four
maximum eight-second synced windows; a Decay setting is not a hard reverb end time.

Independent dynamics and envelope followers can change the stereo balance of
asymmetric input; fixed EQ/filter histories are also independent. The Peak Limiter
uses linked gain. No new stereo-link option or nonlinear oversampling is implied.

Capture keeps four double buffers (history/held audio, L/R). Reverse and Stutter
reserve 8.1 seconds because host sync reaches eight seconds at 30 BPM; their free
Time/Window knob limits are smaller. That is about 11.87 MiB per slot at 48 kHz or
47.46 MiB at 192 kHz, before other prepared state. Sixteen such slots approach
760 MiB at 192 kHz. Freeze reserves 2.1 seconds. Dual Delay reserves two 16.1-second
lines, approximately 11.79 MiB / 47.17 MiB at those rates. These are independent
per-slot histories prepared off audio; live processing does not resize them.

State schema is 13. Existing supported schemas 2/3/6/7/8/9/10/11/12 load with defaults for
new named parameters; unsupported WIP schemas 4/5 still reject. There are 126
utility controls per slot and 44 grid controls: 2,346 parameters total. Performance
scenes contain named values. No backward host-automation-ID guarantee is introduced
for this unreleased WIP.

[Rig presets and global gain placement](PRESETS.md) describe file Save/Load and
the IN / A-B / OUT controls. Scenes remain parameter-only snapshots within a rig.

## HQ source-review increment

The follow-up quality pass added three modules from the MIT source review in
[FX_SOURCE_REVIEW_20261020.md](archive/FX_SOURCE_REVIEW_20261020.md):

| Module | Controls / behavior |
| --- | --- |
| Shimmer | Mix, Decay, Tone, Pitch, Amount, Modulation and Freeze. Eight-line FDN with two granular pitch shifters in the feedback path. Positive Pitch climbs on each pass; Freeze holds the tail and stops new input. |
| Juno Chorus | Mode I / I+II / II, Mix and Brightness. Two fixed triangle LFOs, 1.66–5.35 ms delay range and inverted right-channel modulation. Separate from the generic Chorus module. |
| Frequency Shifter | Shift ±2000 Hz, Feedback, Delay, Side, Tone, Stereo and Mix. Allpass Hilbert quadrature shifter inside a filtered feedback delay. Side moves from lower sideband through ring modulation to upper sideband. |

The three effects are rate-scaled and allocated in `Reset`; audio uses the existing
smoothing, bypass ramp and drain policy. Shimmer and Frequency Shifter add no
compensation latency. Schema 10 states fill the 17 new per-slot controls with
defaults while preserving the earlier 103 controls. The MIT reference notices are in
`installer/ThirdPartyNotices.txt`.

## Verification

The isolated build directory is `build-roadmap`; artifacts are under its `out/`.
The ordinary CMake build remains supported. Regression coverage includes:

- All new modules at 44.1/48/96/192 kHz with variable block partitions, independent
  stereo input, settled bypass, finite output and no observed stage-path allocation.
- Linked limiter ceiling and channel ratio, fractional impulse timing, held capture
  persistence/reset, and the octaver's generated sub-octave spectrum.
- Crossover reconstruction through on/off fades, lane timing/solo, and the shared
  modulation clock across different host partitions.
- CLAP save/recall and processing of each new type, routing/modulation values,
  dynamic Freeze tail reporting, and native performance-panel scene store/recall.
- Existing model, WAV, menu/library, ownership, tuner and native-editor regressions.

Automated results establish these behaviors, not commercial sound parity. Real
guitar listening, Bitwig low-buffer sessions and macOS/VST3-host validation remain
manual acceptance work. Polyphonic transposition, harmonizer, shimmer, spectral
granular engines and physical external FX loops remain deferred as specified in
the review's scope choices.

### Offline CPU sanity check

Local Windows x64 Release / MSVC 19.44, 128-frame blocks, repeated 128,000-sample
renders with a warm-up; fastest of three measured passes, including the complete
plugin callback. At 48 kHz the empty grid measured 0.51% of one core, and a single
new module plus the grid measured 0.67–1.12% (Rotary 0.99%, Uni-Vibe 1.12%, Doubler
0.84%). At 96 kHz the corresponding range was 1.32–2.29%, with an empty-grid cost
of 1.02%. These are offline measurements with default controls and synthetic
input, not DAW CPU or underrun guarantees. Use `nam-clap-bench <plugin> fx-28 1
"" ""` through `fx-38`, or `empty`, to reproduce the modes.
