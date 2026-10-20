# Repository assessment and first increment

Implementation status and the 2026-10-19 upstream checkpoint are recorded in
`VERIFICATION.md`; build/setup commands are in `BUILD.md`. The proposal below describes
the original investigation and planned increments, not additional completed features.

Inspected official NAM plugin commit `16be869746b8915885c7a35bafdcc0061faeb50e`.
The original investigation used branch `prototype/dual-nam-clap`. Development now
continues on `experiment/fixed-4x4-grid` in [Fannon/nam-grid](https://github.com/Fannon/nam-grid).
Facts below refer to that baseline unless explicitly marked as proposals.

## Verified architecture

| Responsibility | Files / classes |
| --- | --- |
| Host lifecycle, DSP, gains, model loading, UI layout | `NeuralAmpModeler/NeuralAmpModeler.cpp`, class `NeuralAmpModeler` |
| Parameter IDs, ownership, sample-rate adapter | `NeuralAmpModeler/NeuralAmpModeler.h`, `EParams`, `ResamplingNAM` |
| Native model selector, settings, meters | `NeuralAmpModeler/NeuralAmpModelerControls.h`, `NAMFileBrowserControl`, `NAMSettingsPageControl`, `NAMMeterControl` |
| EQ | `NeuralAmpModeler/ToneStack.{h,cpp}`, `BasicNamToneStack` |
| State migration | `NeuralAmpModeler/Unserialization.cpp`, included by the main `.cpp` |
| Inference and file parsing | `NeuralAmpModelerCore/NAM/get_dsp.cpp`, `nam::get_dsp`, `nam::DSP` |
| Gate / IR / filters | `AudioDSPTools/dsp/NoiseGate.*`, `ImpulseResponse.*`, `RecursiveLinearFilter.*` |

Existing chain: stereo averaged to mono (standalone sums), input gain including optional input
calibration, gate detector, NAM, gate gain, bass/mid/treble EQ, optional IR, 5 Hz DC blocker,
output gain including optional normalization/calibration, mono broadcast to connected outputs.
The gate detector passes audio through; attenuation currently happens **after** NAM. Moving
attenuation ahead of both models is a deliberate sound/behavior change, not just copying the code.

EQ is three minimum-phase biquads: bass shelf at 150 Hz, mid bell at 425 Hz, treble shelf at
1800 Hz. Gate, EQ and IR have independent switches; the NAM itself has no bypass parameter.
The IR is direct time-domain convolution, truncated to 8192 taps, with an existing -18 dB and
sample-rate-dependent scaling. Reuse it for Phase 1; long IR CPU cost is a known limitation.

Pinned dependencies: NAM Core `9c7b185`, AudioDSPTools `e19ef4b`, Eigen `ed8cda3`,
iPlug2 fork `de5a4fb`. Core's `NAM/version.h` says 0.5.3; its CMake project says 0.4.0,
so the commit is the authoritative version. File versions 0.5.0 through 0.7.0 are fully
accepted by the default support checker. Registered architectures include Linear, ConvNet,
LSTM, WaveNet and SlimmableContainer; slimmable WaveNet is also implemented. A2 is supported
by the WaveNet configuration, including per-layer kernel sizes and output heads. The specialized
`NAM/wavenet/a2_fast.cpp` accepts matching A2 standard/nano shapes; others use the generic
WaveNet path. Windows and macOS plugin configurations define `NAM_ENABLE_A2_FAST` and compile
that source. A1 example models are included. These are source facts, not a successful model test.

`ResamplingNAM` wraps the same Core inference object with Lanczos sample-rate conversion,
using the model's expected rate or 48 kHz for missing metadata. This is model-rate conversion,
not mandatory oversampling. Each wrapper reports conversion latency. `_UpdateLatency` reports
one model's conversion latency today; series stages must sum it. Bypass must preserve delay
or negotiate host restart before latency changes.

## Lifecycle risks that must be addressed

- `_StageModel` does synchronous file parsing/prewarm on the caller (UI picker/state restore),
  writes a shared `unique_ptr`, and `_ApplyDSPStaging` moves it in `ProcessBlock`. Replacing or
  clearing destroys the old inference object on the audio thread. UI reads that same object
  for metadata. Atomic removal flags alone do not protect these shared lifetimes.
- `_PrepareBuffers` runs in every audio block; it allocates on first use/growth. Gate/filter/IR
  helpers also lazily size buffers. Prepare the largest supported block before processing,
  including bypassed stages, and test allocations under changing block sizes.
- `_ResetModelAndIR` resets the pending model **or** active model, rather than both. The
  resampling wrapper passes the host rate to the encapsulated reset even when inference runs
  at model rate; verify and correct this when extracting the wrapper.
- Existing VST3 adapter takes `mParams_mutex` around processing. Upgrading the framework is
  not by itself proof that the complete callback is free of blocking locks.
- `_FallbackDSP` ignores its passed pointers and copies member arrays. That is incorrect for
  a reusable stage receiving the preceding stage's output; implement explicit passthrough.
- Invalid `.nam` handling catches only `runtime_error`; JSON/parser exceptions also need
  containment. Empty/invalid WAVs need validation before IR history length arithmetic.
- State stores absolute NAM/IR paths and sequential parameter doubles under a version header.
  `Unserialization.cpp` reads a fixed 13-param list for versions >= 0.7.14. Appending params
  alone does not restore new params. Missing files and an empty recalled path must explicitly
  replace the previous instance's selection, not leave stale audio behind.

## CLAP decision and builds

The pinned iPlug2 fork has no `IPlug/CLAP` adapter, `CLAP_API` case, or CLAP target.
Enabling a define cannot work. Current official iPlug2 commit
`d54f69050f517e43b941d88c2a170f0a840b9ee4` has `IPlug/CLAP/IPlugCLAP.{h,cpp}`,
CLAP SDK/helpers support, `Examples/IPlugEffect/projects/IPlugEffect-clap.vcxproj`, a
`macOS-CLAP.xcscheme`, and `Scripts/cmake/CLAP.cmake`.
Its active latency changes request host restart; account for this when switching model rates.

The user selected **upgrade iPlug2 and establish CLAP first**. The prototype pins that revision
without introducing JUCE. Compare/backport relevant NAM fork fixes (notably standalone device
validation) if standalone becomes a release target. Keep the format upgrade independently
reviewable from the two-model change.

Baseline build files:

- Windows: `NeuralAmpModeler/NeuralAmpModeler.sln`,
  `projects/NeuralAmpModeler-{app,vst3,aax}.vcxproj`,
  `config/NeuralAmpModeler-win.props`, root `common-win.props`; MSVC v143, C++20, Windows SDK.
- macOS: `projects/NeuralAmpModeler-macOS.xcodeproj/project.pbxproj`,
  `config/NeuralAmpModeler-mac.xcconfig`, root `common-mac.xcconfig`; NanoVG/Metal,
  app/VST3/AU targets and additional schemes. macOS builds cannot be verified on Windows.
- CI: `.github/workflows/build-native.yml` and `release-native.yml` fetch SDKs/prebuilt libraries
  and call `scripts/makedist-{win,mac}`. Pluginval steps are commented out. The Windows packaging
  script still hardcodes a VS2019 path despite v143 project settings.
- No plugin unit suite or plugin CMake build at baseline. Core has `tools/run_tests.cpp`,
  allocation tracking, A2 tests, `loadmodel`, `render`, `benchmodel`, `bench_a2_fast` CMake
  targets. Core CMake includes GNU flags in some MSVC paths; inspect before treating that suite
  as a Windows build recipe.

New root and plugin `CMakeLists.txt` use official iPlug2's format helpers for CLAP/VST3,
reuse the current NAM/DSP sources and native resources, and disable automatic deployment.
Fail configuration if requested SDKs are missing (iPlug2 otherwise silently skips targets).
New CLAP macOS Info.plist and dependency setup instructions are required. The existing solution
and Xcode packaging targets are legacy paths until migrated/validated; do not claim they build
unchanged against the new framework.

## Proposed narrow Phase 1 change

1. Build unchanged single-model CLAP/VST3 against upgraded iPlug2; load factory/lifecycle in
   a small CLAP host harness. Keep native UI and validate UI creation independently.
2. Extract `ResamplingNAM` into `NeuralAmpModeler/NAMStage.h` and reuse it in a fixed
   `std::array<NAMStage, 3>` (Drive, Amp, reserved Cabinet). Only Drive/Amp exposed in Phase 1.
   No graph and no duplicated inference implementation.
3. Prepare models and buffers on the control thread; publish ready objects through a bounded
   atomic handoff. Audio owns active objects; retired objects go back to the control thread
   for destruction. Handle rapid load/clear, pending replacement and shutdown. Reset only while
   processing is stopped; reject stale preparation generations after sample-rate changes.
4. Add independent input gain, output trim and enable parameters to the two stages, smoothing
   gains/bypass and transitions. Preserve upstream parameter indices. Keep musical gain separate
   from calibration; raw Drive output by default, avoiding independent loudness normalization
   between nonlinear stages. Existing amp input calibration/output modes need explicit mapping.
5. `NeuralAmpModeler.cpp`: process Drive then Amp, keep gate/EQ/IR/DC blocker/master and
   meters. Preserve EQ position for this increment; move/add pre/post EQ only in Phase 2.
   Native file browser row and compact extra knobs/switches; no complete UI redesign.
6. `Unserialization.cpp` and state methods: new distinguishable schema with named params,
   three slot paths and IR path, bounded parse before mutation, legacy migration into Amp,
   explicit clearing for absent paths, failure status with retained missing selection for relink.
7. Add focused DSP/CLAP lifecycle tests and CI build targets. See `VERIFICATION.md` for host
   acceptance. Cabinet NAM/IR mode, post-EQ and a reorganized horizontal UI remain Phase 2/3.

Ownership, allocation freedom, calibrated level transfer, variable-rate latency, and framework
API/resource compatibility are the main technical risks. External reference projects were not
copied. Plugin/Core MIT, iPlug2 zlib-style and component notices remain applicable; dependency
license notices must ship with artifacts.
