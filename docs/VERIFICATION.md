# Prototype verification — 2026-10-20

## Performed locally

Windows x64 Release CLAP and VST3 builds succeed. CTest runs:

- `capture-library`: category aliases, ambiguous amp metadata, explicit filename/folder hints,
  root-folder inference (including trailing separators), inherited/nearest category folders,
  Acoustic & Line fallback with explicit pedal metadata preserved,
  whole-word CAB/DI filename markers (including before the extension), No Cab negation,
  contradictory CAB/DI markers and DI pedal/outboard distinctions,
  metadata after weights, nested metadata isolation, malformed metadata, mixed-case extensions,
  Unicode paths, multiword metadata search and gear filters. The same native menu builder used by
  the editor is checked for nested folder navigation, stable selection tags/checkmarks and empty
  results. Missing folders, scan cancellation/latest request, and default-folder preference
  round trips/corruption are covered. Fixtures live in the test build directory.
- `stages`: a noncommutative affine-model pair verifies series order; independent gain/trim
  and settled bypass are checked. Two distinct synthetic A2 nano/standard models match separate
  reference processors at 48, 44.1 and 96 kHz with blocks 1, 3, 64, 127, 256 and 512. Bypass
  preserves model conversion latency, including an impulse check. A host maximum of one frame
  is exercised. The audio-stage paths have zero observed `new`/`delete` operations and pass
  Eigen's runtime allocation prohibition. PReLU and LSTM allocation regressions are covered.
  A 10,000-publication two-thread ownership stress test confirms destruction stays off audio.
- `clap-smoke`: factory creation, two distinct `.nam` files, audio at the three rates and variable
  blocks, all parameter/path recall, upstream project migration, CLAP automation, repeated live
  model replacement/clearing, host reactivation after latency changes, malformed state, missing
  and invalid model files, mono/mono-stereo/stereo layouts, WAV IR loading/recall, teardown, and
  native editor attachment/destruction under a hidden Windows parent. The folder setting survives
  state recall; older states without that setting still load, and malformed folder settings leave
  state untouched. Windows mouse/keyboard messages exercise actual Drive/Amp next buttons across
  nested library folders and search text entry followed by a filtered selection.

A read-only scan of the user's real library found 158 NAM files, including export/catalog subfolders:
91 Amp + Cab, 4 Amp Head, 42 Pedal, 20 Outboard, 1 Unknown. Nine classifications use explicit filename
or folder hints; all others come from metadata. No model files or catalog entries were modified.
These are category results, not claims about capture quality or successful inference of every model.
The scanner doesn't instantiate DSP and adds no work to `ProcessBlock`.

After extending folder aliases and CAB/DI word matching, a later read-only scan of the current library
found 158 captures with 10 inferred classifications and no Unknowns: 92 Amp + Cab, 3 Amp Head,
42 Pedal and 21 Outboard. The user's export filenames were being updated during this work, so these
later totals reflect both the current files and the revised rules; they are not a comparison on a
frozen dataset. No source captures were renamed or edited by the plugin work.

The editor test caught a resource omission in the initial CMake integration. Embedding
`resources/main.rc` in both plugin formats fixed the native editor crash.

Allocation claims apply to the measured stage tests, not every possible NAM architecture,
every third-party DSP path, or an entire DAW session. Eigen allocations are guarded separately
from the test's C++ allocation counters. No general allocator interception across the plugin DLL
was performed. The CLAP harness runs host and audio lifecycle calls sequentially; concurrency
stress covers the ownership handoff, not a complete host/UI race detector run.

## CPU and reported latency

AMD Ryzen 9 5950X, MSVC 19.44, x64 Release. Synthetic A2 nano Drive and A2 standard Amp,
128-frame blocks; gate, EQ, IR and calibration disabled; unity gains. Each timing is the fastest
of three measured 128,000-sample renders after a warm-up render. CPU is elapsed render time
divided by audio duration, expressed as a percentage of one core. This is an offline harness
measurement, not Bitwig's CPU meter, an underrun test, or a listening evaluation.

| Host rate | Original single Amp | Two original instances | Dual prototype | Original / dual latency |
| --- | ---: | ---: | ---: | --- |
| 48 kHz | 5.53% | 7.16% | 7.09% | 0 / 0 samples |
| 44.1 kHz | 5.90% | 7.78% | 7.73% | 27 / 54 samples |
| 96 kHz | 6.26% | 8.45% | 8.41% | 42 / 84 samples |

