# On-demand IR convolution experiment

This standalone experiment compares the current AudioDSPTools direct convolver,
HiFi-LoFi FFTConvolver (uniform and two-stage), and WDL's divided convolver. It
opens no audio device and changes no plugin code. It is separate from the normal
CMake build and CTest/CI. The existing [module benchmark](fx-performance.md)
remains the tool for measuring the complete plugin.

## Build

Prepare the repository's normal pinned dependencies first. In a Release compiler
environment (an x64 MSVC developer prompt on Windows):

```sh
cmake -S tests/ir-comparison -B build-ir-comparison -DCMAKE_BUILD_TYPE=Release
cmake --build build-ir-comparison --config Release --parallel 3
```

The first configure fetches [FFTConvolver](https://github.com/HiFi-LoFi/FFTConvolver)
at commit `f2cdeb04c42141d2caec19ca4f137398b2a76b85` into the ignored build directory.
Its [MIT notice](https://github.com/HiFi-LoFi/FFTConvolver/blob/f2cdeb04c42141d2caec19ca4f137398b2a76b85/COPYING.txt)
remains in the source. WDL and the current reference come from existing pinned
dependencies; WDL carries its permissive zlib-style notices in the source files.
No new library is linked into the distributed plugin.

Use `-DNAM_DEPS_ROOT=/path/to/prepared-checkout` to read dependencies from another
checkout. Use `-DFETCHCONTENT_SOURCE_DIR_FFTCONVOLVER=/path/to/FFTConvolver` for an
offline source checkout of the same revision. This was used locally to avoid
copying or modifying either active plugin checkout's dependencies.

## Run

```sh
python scripts/ir-convolution-comparison.py --output build-ir-comparison/upstream
```

For Visual Studio's multi-configuration generator, select the executable explicitly:

```sh
python scripts/ir-convolution-comparison.py --host build-ir-comparison/Release/nam-ir-comparison.exe --output build-ir-comparison/upstream
```

Defaults are four deterministic dense 48 kHz IR lengths (64/256/2048/16384 source
samples), 44.1/48/96 kHz host rates, 32/64/128 frames, two rounds, and three repeats
per round. Each measurement renders 3 seconds plus 0.5 seconds of warm-up. Add
actual mono WAVs using repeatable `--ir /path/to/cab.wav`. Files remain external;
the report records their paths and SHA256. Narrow with `--rates`, `--buffers`,
`--lengths`, `--rounds`, `--repeats`, `--seconds` or `--warmup`.

Each engine/case runs in a fresh subprocess. Engine order is shuffled within
matched settings in each round. Processing uses stereo double input/output;
adapters include float conversion where required. Reported CPU includes these
adapters and excludes all grid/plugin/host overhead. Mean, p99, maximum and
measured deadline misses are retained. Reports contain medians of repeated means,
individual round medians, and every raw measurement. No outliers are removed.

Preparation feeds 32768 zero samples offline to allocate the reference/WDL's lazy
storage and exercise delayed partitions. Warm-up follows. Timed audio is 75%
stereo excitation and 25% silence/tail. A first-call allocation is therefore not
claimed to be safe on the audio thread merely because steady processing passes.

## Accuracy and latency

All paths use the current reference's cubic resampling, float coefficients/input,
sample-rate-dependent gain and 8192-tap cap. Checks compare an offset impulse
against those coefficients, test stereo isolation and silence, then compare
deterministic random stereo excitation plus the full tail against the actual
direct implementation. Variable blocks include 1/3/7/13/31/32/64/128 frames,
bounded by each configured maximum. Checks require zero declared extra latency,
maximum absolute error below 1e-5 FS, and relative RMS error below 1e-4. These are
explicit experimental thresholds rather than a claim of perceptual equivalence.

WDL uses `WDL_ConvolutionEngine_Div` with zero allowed latency. FFT uniform uses
the host buffer size as its partition size; two-stage uses that head size and a
1024-frame tail partition on the same thread. No background processing is used.
Windows uses the bundled Ooura FFT; SSE accumulation is enabled on x64. Other
platforms have not been benchmarked by this experiment.

Global C++ new/delete and WDL's C malloc/realloc/free calls are counted during
processing. Dependency source files are unchanged. Source inspection found no
processing locks in these configurations; instrumentation is not a general OS
API/lock interception facility. Rebuilding the engine outside processing verifies
fresh preparation/history, rather than assuming a shared runtime reset API.

## Preserve quiet tails in FFTConvolver

Upstream FFTConvolver silently removes trailing coefficients below 1e-6. Both its
uniform and two-stage initializers do this. The quiet-tail fixture is designed to
expose the difference; dense performance fixtures end above this threshold.

This experiment can generate marked copies that trim only exact zeros:

```sh
cmake -S tests/ir-comparison -B build-ir-comparison -DIR_FFT_PRESERVE_TAIL=ON
cmake --build build-ir-comparison --config Release --parallel 3
python scripts/ir-convolution-comparison.py --output build-ir-comparison/preserve-tail
```

The source change is limited to the tail trimming condition in those two files.
Original notices remain, and the fetched source is unchanged. The executable's
`--build-info` and report metadata identify whether this option is active. Use
identical settings/assets for both reports. Turning the option off restores the
upstream implementation.

## Artifacts and limits

`report.md`, `summary.csv`, `report.json` and flushed `raw.jsonl` are written to
the chosen new output directory. The JSON includes all numerical checks and
per-round CPU values. Processing failures produce error rows; interrupted runs
retain completed cases. Numerical audit failures are recorded in the report and
console count; exit status reflects worker/processing failures rather than
making upstream's known trimming policy prevent the comparative experiment.

```sh
python -m unittest discover -s tests -p test_ir_comparison.py
```

These results do not establish device xruns, audible clicks, or production-safe
asset switching. A production integration still needs callback allocation tests,
state/recall checks, IR replacement/crossfade tests, integration benchmarks, and
DAW/driver listening. A faster implementation of the same FIR is not inherently
a reduced-quality IR mode.
