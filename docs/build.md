# Build NAM Grid

NAM Grid builds `nam-grid.clap` and `nam-grid.vst3` with CMake. Its persistent
host identity is CLAP ID `io.github.fannon.nam-grid`, vendor **Simon Heimler**,
and VST3 plugin/manufacturer IDs `NGrd`/`Fann` (see
[`config.h`](../NeuralAmpModeler/config.h)). Hosts use these IDs to find the
plugin in saved projects; they must never change after a release.

## Dependencies

- Windows x64: Visual Studio 2022 C++ tools (v143), Windows SDK, CMake >= 3.20,
  Python >= 3.8, Git. Ninja is optional.
- macOS arm64: Xcode command-line tools, CMake, Python, Git. CI builds and tests
  this target; real-host acceptance is tracked in the [release checklist](release-checklist.md).
- Recursive submodules pinned in this repository: iPlug2, NAM Core,
  AudioDSPTools and Eigen.
- SDKs downloaded by `scripts/setup-deps.py`: CLAP 1.2.10 at `a47f6ba`, helpers at
  `29389e4`, and MIT-licensed VST3 3.8.1 at `3cdf9ca`. No WebView, JUCE, or prebuilt
  graphics libraries are needed for this build.

From the repository root:

```sh
git submodule sync --recursive
git submodule update --init --recursive
python scripts/setup-deps.py
cmake -S . -B build-grid -DCMAKE_BUILD_TYPE=Release
cmake --build build-grid --config Release --target nam-grid-clap nam-grid-vst3 nam-stage-tests nam-clap-smoke nam-tuner-tests nam-parameter-contract nam-library-tests nam-wav-tests nam-resampling-tests nam-ir-tests --parallel 3
ctest --test-dir build-grid -C Release --output-on-failure
python -m unittest discover -s tests -p "test_*.py"
```

CI fetches Eigen from `eigen-mirror/eigen` on GitHub using a command-scoped URL
rewrite, including nested Eigen submodules. This avoids GitLab checkout outages
while retaining every pinned commit. For the same workaround locally, replace
the submodule-update command above with:

```sh
git -c "url.https://github.com/eigen-mirror/eigen.insteadOf=https://gitlab.com/libeigen/eigen" submodule update --init --recursive --depth=1
```

For MSVC without the Visual Studio generator, use an x64 developer command prompt
and add `-G Ninja` to the configure command.

The setup script applies the two MIT Core allocation fixes, the AudioDSPTools WAV
decoder safety/polarity and resampler warm-up fixes, and the iPlug2 CLAP enum-metadata
fix from `patches/`. The patch files are the reproducible source of these changes;
do not reset those submodules after setup without applying the patches again.
Setup checks whether each patch is already applied, and refuses patches that cannot
be applied cleanly.

Output:

- Windows CLAP: `build-grid/out/nam-grid.clap`
- Windows VST3: `build-grid/out/nam-grid.vst3/Contents/x86_64-win/nam-grid.vst3`
- macOS CLAP/VST3: bundles under `build-grid/out`.
- `ThirdPartyNotices.txt` and `notices/` accompany the binaries. Preserve them when sharing artifacts.

Automatic deployment is disabled. Point your DAW at `build-grid/out` for local
evaluation, or copy the artifact to a separate test location, then rescan plugins.
Use `-DNAM_FORMATS=CLAP` to build only CLAP. Requested formats fail configuration if
their SDKs are absent.

## CI and releases

`.github/workflows/build-test.yml` builds and tests Windows x64 and macOS arm64 for
every pull request and every push to `main`. The weekly **Build Native** run and the
release workflow reuse that job. Release archives are `nam-grid-windows-x64.zip` and
`nam-grid-macos-arm64.zip`; each contains CLAP, VST3, `ThirdPartyNotices.txt` and
`notices/`. The packaging step fails if a required notice is missing. Archives are
created before artifact upload to retain macOS executable permissions.

