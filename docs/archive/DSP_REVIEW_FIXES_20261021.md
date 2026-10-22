# DSP review fixes, 2026-10-21

> Historical record. Branch names, measurements and completion claims below describe
> their original checkpoint. See the [archive index](README.md) for current guides.

Based on `9ebb4cb`, including Shimmer, Juno Chorus and Frequency Shifter. Work uses
the isolated `nam-grid-dsp-fixes` worktree and `build-dsp-review` build directory;
the concurrent `experiment/noise-print` worktree is untouched.

## Changes

- Cabinet IR drains 8,192 zero-input frames after its bypass fade, then suspends.
  This clears the full capped FIR history without resetting/allocating on audio.
- Plate/Cloud diffuser capacity derives from the largest diffusion time, maximum
  Size and right-channel scale. Both channels can reach maximum Size without
  silently clamping the last diffuser.
- Crossover-frequency automation has a 10 ms time constant at every sample rate.
  AutoSwell smooths threshold, rise and release coefficients over 20 ms while
  retaining its per-sample gain envelope. Initial controls initialize directly.
- Airwindows Tape initializes every state member and caches rate-only biquad
  coefficients during reset. The upstream nonlinear processing math is retained.
- Limiter reduction metering computes one logarithm from the minimum gain per
  call. Reverb skips motion sine evaluation at exactly zero modulation. Remaining
  envelope followers explicitly flush tiny state, including standalone DSP use.
- Coupled stereo dispatch and standard delay/reverb/modulation families use
  explicit effect sets. All latest HQ modules retain their existing paths. The
  effect-name switch handles NAM explicitly and returns Unknown for invalid IDs;
  ScalarFilter requires a biquad base to match its two-sample histories.
- Only enabled, loaded slots measure levels. Slot meters and the global
  input/output meters now measure `(L + R) / 2`, with native mono input preserved.
  Meters sit in front of wiring and occupy the card height minus its corners.
- Module selection validation now includes Shimmer, Juno Chorus and Frequency
  Shifter: the previous menu offered these but SetSlotEffect rejected them.

Follow-up UI work adds a folder-based rig preset menu, global IN gain, smaller
IN/OUT/A/B dials, more grid spacing, colored card outlines and larger small labels.
Empty-card Add opens with one click; Clear is first in both menus; all FX are
categorized. NAM captures and IRs have separate indexed menu sections, while
library folders remain shared. The header search/filter bar is removed pending
a dedicated browser. [PRESETS.md](../PRESETS.md) describes use and storage behavior.

The new general Distortion has Soft/Hard/Fuzz/Foldback waveshapers and smoothed
Drive/Tone/Bias/Mix/Output controls. The Distortion menu also contains Boost,
Tube Boost, Tape and Bitcrusher. [MODULES.md](../MODULES.md) documents its voicing
and lack of oversampling.

Capture seam topology is retained. Freeze repeats at Capture minus overlap;
Stutter keeps its requested period through the gated seam, and Reverse crossfades
fresh banks. These semantics and memory budgets are documented in
[ROADMAP_EFFECTS.md](../ROADMAP_EFFECTS.md). Eight-second synced capture windows,
16-second dual taps and their conservative drain budgets remain supported.

## Verification

`DSPReviewTests.h` adds actual-stage stereo FIR bypass/re-enable regression at
44.1/48/96/192 kHz with variable 1/3/64/127-frame blocks and allocation guards;
maximum diffuser read-range checks; AutoSwell automation/partition checks;
crossover response comparison at equal elapsed time across rates; deterministic
Tape construction/reset; nonconstant capture seam/period tests; Freeze overlap
timing; and eight-second tempo-synced capture regression. The full existing stage
suite also exercises the three HQ modules from the new main commit.

Windows x64 Release CLAP and VST3 builds passed. All four CTest suites passed:
capture-library, wav-decoder, stages (9.44 s) and clap-smoke (183.16 s), 193.72 s
total. The host test covers native single-click module selection, right-click
clear/copy/move/swap, all HQ module selections, preset Save/Load/Cancel, sibling
preset selection and previous/next navigation. Its timeout is now 300 s to cover
the expanded native-dialog checks. Distortion tests verify distinct styles,
DC rejection, dry blend, automation partition invariance and no audio allocation
at 44.1/48/96/192 kHz. Mono meter tests cover identical channels, opposite polarity
and a silent right channel. A separate visible editor probe passed native menu
and file-dialog checks; the live no-audio preview remains available.

This does not replace real guitar listening, DAW low-buffer sessions or macOS testing. Broader coefficient
caching, float/shared capture storage, linked dynamics and oversampling remain
separate design/performance work, rather than unverified changes to voicing or
supported parameter ranges.
