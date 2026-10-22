# On-demand FX CPU and deadline report

This tool measures every catalog FX through the actual stereo CLAP plugin,
plus NAM models and WAV cabinet IRs. It opens no editor or audio device.
It is disabled by default, excluded from the default build, and never registered
with CTest or invoked by CI. Run it explicitly when investigating performance.

## Build and run

Prepare dependencies as described in [build.md](build.md), then use a Release build:

```sh
cmake -S . -B build-grid -DCMAKE_BUILD_TYPE=Release -DNAM_BUILD_PERFORMANCE_TESTS=ON
cmake --build build-grid --config Release --target nam-grid-clap nam-fx-performance --parallel 3
python scripts/fx-performance.py --output tmp/fx-performance/full
```

Windows requires the same x64 developer environment as the plugin build.
The script finds both Ninja and Visual Studio Release target locations.
Use `--build-dir` for a different build tree, or `--host` and `--plugin` for explicit
paths. On macOS, the bundle executable is resolved automatically from
`build-grid/out/nam-grid.clap`; this platform has not been validated locally.

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
`build-grid/fx-performance/` (also ignored). Use `--output tmp/<investigation>`
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
python scripts/fx-performance.py --module "Noise Gate (Smart)" --rates 48000 96000 --buffers 32 64 128 --seconds 5 --warmup 1 --repeats 3 --output build-grid/gate-before
# Rebuild the optimized plugin, then run the same settings.
python scripts/fx-performance.py --module "Noise Gate (Smart)" --rates 48000 96000 --buffers 32 64 128 --seconds 5 --warmup 1 --repeats 3 --compare build-grid/gate-before/report.json --output build-grid/gate-after
```

The second command also writes `comparison.md`, `comparison.csv` and
`comparison.json`. Comparisons require matching cases, controls, asset hashes,
sample format, rates, buffers, durations, repeats and pacing. Failed or missing
measurements never produce an improvement figure. Values are medians of repeat
means, with p99 and deadline counts retained. Negative CPU change means faster.
Total callback CPU includes shared overhead; JSON also compares estimated module
CPU after subtracting each run's own empty-grid measurement. Repeat the before
and after runs to distinguish improvements from changes in machine load.

## Coverage

The host reads `kModuleGroups`, `SlotDefaultValues`, shared effect choices and
presentation metadata directly from C++. Adding catalog modules, enum choices,
or binary switches extends the benchmark automatically. It tests the Cartesian
product of each module's enum and binary controls, including distortion styles,
filter/gate/Juno modes, console desks, tape-head patterns, Cosmic topologies,
mono/stereo reverb, ping-pong, rotary speed, vibrato, hold/freeze, and tempo sync.
Every tempo division is also tested with other mode controls at insertion defaults;
divisions are not multiplied through every other mode combination.
The legacy Tight Gate is covered by Noise Gate's Tight mode.

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
build-grid/NeuralAmpModeler/nam-fx-performance --self-test
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

Keep generated measurements and comparisons in ignored `tmp/` or attach findings
to the relevant PR. Only enduring instructions belong in this guide.

For a standalone comparison of direct, uniform FFT, two-stage FFT and WDL
convolution, see [IR engine comparison](ir-convolution-comparison.md).
