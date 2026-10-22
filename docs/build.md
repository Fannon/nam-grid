# Build nam-grid

The native UI and DSP remain based on the official NAM plugin. The plugin displays
**nam-grid**, with `nam-grid.clap` / `nam-grid.vst3` artifacts. Its CLAP ID
(`com.StevenAtkinson.NAMGridPrototype`) and VST3 ID (`NGr1`) are retained from the grid
prototype so saved projects can identify it after a rescan. Avoid duplicate copies of the
old grid binary in plugin scan paths. The historical dual prototype on branch `prototype/dual-nam-clap` keeps its own ID (`NDu1`).

## Dependencies

- Windows x64: Visual Studio 2022 C++ tools (v143), Windows SDK, CMake >= 3.20,
  Python >= 3.8, Git. Ninja is optional.
- macOS arm64: Xcode command-line tools, CMake, Python, Git. CI builds and tests
  this target; real-host acceptance is tracked in the [release checklist](release-checklist.md).
- Recursive NAM submodules pinned in this repository. iPlug2 is now the official
  repository at `d54f69050f517e43b941d88c2a170f0a840b9ee4`.
- SDKs downloaded by `scripts/setup-deps.py`: CLAP 1.2.10 at `a47f6ba`, helpers at
  `29389e4`, and MIT-licensed VST3 3.8.1 at `3cdf9ca`. No WebView, JUCE, or prebuilt
  graphics libraries are needed for this build.

From the repository root:

```sh
git submodule sync --recursive
git submodule update --init --recursive
python scripts/setup-deps.py
cmake -S . -B build-grid -DCMAKE_BUILD_TYPE=Release
cmake --build build-grid --config Release --target nam-grid-clap nam-grid-vst3 nam-stage-tests nam-clap-smoke nam-tuner-tests nam-clap-bench nam-library-tests nam-wav-tests nam-resampling-tests --parallel 3
ctest --test-dir build-grid -C Release --output-on-failure
```

CI fetches Eigen from `eigen-mirror/eigen` on GitHub using a command-scoped URL
rewrite, including nested Eigen submodules. This avoids GitLab checkout outages
while retaining every pinned commit. For the same workaround locally, replace
the submodule-update command above with:

```sh
git -c "url.https://github.com/eigen-mirror/eigen.insteadOf=https://gitlab.com/libeigen/eigen" submodule update --init --recursive --depth=1
```

For MSVC without the Visual Studio generator, use an x64 developer command prompt
and add `-G Ninja` to the configure command. Local verification used MSVC 19.44,
Windows SDK 10.0.26100, CMake 4.4.4, Ninja 1.13.2 and Release optimization.
The optional portable toolchain used in this workspace is outside this repository.

The setup script applies the two MIT Core allocation fixes, the AudioDSPTools WAV
decoder safety/polarity and resampler warm-up fixes, and the iPlug2 CLAP enum-metadata
fix from `patches/`. Core remains pinned to `9c7b185`,
AudioDSPTools to `e19ef4b`; these and iPlug2 are intentionally modified after setup.
The patch files are the reproducible source of these changes. Do not reset
those submodules after setup without applying the patches again. Setup checks whether each
patch is already applied, and refuses patches that cannot be applied cleanly.

Output:

- Windows CLAP: `build-grid/out/nam-grid.clap`
- Windows VST3: `build-grid/out/nam-grid.vst3/Contents/x86_64-win/nam-grid.vst3`
- macOS CLAP/VST3: bundles under `build-grid/out`.
- `ThirdPartyNotices.txt` and `notices/` accompany the binaries. Preserve them when sharing artifacts.

Automatic deployment is disabled. Point Bitwig at this build's `out` directory for
local evaluation, or copy the artifact to a separate test location. The baseline benchmark
plugin is built under `build-grid/benchmark`, outside the artifact directory.

Use `-DNAM_FORMATS=CLAP` to build only CLAP. Requested formats fail configuration if their
SDKs are absent. The existing Visual Studio solution/Xcode packaging scripts are retained
for reference; the supported build entry point is CMake. The shared GitHub workflow
is `.github/workflows/dual-prototype.yml`.
The scheduled native build and release workflows reuse that same build/test job.
Release archives are `nam-grid-windows-x64.zip` and `nam-grid-macos-arm64.zip`;
each includes CLAP, VST3, and third-party notices. Archives are created before
artifact upload to retain macOS executable permissions.

To create a release, push a version tag such as `v0.1.0`, or open **Actions →
Release Native → Run workflow** and enter an existing version tag. The workflow
resolves the tag to a commit, builds and tests that commit on both platforms, and
creates a draft release with the archives after all checks pass. Review and publish
the draft from GitHub Releases. Publishing a release directly in GitHub also triggers
the builds and attaches the archives to that release. Re-running the workflow
replaces its archives without changing the release notes or publication status.
Manual **Build Native** runs only produce build artifacts.

