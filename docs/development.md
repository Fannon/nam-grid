# Plugin implementation and compatibility

Use [build.md](build.md) for build, test and screenshot commands,
[grid.md](grid.md) for routing, [ui.md](ui.md) for editor behavior and
the [module guide](modules/README.md) for effect controls.

## Sources of truth

| Responsibility | Source |
| --- | --- |
| Stable host-ID allocation and ownership | [ParameterLayout.h](../NeuralAmpModeler/ParameterLayout.h) |
| Parameter descriptors, ranges and choices | [EffectParameters.h](../NeuralAmpModeler/EffectParameters.h) |
| Effect labels, tooltips and conditional controls | [EffectPresentation.h](../NeuralAmpModeler/EffectPresentation.h) |
| Insertion/reset settings | [SlotDefaults.h](../NeuralAmpModeler/SlotDefaults.h) |
| Browser categories | [ModuleCatalog.h](../NeuralAmpModeler/ModuleCatalog.h) |
| Host registration and state validation | [NeuralAmpModeler.cpp](../NeuralAmpModeler/NeuralAmpModeler.cpp) |
| Slot transactions | [SlotOperations.h](../NeuralAmpModeler/SlotOperations.h) |
| Prepared processor lifetime | [NAMStage.h](../NeuralAmpModeler/NAMStage.h), [RealtimeExchange.h](../NeuralAmpModeler/RealtimeExchange.h) |

Keep exact schema versions, bank strides and total parameter counts in source,
rather than copying them into several guides. Stored rigs use named values;
unsupported or malformed data must be rejected before mutating the current rig.

## Parameter and project compatibility

NAM Grid 0.1.0 is the compatibility baseline. Earlier previews used other plugin
IDs, parameter IDs and state formats; their projects, automation and `.ngrid`
presets are not loaded and must be rebuilt. From 0.1.0 onward, these remain stable:

- **Plugin identity:** CLAP ID, vendor and VST3 class IDs in `config.h`.
- **Host parameters:** every ID, name, range, scaling and meaning. Automatable lists are
  frozen too (the ten tempo divisions); lists that may grow, such as algorithm modes, the
  LFO shape and matrix routing, are saved state without host automation.
- **Module and algorithm IDs:** the integers saved for modules (`SlotEffect`)
  and algorithm selectors (`AlgorithmIdentities()`).
- **Saved state:** the `###nam-grid###` header and every named field of schema 1. Later
  schemas only add fields (schema 2 adds Digital Black Hold); older states load with their defaults.

### Host parameter layout

Host IDs are allocated in append-only segments in
[ParameterLayout.h](../NeuralAmpModeler/ParameterLayout.h). The 0.1.0 segment holds the
globals (Output, A/B Blend, the tuner, then the `grid_performance` controls: Input, lane
routing, the LFO, envelope and sequencer, Macros 1–4 and the eight modulation matrix rows),
followed by one block of 16 IDs per slot control: the 17 shared slot controls (NAM Input,
NAM Output, Enabled, gate, compressor, EQ, IR Output, Full Quality), then every module
control (`UtilityParam`). Every slot has the same controls and names (`Pre 1 NAM Input` …
`Post 4 Limiter Clip`). The segment counts are literals: adding a control fails to compile
until it is allocated, and a compile-time check rejects edits to released segments.

- **New module or shared slot control:** append the `UtilityParam`/descriptor and
  append a `{0, n}` segment (or grow it, once it is the last segment). Its 16 IDs
  come after every existing ID. Matrix destinations store the slot-control index,
  so new controls are modulatable without new matrix parameters.
- **New global:** append `{n, 0}` and access it through `parameter_layout::GlobalID()`.
  Never use contiguous arithmetic across globals; `PerformanceParam()` shows the pattern.

### Saved state

