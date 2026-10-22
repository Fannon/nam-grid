# FX visualization audit

Reviewed all 55 catalog FX against their DSP and parameter definitions. This pass
changes editor drawings and tests, without changing sound, defaults, parameter
ranges, IDs or saved state. The legacy Tight Gate uses the Noise Gate view.

## Shared behavior

- The 332 x 212 panel reserves separate title, explanation, chart and footer rows.
  Text is at least 13, titles 14; chart lines cannot overwrite the footer.
- Module colour identifies processed signals; gray is a dry/reference signal.
  Stereo previews use module colour for L and amber for R. Amber also identifies
  gain reduction/thresholds and early-reflection guides in their labeled contexts.
- Frequency responses use logarithmic frequency, with frequency limits and dB
  limits labeled. Time previews state their window. Reverb guides explicitly say
  logarithmic or relative time. Level histories say older-to-now rather than
  promising an exact duration when host blocks and GUI queueing vary.
  Delay/reverb time windows use stable 1/2/5 steps, so small Time/Decay changes
  move the marks rather than immediately zooming back to an identical picture.
- Curves describe settled controls; previews do not animate an invented audio
  phase or infer input spectra. Measured histories explicitly identify themselves.
  A bypassed module still shows its configured preview; Enabled controls the audio.
- Settings' visualization switch still hides the whole panel. No extra audio
  analysis, allocations or messages are introduced on the audio thread.

## Module-by-module findings

