# Historical module CPU, state and layout checkpoints

> Historical record extracted from the module reference. These figures and
> schema counts describe earlier implementations, not the current plugin.

## Historical offline CPU checkpoint (`64ac242`)

These measurements precede the expanded controls and Tube Boost. See the updated
checkpoint below for the current increment.


Windows x64 Release, Ryzen 9 5950X, 128-frame blocks, one effect in Pre 1 and
all other slots empty. Fastest of three timed 128,000-frame renders after warmup.
Numbers include the complete plugin/grid overhead; they are one-core elapsed-time
percentages, not a DAW meter or an underrun guarantee. All cases report zero latency.

| Effect | 48 kHz | 96 kHz |
| --- | ---: | ---: |
| Empty grid | 0.19% | 0.37% |
| Envelope Filter | 0.26% | 0.52% |
| Auto Swell | 0.23% | 0.47% |
| Digital Delay | 0.31% | 0.63% |
| Analog Echo | 0.34% | 0.68% |
| Tape Echo | 0.43% | 0.86% |
| Room | 0.61% | 1.27% |
| Dense Plate | 0.71% | 1.42% |
| Cloud | 1.04% | 2.14% |
| Chorus | 0.39% | 0.77% |
| Flanger | 0.41% | 0.82% |
| Phaser | 0.35% | 0.69% |
| Auto Tremolo | 0.38% | 0.75% |
| Vibrato | 0.38% | 0.77% |
| Airwindows Tape | 0.49% | 0.99% |

Reproduce from `build-grid`: `NeuralAmpModeler/nam-clap-bench.exe out/nam-grid.clap
fx-17 1 - -` (Cloud). Use `empty` for baseline, or `fx-10` through `fx-23` for
Filter through Tape. Raw local results are in `build-grid/effects-cpu.csv`.

## State and layout checkpoint

Schema 22 contains 3,098 parameters (286 input/global/slot controls + 16 × 173 FX
controls + 44 performance controls). Every slot uses one contiguous FX bank,
including LoFi Tape. Host and editor wording agree for uniquely owned controls;
shared controls retain effect-specific editor labels where useful.
Prototype IDs, names and enum ranges may change before the first public release.
Older unreleased format compatibility remains best effort. All controls participate
in Copy/Move/Swap, automation and project recall. Seven controls plus Enabled share
eight equal cells across the module editor. A smaller effect does not need to fill them.
The new reverb cores and five audition additions are described in
[REVERB_DELAY_AUDITION.md](REVERB_DELAY_AUDITION.md); earlier CPU tables above
describe the older implementations.

Additional tests check Tube Boost's even harmonics with Bias, near-zero steady-state
DC and exact settled dry blend; compressor attack and blend; NAM cuts in the processing
reference; all eight editor positions; and schema-6 later-slot parameter recall.

## Previous mono offline CPU checkpoint

Same host and benchmark method as above. Values include the full grid; all cases
report zero compensation latency. Defaults are used, so this does not measure all
parameter extremes. Raw local results: `build-grid/complete-modules-cpu.csv`.

| Effect | 48 kHz | 96 kHz |
| --- | ---: | ---: |
| Empty grid | 0.20% | 0.40% |
| Smooth Compressor | 0.35% | 0.70% |
| Analog Echo | 0.47% | 0.93% |
| Cloud | 1.09% | 2.17% |
| Tube Boost | 0.39% | 0.79% |

