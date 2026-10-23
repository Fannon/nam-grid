# nam-grid: fixed 4x4 signal chain

The current plugin has 16 slots arranged as four serial input stages, two
parallel four-slot lanes and four serial output stages. Build outputs are in
`build-grid/out`; see [build.md](build.md) for dependencies and commands.

Artifacts are `nam-grid.clap` / `nam-grid.vst3`; plugin identity and compatibility
are described in [build.md](build.md) and [development.md](development.md#parameter-and-project-compatibility).

## Routing and controls

```text
                                  +-- A1 -> A2 -> A3 -> A4 --+
IN -> Pre1 -> Pre2 -> Pre3 -> Pre4 -+                         +- Post1 -> Post2 -> Post3 -> Post4 -> OUT
                                  +-- B1 -> B2 -> B3 -> B4 --+
                                             blend
```

Every numbered position accepts a mono NAM capture (amp, rig, pedal, outboard or cabinet),
or a [built-in effect](modules.md): dynamics, EQ, cabinet IR, boost, filter, swell, tape,
modulation, delay, spring reverb or Wah. NAM slots have input gain,
output trim, Full/Lite where available, and bypass; built-ins expose at most seven knobs and Enabled. Empty slots pass
audio; their gain controls take effect when a model is loaded. Slots are processed left to
right within each row. Both parallel rows receive the same output from Pre4. There are no
cross-connections, sends, feedback, drag/drop routing, or routing graph.

- Click **+ Add module** once on an empty card. Click a loaded card to select its
  bottom editor; double-click it or use **Change module...** to replace it.
  Every FX lives in a category; indexed NAM captures and cabinet IRs have separate sections.
  `<` / `>` in the module editor traverse the library across configured folders.
- Right-click a card to Clear, Copy/Move/Swap with a destination. Tall mono-sum input/output meters
  appear beside active, loaded cards; gate/compressor reduction is shown in the selected module panel.
  See [development.md](development.md).
- Add NAM and IR folders in **Library & settings**. Multiple source folders and their subfolders
  become nested selection menus. Folders and editor preferences for new instances are stored in
  `nam-grid/library.json` in the user's application-support folder.
- **Blend A / B** is the percentage of lane B: 0% uses A, 50% averages both, 100% uses B.
  It uses a smoothed linear crossfade. Two identical or empty lanes remain unity instead of
  becoming 6 dB louder. To use a single modeled lane without mixing in the other lane's dry
  signal, turn the blend fully toward the modeled lane.
- The header preset name opens file Save/Load and presets in the last used folder;
  its arrows step through that folder. Global IN is at the input, OUT at the
  output, and A/B on the right. See [presets.md](presets.md).
- There is no shared gate, EQ or IR processing: add those as modules. Only OUT and a 5 Hz
  DC blocker follow the grid.
- Every slot has the same controls. NAM slots use the capture's raw output level with
  independent In and Trim gains. Full/Lite quality is prepared outside audio and saved per slot.
- Stereo inputs remain stereo through empty and built-in slots. Mono input is duplicated.
  Each loaded mono NAM slot averages L/R, performs one inference pass, and broadcasts its
  wet result. Its bypass retains separate L/R and conversion latency. Use Utility after
  the last NAM to pan/adjust width; before a mono NAM, width is collapsed.
  Mono host output averages L/R.

Utility can adjust level, balance, Stereo width and polarity in either lane or after the merge.
Built-in effects and mono cabinet IRs maintain independent per-channel histories. They
preserve stereo placement; delays, Chorus and reverbs also generate stereo movement.
Fine timing alignment and per-slot advanced calibration remain future work.

## Implementation and state

`FixedNAMGrid.h` owns 16 existing `NAMStage` objects, two pairs of preallocated lane buffers, and independent L/R
alignment delays. It does not duplicate inference. Host latency is:

```text
sum(Pre) + max(sum(A), sum(B)) + sum(Post)
```

The shorter lane is delayed to match the longer lane. Settled model bypass preserves its
conversion latency. Lane alignment also remains at blend endpoints, keeping host latency
stable while automating the blend. This compensates reported NAM sample-rate conversion
latency; it cannot eliminate phase differences or residual capture delays inherent in two
different amp/cab/microphone captures. Parallel combinations still need auditioning.

Model parsing, prewarming, allocation, destruction and directory scanning remain outside audio.
Same-latency loads retain the stage's short fade-out/swap/fade-in. A CLAP load/clear that changes
either lane's delay requests host reactivation even when the total host latency is unchanged.
VST3 retains upstream latency notification with bounded, preallocated alignment histories and
short tap crossfades for dynamic adoption. VST3 host behavior is not verified.

Alignment histories normally reserve about 2 MiB total. Reset increases capacity if existing
models require a larger bound at the new host rate. New models whose individual conversion
delay exceeds 16,384 samples are rejected before publication. This avoids audio-thread resizing.
Empty slots skip per-sample work once switching has settled. Both lanes run; the blend endpoints
do not automatically suspend a loaded lane. Explicitly bypass unused models to skip inference.

Legacy Buffer-based NAM models with zero prewarm samples, such as Linear, receive a silence
block during preparation to reserve lazily allocated audio buffers off the audio thread.

Grid state stores slot paths, module types, named values, content identities and
learned profiles. Validation precedes mutation; missing files retain their paths
while that slot passes audio. See [implementation and compatibility](development.md)
for the source of truth and preview compatibility policy.

## Verification

Use the [build guide](build.md) for automated checks and the
[FX benchmark](fx-performance.md) to measure a current rig. Release acceptance
is tracked in the [release checklist](release-checklist.md).

## Manual DAW check

1. Add `build-grid/out` to Bitwig's plugin locations and load **nam-grid**.
2. Add NAM and IR folders in Library & settings; load a pedal into Pre 1 and different full rigs into A1/B1.
   Use no cabinet module for full rigs. Sweep Blend A/B and check unity/phase by ear.
3. Load and bypass captures in all four rows. Verify that output-row changes affect both lanes.
   Listen to model loads, bypass and blend automation under sustained input.
4. Save/reopen the project and verify all 16 selections and controls. Move a file, recall, and
   relink it. Check dropdowns, OS file/folder pickers, labels and controls at normal/scaled sizes.
5. Change host rates and block sizes; check restart handling, delay compensation, CPU and underruns.
   Repeat on macOS and in a VST3 host before claiming support there.

## Build

Follow [build.md](build.md) for dependency setup, both plugin formats and the
complete test-target list. Keep build commands there so this routing guide does
not maintain a second copy.
