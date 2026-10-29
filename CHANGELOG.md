# Changelog

All notable user-facing changes to NAM Grid are documented here, newest first. See [GitHub Releases](https://github.com/Fannon/nam-grid/releases) for downloads.

## [Unreleased]

### Performance

- Modulated rigs use less CPU by accessing destination controls directly. Sound, modulation timing and saved rigs are unchanged.

### Added

- Digital Delay adds **Time change: Glide / Crossfade**. Crossfade switches delay times with a 30 ms blend instead of a pitch chirp, including tempo-sync jumps. Gentle time modulation still glides. New and existing rigs start on Glide and retain their sound.
- Juno Chorus adds **I+II vintage**: fast, shallow 9.75 Hz motion with centered wet sound, based on Juno-60 measurements. Existing I, I+II and II modes retain their sound.
- Cabinet IR and Long IR now load stereo WAV responses, with independent left/right convolution. After a mono capture, a stereo IR spreads the signal into left and right for stereo room and reverb responses. Existing mono IRs retain their sound; four-channel true stereo files are not supported.
- Routing now has **Trails**: bypassed delays and reverbs ring out instead of cutting their repeats and tail, so a scene that switches them off keeps its ambience. New playing stops reaching the bypassed module after the usual 5 ms fade. It starts off in new and existing rigs, preserving their sound. Tails last up to 30 s for delays and Shimmer, 45 s for Room, Dense Plate, Cloud, Hall and Spring, or 60 s for Airwindows reverbs and Cosmic Echo, including the final fade. A ringing module uses CPU, then drains silently as before.
- Routing now has **Keep dry level** for all delays and reverbs: keep your dry guitar at unity while Mix sets the wet level. It starts off in new and existing rigs, preserving their sound. Turning it on can raise the total level at high Mix; module Output trims still apply to both dry and wet.

### Fixed

- Frequency Shifter now shifts a single sideband as documented. Its quadrature path was silent, so Sideband at ±100% produced both sidebands like a ring modulator. A new zero-latency allpass network keeps the opposite sideband about 45 dB down from 30 Hz to 20 kHz at 44.1/48 kHz. This changes existing rigs: at ±100% Sideband the shifted sound is clearer, about 3 dB louder, and high Feedback sustains longer; Sideband 0 keeps its ring-modulator sound.
- Bypassed compressors, gates, Peak Limiter, Auto Swell and Filter let their detectors settle, so re-enabling them no longer resumes a stale gain reduction or skips a swell.
- Fewer clicks when switching modules, scenes and automation:
  - Re-enabling or replacing a module no longer clicks. This affected Chorus, Flanger, Vibrato, Rotary, Stereo Doubler, Juno Chorus, Frequency Shifter, LoFi Tape Machine, Dual Delay, Tube Boost and Airwindows Tape. The module's input now fades in, and every module briefly drains its history before it is suspended.
  - Re-enabling a delay shortly after bypassing it no longer clicks: feedback fades out over 5 ms instead of being cut.
  - Jumps of Output and Drive in Distortion, Pedal Drive, Tube Boost and Airwindows Tape, and of Output in LoFi Tape Machine, Multi-head Tape, Diffused Delay and the Airwindows reverbs, are smooth instead of zippering.
  - Utility level, balance and polarity changes always take 5 ms. Juno Chorus Mix rising from 0% no longer jumps in. Changing the Cosmic Echo style no longer clicks on the new style's first repeats.
  - Settled sound is unchanged; only these transitions differ.
- Projects and rig presets accept up to 8 MiB instead of 1 MiB, leaving room for future controls in rigs with all three performance scenes stored. Full rigs currently need about 0.9 MiB as a preset.
- Restore the saved library index when Startup rescan is off. Missing, invalid, empty or factory-only indexes automatically trigger a background rescan; opening an empty or factory-only browser retries recovery. Scans with unavailable folders or read failures preserve the last good saved index.

## [0.3.0] - 2026-10-27

### New sounds and controls

- Cosmic Echo adds four modes: Aurora Hall moves from clustered echoes to a diffuse guitar hall and long washes; Photon Plate provides quick, dense plate ambience; Pulsar Echo creates four-stage stereo echo patterns; Binary Bloom couples short and long ambient spaces. New or reset modules start with Aurora Hall; saved rigs retain their modes and settings.
- IR convolution supports reverb and sound-design responses up to 30 seconds with the new Long IR option, plus wet Input, Mix, Pre-delay and Low/High cut controls. Long IR shows the tail envelope. Existing rigs retain their cabinet length and sound.
- More control over dynamics and tone: gate attack, hold and hysteresis; compressor ratio and detector low cut; Guitar EQ mid frequency; EQ output; Boost mid gain; Tube Boost high cut; and Auto Swell curve, hold and mix.
- More ways to shape modulation: Juno wet width, Uni-Vibe spread, stereo modulation phase, selectable phaser stages and ring carriers, rotary horn/drum balance, octaver tracking filter, and doubler humanization and tone.
- More control over space and timing: delay duck recovery, reverse feedback, reverb ducking, spring pre-delay, Freeze fades, Stutter Hold, and Utility channel modes and alignment. New controls load at their legacy defaults in existing rigs.
- LoFi Tape Machine adds a Drive control using the Airwindows Spiral shaper with 4x oversampling. Drive at zero retains its previous sound.

### Sound quality

- Distortion, Tube Boost, Airwindows Tape and Console Colour reduce aliasing with minimum-phase oversampling; Distortion also adds antiderivative antialiasing. Distortion and Tube Boost run at 4x at 44.1/48 kHz and 2x at 88.2/96 kHz; Tape and Console Colour use 2x below 88.2 kHz. All retain zero added reported latency.
- These improvements can change existing rigs: attacks, high-frequency phase and partial-Mix blends may differ. The oversampled wet paths stay flat to about 18 kHz, rolling off by about 2.3 dB at 20 kHz at 48 kHz. Tape's head bump moves up at 2x: Bump at 50% adds roughly 0.5–0.8 dB at 20–160 Hz at typical levels, with larger sub-bass changes at low input levels.
- Tube Boost, Tape and Console Colour align the partial-Mix dry path with the oversampling filters to avoid deep cancellation. Crossing Mix zero uses a 20 ms fade; settled Mix 0% passes the exact dry sample. Tube Boost retains its high cut and DC blocker.
- Pedal Drive's Rodent voice reduces high-frequency foldover with antialiased input saturation and an integrated slew/clip stage at its existing 4x rate. Other pedal voices retain their processing.

### Performance and stability

- Long IR spreads late-tail processing across audio callbacks, reducing periodic CPU spikes while preserving the response, its onset and zero added latency. At small buffer sizes this can trade slightly higher average CPU use for more predictable processing.
- Lower CPU use with the editor closed by suspending meter and spectrum collection. Audio, effect histories, tuner detection and noise learning continue normally. The open editor also reuses its spectrum window and avoids unnecessary spectrum analysis for Utility's stereo scope.
- Lower processing overhead across EQ, Boost, Utility, gates, compressors and Frequency Shifter by reusing settled control calculations and simplifying filter and delay-buffer loops. Distortion and Pedal Drive gain vectorized oversampling on x64; Tape avoids duplicate mono processing. These optimizations preserve their processing quality and latency.
- Modulated rigs avoid unnecessary small-block NAM processing before the first affected input stage. Macro-only rigs batch more of each buffer while retaining their first short control update. Dynamic modulation stays near 0.33–0.36 ms across sample rates: 16 samples at 44.1/48 kHz, 32 at 88.2/96 kHz. Smoothing times are preserved, though fast modulation and control transitions can differ slightly, especially when adjusting Tape in macro-only rigs.

### Editing and displays

- Larger module graphs, clearer live captions and hover explanations make settings easier to read. Truncated module descriptions reveal their full text on hover, and editable knob values show a text cursor.
- Cosmic Echo shows its measured response and a delay-network sketch with timing, density and motion indicators. Each view has its own explanation, including the current delay-line count.
- Compressor and limiter displays use a more readable 16 dB range, with overflow indicators and full numeric readings. Outboard Channel and Butter Comp add 2 and 4 dB guides for gentle compression; gate attenuation retains its wider range.
- Windows numeric entry reliably focuses the plugin, including floating host windows. Text editing consumes key presses and releases so they do not trigger DAW shortcuts.
- Browser search retains typing focus after committing or clearing a query and switching tabs.

### For contributors

- `bash build.sh` provides first-run dependency setup, incremental builds and `--test` across Windows Git Bash, WSL2, macOS and Linux. Incremental builds also work with macOS's default Bash 3.2.
- Build guidance covers Windows WinGet and portable MSVC/Ninja setup, manual cross-platform GitHub Actions builds and macOS signing limits. Performance reports can include Long IR tails with `--ir-length long`.

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
