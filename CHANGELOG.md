# Changelog

All notable user-facing changes to NAM Grid are documented here, newest first. See [GitHub Releases](https://github.com/Fannon/nam-grid/releases) for downloads.

## [Unreleased]

### Added

- Pedal Drive: ten guitar and bass drive styles, with antialiasing and a local Low keep dial for clean bass fundamentals.
- A centered Pedal Drive character dial with a label and function for every style, including Blues OD, Transparent OD and Vintage Fuzz.
- Guitar drive, clean boost and split-band bass example rigs, included on fresh installs and available from Library & settings.
- Documentation: a Getting started guide (install, DAW setup, first rig) and a Troubleshooting & FAQ page on the [manual site](https://fannon.github.io/nam-grid/).

### Changed

- Module Enabled is now a circle in the parameter header, matching the grid node and freeing all eight dial positions.
- Documentation: clearer site navigation, module controls in tables, and player-focused deep dives for Noise Gate (Smart), LoFi Tape Machine and Cosmic Echo.

## [0.1.1] - 2026-10-24

### Added

- NAM Grid logo in the plugin header.
- Library & settings links to the documentation, the source code and the issue tracker.

### Changed

- Tidier header with a wider rig preset name; Modulation now sits next to Tuner.
- Modules with only a few controls keep them together instead of spreading them across both rows.

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
