# NAM Grid 0.3.0

New ambient sounds, longer IRs and more control over your rig, with cleaner drive effects and smoother processing under load. Existing projects, presets and automation keep loading; some drive effects have deliberate sound-quality changes described below.

## New sounds and controls

- **Four new Cosmic Echo modes:** Aurora Hall for guitar halls and long washes, Photon Plate for dense plate ambience, Pulsar Echo for rhythmic stereo patterns, and Binary Bloom for coupled ambient spaces. Aurora Hall is the new-module default; saved rigs keep their settings.
- **Long IRs up to 30 seconds** for convolution reverb and sound design, with wet Input, Mix, Pre-delay, Low/High cut and a tail-envelope display. Existing cabinet IR settings remain intact.
- **Expanded module controls:** more precise gate and compressor behavior, EQ and boost tone shaping, modulation width and character, delay/reverb ducking, Freeze fades, Stutter Hold, Auto Swell shaping, and Utility channel alignment. New controls retain the previous behavior when opening older rigs.
- **LoFi Tape Machine Drive** adds oversampled Spiral saturation. Leave Drive at zero for the previous sound.

## Cleaner sound and steadier processing

- **Less aliasing** in Distortion, Tube Boost, Airwindows Tape, Console Colour and Pedal Drive's Rodent voice, with zero added reported latency. Existing rigs can sound different: attacks, high-frequency phase and partial-Mix blends may change, and Tape's low-end head bump shifts when oversampled. Mix 0% remains exact dry after its short transition fade.
- **Fewer Long IR CPU spikes:** late-tail work is spread across callbacks without changing the response or adding latency. Small buffers can use slightly more average CPU in exchange for steadier processing.
- **Lower overhead throughout the rig:** settled controls, EQ, Boost, Utility, gates, compressors and Frequency Shifter do less repeated work. Closing the editor suspends meter and spectrum collection while audio and tuner processing continue.
- **More efficient modulated rigs:** fewer tiny NAM processing calls and less callback overhead for macro-only control. Fast modulation or control transitions can differ slightly; parameter meanings and smoothing times remain intact.

## Easier editing

- Larger, clearer module graphs and hover explanations, a Cosmic Echo delay-network view, and more readable compressor/limiter gain-reduction displays.
- More reliable keyboard focus for Windows value entry and browser searches; editing values no longer triggers DAW shortcuts.

See the [full changelog](https://github.com/Fannon/nam-grid/blob/v0.3.0/CHANGELOG.md#030---2026-10-27) for controls, oversampling rates and sound-change details.

## Downloads

Each archive includes **CLAP and VST3** plus third-party license notices:

- **Windows x64:** `nam-grid-windows-x64.zip`.
- **Apple Silicon / macOS 11+:** `nam-grid-macos-arm64.zip`. Bundles are ad-hoc signed, without Developer ID signing or notarization; see the [macOS installation steps](https://fannon.github.io/nam-grid/getting-started#macos-allow-the-plugin).
- **Linux x64 / glibc 2.35+:** `nam-grid-linux-x64.zip` (Ubuntu 22.04 or newer). Linux has not yet been tested in a real DAW; feedback is welcome.

Replace the existing plugin bundles and rescan your DAW. There is no Audio Unit, AAX or standalone app in this release. Please keep `ThirdPartyNotices.txt` and `notices/` with the plugins when redistributing them.

Report issues with your OS, DAW, plugin format, sample rate and buffer size on [GitHub](https://github.com/Fannon/nam-grid/issues) or the [KVR Audio discussion](https://www.kvraudio.com/forum/viewtopic.php?t=634501).
