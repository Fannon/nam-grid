# Distortion

Drives, boosts and saturation. Each module shows its editor with the starting settings while guitar plays; see the
[module guide](README.md) for how the editor, its display and the level strip work.

| Module | In short |
| --- | --- |
| [Distortion](#distortion) | Style changes harmonics; High cut softens edges, Mix restores dry attack. |
| [Boost](#boost) | Clean boost into the next module; Tilt balances body and bite. |
| [Tube Boost](#tube-boost) | Softer clipping with asymmetric harmonics; Output matches loudness. |
| [Airwindows Tape](#airwindows-tape) | Drive adds tape saturation; Bump adds low-end body. |
| [Console Colour](#console-colour) | Eight nonlinear console voices; Input drives the colour, Output matches loudness. |
| [Bitcrusher](#bitcrusher) | Bits add grain; Sample rate adds aliasing, High cut softens the result. |

## Distortion

![Distortion editor](../screenshots/modules/distortion.png)

An original waveshaper with six styles, from soft clipping to rectified octave buzz. Bias makes the curve asymmetric for even harmonics; a DC blocker removes the offset. There is no oversampling, so bright, high-drive settings can alias.

**Drive** (0 dB to 36 dB; starts at 9 dB) Gain into the shaper; more drive compresses and adds harmonics. Match loudness with Output.

**High cut** (200 Hz to 12000 Hz; starts at 5500 Hz) High-cut frequency of the processed signal. Lower values tame brightness and harshness.

**Bias** (-100% to 100%; starts at 0%) Asymmetric clipping adds even harmonics and changes attack texture. The processor removes DC.

**Output** (-24 dB to 6 dB; starts at -6 dB) Final level after dry/wet blending. Match bypass loudness before judging the effect.

**Mix** (0% to 100%; starts at 100%) Blend dry and processed sound; higher settings make the effect more prominent. Output, when present, follows the blend.

In the header:

- **Style** (Soft clip, Hard clip, Fuzz, Foldback, Diode, Rectify; starts at Soft clip) Choose soft or hard clipping, dense fuzz, wavefolding, asymmetric diode clipping or octave-like rectification.

**Display:** Left: the static shaper curve before tone and mix; the highlighted span shows how much of the curve your playing reaches. Right: the High cut, Mix and Output response.

**Tips:**

- Drive into a following NAM capture for a pedal-into-amp sound, with Output lowered to keep the capture's input sensible.
- Fuzz and Rectify respond strongly to Bias; small moves change the character more than Drive.

## Boost

![Boost editor](../screenshots/modules/boost.png)

A clean level boost with opposing shelves around 800 Hz (Tilt) and a low cut. It adds no saturation; use it to push the next module harder.

**Gain** (0 dB to 18 dB; starts at 3 dB) Clean level boost before the next module. Drive a following NAM or distortion harder.

**Tilt** (-6 dB to 6 dB; starts at 0 dB) Tilt around 800 Hz: positive is brighter/thinner, negative is darker/fuller.

**Low cut** (0 Hz to 300 Hz; starts at 0 Hz) Removes low end before the boost; 0 is off.

**Display:** The gain stage's transfer line and the Tilt/Low cut response.

**Tips:**

- Raise Gain and Low cut together before a high-gain capture to tighten palm mutes.

## Tube Boost

![Tube Boost editor](../screenshots/modules/tube-boost.png)

Tube-inspired biased soft saturation with adjustable asymmetry, the Boost's Tilt and Low cut, and a DC blocker. It is not a circuit emulation; strong Drive can alias.

**Drive** (0 dB to 24 dB; starts at 6 dB) Gain into the shaper; more drive compresses and adds harmonics. Match loudness with Output.

**Tilt** (-6 dB to 6 dB; starts at 0 dB) Tilt around 800 Hz: positive is brighter/thinner, negative is darker/fuller.

**Low cut** (0 Hz to 300 Hz; starts at 0 Hz) Removes low end before the boost; 0 is off.

**Asymmetry** (0% to 100%; starts at 25%) Asymmetric clipping adds even harmonics and changes attack texture. The processor removes DC.

**Output** (-18 dB to 18 dB; starts at -3 dB) Final level after dry/wet blending. Match bypass loudness before judging the effect.

**Mix** (0% to 100%; starts at 100%) Blend dry and processed sound; higher settings make the effect more prominent. Output, when present, follows the blend.

**Display:** The saturation curve with the input span you reach, and the small-signal EQ.

**Tips:**

- Use Output to match bypass loudness before judging the tone.

## Airwindows Tape

![Airwindows Tape editor](../screenshots/modules/airwindows-tape.png)

A native port of Airwindows Tape: tape saturation and head bump, with no wow or flutter (that lives in Tape Echo and LoFi Tape Machine).

**Drive** (-12 dB to 12 dB; starts at 0 dB) Drive the Airwindows tape saturation harder. Use Output to match loudness.

**Bump** (0% to 100%; starts at 25%) Low-frequency tape head emphasis; more adds body and bass resonance.

**Output** (-18 dB to 6 dB; starts at 0 dB) Final level after dry/wet blending. Match bypass loudness before judging the effect.

**Mix** (0% to 100%; starts at 100%) Blend dry and processed sound; higher settings make the effect more prominent. Output, when present, follows the blend.

**Display:** The Spiral stage curve and the head-bump amount.

**Tips:**

- Small Drive settings add density without obvious distortion; Bump adds body on thin captures.

## Console Colour

![Console Colour editor](../screenshots/modules/console-colour.png)

Eight console voices from Airwindows BussColors4: nonlinear convolution and saturation with fixed per-desk level matching. Desk changes fade through dry.

**Input** (-18 dB to 18 dB; starts at 0 dB) Gain into the console colour, in real dB. More level drives the nonlinear convolution and saturation harder.

**Output** (-18 dB to 18 dB; starts at 0 dB) Wet-path output trim in real dB. Match desk loudness; Mix zero preserves the exact dry signal.

**Mix** (0% to 100%; starts at 100%) Blend console colour with the original stereo signal. Both trims affect only the processed path.

In the header:

- **Desk** (Dark, Rock, Lush, Vibe, Holo, Punch, Steel, Tube; starts at Dark) Eight console voices. Desk changes fade through dry while the convolution and sag histories reset.

**Display:** Measured input against output, and the output history.

**Tips:**

- Try it last in Post for glue; drive Input harder for more colour, then match with Output.

## Bitcrusher

![Bitcrusher editor](../screenshots/modules/bitcrusher.png)

Bit-depth and sample-rate reduction with a high cut after the crush.

**Bits** (2 bits to 16 bits; starts at 10 bits) Bit depth of the quantizer; fewer bits add stronger grain and distortion.

**Sample rate** (100 Hz to 48000 Hz; starts at 16000 Hz) Sample-and-hold rate, capped at the host rate. Lower settings add aliasing and stepped texture.

**High cut** (200 Hz to 20000 Hz; starts at 6500 Hz) High-cut frequency of the processed signal. Lower values tame brightness and harshness.

**Output** (-24 dB to 6 dB; starts at 0 dB) Final level after dry/wet blending. Match bypass loudness before judging the effect.

**Mix** (0% to 100%; starts at 50%) Blend dry and processed sound; higher settings make the effect more prominent. Output, when present, follows the blend.

**Display:** One cycle of a test tone, dry against crushed, with the high cut applied.

**Tips:**

- Lower Sample rate first for aliasing tones; lower Bits for grain.
