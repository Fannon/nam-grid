# Module guide

Choose any module in any grid slot: NAM captures, cabinet IRs, or 54 built-in effects. Start with the sound you want, then use the category guide for controls and tips.

| Sound | Guide |
| --- | --- |
| Amp captures and cabinets | [Captures & cabinets](captures.md) |
| Drive, boost, tape colour, and crushed textures | [Distortion](distortion.md) |
| Noise control, compression, and limiting | [Dynamics](dynamics.md) |
| Tone shaping, filtering, and wah | [EQ / Filter](eq-filter.md) |
| Chorus, phaser, tremolo, rotary, and more | [Modulation](modulation.md) |
| Octave, doubling, and frequency effects | [Pitch / Frequency](pitch-frequency.md) |
| Digital, tape, reverse, and rhythmic echoes | [Delay](delay.md) |
| Rooms, plates, springs, shimmer, and ambience | [Reverb](reverb.md) |
| Freeze, stutter, and lo-fi tape | [Creative](creative.md) |
| Stereo control and volume swells | [Utility / Volume](utility-volume.md) |

## Use the module editor

![Tape Echo selected in the NAM Grid module editor](../screenshots/delay.png)

Select a grid card to open its module below the grid. Each category page shows the same focused parameter-pane view.

- **Name and navigation:** open the quick module menu, browse, or use previous/next within the category.
- **Options:** choose tempo sync and module-specific modes. Two-choice options toggle on click; longer lists open a menu.
- **Controls:** up to four controls share one row. Larger modules place **Output** at the start of the second row and **Mix** at the end; **Enabled** is always at the bottom right.
- **Knobs:** drag or click a value to type. Double-click restores the starting value. Right-click assigns [modulation](../performance.md).
- **Display:** see the effect's response, timing, or interaction with your playing.
- **Level strip:** compare input/output and match volume when judging a tone.

Every knob and Enabled can be automated by your DAW. Header options are saved with the rig but are not host-automatable.

When **Tempo sync** is on, **Division** replaces Time or Rate. Choose 1/32 to 1/1, including dotted notes and triplets. Your manual setting remains stored for when you turn sync off.

New modules use electric-guitar starting settings that make the effect easy to hear without burying the dry sound. See [starting settings](../effect-defaults.md) for reset behavior and acoustic/bass suggestions.

## Common controls

| Control | What it does |
| --- | --- |
| **Mix** | Balances dry sound and effect. 0% is dry; 100% is fully processed. |
| **Output** | Sets final level after Mix. Match bypass volume for a fair tone comparison. |
| **Drive / Input** | Pushes a saturating stage harder for more distortion or colour. |
| **High cut / Low cut** | Removes treble or bass from the processed path. |
| **Damping** | Controls how quickly reverb treble fades; lower is darker. |
| **Modulation** | Adds delay-line or pitch movement. |
| **Feedback** | Sends sound around again for more repeats or resonance. |

Ranges and starting values are listed with every control. Native Airwindows controls sometimes use percentages rather than calibrated frequencies or times; the relevant guide calls this out.

## Read the display

Displays fall into three groups:

- **Live audio:** input/output histories, spectra, gain reduction, and measured responses.
- **Response curves:** EQ, filter, drive, and compressor shapes.
- **Timing guides:** repeats, reverb decay, and modulation shapes based on current settings—not a live LFO position.

The **level strip** shows RMS and peak at input and output. **Gain** near 0 dB makes bypass comparisons fair. A falling **Crest** value usually indicates compression or saturation; a rising value indicates sharper attacks or expansion. Red peak readings warn of levels above 0 dBFS.

Disable displays in **Library & settings > Visualizations** without changing the sound or saved controls.
