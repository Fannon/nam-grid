# DSP aliasing checks

`nam-aliasing-tests` measures the actual stereo slot processing path for every
module. The normal CTest `aliasing` check uses insertion settings at 48 kHz. The
extended audit covers insertion and driven settings at 44.1, 48, 88.2 and 96 kHz, three
coherent input tones near 1.5, 6.5 and 10 kHz, and each registered algorithm
selector. Secondary selectors vary independently, not as a Cartesian product.
Driven profiles also exercise boosted Console EQ bands, the limiter's Input/Clip
path and LoFi Tape's Drive. Neutral Console EQ and zero-Drive LoFi settings alone
cannot establish the aliasing behavior of those nonlinear paths. New controls
that introduce or drive nonlinear processing need an explicit stress case.

Build it with `bash build.sh --test`, or build the `nam-aliasing-tests` target in
an already configured build directory. For an extended audit from the repository
root in Bash:

```sh
mkdir -p tmp/module-aliasing/reports
build/windows/release/NeuralAmpModeler/nam-aliasing-tests.exe . --full --reference-factor 4 \
  --report tmp/module-aliasing/reports/measurements.csv
```

The example uses Ninja; Visual Studio builds place the executable in the
additional `NeuralAmpModeler/Release/` subdirectory. Use the corresponding
`build/macos/release` or `build/linux/release` path without
`.exe` on those platforms. `--module 41` restricts an audit to one persistent
module ID. `--capture path/to/model.nam` replaces the bundled clean NAM capture;
NAM measurements describe that capture, not all trained models. Repeat
`--control "Limiter Input" 12 --control "Limiter Clip" 0`, for example, with
`--module 29` to isolate a particular control path. Overrides apply after the
insertion/driven profile and appear in CSV variant names. Only controls used by
the selected module are accepted; selectors use the registered variants above. Cabinet IR uses
a short, nontrivial synthetic impulse response so an unloaded slot cannot pass
as a linear processor. The program processes DSP directly and does not load the
plugin or access settings or the user's library.

## What the measurements mean

The input sine occupies an odd FFT bin, coprime with the power-of-two analysis
length. A half-second warm-up precedes two consecutive analysis windows. Each
window uses a rectangular FFT because the tone is coherent; no window skirts
are counted as distortion. Both channels are measured independently, with
different input amplitudes and phases. CSV amplitude is the left input's peak;
the right input is 0.71 times that amplitude. DC is excluded from output power.

- `residual_db_relative` measures all output energy outside DC and legitimate
  in-band harmonics, relative to total AC output power.
- `folded_db_relative` measures energy at distinct folded-harmonic bins for
  orders 2 through 63, relative to total AC output power. Bins coinciding with
  legitimate harmonics are excluded. This is a subset of residual energy, not
  an estimate of all aliasing: higher orders and collisions can be missed.
- `folded_rms_dbfs` gives the same selected-bin energy as an absolute RMS level
  relative to a sample value of 1. A full-scale sine has RMS level -3.01 dBFS
  under this convention.
- `folded_change_db` compares the last two windows. A change exceeding 1 dB
  requests a settling/modulation review rather than a stationary conclusion.
- `cpu_percent` is wall time spent in slot processing divided by rendered audio
  duration, at 128-frame blocks. It is diagnostic and machine-dependent, with
  no pass/fail budget. It includes two channels, not initialization or analysis.
- With `--reference-factor 2` or `4`, `reference_folded_rms_dbfs` measures
  those same physical-frequency bins in a separately rendered higher-rate
  processor. Its window contains proportionally more samples, so no decimator
  is needed to measure baseband bins. `reference_reduction_db` is native minus
  reference folded RMS level: positive means less energy at the higher rate.
  This is a convergence diagnostic, not the output of an implemented oversampling
  mode. `reference_residual_rms_dbfs` and `reference_residual_reduction_db`
  compare all non-harmonic energy within the original host bandwidth, including
  aliases that move to other bins as the rate increases. These are more useful
  for judging overall improvement than rejection at selected folded bins alone.
  Filter behavior, model-rate conversion, internal rate-dependent algorithms
  and stateful nonlinearities can change too. Dynamic/creative comparisons retain
  the same interpretation limits; four times the rate is not an alias-free oracle.
- `latency_samples` is the slot's reported compensation delay. All built-in
  modules must keep zero; model resampling may require compensation.

For harmonic processors, selected-bin energy above both -60 dB relative and
-80 dBFS is marked `investigate-aliasing` if it is stable between windows. These
are triage cutoffs, not audibility guarantees or release acceptance thresholds.
No fixture blesses current distortion as acceptable. A low score is not proof of
an alias-free processor. Post-filtering and dry/wet mixing can mask contamination;
compare actual playing, levels and settings before choosing a treatment.

Dynamics, modulation, delays and reverbs are diagnostic: intentional sidebands,
noise, changing envelopes and unsettled tails can occupy the same bins as folded
harmonics. Pitch effects and bitcrushing intentionally change the spectrum.
A high diagnostic result alone must not be reported as proven aliasing. For a
suspect algorithm, disable motion/noise where possible, settle its histories,
isolate the nonlinear stage and compare against an independently filtered
high-rate reference. Two-tone tests also require distinguishing legitimate
intermodulation from aliasing. The default sweep is a screen, not an exhaustive
parameter, transient or musical-content evaluation.

## Regression checks and shared analysis

