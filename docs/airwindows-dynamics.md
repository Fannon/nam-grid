# Digital Black Gate

Digital Black Gate is an adapted Airwindows dynamics module; the other native mappings
are in [airwindows-outboard.md](airwindows-outboard.md). It belongs in Dynamics and has
three knobs, Enabled, a header Mode selector and its own stereo state.

Pressure6 was evaluated and left out. Its threshold is fixed and set by the Compress
amount (`1 - (0.9 A)^3`), so at guitar levels most of the knob does nothing and the
last tenth pumps hard. Butter Comp and Outboard Channel cover that character with
usable control ranges.

The MIT sources are DigitalBlack and CStrip's gate section at the same
[`e68f75154ed0520fc5757fec9e7792dd1747e5ee`](https://github.com/airwindows/airwindows/tree/e68f75154ed0520fc5757fec9e7792dd1747e5ee/plugins/LinuxVST/src)
revision as the first batch. `scripts/vendor-airwindows-outboard.py` generates the
SDK-free double paths and extracts CStrip's gate alone; it imports none of that
strip's EQ, lowpass, compressor or TimeLag code. MIT notices ship with the plugin.

## Digital Black Gate

| Control | Range | Default |
| --- | --- | --- |
| Threshold | −74…−10 dB | −55 dB |
| Hold | 0…200 ms (NAM Grid addition) | 0 ms |
| Mix | 0…100% | 100% |
| Output | −18…+6 dB, after Mix | 0 dB |
| Mode (header) | Zero crossing / Hysteresis | Zero crossing |

Threshold uses the native amplitude expression `A^4 / 3 + 0.00018`, inverted as
`A = (3 * (10^(dB/20) - 0.00018))^0.25`. The native endpoints correspond to
approximately −74.9…−9.54 dB, so a −90 dB knob would claim a range it cannot reach.
The chosen −74…−10 dB range stays inside both cores' native thresholds. Neither core has
attack, release or reduction controls; use Enabled to bypass.

**Hold** is NAM Grid's only addition to the native gates. Both cores compare every sample
with Threshold, so noise whose peaks hover around it reopens the gate on each peak and
sounds like a Geiger counter. Hold keeps the gate's roller from falling for that long after
the gate last triggered, then the native release continues. At 0 ms (the default) the code
path is exactly the native one.

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

## Verification and cost

The DSP suite checks all five rates, both gate modes, exact dry, stereo, full
range automation, block invariance, zero processing allocations, true gate
silence and gate closure consistency. Host tests cover the mode choices, native
gestures, state recall and both ends of the parameter bank.

Use `nam-outboard-audition` for isolated stereo-wrapper measurements or the
[FX performance tool](fx-performance.md) for plugin callback measurements.
The module reports zero compensation latency. Continuous controls smooth over
20 ms; coefficients update every 32 samples on a persistent clock. Histories
drain with silence for one second on bypass before suspension.

Older named states receive descriptor defaults for missing controls while
retaining existing values. See [development.md](development.md) for compatibility.
Guitar listening in a DAW remains the final voicing check.
