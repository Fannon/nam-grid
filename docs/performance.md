# Performance controls

Use **Grid / Control** for routing, shared modulation, macros and scenes.
See [modules.md](modules.md) for effect controls and [presets.md](presets.md) for complete rigs.

## Grid / Control

Open **Grid / Control** in the header. This full-page panel has Routing,
Modulation and Scenes tabs beside its title and closes with Close or Escape.
Hover a control for its explanation.

### Routing

- **Lane A / Lane B**: level, mute and solo per lane, with the lane's modules listed.
  Solo preserves the current blend level; enabling both solos passes both lanes,
  while mute takes precedence. **Blend A / B** stays on the main page.
- **Lane timing**: positive values delay A, negative values delay B, up to 20 ms.
  It never advances live audio. This is additional to the existing conversion-delay
  compensation and helps audition relative capture timing, not correct every phase difference.
- **Frequency split**: optional complementary low/high split, drawn beside its controls.
  A receives the low band and B the residual high band. At centered Blend, neutral
  empty lanes reconstruct the original signal, including while the crossover fades
  on/off. Blend away from center changes the relative band levels. The split is a
  first-order complementary filter, not a steep multiband crossover.

### Modulation

The left side edits the sources; the right side is the modulation matrix.

**Sources.** Tabs select the LFO, the clean-input envelope follower and the
eight-step sequencer, each with a live display:

- **LFO**: Sine, Triangle, Saw up, Saw down, Square or Random (a new held value
  each cycle), at a free rate or a host-tempo division.
- **Envelope**: sensitivity, attack and release of a follower on the input after IN.
- **Sequencer**: drag across the eight bars to draw steps; above the centre line
  adds, below subtracts. Free rate or a tempo division.
- **Macros 1-4**: manual or host-automated knobs below the sources.

The LFO and sequencer swing both ways around a knob's setting; the envelope and
macros only add (or, with negative depth, only subtract).

**Matrix.** Eight rows, each with a source, a destination, a depth and an optional
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
across variable host blocks. Destinations use their own smoothing, so jumps in time
controls such as delay time are heard as pitch or steps. Sources run only while a
row is active, and reset on host reactivation. Tempo sync controls periods; these
sources are not PPQ-position-locked. Host parameter values remain the base settings;
modulation does not write host automation. Row sources, destinations and Via are
saved state, like algorithm selectors; depths, macros and source settings are
automatable.

Projects and presets from before the matrix had one fixed assignment per source
(eight targets such as Delay mix or NAM trim). They load as matrix rows with the
same sound. If host automation turns on such an assignment later, it still plays,
and the Modulation page offers **Convert to rows**.

### Performance scenes

Store and recall three sets of controls. Scenes include gains, bypass, EQ,
dynamics, effects controls, routing, modulation sources and all matrix rows. They retain
the current model paths, module types, quality settings and tuner preferences.
Hold capture switches/audio are excluded. Store overwrites the chosen scene.
Scene values persist with the project and malformed scene data rejects before
changing state. Recall informs the host of changed parameters.

Scenes are performance settings, not alternate complete rigs. This avoids synchronous
model replacement and gives the existing effect histories continuity during recall.

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
