# NAM Grid 0.4.0

A guitar sustain compressor, reusable effect presets, delay and reverb tails that ring out after bypass, and new chorus, spring and delay voices. Existing projects, presets and automation keep loading; a few reverbs, Butter Comp and Frequency Shifter deliberately sound different, as described below.

## New sounds

- **Pedal Comp:** a guitar sustain compressor calibrated against a Ross-type pedal. One **Sustain** knob evens out your playing at bypass loudness; **Attack**, **Body / Pick**, **Pickup** (including Acoustic for piezo pickups), **Blend** and **Level** fine-tune it.
- **Dispersive spring:** Spring Reverb's new **Tank** setting models a real spring's chirp for drip on picked notes.
- **Lusher modulation:** Chorus adds **3 voices**, and Juno Chorus adds a fast, shallow **I+II vintage** mode based on Juno-60 measurements.
- **Clean delay time changes:** Digital Delay's **Time change: Crossfade** switches times, including tempo-sync jumps, without a pitch chirp.
- **Newer NAM captures:** NAM Core 0.6.0 adds support for Sequential models and WaveNets with a dilated head.

## Faster rig building and performance

- **Module presets:** effect editors have a **Presets** menu with established starting points, such as slapback, dotted-eighth and ping-pong delays, a Screamer boost, surf spring and Airwindows plates, halls and tape. Save your own with **Save current as...**; choosing one changes only that module.
- **Reusable Smart Gate calibration:** user module presets save the learned noise profile along with the controls. Older presets retain the current profile; recalling a preset leaves Learn off.
- **Trails:** with this Routing option on, bypassed delays and reverbs ring out instead of cutting off, so scene changes keep their ambience.
- **Keep dry level:** this Routing option keeps your dry guitar at unity in all delays and reverbs while Mix sets only the wet level.
- **Song position:** LFOs and sequencers can lock to your DAW's playback position, so synced sweeps and steps land on the same beat on every playback.
- **Stereo IRs:** Cabinet IR and Long IR load stereo WAV files for stereo rooms and reverbs, even after a mono capture.

Trails and Keep dry level start off, so existing rigs keep their sound.

Hover over the module name to read its description. The row below now has more room for the module's options.

## Sound changes in existing rigs

These corrections change how some saved rigs sound. Parameters, ranges and zero reported latency stay the same.

- **Butter Comp** now works at guitar levels and compresses more at the same setting; previously it barely reacted below about 70%.
- **Frequency Shifter** now shifts a single sideband as documented. At ±100% Sideband it sounds clearer and about 3 dB louder; Sideband 0 keeps its ring-modulator sound.
- **Cloud** and **Dense Plate** decay times now match their settings: short tails are shorter, and long Cloud tails sustain longer and slightly brighter.
- **Shimmer** pitch and Freeze transitions can sound slightly slower; settled pitch is unchanged.
- **NAM captures using FiLM conditioning** now sound as trained. Affected captures can sound different. Tested standard captures retain their 48 kHz output; some resampled captures differ below −110 dBFS.

## Smoother and lighter

- **Fewer clicks** when re-enabling or replacing modules, re-enabling delays, switching scenes and automating Output and Drive. Settled sound is unchanged.
- **Lower CPU** in Smooth and Punch compressors, Multi-head Tape (about 30% for mono guitar), Uni-Vibe (about 25%) and modulated rigs.
- **Smaller installed plugins:** factory content is compressed inside both plugin formats and extracted when missing, without changing existing library files or audio processing.
- Re-enabled dynamics no longer resume a stale gain reduction, and the library browser restores its saved index again when Startup rescan is off.

See the [full changelog](https://github.com/Fannon/nam-grid/blob/v0.4.0/CHANGELOG.md#040---2026-10-30) for module details and measurements.

## Downloads

Each archive includes **CLAP and VST3** plus third-party license notices:

- **Windows x64:** `nam-grid-windows-x64.zip`.
- **Apple Silicon / macOS 11+:** `nam-grid-macos-arm64.zip`. Bundles are ad-hoc signed, without Developer ID signing or notarization; see the [macOS installation steps](https://fannon.github.io/nam-grid/getting-started#macos-allow-the-plugin).
- **Linux x64 / glibc 2.35+:** `nam-grid-linux-x64.zip` (Ubuntu 22.04 or newer). Linux has not yet been tested in a real DAW; feedback is welcome.

Replace the existing plugin bundles and rescan your DAW. There is no Audio Unit, AAX or standalone app in this release. Please keep `ThirdPartyNotices.txt` and `notices/` with the plugins when redistributing them.

Report issues with your OS, DAW, plugin format, sample rate and buffer size on [GitHub](https://github.com/Fannon/nam-grid/issues) or the [KVR Audio discussion](https://www.kvraudio.com/forum/viewtopic.php?t=634501).
