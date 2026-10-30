# Project guidance

NAM Grid is a CLAP/VST3 guitar-rig plugin built on iPlug2, NAM Core and AudioDSPTools. This file holds the rules; the guides hold the details:

- [build.md](docs/build.md): toolchains, `build.sh`, the test catalogue, [upstream pins and patches](docs/build.md#upstream-pins-and-patches), CI and release builds.
- [development.md](docs/development.md): sources of truth, the [compatibility contract](docs/development.md#parameter-and-project-compatibility), adding modes and modules, slot preparation and audio processing.
- [module-implementation.md](docs/module-implementation.md): DSP conventions, latency, tails and sources per module.
- [release-checklist.md](docs/release-checklist.md) and [CONTRIBUTING.md](CONTRIBUTING.md).

## Project and parameter compatibility

- Released projects, host automation and `.ngrid` presets must keep loading with the same parameter meaning. Read the compatibility contract before changing parameters, modules, DSP modes or saved state.
- Preserve plugin identity, parameter IDs, descriptor names/storage keys, ranges, scaling, defaults and meaning. Append new parameter, module and algorithm IDs; never reorder, renumber or reuse released IDs. Use presentation labels for UI wording changes.
- Extend state schemas additively with declared defaults for older states; never lower or reuse schema numbers.
- During 0.x, deliberate DSP quality or performance improvements may change how existing rigs sound: document the audible change and verify it with DSP tests and measurements. From 1.0, material sound changes need a new mode identity.
- Built-in effects add no reported latency: no lookahead or compensation delay. Causal minimum-phase filtering is fine; check its phase against dry/wet and parallel paths. NAM model resampling may still need compensation.
- Settled rigs must process predictably. Modulation assignment, module insertion and mode changes may briefly interrupt; keep preparation and allocation off the audio path.
- Never re-record a compatibility fixture to make a failing change pass.

## Build and test

- `bash build.sh` builds into `build/<platform>/release/` (`windows`, `macos` or `linux`; plugins in `out/`). Add `--setup` after dependency pins or patches change and `--test` for the CTest and Python suites. Git Bash builds Windows plugins and WSL2 builds Linux plugins.
- Put special configurations in `build/<platform>/<purpose>/`, and never share a build directory between operating systems, compilers or worktrees. Root-level `build-*` directories are legacy caches: add none, and don't move them.
- While iterating, build and run one target from the build directory, e.g. `cmake --build build/<platform>/release --target nam-stage-tests` and `ctest --test-dir build/<platform>/release -C Release -R '^stages$' --output-on-failure`. Run `bash build.sh --test` once when the change is complete.
- Choose tests by what changed ([coverage table](docs/build.md#tests)):
  - Module DSP: `stages`, plus `aliasing` for nonlinear or oversampled paths.
  - Parameters, modes or state: `parameter-contract`, `clap-smoke`, and `vst3-parameter-contract` on Windows.
  - Editor: `clap-smoke` and `editor-host`.
  - Library: `capture-library`.
  - IRs: `ir-convolution` and `wav-decoder`.
  - Resampling: `resampling`.
  - Scripts and docs: the Python tests.
- Tests and tools that load the plugin isolate `NAM_GRID_HOME`. `NAM_GRID_HEADLESS_SMOKE=1` runs `nam-clap-smoke` without the editor.
- `clap-smoke` and `editor-host` can fail from mouse or focus activity. Re-run the failing test alone once; if it passes, report a likely interference flake.
- A regression test must fail on the old code (check once by reverting the fix) and assert the property that matters.
- DSP changes trade off sound, CPU, latency, allocation, clicks, compatibility and settled bit-identity.
  - Keep the previous plugin binary in `tmp/<task>/`.
  - Measure once, narrowed to the affected modules: `scripts/fx-performance.py --compare` for CPU, `scripts/verify-fx-processing.py --before/--after` for exactness, and the [aliasing audit](docs/dsp-aliasing.md).
  - Report what each showed, including costs that went up.
- Report which checks actually ran.

## Dependencies

- Submodules are pinned upstream releases. Local changes to them live only in `patches/`, applied by setup; never commit edits inside a submodule.
- When moving a pin or changing a patch, follow [upstream pins and patches](docs/build.md#upstream-pins-and-patches) and keep its table current. The plugin shell derives from NeuralAmpModelerPlugin; check its releases for fixes worth porting.

## Worktrees and artifacts

- Reuse the current checkout unless isolation is needed. Otherwise use the environment's managed worktree location, or `../nam-grid-worktrees/<task>/`. Never nest worktrees inside this checkout, `tmp/` or a submodule.
- Keep each task's artifacts in gitignored `tmp/<task>/` (`screenshots/`, `logs/`, `reports/`). Build outputs belong in `build/`.
- Remove only artifacts and worktrees created for the current task; never force-remove a dirty worktree.
- Only deliberately selected documentation images belong in `docs/screenshots/`; see [the gallery guide](docs/screenshots/README.md).

## Documentation, changelog and releases

- Maintained docs live in `docs/` with lowercase kebab-case filenames. List new guides in [docs/README.md](docs/README.md).
- Guides describe current behavior: avoid copied counts, local paths and historical results. Update links when moving or removing docs.
- Reviews, audits and benchmark results go in `tmp/` or the PR, not in commits. Fold lasting conclusions into the relevant guide.
- Keep DSP source attribution beside the implementation and in `ThirdPartyNotices.txt`.
- Every PR with user-facing changes adds a `CHANGELOG.md` entry under `[Unreleased]`, written for players. Call out audible changes to existing rigs.
- Releases go through a `release/v<version>` PR to `main` that:
  - bumps `PLUG_VERSION_*` in `NeuralAmpModeler/config.h` and both `NeuralAmpModeler/resources/*-Info.plist`;
  - dates and groups the changelog section;
  - rewrites `docs/release-notes.md`.
- After the merge, tag the release and build it as described in [CI and releases](docs/build.md#ci-and-releases). Work through the release checklist before publishing the draft.
