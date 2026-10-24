# Reverb

Rooms, plates, halls, springs, shimmer and ambient echoes. Each module shows its editor with the starting settings while guitar plays; see the
[module guide](README.md) for how the editor, its display and the level strip work.

| Module | In short |
| --- | --- |
| [Room](#room) | Short reflections for intimacy; Size changes spacing, Decay the tail. |
| [Dense Plate](#dense-plate) | Dense, smooth tail; Pre-delay keeps the dry attack distinct. |
| [Cloud](#cloud) | Long moving wash; Damping and Low cut keep the tail clear. |
| [Hall](#hall) | Wide reflection spacing and a long tail; Pre-delay adds separation. |
| [Spring Reverb](#spring-reverb) | Dwell excites the spring; Drip adds splash, Damping tames treble. |
| [Shimmer](#shimmer) | Pitch and Amount build rising/falling tails; Freeze sustains them. |
| [Airwindows Plate](#airwindows-plate) | Airwindows kPlate140: dense plate character with native sustain and resolution. |
| [Airwindows Guitar Hall](#airwindows-guitar-hall) | Airwindows kGuitarHall2: colored hall with selectable early-reflection positions. |
| [Galactic Space](#galactic-space) | Airwindows Galactic3: huge stereo space; Replace controls how new notes enter the wash. |
| [Cosmic Echo](#cosmic-echo) | Valhalla Supermassive-inspired; Warp spreads echoes into a wash, Density then thickens it. |

## Room

![Room editor](../screenshots/modules/room.png)

Short multistage diffusion, early reflections and a small Householder network: a supportive room.

**Decay** (0.15 s to 30 s; starts at 0.8 s) Tail decay time. Higher values let notes overlap into a larger ambient wash.

**Size** (25% to 200%; starts at 80%) Tank scale and reflection spacing. Larger settings sound more open; Decay controls how long they last.

**Damping** (800 Hz to 12000 Hz; starts at 6500 Hz) Treble damping in the reverb. Lower settings make the tail darker than the dry signal.

**Pre-delay** (0 ms to 200 ms; starts at 8 ms) Gap before the reverb starts; separates the pick attack from the tail.

**Modulation** (0% to 100%; starts at 10%) Movement of the reverb delay lines; adds a gently shifting tail.

**Low cut** (0 Hz to 500 Hz; starts at 120 Hz) Remove bass from the reverb send to keep the tail from getting boomy.

**Mix** (0% to 100%; starts at 12%) Blend dry and processed sound; higher settings make the effect more prominent. Output, when present, follows the blend.

In the header:

- **Stereo wet** (off / on; starts on) Stereo wet spread; off folds the reverb to center while preserving dry stereo.

**Display:** Pre-delay, early reflections and the decay to -60 dB on a logarithmic time axis, with the low (amber) and high (gray) bands' decay.

**Tips:**

- Raise Pre-delay to keep the pick attack in front of the tail.

## Dense Plate

![Dense Plate editor](../screenshots/modules/dense-plate.png)

Four input diffusers into a cross-coupled two-branch tank: a voiced digital plate, not a physical model.

**Decay** (0.15 s to 30 s; starts at 2.4 s) Tail decay time. Higher values let notes overlap into a larger ambient wash.

**Size** (25% to 200%; starts at 100%) Tank scale and reflection spacing. Larger settings sound more open; Decay controls how long they last.

**Damping** (800 Hz to 12000 Hz; starts at 6500 Hz) Treble damping in the reverb. Lower settings make the tail darker than the dry signal.

**Pre-delay** (0 ms to 200 ms; starts at 20 ms) Gap before the reverb starts; separates the pick attack from the tail.

**Modulation** (0% to 100%; starts at 15%) Movement of the reverb delay lines; adds a gently shifting tail.

**Low cut** (0 Hz to 500 Hz; starts at 120 Hz) Remove bass from the reverb send to keep the tail from getting boomy.

**Mix** (0% to 100%; starts at 18%) Blend dry and processed sound; higher settings make the effect more prominent. Output, when present, follows the blend.

In the header:

- **Stereo wet** (off / on; starts on) Stereo wet spread; off folds the reverb to center while preserving dry stereo.

**Display:** Pre-delay, early reflections and the decay to -60 dB on a logarithmic time axis, with the low (amber) and high (gray) bands' decay.

**Tips:**

- Raise Pre-delay to keep the pick attack in front of the tail.

## Cloud

![Cloud editor](../screenshots/modules/cloud.png)

A rotating stereo allpass loop with slow delay motion for long, moving washes.

**Decay** (0.15 s to 30 s; starts at 6 s) Tail decay time. Higher values let notes overlap into a larger ambient wash.

**Size** (25% to 200%; starts at 150%) Tank scale and reflection spacing. Larger settings sound more open; Decay controls how long they last.

**Damping** (800 Hz to 12000 Hz; starts at 5000 Hz) Treble damping in the reverb. Lower settings make the tail darker than the dry signal.

**Pre-delay** (0 ms to 200 ms; starts at 30 ms) Gap before the reverb starts; separates the pick attack from the tail.

**Modulation** (0% to 100%; starts at 40%) Movement of the reverb delay lines; adds a gently shifting tail.

**Low cut** (0 Hz to 500 Hz; starts at 150 Hz) Remove bass from the reverb send to keep the tail from getting boomy.

**Mix** (0% to 100%; starts at 30%) Blend dry and processed sound; higher settings make the effect more prominent. Output, when present, follows the blend.

In the header:

- **Stereo wet** (off / on; starts on) Stereo wet spread; off folds the reverb to center while preserving dry stereo.

**Display:** Pre-delay, early reflections and the decay to -60 dB on a logarithmic time axis, with the low (amber) and high (gray) bands' decay.

**Tips:**

- Raise Pre-delay to keep the pick attack in front of the tail.

## Hall

![Hall editor](../screenshots/modules/hall.png)

A larger diffusion network with bass and treble decay shaping and wide reflection spacing.

**Decay** (0.15 s to 30 s; starts at 3.2 s) Tail decay time. Higher values let notes overlap into a larger ambient wash.

**Size** (25% to 200%; starts at 120%) Tank scale and reflection spacing. Larger settings sound more open; Decay controls how long they last.

**Damping** (800 Hz to 12000 Hz; starts at 6000 Hz) Treble damping in the reverb. Lower settings make the tail darker than the dry signal.

**Pre-delay** (0 ms to 200 ms; starts at 25 ms) Gap before the reverb starts; separates the pick attack from the tail.

**Modulation** (0% to 100%; starts at 20%) Movement of the reverb delay lines; adds a gently shifting tail.

**Low cut** (0 Hz to 500 Hz; starts at 120 Hz) Remove bass from the reverb send to keep the tail from getting boomy.

**Mix** (0% to 100%; starts at 22%) Blend dry and processed sound; higher settings make the effect more prominent. Output, when present, follows the blend.

In the header:

- **Stereo wet** (off / on; starts on) Stereo wet spread; off folds the reverb to center while preserving dry stereo.

**Display:** Pre-delay, early reflections and the decay to -60 dB on a logarithmic time axis, with the low (amber) and high (gray) bands' decay.

**Tips:**

- Raise Pre-delay to keep the pick attack in front of the tail.

## Spring Reverb

![Spring Reverb editor](../screenshots/modules/spring-reverb.png)

Three dispersive spring tanks with driven excitation: spring-inspired, not a measured model of a named tank. Strong Dwell can alias.

**Decay** (0.3 s to 4 s; starts at 1.8 s) Tail decay time. Higher values let notes overlap into a larger ambient wash.

**Dwell** (0 dB to 12 dB; starts at 3 dB) Drive the spring send harder for more excitation and soft saturation.

**Drip** (0% to 100%; starts at 60%) Amount of spring dispersion: adds the splashy, metallic drip to attacks.

**Damping** (800 Hz to 12000 Hz; starts at 5000 Hz) Treble damping in the reverb. Lower settings make the tail darker than the dry signal.

**Output** (-12 dB to 12 dB; starts at 0 dB) Final level after dry/wet blending. Match bypass loudness before judging the effect.

**Mix** (0% to 100%; starts at 18%) Blend dry and processed sound; higher settings make the effect more prominent. Output, when present, follows the blend.

In the header:

- **Stereo wet** (off / on; starts on) Stereo wet spread; off folds the reverb to center while preserving dry stereo.

**Display:** The spring decay; Drip and Dwell shape the reflections.

## Shimmer

![Shimmer editor](../screenshots/modules/shimmer.png)

An eight-line reverb with granular pitched feedback: each pass climbs (or falls) by Pitch. Freeze holds the tail and stops new input.

**Decay** (0.5 s to 20 s; starts at 5 s) Tail decay time. Higher values let notes overlap into a larger ambient wash.

**Damping** (800 Hz to 12000 Hz; starts at 6000 Hz) Treble damping in the reverb. Lower settings make the tail darker than the dry signal.

**Pitch** (-24 st to 24 st; starts at 12 st) Semitone interval on each pitched reverb pass. Positive climbs, negative falls.

**Amount** (0% to 100%; starts at 25%) Pitched feedback into the reverb; more creates a stronger, longer shimmer.

**Modulation** (0% to 100%; starts at 30%) Movement of the reverb delay lines; adds a gently shifting tail.

**Freeze** (off / on; starts at off) Sustain the tank and stop feeding new notes; release to return to the normal decay.

**Mix** (0% to 100%; starts at 25%) Blend dry and processed sound; higher settings make the effect more prominent. Output, when present, follows the blend.

**Display:** The decay with the pitch steps each pass adds.

## Airwindows Plate

![Airwindows Plate editor](../screenshots/modules/airwindows-plate.png)

A native port of Airwindows kPlate140.

**Input** (-18 dB to 0 dB; starts at 0 dB) Pad the input to Airwindows kPlate140's nonlinear excitation. Zero dB sends the original level.

**Sustain** (0% to 100%; starts at 50%) Native kPlate140 regeneration and tonal blur. More sustains longer; this percentage is not a calibrated decay time.

**Resolution** (25% to 100%; starts at 100%) Internal processing resolution. Lower settings enlarge/darken the space through undersampling. Rate steps are part of the original algorithm and may be audible when automated.

**Pre-delay** (0 ms to 200 ms; starts at 15 ms) Gap before the plate send, in milliseconds. Separates the attack without changing the tank size.

**Output** (-18 dB to 6 dB; starts at 0 dB) Final level after dry/wet blending. Match bypass loudness before judging the effect.

**Low cut** (0 Hz to 500 Hz; starts at 100 Hz) Remove bass from the plate send. Zero is off; higher values keep driven guitar from making the tank boomy.

**Mix** (0% to 100%; starts at 18%) Blend dry and processed sound; higher settings make the effect more prominent. Output, when present, follows the blend.

**Display:** The relative tank fill; Sustain and Resolution are native controls, not calibrated decay times.

## Airwindows Guitar Hall

![Airwindows Guitar Hall editor](../screenshots/modules/airwindows-guitar-hall.png)

A native port of Airwindows kGuitarHall2 with selectable early-reflection positions.

**Sustain** (0% to 95%; starts at 50%) Native kGuitarHall2 regeneration. More lengthens the hall; limited below its maximum for controlled tails.

**Resolution** (25% to 100%; starts at 100%) Internal processing resolution. Lower settings enlarge/darken the space through undersampling. Rate steps are part of the original algorithm and may be audible when automated.

**Brightness** (0% to 100%; starts at 50%) Brightness of the native hall filters. Lower is darker and softer, higher keeps more pick detail.

**Early level** (0% to 100%; starts at 50%) Level of the hall's early reflections; adds a closer, more defined room around the long tail.

**Output** (-18 dB to 6 dB; starts at 0 dB) Final level after dry/wet blending. Match bypass loudness before judging the effect.

**Position** (0% to 100%; starts at 50%) Selects a different early-reflection delay configuration. The native algorithm has discrete positions, so transitions can be audible.

**Mix** (0% to 100%; starts at 20%) Blend dry and processed sound; higher settings make the effect more prominent. Output, when present, follows the blend.

**Display:** The relative tank fill with the early reflections.

## Galactic Space

![Galactic Space editor](../screenshots/modules/galactic-space.png)

A native port of Airwindows Galactic3: a huge, slowly evolving stereo pad. Replace sets how quickly new notes enter the wash.

**Replace** (5% to 100%; starts at 50%) How quickly new notes replace the existing space. Lower settings retain a longer wash; minimum stays above the infinite-hold endpoint.

**Brightness** (5% to 100%; starts at 50%) Treble entering and leaving Galactic3. Lower settings make a darker ambient bed.

**Detune** (0% to 100%; starts at 35%) Stereo pitch motion within Galactic3; higher values make a wider, more obviously moving wash.

**Size** (5% to 100%; starts at 60%) Native Galactic3 delay-buffer scale. Together with Resolution it sets space size; resizing a sounding tank can make pitch/noise artifacts.

**Output** (-18 dB to 6 dB; starts at 0 dB) Final level after dry/wet blending. Match bypass loudness before judging the effect.

**Resolution** (25% to 100%; starts at 100%) Internal processing resolution. Lower settings enlarge/darken the space through undersampling. Rate steps are part of the original algorithm and may be audible when automated.

**Mix** (0% to 100%; starts at 25%) Blend dry and processed sound; higher settings make the effect more prominent. Output, when present, follows the blend.

**Display:** The relative tank fill.

## Cosmic Echo

![Cosmic Echo editor](../screenshots/modules/cosmic-echo.png)

Six ambient echo and reverb styles from David Braun's MIT Faust reconstruction of the Valhalla Supermassive algorithms. See [Cosmic Echo](../cosmic-echo.md) for the styles.

**Time** (20 ms to 8000 ms; starts at 280 ms) Delay-line time scale, from 20 ms to eight seconds. The dial gives more space to long intervals. Warp spreads shorter lines below this value; Sagittarius can build up over several intervals.

**Warp** (0% to 100%; starts at 51.6%) Spread the delay-line lengths: zero aligns them for echoes, higher values create uneven spacing and more reverb-like texture.

**Feedback** (0% to 98%; starts at 30.9%) Longer repeats and sustain. In Sagittarius and Centaurus, Feedback also changes how quickly the sound builds up. This is not a decay time in seconds.

**Density** (0% to 100%; starts at 49.2%) Rotate signals between the delay lines. Low values leave distinct echoes; high values build a denser stereo field. Interacts with Warp.

**High cut** (200 Hz to 16000 Hz; starts at 7461 Hz) Wet treble cutoff. Virgo filters each feedback pass, darkening successive echoes. The other styles filter their output.

**Modulation** (0% to 100%; starts at 29.5%) Depth of the shared 0.3 Hz stereo modulation. More adds pitch movement and chorus. Stereo width is full and the wet low cut is fixed at 80 Hz.

**Mix** (0% to 100%; starts at 18%) Blend the ambient echo/reverb with the original stereo signal. Zero preserves the exact dry sound.

In the header:

- **Style** (Gemini, Sagittarius, Virgo, Centaurus, Great Annihilator, Lyra; starts at Virgo) Gemini: dense, smooth ambience from sixteen scattering delays. Valhalla Supermassive-inspired Faust model. Style changes fade and clear the old tail.

**Display:** The measured wet response to one impulse, on a dB scale. Separate spikes are distinct echoes; Warp spreads the delay lines into a wash and Density then thickens it. At Warp 0, Density changes little.

**Tips:**

- Start with low Warp for rhythmic echoes, then raise Warp and Density for an ambient wash.
