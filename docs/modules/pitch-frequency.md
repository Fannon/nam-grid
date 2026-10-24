# Pitch / Frequency

Octaves, doubling and frequency shifting. Screenshots show each parameter pane at its starting settings.

| Module | In short |
| --- | --- |
| [Mono Octaver](#mono-octaver) | Sub-octave layers follow single notes; Dry retains the pick attack. |
| [Stereo Doubler](#stereo-doubler) | Opposite detuning and delay suggest a second take; Spread widens it. |
| [Frequency Shifter](#frequency-shifter) | Hertz offsets make metallic sidebands; Feedback shifts each repeat. |

## Mono Octaver

![Mono Octaver editor](../screenshots/modules/mono-octaver.png)

Add one or two octaves below single notes. High cut rounds the generated tone. Play cleanly: chords and weak fundamentals confuse tracking.

**Dry** (0% to 100%; starts at 100%) Level of the original guitar alongside the generated sub-octaves.

**1 oct down** (0% to 100%; starts at 50%) Level of this generated sub-octave. Best tracking comes from clean, single-note playing.

**2 oct down** (0% to 100%; starts at 0%) Level of this generated sub-octave. Best tracking comes from clean, single-note playing.

**High cut** (80 Hz to 5000 Hz; starts at 1000 Hz) High-cut frequency for the generated sub-octaves; the dry guitar keeps its original tone.

**Output** (-24 dB to 6 dB; starts at 0 dB) Final level after the dry/effect blend. Match bypass volume when comparing tones.

**Display:** A 110 Hz note and the two sub-octave square waves.

## Stereo Doubler

![Stereo Doubler editor](../screenshots/modules/stereo-doubler.png)

Opposite pitch detuning and short delays on the left and right suggest a second take.

**Detune** (1 cents to 25 cents; starts at 6 cents) Opposite pitch offsets in cents; higher settings make a wider, more obvious double.

**Delay** (1 ms to 30 ms; starts at 15 ms) Base separation before each detuned voice; increases the double-tracked feel.

**Spread** (0% to 100%; starts at 100%) Spread of detuned voices; zero centers the wet pair, 100 keeps them on opposite sides.

**Output** (-24 dB to 6 dB; starts at 0 dB) Final level after the dry/effect blend. Match bypass volume when comparing tones.

**Mix** (0% to 100%; starts at 25%) Balance the dry sound and effect. Output, when present, is applied after this blend.

**Display:** Pitch up and down against the stereo spread.

## Frequency Shifter

![Frequency Shifter editor](../screenshots/modules/frequency-shifter.png)

Move every partial by the same number of hertz for metallic, shifting, and continuously rising or falling textures. This is not a musical pitch shifter.

**Shift** (-2000 Hz to 2000 Hz; starts at 2 Hz) Fixed hertz offset per pass; unlike a pitch shifter, every partial moves by the same number of hertz.

**Feedback** (0% to 100%; starts at 10%) Return shifted audio into the delay for progressively changing repeats.

**Delay** (0.05 ms to 1000 ms; starts at 20 ms) Time between shifted feedback passes; short settings smear, long settings make distinct echoes.

**Sideband** (-100% to 100%; starts at 100%) Lower sideband at -100, ring modulation at zero, upper sideband at +100.

**High cut** (200 Hz to 12000 Hz; starts at 8000 Hz) High-cut frequency of the processed signal. Lower values tame brightness and harshness.

**Spread** (0% to 100%; starts at 0%) Carrier phase difference between channels; widens the shifted texture.

**Mix** (0% to 100%; starts at 30%) Balance the dry sound and effect. Output, when present, is applied after this blend.

**Display:** How each pass through the feedback moves the partials.
