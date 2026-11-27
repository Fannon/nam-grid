# NAM Grid 0.8.1

This update improves preset switching, audio processing, stereo and high-rate consistency, and library responsiveness. Cosmic Echo gets eight factory presets and changes to Solstice Hall and Binary Bloom. Existing projects, presets, and automation keep loading.

## Sound changes in existing rigs

- **Solstice Hall** fills in earlier, with a stronger early wash and a longer decay at the same settings.
- **Binary Bloom** keeps its first reflections steadier as Density rises. Saved high-Density settings can sound up to 3 dB louder on the wet path. The revised **Chord bloom** preset lowers Mix to keep a similar balance.
- **Transient Shaper** engages negative Attack and Sustain cuts more tightly, matching boost attack speed.
- **Airwindows Guitar Hall** restores its right-channel feedback path, producing fuller and more balanced stereo tails at the same Sustain setting.
- **Noise Gate**, **Smooth Compressor**, **Punch Compressor**, and **Auto Swell** now stereo-link their detectors so asymmetric stereo signals hold their stereo image. Mono and centered stereo rigs are unchanged.
- **Galactic Space** and **Console Colour** scale consistently at 96 and 192 kHz so high-rate projects match 44.1/48 kHz decay, level, and sag dynamics.
- **Pitch Shift**, **Pitch Delay**, **Granular Delay**, and **Granulator** can choose or blend grains slightly differently at the same settings.

The new Cosmic Echo presets range from **Close studio room** and **Aurora studio hall** to **Bright ambient hall** and **Still cavern**.

## Presets, editor, and library

- Preset switches hold the outgoing rig's gain steady through the fade. Smart Gate preserves a newly learned profile across host resets. The Volume Pedal clears latched MIDI values on rig changes and host resets.
- Smoother parameter transitions across Juno Chorus mode switches, Pedal Drive Character automation, Tube Boost High Cut, reverb low-cut and predelay endpoints, Diffused Delay modulation, Reverse Delay, Vibrato Rise, and upward Pitch Shift jumps. Bypassed reverbs with Trails off drain and suspend within a few seconds.
- Several effects use less CPU and memory, and some rigs with Linear NAM captures or cabinet IRs prepare faster.
- Library searches and scans do less work, and Cabinet IR plots refresh when the host sample rate changes. Escape closes live preset search. Linux folder selection and editor closing are fixed.

Bring your own NAM captures and cabinet IRs. Keep any external files available when moving projects or presets.

## Platforms

Windows x64, macOS Apple Silicon and Linux x64 archives contain CLAP and VST3. macOS bundles use ad-hoc signing and require the quarantine clearance described in the manual. Linux uses X11 or XWayland. Linux real-DAW coverage remains unverified. The project compatibility baseline is 0.1.0.

See the [full changelog](https://github.com/Fannon/nam-grid/blob/v0.8.1/CHANGELOG.md) and the [manual](https://fannon.github.io/nam-grid/).
