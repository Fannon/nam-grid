# Metering, EQ, cabinet IRs and slot editing

These controls extend the fixed 4×4 grid on `experiment/fixed-4x4-grid`. Routing stays
four serial input slots → two four-slot parallel lanes → four serial output slots.
The plugin and binaries are **nam-grid**. See [MODULES.md](MODULES.md) for the complete
module list. This prototype is unreleased: Pre/Post labels change host parameter names;
old-project and automation-ID compatibility are not guaranteed.

## Using the cards

- Click a **card** to edit its knobs in the bottom panel. Double-click the card or use the
  panel's **Choose / Change** button to choose NAM, gate, compressor, EQ, WAV IR or Clear.
  Library submenus, search, gear filters and previous/next buttons work across multiple roots.
  Choosing WAV changes the module to IR automatically; choosing NAM changes it back.
- Right-click a **card** (for example `A 1`) to **Copy to**, **Move to**, or **Swap with**
  another position. Copy and Move replace the destination; Move then empties the source.
  Swap exchanges both positions. Destination menus identify their current contents.
- All musical controls, bypass and model quality travel with the slot. Files are referenced,
  not moved or copied on disk. Each copied processor has independent DSP history.
- Existing DAW automation envelopes stay attached to their physical slot parameter IDs.
  Legacy Pre 2 input calibration/normalization settings remain attached to Pre 2 as before.
- Resources for both sides are prepared before an edit is committed. A missing/invalid
  source file, or an unavailable destination file during Swap, rejects the edit and leaves
  both slots unchanged. Edits may produce brief fades/dropouts while processors change.

## Level and reduction meters

Each card has two slim vertical peak bars beside it (input left, output right), spanning -60 to 0 dBFS. Input is measured before
the slot's input gain, output after processing/trim/bypass. Red indicates a peak at or above
0 dBFS and holds for one second. Floating-point audio is not hard-clipped by these meters.
The selected module panel shows gate/compressor attenuation as a **Gain reduction** readout. Makeup gain
is separate from gain reduction. Other effect types report zero GR.

Audio accumulates peaks and sends bounded, allocation-free SPSC packets about 30 times per
second. Rendering, logarithmic scaling, decay and clip holds run on the UI thread. The
meters decay when playback stops and never add audio latency. Meter values are transient
and are not saved in presets.

## Simple EQ

Three EQ choices show their controls in the eight-position bottom module editor:

| Effect | Controls | Defaults |
| --- | --- | --- |
| Guitar EQ | Bass: 150 Hz low shelf; Mid: broad 800 Hz peak; Treble: 3 kHz high shelf | All 0 dB; ±12 dB range |
| Cut EQ | HP: 0–300 Hz; LP: 2–20 kHz; Presence: 3.5 kHz high shelf, ±6 dB | HP off at 0 Hz, LP off at 20 kHz, Presence 0 dB |
| Parametric EQ | Bell 80–8000 Hz, ±12 dB, Q .3–4; HP and LP | 800 Hz, 0 dB, Q .7; cuts off |

NAM also exposes neutral Guitar EQ after inference and before trim.

Cut filters use 12 dB/octave Butterworth responses (-3 dB at the selected cutoff). A neutral Guitar EQ and an all-off Cut EQ
pass unity. Place Guitar EQ before an amp to shape its response; use Cut EQ after a cabinet
to remove low rumble/high fizz. New instances use EQ modules; recalled legacy shared EQ settings are preserved.

