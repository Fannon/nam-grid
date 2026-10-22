# Digital Black Gate and Pressure — 2026-10-22

Priorities five and six from [AIRWINDOWS_CANDIDATES.md](AIRWINDOWS_CANDIDATES.md)
are implemented alongside the [first four outboard modules](AIRWINDOWS_OUTBOARD.md).
Both belong in Dynamics and have three knobs, Enabled, and their own stereo state.
Digital Black Gate also has a header Mode selector.

The MIT sources are DigitalBlack, CStrip's gate section and Pressure6 at the same
[`e68f75154ed0520fc5757fec9e7792dd1747e5ee`](https://github.com/airwindows/airwindows/tree/e68f75154ed0520fc5757fec9e7792dd1747e5ee/plugins/LinuxVST/src)
revision as the first batch. `scripts/vendor-airwindows-outboard.py` generates the
SDK-free double paths and extracts CStrip's gate alone; it imports none of that
strip's EQ, lowpass, compressor or TimeLag code. MIT notices ship with the plugin.

## Digital Black Gate

| Control | Range | Default |
| --- | --- | --- |
| Threshold | −74…−10 dB | −55 dB |
| Mix | 0…100% | 100% |
| Output | −18…+6 dB, after Mix | 0 dB |
| Mode (header) | Zero crossing / Hysteresis | Zero crossing |

Threshold uses the native amplitude expression `A^4 / 3 + 0.00018`, inverted as
`A = (3 * (10^(dB/20) - 0.00018))^0.25`. The native endpoints correspond to
approximately −74.9…−9.54 dB, so a −90 dB knob would claim a range it cannot reach.
The chosen −74…−10 dB range stays inside both cores' native thresholds. No invented
Release, Hold or Reduction controls. Use Enabled to bypass.

Zero crossing retains DigitalBlack's pitch-dependent opening/closing and its
rectified `1-cos` fill. A quiet tail darkens before becoming exactly silent.
This is a different texture from the existing envelope gates, useful after fuzz
as well as for noise control. Hysteresis retains CStrip's re-arming behaviour:
the active gate needs 1.1× the opening threshold to refresh its roller, and it
becomes exactly silent when that roller reaches zero. It is not a conventional
hold/release gate or a replacement for Natural/Tight.

**The candidate table's rate audit was incomplete.** DigitalBlack's release is
scaled but its zero-crossing counter is host-sample-counted. Scaling only release
still lengthens its closing time at higher rates. The port counts reference
samples (`44100 / hostRate`) in doubles and retains the 6000-reference-sample cap.
CStrip's gate needs both this counter treatment and its previously unscaled
release multiplied by `44100 / hostRate`. CStrip's integer truncations at its
reset/cap values (66/220) remain exact at 44.1 kHz. Its local seeded denormal
guard is retained: even that tiny positive signal affects the zero-crossing
state and which half-cycle first opens the gate from silence. Mode changes fade through dry over 10 ms each
way, reset only the small gate histories and allocate nothing.

**Opening is not applied gain.** The original note incorrectly equated the
`gate` multiplier with gain reduction, overlooking the rectified fill and final
hard silence. The display instead measures actual stereo amplitude attenuation
from input and blended output energy, before Output; zero crossings use the
native opening as a stable fallback. The meter floors at 120 dB for exact silence.
It is labelled **Atten**, with complete live IN/OUT histories alongside it.

## Pressure

| Control | Range | Default |
| --- | --- | --- |
| Compress | Native 0…100% | 90% |
| Mix | Conventional outer 0…100% | 100% |
| Output | −18…+6 dB, after Mix | −2.5 dB |

One Pressure6 core processes both channels using `max(abs(L), abs(R))`. Running
two dual-mono copies would lose its central benefit: both channels share the
same detector gain. Its sine stage remains independent per channel and can
colour their peaks differently.

The upstream **Ratio** is not a ratio, and it is not even a conventional mix:
`wet = 0.9 B`, followed by `dry*(1 - 1.1*wet) + sin(clamp(x*wet))*wet`.
The port fixes native B at 1, preserving that nonlinear voice, and adds a true
outer Mix. Mix 0 with Output 0 dB is exactly dry; Mix 100 retains the native
processed voice, including its tiny internal dry component. Compress 0 retains
the sine colour, as upstream; it is not a bit-transparent bypass. The detector
is nonlinear and can recover when driven very hard, so this is a character
compressor rather than a peak guard.

**Pressure6 also needed a deeper rate audit.** `adjSpd` already scales `muSpd`
with rate, but its recovery divides by `muSpd²`, inadvertently squaring the rate
scaling. Recovery becomes progressively slower at higher rates. The adaptation
divides that squared denominator by `overallscale`, preserving the 44.1 kHz law
and reference-time recovery. Before that correction the same default stimulus
lost over 2 dB more at 192 kHz than at 44.1 kHz. After correction the measured
default offsets are +0.063/+0.019/+0.064/−0.034/+0.016 dB at
44.1/48/88.2/96/192 kHz. Controls and the native attack law retain their ranges.

The GR tap is `previousMu * currentMu²`, before the automatic `1/threshold`
gain, sine output and wrapper Output. This avoids confusing makeup or clipping
with detector reduction. The display shows that detector GR and complete IN/OUT
histories. The measured plucked stimulus reaches approximately 4.7 dB detector GR
with nearly unity overall RMS, making the default audible without a level jump.

## Verification and cost

The DSP suite checks all five rates, both gate modes, exact dry, stereo, full
range automation, block invariance, zero processing allocations, true gate
silence, gate closure consistency and Pressure's stereo linking. A recovery probe
after a right-only burst catches the squared-rate recovery bug. Host tests cover
the new mode choices, native gestures, both ends of the parameter bank and
schema-24 migration.

`nam-outboard-audition` measures the isolated stereo wrappers in 128-frame blocks,
including its stimulus/analysis and excluding host/grid overhead. Local Release
figures: Digital Black 0.31/0.59%, Hysteresis 0.24/0.48%, Pressure 0.40/0.77% of one
CPU core at 48/96 kHz. Both report zero compensation latency. Continuous controls
smooth over 20 ms; coefficients update every 32 samples on a persistent clock.
Histories drain with silence for one second on bypass before suspension.

Schema 25 appends seven controls after schema 24's outboard bank (201 utility
controls per slot). Older states retain their previous values and receive native
defaults for the missing controls. The editor and reset use the same descriptor
defaults. Guitar listening in a DAW remains the final voicing check.
