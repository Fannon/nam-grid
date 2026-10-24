# Captures

NAM captures and cabinet IRs. Each module shows its editor with the starting settings while guitar plays; see the
[module guide](README.md) for how the editor, its display and the level strip work.

| Module | In short |
| --- | --- |
| [NAM](#nam) | Input drives the capture; EQ and Output shape its result. |
| [Cabinet IR](#cabinet-ir) | Cabinet convolution; the curve is the loaded IR's frequency response. |

## NAM

![NAM editor](../screenshots/nam.png)

Loads a Neural Amp Modeler capture (.nam): an amp, a pedal or a full rig. The EQ after the capture is neutral processing, not the captured amp's controls. A2 captures packaged with a smaller submodel offer **Quality: A2 / A2 Lite** in the header.

**Input** (-40 dB to 40 dB; starts at 0 dB) Drives the capture.

**Bass** (-12 dB to 12 dB; starts at 0 dB) 150 Hz shelf after the capture.

**Mid** (-12 dB to 12 dB; starts at 0 dB) Broad 800 Hz bell after the capture.

**Treble** (-12 dB to 12 dB; starts at 0 dB) 3 kHz shelf after the capture.

**Output** (-40 dB to 40 dB; starts at 0 dB) Level after the capture and its EQ.

**Low cut** (0 Hz to 300 Hz; starts at 0 Hz) Removes low end after the capture; 0 is off.

**High cut** (2000 Hz to 20000 Hz; starts at 20000 Hz) Removes highs after the capture; 20000 is open.

**Display:** Left: the capture's measured input against output. Right: the post-capture EQ over the live output spectrum.

**Tips:**

- Set Input so the capture is driven as intended; its calibration in dBu shows in the display.

## Cabinet IR

![Cabinet IR editor](../screenshots/ir.png)

Loads a mono cabinet impulse response (.wav). Short and long IRs are handled automatically; IRs are not normalized.

**Output** (-36 dB to 18 dB; starts at -18 dB) IR gain. 0 dB gives unity gain for a unit impulse; -18 dB is the usual cabinet headroom.

**Display:** The loaded IR's frequency response, relative to its peak.

**Tips:**

- Start at -18 dB Output and match the level with the strip below the editor.
