# Zero-latency optimization, 2026-10-21

Baseline: `214efc7`, on the isolated `experiment/noise-print` branch. No change to
parameters, state schema, profile format, detector/gain update rate or latency.
No new runtime dependency or required CPU instruction set.

The change:

- Compute threshold reciprocals and the total learned-power reciprocal once per
  callback, replacing divisions inside the sample/band loops with multiplies.
  Learning also uses one reciprocal sample count per sample instead of one divide
  per band.
- Store resonator coefficients and states in contiguous band arrays to help the
  compiler vectorize independent filters. Remove redundant zero-initialization
  of scratch arrays that are fully written before reading.
- Skip expander target calculations when learning/untrained and skip output
  synthesis when it is inaudible. Filters, envelopes and gain histories stay warm;
  the final gate keeps running. Returning from dry/Learn therefore retains the
  existing transition behavior.

## Measurements

Windows x64 Release, MSVC 19.44, Ryzen 9 5950X. Same CLAP host, stereo 128-frame
blocks, 1,000 callbacks per run; fastest of three measured runs after warmup.
Baseline and optimized binaries were run sequentially, alternating by scenario.
Figures include the grid and are percentages of one core's available audio time.

| Scenario | 48 kHz before | 48 kHz after | 96 kHz before | 96 kHz after |
| --- | ---: | ---: | ---: | ---: |
| Trained, processing | 3.74% | 3.26% | 7.28% | 6.37% |
| Untrained | 2.79% | 2.20% | 5.49% | 4.31% |
| Trained, Mix=0 | 3.71% | 2.83% | 7.31% | 5.43% |
| Learning | 3.03% | 2.34% | 6.06% | 4.62% |

Normal processing uses approximately 13% less CPU; the other scenarios save
approximately 21–26%, depending on rate. Reported added latency remains **0 samples**.
These are local minimum-time comparisons, not worst-case DAW performance claims.

## Numerical and lifecycle checks

`scripts/verify-noise-print.py` compiles the current header alongside a generated
reference from the committed baseline. It compares 3,090,210 mono/stereo samples
at 44.1/48/96/192 kHz, varying blocks of 1/7/17/31/128/512 frames, control changes,
Learn/relearn, dry/resume, gate, invalid-input recovery and sample-rate reset.

- Maximum absolute sample difference: `6.94e-17`.
- Maximum relative learned-band-power difference: `4.21e-16`.
- Maximum GR-meter difference: `8.0e-15 dB`.

The reciprocal multiplications can change the last few floating-point bits.
This is a CPU optimization; the noise-reduction quality and transient/sustain
tradeoffs remain those of the existing prototype. The ordinary four CTest suites
also cover audio allocations, block invariance, bypass, profile recall, real CLAP
automation and native editor interaction.

Reproduce in a C++20 compiler environment, with full Git history available:

```powershell
python scripts/verify-noise-print.py
ctest --test-dir build-grid -C Release --output-on-failure
```

The audit uses MSVC `cl` when available, or `c++`; `CXX` can name another compiler
executable. Generated sources/objects stay in `build-grid/noise-optimization`.
The CLAP benchmark accepts `fx-64`, `fx-64-untrained`, `fx-64-dry` and `fx-64-learn`.
For example, use the command in [NOISE_PRINT.md](NOISE_PRINT.md) and replace its
mode argument. Preserve an older binary outside host scan paths for comparisons.

## Latency constraint

Keep the denoiser at zero added latency by default. If a future optional lookahead
is justified by listening tests, target 2–3 ms and cap additional denoising latency
at 5 ms along an active signal path, accounting for multiple serial modules.
Host/interface buffering and existing NAM rate-conversion latency are separate
contributors to monitoring delay. This optimization introduces no lookahead.
