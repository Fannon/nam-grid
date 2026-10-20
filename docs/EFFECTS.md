# Historical dynamics implementation notes

For the current module list, controls, licensing and tests, see [MODULES.md](MODULES.md).
Parameter counts, UI and measurements below describe the earlier dynamics checkpoint.

# Slot dynamics

Each grid slot now accepts a NAM capture, a noise gate or a compressor. Click its name,
then **Noise Gate** or **Compressor**. Three knobs and the existing On switch are visible
for every built-in effect. Selecting a built-in replaces that slot's NAM selection;
the arrow buttons select NAM captures again. Nothing changes the fixed routing.
Keep future effects within one to six knobs; use a small number of voiced variants when
that avoids exposing controls that rarely help with guitar playing.
See [SLOTS.md](SLOTS.md) for added EQ/IR effects, meters, Full/Lite and Copy/Move/Swap.

| Effect | Character | Knobs |
| --- | --- | --- |
| Natural Gate | Gentle 2:1 downward expansion, 1.5 ms opening, 40 ms hold | Threshold, Decay, Depth |
| Tight Gate | Closes to the selected depth, 0.3 ms opening, 15 ms hold | Threshold, Decay, Depth |
| Smooth Compressor | 2.5:1, broad 9 dB knee, 8 ms attack | Threshold, Release, Makeup |
| Punch Compressor | 4:1, narrow 3 dB knee, 25 ms attack preserves more pick transient | Threshold, Release, Makeup |

These are intentionally voiced digital dynamics, not emulations of particular hardware.
Listen with your own captures before deciding which settings fit your playing.

## Controls and placement

- **Gate Threshold:** -90 to -10 dBFS, default -55. Raise it until idle noise is reduced,
  then play lightly to check note tails. Detection measures the audio arriving at this slot.
- **Decay:** 20–500 ms, default 120. Lower values tighten stops; higher values preserve tails.
- **Depth:** 0–80 dB, default 60. Sets maximum noise attenuation; 0 leaves the signal open.
  Gates attenuate to this floor rather than guaranteeing digital silence.
- **Compressor Threshold:** -48 to 0 dBFS, default -18. Lower values give more compression.
- **Release:** 40–600 ms, default 120. Shorter settings recover faster; longer settings smooth
  sustained playing. Start around 80–150 ms.
- **Makeup:** 0–18 dB, default 0. Match enabled/bypassed loudness manually before comparing.
  There is no automatic makeup or limiter; downstream gain staging still matters.

Time values are exponential time constants, not a promise to finish the transition at that
exact time. Both gates use a 3 dB closing hysteresis band. The peak follower has a 0.1 ms
attack and 10 ms gate / 15 ms compressor release. Compressor reduction is then smoothed with
the variant's attack and the Release knob. Threshold, depth and makeup changes smooth over
5 ms. No oversampling, lookahead, extra channels or added algorithmic latency is used.

For a starting chain, put Natural Gate in IN1, NAM drive/amp stages after it, and optionally
Smooth Compressor in OUT1. Tight Gate suits more abrupt stops; try a shorter Decay. A gate
after a noisy amp capture can suppress that stage's idle noise, but does not have a separate
clean-input sidechain. The original shared gate remains available for existing projects;
disable it when auditioning slot gates on their own. Parallel compression can use one
compressed lane and one empty lane, with Blend controlling the mixture.

Built-ins use their three dedicated controls. NAM input/trim/calibration settings are retained
for later NAM use and do not change a built-in's sound. Each slot retains separate gate and
compressor settings; the two variants in a family share those settings. Variant selection is
a control-thread operation saved in project state, rather than an automatable routing change.
All visible knobs and slot bypass remain host-automatable, with stable parameter IDs.

## DSP, lifecycle and state

`SlotDynamics.h` is a small mono processor with scalar state and no dynamic allocation.
`NAMStage::Prepared` owns either its existing NAM processor or a built-in. The existing
publication/retirement mailbox, 5 ms bypass ramp and fade-out/swap/fade-in also cover effects.
Preparation and destruction stay off the audio thread. A NAM-to-effect replacement that
changes conversion latency uses the existing CLAP restart handshake and lane alignment.
Once bypass is settled, effect computation stops. Reset clears detector/envelope history.

The dynamics increment retained the original 61 IDs and appended six parameters per slot
(three gate and three compressor), reaching 157. The expanded grid now has 284 parameters
and schema 4; see [SLOTS.md](SLOTS.md) for the current format. Dynamics effect IDs remain
1 = Natural Gate, 2 = Tight Gate, 3 = Smooth Compressor, 4 = Punch Compressor. Invalid types,
counts, enum values and conflicting selections are rejected before mutation. Older grid,
dual and upstream states still migrate.

## Open-source review

Only MIT-licensed algorithm references were used for these additions; there is no new runtime
dependency or GPL/AGPL code. The implementation is newly written, drawing on these DSP ideas:

