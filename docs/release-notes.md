# NAM Grid 0.7.0

Six distortion-pedal voices, four new fuzzes, an experimental automatic noise gate, and lower CPU use for driven sounds. Existing projects, presets and automation keep loading; audible changes to existing rigs are described below.

## New sounds

- **Pedal Distortion:** Classic Dist, Raw Dist, British Crunch, Hot Lead, Metal Stack and Chainsaw, with three-band EQ, sweepable mids and Tight before clipping. Five presets cover classic rock, British crunch, scooped metal, singing lead and raw garage grit.
- **Pedal Drive:** Bender Fuzz, Octave Fuzz, Gated Fuzz and Sub Fuzz add cutting rasp, octave-up leads, sputtering decay and octave-down synth textures.
- **Distortion:** Overdrive, Half-wave and Sine fold join the existing styles.
- **Noise Gate (AI), experimental:** automatic band-by-band hum and hiss reduction without Learn or a threshold. Adjust Sensitivity and Protect attack against quiet picking and decays; Smart Gate remains recommended for gentle note preservation. Its optional final gate starts Off.

## Sound and performance

Pedal Drive antialiasing reduces high-note fizz under hard drive. Treble Drive and Vintage Fuzz now keep their asymmetric character on loud notes. Pedal Drive and Distortion use about a quarter to a third less CPU, and Tube Boost about 11% less; partial dry/wet blends can differ by about half a decibel after the filter changes. These improvements can audibly change existing rigs without changing their settings or adding latency.

On Apple Silicon, more efficient filter sums further reduce CPU use for Tube Boost, driven LoFi Tape Machine, Console Colour and Airwindows Tape. Output differs only by floating-point roundoff.

## Smart Gate

- **Protect attack** starts at 100%, including in older projects and presets, and opens note starts slightly faster. Set it to 0% for the previous response; higher settings can pass more sudden noise.
- **Tracking: Follow** adapts to a stable rise in learned noise, up to 6 dB. New Smart Gates start at Follow; older saved rigs and module presets retain Fixed. Allow a few seconds of idle noise, and Learn again if its character changes.
- Brief clicks and accidental plucks have less influence during Learn. Existing saved profiles are unchanged.
- All four gates show measured attenuation in their history. Gates that fully silence their output show **closed**.

## Windows typing fix

Search and name fields work after Alt+Tab back to the plugin in Bitwig. NAM Grid takes the keys its fields need before the host can drop them; unused keys still reach host shortcuts.

## Platforms and compatibility

Windows x64, macOS Apple Silicon and Linux x64 archives contain CLAP and VST3. macOS bundles are ad-hoc signed, not notarized. Linux uses X11/XWayland and has not been tested in a real DAW.

NAM captures and cabinet IRs outside the bundled examples remain external files: keep them available when moving rigs between computers. Projects and presets from before the 0.1.0 compatibility baseline are not supported.

See the [changelog](https://github.com/Fannon/nam-grid/blob/v0.7.0/CHANGELOG.md) for all changes and the [manual](https://fannon.github.io/nam-grid/) for controls and installation.
