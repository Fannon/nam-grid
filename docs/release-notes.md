# NAM Grid 0.8.0

Six new delays, an experimental Granulator, a MIDI-controlled Volume Pedal and a rebuilt Shimmer Reverb. Existing projects, presets and automation keep loading. See the sound changes below before updating a rig you rely on.

## New delays and performance tools

- **Space Tape Echo** pairs three moving tape heads with spring reverb. **Bucket Brigade** covers dark, clear and driven analog repeats. **Pitch Delay** shifts and reverses echoes, **Feedback FX Delay** processes each repeat, **Pattern Delay** lets you drag four taps into place, and **Granular Delay** scatters, reverses or pitch-shifts fragments. All six support tempo sync and Trails.
- **Granulator**, experimental, replays the last four seconds as overlapping grains. Freeze a phrase, trigger grains with your pick, or build a tempo-synced texture from octaves and fifths.
- **Volume Pedal** follows an expression pedal or automation, with adjustable heel and toe levels, taper and smoothing. CLAP can receive MIDI CC directly. In VST3, map your pedal to Position through the host.
- **Digital Delay** adds Rack and Lo-fi voices and stereo modulation. **Cosmic Echo** adds Velvet Chamber, Solstice Hall and Sunken Cathedral. **Filter** gains more envelope controls, and **Ring Modulator** can sweep its carrier in sync with the host.

## Sound changes in existing rigs

- **Shimmer Reverb** recirculates pitch shifts through the tail. Existing rigs get a stronger, louder shimmer and longer audible tails at long Decay settings. Lower Amount or Mix to keep it subtle. Shimmer now uses more CPU. Density sets how distinct the octaves are, and Bloom fades the wet sound in behind each note.
- **Dense Plate** fills in earlier, and **Cloud** is about 6 dB louder at the same Mix.
- **Console EQ** now follows the labeled band gains and shelf frequencies more closely, so existing EQ curves change. **Filter** can sound louder in Band-pass mode when Resonance is above 1.
- Existing Digital Delay rigs load with Clean and modulation off. An automation lane using the formerly inactive Delay Modulation parameter now controls modulation.

## Editor and factory content

Click a slot ID on the grid or module header to bypass that module. The tuner holds a steadier reading and shows ±15 cents of history. Delay displays show repeats and tap timing, while module presets are more closely matched in level.

Factory content installs Empty and the Drive Examples. Bring your own NAM captures and cabinet IRs and keep them available when moving projects or presets. Previously installed captures and rigs stay in your library.

## Platforms

Windows x64, macOS Apple Silicon and Linux x64 archives contain CLAP and VST3. macOS bundles use ad-hoc signing and require the quarantine clearance described in the manual. Linux uses X11 or XWayland. Linux real-DAW coverage remains unverified. The project compatibility baseline is 0.1.0.

See the [full changelog](https://github.com/Fannon/nam-grid/blob/v0.8.0/CHANGELOG.md) and the [manual](https://fannon.github.io/nam-grid/).
