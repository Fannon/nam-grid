# Changelog

All notable user-facing changes to NAM Grid are documented here, newest first. See [GitHub Releases](https://github.com/Fannon/nam-grid/releases) for downloads.

## [Unreleased]

### Added

- Focused module controls: gate attack/hold/hysteresis; compressor ratio and detector low cut; Guitar EQ mid frequency; EQ output, boost mid gain and Tube Boost high cut.
- Juno wet width, Uni-Vibe spread, stereo modulation phase, selectable phaser stages and ring carriers, rotary horn/drum balance, octaver tracking filter and doubler humanization/tone.
- Delay duck recovery, reverse feedback, reverb ducking, spring pre-delay, Freeze fades, Stutter Hold, Utility channel modes/alignment and Auto Swell curve/hold/mix.
- LoFi Tape Machine Drive reuses the Airwindows Spiral shaper with 4x oversampling; zero Drive retains the existing sound. Existing rigs load all new controls at their legacy defaults.
- IR convolution adds wet Input, Mix, Pre-delay and Low/High cut controls, plus a Long IR option for reverb and sound-design responses up to 30 seconds. Existing rigs keep their original cabinet length and sound. Long IR displays the tail envelope.
- Compressor and limiter graphs mark gain reduction beyond 16 dB with a red edge or tip; their numeric readouts continue to show the full reduction.
- Cosmic Echo shows its delay-line network sketch below the measured response, with timing markers, density connections and modulation halos. Hover for the 4- or 16-line count and explanation.
- Hover over a truncated module description to read its full text.
- A single-command `bash build.sh` build for Git Bash on Windows, WSL2, macOS and Linux, with automatic first-run dependency setup, incremental builds under `build/<platform>/release/`, and `--test` to run the standard test suites.
- Windows build guidance includes WinGet installation commands and portable MSVC/Ninja support; the build help documents manual cross-platform GitHub Actions builds and macOS signing limitations.

### Changed

- Tube Boost reduces aliasing with local minimum-phase oversampling (4x at 44.1/48 kHz, 2x at 88.2/96 kHz). Airwindows Tape and Console Colour use 2x below 88.2 kHz and native processing above it. These causal wet-path filters add no reported latency but can change attack and high-frequency phase; the oversampled wet paths are flat to about 18 kHz and roll off slightly above (about -2.3 dB at 20 kHz at 48 kHz). Tape and Console Colour now blend a partial Mix inside the oversampled stage, so it no longer combs against the dry signal, and Mix 0% still passes the exact dry sample. Tube Boost keeps its host-rate high cut and DC blocker, so its partial Mix can comb above roughly 3 kHz. Tape's head bump moves up when the core runs at 2x: with Bump at 50% expect about +0.5 to +0.8 dB from 20-160 Hz at typical levels and larger sub-bass changes at low levels.
- Pedal Drive's Rodent voice antialiases its input saturation and continuously integrates its slew/clip stage, reducing high-frequency foldover without raising its existing 4x rate. Other pedal voices retain their processing. Tape's mono path avoids computing a discarded duplicate channel, offsetting part of the added antialiasing cost.
- Dynamic modulation batches independent input stages before the first affected input destination, reducing unnecessary small-block NAM inference. The remaining stages retain control-rate updates, including envelope feedback.
- Modulation control intervals now scale with host sample rate to stay near 0.33-0.36 ms (16 samples at 44.1/48 kHz, 32 at 88.2/96 kHz). Higher rates can have slightly different fast modulation and gain transitions compared with the previous fixed 16-sample interval; existing destination smoothing times are preserved.
- Macro-only modulation retains the first short control step and batches the rest of the audio buffer instead of subdividing every module into 16-sample calls, reducing callback overhead without adding latency. Dynamic sources, including Via, retain their 16-sample updates; block-dependent transitions can differ slightly for macro-only rigs, particularly when adjusting Airwindows Tape controls.
- Reduce Distortion and Pedal Drive CPU on x64 with SIMD oversampling filters, and reuse exact antiderivative/bias evaluations in Distortion. Antialiasing quality and zero added latency are preserved.
- Distortion reduces aliasing with minimum-phase oversampling and antiderivative antialiasing. It runs at 4x at 44.1/48 kHz, 2x at 88.2/96 kHz and 1x from 176.4 kHz, with zero added reported latency. Existing Distortion rigs can sound cleaner, with changes to attack and partial Mix blends.
- Cosmic Echo's response graph and delay-line sketch have separate, shorter hover explanations, including the current line count.
- Module graphs use more vertical space, with explanations available on hover and captions focused on live measurements and useful derived information instead of repeating knob values. Editable knob values show a text cursor on hover.
- Compressor and limiter gain-reduction displays use a 16 dB scale instead of 24 dB, with 2 and 4 dB guides in Outboard Channel and Butter Comp histories so gentle compression is easier to see. Gate attenuation displays retain their wider ranges.

## [0.2.1] - 2026-10-25

### Changed

- Modulation uses compact knobs and aligned selectors and switches, matching the module pane. Source menus are alphabetical and renamed macros keep their number, for example `Macro 1: Test`.
- Grid cells and the module header show the selected mode, such as the Pedal Drive style, Cosmic Echo topology or filter mode, so a rig can be read without opening each module.
- Pedal Drive adds no latency. Its antialiasing filters are now minimum phase, with the same bandwidth and alias rejection, so the pedal no longer reports 64 samples and stacking pedals does not accumulate delay.
- Pedal Drive uses about 20% less CPU on Apple Silicon, with unchanged sound. The oversampling filters now pair their independent phase sums; every other platform keeps the portable loop.
- Documentation: full-width navigation on every page, a linked Module guide section, and a cleaner home page without the duplicate quick-start strip.
- Documentation: link the KVR Audio discussion thread from the README and troubleshooting page.

### Fixed

- Focus browser search when opening Add module, a module's browser or the preset browser, so typing can begin immediately.
- Cancelling an in-progress library scan preserves the saved index and does not restart the cancelled scan.
- Correct compact Via labels for the second LFO, envelope follower and sequencer.
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
