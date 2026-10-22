# External audio-FX source review

> Historical record. Branch names, measurements and completion claims below describe
> their original checkpoint. See the [archive index](README.md) for current guides.

Reviewed 2026-10-20 against the Schwung audio-FX catalog, the SLOOP/FM-1 firmware
and JIANT. No third-party DSP was copied into `nam-grid`. This note records what is
worth adding or improving, what is not, and the licensing constraints behind those
choices.

## Scope and method

- Catalog snapshot: <https://schwung.dev/data/module-catalog.json> (catalog version 2,
  163 modules, 49 entries with `component_type: audio_fx`).
- Checked repository `LICENSE`/SPDX metadata, README claims and the DSP entry point for
  every plausible candidate. Shallow checkouts used for inspection:
  `schwung-shimwash a27d7a2`, `schwung-junologue-chorus 0b1ecd9`,
  `schwung-ruminant d6cdedc`, `schwung-cloudseed fdbc24b`,
  `schwung-stereoid 7efecf1`, `spectra-move c631fff`, `schwung-filter 32d4170`.
- Inspected `isod89/sloop-fm1` (`4ec1ac8`) and `juanjiant-bit/JIANT` (`e9692d7`)
  for feature ideas only.
- Cross-checked current coverage in [MODULES.md](../MODULES.md) and
  [EFFECT_COVERAGE.md](EFFECT_COVERAGE.md).

## Bottom line

`nam-grid` already covers the useful small guitar chain. More low-CPU utility effects
would mostly duplicate what is there. The worthwhile next step is a **quality pass**,
not another inventory expansion:

1. **Shimmer reverb** — the largest missing musical category. It adds pitched feedback
   to the existing reverb vocabulary and is distinct from Room/Plate/Cloud/Spring.
2. **Juno-style BBD chorus** — the current Chorus is a good generic chorus; a faithful
   two-LFO Juno ensemble is a clear quality upgrade at very low cost.
3. **Frequency-shifting delay** — optional, but a compact and genuinely different
   texture that complements Ring Mod and the existing delays.

The SLOOP/JIANT effects are low-CPU because they are fixed-point, 16-bit, shared-bus
firmware effects. They are not a numerical or musical upgrade, and both projects are
GPL-3.0-only, so their code cannot be brought into this MIT plugin.

## Implemented in this branch

The three recommended modules are now implemented as independently written,
rate-scaled double-precision code:

| Module | Enum | Controls |
| --- | ---: | --- |
| Shimmer | 39 | Mix, Decay, Tone, Pitch, Amount, Modulation, Freeze |
| Juno Chorus | 40 | Mode, Mix, Brightness |
| Frequency Shifter | 41 | Shift, Feedback, Delay, Side, Tone, Stereo, Mix |

Local macOS-arm64 CLAP benchmark, 128-frame blocks, full plugin callback, fastest of
three runs (not comparable to the Windows/MSVC figures in MODULES.md):

| Case | 48 kHz | 44.1 kHz | 96 kHz |
| --- | ---: | ---: | ---: |
| Empty grid | 0.59% | 0.53% | 1.11% |
| Shimmer | 1.72% | 1.58% | 3.49% |
| Juno Chorus | 0.72% | 0.67% | 1.41% |
| Frequency Shifter | 0.95% | 0.88% | 1.90% |

All three report zero compensation latency. Shimmer commits delay memory only for
slots that select it (roughly 1.4 MB at 44.1/48 kHz, scaling with host rate). State
schema 11 preserves every earlier named control and fills the 17 new controls from
defaults when loading schema 10.

## License triage

Of the 49 catalog entries at review time:

| License found | Count | Consequence |
| --- | ---: | --- |
| MIT | 22 | Candidates for adapted code with notices |
| GPL-3.0 | 8 | Do not copy into the MIT build |
| AGPL-3.0 | 2 | Do not copy into the MIT build |
| No assertion / no repository LICENSE | 17 | Treat as unlicensed until clarified |

Notable exclusions regardless of how attractive the effect is:

