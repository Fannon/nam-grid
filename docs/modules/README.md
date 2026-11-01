# Module guide

Any slot can hold a NAM capture, a cabinet IR or a built-in effect. Each category page lists its modules with their controls, ranges, starting settings and tips; the screenshots show every module at its starting settings.

| Category | Modules |
| --- | --- |
| [Captures & cabinets](captures.md) | [NAM capture](captures.md#nam), [Cabinet IR](captures.md#cabinet-ir) |
| [Distortion](distortion.md) | [Distortion](distortion.md#distortion), [Pedal Drive](distortion.md#pedal-drive), [Boost](distortion.md#boost), [Tube Boost](distortion.md#tube-boost), [Airwindows Tape](distortion.md#airwindows-tape), [Console Colour](distortion.md#console-colour), [Bitcrusher](distortion.md#bitcrusher) |
| [Gate](gate.md) | [Noise Gate](gate.md#noise-gate), [Noise Gate (Smart)](gate.md#noise-gate-smart), [Digital Black Gate](gate.md#digital-black-gate) |
| [Dynamics](dynamics.md) | [Pedal Comp](dynamics.md#pedal-comp), [Smooth Compressor](dynamics.md#smooth-compressor), [Punch Compressor](dynamics.md#punch-compressor), [Butter Comp](dynamics.md#butter-comp), [Outboard Channel](dynamics.md#outboard-channel), [Transient Shaper](dynamics.md#transient-shaper), [Multiband Dynamics](dynamics.md#multiband-dynamics), [Peak Limiter](dynamics.md#peak-limiter) |
| [EQ / Filter](eq-filter.md) | [Guitar EQ](eq-filter.md#guitar-eq), [Cut EQ](eq-filter.md#cut-eq), [Graphic EQ](eq-filter.md#graphic-eq), [Parametric EQ](eq-filter.md#parametric-eq), [Dynamic EQ](eq-filter.md#dynamic-eq), [Console EQ](eq-filter.md#console-eq), [Filter](eq-filter.md#filter), [Wah](eq-filter.md#wah) |
| [Modulation](modulation.md) | [Chorus](modulation.md#chorus), [Juno Chorus](modulation.md#juno-chorus), [Flanger](modulation.md#flanger), [Phaser](modulation.md#phaser), [Auto Tremolo](modulation.md#auto-tremolo), [Harmonic Tremolo](modulation.md#harmonic-tremolo), [Vibrato](modulation.md#vibrato), [Rotary](modulation.md#rotary), [Uni-Vibe](modulation.md#uni-vibe), [Ring Modulator](modulation.md#ring-modulator) |
| [Pitch / Frequency](pitch-frequency.md) | [Pitch Shift](pitch-frequency.md#pitch-shift), [Mono Octaver](pitch-frequency.md#mono-octaver), [Stereo Doubler](pitch-frequency.md#stereo-doubler), [Frequency Shifter](pitch-frequency.md#frequency-shifter) |
| [Delay](delay.md) | [Digital Delay](delay.md#digital-delay), [Analog Echo](delay.md#analog-echo), [Tape Echo](delay.md#tape-echo), [Multi-head Tape](delay.md#multi-head-tape), [Reverse Delay](delay.md#reverse-delay), [Dual Delay](delay.md#dual-delay), [Diffused Delay](delay.md#diffused-delay) |
| [Reverb](reverb.md) | [Room](reverb.md#room), [Dense Plate](reverb.md#dense-plate), [Cloud](reverb.md#cloud), [Hall](reverb.md#hall), [Spring Reverb](reverb.md#spring-reverb), [Shimmer](reverb.md#shimmer), [Airwindows Plate](reverb.md#airwindows-plate), [Airwindows Guitar Hall](reverb.md#airwindows-guitar-hall), [Galactic Space](reverb.md#galactic-space), [Cosmic Echo](reverb.md#cosmic-echo) |
| [Creative](creative.md) | [Freeze / Hold](creative.md#freeze--hold), [Stutter](creative.md#stutter), [LoFi Tape Machine](creative.md#lofi-tape-machine) |
| [Utility / Volume](utility-volume.md) | [Utility](utility-volume.md#utility), [Auto Swell](utility-volume.md#auto-swell) |

Five modules have their own pages with more background and recipes: [Pedal Drive](pedal-drive.md), [Noise Gate (Smart)](noise-gate-smart.md), [LoFi Tape Machine](lofi-tape.md), [Cosmic Echo](cosmic-echo.md) and [Cabinet IR processing](ir-processing.md).

## The module editor

![Tape Echo selected in the NAM Grid module editor](../screenshots/delay.png)

Select a module in the grid to edit it below. Every module uses the same layout: name, navigation and an enable/bypass circle at the top, header options such as **Style** or **Tempo sync**, up to eight knobs, and a display with input/output meters in its footer. See [Shape a module](../ui.md#shape-a-module) for how to use it.

Every knob and **Enabled** can be automated by your DAW. Other header options are saved with the rig but cannot be automated.

## Starting settings

New modules start with electric-guitar settings that make the effect easy to hear without burying the dry sound. Each category page lists them next to every control.

- Adding or replacing a module applies its starting settings; double-click a knob to return to its starting value. Saved rigs, projects and Copy/Move/Swap keep your exact settings.
- Outputs start at unity unless a module lists an offset, such as Distortion's -6 dB. Cabinet IRs start at -18 dB, the usual cabinet headroom; IRs vary, so match them with the level meters.
- NAM captures, Guitar EQ, Parametric EQ and Utility start neutral. Thresholds and makeup depend on your instrument and interface, so set them by ear.

**Acoustic guitar:** start with neutral EQ and a low Room or Dense Plate mix, and reduce or bypass gates if they trim finger noise or sustain. For compression, set Pedal Comp's **Pickup** to **Acoustic**.

**Bass:** lower Cut EQ's Low cut to about 30–45 Hz or turn it off, keep delay and reverb mixes low, and give Mono Octaver less sub level.

## Common controls

| Control | What it does |
| --- | --- |
| **Mix** | Balances dry sound and effect. 0% is dry; 100% is fully processed. **Output** applies after Mix. |
| **Output** | Sets the module's final level. Match the bypassed volume for a fair comparison. |
| **Drive / Input** | Pushes a saturating stage harder for more distortion or colour. |
| **High cut / Low cut** | Removes treble or bass from the processed sound. |
| **Damping** | Controls how quickly reverb treble fades; lower is darker. |
| **Modulation** | Adds delay-line or pitch movement. |
| **Feedback** | Sends sound around again for more repeats or resonance. |

Some Airwindows controls are relative amounts in percent rather than frequencies or times; their descriptions say so.

## Tempo sync

Delays, modulation effects and Stutter offer **Tempo sync** in the header. When it is on, **Division** replaces Time or Rate: choose 1/32 to 1/1, including dotted notes and triplets. Without a DAW tempo, sync uses 120 BPM. Your manual setting stays stored for when you turn sync off.

## Read the display

Displays fall into three groups:

- **Live audio:** input/output histories, spectra, gain reduction and measured responses.
- **Response curves:** EQ, filter, drive and compressor shapes.
- **Timing guides:** repeats, reverb decay and modulation shapes drawn from the current settings, not a live LFO position.

The meters in every module display footer compare its input and output; see [displays and meters](../ui.md#displays-and-meters). Turn all displays off in **Settings > Visualizations** without changing the sound.
