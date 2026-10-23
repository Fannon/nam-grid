# Adaptive cabinet convolution

`AdaptiveIR` is the cabinet processor for Cabinet IR slots. The WAV decoder, cubic resampling, rate-dependent gain, maximum 8192 resampled
taps, mono-kernel API and independent L/R histories are retained. Parameters,
module IDs and saved state are unchanged. No IR Lite control is introduced.

## Preparation and processing

Construction happens on control/stopped paths before publishing a processor. The
same raw `ImpulseResponse::IRData` is retained for rate/block-size reconstruction.
The resampled effective length chooses the backend:

- At most 64 taps: the existing AudioDSPTools direct processor, primed offline.
- Longer responses: the MIT FFTConvolver uniform implementation, with every
  nonzero coefficient preserved, including tails quieter than 1e-6.

The partition is the largest power of two up to the host maximum block and 128,
with a minimum of 32. A 127-frame host maximum therefore selects 64; host maxima
below 32 use a 32-sample partition with partial input calls. This partition does
not impose buffering latency: output for each call is delivered immediately.
These conservative defaults balance FFT overhead against direct convolution work;
the threshold is not necessarily optimal on every CPU. Use the
[engine comparison tool](ir-convolution-comparison.md) to measure alternatives.

All FFT plans, spectra, float adapters and double output buffers are allocated
before audio. Each call uses reserved buffers even when frames vary or modulation
subdivides the host block. The existing control-thread publication/reclamation,
replacement fades, bypass mixing and 8192-sample cabinet draining remain in place.
Reactivation reconstructs the processor for the new rate and block maximum.

The upstream uniform convolver is vendored at revision
`f2cdeb04c42141d2caec19ca4f137398b2a76b85`. The only local algorithm edit changes
tail trimming from a magnitude threshold to exact zeros. Individual copyright
headers and COPYING.txt are preserved; the shipped ThirdPartyNotices includes
its MIT text. The bundled Ooura FFT is used with float spectra/output and double
transform work; SSE accumulation is enabled on x64. Portable code is retained
for other architectures. WDL and the two-stage convolver are not production dependencies.

## Shared processing

The accompanying grid changes remove repeat work while retaining its state:

- Crossover frequency smoothing and filter histories still advance for every
  sample; the exponential coefficient is reused only while that frequency is
  exactly unchanged. Reset invalidates it for the new sample rate.
- Tail reporting avoids slot parameter reads when the freeze-capable module mask
  is empty. Freeze/Shimmer active/hold semantics are retained.
- Modulation reuses the host tempo snapshot already taken for that callback.

No empty-grid bypass shortcuts skip slot staging, routing smoothing, metering or
tuner activity. The remaining cost is small; a broader routing rewrite is not
justified by the measured savings alone.

## Verification

`nam-ir-tests` is a normal correctness test (`ir-convolution` in CTest). It checks
four rates, the direct/FFT boundary, resampling/capping, variable blocks down to
one frame, quiet tails, silent IRs, original mono-kernel channel behavior, original
raw-data identity, and no processing new/delete or Eigen allocation. The existing
stage suite covers stereo histories, tail draining and realtime stage ownership.

The [on-demand waveform audit](fx-performance.md) compares actual before/after
CLAP output through live routing/EQ/tuner controls and IR bypass/level/modulation
events. Whole-plugin CPU measurements use the normal module benchmark with exact
assets/settings and preserved reference binaries. DAW playback and macOS/ARM
performance remain separate validation steps.