- **GPL/AGPL:** OTTx, Midiverb, MVerb, Tape Echo 2, 4K EQ, CHOWTape, Boris,
  War Bells, Capicola, neon-ingvy audio plugins.
- **No clear license:** Filter (`module.json` says MIT but the repository has no
  LICENSE), PushNPull, Super Boom, Dragonfly Hall, Dissolver, RRVerb-10, Smack,
  Belt, Work, Forgetful, Wayward, TinyVoice, Gresil, Viz.
- **SLOOP/FM-1 and JIANT:** GPL-3.0-only. Their DSP, menus and state code must not be
  copied. Feature ideas such as tape stop, DJ filter, shimmer and grain delay are not
  copyrightable and can be implemented independently.

The catalog description is not a license. Several entries disclose GPL in the author
string; many do not. Always check the repository before adapting anything.

## Shortlist

| Candidate | License | What it would add | Cost / risk | Verdict |
| --- | --- | --- | --- | --- |
| **Shimwash** | MIT | 8-line FDN reverb with two granular pitch shifters, bloom, freeze and duck | Medium/high port effort. About 1.4 MB of delay memory per selected slot; standalone Apple-arm64 benchmark was ~0.4% of one core at 44.1 kHz, defaults | **Add a Shimmer module** |
| **Junologue Chorus** | MIT | Juno-60-style dual triangle LFO BBD chorus, modes I / I+II / II | Small. One 512-sample delay, three one-pole filters; very low CPU | **Add a Juno Chorus module** (or a mode on the existing Chorus, but a separate module preserves current voicing and state) |
| **Ruminant** | MIT | Frequency-shifting feedback delay, allpass Hilbert shifter, soft-saturated feedback | Medium. Source is hard-coded to 44.1 kHz; needs rate scaling and double-precision rewrite | **Optional add** as Frequency Shifter |
| **Stereoid** | MIT | Mono-safe Comb/Haas/Disperse/M-S widening | Small/medium. Existing Utility Stereo is M/S-only and does nothing to a mono source | Optional Utility improvement; Stereo Doubler already covers some of this |
| **CloudSeed** | MIT | 12-line modulated algorithmic reverb | High. Fixed 48 kHz, roughly 18 MB of delay storage per channel before allpasses; overlaps Room/Plate/Cloud | Skip in the current 16-slot design |
| **Spectra** | MIT | Pitch-tracking multiband resonator bank with scale quantiser | High UI/DSP complexity, 8+ controls, FFT and up to 48 SVFs; needs key/scale choices | Later, only if a resonator module is wanted |
| **Structor / Magneto / Ambiotica / Palette** | MIT | Granular deconstruction, cassette looper, ambient chain, multi-FX | Large state/UI/CPU surface; several already overlap Freeze, Stutter and the delays | Not a small grid module; defer |
| **PSXVerb / TAPESCAM / TapeDelay / Munchi Delay** | MIT | Lo-fi PS1 reverb, tape degradation, cheap tape delay, glitch dual delay | Small/medium, but overlap existing Tape/Tape Echo/Dual Delay with lower fidelity or much more specific behaviour | Not a quality upgrade |
| **Bus Driver** | MIT | Ableton Drum Buss model | Drum-bus-specific, not a guitar insert | Skip |
| **Filter** (schwung-filter) | MIT declared in `module.json`, no LICENSE file | SVF/ladder multimode filter with envelope/LFO | Medium; Wah, Envelope Filter and EQ already cover most guitar use | Optional, clarify license first |

## What SLOOP and JIANT actually contribute

The firmware effects are 16-bit fixed point and share a small number of buses. That is
the right engineering choice for an embedded instrument, but it is not the quality
target for `nam-grid`'s double-precision, per-slot processing.

Useful feature ideas not already covered here:

- **Tape stop** (pitch/speed ramp to silence). `nam-grid` has Reverse Delay, Freeze and
  Stutter but no performance brake. It is a small effect, not a quality upgrade.
- **DJ filter** (one bipolar low-pass/high-pass control). Wah is a resonant band-pass;
  a dedicated DJ-style filter would be more useful live than another EQ.
