# nam-grid: fixed 4x4 signal chain

The current plugin has 16 slots arranged as four serial input stages, two
parallel four-slot lanes and four serial output stages. Build outputs are in
`build-grid/out`; see [BUILD.md](BUILD.md) for dependencies and commands.

The grid plugin is named **nam-grid** (`NGr1`), distinct from the dual prototype (`NDu1`).
Artifacts are `nam-grid.clap` / `nam-grid.vst3`. This unreleased prototype does not guarantee
old parameter-name/ID or project compatibility. Pre/Post labels replace the former IN/OUT rows. Remove older copies of the grid binary from
scan paths when adopting the renamed binary, then rescan. This workspace archives its previous
grid artifacts outside `build-grid/out`. Windows builds are copied to the system CLAP folder on request; restart the host to load replacements.

## Routing and controls

```text
                                  +-- A1 -> A2 -> A3 -> A4 --+
IN -> Pre1 -> Pre2 -> Pre3 -> Pre4 -+                         +- Post1 -> Post2 -> Post3 -> Post4 -> OUT
                                  +-- B1 -> B2 -> B3 -> B4 --+
                                             blend
```

Every numbered position accepts a mono NAM capture (amp, rig, pedal, outboard or cabinet),
or a [built-in effect](MODULES.md): dynamics, EQ, cabinet IR, boost, filter, swell, tape,
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
  See [SLOTS.md](SLOTS.md).
- Add NAM and IR folders in **Library & settings**. Multiple source folders and their subfolders
  become nested selection menus. The default preference file remains `NAMDualPrototype/library.json`.
- **Blend A / B** is the percentage of lane B: 0% uses A, 50% averages both, 100% uses B.
  It uses a smoothed linear crossfade. Two identical or empty lanes remain unity instead of
  becoming 6 dB louder. To use a single modeled lane without mixing in the other lane's dry
  signal, turn the blend fully toward the modeled lane.
- The header preset name opens file Save/Load and presets in the last used folder;
  its arrows step through that folder. Global IN is at the input, OUT at the
  output, and A/B on the right. See [PRESETS.md](PRESETS.md).
- New instances have no shared gate, EQ or IR processing: add those as modules. Master and the
  existing 5 Hz DC blocker remain after the grid. Recalled projects retain their legacy global
  processing settings, with compatibility toggles in Settings when used; this preserves their sound.
- The former Amp's optional calibration, output mode and Slim parameters apply to **Pre 2**,
  Settings labels identify Pre 2. Other slots use raw NAM levels with
  independent musical input gains/trims. Full/Lite is prepared outside audio and saved per slot.
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

The layout has **3,099 parameters**, including one contiguous bank of 173 FX
controls per slot. Prototype host IDs, names and enum ranges are not frozen.
`###NAMGrid-v1###` / schema 23 stores all 16 paths, types, named values, content
identities and Noise Gate (Smart) profiles. Validation
precedes mutation; missing files retain their paths while that slot passes audio.
Some older-format readers remain, but Pre/Post parameter-name changes mean old
unreleased projects may require rebuilding. No compatibility guarantee is made.
The separate grid plugin ID does not automatically replace the former dual plugin.


## Verification

Windows x64 Release CLAP and VST3 build successfully. The five CTest suites are `capture-library`, `wav-decoder`, `resampling`, `stages` and `clap-smoke`.
Current module coverage and limitations are in [MODULES.md](MODULES.md).

- Library scanning/search/classification and nested menu selection tests remain intact.
- All 16 positions use distinct noncommuting affine fixtures to check serial order, independent
  gain/trim, selective bypass, and split/blend/rejoin order at both blend endpoints and a mixture.
- Empty-lane unity holds during blend changes. Four synthetic A2 processors across input/A/B/output
  match independent references with variable 1/3/64/127/256/512-frame blocks at 48/44.1/96 kHz.
- Unequal-lane impulse tests check physical alignment, preserved bypass latency, and hosts whose
  maximum block size is one sample. Tested grid paths observe no C++ allocations/deletions and
  pass Eigen's allocation guard.
- CLAP covers all 16 model/effect selections and 2,490 parameter values through save/reload, old-state
  migration, invalid later-slot state, missing files, bypassed slots, mono/stereo port layouts,
  and full-grid processing at all three rates. The synthetic NAM fixture grid measures 0/324/504 conversion-latency samples;
  these figures describe those models/rates, not a fixed latency for all modules.
- A hidden native Windows editor test exercises IN1/IN2/A1/B1/OUT4 next buttons, library search,
  state-dirty notification, editor teardown, and a shorter-lane load that changes alignment while
  leaving total host latency unchanged. The latter correctly requests CLAP restart/reactivation.
- A read-only DSP probe loaded four real user exports: Avalon AD2022 in IN1, Vox AC30 in A1,
  Fender Deluxe in B1, and UA 610B in OUT1. Variable-block 48 kHz processing produced finite,
  nonzero audio without observed audio-path allocations. Captures were not modified or copied.

These are offline/harness results. No listening evaluation, Bitwig session, macOS build, VST3
host validation, full visual layout review, or general race detector run was performed.

### Historical CPU and latency (schema-7 checkpoint)

These recorded timings predate the stereo/Spring/Wah changes; they are retained for
comparison and were not remeasured for the current build. For later effect measurements,
see [MODULES.md](MODULES.md).

Ryzen 9 5950X, Windows x64 Release, 128-frame blocks. Same two deterministic A2 nano/standard
fixtures and benchmark procedure as the dual checkpoint: 128,000 samples per render, one warm-up,
fastest of three timed renders. Gate/EQ/IR/calibration are disabled, with unity gains. The 16-model
case alternates the two fixtures across all 16 slots and blends the parallel lanes equally.
Measurements below were taken sequentially with builds/tests idle. CPU is elapsed time divided
by audio duration, expressed as a percentage of one core, not Bitwig's DSP meter.

| Host rate | Dual checkpoint, 2 models | Grid, 2 models | Grid, 16 models | Grid latency, 2 / 16 models |
| --- | ---: | ---: | ---: | --- |
| 48 kHz | 6.99% | 7.34% | 57.48% | 0 / 0 samples |
| 44.1 kHz | 8.03% | 7.93% | 64.64% | 54 / 324 samples |
| 96 kHz | 8.45% | 8.52% | 69.00% | 84 / 504 samples |

Two loaded models remain close to the checkpoint cost, with variation across runs/rates.
Loading all 16 adds substantial inference work; parallel signal lanes run on the same audio
thread, not on extra CPU worker threads. These are synthetic offline timings, not a low-buffer
underrun test or a performance guarantee for arbitrary captures. No extra fixed algorithmic
latency is introduced when models run at the host rate.

To reproduce, use `nam-clap-bench <plugin.clap> dual 1 <drive.nam> <amp.nam>` for two models or
`nam-clap-bench <grid.clap> grid 1 <drive.nam> <amp.nam>` for all 16, using the synthetic fixtures
created by the CLAP smoke test. The benchmark can read both checkpoint and grid state formats.

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

Follow [BUILD.md](BUILD.md) for dependency setup, both plugin formats and the
complete test-target list. Keep build commands there so this routing guide does
not maintain a second copy.
