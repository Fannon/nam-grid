# Smart Gate and grid CPU optimization — 2026-10-22

Two fresh before/after rounds measured trained Smart Gate at 96 kHz / 32 frames at **8.62% → 4.52%** of one audio thread's buffer budget: **47.5% lower callback cost**. The empty grid fell from **2.45% → 1.07%**, a **56.3% reduction**.

These are averages of the two rounds' medians of three repeated callback means. Both rounds used the same preserved reference binary and the same optimized binary. The machine was under mixed load, so deadline spikes remain scheduling observations rather than a diagnosis of audible DAW pops.

Machine: AMD64 Family 25 Model 33 Stepping 2, AuthenticAMD; Windows-10-10.0.26300-SP0; 32 logical CPUs. Windows x64 Release CLAP, stereo float64, unpaced offline rendering.

## Changes

- Smart Gate processes pairs of its 96 independent bands with baseline SSE2 on x64. Filter/energy work is combined and gain/synthesis operations use double-precision SIMD. Detector, envelope and gain updates still run for every sample. No AVX/FMA requirement, fewer bands, lookahead, new latency, reduced precision or loud-signal bypass was introduced. Other architectures keep the independently verified scalar fallback.
- The grid retains an audio-owned control snapshot for each slot. Parameter callbacks publish a lock-free revision for the affected bank; the audio thread reloads that bank only after changes. Changes during a refresh remain pending for the next callback. Preset/reset invalidation, legacy input/trim aliases, live modulation and host-tempo refresh are retained. This avoids rereading thousands of unchanged parameter values and rebuilding all slot control structures every callback.
- The on-demand benchmark now supports exact module selection and matching before/after comparisons in Markdown, CSV and JSON. Reverb and IR algorithms were left unchanged.

## Measurements

48/96 kHz × 32/64/128 frames × three repeats; 1 s warm-up followed by 5 s measured audio (75% stereo excitation, 25% silence/tail). Trained, learning and untrained Smart Gate paths were tested, alongside the empty grid. Each before/after round contains 72 + 72 measurements.

| Case, 32 frames | Hz | Before round 1 | After round 1 | Before round 2 | After round 2 | Reduction |
|---|---:|---:|---:|---:|---:|---:|
| Empty grid | 48000 | 1.180% | 0.543% | 1.200% | 0.544% | 54.3% |
| Empty grid | 96000 | 2.478% | 1.081% | 2.414% | 1.058% | 56.3% |
| Smart Gate, trained | 48000 | 4.283% | 2.294% | 4.284% | 2.267% | 46.8% |
| Smart Gate, trained | 96000 | 8.433% | 4.541% | 8.811% | 4.506% | 47.5% |
| Smart Gate, learning | 48000 | 3.270% | 1.824% | 3.603% | 1.837% | 46.7% |
| Smart Gate, learning | 96000 | 6.343% | 3.639% | 7.129% | 3.631% | 46.0% |
| Smart Gate, untrained | 48000 | 3.121% | 1.745% | 3.184% | 1.736% | 44.8% |
| Smart Gate, untrained | 96000 | 6.151% | 3.476% | 6.190% | 3.474% | 43.7% |

Trained Smart Gate across all tested buffers:

| Hz | Frames | Before CPU % | After CPU % | Total reduction |
|---|---:|---:|---:|---:|
| 48000 | 32 | 4.284 | 2.280 | 46.8% |
| 48000 | 64 | 3.825 | 2.177 | 43.1% |
| 48000 | 128 | 3.579 | 2.101 | 41.3% |
| 96000 | 32 | 8.622 | 4.524 | 47.5% |
| 96000 | 64 | 7.676 | 4.302 | 44.0% |
| 96000 | 128 | 9.893 | 4.213 | 57.4% |

Subtracting the matching empty-grid medians estimates **44.1% lower incremental Smart Gate DSP cost** at 96 kHz / 32 frames. This subtraction is approximate because the cases run in separate processes.

