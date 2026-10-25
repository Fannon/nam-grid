# Pedal Drive

Pedal Drive is in the **Distortion** module category. It provides voiced overdrive,
distortion, fuzz and colored boost for electric guitar and bass. Choose **Style**
in its header; the [module guide](distortion.md#pedal-drive) lists the controls.

The existing Distortion, Tube Boost and Boost modules retain their previous sounds.
Pedal Drive has separate controls and saved style identities.

## Choose a voice

| Style | Character | Useful starting application |
| --- | --- | --- |
| **Screamer** | Mid-focused soft clipping, less bass entering saturation, rounded top end | Mild overdrive into a clean amp, or focused boost into a driven capture |
| **Rodent** | Frequency-shaped gain, slew rounding and harder clipping | Crunchy rhythm and heavier distortion; bass grind with clean lows |
| **Bass Edge** | Asymmetric two-stage saturation and upper-mid emphasis | Defined bass drive with Low keep raised |
| **Parallel OD** | Drive increases gain and shifts an internal clean/clipped balance toward clipping | Articulate low-gain boost and overdrive |
| **Sustain Fuzz** | Cascaded clipping, interstage filtering and scooped mids | Dense guitar leads, heavy bass fuzz and sustained chords |
| **Cascaded Preamp** | Two mildly asymmetric stages with filtering between them | Gentle breakup and a warmer bass upper band |
| **Treble Drive** | Bass-light colored boost with asymmetric grit | Push a crunchy amp capture for classic rock sounds |

These are original voices informed by pedal families, rather than measured or calibrated
replicas. Style selection changes a combination of filters and gain stages. High cut is
always an output low-pass frequency in Hz; it does not become a reversed pedal tone knob.

Match loudness with Output when comparing styles. Drive is gain into the stages, so its
audible effect depends on the instrument's level and the chosen voice. Start lower with a
hot active bass or an already loud signal.

## Preserve lows within one slot

For a simple single-lane bass rig, start here: Low keep preserves bass weight without
using a second grid lane. Both Low keep and Split are dials in the module editor.

**Low keep** removes a share of the low-frequency signal before Body and the drive stages,
then restores that share after High cut. **Split** sets this local low-band frequency.
At Low keep zero, Split has no effect on the sound.

```text
              +-- Body -> drive stages -> High cut --+
Input -> split+                                     +-> Mix -> Output
              +-- clean low return -----------------+    ^
Input ---------------- aligned full-range dry ----------+
```

At Low keep 100%, the entire extracted low band follows the clean return. The split is
gentle: some low-frequency content remains in the driven branch, and distortion can create
new components there. This is not a brick-wall separation or a guarantee of an unchanged
final bass spectrum.

Start a bass rig with Bass Edge, Split around 180 Hz and Low keep at 100%. Adjust Split
to move the balance between bass weight and driven definition. Lower Low keep to let more
of the fundamental enter saturation. Body shapes only the drive path; use Output to set
the level of the complete result.

**Mix** has a different job: it blends the complete pedal result, including its restored
lows, with the original full-range input. It can restore dry attack and definition, but
does not prevent bass from reaching the clipping stages. Parallel OD has an additional
internal clean/clipped blend linked to Drive; that blend and the outer Mix are independent.

This local return does not use either grid lane. You can place Pedal Drive in Pre, A, B or
Post, and use both parallel lanes for other processing.

## Using the grid split

The grid's **Routing > Frequency split** works earlier in the signal chain, after Pre4.
It sends low-pass audio to lane A and the complementary remainder to lane B. It is a
first-order split with broad overlap. Centered A/B, equal lane levels and unprocessed lanes
reconstruct the original signal.

```text
                     +-- A: clean lows -------------------+
IN -> Pre -> split --+                                    +-> Post -> OUT
                     +-- B: Pedal Drive, Low keep 0% ------+
```

Use the grid split when you want to edit, compress, EQ, mute or level the two branches
separately. Start with Crossover around 180 Hz, A/B at 50%, lane levels at 0 dB and Lane
timing at 0 ms. Put Pedal Drive in B1 with local Low keep at zero. The included **Bass -
Split Edge** and **Bass - Split Warm** rigs demonstrate this arrangement.

Moving A/B away from the center changes the low/high balance; it is not a conventional
full-range wet/dry control while the split is enabled. Keep it centered initially and use
lane levels to set the balance.

Placement matters:

- A drive in **Pre** processes the full signal before the grid split. Splitting it later
  cannot recover clean bass that was already distorted.
- A drive in **B** processes the upper branch, while A can retain a clean low band.
- A drive in **Post** processes the recombined signal and can distort the restored lows.
  The same applies to a driven NAM capture in Post.
- Pedal Drive's local Low keep acts on the signal arriving at that slot. In B it would
  split the upper branch again; start with it off in a grid-split bass rig.

Neither split removes or reverses previous distortion. To keep the final bass lows clean,
route them around downstream nonlinear processing too. A common amp or cabinet after the
lanes can still be appropriate when its coloration is part of the intended sound.

The grid compensates the lanes' reported processing delays, including Pedal Drive's delay.
Filter phase and nonlinear coloration can still change the combined tone. Check the result
in mono and leave Lane timing at zero unless you deliberately want a timing offset.

## Example rigs

Fresh installations include **Drive Examples** in the preset library. Existing users can
choose **Library & settings > Add drive presets...**, select a folder and rescan if needed.
The action adds the folder to the preset library and keeps existing files, including
edited examples. Loading an example replaces the current rig.

The examples contain effects without external capture or IR dependencies:

- **Drive - [style]** starts each Pedal Drive voice in Pre1. Bass Edge includes local
  Low keep; the other voices start without it.
- **Boost - Flat**, **Boost - Tight** and **Boost - Warm** use clean Boost. Tight raises
  the low cut and brightens Tilt; Warm tilts toward the bass.
- **Bass - Split Warm** uses Cascaded Preamp in B1; **Bass - Split Edge** uses Bass Edge.
  Both retain the grid's low branch in A and disable local Low keep.

Add your amp/cabinet processing where needed. For guitar, try the drive before a NAM amp
capture followed by a cabinet IR if the capture does not already include one. The examples
are starting points; their output trims are not automatic loudness matching.

## Processing and latency

Pedal Drive runs its nonlinear path at four times the host sample rate, with FIR filtering
on interpolation and decimation. This reduces aliasing while adding **64 host samples**
of latency: approximately 1.45 ms at 44.1 kHz or 1.33 ms at 48 kHz. The plugin reports the
delay to the host and aligns dry Mix, bypass, the clean low return and parallel grid lanes.
The delay stays constant across styles and while bypassed.

Style changes crossfade over 20 ms. Left and right channels have independent histories.
Control changes are smoothed, and processing does not allocate memory in the audio callback.
The signal-flow display shows routing; it does not claim to be the transfer function of
the complete stateful drive.

## Design references

No pedal-model source code was copied. The original implementation uses the following
published designs and descriptions as references:

- [Aion Stratus schematic and notes](https://aionfx.com/app/files/docs/stratus_documentation.pdf):
  mid-focused feedback clipping and low-frequency gain shaping.
- [Aion Helios schematic and notes](https://aionfx.com/app/files/docs/helios_documentation.pdf):
  frequency-shaped gain followed by diode-to-ground clipping.
- [Aion Refractor documentation](https://aionfx.com/app/files/docs/refractor_documentation.pdf):
  coupled gain and internal clean/clipped blending.
- [Aion Halo documentation](https://aionfx.com/app/files/docs/halo_documentation.pdf):
  cascaded clipping and midrange shaping.
- [Darkglass B3K](https://www.darkglass.com/pages/microtubes-b3k-manual) and
  [X Ultra](https://www.darkglass.com/pages/microtubes-x-ultra-manual) manuals:
  clean-signal blending and separate low/high processing.
- [Keeley Katana](https://robertkeeley.com/product/katana-boost/) and
  [BOSS BD-2](https://www.boss.info/global/products/bd-2/): staged preamp/drive concepts.
- [Analog Man Beano Boost](https://www.analogman.com/beano.htm): a colored, frequency-selective booster.

For the aliasing problem and approaches beyond oversampling, see
[Parker, Zavalishin and Le Bivic, DAFx 2016](https://www.dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf).
