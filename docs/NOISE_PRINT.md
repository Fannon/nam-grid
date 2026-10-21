# Noise Print experiment

Branch: `experiment/noise-print`, based on `ce1cc93`. An original MIT noise-profile
expander, inspired by the public description of frequency-selective gates and
Learn in [Klevgrand Brusfri](https://klevgrand.com/products/brusfri). No Klevgrand
implementation, assets or proprietary processing details were used. This does not
claim Brusfri's sound, internal architecture or phase behavior.

## Try it

Build the CLAP target using [BUILD.md](BUILD.md), with `-DNAM_FORMATS=CLAP`.
On Windows the artifact is `build-grid/out/nam-grid-noise-print.clap`. Add this
directory to the host's scan paths, rescan, and choose **nam-grid Noise Print**.
Its CLAP ID is `com.Fannon.NAMGridNoisePrintExperiment`; the existing nam-grid can
remain installed. This experiment has not been installed in the system CLAP folder.

1. Add **Noise Gate > Noise Print** in a Pre slot, preferably before gain/NAM stages.
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

The optional final gate is independent of the snapshot and spectral Mix/Reduction:
it also works without learning. It detects the original input's maximum L/R peak,
applies one linked gain after the spectral mix, and closes linearly to exact zero
over Release after the hold. Either channel can open it, including anti-phase input.
An enabled gate starts closed after reset/recall to avoid an initial noise burst.
Turning Gate Off reopens it over 0.5 ms. Keep the module before delay/reverb to
preserve their tails. The standalone Natural/Tight gates remain available.

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

## Verification

Windows x64 Release, MSVC 19.44, Ryzen 9 5950X, 2026-10-21:

- All four CTest suites passed: library, WAV decoder, stages, CLAP smoke/native UI.
- DSP covers 44.1/48/96/192 kHz, mono/stereo linking, block sizes 1/512, exact dry
  paths, invalid-input recovery, reset, allocation/deletion tracking, snapshot
  concurrency and slot transactions. The final gate tests cover exact closed
  silence, opening, hold, hysteresis, shared release, Off/Learn, stereo linking
  with right-only and anti-phase signals, and block partition invariance.
- Actual CLAP harness covers Learn parameter events, completed snapshot save/load,
  sample-rate reactivation, malformed profiles without state corruption, older
  schema migration, native Learn toggle and profile copy, plus gate automation,
  Off display, actual gated audio, recall, native threshold knob and copy.
  First-experiment schema 1001 recalls Gate Off.
- Deterministic white noise plus 60 Hz hum, defaults: approximately **22.6–23.8 dB**
  noise-only reduction after settling. Against harmonic tones at 82.4/220/440/1760 Hz,
  total error versus the clean reference improves approximately **0.49–2.20 dB**.
  This includes wanted-signal amplitude/phase damage and is much more modest than
  the between-notes figure. These synthetic tests do not establish guitar quality.

The CLAP benchmark uses stereo 128-frame blocks, 1,000 callbacks per run, and the
fastest of three measured runs after a warmup. One trained Noise Print module
costs roughly 3.3% of one core at 48 kHz, including the grid, versus about 0.3%
empty and 0.45% with Natural Gate. At 96 kHz it costs roughly 6.4%. These local
minimum-time measurements do not establish worst-case host performance.

| Stereo plugin configuration | 44.1 kHz | 48 kHz | 96 kHz | Reported latency |
| --- | ---: | ---: | ---: | ---: |
| Empty grid | 0.27% | 0.29% | 0.59% | 0 samples |
| Natural Gate | 0.41% | 0.44% | 0.89% | 0 samples |
| Trained Noise Print (Gate Off) | 2.91% | 3.26% | 6.37% | 0 samples |

See the [optimization audit](NOISE_PRINT_OPTIMIZATION.md) for matched before/after
measurements and a reproducible numerical comparison with the previous DSP.

Reproduce after building:

```powershell
ctest --test-dir build-grid -C Release --output-on-failure
# Executables may be in a Release subdirectory with a multi-config generator.
build-grid/NeuralAmpModeler/nam-clap-bench.exe build-grid/out/nam-grid-noise-print.clap empty 1 - -
build-grid/NeuralAmpModeler/nam-clap-bench.exe build-grid/out/nam-grid-noise-print.clap fx-1 1 - -
build-grid/NeuralAmpModeler/nam-clap-bench.exe build-grid/out/nam-grid-noise-print.clap fx-64 1 - -
```

Bitwig, real guitar listening and macOS have **not** been tested. Before adoption:
record a noisy DI, compare bypass/processed at matched levels before clean and
high-gain NAMs, assess pick attacks and long decays, change block sizes/rates,
save/reopen the project, copy/move/replace the module while running, and check
host CPU peaks. An ordinary gate is cheaper when only silence between notes matters.

## Integration handoff

Main advanced independently to `9ebb4cb`, adding roadmap effects, scenes and shared
modulation. This worktree intentionally leaves those changes alone. Module ID 64
is reserved here to avoid main's IDs 28–41. The experiment has **1,374 parameters**
and state schema **1002**; profile format version 1 contains exactly 96 powers.
It accepts the base branch's tested schemas through 9 and experiment schema 1001,
and explicitly rejects main's schemas 10/11. Do not substitute this plugin for
normal nam-grid in projects.

A later integration must combine the effect enums/validation, append controls after
main's current stride, migrate its named parameters, handle snapshots in performance
scenes (Learn should stay transient), assign a shared next schema, and rerun both
branches' tests. `NoisePrint.h` and `NoisePrintState.h` are independent of iPlug2;
integration points are `NAMStage`, `SlotOperations`, effect descriptors/menu/editor,
and plugin serialization/idle polling. Separate plugin identity/build names should
be removed only when that integration is ready.
