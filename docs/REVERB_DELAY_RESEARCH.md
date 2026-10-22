# Reverb and delay research — 2026-10-21

Improve the existing Plate and Room/Hall algorithms before expanding the menu.
For new delay vocabulary, prioritize multi-head tape patterns and a diffused delay.
Keep the familiar reverb families as separate modules; introduce an algorithm
selector only when two proven variants share a useful control set.

The initial recommendations below are engineering judgments about likely value,
not a listening comparison. The authorized implementation now includes distinct
Room/Hall, Plate and Cloud prototypes, two creative delays and three Airwindows
audition modules. See [REVERB_DELAY_AUDITION.md](archive/REVERB_DELAY_AUDITION.md) for the
current controls, rendered examples, measurements and remaining listening work. The
previous external-source pass is in [FX_SOURCE_REVIEW_20261020.md](archive/FX_SOURCE_REVIEW_20261020.md).

## Baseline before this pass

The earlier `SlotTimeEffects.h` implemented Room, Plate, Cloud and Hall using the same
eight-line Hadamard feedback-delay network (FDN). They differ in space scale,
input diffusion, early-reflection contribution, modulation depth and output level.
Stereo uses two differently sized networks with a cross-channel input matrix.
These are meaningful voicings, but not four fundamentally different tank designs.

That input diffuser used four serial allpasses; Room bypassed it. Early
reflections are three fixed-relative-time taps. Decay is set per delay line,
while high-frequency damping is a one-pole loop filter. These leave room for
better reflection buildup, low/mid/high decay relationships and modulation that
decorrelates without obvious pitch wobble. More line count alone is not a quality
criterion.

Spring uses a separate dispersive design, and Shimmer has its own pitched-feedback
path. Neither should be collapsed into an all-purpose FDN merely for code reuse.

Digital, Analog and Tape delays share a fractional-delay engine. Analog adds
stronger filtering; Analog/Tape add nonlinear feedback and periodic movement.
They are useful original voices, not detailed BBD or tape-machine simulations.
Dual Delay and Reverse have separate processing. A multi-head feedback pattern
or diffusion network would therefore add more than another dark/bright preset.

## Reverb candidates

