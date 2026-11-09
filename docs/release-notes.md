# NAM Grid 0.6.0

A wider editor, clean lows under Distortion, a Smart Gate that keeps more of your pick attack, and much richer IR details in the browser. Existing projects, presets and automation keep loading. Smart Gate is the only audible change to existing rigs.

## Sound

- **Distortion Low preserve:** keep bass weight clean while adding grit above it. Set the knob around 150–300 Hz, or start from the new **Clean lows, warm grit** preset. It starts Off, so existing rigs sound the same.
- **Smart Gate:** notes clearly above the learned noise open faster, so short picked notes keep more attack; abrupt noise bursts can also pass more easily. The optional final gate now listens to the cleaned signal and has its own **Gate release** knob, starting at 40 ms. An amber trace in the display shows what the gate hears. Existing rigs with the final gate enabled can close sooner.

## Editor

The window is now 1080 pixels wide, a 3:2 shape. Module displays and the Tuner get most of the extra room, grid cards are wider, and long NAM and IR names fit in the browser. Pre 1 has its own input meter beside the stereo IN meter, Settings is a gear icon, and button labels beside icons no longer get cut short at some window sizes.

## Finding captures and IRs

IR files now show what their names say: cabinet size, speaker, mic and position, acoustic guitar and pickup, reverb unit, and a type such as Cabinet, Hall, Plate or Spring. Search with `category:hall`, `speaker:creamback` or `mic/cab:md421`. NAM captures gain more brands, mics, speakers and model families, with fewer false labels for both captures and IRs.

## Host compatibility

When a rig changes the plugin's latency, for example while a project with resampled captures loads, NAM Grid now reports it the way CLAP requires. Bitwig should no longer log "plugin reported latency change while activated".

## Platforms and compatibility

Windows x64, macOS Apple Silicon and Linux x64 builds contain CLAP and VST3. macOS bundles are ad-hoc signed, not notarized. Linux uses X11/XWayland and has not been tested in a real DAW.

NAM captures and cabinet IRs outside the bundled examples remain external files: keep them available when moving rigs between computers. Projects and presets from before the 0.1.0 compatibility baseline are not supported.
