# Performance controls

**Modulation** in the header opens the modulation page; **Routing**, below the A / B
knob, shows the lane controls; the header's **1 2 3** buttons are the scenes.
See the [module guide](modules/README.md) for effect controls and [presets.md](presets.md) for complete rigs.
Hover a control for its explanation.

## Routing

**Routing** replaces the module editor below the grid, with the same layout: two rows of
controls and, on the right, the crossover response and the modules in each lane. Select
a slot, click **Close** or press Escape to return to the module editor. The tuner uses the
same area, so turning it on closes Routing.

- **Lane A / Lane B**: level, mute and solo per lane. Solo preserves the current blend
  level; enabling both solos passes both lanes, while mute takes precedence.
- **Lane timing**: positive values delay A, negative values delay B, up to 20 ms.
  It never advances live audio. This is additional to the existing conversion-delay
  compensation and helps audition relative capture timing, not correct every phase difference.
- **Frequency split**: optional complementary low/high split at the Crossover frequency.
  A receives the low band and B the residual high band. At centered Blend, neutral
  empty lanes reconstruct the original signal, including while the crossover fades
  on/off. Blend away from center changes the relative band levels. The split is a
  first-order complementary filter, not a steep multiband crossover.

## Modulation

**Modulation** in the header opens a full page: sources on the left, the matrix and
the macros on the right. Close or Escape returns to the grid.

### Sources

Tabs select the LFO, the envelope follower and the eight-step sequencer, each with
a live display:

- **LFO**: Sine, Triangle, Saw up, Saw down, Square or Random (a new held value
  each cycle), at a free rate or a host-tempo division.
- **Envelope**: follows the level where it listens (**Listen at**: the grid input after
  IN, or after any slot, e.g. after the amp). **Threshold** ignores everything below it,
  such as pick noise or quiet playing; **Sensitivity** sets how fast the output rises
  above it; **Curve** bends the response (positive reacts sooner, negative only to louder
  playing); **Attack** and **Release** set its speed. The display shows the level in grey
  on a dBFS scale with the threshold line, and the output in colour.
- **Sequencer**: drag across the eight bars to draw steps; above the centre line
  adds, below subtracts. Free rate or a tempo division.
- **Macros 1-4**: manual or host-automated knobs below the matrix.

The LFO and sequencer swing both ways around a knob's setting; the envelope and
macros only add (or, with negative depth, only subtract). For ducking, use the
Envelope with a negative depth, e.g. on a delay or reverb Mix: the effect drops while
you play and returns over the Release time.

**Retrigger.** The LFO and the sequencer run freely, or restart on each **Audio onset**
(a played note, heard where the Envelope listens and above its threshold) or on each
**MIDI note** sent to the plugin. The plugin has a MIDI note input for this; notes do
not change the audio. With **One shot**, the LFO runs one cycle per retrigger and then
holds its end, so Saw down becomes a decay for each note.

### Matrix

Eight rows, each with a source, a destination, a depth and an optional
**Via** source that scales the depth (from none at its minimum to all of it;
the LFO and sequencer are centred, so their midpoint gives half). Destinations are
any continuous knob of the module in a slot, listed with that module's own labels,
plus four grid controls: Blend A / B, the two lane levels and the split frequency.
Depth is a share of the destination knob's range, so frequency and time knobs move
along the same curve as when you turn them. Rows on the same knob add and stay within
its range. A row whose knob the slot's current module does not use is shown as such
and has no effect until a module uses it again. The × button clears a row.

**At the knob.** Right-click any module knob, a lane level, the split frequency or
the main A / B knob and choose **Modulate with** a source; it takes the next free row
at +25%. **Remove modulation** clears the knob's rows. A modulated knob shows a ring
in its source's colour covering the reach of all its rows, with a dot at the live
value while audio plays. Alt/Option-drag a modulated knob to change its first row's
depth (Shift for fine steps). Grid cards show a dot per source modulating them.

The engine processes bounded 16-sample control intervals and preserves that clock
across variable host blocks. Retriggers land on their own sample, every note-on of a
block counts, and held values update at the next interval. Destinations use
their own smoothing, so jumps in time controls such as delay time are heard as pitch or
steps. Sources run only while a row is active, and reset on host reactivation. Tempo
sync controls periods; these sources are not PPQ-position-locked. Host parameter values
remain the base settings; modulation does not write host automation. Row sources,
destinations and Via, the LFO Shape, Listen at and the Retrigger modes are saved state,
like algorithm selectors; depths, macros and the other source settings are automatable.

## Performance scenes

The header buttons **1 2 3** are three scenes. Click one to recall it; right-click to
store the current settings there or clear it. Stored scenes are outlined in the accent
colour and the last stored or recalled one is filled; empty scenes are grey and do
nothing when clicked.

Scenes include gains, bypass, EQ, dynamics, effects controls, routing, modulation
sources and all matrix rows. They retain
the current model paths, module types, quality settings and tuner preferences.
Hold capture switches/audio are excluded. Store overwrites the chosen scene.
Scene values persist with the project and malformed scene data rejects before
changing state. Recall informs the host of changed parameters.

Scenes are performance settings, not alternate complete rigs. This avoids synchronous
model replacement and gives the existing effect histories continuity during recall.
Scenes belong to the current rig: a rig preset saves its three scenes, and loading a
preset replaces them with its own (empty if it stored none). To switch between rigs
with different modules or captures, use rig presets.

## Processing and state

Storage is allocated/reset outside audio processing. Capture processors swap
preallocated banks at boundaries rather than copy entire multi-second windows.
Stereo-linked processors run once on both channels; the mono NAM policy still
uses one downmixed inference pass. New module preparation uses the existing
publish/retire handoff. No audio-thread mutex or new runtime dependency was added.

Main's finite host render-tail policy remains capped at 60 seconds. An enabled
Freeze with Hold active instead reports an infinite tail; releasing Hold restores
the finite policy. Bypassing Freeze retains the held pad for re-enabling. Bypassed
delays stop feedback while draining; Dual Delay reserves 17 seconds to clear its
maximum 16-second history. Intentional delay/pitch-window behavior is separate
from the conversion latency reported to the host.

Cabinet IR drains 8,192 silent frames after its bypass fade, covering the
convolver's maximum resampled FIR length before processing suspends. Re-enabling
after drainage cannot replay earlier input. Re-enabling sooner can resume its
remaining history during the ordinary fade. Stutter's 33-second drain covers four
maximum eight-second synced windows; a Decay setting is not a hard reverb end time.

Independent dynamics and envelope followers can change the stereo balance of
asymmetric input; fixed EQ/filter histories are also independent. The Peak Limiter
uses linked gain. No new stereo-link option or nonlinear oversampling is implied.

Capture and delay history storage grows with sample rate and the number of
loaded slots. Histories are prepared off audio and are not resized during live
processing. Compatibility and state handling are described in
[development.md](development.md).

[Rig presets and global gain placement](presets.md) describe file Save/Load and
the IN / A-B / OUT controls. Scenes remain parameter-only snapshots within a rig.