| Candidate | Relevant evidence | Proposed role and work |
| --- | --- | --- |
| **Dattorro/Griesinger tank** | Mutable Instruments Clouds has an MIT implementation with four input allpasses and a cross-coupled two-branch tank, with modulation. Its engine assumes 32 kHz and compressed delay storage. [Source](https://github.com/pichenettes/eurorack/blob/master/clouds/dsp/fx/reverb.h) | First Plate replacement prototype. Write a rate-scaled floating-point core; retain topology and attribution if adapting. Moderate effort; structurally compact. |
| **Signalsmith Basics reverb** | MIT; stereo float/double variants, multistage diffusion, early reflections, feedback detuning, separate low/high damping and crossfaded delay changes. Its Decay is RT20. [DSP](https://github.com/Signalsmith-Audio/basics/blob/main/include/signalsmith-basics/reverb.h), [license](https://github.com/Signalsmith-Audio/basics/blob/main/LICENSE.txt) | Strong Room/Hall replacement candidate and quality reference for our FDN. Moderate integration: separate its STFX wrapper or adapt it, convert RT20 to our RT60 convention, and audit fades/reset. |
| **JPverb** | Explicitly MIT in Faust; a modulated, diffused stereo design aimed at lush synthetic ambience. [Source and declarations](https://github.com/grame-cncm/faustlibraries/blob/master/reverbs.lib), [reference](https://faustlibraries.grame.fr/libs/reverbs/#rejpverb) | Cloud replacement candidate. More appropriate to lush/blooming ambience than realistic rooms. Audit helper licenses and rate-dependent buffers before any port. |
| **Airwindows kPlate140** | MIT; a specifically tuned 5×5 Householder design, nonlinear bass treatment and internal undersampling. Its wet law includes full dry plus full wet at center. [Design](https://www.airwindows.com/kplate140/), [DSP](https://github.com/airwindows/airwindows/blob/master/plugins/LinuxVST/src/kPlate140/kPlate140Proc.cpp) | Alternative Plate audition candidate. Adapt the DSP rather than VST2 wrapper; standardize outer mix, smoothing, deterministic reset and sample-rate behavior. Higher integration uncertainty than the compact tank. |
| **Airwindows kGuitarHall2** | MIT; dedicated guitar hall with adjustable early-reflection selection and revised internal undersampling. [Author](https://www.airwindows.com/kguitarhall2/), [DSP](https://github.com/airwindows/airwindows/blob/master/plugins/LinuxVST/src/kGuitarHall2/kGuitarHall2Proc.cpp) | Hall audition candidate if a colorful guitar space is preferred to a neutral room. Evaluate actual memory, CPU and automated changes before choosing it. |
| **Zita-style FDN** | The Faust implementation adds allpass combs and separate bass/mid decay control. [Description](https://faustlibraries.grame.fr/libs/reverbs/#rezita_rev_fdn) | Reference for improving frequency-dependent decay in the existing Hall. Do not assume another eight-line FDN automatically sounds better; test it against Signalsmith and the current core. |

Suggested replacement map:

| Existing module | Next prototype | Keep the musical identity |
| --- | --- | --- |
| Room | Signalsmith-style diffusion/early reflections | Fast buildup, unobtrusive short ambience and clear attacks |
| Dense Plate | Dattorro-style tank; compare kPlate140 | Dense, smooth sustain, relatively weak positional early reflections |
| Hall | Improved FDN; compare Signalsmith and kGuitarHall2 | More spacious onset, smooth long decay, controlled bass |
| Cloud | JPverb-style diffusion/modulation | Slower buildup and deliberately lush motion |
| Spring | Retain specialized core; improve after tank-recording comparison | Dispersive drip and driven excitation, rather than a small generic room |
| Shimmer | Retain pitch path; evaluate a better underlying tank separately | Pitched feedback, Freeze behavior and stereo stability |

This map is a proposal for comparison, not a decision to import six dependencies.
Room/Hall can still share infrastructure and even a core where it earns its place;
Plate should be allowed to have a different topology.

## Delay candidates and quality changes

| Candidate | Why it adds something | Scope and evidence |
| --- | --- | --- |
| **Multi-head Tape Echo** | Rhythmic tap combinations from one moving delay buffer, with filtering/saturation in a deliberately chosen feedback path | First new delay style. The BOSS RE-202 is a useful interaction reference: heads at integer multiples of the base time and selectable combinations. This is a behavior reference, not available DSP to copy. [Official manual](https://static.roland.com/assets/media/pdf/RE-202_reference_eng01_W.pdf) |
| **Diffused Delay / Greyhole-style** | Discrete echoes gradually smear into reverb-like clouds; diffusion changes transient shape rather than only treble | First ambient delay candidate. Greyhole is explicitly MIT in Faust; audit helpers and fixed buffer limits. [Source](https://github.com/grame-cncm/faustlibraries/blob/master/reverbs.lib), [author-facing documentation](https://doc.sccode.org/Classes/Greyhole.html) |
| **Time-change behavior** | Digital time/tempo jumps can crossfade between fixed read positions, while Tape deliberately bends pitch | Useful quality improvement to existing delays. Smoothed moving reads and crossfades are different musical behaviors. [Faust smooth-delay reference](https://faustlibraries.grame.fr/libs/delays/#desdelay) |
| **Improved BBD/interpolation** | A real clocked BBD model or higher-quality interpolation would distinguish Analog from a simple filtered digital delay | Investigate after rhythmic/diffused modes. ChowMatrix documents BBD, sinc, pitch and diffusion modes; repository code is BSD-3-Clause, but dependencies need their own audit. Extract only suitable DSP, rather than adding its full JUCE plugin. [Project](https://github.com/Chowdhury-DSP/ChowMatrix), [modes](https://github.com/Chowdhury-DSP/ChowMatrix/releases) |

For multi-head echo, prototype a few named patterns such as **Single**, **Dual**,
**Triplet** and **Cascade**, with their actual tap ratios visible. Normalize or
voice feedback deliberately: summing multiple taps into one feedback write can
increase loop gain and destabilize settings that were safe with one head.
One buffer need not mean one feedback topology.

A diffused delay fits seven controls: **Time, Feedback, Diffusion, Tone,
Modulation, Mix, Output**, plus tempo sync. This is a proposed interface, not
Greyhole's exact parameter list. Time should describe a repeat interval;
Diffusion should control smearing, with a diagram showing widening echo clusters.

## Separate modules or one algorithm selector?

Valhalla's author explicitly describes modes as different algorithms, rather
than parameter presets. This is a useful design precedent; its commercial DSP
is not a source implementation for us. [Explanation](https://valhalladsp.com/2022/10/21/the-more-you-know-valhalla-modes-algorithms/)

In a standalone reverb plugin, the user already chose the reverb category, so a
top-level algorithm selector is natural. In this grid, the Add Module menu already
performs that choice. Keep **Room, Plate, Hall, Cloud, Spring and Shimmer** there.
Another all-family selector in each module would repeat the decision and obscure
which controls apply.

Use a small **Style** selector within a family only when at least two worthwhile
algorithms have the same parameter meanings. **Hall: Clear / Lush** is a possible
future example; technical algorithm names need not appear in the UI. If a voice
needs different controls or a different musical task, make it a separate module.
Do not add styles until listening establishes the distinction.

Implementation can still use shared reverb controls and a private algorithm
factory. Switching algorithms must prepare storage off audio, preserve each
slot's values, crossfade safely and define whether the old tail carries over.
Whole-module replacement already has a preparation/fade mechanism; an in-module
automatable style switch needs its own tail and transition contract.

Valhalla also says it reserves algorithm capacity for future additions. That
validates reserving internal identifiers/storage. The article does not establish
its host automation encoding. In our current iPlug implementation, exposing a
16-entry enum creates selectable spare values; keeping only implemented choices
does not freeze VST3 normalization. Resolve release versioning explicitly rather
than relying on an arbitrary reserve of 16. [Mode/container discussion](https://valhalladsp.com/2022/10/21/the-more-you-know-valhalla-modes-algorithms/)

## Selection and verification

Start with three audition branches: **Plate tank**, **Room/Hall diffusion**, and
**multi-head or diffused delay**. Choose replacements by level-matched listening
and measurements, then expose only the winners. Avoid adding another general
reverb module simply to keep both an old and a new implementation in the menu.

For each prototype:

- Render identical impulses, muted pick attacks, sustained clean chords and
  distorted guitar at 44.1/48/96/192 kHz. Keep dry gain and wet loudness comparable.
- Check reflection buildup, broadband and band-limited decay, tail resonances,
  modulation sidebands, stereo correlation and mono collapse. Measure requested
  RT60 rather than assuming a feedback knob calibrates it.
- Test repeated resets, tiny/uneven blocks, all parameter extremes, silent input,
  rapid automation, bypass drain and tails; guard audio allocation/deletion.
- Benchmark one selected slot and a plausible multi-slot chain at low buffers.
  Source topology is insufficient evidence for a CPU claim.
- Tie the display to the chosen algorithm: reflection density/decay for reverb,
  tap timing/routing/smearing for delays. Label illustrative setup curves; use
  actual output histories or measured envelopes when claiming live feedback.

## Source boundaries

Check licenses per file. Clouds and Signalsmith above carry permissive notices;
the Faust file explicitly marks JPverb/Greyhole MIT but has mixed licensing in
other sections. Its Dattorro section has a different explicit function-level
license: the source declares `LicenseRef-STK-4.3` for
`dattorro_rev_default`. Our Plate adaptation uses the separately credited MIT
Clouds source instead.

Plateau's repository is GPL-3.0-or-later; Dragonfly is GPL-3.0. DaisySP's
ReverbSc is in its LGPL-2.1 component despite the main DaisySP library being MIT.
These are useful comparisons but not direct adoption choices for this permissive
build without a separate licensing decision. [Plateau license](https://github.com/ValleyAudio/ValleyRackFree/blob/main/LICENSE.md),
[Dragonfly](https://github.com/michaelwillis/dragonfly-reverb),
[ReverbSc header](https://daisy.audio/DaisySP/reverbsc_8h_source/).

## Faust's Supermassive reconstruction

Worth a separate audition prototype. The current `re.valhallaSupermassive` is
David Braun's independent model of six modes, with an explicit MIT declaration.
It is not Valhalla's original source and does not cover all of the official
plugin's modes. Its implementation notes identify differences in minimum delay
clamping and feedback limits. [Faust source](https://github.com/grame-cncm/faustlibraries/blob/master/reverbs.lib),
[function reference](https://faustlibraries.grame.fr/libs/reverbs/#revalhallasupermassive).

| Available mode | Audition value |
| --- | --- |
| Gemini (0) | Fast onset and dense, smooth ambience; useful reference against Cloud/Galactic |
| Centaurus (2) | Blooms and sustained spaces, with attack coupled to Feedback |
| Sagittarius (3) | Especially slow buildup; a stronger contrast to the existing halls |
| Great Annihilator (4) | Very long decays; optional fourth audition style if Sagittarius is insufficient |
| Lyra (6) | Sparse space echoes; likely overlap with Virgo |
| Virgo (19) | Sparse echoes with filters inside the feedback path; best sparse shortlist candidate |

The listening shortlist is **Gemini / Sagittarius / Virgo**. The descriptions
come from the official algorithms; whether the reconstruction delivers the same
strengths needs direct listening. [Valhalla mode descriptions](https://valhalladsp.com/shop/reverb/valhalla-supermassive/).

One ambient echo/reverb module with a small Style selector makes sense here:
the candidates share Delay, Warp, Feedback, Density and modulation/filter controls.
That differs from putting unrelated Spring, Plate and Hall controls behind a
single selector. Galactic3 remains a separate native-control candidate until
audition establishes overlap. Use an original module name and credit both the
reconstruction author and the design references.

Integration should preserve the meaningful interaction: Delay establishes the
time scale, Warp spreads line lengths, Density changes scattering and Feedback
changes both decay and, in some modes, attack. Do not relabel Feedback as seconds.
There are more native controls than our seven-knob row; decide which belong in
the header or an expanded view before hiding important sonic choices. A live wet
envelope plus a delay-length/scattering sketch would explain this better than
our conventional exponential reverb sketch.

The Faust mode argument is compile-time. A runtime selector needs prepared core
instances and a defined transition/tail policy, rather than changing an integer
in a generated core. Also audit helper licenses, maximum-rate buffer memory,
feedback near unity, zero/short Delay with strong modulation, and automation of
moving allpass-interpolated delays. The first nine-choice audition package
predates this prototype. The subsequently authorized three-style C++ adaptation
is now [Cosmic Echo](COSMIC_ECHO.md). It reuses one prepared buffer bank and
defines a fade/clear/fade transition, rather than instantiating all three
generated Faust cores in parallel.
