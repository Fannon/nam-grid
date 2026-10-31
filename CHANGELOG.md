# Changelog

All notable user-facing changes to NAM Grid are documented here, newest first. See [GitHub Releases](https://github.com/Fannon/nam-grid/releases) for downloads.

## [Unreleased]

- **Auto-wah:** a new **Auto** button in the Wah module header assigns Env 1 to Position at +25%, so playing opens the wah without setting up a modulation row by hand. Adjust the envelope on the Modulation page, or remove the assignment there.
- **Steep frequency split:** the Routing page's **Slope** choice adds a 24 dB/octave Linkwitz-Riley lane split next to the released 6 dB/octave one, so a driven lane B receives far less low end. Existing rigs keep the gentle split, which sounds exactly as before. The steep split keeps a flat overall level, but its phase turns a full circle around the crossover, so it combines differently with a capture in the other lane; switching slopes crossfades.

- Saved rigs and DAW projects now store scenes more compactly. Scene recall stays fixed to the values you stored, and older presets and projects still load.

- Delete or Backspace clears the selected module while the grid is open. Escape closes the preset browser even while searching and cancels an audition; Settings still saves changes immediately and Escape closes it. Module preset naming has a larger, labelled field with save/cancel hints.

- **Library stars:** mark modules, NAM captures, IR files and rig presets with the star left of their name in the browser. Each click cycles yellow, orange and red, then back to an empty outline. Turn on **Starred** above the results to list only starred items. Captures and IRs keep their star when moved or renamed, and identical copies share it. Stars are saved in `favorites.json` in your preset folder, so they travel with it to another computer, and they never change how anything sounds.
- Add Pitch Shift for fixed-interval harmony, octave layers and fine detuning, with four starting presets and a live spectrum. The dry attack stays immediate; the shifted voice has short grain delay and can smear or warble on chords and large shifts.

- **Signal Monitor:** click the input/output meters or the small wire taps between modules to inspect that point in the rig without using a module slot. The on-demand panel shows stereo spectrum and resonance markers, peak/RMS meters, level history, headroom and correlation. Select a module or the tuner, press Escape, or click Close to return. The unreleased monitor module has been removed; saved development rigs containing it are no longer supported.

- **Graphic EQ:** A new EQ / Filter module with seven fixed bands and Level, like a stompbox graphic EQ. Voicing sets the band frequencies for guitar, bass or full range, and the knob labels follow it. Presets: Lead mid boost, Tight into amp, Bass clarity and Acoustic air.
- **Multiband Dynamics:** A new Dynamics module with three bands of upward and downward compression and a Depth blend: from a dense "over the top" sound to a regular multiband compressor that evens out boomy lows or spiky highs. Drag the display to move the band splits and Threshold. Presets: Over the top, Gentle glue, Fast and tight and Clean sparkle.
- **Harmonic Tremolo:** A new Modulation module that pulses bass and treble in turn, the swirling tremolo of early-1960s brown-panel amps. Crossover sets where the bands split; Tempo sync and Stereo phase work as in Auto Tremolo. Presets: Brown-panel amp, Slow swirl, Synced throb and Subtle shimmer.

- **Transient Shaper:** A new Dynamics module that shapes the start and the decay of each note separately: sharpen or soften the pick, or shorten or lengthen the ring-out. It follows changes in level rather than a threshold, so the same settings work on a DI, after an amp or on bass. Presets: Defined picking, Soft acoustic, Tight palm mutes and Gentle note sustain.
- **Dynamic EQ:** A new EQ / Filter module that cuts one band only while it is loud: tame palm-mute boom, harsh pick attacks or bass boom without thinning out the rest of your playing. The display shows the cut as it happens. Presets: Palm-mute control (after amp), Tight into amp, Harsh pick tamer (after amp) and Bass boom control.
- More useful delay starting points: quiet ducked support, brighter dotted analog and tape echoes, sparse stereo tape heads, short reverse texture, tight stereo doubling and gentle pad echoes. Existing rigs and insertion defaults keep their settings.

- More module presets where a module had few or none: a telephone band (Cut EQ), bass envelope filter, mid push boost, hot tube preamp, sputtering gated fuzz, deep watery chorus, Juno mode II and fast vintage shimmer, parked metallic flanger comb, resonant vintage phaser, stereo auto-pan, delayed lead vibrato, fast Uni-Vibe churn, two-octave blend, 12-bit sampler and a long ambient swell. Frequency Shifter gains its first presets: Barberpole sweep and Spiraling echoes. Existing rigs and insertion defaults keep their settings.

- More factory presets across the reverbs, with a focus on Cosmic Echo: dense modulated spaces, dark washes, clear repeats, swells and a Photon Plate lead sound. Practical choices include a warm rhythm plate, small recital hall, light moving cloud, short spring splash and gentle shimmer. Existing rigs and insertion defaults keep their settings.

## [0.4.0] - 2026-10-31

Existing projects, presets and automation keep loading. New modes and routing options retain the previous settings in existing rigs. Some sound corrections are deliberate changes, listed below.

### New modules and sounds

- **Pedal Comp:** a guitar sustain compressor calibrated against a Ross-type pedal, for electric and acoustic guitar. Sustain evens out your playing near bypass loudness; Attack, Body / Pick, Pickup, Blend and Level fine-tune the response. The Acoustic pickup setting suits piezo pickups and strumming. It adds no latency.
- **Spring Reverb — Dispersive tank:** a new spring character with a chirp and drip on picked notes. Drip sets its strength. It uses about four times the CPU of Classic; existing rigs keep Classic.
- **Chorus — 3 voices:** a lusher chorus with three moving delays at the same overall level. Existing rigs keep 1 voice.
- **Juno Chorus — I+II vintage:** a fast, shallow mode with centered wet sound, based on Juno-60 measurements. Existing modes retain their sound.
- **Digital Delay — Time change: Crossfade:** switches delay times, including tempo-sync jumps, without a pitch chirp. Gentle time modulation still glides. New and existing rigs start on Glide.
- **Newer NAM captures:** support for Sequential models (several models chained in one file) and WaveNets with a dilated head, through NAM Core 0.6.0.

### Rig building and performance controls

- **Module presets:** a Presets menu for every built-in effect, with established starting points such as slapback, dotted-eighth and ping-pong delays, a Screamer boost, surf spring, octave-down shimmer and Airwindows plates, halls and tape. Save your own with Save current as... and reuse them in any slot. Enter a name to save into the module's configured preset folder; the preset appears in the menu immediately. Choosing one changes only that module. Gates, Utility and modules whose defaults already provide their usual sound offer user presets only; NAM and Cabinet IR keep their file browser.
- **Smart Gate presets** also save the learned noise profile, so you can reuse a calibrated gate across rigs. Saving an untrained gate remembers that state and clears the profile when recalled. Older presets retain the current profile. Recall always leaves Learn off, and preset checkmarks distinguish different learned profiles.
- **Trails:** this Routing option lets bypassed delays and reverbs ring out, including during scene changes, while new playing passes through dry. It starts off in new and existing rigs. Tails can last up to 30 seconds for delays and Shimmer, 45 seconds for Room, Dense Plate, Cloud, Hall and Spring, or 60 seconds for Airwindows reverbs and Cosmic Echo. A ringing effect continues to use CPU until its tail ends.
- **Keep dry level:** this Routing option keeps your dry guitar at unity in delays and reverbs while Mix sets the wet level. It starts off, preserving existing rigs. Turning it on can raise the total level at high Mix; the module's Output still trims both dry and wet.
- **Retrigger: Song position:** LFOs and sequencers can follow the DAW's beat position, keeping synced sweeps and steps aligned on every playback, through tempo changes and loops. They run free when stopped. Existing rigs keep their Retrigger setting.
- **Stereo IRs:** Cabinet IR and Long IR load stereo WAV responses for stereo rooms and reverbs, even after a mono capture. Existing mono IRs retain their sound. Four-channel true stereo files are not supported.

### Sound changes in existing rigs

These corrections can change the sound of saved rigs. Controls, ranges and reported latency stay the same.

- **Butter Comp** now responds at guitar levels and compresses more at the same setting; previously it barely reacted below about 70%. The default gently glues typical notes by about 1 dB, and 100% by 5–8 dB, while staying near bypass loudness.
- **Frequency Shifter** now produces the selected sideband correctly. At ±100% Sideband the shifted sound is clearer and about 3 dB louder, and high Feedback sustains longer. Sideband 0 keeps its ring-modulator sound.
- **Cloud** decay times now match their settings: short tails are shorter, and long tails sustain longer and slightly brighter. The mids of long tails ring 20–30% longer than before, as the editor's decay display shows.
- **Dense Plate** has shorter, less dense tails at short Decay settings, particularly at large Size. Reflection levels can also change.
- **Shimmer** pitch and Freeze transitions can sound slightly slower. Settled pitch and the mono pitch-shifter path are unchanged.
- **NAM captures using FiLM conditioning** now sound as trained. Affected captures can sound different; tested standard WaveNet, A2, LSTM and bundled captures retain their output at 48 kHz. Some resampled captures at 44.1 kHz differ below −110 dBFS.

### Lower CPU and smaller plugins

- **Smooth and Punch compressors** use less CPU. Their gain curves, smoothing and latency stay the same; existing rigs can differ by floating-point rounding.
- **Multi-head Tape** uses about 30% less CPU while both channels carry the same signal, such as mono guitar through captures. Sound is unchanged.
- **Uni-Vibe** uses about 25% less CPU without added latency. Moving sweeps can differ very slightly; the static tone, controls and saved settings stay the same.
- **Modulated rigs** use less CPU, with unchanged sound, modulation timing and saved settings.
- **Smaller installed plugins:** factory content is compressed inside both plugin formats on every platform and extracted when missing. Existing library files and audio processing are unchanged.

### Smoother operation and fixes

- Windows library-index, settings and preset saves tolerate brief file-reader contention instead of failing immediately. Persistent locks still fail safely, preserving the previous file.
- Saved module presets load correctly when continuous controls contain fractional values, including delay and Cosmic Echo times. Existing affected files load without changes.
- Long filenames, metadata and paths stay inside the library browser's details panel without overlapping its controls.
- Fewer clicks when bypassing, re-enabling or replacing modules and switching scenes. Re-enabling delays no longer cuts feedback abruptly, and changing Cosmic Echo's style no longer clicks on the first repeats.
- Smoother Output and Drive automation across drive, tape, delay and reverb modules. Utility level, balance and polarity changes and Juno Chorus Mix near zero also transition smoothly. Settled sound is unchanged.
- Smoother automation of Outboard Channel Saturate and Console EQ Mid/High Cut, with unchanged settled sound and latency.
- Re-enabled compressors, gates, Peak Limiter, Auto Swell and Filter no longer resume stale dynamics or skip a swell.
- With Startup rescan off, the library browser restores its saved index. Empty or invalid indexes trigger a scan; unavailable folders and read failures retain the last good index.
- Projects and rig presets can hold up to 8 MiB, allowing complete rigs with all three performance scenes.

### Editing

- Hover over the module name for its full description. The row below has more room for options, without shortened description fragments.
- Switches and choice boxes, such as Rotary's Fast and Smart Gate's Learn, align with the knob dials.
- The module picker's Delay > Slapback preset item has moved: insert Digital Delay and choose Slapback from its Presets menu for the same settings.

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