Linux plugin releases are blocked by the pinned iPlug2 framework. Its CMake modules
report Linux as unsupported, and `IGraphicsLinux.cpp` is an unfinished stub.
Adding a Linux runner alone cannot build this plugin; a working Linux framework
backend and CLAP/VST3 platform integration are required before adding Linux to the
release matrix. No Linux plugin archive is currently produced.

## Using a local build

Point your DAW at `build-grid/out` for local evaluation, then rescan plugins.
For installation and a first rig, see the [main README](../README.md).
The maintained [editor guide](ui.md), [browser guide](browser.md),
[routing reference](grid.md) and [preset guide](presets.md) cover library setup,
slot editing, external file references and project recall.

## Reproduce the CPU comparison

For an on-demand report covering every built-in FX mode, NAM, IR loading and
small-buffer callback deadlines, see [fx-performance.md](fx-performance.md).
That tool is opt-in and runs separately from CTest and CI.

For DSP/state regression checks without opening the native editor, set
`NAM_GRID_HEADLESS_SMOKE=1` when running `nam-clap-smoke`. This includes Smart Gate
learning, host automation, routing/modulation and cached-control checks in every
slot. `NAM_GRID_CACHE_SMOKE=1` limits the later checks to cached slot automation.
Unset these variables for the normal complete CLAP/editor smoke test.

```sh
python scripts/prepare-baseline.py
cmake -S . -B build-grid -DNAM_BUILD_BASELINE=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-grid --config Release --target NAMBaseline-clap nam-clap-bench
```

Run `nam-clap-smoke` (or CTest) first to generate `fixtures/drive.nam` and `fixtures/amp.nam`
in the test working directory. The benchmark executable is in `build-grid/NeuralAmpModeler`
(or its `Release` subdirectory for Visual Studio). Run from any directory using absolute paths:

```text
nam-clap-bench <baseline.clap> original 1 <drive.nam> <amp.nam>
nam-clap-bench <baseline.clap> original 2 <drive.nam> <amp.nam>
nam-clap-bench <prototype.clap> dual     1 <drive.nam> <amp.nam>
```

The baseline script extracts the original plugin DSP/header/state reader from upstream commit
`16be869` and qualifies its base class for CLAP. It uses the same framework, Core and compiler
as the prototype. The benchmark establishes host rate/block size before loading baseline state,
because the original loader assumes those values are available. The baseline is a DSP comparison
build, not an official upstream CLAP release. Compare preserved binaries under
matching conditions; use [fx-performance.md](fx-performance.md) for current reporting.

WAV regression tests are in the `nam-wav-tests` target (`wav-decoder` in CTest).
Build that target alongside the existing library/stage/CLAP tests. A separate
configuration with `-DNAM_WAV_ASAN=ON` enables MSVC AddressSanitizer or GCC/Clang
address/undefined sanitizers for this focused target. Build `nam-wav-tests` in
that configuration and run `ctest --test-dir <build> -C Release -R wav-decoder --output-on-failure`.

Resampler warm-up regressions are in `nam-resampling-tests` (`resampling` in CTest).
They use exact WDL allocations to expose reads hidden by its usual allocation padding.
For a separate sanitizer build, configure with `-DNAM_RESAMPLING_ASAN=ON`, build that
target, and run `ctest --test-dir <build> -C Release -R '^resampling$' --output-on-failure`.
On Windows run both build and test inside the same x64 developer command prompt so
the compiler's `clang_rt.asan_dynamic-x86_64.dll` is on `PATH`. Ordinary builds leave
this option off and do not need that DLL.

For browser UI iteration on Windows, set `NAM_GRID_BROWSER_SMOKE=1` when running
`nam-clap-smoke`. This retains the DSP/state setup and the browser/preset interaction
checks, skipping the large slot-edit matrix and subsequent unrelated editor checks.
Unset it for the complete `clap-smoke` regression. An optional fourth argument names
a screenshot directory; captures come from the plugin's own OpenGL surface.
The native test restores library preferences after a successful run.

For a native FX visualization sweep, set `NAM_GRID_FX_VISUALS=1` instead. It
retains the DSP/state setup, then renders all 55 catalog modules at musical
defaults with audio and at minimum/maximum control values. Supply the optional
screenshot directory to review `fx-<effect ID>-default/min/max.bmp` artifacts.
It exits before the unrelated browser/menu matrix. See [ui.md](ui.md) for display semantics; ordinary CTest still runs the complete native editor regression.
For a targeted iteration, `NAM_GRID_FX_VISUALS_ONLY=12,15` limits that opt-in sweep
to the listed effect IDs (Digital Delay and Room here); unset it for all modules.