To release, set the version in `config.h` (`PLUG_VERSION_STR`, `PLUG_VERSION_HEX`,
`PLUG_VERSION_RC`) and both `resources/nam-grid-*-Info.plist` files, then push a
matching tag such as `v0.1.0`. Alternatively, open **Actions → Release Native → Run
workflow** with an existing tag. The workflow resolves the tag to a commit, rejects
it unless every version string in that commit matches, builds and tests the commit on
both platforms, and creates a draft release with the archives and
[release notes](release-notes.md). Review and publish the draft from GitHub Releases.
Publishing a release also triggers the builds and attaches the archives; re-running
the workflow replaces its archives without changing the release notes or publication
status. Manual **Build Native** runs only produce build artifacts.

The macOS archives are not code-signed or notarized. The [README](../README.md)
explains how users clear the download quarantine.

Linux plugin releases are blocked by the pinned iPlug2 framework. Its CMake modules
report Linux as unsupported, and `IGraphicsLinux.cpp` is an unfinished stub.
A working Linux framework backend and CLAP/VST3 platform integration are required
before adding Linux to the release matrix.

## Tests

| CTest | Coverage |
| --- | --- |
| `ir-convolution` | Adaptive direct/FFT cabinet convolution against the direct processor, variable blocks, resampling and allocation checks |
| `capture-library` | Library scanning, metadata, search, menus and atomic preference writes |
| `wav-decoder` | WAV decoding and malformed-file rejection |
| `resampling` | Resampler warm-up with exact WDL allocations |
| `stages` | Every DSP module, slot stages, grid routing and realtime allocation checks |
| `clap-smoke` | The real CLAP plugin: state recall and rejection, automation, every catalog control, presets, library browser and (on Windows) the native editor |
| `tuner` | Tuner detection, mute and automation through CLAP |
| `parameter-contract` | Frozen host IDs, names, ranges, mappings, module/algorithm IDs and plugin identity |
| `vst3-parameter-contract` | Windows only: the built VST3 controller's IDs, class ID, automation flags and normalization |

The Python unit tests cover the on-demand report and audit scripts. See the
[compatibility contract](development.md#parameter-and-project-compatibility) before
changing controls, modules or saved state.

For DSP/state regression checks without opening the native editor, set
`NAM_GRID_HEADLESS_SMOKE=1` when running `nam-clap-smoke`. This includes Smart Gate
learning, host automation, routing/modulation and cached-control checks in every
slot. `NAM_GRID_CACHE_SMOKE=1` limits the later checks to cached slot automation.
Unset these variables for the normal complete CLAP/editor smoke test.

WAV regression tests can run under sanitizers: configure a separate build with
`-DNAM_WAV_ASAN=ON`, build `nam-wav-tests`, and run
`ctest --test-dir <build> -C Release -R wav-decoder --output-on-failure`.
Resampler warm-up regressions work the same way with `-DNAM_RESAMPLING_ASAN=ON`,
target `nam-resampling-tests` and `-R '^resampling$'`. On Windows run both build and
test inside the same x64 developer command prompt so the compiler's
`clang_rt.asan_dynamic-x86_64.dll` is on `PATH`. Ordinary builds leave these options
off and do not need that DLL.

For CPU, deadline and waveform-equivalence reports covering every built-in FX mode,
NAM and IR loading, see [fx-performance.md](fx-performance.md). Those tools are
opt-in and run separately from CTest.

## Editor screenshots

The README gallery comes from the opt-in, Windows-only `nam-readme-gallery`
target; see [screenshots/README.md](screenshots/README.md).

For browser UI iteration on Windows, set `NAM_GRID_BROWSER_SMOKE=1` when running
`nam-clap-smoke`. This retains the DSP/state setup and the browser/preset interaction
checks, skipping the large slot-edit matrix and subsequent unrelated editor checks.
An optional fourth argument names a screenshot directory; captures come from the
plugin's own OpenGL surface. The native test restores library preferences after a
successful run.

For a native FX visualization sweep, set `NAM_GRID_FX_VISUALS=1` instead. It
retains the DSP/state setup, then renders every catalog module at musical defaults
with audio and at minimum/maximum control values. Supply the optional screenshot
directory to review `fx-<effect ID>-default/min/max.bmp` artifacts. It exits before
the unrelated browser/menu matrix. See [ui.md](ui.md) for display semantics.
For a targeted iteration, `NAM_GRID_FX_VISUALS_ONLY=11,14` limits the sweep to the
listed module IDs (Digital Delay and Room here); unset it for all modules.
