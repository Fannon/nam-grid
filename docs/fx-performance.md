# On-demand FX CPU and deadline report

This tool measures every catalog FX through the actual stereo CLAP plugin,
plus NAM models and WAV cabinet IRs. It opens no editor or audio device.
It is disabled by default, excluded from the default build, and never registered
with CTest or invoked by CI. Run it explicitly when investigating performance.

For a waveform audit against a preserved CLAP binary, use the explicitly built
`nam-fx-performance` host with:

```sh
python scripts/verify-plugin-processing.py --before /path/to/before.clap --after build/windows/release/out/nam-grid.clap --output tmp/processing-audit/reports
```

This compares crossover, lane/input/output gains, tuner mute,
IR levels/bypass and modulation at four rates and three buffers. Shared processing
must match exactly; IR output has a 1e-6 maximum error tolerance for FFT rounding.
Native-double render files are generated only for this explicit audit and removed
after comparison. The JSON retains per-case errors, sample counts and binary hashes.
Rendering and audit automation are absent from ordinary benchmark settings.

For module DSP changes, `verify-fx-processing.py` renders every benchmark case
(each FX mode, the NAM fixtures and IR fixtures) through both binaries:

```sh
python scripts/verify-fx-processing.py --before /path/to/before.clap --after build/windows/release/out/nam-grid.clap --output tmp/fx-audit
```

Each render automates the slot's shared dynamics/EQ/level controls and every
visible continuous control of the module twice, then raises the depths of matrix rows
from the LFO, envelope, sequencer and Macros 1-2 to slot 1 and the A/B blend. Row routing
is saved state, so the audit sets it in the case's initial state.
Cases that latch Freeze, Shimmer Freeze or Noise Learn render without automation,
since the host reserves its event for that trigger. Defaults are 44.1/48 kHz and
32/37/128 frames; the odd size exercises partial control quanta. Outputs must be
bit-identical unless `--tolerance` sets a maximum absolute sample difference.
`--module`, `--filter`, `--rates`, `--buffers`, `--nam` and `--ir` narrow or extend
the run, as for the benchmark. `report.json` keeps each case's maximum error,
mismatch count and first differing sample. Audit events may name an enumerated
parameter by its display text, such as `"1/4"` for a tempo division; numeric text does
not select an enumerated choice.

## Build and run

Run `bash build.sh` first, then enable the optional benchmark target in that build.
The commands below use Windows paths; substitute `macos` or `linux` on those systems:

```sh
cmake -S . -B build/windows/release -DCMAKE_BUILD_TYPE=Release -DNAM_BUILD_PERFORMANCE_TESTS=ON
cmake --build build/windows/release --config Release --target nam-grid-clap nam-fx-performance --parallel 3
python scripts/fx-performance.py --output tmp/fx-performance/full
```

The Windows shortcut selects Visual Studio, so a developer prompt is unnecessary.
A manually configured Ninja/MSVC build still requires an x64 developer environment.
The script finds both Ninja and Visual Studio Release target locations.
Use `--build-dir` for a different build tree, or `--host` and `--plugin` for explicit
paths. On macOS, the bundle executable is resolved automatically from
`build/macos/release/out/nam-grid.clap`.

The default matrix is **44.1/48/96 kHz × 32/64/128 frames × three repeats**.
Each tuple renders 0.5 seconds of warm-up followed by three seconds of measured
audio: 75% continuous stereo excitation and 25% silence/tail. Every repeat uses
a fresh plugin instance; every scenario runs in a separate process. A crash or
timeout retains completed rows and marks unfinished tuples as errors.
Expect several minutes or longer depending on the machine and model complexity.

```sh
# Enumerate module/mode cases without loading a plugin.
python scripts/fx-performance.py --list

# Quick investigation of all Cosmic modes at the smallest buffers.
python scripts/fx-performance.py --filter "Cosmic Echo" --rates 48000 --buffers 32 64 --repeats 1

# Add your actual captures and IRs; arguments are repeatable.
python scripts/fx-performance.py --nam /path/amp.nam --ir /path/cab.wav

# Longer signal/tail runs. Return nonzero on observed callback deadline misses too.
python scripts/fx-performance.py --seconds 30 --warmup 2 --fail-on-overrun

# Real wall-time callback pacing for a focused case (ordinary OS thread).
python scripts/fx-performance.py --filter "Digital Black" --rates 48000 --buffers 32 --seconds 30 --paced
```

`--filter` is a case-insensitive substring of module name, readable mode, or case
ID, and can be repeated (OR). The empty-grid baseline always runs. `--output`
chooses a report directory; the default is a timestamped folder beneath
`tmp/fx-performance/` (also ignored). Use `--output tmp/<investigation>`
for local reports, as above. Exit code 1 means errors (or overruns when
`--fail-on-overrun` is set); 2 means an invocation/setup failure. Other runs
report overruns without making variable machine load a hard pass/fail gate.
`--timeout` overrides the automatically sized per-scenario wall-time timeout.
Ctrl+C retains the rows from completed scenarios and writes a partial report.

