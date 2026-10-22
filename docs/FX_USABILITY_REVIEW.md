# FX usability review, 22 October 2026

The checkpoint before the bottom-editor redesign is commit `6a187cb`.
The current layout removes the outer editor frame and individual parameter cells,
uses two rows of four controls, and gives the visualization a full-height column.
The plot area grows from 160 × 55 to 308 × 154 logical pixels. Controls keep their
physical-slot parameter bindings. The module name opens the full picker; its
separate down-arrow opens the current FX category or NAM/IR folder.

## Control changes

- Conventional delays: 5–4000 ms, quadratic scale (midpoint approximately one second).
- Cosmic Echo: 20–8000 ms, quadratic scale (midpoint approximately two seconds), default 500 ms.
- A cutoff formerly labeled Tone is now High cut in eight FX families. Tilt and native reverb damping/brightness remain distinct controls.
- Multi-head Tape: eight patterns, including Stereo dual and Stereo cascade. Stereo patterns alternate last-head feedback between channels; dry stereo remains intact.
- State schema 22 migrates earlier cutoff names in rigs and performance scenes.

## Displays

Delay previews show relative tap timing, cyan/orange channel routing, modulation
spread and diffusion/saturation settings. Reverb previews focus on early/late
reflections and damping. These are parameter sketches; native Airwindows controls
use relative scales instead of an invented decay calibration.

Dynamics show an orange reduction bar. Compressor history monitors the actual
parallel blend before makeup, while audio and the slot output meter retain makeup.
Smart Gate's reduction indication also follows its noise-reduction Mix.

Distortion, Boost and Tube Boost show gain/saturation and frequency shaping in
separate panels. Boost EQ uses the actual low-cut and opposing shelf coefficients.
Tube EQ is labeled as a small-signal response. Tape shows its shared Spiral stage
and Bump/Mix/Output settings, without presenting a static curve as its full dynamic response.

## Output-level investigation

A direct native CLAP probe found the empty grid and sixteen neutral Utility slots
both within 0.004 dB of unity at 440 Hz. Hot floating-point signals above 0 dBFS
also pass without a hidden global pad or limiter. Serial wet/dry effects can
change level; a centered A/B mix with only one audible lane contributes -6 dB.
Cabinet IR's default -18 dB reference level is another explicit gain stage.

These checks do not reproduce the reported -12 to -20 dB drop without the actual
chain/preset. No global compensating gain has been introduced. The user confirmed
that the comparison used peak levels and the downstream host meter. IN/OUT now
show peak first, with RMS clearly labeled underneath.

Cosmic Echo now exposes all six modes of the Faust Supermassive reconstruction, with the inspiration credited in the editor and per-mode tooltips. Grid bypass controls are larger, equally inset from the top/right corner, and bypassed cards are darker and less saturated.

The eight-second Time extension also requires a longer bypass drain for Sagittarius. The drain is now derived from eight maximum-length lines plus settling margin, preventing a delayed old impulse from returning after re-enable. A deterministic regression first reproduced the failure with the former 60-second drain.

## Verification

All five suites passed in the final cleanup run (566.25 seconds total). The
native CLAP host/editor regression took 566.20 seconds and includes all visible
FX controls, category switching, presets/settings and both former empty-slot
toggle hit areas. The DSP suite passed in 25.67 seconds. Capture-library, WAV
decoding and resampling also passed. Fourteen audition presets load with their
expected mode values. CLAP and VST3 builds use the ordinary non-ASAN configuration.

The native offline six-mode run at 48 kHz/128 frames measured approximately
1.1–2.2% CPU. Eight-second energy-decay fits ranged from about 6.2 to 16.3 seconds;
these are approximate single-run audition measurements, not calibrated decay
controls or DAW performance guarantees. The archive includes the raw CSV.

## Cleanup before the main-branch build

Empty slots now hide their card bypass toggle and the editor's Enabled/step
controls. Native UI checks exercise both former toggle hit areas after clearing
a slot. Category popup and previous/next navigation share one catalog lookup.
Drive previews live in a dedicated drawing helper and reuse sampled endpoints
instead of calculating each curve point twice. The audition exporter takes its
Cosmic/head mode counts from the shared definitions and accepts `heads` to
render all eight tape-head patterns.