| Module | Useful interpretation and audit result |
| --- | --- |
| Distortion | Keep the actual static shaper and live input-range markers. Separate tone/mix/output response remains distinct from nonlinear gain. Label both scales and contain high-drive/bias curves inside the chart. |
| Boost | Keep the gain line and full tilt/low-cut response; label the different transfer and frequency scales. |
| Tube Boost | Keep the actual biased shaper and small-signal EQ/mix response. Clearly label the small-signal assumption. |
| Airwindows Tape | Keep the Spiral-stage curve and head-bump setup. Explicitly identify the core's memory as outside the static curve; show mix/output. |
| Console Colour | Keep measured IN/OUT: sag and FIR histories cannot be represented by one honest static transfer. Desk remains in the caption; identify output as including colour and trims. |
| Bitcrusher | Replace generic levels with a 220 Hz reference processed through the actual quantization, sample-hold, high-cut, mix and output equations. Bits change vertical steps; sample rate changes hold intervals; tone rounds them. |
| Noise Gate | Add the configured closing characteristic beside live levels/threshold and GR. Natural expansion and Tight depth differ visibly; this is a closing guide, not a simulation of hold/hysteresis. |
| Noise Gate (Smart) | Retain measured levels and gate line. Spectral suppression is input/profile dependent and must not be drawn as a static full-band transfer. Gate Off remains explicit. |
| Digital Black Gate | Keep actual attenuation and measured IN/OUT, and add a threshold guide. Native zero-crossing/hysteresis behavior is deliberately not reduced to a generic compressor curve. |
| Smooth Compressor | Add the actual 2.5:1 soft-knee static curve, including parallel mix and makeup; retain live reduced history without makeup and GR. |
| Punch Compressor | Same conventions, using its 4:1 ratio and narrower knee. Static and measured plots remain separately labeled. |
| Butter Comp | Keep native detector GR and measured IN/OUT including automatic recovery/output gain. Avoid inventing a fixed ratio or threshold for the Compress control. |
| Outboard Channel | Keep native detector GR and final IN/OUT including cuts, saturation, mix and output. Use the shared measured-level legend and GR scale. |
| Pressure | Keep native linked-detector GR and actual nonlinear output levels; do not present its intensity dial as a calibrated ratio. |
| Peak Limiter | Add ceiling characteristic beside live linked reduction/levels; release remains labeled. The curve describes steady peaks, not release memory. |
| Guitar EQ | Keep combined response from the DSP coefficient classes; add frequency and dB scale labels. |
| Cut EQ | Same, including cut bypasses and Presence. |
| Parametric EQ | Keep bell/notch plus cuts, exact center sample and Q sensitivity; label combined response and scales. |
| Console EQ | Keep signed native band-density bars, frequency labels and cut amounts. Do not portray its level-dependent density as a linear EQ curve. |
| Filter | Keep actual applied L/R mode/cutoff/Q responses; add markers for configured resting and fully opened envelope cutoffs, including DSP clamps. |
| Wah | Replace levels with the exact bilinear bandpass/body/mix/output response and center marker. Position/Register move the peak; Resonance narrows it. |
| Chorus | Keep configured L/R delay trajectories; use consistent stereo colours and state the millisecond range. Mix changes wet prominence, not delay excursion. |
| Juno Chorus | Keep actual I/II triangle rates and opposite L/R delay trajectories; identify brightness and wet prominence. |
| Flanger | Correct phase snapshots to include texture harmonics, tone shaping, fractional-delay interpolation, feedback sign and parallel mix. Label these as frozen small-signal responses. |
| Phaser | Keep six allpasses and one-sample feedback delay, with explicit sweep phases and frequency/dB limits. |
| Auto Tremolo | Keep gain shape with depth/mix. Apply mix to the faint loud-note-rate guide too, so Mix zero shows no residual gain modulation. |
| Vibrato | Keep delay excursion; identify pitch movement and milliseconds. The core is fully wet, independent of the shared hidden Mix. |
| Rotary | Keep horn/drum gain trajectories and stereo offsets; identify them as settled speed and expose ramp/drive context. Thin lines are drum, stronger lines horn. Doppler/saturation are not a measured waveform. |
| Uni-Vibe | Keep the four unequal stage-frequency sweeps; identify frequency scale, mode and wet amount. Do not claim to display the live optical phase. |
| Ring Modulator | Include both Mix and high-cut response in the carrier/gain preview. Label the DC-input reference: this is not a detected waveform or input-dependent sideband spectrum. |
| Mono Octaver | Keep independent dry/-1/-2 layer bars with a fixed 0–2x scale. Output no longer silently saturates all high bars at the old arbitrary cap. Tone is explicitly for subs. |
| Stereo Doubler | Keep pitch separate from width/mix. Correct the L-up/R-down convention; label pitch versus horizontal placement, delay, mix and output. |
| Frequency Shifter | Replace misleading one-path feedback harmonics with first-pass sidebands on a 220 Hz harmonic reference, including tone attenuation. Feedback mixes paths on later passes; show delay/feedback/spread as settings, not a fabricated later spectrum. |
| Digital Delay | Fixed -60 to +12 dB repeat scale makes mix/output visible. Bright inner stems show surviving 5 kHz content; L/R arrivals remain distinct. Ducking is signal dependent and excluded from the steady-repeat guide. |
| Analog Echo | Same, including its two-pole age-adjusted treble loss. Motion halos and saturation are setup cues, not an impulse measurement. |
| Tape Echo | Same, with separate saturation/motion context. |
| Multi-head Tape | Keep physical quarter-time head positions and selected gains. First/last-head timing and last-head feedback are unchanged. The guide assumes the same impulse on both inputs; cross-feedback does not exchange output head assignments. |
| Reverse Delay | Show complementary seam blending and reverse arrows with the full window period. Do not imply a repeated amplitude gate. |
| Dual Delay | Keep ratio, tap balance and alternating cross-feedback timing. Offset simultaneous L/R stems so one channel no longer hides the other. |
| Diffused Delay | Keep repeat timing plus explicitly illustrative diffusion clusters and motion halos. Share level/treble scale with other delays. |
| Room | Retain early-spacing and late-decay guides; replace an unlabeled linear-amplitude triangle with a nominal -60 dB decay guide and explicit logarithmic time. |
| Dense Plate | Same guide convention; topology-dependent early marks remain illustrative rather than an impulse response. |
| Cloud | Same, with larger configured spacing and a longer labeled window. |
| Hall | Same, preserving its longer reflection spacing. |
| Spring Reverb | Keep decay/damping/dispersion setup; identify Dwell/Drip and output context. Marks describe the tank qualitatively, not measured springs. |
| Shimmer | Keep decay and requested frozen-tail view. Pitch arrow direction, length and prominence follow pitch/amount; omit it when pitch feedback is inactive or frozen. |
| Airwindows Plate | Keep a relative tank guide; Sustain/Resolution are native controls, not seconds/RT60. Label wet/output and relative time. |
| Airwindows Guitar Hall | Same relative convention; Early level/Position affect the early guide. No calibrated room dimensions are implied. |
| Galactic Space | Include Resolution in the relative timing guide alongside Size. Replace/brightness/detune remain qualitative network controls; no invented RT60. |
| Cosmic Echo | Keep exact core line times/warp, density connections and motion halos. Correct the legend: line groups are not direct L/R output taps in this mixed feedback network. |
| Freeze / Hold | Show complementary seam weights and actual shortened period. Explicitly retain full dry plus additive held level; requested Hold is not proof a capture is ready. |
| Stutter | Show wet gate/edge fades including dry blend over three labeled windows. Mix zero becomes flat full gain. |
| LoFi Tape Machine | Keep measured wow/flutter cents; align surface/rows with other plots, label the 2.56 s history and explain Speed's bandwidth role. Noise/wear are not misrepresented as pitch. |
| Utility | Keep M/S width, balance and polarity setup; identify L/R and common delay. Muted channels lose prominence; mono input is explicitly unaffected by Width. |
| Auto Swell | Identify the trigger/re-arm example and 40 ms hold. Rise is linear, release exponential; release does not automatically begin when the rise completes. |

NAM and Cabinet IR retain labeled measured IN/OUT levels; an unknown capture or
IR spectrum cannot be derived from dials. Their history is not a frequency response.

## Verification

The DSP suite compares Wah, Flanger/Phaser, Ring Mod and Bitcrusher previews with
actual processor output at 44.1/48/96 kHz, including tone, mix, quantization and
fractional-delay behavior. Existing EQ, distortion-tone, delay/head timing and
Cosmic timing regressions remain in place.

On Windows, `NAM_GRID_FX_VISUALS=1` selects a focused native catalog pass in
`nam-clap-smoke`: render each module with its musical defaults and audio, then
minimum/maximum settings. The optional screenshot directory receives full-editor
BMPs for layout review. This is an opt-in rendering check, not a replacement for
the ordinary full editor regression or listening in a DAW.