Projects and presets store named values, module IDs, file paths/hashes and Noise
Gate (Smart) profiles under the state header with an integer `schema`. Newer
schemas, unknown modules or algorithms, fractional or out-of-range IDs, missing
fields and malformed data are rejected before the current rig changes.
Per-user preferences, including the library folders, are never part of a state: they live in
`settings.json` (see [library folders](browser.md#library-folders)), and loading a state re-reads
them. Every test or tool that loads the plugin uses `IsolatedUserHome` so it never touches a
developer's real settings or library.

When adding saved data, bump `kEffectStateSchema` and set the new control
descriptor's `introducedIn` to that schema. Older states then receive that
control's default; a field that the saved schema already had stays required.
Never lower or reuse a schema number.

Modulation matrix rows ([GridPerformance.h](../NeuralAmpModeler/GridPerformance.h),
[ModulationMatrix.h](../NeuralAmpModeler/ModulationMatrix.h)) follow the algorithm-selector
rule: Source, Slot, Destination and Via, the LFO Shape, the envelope's Listen at and the Retrigger
modes are saved state without automation flags (`IsStateOnlyParam`), so their lists
can grow; Depth is automatable. MIDI input exists only to retrigger modulation; CMake
sets `VST3_NUM_CC_CHANS=0` so VST3 adds no MIDI-CC proxy parameters. Destinations store
slot-control indices. `grid_modulation::VisitSlotControls` maps every slot control to
its audio-snapshot field for both the host refresh and the matrix, so a new snapshot
field must be added there; the stage tests check that each control is mapped exactly once.

Editor open, state restore and parameter reset push values to the UI in one pass over
the controls (`_SendAllParamValuesToUI`, `OnParamReset`). iPlug2's defaults handle one
parameter at a time, scanning every control and refreshing every slot card per
parameter, which took seconds at NAM Grid's size. Bulk updates should use the same
single pass instead of looping `SendParameterValueFromDelegate`.

### Adding a mode or module

1. Append an explicit permanent ID; never reorder, renumber or reuse old IDs.
   Register algorithm keys in `AlgorithmIdentities()`, and keep old DSP
   implementations. Display labels can change independently of identity.
2. Expose only implemented choices. Algorithm selectors are saved state, not
   automation: they have no CLAP/VST3 automation flag and editor changes mark the
   project dirty instead. Growing an algorithm menu therefore never reinterprets
   saved automation.
3. Keep existing descriptor `name` values; they are host names and storage keys.
   Use `label` for editor wording. Never change an existing automatable control's
   range, scaling or meaning to accommodate a new mode.
4. If an algorithm's behavior must materially change, add a new mode identity so
   existing rigs keep their sound.
5. Run the compatibility tests and the new algorithm's DSP/state tests.

`nam-parameter-contract` compares the running plugin against
[`tests/fixtures/parameter-contract-v1.json`](../tests/fixtures/parameter-contract-v1.json):
plugin identity, every slot and global parameter (ID, name, flags, default and
sampled normalized-to-display mappings), module and algorithm IDs, preset/scene
recall of every algorithm and atomic rejection. It also simulates future segment
growth. Windows builds add `nam-vst3-parameter-contract`, which loads the real VST3
controller and checks its class ID. Appending controls preserves the fixture's
expectations; never re-record it to bypass a failure. `--record` exists only for
establishing a reviewed baseline. Metadata checks do not prove identical DSP output;
sound changes still need DSP regressions and the
[release acceptance checks](release-checklist.md).

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
replacement uses the existing switching fades. Lane timing is compensated the
same way: the shorter lane is delayed to match the longer one, so the reported
host latency is the Pre sum plus the longer lane plus the Post sum. Legacy
Buffer-based NAM models with zero prewarm samples receive a silence block
during preparation so their lazily allocated buffers are reserved in time.

The plugin owns two grids. Slot edits go to the edited grid, which is the audible
one except during a rig preset switch. `RigTransition` coordinates a switch: the
control thread asks audio to hold the audible rig's controls (slot banks, routing,
blend, IN/OUT), stages the new rig into the other grid, resets that grid and starts
the switch. Audio freezes the outgoing rig's last controls and either dips or
crossfades into the new grid. OnIdle frees the previous grid once audio reports
idle. If audio does not acknowledge the hold within 250 ms, the rig is applied to
the audible grid directly, as for host state loads. A2 quality changes reload and
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

Cabinet IR slots use one adaptive direct/FFT processor; its rules are documented in
`NeuralAmpModeler/AdaptiveIR.h` and summarised for players in [cabinet IR processing](modules/ir-processing.md).
It selects the backend during preparation and preserves the full resampled IR
without adding buffering latency or exposing a quality mode.

## DSP sources and adaptations

Preserve source pins, licenses and deliberate deviations beside the adapted DSP:

- [Reverb cores](../NeuralAmpModeler/dsp/reverb/README.md) and
  [Cosmic Echo](../NeuralAmpModeler/dsp/reverb/COSMIC.md).
- [Airwindows outboard mappings](airwindows-outboard.md) and
  [gate/compressor adaptations](airwindows-dynamics.md).
- [Third-party notices](../ThirdPartyNotices.txt)
  and accompanying license files. Vendor generators live in `scripts/`.

Completed investigations and old benchmark results are available in Git history.
Keep maintained guides focused on current behavior and reproducible checks.
