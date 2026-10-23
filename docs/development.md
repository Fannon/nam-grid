# Plugin implementation and compatibility

Use [build.md](build.md) for build, test and screenshot commands,
[grid.md](grid.md) for routing, [ui.md](ui.md) for editor behavior and
[modules.md](modules.md) for effect controls.

## Sources of truth

| Responsibility | Source |
| --- | --- |
| Stable host-ID allocation and ownership | [ParameterLayout.h](../NeuralAmpModeler/ParameterLayout.h) |
| Parameter descriptors, ranges and choices | [EffectParameters.h](../NeuralAmpModeler/EffectParameters.h) |
| Effect labels, tooltips and conditional controls | [EffectPresentation.h](../NeuralAmpModeler/EffectPresentation.h) |
| Insertion/reset settings | [SlotDefaults.h](../NeuralAmpModeler/SlotDefaults.h) |
| Browser categories | [ModuleCatalog.h](../NeuralAmpModeler/ModuleCatalog.h) |
| Host registration, state validation and migrations | [NeuralAmpModeler.cpp](../NeuralAmpModeler/NeuralAmpModeler.cpp) |
| Slot transactions | [SlotOperations.h](../NeuralAmpModeler/SlotOperations.h) |
| Prepared processor lifetime | [NAMStage.h](../NeuralAmpModeler/NAMStage.h), [RealtimeExchange.h](../NeuralAmpModeler/RealtimeExchange.h) |

Keep exact schema versions, bank strides and total parameter counts in source,
rather than copying them into several guides. Stored rigs use named values;
unsupported or malformed data must be rejected before mutating the current rig.

## Parameter and project compatibility

This change establishes a new automation compatibility baseline. Prototype host
parameter IDs are intentionally reorganized; older preview automation envelopes
must be recreated. The plugin identities are retained, but prototype automation
is not migrated. The contract below applies from this baseline onward.

`ParameterLayout.h` places the fixed NAM/core controls and globals first, then
assigns each extendable FX control a block of slot IDs. Adding a control appends a block; its ID never depends on the total
number of controls in a slot. The same forward/reverse mapping drives audio-cache
ownership and state handling. No hidden/dummy parameters or prototype-stride
exceptions are needed. New global/performance controls need explicit extension
allocation; compile-time checks prevent shifting the FX base.

Algorithm selection is saved state, not automation. The editor, rig presets,
projects and performance scenes retain native integer identities. Algorithm
selectors keep their parameter handles for state/editor compatibility but have
no CLAP/VST3 automation flag and emit no automation gestures or values. Editor
changes mark the host project dirty. Shared controls such as Drive, Decay and
Mix retain automation, as do tempo divisions, bypass, ping-pong, stereo switches
and rotary speed.

This covers Distortion Style, Cosmic Echo Style, Console Colour Desk, Digital
Black Mode, Multi-head Tape patterns, Juno Mode, Noise Gate Mode, Filter Mode,
Parametric EQ Bell/Notch and Uni-Vibe Chorus/Vibrato. Other reverbs and modules
are selected through the existing state-only module chooser. Unknown or
fractional algorithm IDs in presets/scenes are rejected without changing the rig.

When adding a mode or module:

1. Append an explicit permanent ID; never reorder, renumber or reuse old IDs.
   Register algorithm keys in `AlgorithmIdentities()`, and keep old DSP
   implementations. Display labels can change independently of identity.
2. Expose only implemented choices. Growing a non-automatable algorithm menu
   does not reinterpret saved VST3 normalized automation positions.
3. Append new `UtilityParam` entries and descriptors. Older presets default only
   missing appended keys according to each descriptor's `introducedIn` version.
   Bump `kEffectStateSchema` for new controls and set that version on their
   descriptors; missing fields that should already exist remain invalid. Keep
   existing descriptor `name` values as permanent storage keys; use `label` for editor wording. Never change an existing
   automatable control's range, scaling or meaning to accommodate a new mode.
4. If an old algorithm's behavior must materially change, introduce a new mode
   identity so existing rigs retain their sound.
5. Run the compatibility tests and the new algorithm's DSP/state tests.

`nam-parameter-contract` checks every original parameter ID and sampled
normalized-to-display mapping, native mode/scene recall, module identities and
atomic rejection. It also simulates future control allocation without exposing
placeholders. Windows builds add `nam-vst3-parameter-contract`, which loads the
real VST3 controller to verify IDs, automation flags and normalized mappings.
Native CLAP editor tests verify state notifications without mode automation.
The historical fixture in `tests/fixtures/` must not be regenerated to bypass
failures: adding new modes/controls preserves its existing expectations. Its
explicit `--record` option is only for establishing a reviewed baseline.

The fixture records the new stable layout, rather than preserving a prototype
layout. Older named-state migrations remain best effort because retaining those
readers is inexpensive; they are outside the new compatibility guarantee. Mode
automation from earlier previews is intentionally unsupported. Metadata checks
do not prove identical DSP output; sound changes still need DSP regressions and the [release acceptance checks](release-checklist.md).

Changes to a module must keep its menu membership, descriptors, defaults,
presentation, DSP dispatch, state handling and tail reporting consistent.
Tests should cover insertion, replacement, bypass, automation, recall and the
first/last slot banks where applicable.

## Slot preparation and audio processing

File parsing, allocation, DSP preparation, replacement and retirement happen
outside the audio callback. Copy/Move/Swap prepares both sides before committing;
invalid or missing resources leave the slots unchanged. Automation remains
attached to physical slots. Captures and IRs are external files, not copied assets.

Model changes that alter CLAP latency request host reactivation. Same-latency
replacement uses the existing switching fades. A2 quality changes reload and
prepare the selected submodel on the control thread; Core's size setter must
not be called from audio processing. Rapid quality requests can coalesce.

EQ uses AudioDSPTools shelf/peak coefficients and RBJ cut-filter equations;
the attribution is in the shipped notices. Slot EQ smooths controls over 5 ms
and updates coefficients on a continuous 16-sample schedule. Cut bypasses ramp,
histories reset explicitly, and processing adds no buffering latency.

Meters use bounded SPSC packets. Rendering, logarithmic display scaling and
holds run on the UI thread; meter state is transient. Keep visualization work
out of audio processing, and distinguish measured signals from parameter sketches.

## Cabinet convolution

Grid and legacy cabinet IRs use one [adaptive direct/FFT processor](adaptive-ir.md).
It selects the backend during preparation and preserves the full resampled IR
without adding buffering latency or exposing a quality mode.

## DSP sources and adaptations

Preserve source pins, licenses and deliberate deviations beside the adapted DSP:

- [Reverb cores](../NeuralAmpModeler/dsp/reverb/README.md) and
  [Cosmic Echo](../NeuralAmpModeler/dsp/reverb/COSMIC.md).
- [Airwindows outboard mappings](airwindows-outboard.md) and
  [gate/compressor adaptations](airwindows-dynamics.md).
- [Third-party notices](../NeuralAmpModeler/installer/ThirdPartyNotices.txt)
  and accompanying license files. Vendor generators live in `scripts/`.

Completed investigations and old benchmark results are available in Git history.
Keep maintained guides focused on current behavior and reproducible checks.