- **Shimmer / resonator / cloud reverbs.** Shimmer is the useful one. The "reso" model
  needs a key/scale and is not a natural fit for a NAM slot. The cloud model overlaps
  the existing Cloud and Freeze.
- **Grain delay** (pitch-shifting grains). Distinct from Reverse Delay and Stutter, but
  shares most of the pitch-shifter work with Shimmer.
- **Slicer with pitch sequence, auto-pan, master limiter.** Limiter and tremolo exist;
  auto-pan and pitched slicer are nice-to-have, not priorities.

Already covered in `nam-grid`: room/plate/cloud/spring reverb, freeze, stutter, reverse
delay, limiter, wah, tremolo, octaver, bitcrusher, chorus/flanger/phaser and the main
delay variants. Do not add a second low-fidelity version of any of these.

## Recommended implementation order

### 1. Juno Chorus (quick, low-risk quality win)

- Three controls plus Enabled: **Mode** (I, I+II, II), **Mix**, **Brightness**.
- Two fixed triangle LFOs at roughly 0.513 Hz and 0.863 Hz, 1.66–5.35 ms delay range,
  inverted modulation on the right channel, mono sum into the delay as on the original.
- Keep the existing generic Chorus module. A separate `Juno Chorus` avoids changing the
  sound and state of existing projects.
- Reference implementation: Junologue Chorus (MIT), but replace its fixed 44.1 kHz
  constants and int16 host API with rate-scaled, double-precision nam-grid code.
- Add the MIT notice to `ThirdPartyNotices.txt` if any code is adapted; an independent
  implementation still benefits from a reference note.

### 2. Shimmer (main quality addition)

- Seven controls plus Enabled, for example: **Mix**, **Decay**, **Tone**, **Pitch**,
  **Amount**, **Modulation**, **Freeze**. Bloom or pre-delay can replace Modulation if
  listening prefers it; seven is a ceiling, not a target.
- Reverb core: 8-line Hadamard FDN with rate-scaled line lengths, four input diffusers,
  damping and slow modulation. Reuse `grid_fx::DelayLine` / `SmoothControls` where they
  fit.
- Shimmer path: two crossfaded granular read heads, one per side, either feeding the
  reverb input (pre) or the reverb output back into the network (feedback). Keep the
  feedback bounded and soft-saturated; freeze must hold without new input.
- The Shimwash algorithm is the MIT reference. Do not copy its fixed 4096-sample line
  sizes or fixed 44.1 kHz assumptions: at 96/192 kHz the lines would become too short.
  Allocate and scale in `Reset`, outside audio, and keep the existing bypass/drain
  policy. A shimmer tail should be included in the 60-second host tail budget, like the
  other reverbs.
- Integration points: `SlotDynamics.h` enum/name, `EffectParameters.h` controls,
  `UtilityControls`, `NAMStage::Prepared`, `ModuleMenu.h`, `NAMGridControls.h` tone and
  help text, state schema/migration, docs and third-party notices.
- Tests: 44.1/48/88.2/96/192 kHz, variable blocks including one frame, zero allocation,
  freeze, finite output, distinct pitch-up/down spectra, bypass drain and CLAP state
  recall.
- Target cost: one slot should stay in the same range as Cloud/Spring, roughly 2–3% of
  one desktop core at 48 kHz. Memory is only committed for slots that actually select
  the module.

### 3. Frequency Shifter (optional creative module)

- Seven controls: **Shift**, **Feedback**, **Delay**, **Side**, **Tone**, **Stereo**,
  **Mix**.
- Allpass quadrature Hilbert shifter plus a quadrature oscillator inside a fractional
  feedback delay. Bound the feedback with the existing soft-saturation approach.
- Ruminant is the MIT reference, but its 44.1 kHz constants and 1-second fixed buffer
  need the same rate-scaling treatment as the other modules.
- This is not a replacement for Ring Mod: fixed-hertz shifting detunes each repeat
  progressively, which is useful for metallic/dub textures rather than static sidebands.