- [AudioDSPTools NoiseGate.cpp](https://github.com/sdatkinson/AudioDSPTools/blob/e19ef4b5b3bf2171c431847563acde29eedf85c0/dsp/NoiseGate.cpp),
  already pinned in this repository at `e19ef4b5b3bf2171c431847563acde29eedf85c0`.
  Inspiration: held detector-driven attenuation, bounded reduction and separate opening/closing
  behavior. The new gate uses an asymmetric peak follower, hysteresis and linear-dB expansion
  rather than copying its buffer/listener API or quadratic expansion curve.
  Its [MIT license](https://github.com/sdatkinson/AudioDSPTools/blob/e19ef4b5b3bf2171c431847563acde29eedf85c0/LICENSE)
  credits Steven Atkinson (2023).
- [LEAF leaf-dynamics.c, tCompressor](https://github.com/spiricom/LEAF/blob/b15fafff98ee28f23dab41c7c6ea0eae2bc393be/leaf/Src/leaf-dynamics.c),
  inspected at `b15fafff98ee28f23dab41c7c6ea0eae2bc393be`, specifically compressor initialization
  and the non-table tick function. Inspiration: feed-forward dB-domain compression with a
  quadratic soft knee and attack/release gain smoothing. This implementation has explicit
  positive reduction, a peak detector, independent variants and no LEAF tables/memory pool.
  Its [MIT license](https://github.com/spiricom/LEAF/blob/b15fafff98ee28f23dab41c7c6ea0eae2bc393be/LICENSE)
  credits Jeff Snyder (2020); the inspected compressor block has no separate embedded license.

Both licenses permit modification and redistribution with their copyright/permission notices.
The complete notices are retained in
[`ThirdPartyNotices.txt`](../NeuralAmpModeler/installer/ThirdPartyNotices.txt), including LEAF
as a conservative attribution for algorithm inspiration. Ship that notice file with binaries.

## Verification

Automated checks cover the known compressor curve/knee joins; settled attenuation and polarity
for every variant at 44.1/48/96 kHz; brief gate hold and zero-depth behavior; variable blocks and
automation; exact block-partition invariance; effect replacement, bypass, reset down to a
one-frame maximum, and absence of C++ allocations/deletions on tested slot audio paths.
An analytical mixed NAM/gate/parallel-lanes/compressor render also checks processing order.
The CLAP harness saves/restores all four types across all 16 slots, renders finite nonzero audio
at all three rates with zero reported effect latency, and automates a compressor parameter
through host events. The native Windows editor test restores effects into an open editor,
drags a gate threshold and compressor makeup knob, and switches those slots back to NAM.
The test host now provides the CLAP parameter extension needed by editor automation gestures.

Historical dynamics checkpoint (`1337a7e`), Ryzen 9 5950X / Windows x64 Release,
128-frame blocks, fastest of three timed 128,000-sample
renders after a warm-up (same harness as the grid measurements):

| Host rate | Two NAM models | Two NAMs + Natural Gate + Smooth Compressor | 16 built-ins, no NAMs |
| --- | ---: | ---: | ---: |
| 48 kHz | 7.32% | 7.54% | 1.61% |
| 44.1 kHz | 7.79% | 8.02% | 1.48% |
| 96 kHz | 8.41% | 8.83% | 3.22% |

Percentages are one-core offline elapsed-time cost, not a DAW CPU meter. The mixed chain is
Natural Gate → two A2 fixtures → empty parallel lanes → Smooth Compressor. The 16-effect case
repeats all four variants in every row. Both lanes run on one audio thread. Built-ins add zero
reported latency: the mixed chain keeps the NAM-only 0/54/84-sample conversion latency at
48/44.1/96 kHz; the effect-only chain reports zero at all three rates. These are synthetic
measurements, not a low-buffer underrun test. Reproduce with `nam-clap-bench <plugin.clap>
dual|dual-effects|effects 1 <drive.nam> <amp.nam>` using the smoke test's generated fixtures.

Run the existing build and three CTest suites as described in [GRID.md](GRID.md).
Listening/Bitwig validation, macOS and a VST3 host still need manual testing:

1. Load all variants from slot menus, move knobs and automate them while playing. Match
   compressor bypass loudness and check pick transients, pumping and sustained note tails.
2. Test threshold extremes and low-level playing; sweep Depth down to 0. Test Tight Gate with
   short Decay on palm-muted stops. Turn the original shared gate off for these comparisons.
3. Alternate NAM, gates, compressors and Clear while playing, including resampled models in
   unequal parallel lanes. Confirm no crashes and acceptable switching fades/restarts.
4. Save/reopen a project containing mixed models/effects; confirm type, knob and bypass recall.
5. Change host rate/buffer size; check mono/stereo outputs, CPU, latency and underruns.