A separate intermediate build containing only the control cache reduced empty-grid cost from 2.58% to 1.07% and trained Smart Gate from 9.00% to 6.96% at 96 kHz / 32 frames. The SIMD change supplies the additional Smart Gate reduction. Its raw data is in [the cache-only comparison](../build-grid/smart-gate-cache-only/comparison.md).

## Correctness and automation checks

- The independent numerical audit compared 3,090,210 mono/stereo samples at 44.1/48/96/192 kHz against commit `8c4c451`'s pre-change implementation, with variable blocks, learning transitions, changing attack/release/mix, a final gate, reset and invalid-input recovery. Maximum output difference: **3.33 × 10⁻¹⁶**. Learned profile powers were **identical**; maximum meter difference was **1.75 × 10⁻¹⁴ dB**. The forced scalar fallback matched output, profiles and metering exactly.
- The DSP regression suite passed, including noise-only reduction, wanted-tone error checks, exact dry paths, block partition invariance, allocation/deletion tracking, routing, modulation and module lifecycle checks.
- The headless CLAP suite passed, including actual Learn/gate automation and preset recall. A new regression verifies that live input, trim and utility-gain automation reaches DSP in all 16 slots. Compile-time checks cover ownership of every cached slot parameter; cache tests cover invalidation during refresh and unrelated-bank isolation.
- Eight report/selection/comparison unit tests passed. Invalid or missing measurements do not yield CPU improvement figures.

Deadline counts for these repeated measurements:

| Run | Processing/loading failures | Measured deadline misses | Warm-up misses |
|---|---:|---:|---:|
| Round 1, before | 0 | 0 | 0 |
| Round 1, after | 0 | 0 | 0 |
| Round 2, before | 0 | 2 | 0 |
| Round 2, after | 0 | 0 | 0 |

## Louder signals and noise reduction

The existing algorithm already relaxes spectral suppression when current summed band power substantially exceeds the learned noise baseline. Above four times learned power, a smooth confidence guard raises the minimum band gain toward 0.5 (about −6 dB); confident individual bands can open fully. This protects wanted harmonics. As notes decay, the normal per-band attenuation returns through the existing attack/release smoothing.

The optimization preserves that behavior and keeps detector histories running. Relaxing suppression further could reduce coloration but would also allow more noise during strong notes, so that would need listening tests on quiet picking, sustained chords and note tails. The separate optional broadband gate retains its own threshold.

## Reproduce an individual-module comparison

```sh
python scripts/fx-performance.py --module "Noise Gate (Smart)" --rates 48000 96000 --buffers 32 64 128 --seconds 5 --warmup 1 --repeats 3 --output build-grid/gate-before
# Rebuild the optimized plugin, then use the identical settings.
python scripts/fx-performance.py --module "Noise Gate (Smart)" --rates 48000 96000 --buffers 32 64 128 --seconds 5 --warmup 1 --repeats 3 --compare build-grid/gate-before/report.json --output build-grid/gate-after
```

Use `--module empty` to isolate shared overhead. Preserve old binaries and pass `--plugin` to repeat both versions. See [the benchmark guide](FX_PERFORMANCE.md) for filtering an individual mode, pacing and compatibility checks.

In a compiler environment, reproduce the numerical audit with `python scripts/verify-noise-print.py --reference 8c4c451`, then repeat with `--scalar`.

- [Round 1 comparison](../build-grid/smart-gate-ab-after1/comparison.md) ([JSON](../build-grid/smart-gate-ab-after1/comparison.json)).
- [Round 2 comparison](../build-grid/smart-gate-ab-after2/comparison.md) ([JSON](../build-grid/smart-gate-ab-after2/comparison.json)).

Reference plugin SHA256: `433f155664356d49789967b36cb599a8dd92ffd0c4c4af1b364fbad35a47dbbd`.

Optimized plugin SHA256: `7070f4d684ac9fa46b3c8010b6094c217c1b72267bf5343d6ae0087dd38ccc46`.

Raw artifacts remain local in the ignored build directory. Actual DAW/driver playback and audible-pop verification remain separate from this offline CPU and numerical audit.