## Individual modules and before/after comparisons

`--module` matches an exact module name, case-insensitively. It includes all that
module's tested modes and the empty-grid baseline. Repeat it to select several
modules, use `--module empty` to measure only shared overhead, or combine it with
`--filter` to select one mode:

```sh
python scripts/fx-performance.py --module "Noise Gate (Smart)" --list
python scripts/fx-performance.py --module "Noise Gate (Smart)" --filter "Noise Learn=0"
python scripts/fx-performance.py --module empty
```

Record a baseline before changing/rebuilding the plugin. Keep a copy of the old
binary if you want to repeat both versions under comparable background load;
`--plugin` selects it explicitly. For a focused comparison:

```sh
python scripts/fx-performance.py --module "Noise Gate (Smart)" --rates 48000 96000 --buffers 32 64 128 --seconds 5 --warmup 1 --repeats 3 --output tmp/gate-comparison/before
# Rebuild the optimized plugin, then run the same settings.
python scripts/fx-performance.py --module "Noise Gate (Smart)" --rates 48000 96000 --buffers 32 64 128 --seconds 5 --warmup 1 --repeats 3 --compare tmp/gate-comparison/before/report.json --output tmp/gate-comparison/after
```

The second command also writes `comparison.md`, `comparison.csv` and
`comparison.json`. Comparisons require matching cases, controls, asset hashes,
sample format, rates, buffers, durations, repeats and pacing. Failed or missing
measurements never produce an improvement figure. Values are medians of repeat
means, with p99 and deadline counts retained. Negative CPU change means faster.
Total callback CPU includes shared overhead; JSON also compares estimated module
CPU after subtracting each run's own empty-grid measurement. Repeat the before
and after runs to distinguish improvements from changes in machine load.

## Stacked processing and buffer headroom

Use `--copies` to measure repeated active processors rather than extrapolating
from one module. The empty-grid control runs once per rate/buffer/repeat:

```sh
python scripts/fx-performance.py --module "Distortion" --copies 1 4 8 16 --rates 48000 --buffers 64 128 --output tmp/audio-buffer-performance/reports/stacked
```

Each copy has independent DSP state and the same initial controls and asset.
Copies fill slots in grid order (Pre, A, B, Post), retaining the grid's normal
parallel routing. They are not an all-serial chain or an arbitrary user rig.
Every requested asset must load in every copied slot. Stacked latch/learn cases
are excluded because their triggers require separate live-history handling;
single-copy latch/learn measurements remain available. The default is one copy,
so existing benchmarks and waveform audits retain their normal case selection.
Comparisons require the same copy counts, controls and asset hashes.

At 48 kHz, 64 samples allow about 1.33 ms per callback and 128 allow 2.67 ms.
Those are the whole audio thread's budgets, not an allowance for each module.
Inspect signal, warm-up, p99/max and deadline counts as well as average CPU.
A clean single-module screen does not establish that a populated rig or DAW
project meets its deadline. Keep the zero-lookahead design and measure useful
CPU savings before lowering oversampling or model quality.

## Coverage

The host reads `kModuleGroups`, `SlotDefaultValues`, shared effect choices and
presentation metadata directly from C++; `--modules` prints module names and IDs, so the
Python scripts never hard-code module IDs. Adding catalog modules, enum choices,
or binary switches extends the benchmark automatically. It tests the Cartesian
product of each module's enum and binary controls, including distortion styles,
filter/gate/Juno modes, console desks, tape-head patterns, Cosmic topologies,
mono/stereo reverb, ping-pong, rotary speed, vibrato, hold/freeze, and tempo sync.
Every tempo division is also tested with other mode controls at insertion defaults;
divisions are not multiplied through every other mode combination.

Airwindows reverb processing-resolution minimum, default and maximum are sampled.
Other continuous knobs use their module's insertion defaults, so these figures
are representative costs rather than an exhaustive worst case over all settings.
Smart Gate covers untrained, trained and learning operation. Freeze Hold and
Shimmer Freeze receive live input history before the freeze event. Silence runs after excitation so reverb,
delay and filter tails remain active instead of timing only an empty processor.

