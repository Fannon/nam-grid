# Build nam-grid

The native UI and DSP remain based on the official NAM plugin. The plugin displays
**nam-grid**, with `nam-grid.clap` / `nam-grid.vst3` artifacts. Its CLAP ID
(`com.StevenAtkinson.NAMGridPrototype`) and VST3 ID (`NGr1`) are retained from the grid
prototype so saved projects can identify it after a rescan. Avoid duplicate copies of the
old grid binary in plugin scan paths. The historical dual prototype on branch `prototype/dual-nam-clap` keeps its own ID (`NDu1`).

## Dependencies

- Windows x64: Visual Studio 2022 C++ tools (v143), Windows SDK, CMake >= 3.20,
  Python >= 3.8, Git. Ninja is optional.
- macOS: Xcode command-line tools, CMake, Python, Git. The new CMake/macOS path and
  CI configuration are provided but have **not been run on macOS**.
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
cmake --build build-grid --config Release --target nam-grid-clap nam-grid-vst3 nam-stage-tests nam-clap-smoke nam-clap-bench nam-library-tests nam-wav-tests nam-resampling-tests --parallel 3
ctest --test-dir build-grid -C Release --output-on-failure
```

For MSVC without the Visual Studio generator, use an x64 developer command prompt
and add `-G Ninja` to the configure command. Local verification used MSVC 19.44,
Windows SDK 10.0.26100, CMake 4.4.4, Ninja 1.13.2 and Release optimization.
The optional portable toolchain used in this workspace is outside this repository.

The setup script applies the two MIT Core allocation fixes and the AudioDSPTools WAV
decoder safety/polarity and resampler warm-up patches from `patches/`. Core remains pinned to `9c7b185`,
AudioDSPTools to `e19ef4b`; both submodules are intentionally modified after setup. The patch files are the reproducible source of these changes. Do not reset
those submodules after setup without applying the patches again. Setup checks whether each
patch is already applied, and refuses patches that cannot be applied cleanly.

Output:

- Windows CLAP: `build-grid/out/nam-grid.clap`
- Windows VST3: `build-grid/out/nam-grid.vst3/Contents/x86_64-win/nam-grid.vst3`
- macOS CLAP/VST3: bundles under `build-grid/out` (unverified).
- `ThirdPartyNotices.txt` and `notices/` accompany the binaries. Preserve them when sharing artifacts.

Automatic deployment is disabled. Point Bitwig at this build's `out` directory for
local evaluation, or copy the artifact to a separate test location. The baseline benchmark
plugin is built under `build-grid/benchmark`, outside the artifact directory.

Use `-DNAM_FORMATS=CLAP` to build only CLAP. Requested formats fail configuration if their
SDKs are absent. The existing Visual Studio solution/Xcode packaging scripts are retained
for reference; this prototype's validated build entry point is CMake. The new GitHub workflow
is `.github/workflows/dual-prototype.yml`; it has not been run remotely in this session.
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

## Using the prototype

The 4x4 grid contains four serial input slots, two parallel four-slot lanes (A/B), and four
serial output slots. Each slot loads NAM and has input gain, trim and bypass. Empty slots pass
through. Blend is the percentage of lane B; 0% is A only, 50% averages both, and 100% is B only.
The header contains the preset browser, tuner, performance controls and settings.
IN gain sits before the grid; OUT gain and lane balance sit beside the output/parallel lanes.
Gate, EQ and cabinet IR are slot modules. The old Amp calibration,
output mode and Slim apply to Pre 2. See [GRID.md](GRID.md) for routing, migration, measurements and limits.

### Capture folder browser

Open **Library & settings / + Add folder** to add NAM and/or WAV IR directories (up to 32).
Folders are saved with project state and as the default for new instances. The default lives in
`NAMDualPrototype/library.json` beneath the current user's application-support directory;
project recall overrides that default for the recalled instance. Older single-folder preferences
and projects still load. No library path is compiled in. **Rescan all** refreshes the index;
**Remove** removes that source without unloading modules or deleting files. Scroll the list for
more than seven folders. Duplicate/overlapping sources index each file once.

Select a card to edit it in the bottom module panel. Double-click the card or use **Choose / Change**
in that panel to open its menu. Each library source has a submenu when multiple roots are configured;
subfolders remain nested. Individual NAM/WAV file pickers and Clear slot remain available.
Choosing a `.wav` entry automatically makes the slot an IR loader; choosing a `.nam` switches it
back to NAM. The scanner treats WAV files as IR candidates; decoding/validation occurs only on load.
Previous/next arrows stay within the selected file's physical folder and file type,
or within the current built-in module category. The header search/filter bar is
removed pending a dedicated browser. Selecting a card changes its editor without
changing audio. See [UI.md](UI.md) for the layout, compatibility and manual checks.

The gear categories are **Amp + Cab, Amp Head, Cabinet, Pedal, Outboard, Spaces**, plus **Unknown**.
Explicit `metadata.gear_type` takes priority (`amp_cab`/`full-rig`/`amp_pedal_cab`, `pedal`, `studio`, etc.).
When metadata is missing or ambiguous, explicit whole-word filename markers (`CAB`, `Full Rig`, `DI`,
`Direct`, `No Cab`) and category folder names are used. Markers are case-insensitive, including at the
end of the filename or separated by underscores/hyphens. `CAB` identifies Amp + Cab; direct/no-cab
markers identify Amp Head only with amp metadata or an amp folder as context. DI alone establishes no
cabinet, not the kind of processor: a pedal/outboard folder keeps that category, and a context-free DI
capture stays Unknown. Conflicting CAB/direct markers stay Unknown unless explicit metadata supplies
the category. `No Cab` is treated as direct, not as a positive CAB marker. Such results are marked
**inferred**. Folder inference includes the chosen root itself and inherits through maker/model
subfolders; the nearest recognized category folder wins. Common aliases include `Full Rigs`,
`Amp Heads`, `Direct Amps`, `Cabinets`/`IRs`, `Pedals`/`Stompboxes`, `Outboard`/`Studio Preamps`,
`Acoustic & Line` (Outboard fallback), and `Spaces`/`Rooms`. Metadata still takes priority, so an
explicitly tagged acoustic pedal stays Pedal inside `Acoustic & Line`. A generic `Amps` folder or
`amp` tag by itself stays Unknown because it doesn't establish cabinet inclusion. Classification does not analyze audio,
validate inference compatibility, automatically change cabinet bypass, or restrict a stage's selection.
NAM cabinets and WAV IRs can load in any grid slot.

Scanning runs on a background worker and reads metadata without constructing DSP models. The index
stays in memory, with no database or cloud requests. All `.nam` files under the selected root are
included alongside `.wav` IR candidates (case-insensitive extensions), including export/catalog subfolders.
Directory symlinks are not followed. Gear/search text matching currently folds ASCII case only.
A missing folder displays a status message without unloading selected models. Metadata inspection is
not full model validation; malformed/unsupported models still fail gracefully through the normal loader.

Paths are currently external file references, not embedded models or project-relative paths.
Saved content hashes allow relinking moved/renamed files within indexed library folders;
an existing chosen path is kept while its contents still match. Keep model/IR files available
when sharing rigs; reselect missing files or rescan the folders containing their new locations.
Loading errors preserve the previous selection during interactive loading. Project recall of
a missing file clears that stage's DSP, retains the requested path, and displays a failure.

Model file parsing/prewarming is synchronous on the control thread, so a large model can
briefly occupy the UI thread. It never runs in `ProcessBlock`. Latency-changing CLAP loads
request host restart; the new model waits for host reactivation. Same-latency loads use a
short fade-out/swap/fade-in. A bypassed model retains its conversion latency through a dry
delay and skips inference after its bypass ramp settles.

The grid retains upstream's gate attenuation, EQ and global WAV IR after its final serial row.
Gate, compressor, EQ and cabinet IR slot modules are included; see [SLOTS.md](SLOTS.md).

## Reproduce the CPU comparison

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
build, not an official upstream CLAP release. See `VERIFICATION.md` for results and limits.

WAV regression tests are in the `nam-wav-tests` target (`wav-decoder` in CTest).
Build that target alongside the existing library/stage/CLAP tests. A separate
configuration with `-DNAM_WAV_ASAN=ON` enables MSVC AddressSanitizer or GCC/Clang
address/undefined sanitizers for this focused target. See
[REVIEW_FIXES_20261020.md](REVIEW_FIXES_20261020.md) for reproduction details.

Resampler warm-up regressions are in `nam-resampling-tests` (`resampling` in CTest).
They use exact WDL allocations to expose reads hidden by its usual allocation padding.
For a separate sanitizer build, configure with `-DNAM_RESAMPLING_ASAN=ON`, build that
target, and run `ctest --test-dir <build> -C Release -R '^resampling$' --output-on-failure`.
On Windows run both build and test inside the same x64 developer command prompt so
the compiler's `clang_rt.asan_dynamic-x86_64.dll` is on `PATH`. Ordinary builds leave
this option off and do not need that DLL.
