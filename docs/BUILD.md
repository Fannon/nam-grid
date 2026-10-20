# Build the Phase 1 prototype

The native UI and DSP remain based on the official NAM plugin. The plugin displays
**NAM Dual Prototype**, has its own plugin ID (`NDu1`), and retains upstream's binary
filename. Do not overwrite an official NAM installation with this prototype.

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
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --config Release --target NeuralAmpModeler-clap NeuralAmpModeler-vst3 nam-stage-tests nam-clap-smoke nam-clap-bench nam-library-tests --parallel 3
ctest --test-dir build-native -C Release --output-on-failure
```

For MSVC without the Visual Studio generator, use an x64 developer command prompt
and add `-G Ninja` to the configure command. Local verification used MSVC 19.44,
Windows SDK 10.0.26100, CMake 4.4.4, Ninja 1.13.2 and Release optimization.
The optional portable toolchain used in this workspace is outside this repository.

The setup script also applies two narrow MIT-licensed upstream Core allocation fixes
from `patches/`. Core remains pinned to `9c7b185`; a modified Core submodule is expected
after setup. The patch files are the reproducible source of these changes. Do not reset
that submodule after setup without applying the patches again. Setup checks whether each
patch is already applied, and refuses patches that cannot be applied cleanly.

Output:

- Windows CLAP: `build-native/out/NeuralAmpModeler.clap`
- Windows VST3: `build-native/out/NeuralAmpModeler.vst3/Contents/x86_64-win/NeuralAmpModeler.vst3`
- macOS CLAP/VST3: bundles under `build-native/out` (unverified).
- `ThirdPartyNotices.txt` and `notices/` accompany the binaries. Preserve them when sharing artifacts.

Automatic deployment is disabled. Point Bitwig at this build's `out` directory for
local evaluation, or copy the artifact to a separate test location. The baseline benchmark
plugin is built under `build-native/benchmark`, outside the artifact directory.

Use `-DNAM_FORMATS=CLAP` to build only CLAP. Requested formats fail configuration if their
SDKs are absent. The existing Visual Studio solution/Xcode packaging scripts are retained
for reference; this prototype's validated build entry point is CMake. The new GitHub workflow
is `.github/workflows/dual-prototype.yml`; it has not been run remotely in this session.

## Using the prototype

The upper Input knob controls **Amp input**; the upper Output knob is the master output.
The added lower row contains Drive input/trim, Amp trim and the two enable switches.
The two `.nam` selectors identify Drive and Amp; the existing `.wav` selector is the cabinet IR.
The initial output mode is Raw. Optional input calibration and output normalization/calibration
in Settings apply to the Amp. Drive uses raw model levels and its musical gain/trim controls.

### Capture folder browser

Open **Settings (gear icon) → NAM capture folder** and choose your library directory.
The folder is saved with project state and as the default for new instances. The default lives in
`NAMDualPrototype/library.json` beneath the current user's application-support directory;
project recall overrides that default for the recalled instance. No library path is compiled in.
**Rescan** refreshes the index after adding/moving files. **Clear** removes the folder setting
and restores the original sibling-file menus; it does not unload either active model.

Click the Drive or Amp **filename/selection text** to open the library menu. Subfolders become
nested submenus. The file icon still opens the ordinary single-file picker. Previous/next arrows
cycle through the current library search/category results. The search field below the IR selector
matches all entered words across filenames, relative folders, model name, make and model metadata;
press Enter to apply. The shared gear dropdown filters both NAM selectors. Neither filter changes
audio until you select a capture. Search/filter choices are transient UI state.

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
The Cabinet NAM slot is still a later milestone; the Cabinet category is currently descriptive.

Scanning runs on a background worker and reads metadata without constructing DSP models. The index
stays in memory, with no database or cloud requests. All `.nam` files under the selected root are
included (case-insensitive extension), including export/catalog subfolders; WAVs are not indexed.
Directory symlinks are not followed. Gear/search text matching currently folds ASCII case only.
A missing folder displays a status message without unloading selected models. Metadata inspection is
not full model validation; malformed/unsupported models still fail gracefully through the normal loader.

Paths are currently external file references, not embedded models or project-relative paths.
Keep model/IR files at stable locations; reselect missing files in the appropriate browser.
Loading errors preserve the previous selection during interactive loading. Project recall of
a missing file clears that stage's DSP, retains the requested path, and displays a failure.

Model file parsing/prewarming is synchronous on the control thread, so a large model can
briefly occupy the UI thread. It never runs in `ProcessBlock`. Latency-changing CLAP loads
request host restart; the new model waits for host reactivation. Same-latency loads use a
short fade-out/swap/fade-in. A bypassed model retains its conversion latency through a dry
delay and skips inference after its bypass ramp settles.

This first increment keeps upstream's post-model gate attenuation and post-Amp EQ placement.
Cabinet NAM selection, pre-Amp EQ placement, post EQ and the horizontal chain UI are later phases.

## Reproduce the CPU comparison

```sh
python scripts/prepare-baseline.py
cmake -S . -B build-native -DNAM_BUILD_BASELINE=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --config Release --target NAMBaseline-clap nam-clap-bench
```

Run `nam-clap-smoke` (or CTest) first to generate `fixtures/drive.nam` and `fixtures/amp.nam`
in the test working directory. The benchmark executable is in `build-native/NeuralAmpModeler`
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