Default assets are deterministic small/large A2 NAM fixtures, a slimmable NAM
fixture, NAM Core's LSTM and WaveNet examples, and dense mono IRs of 2048 and
16384 samples at 48 kHz. The multi-rate matrix also exercises resampling.
Slimmable architectures run at A2 and A2 Lite quality. Normal NAMs have one quality
path. Supplied real assets are added to these fixtures; WAV IRs must be mono,
matching the cabinet loader's supported format. The report records paths,
architectures, content hashes and plugin identity: a synthetic model or IR is not
a prediction of every capture's cost. Successful state recall alone is insufficient
to prove loading; the host checks the loaded asset's saved content identity too.

## Reading the report

- `report.md`: per-module summary and the highest-cost module/mode measurements.
- `measurements.csv`: every mode/rate/buffer/repeat, ready to sort or compare.
- `report.json`: complete results, including phase statistics and run metadata.
- `raw.jsonl`: flushed results after each scenario; `worker.log` records worker failures.

**CPU % = elapsed callback time / (buffer frames / sample rate) × 100**.
This represents one audio thread's available processing time, including the plugin
and empty grid overhead, rather than total OS CPU usage. A module near 100% leaves
no time for other plugins on that thread. Estimated module CPU subtracts the mean
of the matching empty-grid row, clamped to zero; separate-process scheduling and
clock changes make this an approximation. Use total callback cost when judging
headroom. No fastest-run selection or outlier removal is performed.

The report keeps mean, p95, p99 and maximum callback times, deadline-miss counts,
separate signal and silence/tail costs, first callback and warm-up overruns,
state-preparation and activation time, plugin latency, output peak, non-finite
samples and CLAP process errors. NaN-poisoned output buffers detect incomplete
output writes as well. State preparation/activation happens outside the callback
and is not charged as steady DSP CPU. Warm-up is checked, even though it is excluded
from the steady-state mean. Output above unity is reported as a peak, not treated
as a click or stability failure; distortion/boost can legitimately exceed unity.

`OK` means no observed errors or callback overruns. `LOW HEADROOM` means a measured
callback used more than half its deadline. `OVERRUN` means at least one warm-up or
measured callback exceeded the full buffer deadline. `ERROR` means failed loading,
processing errors, non-finite/unwritten output, a crash or a timeout.

Default mode renders as fast as possible and compares each callback against its
buffer deadline. `--paced` schedules callbacks in wall time and separately records
late completion, including OS wake-up delays. It uses an ordinary thread and can
take hours for the full matrix; narrow the filters first. Pacing lateness does not
change the DSP-overrun status, since timer behavior and device xruns differ.

These are DSP diagnostics. They do not detect audible pops, driver xruns/DPC
latency, UI interference, automation/mode transitions, concurrent file loading,
or contention in a full DAW project. A deliberate waveform edge from distortion,
stutter or a gate cannot reliably be classified as a pop by a generic jump detector.
Confirm candidates in the intended DAW/audio driver at the target buffer size,
with the real rig, transport and UI activity. Repeat under comparable power and
background-load conditions when comparing code revisions.

The report implementation has focused on-demand checks:

```sh
python -m unittest discover -s tests -p test_fx_performance.py
python -m unittest discover -s tests -p test_verify_fx_processing.py
build/windows/release/NeuralAmpModeler/Release/nam-fx-performance.exe --self-test
```

Use the `Release` subdirectory and `.exe` suffix for a Visual Studio generator.

## Comparing implementation changes

Measure the current build before choosing an optimization. Compare the same
captures, IRs, quality settings, sample rates and buffer sizes under comparable
load, retaining both binaries. Repeated mean/p99 costs distinguish consistent
work from isolated scheduling spikes; audible device underruns need a DAW test.

For a convolution replacement, preserve onset, gain/sample-rate scaling, stereo
independence, state recall and truncation policy. Quantify output error against
the direct reference with impulses, silence/tails, variable blocks, reset and IR
replacement. Prepare plans/buffers off audio and verify processing allocations
and locks separately. Do not claim speedups from library descriptions alone.

### Modulation and shared architecture

Use `--modulation none|depth-zero|macros|lfo|envelope|sequencer|all` to apply the
same matrix profile to every selected case, including the empty-grid baseline.
The profiles route to A/B blend, allowing NAM inference and other module costs
to be compared with and without control-rate subdivision. Keep separate output
folders; comparison requires the same cases, routing and settings.

With no nonzero-depth source assignments, source DSP is skipped and the grid
processes whole host buffers. Macro-only assignments retain the first control
quantum for gain transitions, then batch the rest of the host buffer; source
clocks and follower histories continue to advance.

Dynamic LFO, envelope or sequencer dependencies, including Via, use a control
interval near 0.33-0.36 ms at standard host rates: 16 frames at 44.1/48 kHz,
32 at 88.2/96 kHz and 64 at 176.4/192 kHz. It selects the nearest power of two
to `sampleRate / 3000`, with a minimum of one frame, preserving alignment with
common host buffers instead of introducing short 15- or 29-frame calls.
The clock continues across host callback boundaries. It sets control reaction
resolution without buffering or delaying the audio output. Existing module
smoothing times and internal coefficient-update counters remain independent.
Most built-in controls smooth per sample with a 20 ms time constant; NAM EQ
uses 5 ms smoothing, while routing and slot gain controls have their own ramps.
There is no universal extra smoother in the modulation matrix.

