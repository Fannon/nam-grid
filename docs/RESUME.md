# Development checkpoint

Paused at the user's request on 2026-10-20.

## Completed and tested

Latest implementation: `3ba7d83`, on `experiment/fixed-4x4-grid`.

- Eight editor positions, aligned in pairs beneath the four grid columns;
  Enabled is always last, with unused positions left empty.
- Tube Boost plus expanded NAM cuts, compressor attack/blend, delay modulation/
  output and reverb modulation/wet low-cut.
- Windows Release CLAP/VST3 builds and all three CTest suites pass. DSP coverage
  includes 44.1/48/88.2/96/192 kHz, real-time allocation checks and state recall.
- Controls and measurements: [MODULES.md](MODULES.md).
- Focused research using the user's Pulze 1.4.0 and Ampero II Stomp 2.5.0 PDFs:
  [EFFECT_COVERAGE.md](EFFECT_COVERAGE.md). These are inspiration, not replication targets.

The current system CLAP copy is older: the Windows UAC installation prompt was
canceled. Updated artifacts remain in `build-grid/out`. No Bitwig listening,
macOS or VST3-host validation has been claimed. Repeated optional OpenGL captures
can produce partial frames; verify repainting in a real host.

## Next authorized work — not implemented yet

1. **Tuner panel**, opened by a button beside Settings. Inspect the sibling
   `../trace-tuner` repository and adapt one useful detector, rather than bringing
   its Rust plugin/UI or all detector variants into nam-grid. Its MIT license
   credits Simon Heimler (2026); retain the notice if adapting code. YIN, MPM and
   autocorrelation implementations/tests were inspected. MPM is a candidate;
   no algorithm has been ported or finally selected. Tap clean input, keep analysis
   work off audio, and add note/cents display, a reference pitch and optional mute.
2. **Host tempo sync** for delays and modulation, with beat divisions, a defined
   missing-tempo fallback, automation/state recall and host-tempo-change tests.
   iPlug2 already exposes `GetTempo()`/`GetPPQPos()` and its CLAP adapter reads
   host transport tempo. Keep the eight-position editor consistent.
3. **Utility module** with L/R balance, polarity and level; consider fine delay
   for parallel paths. No separate Volume module is wanted. A meaningful L/R
   control requires addressing the present mono grid: do not silently implement
   a decorative balance knob. Preserve fixed routing and avoid doubling NAM
   inference merely to produce stereo output. Decide and document how mono NAM
   stages handle stereo signals, with tests for signal order and channel behavior.

Only investigation was done after `3ba7d83`; these three features have no source
implementation yet. Do not treat earlier proposed spring/wah/pitch additions as
part of this pending increment.

## Workspace/build details

- Repository directory is historically named `nam-dual`; plugin name is `nam-grid`.
- Build/setup instructions: [BUILD.md](BUILD.md). The local helper is
  `../nam-build-tools/build-grid.bat`; run CTest with `--test-dir build-grid`.
- `NeuralAmpModelerCore` is intentionally dirty from the two tracked allocation
  patches in `patches/`. They are applied by setup. Do not revert or stage its
  gitlink; the checked-in patches make the changes reproducible.
- No framework upgrade or new runtime dependency is needed for the tuner UI or
  tempo interface. Keep MIT licensing and the existing prepared-object lifetime rules.
