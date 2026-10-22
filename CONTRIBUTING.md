# Contributing to nam-grid

Report bugs and propose changes in [this fork's issues](https://github.com/Fannon/nam-grid/issues).
For bugs, include the build/commit, OS, plugin format, DAW, sample rate, block size,
and reproduction steps. Include a minimal rig/model/IR example when it can be shared.

## Build and tests

Use the CMake instructions in [build.md](docs/build.md). Build both native formats
and the test targets listed there, then run:

```sh
ctest --test-dir build-grid -C Release --output-on-failure
```

The suites cover the capture library/presets, WAV decoding, resampling, DSP stages,
CLAP hosting/native editor, and tuner. Assertions remain enabled in Release tests.
Focused sanitizer options and their runtime requirements are documented in the
build guide. Real DAW listening and VST3/macOS host validation are separate
[acceptance checks](docs/release-checklist.md); describe exactly what was tested
in the PR.

## Change scope

- Keep existing plugin identity, parameter IDs and serialized names stable.
  Add migration and state-rejection coverage when changing saved data.
- Keep file I/O, allocation, DSP preparation and destruction off the audio thread.
  Extend the stage allocation checks when introducing a new processing path.
- Keep module menus, parameter ownership, defaults, DSP dispatch and tail reporting
  consistent. Test replacement, bypass and recall for new effects.
- Preserve dependency pins. Changes to patched dependencies belong in `patches/`
  and must apply through `scripts/setup-deps.py` from a clean checkout.
- Document external assets/resources and include applicable third-party notices.

Follow the surrounding C++ style and repository `.clang-format`; limit formatting
to touched code. Run `git diff --check` before submitting. A PR should explain the
user-visible change, relevant tradeoffs, tests run and remaining validation limits.

Keep maintained guides discoverable through the [documentation index](docs/README.md).
Use lowercase kebab-case guide filenames. Put reviews, audits and one-off reports
in gitignored `tmp/`, or in the relevant PR description/comments. Fold lasting
conclusions into maintained guides and update references when moving a document.
See [AGENTS.md](AGENTS.md).
