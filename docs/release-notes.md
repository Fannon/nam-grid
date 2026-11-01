# NAM Grid 0.5.0

Six new effects, more useful starting presets, clearer routing and faster ways to find and inspect your sound. Existing released projects, presets and automation keep loading. Saved rigs retain their routing; new rigs start with sequential lanes.

## New sounds

- **Pitch Shift:** fixed-interval harmony, octave layers and fine detuning. The dry attack stays immediate; the shifted voice has short grain delay and can smear or warble on chords and large shifts.
- **Graphic EQ:** seven stompbox-style bands, with guitar, bass and full-range voicings.
- **Dynamic EQ:** cut a troublesome band only when it gets loud, for palm-mute boom or harsh pick attacks.
- **Multiband Dynamics:** three bands of upward and downward compression, from gentle control to a dense over-the-top sound. Drag the display to set the band splits and Threshold.
- **Transient Shaper:** sharpen or soften the pick, and shorten or lengthen the sustain without setting a threshold.
- **Harmonic Tremolo:** the swirling bass/treble pulse of early brown-panel amps.
- **Auto-wah:** one Auto button assigns the envelope to Wah Position, ready to follow your playing.

## Build and navigate your rig

- Choose **Sequential** (Pre → A → B → Post) or **Parallel** lanes. New rigs and Clean Default start sequential, so an empty lane no longer adds a dry return. Saved rigs keep their routing and sound. Parallel adds separate left/right balances for each lane, direct mute/solo buttons and stereo contribution meters.
- The optional frequency split offers **Gentle** or **Steep** filtering. Steep keeps more low end out of a driven lane. Its overall level is flat, but its phase changes how the lanes combine with other processing; switching slopes crossfades.
- Mark modules, captures, IRs and rig presets with yellow, orange or red **stars**, and use Starred to narrow the browser. Capture and IR stars survive moves and renames; identical copies share them.
- NAM and IR browsers show folders in the sidebar, with `_` folders first. Search and Details understand more device, maker, gain, channel, cabinet and microphone abbreviations. Click a tag value in Details to search across folders; scroll long details.
- **Gate** now holds the three noise gates, before Dynamics. Gate stays silver; Dynamics uses warm sand. Module arrows cycle within each category.
- More factory presets across delays, reverbs and other effects, including beat-synced rhythms, practical guitar support, Frequency Shifter recipes and **Aurora Hall Medium** for Cosmic Echo. Existing rigs and insertion settings stay as they were.

## See and control your sound

Click a global or slot output meter to open **Signal Monitor**: stereo spectrum, resonance markers, level history, headroom and correlation, without using a module slot. A module input meter opens the preceding slot's output. Hover over frequency charts for frequency and note readouts; module spectra and response curves are clearer.

Multiband Dynamics has clearer labels and shows its actual thresholds, detector levels and gain changes. Its presets now restore Threshold and band splits too. Delete or Backspace clears the selected module; Escape closes the preset browser and cancels audition. Windows module preset naming accepts letters and spaces in a native text field.

**Install factory content...** in Settings adds the bundled clean capture, Empty, Clean Default and drive examples to your chosen library folder, preserving existing files. Trails and Keep dry level now live in Settings and remain saved per rig. Settings and Modulation have cleaner layouts. Saved scenes take less space and keep the values you stored.

## Sound changes and fixes

- **Frequency Shifter at high Feedback:** repeats now decay and shed their lowest frequencies, preventing a stuck drone or DC buildup. Existing high-feedback rigs will sound different; the control range is unchanged.
- Fixed a low bass burst when switching the frequency split from Gentle back to Steep after silence.
- Fixed mute/solo buttons leaving a lane muted or soloed when clearing conflicting settings recalled from an older rig. Loading the rig still preserves its saved sound.

## Platforms and compatibility

Windows x64, macOS Apple Silicon and Linux x64 builds contain CLAP and VST3. macOS bundles are ad-hoc signed, not notarized. Linux uses X11/XWayland and has not been tested in a real DAW. Automated tests complement listening and host testing; they do not establish complete DAW coverage.

NAM captures and cabinet IRs outside the bundled examples remain external files: keep them available when moving rigs between computers. Projects and presets from before the 0.1.0 compatibility baseline are not supported. Development rigs that used the unreleased standalone Signal Monitor module must use the new meter view instead.
