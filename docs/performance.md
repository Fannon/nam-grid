# Performance controls

Use **Grid / Control** for routing, shared modulation, macros and scenes.
See [modules.md](modules.md) for effect controls and [presets.md](presets.md) for complete rigs.

## Grid / Control

Open **Grid / Control** in the header. This full-page panel has Routing,
Modulation and Scenes tabs and closes with Close or Escape.

### Routing

- Independent A/B levels, mute and solo. Solo preserves the current blend level;
  enabling both solos passes both lanes, while mute takes precedence.
- **Lane timing**: positive values delay A, negative values delay B, up to 20 ms.
  It never advances live audio. This is additional to the existing conversion-delay
  compensation and helps audition relative capture timing, not correct every phase difference.
- Optional complementary low/high frequency split: A receives the low band and
  B the residual high band. At centered Blend, neutral empty lanes reconstruct the
  original signal, including while the crossover fades on/off. Blend away from
  center changes the relative band levels. The split is a first-order complementary
  filter, not a steep multiband crossover.
- Two Macro knobs with assignments on the Modulation page.

### Shared modulation

One sine LFO, one clean-input envelope follower and one eight-step sequencer can
each target a slot/parameter. Each has signed depth. The LFO and sequencer support
free rate or the existing host-tempo divisions. The follower exposes sensitivity,
attack and release. The eight step values are edited below the source controls.

Targets are Wah position, envelope-filter cutoff, Utility balance, delay mix,
reverb mix (including Spring), NAM trim, parallel Blend and ring frequency.
Choose a relevant target for the selected module; a Wah assignment does not
turn another module into a Wah. Two macros provide additional manual/host-automated
assignments using the same target list. Multiple assignments add and are bounded
to the destination's musical range. Depth zero or target None disables an assignment.

The engine processes bounded 16-sample control intervals and preserves that clock
across variable host blocks. Destinations use their existing smoothing. Shared
sources run while assignments are active, and reset on host reactivation. Tempo
sync controls periods; these sources are not PPQ-position-locked sequencers.
Host parameter values remain the base settings; modulation does not write host automation.

### Performance scenes

Store and recall three sets of controls. Scenes include gains, bypass, EQ,
dynamics, effects controls, routing and modulation assignments. They retain
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
