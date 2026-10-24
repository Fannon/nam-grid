# Module guide

NAM Grid has 54 built-in effects plus NAM captures and cabinet IRs. Any slot takes any
module. This guide explains how a module editor works; the category guides describe each
module with a screenshot of its editor at its starting settings, every control with its
range and starting value, what its display shows, and a few tips.

| Guide | Modules |
| --- | --- |
| [Captures](captures.md) | NAM, Cabinet IR |
| [Distortion](distortion.md) | Distortion, Boost, Tube Boost, Airwindows Tape, Console Colour, Bitcrusher |
| [Dynamics](dynamics.md) | Noise Gate, Noise Gate (Smart), Digital Black Gate, Smooth Compressor, Punch Compressor, Butter Comp, Outboard Channel, Peak Limiter |
| [EQ / Filter](eq-filter.md) | Guitar EQ, Cut EQ, Parametric EQ, Console EQ, Filter, Wah |
| [Modulation](modulation.md) | Chorus, Juno Chorus, Flanger, Phaser, Auto Tremolo, Vibrato, Rotary, Uni-Vibe, Ring Modulator |
| [Pitch / Frequency](pitch-frequency.md) | Mono Octaver, Stereo Doubler, Frequency Shifter |
| [Delay](delay.md) | Digital Delay, Analog Echo, Tape Echo, Multi-head Tape, Reverse Delay, Dual Delay, Diffused Delay |
| [Reverb](reverb.md) | Room, Dense Plate, Cloud, Hall, Spring Reverb, Shimmer, Airwindows Plate, Airwindows Guitar Hall, Galactic Space, Cosmic Echo |
| [Creative](creative.md) | Freeze / Hold, Stutter, LoFi Tape Machine |
| [Utility / Volume](utility-volume.md) | Utility, Auto Swell |

## The module editor

![Tape Echo in the module editor](../screenshots/delay.png)

Select a tile in the grid and its editor opens below. Every module uses the same layout:

- **First row:** the slot (here A2), the module name, which opens the quick module menu,
  the browser icon, the previous/next arrows that step through the module's category, and
  the category badge.
- **Second row:** a one-line description of the module, then its saved options on the
  right: **Tempo sync** for delays and modulation, and one mode or option such as
  **Style**, **Mode**, **Desk**, **Heads**, **Ping-pong**, **Stereo wet** or
  **Notch mode**. Options read "Label: value". Modes with three or more choices open a
  menu; two-choice options switch on click. Options are saved with the rig but are not
  host-automatable, so a longer list never changes recorded automation.
- **Controls:** two rows of four positions. The module's main controls fill the top row
  from the left. **Output** sits below the first control, **Mix** in the third column of
  the bottom row, and **Enabled** (the module bypass) at the bottom right. Positions a
  module does not use stay empty, so the same control is always in the same place.
- **Display:** the panel on the right shows what the controls do (see below).
- **Level strip:** the line below the editor compares the module's input and output.

Working with controls:

- Drag a knob, or click its value to type one. Double-click resets it to the module's
  starting value. Every knob and Enabled is host-automatable.
- Right-click a knob to modulate it from the LFO, the envelope, the sequencer or a macro;
  Alt/Option-drag a modulated knob to change its depth. See
  [macros, modulation and scenes](../performance.md).
- When **Tempo sync** is on, **Division** (1/32 to 1/1, with triplets and dotted notes)
  replaces Time or Rate. The manual value is kept for when sync is off again.

Adding or replacing a module applies its starting settings, which are chosen for electric
guitar: audible, but without overwhelming the dry sound. See
[starting settings](../effect-defaults.md) for acoustic and bass adjustments.

## Common controls

The same words mean the same thing in every module:

| Control | Meaning |
| --- | --- |
| **Mix** | Blend of the dry and processed sound. 0% is the exact dry signal. |
| **Output** | Final level after the Mix blend. Use it to match bypass loudness. |
| **Drive**, **Input** | Level into a nonlinear stage: more saturation or colour. |
| **High cut**, **Low cut** | Filters on the processed path (or the repeats), in Hz. A few Airwindows modules use native percentages instead, noted in their guide. |
| **Damping** | How quickly a reverb's treble dies away. Lower is darker. |
| **Brightness** | A native tone control without a calibrated frequency. |
| **Modulation** | Amount of pitch or delay-line movement. |
| **Feedback** | Amount sent back for another pass: more repeats or resonance. |

Host automation names combine the slot, the family and the control, for example
`A 2 Delay Mix`. Modules of one family share their controls within a slot, so
`A 2 Delay Character` is Ducking, Age or Saturation depending on the delay, and
`A 2 Mod Character` is Texture, Sweep center, Feedback sign, Pulse / feel or Sweep color.

## Reading the display

Each display says what it shows in its title, detail and footer rows. There are three kinds:

- **Measured:** the live output spectrum behind the EQ curves, the NAM capture's input
  against output, the compressor and gate histories, Cosmic Echo's impulse response,
  and the LoFi Tape Machine's pitch motion.
- **Curves from the module's own math:** shaper and compressor curves, EQ and filter
  responses, the cabinet IR's frequency response.
- **Settings over time:** delay repeats, reverb decay, modulation shapes. These show what
  the settings do; they do not follow a live LFO phase.

The **level strip** shows input and output RMS (bar) and peak (tick) from -60 to 0 dBFS
over the last 0.4 s, the **Gain** between them and the **Crest** factor (peak to RMS) at
both ends. Gain near 0 dB is a fair comparison with bypass. A crest that drops through the
module means it compresses or saturates; one that rises means it expands or sharpens
attacks. Library & settings can turn displays and the strip off; audio is unaffected.

For implementation notes (real-time behavior, latency, tails and sources) see
[module implementation](../module-implementation.md).