`SlotEQ.h` reuses the MIT-licensed AudioDSPTools low/high shelf and peak coefficient
implementations and biquad base. Cutoff coefficients use the standard
[RBJ Audio EQ Cookbook equations](https://www.w3.org/TR/audio-eq-cookbook/).
The Cookbook's permissive [W3C license](https://www.w3.org/copyright/software-license-2015/)
allows modification/distribution with retained notices; its attribution and full notice are
included alongside the existing MIT notices in `ThirdPartyNotices.txt`.
Small scalar histories replace the multichannel buffer interface; no new
DSP dependency is introduced. Parameters smooth over 5 ms, coefficient updates occur on a
continuous 16-sample schedule, and cutoff bypass transitions also ramp over 5 ms. Reset
clears histories. Processing adds no buffering latency and allocates nothing.

## Cabinet IR

**Choose cabinet IR (.wav)** loads a mono WAV into any slot, using the existing NAM IR
loader/convolver. The card shows the filename and `WAV / IR`; its bottom editor has one **Level** knob (-36 to +18 dB).
Each slot holds either a NAM or an IR. Place a NAM cabinet capture using the normal NAM
selector. Full amp+cab captures typically do not need another cabinet stage.

The default Level is -18 dB to match the original global cabinet's headroom convention.
At 0 dB, the slot cancels the reused convolver's fixed -18 dB gain, so a unit impulse has
unity convolution gain at its native 48 kHz rate. Its existing sample-rate-dependent IR
resampling gain is retained. Nothing auto-normalizes IRs. Recalled projects can still have a
legacy global IR; its compatibility toggle appears in Settings. It is disabled in new instances.

The reused processor has zero block buffering latency and caps the resampled IR at 8192
taps. It is intended for cabinet impulses, rather than long reverbs. WAV parsing, resampling,
history allocation, and prewarming occur off audio. Reset reconstructs and primes the
convolver even when the sample rate is unchanged. Variable host blocks use reserved capacity.
Invalid loads preserve the current selection; recalled missing/invalid files retain their
paths and IR type for relinking, while that slot passes audio.

## A2 Full / Lite

The module menu offers **A2 Lite** when Core reports a slimmable model. A2 captures packaged
with both submodels select them from the same `.nam` file. Fixed-size captures have no quality
toggle; a smaller model cannot be synthesized from an arbitrary fixed model.
The supported target is A2. No A1-specific implementation was added.

The [Core size API](https://github.com/sdatkinson/NeuralAmpModelerCore/blob/main/NAM/slimmable.h)
explicitly states that changing size is not real-time safe. Size changes now reload/prepare
the requested model outside audio, then use the slot's existing publication/retirement and
switching fade. Core's setter is never called by `NAMStage::Process`. A short dropout is
allowed during changes. UI toggles and host automation requests are applied on the idle/control
thread; rapid requests can coalesce. If the selected file disappears, the old processor stays
active and the quality parameter reverts to the prepared value.

Other slots append boolean Full Quality parameters. Pre 2 reuses the original `Slim` parameter
(0 = Lite, 1 = Full), preserving its automation ID and old state. New projects default to
Full. Saved quality travels with Copy/Move/Swap and is applied before audio is activated.

All new effects add zero buffering latency. Existing NAM rate conversion still reports its
actual delay when capture/host rates differ; use matching rates for a zero-latency NAM chain.
Typical captures are 48 kHz. Parallel lane compensation continues to follow actual delays.

## State and verification

The current layout has **1262 parameters**: the previous 284, then 61 utility/time-effect
parameters per slot plus two tuner preferences. Schema **9** stores paths, types and named values under
`###NAMGrid-v1###`. IDs 0–9 cover NAM, dynamics, EQ, IR and Boost; 10/11 Filter/Swell;
12–14 delays; 15–17 reverbs; 18–22 modulation; 23 Airwindows Tape; 24 Tube Boost; 25 Utility; 26 Spring Reverb; 27 Wah. Only NAM/IR accept paths.
Older-format readers remain where practical, but old unreleased project compatibility is not guaranteed.
See [MODULES.md](MODULES.md) for the expanded test coverage and manual checklist.

Windows x64 Release CLAP/VST3 builds and all three CTest suites pass. Added checks cover:

- Neutral EQ unity, measured band/cutoff responses, automation stability, exact output under
  different block partitions at 44.1/48/96 kHz.
- Slot IR output against the existing convolver, native/resampled host rates, zero reported
  buffering latency, a one-frame maximum, reset and no observed audio-path allocations.
- Packed A2 Full and Lite against independently prepared corresponding A2 submodels, with
  variable blocks and C++/Eigen allocation guards.
- Prepared Copy/Move/Swap transactions with real model/IR files, independent processors,
  all settings, empty sources and rejection before commit when a file is unavailable.
- Peak/GR tap values, mixed NAM/effect processing, schema migration, EQ/IR recall, and missing
  IR state. Native editor knob checks and a live Full/Lite toggle followed by save/recall.
- Read-only processing of the user's four real A2 exports across the grid, and both Full and
  Lite of the Avalon AD2022 export: finite nonzero output, with no observed audio allocations.

Ryzen 9 5950X / Windows x64 Release, 128-frame blocks, fastest of three timed 128,000-sample
renders after warm-up. The complete chain is Natural Gate → two deterministic A2 models →
Guitar EQ → empty parallel lanes → 2048-tap synthetic cabinet IR → Cut EQ → Smooth Compressor.
Meters are active. Cost is elapsed time as a percentage of one CPU core, not a DAW meter.

| Host rate | Two models | Complete chain | Reported latency |
| --- | ---: | ---: | ---: |
| 48 kHz | 7.41% | 8.24% | 0 samples |
| 44.1 kHz | 8.07% | 8.74% | 54 samples |
| 96 kHz | 8.36% | 12.33% | 84 samples |

The extra delay at non-native model rates is existing NAM conversion, not EQ, IR or dynamics
buffering. The cabinet coefficients resample at reset, so its tap count/processing cost grows
at higher host rates. These are synthetic offline measurements; arbitrary captures, long IRs
and low-buffer host scheduling still need auditioning. Reproduce with `nam-clap-bench
<plugin.clap> complete 1 <drive.nam> <amp.nam> <cabinet.wav>`; use `dual` without the final IR
argument for the two-model comparison.

These are automated/offline checks. Bitwig, listening, native destination-menu interaction,
full visual layout/DPI review, macOS and VST3 host validation remain manual work:

1. In Bitwig, build gate → NAM drive/amp → cabinet IR → Cut EQ → compressor; turn off the
   global cabinet/shared gate for the first audition. Match host and capture rates.
2. Verify I/O/clip and GR meters with quiet playing, over-0-dBFS peaks and silence after Stop.
3. Copy a NAM, Move a tweaked EQ, Swap a compressor with an IR. Check both settings and signal
   order. Test same-row and cross-lane destinations; save/reopen the project.
4. Toggle Full/Lite during playback and by automation. A brief switching dropout is allowed;
   steady playback must stay stable. Compare CPU and tone using the same playing material.
5. Remove a referenced file and repeat a slot operation/load/recall. Check error handling and
   relinking, then test rates, buffers, editor reopen and display scaling.
