# FX CPU and low-buffer stability report — 2026-10-22

Two complete module sweeps were run on this computer under the mixed load described by the user (another coding session was active). Each configuration also ran three consecutive repeats. These are measurements of the real CLAP DSP callback, without an audio device or editor.

These measurements precede the [Smart Gate and grid CPU optimizations](SMART_GATE_OPTIMIZATION_20261022.md), which have their own repeated before/after results.

Machine: AMD64 Family 25 Model 33 Stepping 2, AuthenticAMD; Windows-10-10.0.26300-SP0; 32 logical CPUs.

Plugin SHA256: `433f155664356d49789967b36cb599a8dd92ffd0c4c4af1b364fbad35a47dbbd`. Both sweeps used the same plugin and benchmark host binaries.

## Findings

- 275 cases per sweep, including the empty grid, every catalog FX and its tested modes, NAM and IR. 7,425 measurements per sweep; 50,928,900 measured callbacks across both sweeps.
- Processing/loading failures, crashes or non-finite output: **0 / 0** (sweep 1 / sweep 2).
- Measured callback deadline misses: **28 / 22**; warm-up misses: **6 / 2**.
- Highest median callback CPU for any single configuration: **14.82% / 14.81%**.

CPU means elapsed callback time divided by buffer duration, expressed as a percentage of one audio thread. 100% exhausts that buffer deadline. Values include the grid/plugin overhead. They are not percentages of total machine CPU.

Deadline misses under mixed load identify risk; they do not establish a module bug or an audible DAW pop. Scheduling/preemption is included in wall-clock callback time. No fastest-run selection or outlier removal was used.

## Method and coverage

- [44100, 48000, 96000] Hz; [32, 64, 128] frames; 3 repeats per configuration.
- 0.5 s warm-up + 3.0 s measured audio per repeat; 75% stereo excitation and 25% silence/tail. Stereo float64, Release builds, unpaced offline rendering.
- Enum/binary mode combinations come from the shared module catalog. All tempo divisions are additionally tested at other controls' insertion defaults. Continuous knobs use insertion defaults except reverb resolution endpoints. This is not an exhaustive test of all knob settings or transition automation.
- The first sweep was supplemented with two real captures after the default fixtures; the second sweep included those same captures. Matching case/control/rate/buffer/repeat keys, asset hashes and binary hashes were checked.

## Per-module CPU and deadline results

CPU columns show the highest **median of three repeat means** among that module's modes, rates and buffer sizes. This summarizes its most expensive tested configuration while reducing sensitivity to a single repeat. Miss columns count all measured callback deadline misses; warm-up misses are separate above. CSV/JSON retain every individual repeat, p95/p99/worst timing and phase split.

