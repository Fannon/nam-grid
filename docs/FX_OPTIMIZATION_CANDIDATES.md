# Further FX optimization candidates

Investigation on 2026-10-22, after the [Smart Gate/grid optimizations](SMART_GATE_OPTIMIZATION_20261022.md).
No convolution replacement has been implemented or measured yet.

## Priorities

1. **Cabinet IR convolution.** The strongest next candidate. In the original two
   sweeps, the longer IR used 14.81–14.82% of the callback budget at 96 kHz / 32
   frames. Even the 2,048-sample, 48 kHz source IR used 8.73–8.74% at that host
   rate. These figures include the old grid overhead; rerun against the optimized
   grid before comparing a replacement.
2. **NAM inference.** Real full A2 captures used roughly 11% at the same settings
   in the earlier sweep. This is model-dependent; compare the same captures and
   quality mode when optimizing. A2 Lite already provides a cheaper alternative,
   but selecting it changes the inference model, unlike an equivalent kernel
   optimization.
3. **Diffused Delay, then Multi-head Tape/Console Colour.** Earlier maximum median
   callback costs were about 5.1%, 3.8–4.1% and 3.9–4.2%, respectively. Profile
   individual modules after the shared control-cache improvement before deciding
   whether their own DSP is expensive enough to justify changes.

Reverbs remain a lower priority because cheaper alternatives are available.
Occasional deadline spikes under mixed load alone do not establish a DSP defect.
See the [original measurement report](FX_PERFORMANCE_REPORT_20261022.md) for
coverage and limitations. The values above are historical, not post-optimization
measurements of these modules.

## Why the current IR path is a candidate

[`AudioDSPTools/dsp/ImpulseResponse.cpp`](../AudioDSPTools/dsp/ImpulseResponse.cpp)
uses an Eigen float-vector dot product for every output sample. The plugin runs
independent convolvers for left and right audio. Cost scales with the number of
resampled taps, capped at 8,192. Its WAV decoding/resampling is prepared outside
steady-state processing; replacing the convolution engine is the relevant CPU
experiment, rather than replacing the WAV loader.

## Permissive engines to compare

| Engine | Published license | Fit and points to verify |
|---|---|---|
| [HiFi-LoFi FFTConvolver](https://github.com/HiFi-LoFi/FFTConvolver) | [MIT](https://github.com/HiFi-LoFi/FFTConvolver/blob/master/COPYING.txt) | Small C++ implementation with bundled FFT and optional SSE. Its [uniform convolver](https://github.com/HiFi-LoFi/FFTConvolver/blob/master/FFTConvolver.h) documents immediate output for arbitrary input lengths and allocation-free processing after initialization. Start with this simple baseline; compare its [two-stage variant](https://github.com/HiFi-LoFi/FFTConvolver/blob/master/TwoStageFFTConvolver.h) for longer IRs, checking callback peaks as well as means. |
| [Cockos WDL convolution engine](https://www.cockos.com/wdl/) | Permissive zlib-style notice in [convoengine.h](https://github.com/justinfrankel/WDL/blob/main/WDL/convoengine.h) | Already present in the pinned iPlug2 dependency. Offers direct and partitioned FFT convolution; compare `WDL_ConvolutionEngine_Div` with zero allowed latency. The basic engine reports FFT buffering latency, so it needs careful configuration rather than a blind replacement. |

The recommendation is to benchmark both against the current direct implementation.
Partitioned FFT processing is a promising way to reduce long-IR work, but the
crossover for short IRs and the best partition sizes must be measured locally.
No speedup percentage can be claimed from library descriptions.

## Acceptance for a replacement

- Preserve onset alignment, current gain/sample-rate scaling, resampling, stereo
  independence, state recall and the current truncation policy for the comparison.
- Check output error against direct convolution, including impulse response,
  silence/tails, variable host blocks, reset and IR replacement. FFT arithmetic
  changes rounding; quantify the difference rather than expecting bit identity.
- Run the [individual-module benchmark](FX_PERFORMANCE.md) at 32/64/128 frames,
  44.1/48/96 kHz, short/long synthetic IRs and real cabinet files. Repeat both
  preserved binaries under comparable load and compare mean, p99 and worst calls.
- Prepare FFT plans/buffers off the audio thread; verify no processing allocations
  or locks. Prefer a single audio-thread implementation initially, then validate
  driver/DAW behavior separately.
