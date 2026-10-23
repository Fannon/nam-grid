# Noise Gate (Smart)

An original MIT noise-profile expander, inspired by the public description of
frequency-selective gates and Learn in [Klevgrand Brusfri](https://klevgrand.com/products/brusfri).
No Klevgrand implementation, assets or proprietary processing details were used.
This does not claim Brusfri's sound, internal architecture or phase behavior.

## Try it

Build the targets using [build.md](build.md). The Windows artifacts are the normal
`build-grid/out/nam-grid.clap` and `nam-grid.vst3` bundle, with the existing NAM Grid
host identity. No separate experimental plugin is required.

1. Add **Dynamics > Noise Gate (Smart)** in a Pre slot, preferably before gain/NAM stages.
2. Leave the guitar/interface at normal playing settings. Mute the strings without
   turning the guitar volume down. Switch **Learn noise** on for 1–3 seconds, then
   off. Audio continues while learning. Digital silence and captures shorter than
   about 300 ms do not replace an existing valid print.
3. Start with the defaults. Increase **Sensitivity** to suppress more bands; lower
   it if sustain loses detail. **Reduction** limits attenuation, **Attack** opens
   bands, **Release** closes them, and **Mix** blends the spectral result.
   **Gate threshold** adds an optional final broadband gate: its minimum (-100 dB)
   displays **Off**, the default. Raise it just above the noise floor to silence
   pauses. It uses the same Release knob, a fixed 0.5 ms opening ramp, 20 ms hold,
   5 ms peak-detector decay and 3 dB hysteresis. Enabled occupies the last editor
   position, filling all eight positions with seven controls and bypass. The GR
   readout estimates band gain plus gate attenuation, capped at 120 dB; it is not
   a measurement of perceptual noise reduction.
4. Compare against bypass on low notes, quiet picking and decaying chords. Relearn
   after changing pickups, guitar volume, input gain or recording conditions.

Completed snapshots and the seven controls survive project recall and Copy/Move/Swap.
An in-progress Learn is saved/copied as off. During a new capture, the last completed
print remains the recallable snapshot. With Gate Off, untrained, settled Mix=0 and
Reduction=0 pass the exact finite input. Learning transitions to dry over 5 ms and
opens the final gate. Very large/invalid samples are bounded to prevent poisoning
the filter histories.

## DSP and real-time behavior

`NoisePrint.h` has 96 logarithmically spaced constant-Q resonators, Q=4, centered
from 5 Hz to 20 kHz (limited below Nyquist). Stereo channels share gain decisions
and retain independent filter histories. Learn averages each band's noise power;
a soft expansion knee compares the current envelope with that snapshot. A global
signal-confidence guard relaxes suppression during strong notes.

That guard compares current summed band power with the learned baseline. Once
current power exceeds four times the learned power, it smoothly raises the
minimum band gain toward 0.5 (about -6 dB); individual confident bands can still
open fully. This reduces coloration of strong notes while retaining the learned
profile and per-band detection for the return to quiet playing. The optional
final broadband gate remains independent. Relaxing reduction further would be
an audible algorithm change, rather than a CPU optimization.

The 2026-10-22 optimization processes independent pairs of bands with baseline
SSE2 on x64, retaining 96 bands, double precision and sample-by-sample detection,
gain smoothing and learning. Other architectures retain the scalar implementation.
There is no loud-signal bypass that suspends detector histories. The shared grid
also caches unchanged slot controls; parameter events invalidate the affected
bank, and host tempo is still refreshed every callback.

The optional final gate is independent of the snapshot and spectral Mix/Reduction:
it also works without learning. It detects the original input's maximum L/R peak,
applies one linked gain after the spectral mix, and closes linearly to exact zero
over Release after the hold. Either channel can open it, including anti-phase input.
An enabled gate starts closed after reset/recall to avoid an initial noise burst.
Turning Gate Off reopens it over 0.5 ms. Keep the module before delay/reverb to
preserve their tails. The standalone Noise Gate offers Natural and Tight modes.

Synthesis adds band corrections to a common-gain dry signal. Equal band gains
reconstruct that scaled signal exactly. With unequal gains the active IIR filters
can change amplitude and phase, and noise overlapping a wanted note remains hard
to remove. Resonator spacing and normalization are original implementation choices;
standard biquad equations do not imply an independently validated circuit model.

No FFT, lookahead, block buffering, callback allocations, file access or blocking
locks are introduced. Host-reported added latency is **0 samples**. Filter group
delay, envelope response and resonator ringing still exist; zero reported latency
does not mean instantaneous or phase-neutral processing.

Model exchange retains the existing ownership mechanism. A stage-lifetime atomic
mailbox publishes completed prints, with bounded reader retries and module epochs
to reject retired-module data. JSON parsing and validation happen on the control
thread. Reset preserves the completed print but clears envelopes/histories. The
centers remain in Hz across rate changes; relearning is sensible for best results
because the discrete filter responses change with rate.

## Verification and persistence

DSP and CLAP tests cover learning, dry paths, stereo linking, variable blocks,
reset, profile snapshots, malformed data, slot edits and native controls. Use
[build.md](build.md) for the test suite and [fx-performance.md](fx-performance.md)
for current CPU measurements. An ordinary gate is cheaper when only silence
between notes matters; assess pick attacks and long decays at matched levels.

Snapshots survive project recall, rig-preset save/load and Copy/Move/Swap. Learn
is transient: it is saved off, suppressed on rig-preset load and excluded from
performance scenes. Scenes adjust controls; the profile remains attached to
the slot. Unsupported profile data must reject without corrupting current state.

`NoisePrint.h` and `NoisePrintState.h` are independent of iPlug2. The standalone
numerical audit is `scripts/verify-noise-print.py`; `--reference REV` chooses a
historical implementation and `--scalar` verifies the non-SIMD fallback. It is an
on-demand developer check, not a substitute for guitar and DAW listening.
