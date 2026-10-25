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
- Linux x64: GCC >= 12, CMake, Ninja, Python, Git and the X11, OpenGL, FreeType,
  Fontconfig and GTK 3 development packages. On Ubuntu 22.04, the oldest supported
  release base (glibc 2.35):

  ```sh
  sudo apt-get install g++-12 ninja-build pkg-config libfreetype-dev libfontconfig-dev     libx11-dev libxi-dev libxext-dev libxrender-dev libxrandr-dev libxfixes-dev     libxcursor-dev libxinerama-dev libxtst-dev libgl-dev libgtk-3-dev libasound2-dev libjack-jackd2-dev libpulse-dev xvfb libgl1-mesa-dri
  ```

  Configure with `CC=gcc-12 CXX=g++-12` and `-G Ninja`. Run CTest under
  `xvfb-run -a` on machines without a display; see [Linux](#linux).
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
decoder safety/polarity, resampler warm-up and GCC build fixes, and two iPlug2 CLAP adapter fixes
from `patches/`: enum metadata, and host-contract corrections (bounded value-to-text,
canonical stepped text, looping short state writes, a value rescan after host state
loads, and correct main-port configuration summaries). The patch files are the reproducible source of these changes;
do not reset those submodules after setup without applying the patches again.
Setup checks whether each patch is already applied, and refuses patches that cannot
be applied cleanly.

Setup also backports the planar NEON A2 kernels from
[NAM Core PR #313](https://github.com/sdatkinson/NeuralAmpModelerCore/pull/313)
to the pinned Core API. Supported ARM targets select these kernels automatically;
other targets retain the portable implementation. Release stage tests compare
both implementations bit-for-bit, including odd block sizes and repeated prewarm.
Do not enable fast-math: those transformations invalidate the arithmetic-order
guarantee. To compare the portable path, add
`-DCMAKE_CXX_FLAGS=-DNAM_DISABLE_A2_PLANAR` when configuring a separate build.

macOS uses AudioFFT's Apple Accelerate backend for cabinet IR convolution.
`-DNAM_USE_ACCELERATE_FFT=OFF` selects the portable FFT backend for comparisons.
FFT rounding can differ between backends; convolution tests check the direct
reference, variable blocks, silence/tails and processing allocations.

Output:

- Windows CLAP: `build-grid/out/nam-grid.clap`
- Windows VST3: `build-grid/out/nam-grid.vst3/Contents/x86_64-win/nam-grid.vst3`
- macOS CLAP/VST3: bundles under `build-grid/out`.
- Linux CLAP: the single file `build-grid/out/nam-grid.clap`, with the font embedded.
  VST3: the `build-grid/out/nam-grid.vst3` bundle (`Contents/x86_64-linux/nam-grid.so`).
  The build also leaves loose `resources/` copies in `out/`; the plugins do not need them.
- `ThirdPartyNotices.txt` and `notices/` accompany the binaries. Preserve them when sharing artifacts.
- The plugin embeds `NeuralAmpModeler/resources/defaults` (the RiffSmith Clean capture and the
  Clean Default preset) through `cmake/EmbedFiles.cmake`, which needs no extra tools. A fresh
  install writes them into its new default library folders. The preset names its capture by
  filename; keep it loadable, as the smoke test checks.

Automatic deployment is disabled. Point your DAW at `build-grid/out` for local
evaluation, or copy the artifact to a separate test location, then rescan plugins.
Use `-DNAM_FORMATS=CLAP` to build only CLAP. Requested formats fail configuration if
their SDKs are absent.

## CI and releases

`.github/workflows/build-test.yml` builds and tests the plugin. Hosted minutes are
billed for this private repository (Linux x1, Windows x2, macOS x10), so the workflow is the
merge gate rather than a post-merge check:

- Pull requests build and test Windows x64 only; documentation-only changes
  (`docs/**`, Markdown, `LICENSE`) do not build. A new push cancels the running build.
- `main` is not rebuilt after merging.
- macOS arm64 and Linux x64 build in the weekly **Build Native** run, on manual runs
  and for releases, which all build every platform.
- sccache caches compiled objects in the GitHub Actions cache (Ninja + MSVC on
  Windows), so unchanged dependencies are not recompiled. A 45-minute job timeout
  stops hung editor tests; pull request artifacts are kept for 3 days, others for 14.

A manual **Build self-hosted** workflow (`self-hosted.yml`) runs the Linux
build and tests on a self-hosted runner. Provision it once with
`sudo scripts/setup-selfhosted-linux.sh`; the workflow checks for the compiler,
CMake, Ninja, X11/GTK and Xvfb dependencies before starting. This lane is
validation-only: the runner may use a newer glibc than the release base, so its
binaries are not published. Release Linux archives remain built on
`ubuntu-22.04` by `build-test.yml`. The workflow is manual-only; do not attach a
self-hosted runner to public pull-request workflows.

The weekly **Build Native** run and the release workflow reuse that job. Release archives are
`nam-grid-windows-x64.zip`, `nam-grid-macos-arm64.zip` and `nam-grid-linux-x64.zip`; each
contains CLAP, VST3, `ThirdPartyNotices.txt` and `notices/`. `scripts/package-release.py`
creates them and fails if a required notice is missing. Archives are created before
artifact upload to retain executable permissions.

To release, set the version in `config.h` (`PLUG_VERSION_STR`, `PLUG_VERSION_HEX`,
`PLUG_VERSION_RC`) and both `resources/nam-grid-*-Info.plist` files, then push a
matching tag such as `v0.1.0`. Alternatively, open **Actions → Release Native → Run
workflow** with an existing tag. The workflow resolves the tag to a commit, rejects
it unless every version string in that commit matches, builds and tests the commit on
all three platforms, and creates a draft release with the archives and
[release notes](release-notes.md). Review and publish the draft from GitHub Releases.
Publishing a release also triggers the builds and attaches the archives; re-running
the workflow replaces its archives without changing the release notes or publication
status. Manual **Build Native** runs only produce build artifacts.

macOS builds ad-hoc sign each complete plugin bundle after linking. They are not
Developer ID signed or notarized. The [README](../README.md) explains how users
clear the download quarantine.

## Linux

The pinned iPlug2 has no Linux backend. On Linux only, setup applies
`patches/iplug2-linux-support.patch`: the X11/GLX NanoVG backend, CLAP and VST3
integration from [iPlug2 PR #1336](https://github.com/iPlug2/iPlug2/pull/1336), rebased
onto the pinned commit after the CLAP patches. Windows and macOS sources are unchanged.
The patch also lets hosts without CLAP timer support keep the editor's own render thread,
and tracks the registered X11 file descriptor. When upstream merges Linux support,
update the iPlug2 pin and remove the patch.

The editor uses X11 (XWayland on Wayland desktops). File dialogs load GTK 3 at runtime.
CLAP hosts drive the editor through their timer and the X11 connection's file descriptor;
the `clap-smoke` test does the same in an X11 parent window, renders the editor, clicks
through the X server, and reopens it. Without a display those editor checks are skipped,
so run CTest under `xvfb-run -a -s "-screen 0 1920x1080x24"`. The VST3 controller test is
Windows only.

Build releases on Ubuntu 22.04 so they run on that glibc and newer. On Windows, a
dedicated WSL distribution works without changing others:
`wsl --install Ubuntu-22.04 --name nam-linux-build`, then build in its Linux file system,
not under `/mnt/c`.

## Manual releases

Without GitHub Actions, build, test and package each platform locally from the tagged
commit, then upload the archives:

```sh
git checkout v0.1.0 && git submodule update --init --recursive && python scripts/setup-deps.py
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release   # add -G Ninja on Windows and Linux
cmake --build build-release --config Release --target nam-grid-clap nam-grid-vst3 nam-stage-tests nam-clap-smoke nam-tuner-tests nam-parameter-contract nam-library-tests nam-wav-tests nam-resampling-tests nam-ir-tests --parallel 3
ctest --test-dir build-release -C Release --output-on-failure   # xvfb-run -a ... on Linux
python scripts/package-release.py --build-dir build-release --platform macos-arm64
gh release upload v0.1.0 build-release/nam-grid-macos-arm64.zip --repo Fannon/nam-grid
```

`--platform` is `windows-x64`, `macos-arm64` or `linux-x64`. Create the draft release
once, with `gh release create v0.1.0 --draft --verify-tag --title "NAM Grid v0.1.0"
--notes-file docs/release-notes.md --repo Fannon/nam-grid`, and publish it from GitHub
Releases after all archives are attached and inspected. Check the version strings first,
as the release workflow does. On Windows, also add `nam-vst3-parameter-contract` to the
build targets.

## Tests

| CTest | Coverage |
| --- | --- |
| `ir-convolution` | Adaptive direct/FFT cabinet convolution against the direct processor, variable blocks, resampling and allocation checks |
| `capture-library` | Library scanning, metadata, search, menus and atomic preference writes |
| `wav-decoder` | WAV decoding and malformed-file rejection |
| `resampling` | Resampler warm-up with exact WDL allocations |
| `stages` | Every DSP module, slot stages, grid routing, realtime allocation checks and ARM planar/portable A2 bit identity |
| `clap-smoke` | The real CLAP plugin: state recall and rejection, automation, every catalog control, presets, library browser and (on Windows) the native editor |
| `tuner` | Tuner detection, mute and automation through CLAP |
| `parameter-contract` | Frozen host IDs, names, ranges, mappings, module/algorithm IDs and plugin identity; CLAP host-contract regressions (text buffers, stepped text, short writes/reads, rescans, port configurations) |
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

For focused native mode-selector checks on Windows, set `NAM_GRID_MODE_SMOKE=1`
when running `nam-clap-smoke`. It checks every dropdown choice, the current-item
checkmark, cancellation, unchanged selection, recall and two-choice toggles, then
exits before the remaining editor checks. An optional screenshot directory captures
the closed selectors for visual review. Unset it for the complete smoke test.

The README gallery comes from the opt-in, Windows-only `nam-readme-gallery`
target; see [screenshots/README.md](screenshots/README.md).

For browser UI iteration on Windows, set `NAM_GRID_BROWSER_SMOKE=1` when running
`nam-clap-smoke`. This retains the DSP/state setup and the browser/preset interaction
checks, skipping the large slot-edit matrix and subsequent unrelated editor checks.
An optional fourth argument names a screenshot directory; captures come from the
plugin's own OpenGL surface. The native test runs in a private temporary
`NAM_GRID_HOME`, so it never reads or changes your own settings or library folders,
even if it aborts.

For a native FX visualization sweep, set `NAM_GRID_FX_VISUALS=1` instead. It
retains the DSP/state setup, then renders every catalog module at musical defaults
with audio and at minimum/maximum control values. Supply the optional screenshot
directory to review `fx-<effect ID>-default/min/max.bmp` artifacts. It exits before
the unrelated browser/menu matrix. See [ui.md](ui.md) for display semantics.
For a targeted iteration, `NAM_GRID_FX_VISUALS_ONLY=11,14` limits the sweep to the
listed module IDs (Digital Delay and Room here); unset it for all modules.