| Module | Cases | CPU % sweep 1 | CPU % sweep 2 | Misses sweep 1 | Misses sweep 2 |
|---|---:|---:|---:|---:|---:|
| Airwindows Guitar Hall | 3 | 3.14 | 3.10 | 0 | 0 |
| Airwindows Plate | 3 | 3.60 | 3.68 | 0 | 0 |
| Airwindows Tape | 1 | 3.60 | 3.69 | 0 | 0 |
| Analog Echo | 13 | 3.67 | 3.19 | 4 | 5 |
| Auto Swell | 1 | 2.74 | 2.62 | 0 | 0 |
| Auto Tremolo | 11 | 3.11 | 3.22 | 0 | 0 |
| Bitcrusher | 1 | 2.82 | 2.71 | 0 | 0 |
| Boost | 1 | 2.96 | 3.03 | 0 | 0 |
| Butter Comp | 1 | 3.34 | 3.55 | 0 | 4 |
| Cabinet IR | 2 | 14.82 | 14.81 | 1 | 0 |
| Chorus | 11 | 3.45 | 3.30 | 4 | 1 |
| Cloud | 2 | 5.64 | 5.66 | 1 | 0 |
| Console Colour | 8 | 4.22 | 3.89 | 0 | 0 |
| Console EQ | 1 | 2.69 | 2.72 | 1 | 0 |
| Cosmic Echo | 6 | 5.36 | 5.36 | 0 | 0 |
| Cut EQ | 1 | 2.78 | 2.97 | 0 | 0 |
| Dense Plate | 2 | 3.90 | 3.79 | 0 | 0 |
| Diffused Delay | 11 | 5.14 | 5.13 | 0 | 0 |
| Digital Black Gate | 2 | 2.84 | 2.98 | 0 | 0 |
| Digital Delay | 13 | 3.28 | 3.06 | 1 | 1 |
| Distortion | 6 | 2.87 | 2.91 | 1 | 0 |
| Dual Delay | 13 | 2.88 | 2.90 | 0 | 0 |
| Empty grid | 1 | 2.53 | 2.44 | 0 | 0 |
| Filter | 3 | 2.82 | 2.89 | 0 | 0 |
| Flanger | 11 | 3.36 | 3.37 | 0 | 0 |
| Freeze / Hold | 2 | 2.77 | 2.87 | 0 | 0 |
| Frequency Shifter | 1 | 3.29 | 3.19 | 0 | 0 |
| Galactic Space | 3 | 3.05 | 2.93 | 1 | 0 |
| Guitar EQ | 1 | 2.76 | 2.73 | 0 | 0 |
| Hall | 2 | 7.71 | 7.66 | 0 | 0 |
| Juno Chorus | 3 | 3.02 | 3.01 | 0 | 0 |
| LoFi Tape Machine | 1 | 2.96 | 2.89 | 0 | 0 |
| Mono Octaver | 1 | 2.71 | 2.75 | 0 | 0 |
| Multi-head Tape | 25 | 4.08 | 3.82 | 4 | 7 |
| NAM | 10 | 12.75 | 11.05 | 0 | 0 |
| Noise Gate | 2 | 2.79 | 2.76 | 0 | 0 |
| Noise Gate (Smart) | 3 | 8.43 | 8.51 | 0 | 0 |
| Outboard Channel | 1 | 3.90 | 3.76 | 0 | 1 |
| Parametric EQ | 2 | 3.01 | 2.98 | 6 | 0 |
| Peak Limiter | 1 | 2.69 | 2.84 | 0 | 0 |
| Phaser | 11 | 3.21 | 3.10 | 1 | 0 |
| Pressure | 1 | 3.20 | 3.09 | 0 | 0 |
| Punch Compressor | 1 | 3.36 | 3.19 | 0 | 0 |
| Reverse Delay | 11 | 3.27 | 2.88 | 1 | 0 |
| Ring Modulator | 1 | 3.06 | 2.94 | 0 | 0 |
| Room | 2 | 7.69 | 7.72 | 0 | 0 |
| Rotary | 2 | 3.46 | 3.44 | 0 | 0 |
| Shimmer | 2 | 6.01 | 6.00 | 0 | 0 |
| Smooth Compressor | 1 | 3.27 | 3.21 | 0 | 0 |
| Spring Reverb | 2 | 5.08 | 5.11 | 0 | 0 |
| Stereo Doubler | 1 | 3.07 | 3.06 | 0 | 0 |
| Stutter | 11 | 2.92 | 2.90 | 1 | 0 |
| Tape Echo | 13 | 3.37 | 3.26 | 0 | 1 |
| Tube Boost | 1 | 3.17 | 3.28 | 0 | 0 |
| Uni-Vibe | 13 | 3.82 | 3.90 | 0 | 1 |
| Utility | 4 | 2.68 | 2.71 | 0 | 0 |
| Vibrato | 11 | 3.31 | 3.28 | 1 | 1 |
| Wah | 1 | 3.27 | 3.15 | 0 | 0 |

## Most expensive configurations present in both sweeps

Sorted by the smaller of the two sweep medians, so a cost must appear in both runs to rank highly.

| Module / mode | Hz | Frames | CPU % sweep 1 | CPU % sweep 2 |
|---|---:|---:|---:|---:|
| Cabinet IR / ir-16384.wav | 96000 | 32 | 14.82 | 14.81 |
| Cabinet IR / ir-16384.wav | 96000 | 64 | 13.73 | 13.72 |
| Cabinet IR / ir-16384.wav | 96000 | 128 | 13.34 | 13.27 |
| NAM / 08 PVY 5150 HG CAB.nam (SlimmableContainer; A2) | 96000 | 32 | 11.48 | 11.05 |
| NAM / a2-large.nam (WaveNet; fixed) | 96000 | 32 | 11.08 | 10.79 |
| NAM / slimmable.nam (SlimmableContainer; A2) | 96000 | 32 | 10.83 | 10.74 |
| NAM / 01 FND DLX CLN CAB.nam (SlimmableContainer; A2) | 96000 | 32 | 12.75 | 10.73 |
| NAM / 08 PVY 5150 HG CAB.nam (SlimmableContainer; A2) | 96000 | 64 | 8.84 | 8.76 |
| Cabinet IR / ir-2048.wav | 96000 | 32 | 8.74 | 8.73 |
| Noise Gate (Smart) / Noise Learn=0 | 96000 | 32 | 8.43 | 8.51 |
| NAM / 08 PVY 5150 HG CAB.nam (SlimmableContainer; A2) | 44100 | 32 | 8.55 | 8.41 |
| NAM / 01 FND DLX CLN CAB.nam (SlimmableContainer; A2) | 96000 | 64 | 9.03 | 8.40 |
| NAM / slimmable.nam (SlimmableContainer; A2) | 96000 | 64 | 8.36 | 8.40 |
| NAM / a2-large.nam (WaveNet; fixed) | 96000 | 64 | 8.33 | 8.38 |
| NAM / 08 PVY 5150 HG CAB.nam (SlimmableContainer; A2) | 48000 | 32 | 8.48 | 8.10 |

