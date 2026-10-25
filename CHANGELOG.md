# Changelog

All notable user-facing changes to NAM Grid are documented here, newest first. See [GitHub Releases](https://github.com/Fannon/nam-grid/releases) for downloads.

## [Unreleased]

### Changed

- Documentation: full-width navigation on every page, a linked Module guide section, and a cleaner home page without the duplicate quick-start strip.
- Documentation: link the KVR Audio discussion thread from the README and troubleshooting page.

### Fixed

- Load projects and presets saved by 0.2.0 preview builds. State schema 6 now separates the Parametric EQ schema from the second modulation bank, so those files receive the new bank's defaults instead of failing validation.
- Keep the saved library index when Startup rescan is off instead of replacing it with an empty index.

## [0.2.0] - 2026-10-25

### Added

- Two independent LFOs, envelope followers and eight-step sequencers, with numbered source selectors and the same four macros. Existing projects, presets and automation keep their original parameter identities.
- Pedal Drive: ten guitar and bass drive styles, with antialiasing and a local Low keep dial for clean bass fundamentals.
- A centered Pedal Drive character dial with a label and function for every style, including Blues OD, Transparent OD and Vintage Fuzz.
- Guitar drive, clean boost and split-band bass example rigs, included on fresh installs and available from Settings.
- Documentation: a Getting started guide (install, DAW setup, first rig) and a Troubleshooting & FAQ page on the [manual site](https://fannon.github.io/nam-grid/).

### Changed

- Pedal Drive's display shows the selected style shaping a test tone at the current Drive and Character, at a loud and a quiet input, plus a live Body / High cut / Low keep curve, instead of a static signal-flow sketch.
- Module Enabled is now a circle in the parameter header, matching the grid node and freeing all eight dial positions.
- Clearer editor header with icons for Settings, Tuner and Modulation, compact New/Save buttons, a wider preset selector, matching outer margins, and consistent page headers. Library & settings is now Settings.
- A subtle background and divider separate the grid from module parameters, the tuner and routing controls without adding margins.
- Wider, black visualization panels keep the parameter area to two background shades and match the top and right spacing.
- The Routing page drops the list of modules in each lane; the crossover display fills that space instead.
- Settings colours each library folder kind like the module badges, in the list and on the Add folder buttons: captures orange, IRs blue, rig presets the plugin accent.
- Module category badges and bypass now follow the slot ID at its measured width, with browse and previous/next aligned on the right. The description row keeps a short guide to the module, with its options on the right.
- Module input and output levels with their gain moved from below the dials into the visualization footer, sharing the chart's margins.
- Pedal previews reuse unchanged curves between meter updates, and module peak ticks continue to decay when playback stops.
- Four macro knobs appear below IN, with a Settings option to hide them. Unassigned macros are dimmed; double-click a macro label to rename it in the rig preset. Wider module displays give the IN/OUT footer meters more room.
- Parametric EQ adds independent Low Q and High Q, plus a low-shelf option with its own gain. Its curve reflects the resonant cuts and shelf; existing presets keep their original cut response.
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