Input stages before the first dynamically modulated input destination can run
ahead in larger batches. They retain their first short quantum for gain ramps;
the remaining grid runs in control quanta. Prefix follower taps retain their
whole-buffer histories, and later taps are read after each quantum, preserving
feedback into the affected stages. A dynamic assignment to the first input slot
disables this optimization. Parallel lanes and output stages still follow the
quantum schedule, even if some are individually unaffected. Both source banks
currently advance together whenever modulation is active; an unused source
within an active matrix is therefore not free.

Run the waveform audit with `--macros-only` for macro routing, or `--routing-only`
to put changing destinations after the independent input stages. Use
`--envelope-listen 1 13`, for example, to exercise the two followers after input
slot 1 and output slot 1. The normal audit targets the first input slot and checks
the conservative fallback. Check all paths when changing control scheduling.

Batching can slightly alter block-dependent control transitions. Airwindows
Tape samples its smoothed drive/head-bump controls in short internal spans;
keep its transition error budget separate from inference and convolution
rounding. At higher sample rates, the time-based matrix interval also
changes control timing relative to the previous fixed 16-frame schedule. Fast
modulation and gain transitions may sound slightly different; destination
smoothing times, parameter identities and saved-state formats are unchanged.

Only the selected effect in each slot runs its DSP. Empty slots skip sample DSP;
bypassed slots fade and drain their tails before suspending it. Bypass still has
routing/mixing overhead. Mix at zero generally keeps the effect running, as do
muted or inaudible parallel lanes, preserving histories, tails and follower taps.
Meters, DC filtering, control snapshots and routing contribute shared overhead.
These cases need separate measurements; neither Mix zero nor lane mute should
be treated as a CPU suspension command.

Prioritize settled-rig callback mean and upper-tail costs. Modulation assignment,
module insertion and mode changes may briefly interrupt sound-design playback;
prepare resources outside the steady audio path. No optimization may add
lookahead or fixed effect latency. During beta, a small measured sonic change
can be an acceptable performance tradeoff when documented and tested.

### Per-sample idioms

Small-buffer cost is dominated by per-sample work, so these patterns keep
module loops cheap without changing output:

- Recompute `exp`/`pow`/`tan` of smoothed controls only when the argument
  changes. `grid_fx::Memo` (`ExactMemo.h`) caches by the argument's exact bits;
  pass the function's complete argument, including any sample-rate term.
  Hoist expressions of the sample rate alone out of the loop. It does not help
  values that move every sample, such as gain reduction or LFO-swept filters.
  Measure each cache: the comparison and branch can cost more than the function
  it replaces. Check parameter automation as well as settled controls.
- Avoid `size_t`/`double` conversions in delay reads (they are multi-instruction
  on MSVC x64), redundant `std::clamp` of doubles, and integer `%` on ring
  indices; compare-and-wrap gives the same index.
- For ADAA, cache an antiderivative only for the exact same curve and previous
  input sample. Drive/bias automation, style activation and the small-interval
  fallback must invalidate stale values. Bias offsets depend on bias and style;
  they can be reused separately. The stage tests compare cached and uncached
  processors through moving drive/bias/styles and tiny/odd blocks; the plugin
  waveform audit covers the full automation and modulation paths.
- Prepare per-callback copies once; modulation restores only the slots it edits
  for each control quantum.
- Where a loop has independent accumulators, such as the phases of a polyphase
  filter or a band bank, let each accumulator keep its own tap order and vectorize
  across accumulators. Pairing two of them in one register is bit-identical;
  reordering taps within one, or letting the compiler contract multiply-add pairs,
  is not. The shared Distortion/Pedal Drive oversampler packs phases with SSE2
  on x64 and NEON for its 4x path on arm64; other paths retain scalar loops.
  SSE2 requires no AVX/FMA dispatch or extra latency. `-DNAM_PEDAL_DRIVE_SCALAR`
  forces the portable path for comparisons.

Confirm such changes are bit-identical with `verify-fx-processing.py`. A change
that alters rounding, such as recurrence oscillators or a different FFT
partitioning, is a sound change and needs its own error budget and listening.

Keep generated measurements and comparisons in ignored `tmp/` or attach findings
to the relevant PR. Only enduring instructions belong in this guide.

For a standalone comparison of direct, uniform FFT, two-stage FFT and WDL
convolution, see [IR engine comparison](ir-convolution-comparison.md).