## Improvements to existing modules

- **Tube Boost / drive aliasing:** a 2x oversampled "HQ" path would be a real quality
  improvement, but it adds latency/CPU and a new control. Evaluate it against a good
  NAM drive capture before committing.
- **Utility stereo:** M/S `Stereo` cannot widen a mono source. A Comb or Haas mode
  (Stereoid's approach) would be a useful extension, but Stereo Doubler already covers
  the obvious mono case. Keep this as a low-priority enhancement.
- **Filter:** a general multimode filter or DJ filter would fill a live-performance gap.
  It is easy DSP; the question is whether it earns a seventh control position next to
  Wah and Envelope Filter. Defer until a real playing need appears.
- **Reverbs:** do not replace Room/Plate/Cloud with CloudSeed. The memory cost and
  fixed 48 kHz port would be a regression in this architecture. If more reverb
  quality is wanted later, improve diffusion/modulation inside the existing
  `SlotReverb` and measure it against the current voicing.

## Verification used for this review

- The Shimwash and Junologue sources were built as standalone harnesses on Apple arm64
  with `-O2` and run on 60 seconds of stereo noise at 44.1 kHz. Shimwash measured
  about 0.26 s (~0.4% of one core) at defaults; Junologue measured below 0.1%. These
  are order-of-magnitude checks on a different CPU and are not comparable to the
  Windows/MSVC figures in [MODULES.md](../MODULES.md).
- No `nam-grid` source was changed by this review, so the existing test/CTest results
  remain the relevant validation for the current plugin.

## TAPESCAM comparison

TAPESCAM (MIT, `charlesvestal/schwung-tapescam`) is not just another saturation
module. Its chain is:

```
Input → two-stage GainStage → TapeSat → wow/flutter → hiss → dropouts
      → tilt Tone → compression → stereo width → Output
```

It exposes Input, Drive, Color, Wobble, Noise, Tone, Output, Age, Speed,
Compression and Widen. That is a complete lo-fi tape machine, including
age-dependent hiss/dropouts and three speed modes. `nam-grid` currently splits
those jobs across Airwindows Tape (saturation/head bump), Tape Echo (wow/flutter
inside a delay), Utility (width) and the dynamics modules. TAPESCAM would
therefore be a **broader tape-character insert**, not a direct replacement for
Airwindows Tape.

Standalone benchmark on the same Apple-arm64 host, 60 seconds of stereo noise at
48 kHz, 128-frame blocks, `-O2`:

| Processor | Approx. one-core cost | Notes |
| --- | ---: | --- |
| TAPESCAM (default/medium settings) | 0.30% | Standalone C harness, no plugin overhead |
| nam-grid Airwindows Tape (`fx-23`) | 0.77% net | Full CLAP callback at 48 kHz, empty-grid baseline subtracted |
| nam-grid Shimmer (`fx-39`) | 1.13% net | Full CLAP callback at 48 kHz |
| nam-grid Juno Chorus (`fx-40`) | 0.13% net | Full CLAP callback at 48 kHz |
| nam-grid Frequency Shifter (`fx-41`) | 0.37% net | Full CLAP callback at 48 kHz |

TAPESCAM is cheap because it is float32/int16 at a fixed 48 kHz and uses short
filters and delay lines. A nam-grid port would still be inexpensive, but it
would need three changes before it met the plugin's rules:

1. Rate-scale the wow/flutter, dropout and filter timing for 44.1–192 kHz.
2. Replace the int16 host boundary with the existing double-precision slot path.
3. Trim eleven controls to seven, or drop the secondary menu controls. A sensible
   seven-control grid version would be **Drive, Color, Wobble, Noise, Tone, Age,
   Output**; Compression and Widen could stay at fixed defaults, or Widen could
   give way to the existing Utility module.

**Verdict:** worth adding later if a full tape-degradation character is wanted.
It is not a quality replacement for Airwindows Tape, and it is not part of this
branch's three-module increment. The fixed-48 kHz source should not be dropped in
unchanged.
