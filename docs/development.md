# Plugin implementation and compatibility

Use [build.md](build.md) for build, test and screenshot commands,
[grid.md](grid.md) for routing, [ui.md](ui.md) for editor behavior and
[modules.md](modules.md) for effect controls.

## Sources of truth

| Responsibility | Source |
| --- | --- |
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

This preview does not guarantee compatibility with every unreleased project or
automation mapping. Existing CLAP/VST3 plugin identities are retained. Older
named-state readers and migrations remain best effort; consult the state loader
for accepted schemas and aliases.

Expose only implemented enum choices. iPlug2 normalizes VST3 enum positions using
`index / (count - 1)`, so adding choices can change old automation positions.
Reserving internal storage or IDs does not reserve those normalized positions.
Before a stable release, freeze parameter IDs/ranges or define a versioning
strategy. See the [release checklist](release-checklist.md).

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