`tests/SpectralAnalysis.h` supplies independent FFT power measurement, direct DFT
tone amplitude, harmonic folding and alias suppression measurement. Pedal Drive's
existing tests reuse this analysis. The spectral test checks the analyzer against
known clean, harmonic and aliased signals, checks linear modules for unexpected
energy, checks finite output and complete module coverage, and verifies block
partition independence for Distortion and Pedal Drive. A new module must declare
its spectral interpretation. All registered algorithms are enumerated at runtime.

The existing Pedal Drive requirement is retained: the 4x oversampler must suppress
a selected folded fifth from `tanh` by at least 40 dB against native processing.
This is a nonlinear processor test, separate from resampler stopband tests, and
is not a guarantee for every pedal voice, drive or input frequency.

## Zero added latency

Zero added reported latency excludes adding a fixed lookahead or compensation
delay to built-in modules. It does not exclude filtering. Ordinary EQ, minimum-
phase FIR and IIR filters are causal and introduce frequency-dependent phase and
group delay. Pedal Drive already uses minimum-phase FIR interpolation and
decimation with zero reported latency. Its wet signal is not phase-identical to
its unshifted dry path; parallel blends must be evaluated accordingly.

A linear-phase resampling filter normally adds a fixed group delay and is not a
suitable default for this goal. Distortion combines local minimum-phase
oversampling and first-order
antiderivative antialiasing (ADAA). ADAA has its own filtering and effective delay;
it is not a zero-group-delay substitute. The tone preview includes both FIRs'
complex response and ADAA's small-signal averaging, including partial Mix phase.
The [compatibility contract](development.md#parameter-and-project-compatibility)
allows measured DSP quality improvements during 0.x while keeping parameter
contracts and saved-state loading stable.

Pedal Drive currently uses a fixed 4x multiplier at every host rate. For example,
44.1 kHz runs its driven path at 176.4 kHz, while 88.2 kHz runs it at 352.8 kHz.
A future rate-aware mode could use 4x at 44.1/48 kHz and 2x at 88.2/96 kHz to
keep a similar internal rate. Equal internal rates do not guarantee identical
output: interpolation bandwidth, phase and input bandwidth can differ. Any such
change must follow the sound-quality policy above.

Distortion already uses the rate-aware 4x/2x/1x policy. Its static waveshaper
curves and controls are unchanged; the time-domain rendering is deliberately
cleaner. Extreme Foldback still needs particular care, since its many folds can
produce substantial content above even the elevated internal Nyquist rate.

For a reproducible comparison of Distortion's factors (1x/2x/4x/8x), with and
without ADAA, run the executable with `. --distortion-study --report
tmp/module-aliasing/reports/distortion-study.csv`.
This uses all six styles, 48/96 kHz, three test tones and two drive levels and
reports absolute/relative non-harmonic energy and processing time. Explicit
factor and ADAA overrides are offline experiment hooks, not plugin parameters.
The standard test asserts alias reduction against the old native shaper at all
six common rates through 192 kHz, alongside the generalized pedal checks.

References: [DAFx: Antialiasing Piecewise Polynomial Waveshapers](https://dafx.de/paper-archive/2023/DAFx23_paper_61.pdf)
and [JUCE oversampling documentation](https://docs.juce.com/master/classjuce_1_1dsp_1_1Oversampling.html).

## Additional drive treatments

Tube Boost uses local 4x/2x/1x minimum-phase oversampling at the same host-rate
bands as Distortion, with its pre-EQ and post/DC filtering at host rate. Tape and
Console Colour use a lower-cost 2x treatment below 88.2 kHz and native cores at
higher host rates. Their 32-base-tap FIR designs do not change the established
64-base-tap Distortion/Pedal filters. Stereo core resampling keeps corresponding
high-rate channel samples together. Tape uses the original left-channel math
without evaluating a discarded duplicate right channel, and batches its core in
bounded control spans; neither adds an audio buffer delay.

Rodent keeps the existing 4x resampler. Its input saturation uses first-order
ADAA and its slew limiter tracks a line between target samples, integrating the
following clip across any catch-up boundary. This addresses earlier nonlinear
stages rather than merely filtering after the final clip. Other pedal voices
retain their DSP. Test interval averages against numerical integration, including
near-zero intervals and curve-cache initialization.

A partial Mix is a dry/wet blend across a path that has passed through
minimum-phase filters, so its phase differs from the unshifted dry signal. Tape
and Console Colour therefore combine the dry reference and the trimmed wet inside
the oversampled stage, before decimation; the blend stays time-aligned and Mix 0%
still returns the exact dry sample. Tube Boost keeps its high cut and DC blocker
at the host rate, so its partial Mix does carry the filters' phase shift and can
comb above roughly 3 kHz. Because Tape and Console Colour run natively from
88.2 kHz, they have no filter-related dry/wet group delay at those host rates.
The 32-base-tap pair is flat to about 18 kHz and attenuates about 2.3 dB at
20 kHz for a 48 kHz host; the tube response preview draws that response.

Run `. --drive-study --report tmp/module-aliasing-followup/reports/drive-study.csv`
for the alternatives each shipped treatment supports: Tube Boost at 1x/2x/4x, Tape
and Console Colour at native or 2x, and the Rodent at its fixed 4x with the local
treatment on or off. `. --voicing-study --report ...` compares quiet
fundamental gains against native/legacy processing from 20 Hz through 18 kHz.
These factor and treatment switches are offline test hooks, not plugin parameters;
production always selects the rate-based factor with the treatment enabled. The normal aliasing
CTest also checks quality improvement against native/legacy references. All wet
filters and interval averages can change phase, transients and partial Mix; zero
reported latency is not a claim of zero group delay or identical existing sound.
