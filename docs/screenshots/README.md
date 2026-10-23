# README screenshots

Captured on Windows x64 from the 0.1.0 release candidate on 2026-10-23,
using the Release CLAP plugin in `build-grid/out`.

- `grid.png`: an illustrative built-in-effects pedalboard, with B1 Cosmic Echo
  selected and generated audio driving the meters. “Ambient pedalboard” is the
  example's name; it is not a bundled preset.
- `browser.png`: the same instance's module browser, opened from A1 Juno Chorus.

Both images are the native editor at its original 1000 × 720 resolution,
captured directly from its OpenGL surface with
[`CaptureEditor`](../../tests/WinEditorCapture.h) and converted losslessly from
BMP to PNG. No desktop chrome, NAM captures, IRs or personal library paths appear.
For future captures, the existing native smoke-test screenshot modes are
documented in [build.md](../build.md).