## A2 versus A2 Lite

The same slimmable file selects its 8-channel A2 or 3-channel A2 Lite submodel before processing. The real captures are a clean Fender Deluxe and a high-gain Peavey 5150 from the local collection. The synthetic fixture measures the same architecture shape with deterministic weights; it does not assess tonal quality. Fixed-size A2, LSTM and WaveNet fixtures are also included in the raw NAM results.

The following are median callback CPU percentages at 32-frame buffers. Savings compare the mean of both sweeps' medians and include the common plugin overhead.

| Capture | Hz | A2 sweep 1 | A2 Lite sweep 1 | A2 sweep 2 | A2 Lite sweep 2 | CPU reduction |
|---|---:|---:|---:|---:|---:|---:|
| slimmable.nam | 48000 | 7.84% | 3.04% | 7.72% | 3.05% | 60.9% |
| slimmable.nam | 96000 | 10.83% | 4.94% | 10.74% | 4.93% | 54.2% |
| 01 FND DLX CLN CAB.nam | 48000 | 8.58% | 3.12% | 7.78% | 3.03% | 62.4% |
| 01 FND DLX CLN CAB.nam | 96000 | 12.75% | 5.04% | 10.73% | 4.97% | 57.4% |
| 08 PVY 5150 HG CAB.nam | 48000 | 8.48% | 3.22% | 8.10% | 3.17% | 61.5% |
| 08 PVY 5150 HG CAB.nam | 96000 | 11.48% | 5.12% | 11.05% | 5.02% | 55.0% |

## Existing cabinet IR loader

No IR quality mode was added. These runs use deterministic mono 48 kHz WAVs of 2,048 and 16,384 samples with independent stereo audio processing. The existing direct convolver caps processing at 8,192 samples after resampling; the longer file therefore exercises that cap. This is not a comparison of different convolution implementations.

| Input IR samples | Hz | Frames | CPU % sweep 1 | CPU % sweep 2 |
|---|---:|---:|---:|---:|
| 2048 | 48000 | 32 | 2.82 | 2.88 |
| 2048 | 48000 | 64 | 2.37 | 2.40 |
| 2048 | 48000 | 128 | 2.13 | 2.17 |
| 2048 | 96000 | 32 | 8.74 | 8.73 |
| 2048 | 96000 | 64 | 7.79 | 7.79 |
| 2048 | 96000 | 128 | 7.26 | 7.35 |
| 16384 | 48000 | 32 | 7.33 | 7.42 |
| 16384 | 48000 | 64 | 6.83 | 6.87 |
| 16384 | 48000 | 128 | 6.65 | 6.61 |
| 16384 | 96000 | 32 | 14.82 | 14.81 |
| 16384 | 96000 | 64 | 13.73 | 13.72 |
| 16384 | 96000 | 128 | 13.34 | 13.27 |

## Interpreting spikes under mixed load

0 case/rate/buffer combinations had measured deadline misses in both sweeps. The empty grid had 0 / 0 measured misses. Recurrence is a useful prioritization signal, but the benchmark cannot separate algorithm time from OS preemption or determine whether the audio driver actually underruns.

Use repeated average/p99 costs to identify consistently expensive modules, then reproduce deadline spikes in the intended DAW/driver with the real rig and the target buffer size. This benchmark does not open ASIO/CoreAudio, count device xruns, listen for clicks, test an entire project, or exercise UI activity and concurrent asset loading. A complete rig and other plugins need additional processing headroom.

## Full measurements and reproduction

- [Benchmark instructions](FX_PERFORMANCE.md).
- [First sweep summary](../build-grid/fx-performance-full/report.md), [CSV](../build-grid/fx-performance-full/measurements.csv), [JSON](../build-grid/fx-performance-full/report.json).
- [First-sweep real A2 supplement](../build-grid/fx-performance-real-a2-first/report.md), [JSON](../build-grid/fx-performance-real-a2-first/report.json).
- [Second sweep summary](../build-grid/fx-performance-second/report.md), [CSV](../build-grid/fx-performance-second/measurements.csv), [JSON](../build-grid/fx-performance-second/report.json).

Raw artifacts stay in the ignored build directory; this Markdown report remains in the repository. Asset hashes and absolute local paths are recorded in JSON; model/IR files are not embedded in this report.

```sh
python scripts/fx-performance.py --nam "<clean A2 capture.nam>" --nam "<high-gain A2 capture.nam>" --output build-grid/fx-performance-second
```
