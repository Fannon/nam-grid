# First-release review — 2026-10-21

> Historical record. Branch names, measurements and completion claims below describe
> their original checkpoint. See the [archive index](README.md) for current guides.

Reviewed checkout: `89ec5a6`, plus the changes described below. The existing Core
and AudioDSPTools modifications were present before this review; they are
reproducible through `scripts/setup-deps.py` and the tracked dependency patches.

## Release assessment

The project has a strong automated foundation for an initial Windows preview.
A stable release is premature until real DAW sessions and the intended release
formats/platforms have passed acceptance checks. The Windows harness exercises
CLAP processing and the native editor; a successful VST3 link is not a VST3 host test.

Keep the first release small: a clearly labeled preview with the tested native
formats, explicit platform limits, installation steps, and external-model/IR
requirements. Preserve existing plugin IDs. Choose and synchronize the fork's
release version before tagging; `config.h` still carries upstream version `0.8.0`.
Historical upstream installers and standalone/AU projects are outside this scope.

## Findings addressed

| Priority | Finding and consequence | Change and evidence |
| --- | --- | --- |
| High | Resampler reset assumed host-block-sized buffers covered conversion warm-up. At 8 kHz model / 192 kHz host, the 128-frame workaround still read past initialized scratch. Smaller blocks also exposed an undersized intermediate buffer. | Exact warm-up sizes now determine both buffers, including channel strides and zeroing. The new tracked AudioDSPTools patch is applied by existing setup. AddressSanitizer reproduced a heap over-read with exact WDL allocation, then passed after the fix. |
| High | Tag releases used upstream packaging scripts rather than the tested grid CMake build, and created the draft before tests. | Scheduled and release jobs now reuse the grid build/test matrix. A draft is created after successful tests, with the actual CLAP/VST3 archives and notices. Archives are made on the build host to retain macOS executable permissions through artifact transfer. |
| Medium | Shimmer Freeze reported a finite tail; Freeze also reverted to a finite tail on activation until the first block. Hosts could truncate sustained frozen output. | Shared tail calculation covers both hold controls, activation, automation and bypass. CLAP regressions cover the first and last slots and ensure unrelated hold parameters do not make a tail infinite. |
| Medium | Byte strings containing NUL silently truncated headers/JSON. The legacy IR path alias also accepted NUL despite slot paths rejecting it. Malformed recall could succeed and change state. | Reject embedded NUL before conversion to C strings; apply the same check to all path aliases. The new rejection regression failed on the original binary. Rejections assert that committed state stays unchanged. |
| Medium | Preset application scanned every parameter name for every JSON key. Full performance scenes repeated that search during validation. | Iterate the parameter list once and use JSON object lookup. This removes quadratic name scans, with existing migration, round-trip, invalid-scene and native scene-recall tests covering behavior. No numerical CPU-speedup claim is made. |
| Low | The README, build guide and contributor instructions advertised inherited installers, obsolete controls, or no unit tests. | Current entry points and preview limits replace those claims; historical review documents remain dated records. |
| Low | The UI guide described the input meter as preceding IN gain, although the audio callback measures the gained input. | The guide now matches the existing signal path: input and output readings follow their respective gains. |

## Verification

- Windows x64 Release CLAP and VST3 rebuilt using the existing MSVC/Ninja toolchain.
- The original four-suite baseline passed before changes.
- Final CTest: **5/5 passed in 298.19 s**: capture-library (0.21 s), WAV decoder
  (0.89 s), resampling (0.34 s), DSP stages (13.39 s), and CLAP/native editor
  (283.12 s). The final run used the rebuilt plugins with every review fix applied.
- Focused resampling tests passed under MSVC AddressSanitizer with exact WDL
  allocations. Coverage spans 8/44.1/48/192 kHz in both directions, mono/stereo,
  1/3/128-frame maxima, repeated resets, and silent output after warm-up.
- Dependency setup was rerun without network access through its patch-application
  function to verify idempotence. The Core/WAV patches and submodule pins are retained.
- Workflow YAML was parsed locally. Remote GitHub Actions, macOS execution and
  creation of an actual release are not verified by local parsing.
- A local ZIP of the rebuilt Windows artifacts passed integrity checks and
  contains the CLAP, VST3 bundle and third-party notices. The resampler patch
  applied cleanly to the pinned original header and reproduced the working copy.

On Windows, sanitizer runs need the compiler runtime directory on `PATH` during
testing as well as compilation. The initial standalone test launch omitted it
and displayed a missing-DLL dialog; the corrected launch passed. The ordinary
`build-grid` configuration has sanitizers off and does not need that DLL.

## Checks before tagging

1. In Bitwig, load real pedal/amp models and an IR; play and record at 32/64/128/512
   frames and 44.1/48/96 kHz. Listen during bypass, replacement, parameter changes,
   transport stop/start, and offline export; confirm latency compensation.
2. Save/reopen a project with the editor closed, reopen the UI, and compare sound,
   parameters and file references. Exercise moved/missing files, repeated instance
   creation, editor reopen, and at least a 30-minute low-buffer session.
3. Validate VST3 in a real host and with the SDK validator. Validate CLAP with a
   dedicated validator. Add the VST3 smoke/validation path to CI before describing
   the two formats as equally verified.
4. Run the actual GitHub matrix from a clean checkout, inspect downloaded archives
   and notices, and verify the draft-release job. Claim macOS support only after
   bundle loading, resources and real-host operation are checked on that platform.
5. Finalize the release version, notes and supported-platform wording, then create
   a preview tag and review its draft before publication.

## Follow-up simplification and performance work

- Centralize effect metadata gradually. Module categories, parameter ownership,
  DSP dispatch and tail budgets are currently spread across several headers;
  changes should have an exhaustive catalog/dispatch consistency check.
- Split state validation/migrations and UI actions out of the large plugin source
  file after release acceptance. Preserve name-based migrations and serialized IDs;
  a broad rewrite immediately before tagging would add regression risk.
- Measure a representative low-buffer rig before optimizing DSP. Crossover math
  still runs when splitting is off, control extraction reads every slot family,
  and long cabinet IRs use direct convolution. These are profiling candidates.
- Capture and delay modules reserve their largest windows: the documented
  16-slot capture configuration approaches 760 MiB at 192 kHz before other state.
  Optimize storage separately with long-window and bypass-drain regressions.
- The framework's VST3 adapter still takes a parameter mutex. Existing stage
  allocation tests and the ownership stress test do not establish lock-free
  end-to-end VST3 processing or complete host/control/audio concurrency coverage.