The dual prototype costs approximately the same as two original instances for this workload.
The small differences between the two dual-model configurations are within benchmark variability.
The prototype reports the sum of the two model-rate conversion delays. The original two-instance
chain reports the same total delay. No intentional oversampling or lookahead was introduced.

## Reproducible DAW checklist — not yet performed

- [ ] In Settings choose a capture folder using the OS picker; confirm cancellation leaves the path
  unchanged. Add/move a NAM, Rescan, browse nested submenus and exercise category/search filtering.
  Verify the chosen capture and sound, then save/reopen the project. Create a new instance to verify
  the default folder. Clear the folder and confirm loaded audio/models remain intact. Check missing
  folders, duplicate filenames and large folders. Native folder-picker interaction and a complete
  visual layout review still need manual confirmation; harness tests do not establish DAW usability.
- [ ] Windows Bitwig: scan the prototype's `out` directory and add **NAM Dual Prototype** to a
  blank audio track. Avoid overwriting the official plugin installation.
- [ ] Load a real A2 pedal in Drive, a different A2 amp in Amp, and a known WAV IR. Play/record
  guitar and confirm serial behavior with each input gain, trim and bypass.
- [ ] Listen to bypass and model replacement during sustained audio; test rapid load/clear and
  different model sample rates. Confirm Bitwig honors restart requests and updates delay compensation.
- [ ] Save a project with nondefault values for every control and reopen it with the editor closed,
  then open the editor. Confirm settings, model names, bypass and sound match.
- [ ] Rename/move each model and IR file, reload, verify failure status and passthrough for the missing
  stage, then relink. Try malformed `.nam`, empty/invalid WAV, cancellation, and unsupported channels.
- [ ] Exercise 32/64/128/512/1024-frame blocks, 44.1/48/88.2/96 kHz, mono and stereo tracks, transport
  start/stop, sample-rate changes, offline rendering, repeated instance creation/deletion and UI reopen.
- [ ] Automate gains, trims and bypasses; check for clicks, level jumps, clipping and stale model state.
- [ ] Compare CPU, actual loopback latency and sustained low-buffer underruns with the original plugin.
- [ ] Repeat on macOS Intel/Apple Silicon. Run CLAP validation and VST3 plugin validation on release artifacts.

## Remaining limits and decisions

- Bitwig, audible click behavior, measured loopback latency, macOS, VST3 host loading and remote CI are
  **unverified**. A successful harness or build does not establish those outcomes.
- The framework's VST3 processing adapter still uses a parameter mutex; this prototype makes no claim
  that the complete VST3 callback is lock-free. The new DSP handoff adds no audio-thread mutex.
- The original direct-convolution IR implementation is reused (8192 taps, existing gain convention).
  Long IR CPU cost, gate/EQ/IR bypass smoothing, and per-stage metering need later attention.
- State uses absolute external paths. Portable project bundles and asynchronous inference-model loading
  are not implemented. The new library metadata scan runs in the background; loading a selected model
  still prepares it on the control thread. Models with unusual metadata/advanced conditioning require
  further fixtures. Search/filter UI choices are not project parameters and are not serialized.
- Core stays at the plugin's pinned revision plus two targeted upstream fixes; latest Core has wider
  validation and API changes. Broad migration should be a separate tested increment.

## Upstream checkpoint

On 2026-10-19, NAM Plugin `main` remained `16be869` (unchanged from the earlier investigation).
iPlug2 `master` was `39ea73a`; the newer changes concern iOS lifecycle, REAPER text entry and CI,
with no Windows CLAP/editor fix relevant here. The prototype keeps the tested `d54f690` pin.
Core `main` was `0b3d3c9` (0.6.0 API, latest commit dated 2026-09-18), ahead of the plugin's
`9c7b185` pin; this is dependency drift, not evidence of changes during the last week.

Two applicable fixes are preserved as local patches from official Core commits:
`11f1d6c` (PReLU slope-vector allocation) and `c9ac48e` (LSTM hidden-state allocation).
The PReLU regression test was observed failing against the original pin before applying its fix.
The rest of the Core 0.6 migration is intentionally separate.
