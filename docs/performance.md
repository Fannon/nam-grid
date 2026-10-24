# Performance controls

Shape sounds with your playing, assign a few useful controls to macros, and switch between three snapshots without rebuilding the rig.

## Routing

Click **Routing** below the A/B knob to open the lane controls. Select a module, close the page, or press Escape to return.

- **Lane A / Lane B:** adjust level, mute, or solo each lane. Solo respects the current blend; if both lanes are soloed, both play. Mute wins over solo.
- **Lane timing:** shift one lane by up to 20 ms to explore timing and phase between captures. Positive values delay A; negative values delay B.
- **Frequency split:** send lows to A and the remaining highs to B at **Crossover**. At centered Blend with empty lanes, the original sound is reconstructed. This is a gentle split, not a steep multiband crossover.

Use the split for rigs such as a clean low end in A and a driven high band in B. Check the combined tone in mono as well as stereo.

## Modulation

Click **Modulation** in the header. Sources are on the left; assignments and macros are on the right.

### Quick assignment

Right-click a module knob, lane level, crossover, or A/B knob and choose **Modulate with**. NAM Grid uses the next free matrix row at +25% depth.

- The ring around a modulated knob shows its range; the dot shows its current value.
- **Alt/Option-drag** adjusts depth; hold Shift for finer moves.
- **Remove modulation** clears assignments from that control.

### Sources

- **LFO:** sine, triangle, rising/falling saw, square, or random. Run freely or sync to your DAW tempo.
- **Envelope:** follows your playing. Choose where it listens, set **Threshold** to ignore background noise, and shape the response with **Sensitivity**, **Curve**, **Attack**, and **Release**.
- **Sequencer:** draw eight bipolar steps. Drag above the center to add and below to subtract; use free rate or tempo sync.
- **Macros 1–4:** four manual or host-automatable controls for your most important assignments.

The LFO and sequencer move above and below a knob's setting. The envelope and macros move in one direction. For a delay that gets out of the way while you play, assign Envelope to delay **Mix** with negative depth and adjust Release for the return.

**Retrigger** can restart the LFO or sequencer on an audio onset or incoming MIDI note. **One shot** plays one LFO cycle per trigger and then holds—useful for a fresh sweep on every note. MIDI notes trigger modulation; they do not change the guitar audio.

### Matrix

Each of the eight rows has a **source**, **destination**, **depth**, and optional **Via** control. Via scales the assignment from another source—for example, let a macro control how much LFO reaches chorus Depth.

Depth is a share of the destination's range. Multiple rows can affect one knob without pushing it beyond its limits. Rows for controls the current module does not have remain saved but inactive. Click **×** to clear a row.

All assignments and source settings are saved with the rig. Depths, macros, and most source controls can also be automated by your DAW.

## Performance scenes

The header buttons **1 2 3** store three snapshots of the current rig.

- **Click** a scene to recall it.
- **Right-click** to store the current controls or clear that scene.
- An outlined button contains a scene; the filled button is the last one stored or recalled.

Scenes capture controls, bypass, routing, and modulation assignments. They keep the same modules, captures, IRs, and quality settings. Use them for changes such as rhythm to lead, more delay, or a different lane balance.

Scenes belong to the rig and are saved with its preset. To change modules or captures, load a different [rig preset](presets.md) instead. Scenes do not change tuner settings or held/frozen audio.

## Tails and bypass

Scene changes are smooth because the modules stay loaded. Delays and reverbs also avoid abruptly cutting every tail:

- Bypassed delays stop feeding back while existing repeats fade.
- Bypassing Freeze keeps its held sound for when you return; releasing Hold ends it.
- Quickly re-enabling a bypassed cabinet or Stutter can resume its remaining sound.

Use your ears with stereo routing and modulation: independent channel processing can change stereo balance, especially with an uneven input signal.
