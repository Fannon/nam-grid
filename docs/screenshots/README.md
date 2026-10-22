# README screenshots

Captured on Windows x64 from `main` commit `17f6217` on 2026-10-22,
after rebuilding the Release CLAP plugin in `build-grid/out`.

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
