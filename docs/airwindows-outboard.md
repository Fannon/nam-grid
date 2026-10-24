# Airwindows outboard implementation â€” 2026-10-22

This page documents Outboard Channel, Console Colour, Console EQ and Butter
Comp: source pins, deliberate adaptations and control mappings. See
[airwindows-dynamics.md](airwindows-dynamics.md) for Digital Black Gate.

Sources are the double-precision LinuxVST paths of CStrip2, BussColors4 and
ButterComp2 at
[`e68f75154ed0520fc5757fec9e7792dd1747e5ee`](https://github.com/airwindows/airwindows/tree/e68f75154ed0520fc5757fec9e7792dd1747e5ee/plugins/LinuxVST/src).
The SDK-free headers and MIT license live in `dsp/airwindows/outboard/`.
Reproduce them with `python scripts/vendor-airwindows-outboard.py`; select cores
with `--plugins`, or use `--source-dir` with cached upstream files. The existing
reverb generator is unchanged.

## Critical decisions

**CStrip2 and ButterComp2 are related, different compressors.** CStrip2 uses
`1 + 35 H^4` input gain, a user-controlled recovery coefficient and golden-ratio
sample averaging in its detector. ButterComp2 uses `10^(14 A / 20)` input gain,
`0.012 A / 135 / (1 + abs(lastOutput))` recovery, different automatic output
compensation and no such averaging. Keep both upstream designs; do not promise
that the strip is ButterComp2 with filters. Both retain separate L/R detectors.

**Native Capacitor percentages are more honest than invented cutoff Hz.** Both
cuts expose 0–100% rather than assigning an unmeasured single corner to a
rotating, three-stage ladder. Low cut 0 is off; High cut 100 is open. High cut
decreases as the control is turned down. The native coefficient is `p²`; the port
uses `1 - (1 - p²)^(44100 / hostRate)` to preserve pole time constants, with both
endpoints exact. Rotation remains sample-clocked, so this is compensation rather
than a claim of identical transfer functions at every rate. No RBJ replacement.

**Console EQ's frequency numbers are coefficient scales.** Preserve the DSP
ranges: Treble 1000–16000 Hz and Bass 300–16000 Hz. Their inverse mappings are
`sqrt((f - 1000) / 15000)` and `sqrt((f - 300) / 15700)`. They do not describe
measured shelf corners. The band controls retain the DSP's nominal ±6 dB range;
their sine/cosine shaping makes the actual gain level-dependent. Flat with cuts
open is exactly dry, including digital silence. Bass defaults to 300 rather
than upstream's surprising 4225 coefficient scale, so the first Bass adjustment
works in the useful lower part of its native range. Treble retains upstream's
4750 scale. The display measures the strip's quiet-signal response from an impulse
through its own DSP and draws it over the live output spectrum; louder input adds band
density that this linear curve leaves out.

**Outboard Channel retains the native unity-to-Spiral section.** EQ is flat;
Capacitor precedes the native compressor. Saturate maps to `J = (1 + s) / 3`,
so its range drives 1–2× into the original Spiral crossfade. The Density third
is omitted. Output is a separate conventional gain after Mix. Saturate changes
drive as well as colour; it is not a constant-loudness blend. At Compress 0,
Saturate 0, cuts off/open and Output 0 dB, nonzero input is unchanged.

**Console Colour trims use actual dB.** BussColors4's upstream numeric trims
use `/14`, not `/20`; carrying those numbers across as dB would be misleading.
Its native trims stay at centre while wrapper Input and Output trims each cover
±18 real dB. Both affect only the wet path, so Mix 0 is bit-transparent at any
trim. The eight upstream voices and built-in per-voice gains are retained, with
additional fixed compensation for default-level comparison (see calibration below).
Desk changes fade to dry over 10 ms, reset the small FIR/sag history, and fade
back over 10 ms. They allocate no memory. This prevents old-desk convolution
history leaking into the new desk.

**ButterComp2 noise is local and deterministic.** Its original function-static
residue counters share state between instances and use signed-overflow integer
squaring. The port instead uses the core's local seeded PRNG for denormal
protection, matching the newer Airwindows cores. This deliberate tiny-noise
deviation removes shared mutable state and undefined arithmetic. The compressor
math is retained. Output maps to −18…+6 real dB after Mix, rather than the native
0…2 linear output multiplier; the internal output remains unity so the native
output-dependent recovery follows the compressed signal.

## Defaults and feedback

| Module | Insertion settings | Display |
| --- | --- | --- |
| Outboard Channel | Low cut 0%; High cut 100%; Compress 40%; Comp speed 25%; Saturate 15%; Output −2 dB; Mix 100% | Measured cut response over the output spectrum, Spiral saturation curve and detector gain-reduction history |
| Console Colour | Dark desk; Input/Output trim 0 dB; Mix 100% | Desk name and live stereo peak IN/OUT histories |
| Console EQ | Bands 0; Treble scale 4750 Hz; Bass scale 300 Hz; High cut 100%; Low cut 0% | Measured quiet-signal response over the live output spectrum |
| Butter Comp | Compress 35%; Output −1 dB; Mix 100% | Detector gain-reduction history beside live peak IN/OUT histories |

Compressor percentages are intensity/speed, not threshold, ratio, or milliseconds.
The GR tap reads the native per-sample multiplier, blended by Mix, before automatic
gain recovery, saturation and user output. The larger L/R attenuation is shown.
IN/OUT histories include the complete effect, automatic gain recovery and output.
They must not be labelled "no makeup". Colour's dynamic convolution cannot be
represented by a static waveshaper curve.

Continuous controls smooth over 20 ms. Core coefficients update every 32 samples
using a persistent sample counter; host block partitioning does not affect that
clock. The selected stereo core is allocated only during preparation. All four
report zero latency. Bypass drains silence for one second before suspension;
very slow native detector/filter state can persist. Menu membership follows the player decision: Distortion for Colour,
EQ / Filter for Console EQ, Dynamics for the strip and Butter Comp.

Host automation and saved-state compatibility are described in
[development.md](development.md#parameter-and-project-compatibility).

## Verification and measurement

`OutboardTests.h` exercises five host rates (44.1/48/88.2/96/192 kHz), continuous
extremes, every desk, variable blocks, exact dry paths, independent stereo and
zero allocations/deletions in processing. CLAP tests check host choice metadata,
automation and state recall at both ends of the slot bank.
The catalog and presentation tests automatically include all four modules.

Build and run `nam-outboard-audition` for a reproducible 128-frame stereo workload
and stimulus-specific RMS gain/GR. It includes signal generation and analysis,
uses the best of two measured runs after warm-up, and excludes plugin/host
overhead. Console EQ is measured with all three bands engaged, rather than its
flat insertion setting. Measurements and guitar listening are separate checks.

## Desk calibration

The fixed compensation in Dark/Rock/Lush/Vibe/Holo/Punch/Steel/Tube order is
`+0.8841, +0.6937, −1.3250, +1.0910, +0.5984, +0.7611, +2.7876, +1.1550 dB`.
It cancels each voice's 48 kHz offset for the tool's plucked stereo harmonic
stimulus. All eight remain within 0.08 dB of unity for that stimulus across all
five rates. This does not flatten their spectral differences or guarantee equal
perceived loudness on other material.
