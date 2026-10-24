# Changelog

All notable user-facing changes to NAM Grid are documented here, newest first. See [GitHub Releases](https://github.com/Fannon/nam-grid/releases) for downloads.

## [Unreleased]

## [0.1.1] - 2026-10-24

### Changed

- Tidier header: every box has the same height and outline, the rig preset name has more room, and Modulation now sits next to Tuner.
- Modules with only a few controls keep them together, filled from the left, instead of spreading them across both rows.
- The IN and OUT lines reach their knobs, passing behind the stereo meters, with a little more space between the meters and the first and last modules.

### Added

- Library & settings page links to the documentation site, the open-source repository and the issue tracker.
- Documentation website scaffold (VitePress, dark by default) presenting the guides and module pages.
- User guides rewritten from the player's point of view: routing, editor, performance,
  tuner and effect deep-dives no longer carry implementation details (headers, buffers,
  SIMD paths, test coverage). Internals now live in `development.md` and
  `module-implementation.md`, including a new audition-renders section.

## [0.1.0] - 2026-10-24

Our first release! NAM Grid is a 16-slot guitar rig in one CLAP / VST3 plugin.

### Added

- 16 slots in four rows (Pre, two parallel lanes A and B with blend, Post); any slot takes any module.
- Neural Amp Modeler captures, cabinet IRs and 54 built-in effects, plus rig presets, a library browser, tuner, macros, modulation and performance scenes.
- Bundled starter sound: one clean amp-and-cab capture (RiffSmith Clean) and two rigs, Empty and Clean Default.
- Downloads, each with CLAP and VST3: Windows x64, Apple Silicon Macs (macOS 11 or newer), and Linux x64 (Ubuntu 22.04 or newer works well).

### Notes

- If you tried an earlier preview build, please remove those old plugin files and rescan your DAW. 0.1.0 is a fresh start — rigs and projects from here on keep working in future updates.
- The Mac build isn't signed yet, so macOS will ask you to confirm it once — see the README.
- Linux is still an early preview; start with a moderate buffer size.
- No Audio Unit, AAX or standalone app in this release.
