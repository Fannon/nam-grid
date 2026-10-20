# Development checkpoint

Resumed on 2026-10-20 on `experiment/fixed-4x4-grid`.

## Completed

- Prior eight-position editor and Tube Boost checkpoint remains intact.
- Tuner beside Settings: C++ MPM adapted from MIT trace-tuner, clean-input worker
  analysis, note/cents/frequency, 430–450 Hz reference and optional output mute.
  Closing the panel/editor restores audio; visibility is not saved.
- Host tempo sync for all delays and modulation, nine straight/dotted/triplet
  divisions, 120 BPM fallback, state/automation. Rate sync is free-running,
  not PPQ-position locked. Manual Time/Rate values are retained.
- Stereo Utility: level, L/R balance, centered Stereo (−100 mono / 0 neutral / +100 wider)
  and independent polarity. Stereo
  survives built-ins and IRs; mono NAM averages input and runs inference once.
- Reset now clears stale input buffers/DC-filter histories. Fractional ring reads
  no longer round outside the delay buffer at near-integer boundaries.
- Windows Release CLAP/VST3 builds and four CTest suites cover the current changes. New tests cover
  tuner accuracy/worker, no audio allocations, stereo/A2/IR, tempo transport,
  older-schema migration and tuner UI mute/teardown. See MODULES.md for details.
- Ping-pong switch on all three delays, Chorus Spread, stereo wet output from
  Room/Plate/Cloud and the new Spring Reverb; five-knob automatable Wah.
- Spring uses original dispersive allpass tanks; no copied external DSP or new
  dependency. Scope/algorithms/acceptance: [STEREO_GUITAR_EFFECTS.md](STEREO_GUITAR_EFFECTS.md).
- Current CPU measurements and manual acceptance checks: [MODULES.md](MODULES.md).

## Next suggestions (not implemented)

1. Bitwig listening/automation/project-recall validation at low buffers; real
   guitar tuner evaluation, display scaling/reopen, and stereo/mono host layouts.
2. Audition Spring Drip/Dwell and Wah range with real playing, then adjust voicing
   based on listening. Spatial effect dry placement/mono fold also needs auditioning.
3. Fine lane timing, convenient presets and portable model relinking are useful
   follow-ups; avoid another large effect catalog. Pitch/shimmer remains deferred.

The current system CLAP copy remains older because the prior UAC install prompt
was canceled. Current binaries and notices are in `build-grid/out`; do not claim
system installation or Bitwig/macOS/VST3-host validation. Optional OpenGL readback
can show partial frames; final native repainting still needs real-host review.

## Workspace/build details

- Checkout directory: `C:/Users/Fannon/dev/nam-grid` (renamed from `nam-dual`).
- Build: [BUILD.md](BUILD.md), local helper `../nam-build-tools/build-grid.bat`.
- `NeuralAmpModelerCore` and `AudioDSPTools` are intentionally dirty from tracked
  patches in `patches/`. Do not revert or stage their gitlinks.
- MIT retained, trace-tuner attribution included in shipped ThirdPartyNotices.
  No new runtime dependency. Schema 9 has 1262 parameters, accepting earlier
  supported schemas 2/3/6/7/8 with name-based defaults for newly added fields.
  Unused WIP schemas 4/5 reject without mutation. Gate Release accepts the older
  Gate Decay state name. Schema 8 Utility Width migrates to centered Stereo by subtracting 100; tuner preferences are preserved.

The four actionable review findings are fixed: safe/transactional WAV reads,
correct PCM32 polarity, uniform slot input ranges and smoothed Master gain.
Regression/sanitizer details: [REVIEW_FIXES_20261020.md](REVIEW_FIXES_20261020.md).

Follow-up static-review fixes and parallel-branch integration notes:
[REVIEW_FOLLOWUPS_20261020.md](REVIEW_FOLLOWUPS_20261020.md).
