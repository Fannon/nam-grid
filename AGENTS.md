# Project guidance

## Project and parameter compatibility is required

- Released projects, host automation and `.ngrid` presets must keep loading and retain their parameter meaning. During 0.x, deliberate DSP quality or performance improvements may change the sound of existing rigs; document the audible change and verify it with relevant DSP tests and measurements. Read [the compatibility contract](docs/development.md#parameter-and-project-compatibility) before changing parameters, modules, DSP modes or saved state.
- Preserve plugin identity, parameter IDs, descriptor names/storage keys, ranges, scaling, defaults and meaning. Append new parameter, module and algorithm IDs; never reorder, renumber or reuse released IDs. Use presentation labels for UI wording changes.
- Extend state schemas additively, supply declared defaults for older states, and never lower or reuse schema numbers. From 1.0 onward, material sound changes need a new mode identity so existing rigs retain their behavior. During 0.x, a new identity is optional for deliberate quality or performance improvements; keep existing parameter and mode identities, ranges and meanings intact.
- Built-in effects must keep zero added reported latency: no lookahead or fixed compensation delay. Causal minimum-phase filtering is allowed; evaluate its group delay and phase interaction with dry/wet and parallel paths. NAM model resampling may still require compensation.
- Prioritize predictable processing for settled rigs. Modulation assignment, module insertion and mode changes may incur a short reconfiguration interruption during sound design; keep preparation/allocation off the steady audio path. Small sonic differences may be accepted for a measured performance benefit when documented and tested.
- Run the relevant DSP/state tests and parameter-contract tests (including the VST3 contract on Windows). Never re-record a compatibility fixture to make a failing change pass.

## Build and verification

- Use `bash build.sh` by default in Git Bash, WSL2, macOS and Linux. Use `bash build.sh --setup` after dependency pins or patches change, and `bash build.sh --test` for the standard CTest and Python suites. See [build.md](docs/build.md) for prerequisites and focused checks.
- Reuse `build/<platform>/release/` (`windows`, `macos` or `linux`); plugins are in its `out/` directory. SDK caches live in `build/deps/`. Keep special configurations in `build/<platform>/<purpose>/` (for example `asan`), and use direct CMake commands only when the shortcut does not cover the task. Do not add more root-level `build-*` directories or machine-specific build wrappers.
- Git Bash builds Windows plugins; WSL2 builds Linux plugins. Do not share a CMake build directory between operating systems, compilers or worktrees. Existing legacy `build-*` directories are disposable caches, not templates for new paths; do not move CMake caches or remove artifacts still in use.
- Run checks appropriate to the change and report what actually ran. Tests/tools that load the plugin must isolate `NAM_GRID_HOME` rather than touching the user's settings or library.

## Testing efficiently

The full suite is slow and partly drives the native editor, so test in layers. The CTest coverage table is in [build.md](docs/build.md#tests).

- **While iterating:** build only the target you need, e.g. `cmake --build build/<platform>/release --target nam-stage-tests`, and run that test with `ctest --test-dir build/<platform>/release -C Release -R '^stages$' --output-on-failure`. Running a test executable directly also works; run it from the build directory, because some tests write fixtures into the current directory. After a failure, re-run only that target until it passes.
- **Pick tests by what changed:**
  - Module DSP or `NAMStage`: `stages`, which includes the module tests and the click/pop sweep, plus `aliasing` for nonlinear or oversampled paths.
  - Parameters, descriptors, modes or saved state: `parameter-contract`, `vst3-parameter-contract` on Windows, and `clap-smoke`.
  - Editor and UI: `clap-smoke` and `editor-host`.
  - Library and browser: `capture-library`.
  - IR loading or processing: `ir-convolution` and `wav-decoder`.
  - Resampling: `resampling`.
  - Scripts: the Python tests.
- **Headless first:** `NAM_GRID_HEADLESS_SMOKE=1` runs `nam-clap-smoke` without the editor for DSP/state/automation checks; run the complete smoke test only when editor behavior is involved.
- **Full suite once per change:** run `bash build.sh --test` when the change is complete, not after each edit.
- **Flaky editor tests:** `clap-smoke` and `editor-host` can fail from mouse or focus activity on a machine that is in use. Re-run the failing test alone once; if it passes, report it as a likely interference flake rather than changing code.
- **New tests must catch the bug:** a regression test should fail on the old code. Check this once, by temporarily reverting the fix, and assert the property that matters (e.g. the unwanted sideband, not just "louder than dry").
- **Weigh the trade-offs together, not just sound quality:** a DSP change can affect sound, CPU cost, latency, allocation, clicks, compatibility and settled-state bit identity. Keep a copy of the previous plugin binary in `tmp/<task>/`.
  - Use the on-demand tools that answer the question at hand: `scripts/fx-performance.py --module <name> --compare` for CPU, `scripts/verify-fx-processing.py --before/--after` (optionally with `--module`/`--filter`) for exactness, and the extended [aliasing audit](docs/dsp-aliasing.md).
  - These tools take minutes to tens of minutes, so narrow them to the affected modules and run them once, not per edit.
  - Report what each measurement showed, including costs that went up.

## Worktrees and generated artifacts

- Reuse the current checkout unless isolation is needed. Use the environment's managed worktree location when provided; otherwise put worktrees beside the repository under `../nam-grid-worktrees/<task>/`. Never nest worktrees inside this checkout, its `tmp/` directory or a submodule.
- Keep all temporary task artifacts in gitignored `tmp/<task>/`, with `screenshots/`, `logs/` and `reports/` subdirectories as needed. Reuse one task directory instead of scattering timestamped folders across the root. Build intermediates and binaries belong in `build/`, not `tmp/`.
- Capture review screenshots into `tmp/<task>/screenshots/`. Only deliberately selected, maintained documentation images belong in `docs/screenshots/`; see [the gallery guide](docs/screenshots/README.md).
- Keep each worktree's build and temporary artifacts local to that worktree. Remove only artifacts and worktrees created for the current task when no longer needed; never clean up another task's files or force-remove a dirty worktree.

## Documentation and releases

- Keep maintained documentation for users and contributors in `docs/`. Use lowercase kebab-case filenames; retain conventional names such as `README.md`, `CONTRIBUTING.md` and `AGENTS.md`.
- Put reviews, audits, benchmark results and development reports in gitignored `tmp/`, or in the relevant PR description/comments. Do not commit report files or maintain a documentation archive.
- Fold useful, lasting conclusions into the appropriate guide. Preserve DSP source attribution and licenses beside the implementation and in third-party notices.
- Keep guides focused on current behavior; avoid copied schema counts, local paths and historical test results. Update links when moving or removing docs.
- Maintain `CHANGELOG.md` (newest first). Every release updates it; every PR with user-facing changes adds an entry under `[Unreleased]`.
- Prepare releases on `release/v<version>` branches (for example, `release/v0.2.1`) and submit release changes through a PR to `main`.
