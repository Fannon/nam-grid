# Fixed 4x4 NAM grid experiment

Branch: `experiment/fixed-4x4-grid`. The dual-model/browser prototype is preserved on
`prototype/dual-nam-clap` at `543d2b2`. Its local binaries remain in `build-native/out`;
the grid is built separately in `build-grid/out`. Core's two documented allocation patches
remain applied to the submodule, as before.

The grid plugin identifies as **NAM Grid Prototype** (`NGr1`), distinct from the dual prototype
(`NDu1`). Both builds retain the upstream binary filename, so keep their artifact directories
separate when scanning or installing them. Nothing was deployed to a system plugin folder.

## Routing and controls

```text
                           +-- A1 -> A2 -> A3 -> A4 --+
IN1 -> IN2 -> IN3 -> IN4 ---+                         +--- OUT1 -> OUT2 -> OUT3 -> OUT4
                           +-- B1 -> B2 -> B3 -> B4 --+
                                      blend
```

Every numbered position accepts a mono NAM capture (amp, rig, pedal, outboard or cabinet),
or a [built-in noise gate/compressor](EFFECTS.md). NAM slots have independent input gain,
output trim and bypass; built-ins expose three dedicated knobs and bypass. Empty slots pass
audio; their gain controls take effect when a model is loaded. Slots are processed left to
right within each row. Both parallel rows receive the same output from IN4. There are no
cross-connections, sends, feedback, drag/drop routing, or routing graph.

- Click a slot's model name to open nested capture-folder menus, choose an individual `.nam`
  file, select a noise gate/compressor, or clear the slot. `<` / `>` select NAM captures using
  the shared search/category filter.
- Choose the capture folder in Settings. The browser/default folder implementation is shared
  with the dual prototype; the default preference file remains `NAMDualPrototype/library.json`.
- **Blend A / B** is the percentage of lane B: 0% uses A, 50% averages both, 100% uses B.
  It uses a smoothed linear crossfade. Two identical or empty lanes remain unity instead of
  becoming 6 dB louder. To use a single modeled lane without mixing in the other lane's dry
  signal, turn the blend fully toward the modeled lane.
- Shared gate detection remains before the grid. Upstream gate attenuation, EQ, global WAV IR,
  DC blocker and master remain after the grid. The shared gate is separate from slot gates;
  turn it off when auditioning the new gate variants.
- The former Amp's optional calibration, output mode and Slim parameters apply to **IN2**,
  preserving old states. Settings labels identify IN2. Other slots currently use raw NAM levels
  with their independent musical input gains and trims. Slim remains host-automatable.
- The complete grid remains mono internally, with mono duplicated to stereo outputs as before.

This is a routing experiment with four simple dynamics variants. Per-slot IRs, stereo effects,
per-lane polarity/phase controls, movable slots and per-slot advanced calibration are not added.

## Implementation and state

`FixedNAMGrid.h` owns 16 existing `NAMStage` objects, two preallocated lane buffers, and two
alignment delays. It does not duplicate inference. Host latency is:

```text
sum(IN) + max(sum(A), sum(B)) + sum(OUT)
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

Alignment histories normally reserve about 1 MiB total. Reset increases capacity if existing
models require a larger bound at the new host rate. New models whose individual conversion
delay exceeds 16,384 samples are rejected before publication. This avoids audio-thread resizing.
Empty slots skip per-sample work once switching has settled. Both lanes run; the blend endpoints
do not automatically suspend a loaded lane. Explicitly bypass unused models to skip inference.

Legacy Buffer-based NAM models with zero prewarm samples, such as Linear, receive a silence
block during preparation to reserve lazily allocated audio buffers off the audio thread.

The original 18 parameter IDs remain mapped to the original two stages, now IN1 and IN2.
The other 14 slots append gain/trim/enable parameters, followed by the blend (61 IDs).
Slot dynamics append another 96 parameters, for 157 total. `###NAMGrid-v1###` / schema 3
stores all 16 file paths, effect types and named parameters; schema 2 still migrates. State validation
precedes mutation. Missing files retain their requested paths while clearing that slot's DSP.
Previous dual schema-1 states and upstream states migrate into IN1/IN2; other slots are empty,
and the parallel blend defaults to 50%. Since both lanes are then dry, migration preserves
the original series behavior. The previous dual plugin cannot read grid state.
The separate plugin ID means old DAW projects keep using the dual prototype; they are not
automatically replaced by the grid. The migration reader handles old state when supplied;
no standalone preset/state import UI was added.

## Verification

Windows x64 Release CLAP and VST3 build successfully. All three CTest suites pass:

- Library scanning/search/classification and nested menu selection tests remain intact.
- All 16 positions use distinct noncommuting affine fixtures to check serial order, independent
  gain/trim, selective bypass, and split/blend/rejoin order at both blend endpoints and a mixture.
- Empty-lane unity holds during blend changes. Four synthetic A2 processors across input/A/B/output
  match independent references with variable 1/3/64/127/256/512-frame blocks at 48/44.1/96 kHz.
- Unequal-lane impulse tests check physical alignment, preserved bypass latency, and hosts whose
  maximum block size is one sample. Tested grid paths observe no C++ allocations/deletions and
  pass Eigen's allocation guard.
- CLAP covers all 16 model/effect selections and 157 parameter values through save/reload, old-state
  migration, invalid later-slot state, missing files, bypassed slots, mono/stereo port layouts,
  and full-grid processing at all three rates. Full-grid conversion latency is 0/324/504 samples.
- A hidden native Windows editor test exercises IN1/IN2/A1/B1/OUT4 next buttons, library search,
  state-dirty notification, editor teardown, and a shorter-lane load that changes alignment while
  leaving total host latency unchanged. The latter correctly requests CLAP restart/reactivation.
- A read-only DSP probe loaded four real user exports: Avalon AD2022 in IN1, Vox AC30 in A1,
  Fender Deluxe in B1, and UA 610B in OUT1. Variable-block 48 kHz processing produced finite,
  nonzero audio without observed audio-path allocations. Captures were not modified or copied.

These are offline/harness results. No listening evaluation, Bitwig session, macOS build, VST3
host validation, full visual layout review, or general race detector run was performed.

### CPU and latency

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

1. Add `build-grid/out` to Bitwig's plugin locations and load **NAM Grid Prototype**.
2. Choose the NAM folder in Settings; load a pedal into IN1 and different full rigs into A1/B1.
   Leave global IR bypassed for full rigs. Sweep Blend A/B and check unity/phase by ear.
3. Load and bypass captures in all four rows. Verify that output-row changes affect both lanes.
   Listen to model loads, bypass and blend automation under sustained input.
4. Save/reopen the project and verify all 16 selections and controls. Move a file, recall, and
   relink it. Check dropdowns, OS file/folder pickers, labels and controls at normal/scaled sizes.
5. Change host rates and block sizes; check restart handling, delay compensation, CPU and underruns.
   Repeat on macOS and in a VST3 host before claiming support there.

## Build

```sh
python scripts/setup-deps.py
cmake -S . -B build-grid -DCMAKE_BUILD_TYPE=Release
cmake --build build-grid --config Release --target NeuralAmpModeler-clap NeuralAmpModeler-vst3 nam-stage-tests nam-clap-smoke nam-library-tests nam-clap-bench --parallel 3
ctest --test-dir build-grid -C Release --output-on-failure
```

Use an MSVC developer prompt on Windows (add `-G Ninja` if using Ninja). Detailed dependency
instructions are in [BUILD.md](BUILD.md).
