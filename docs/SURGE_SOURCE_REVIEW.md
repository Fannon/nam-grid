# Surge source review: licence verdict and one useful find

Reviewed 2026-10-21 against `surge-synthesizer/surge` `main` at
`a30341be00484d3de6b65869998100ccc6db6945` (2026-10-20), plus the repository
licences of the `surge-synthesizer` organisation queried through the GitHub API.
This note is a companion to [AIRWINDOWS_CANDIDATES.md](AIRWINDOWS_CANDIDATES.md):
it answers "is there anything in Surge worth taking for the grid?" and records
the one thing worth taking, which turns out not to be Surge code at all.

## Verdict

**No DSP code from Surge or its `sst-*` libraries can be adopted.**
Surge XT is GPL-3.0-or-later, and the DSP that was factored out of it for reuse
is GPL too:

| Repository | Licence | Usable here |
| --- | --- | --- |
| `surge` | GPL-3.0-or-later | No |
| `sst-effects` | GPL-3.0 | No |
| `sst-filters` | GPL-3.0 | No |
| `sst-waveshapers` | GPL-3.0 | No |
| `sst-basic-blocks` | GPL-3.0 | No |
| `surge-fx`, `surge-rack`, `SpectrumWorx`, `OB-Xf`, `stochas` | GPL-3.0 | No |
| `chowdsp_*` modules vendored inside `surge` | GPL-3.0 (relicensed) | No |
| `tuning-library`, `sst-cpputils`, `sst-cmake`, `sst-jucegui`, `sst-clap-helpers`, `FFTConvolver`, `sst-oscillators-mit`, `shortcircuit-xt` | MIT | Only with a use case (below) |
| `sst-plugininfra` | has both `LICENSE.md` and `LICENSE_MIT.md` | Per-file check only |

The effects that would have been interesting as *code* are all on the wrong side
of that line. Anything wanted from them has to be written from the published
technique or from scratch — see "Licence-safe techniques" below.

The library this plugin already uses for DSP structure, AudioDSPTools/LEAF, plus
Airwindows, remain the only code sources.

## The find: Surge XT vendors the whole Airwindows catalogue (MIT)

`libs/airwindows/` in Surge XT contains **76 adapted Airwindows effect sources**
with Airwindows' own MIT `LICENSE` (`Copyright (c) 2018 Chris Johnson`) beside
them. Their own note: "This code is an adapted version of the Airwindows plugins
… released under the MIT license". The adaptation is mechanical: the code is
wrapped in a namespace, `audioeffectx.h` is swapped for a minimal shim, and
`getParameterDisplay` gains two arguments (`libs/airwindows/grab.py` or
`grab.pl` per plugin).

So the Airwindows half of that tree is MIT and *is* legally usable. Three
reasons to care, and one reason not to use it as a source.

### 1. It is an independent second opinion on our candidate list

Surge ships these Airwindows effects (grouped as their preset folder groups them):

- **Ambience**: Bright Ambience, Chamber, Galactic, Infinity, Matrix Verb, Melt,
  Nonlinear Space, Pocket Verbs, Star Child, Verbity
- **Clipping**: AD Clip, One Corner Clip, Slew 1, Slew 2
- **Dynamics**: Block Party, Butter Comp, Compresaturator, Drum Slam, Logical,
  Point, Pop, Pressure, Pye Wacket, Surge, Vari-Mu
- **Filter**: Air, Cabs, Capacitor, De-Bess, Dub Center, Dub Sub, Hombre, MackEQ,
  Pafnuty, Y/Z-series filters
- **Lo-Fi**: Bit Glitter, Crunchy Groove Wear, DeRez, Deck Wrecka, Groove Wear,
  To Vinyl
- **Noise**: Dust Bunny, Noise, Tape Dust, Voice Of The Starship
- **Pitch**: Glitch Shifter
- **Saturation And More**: Apicolypse, Bass Drive, Buss Colors, Cojones, Density,
  Drive, Fire Amp, Focus, Fracture, Hard Vacuum, Loud, Mackity, Mojo, NC-17,
  Power Sag, Single-Ended Triode, Spiral, Unbox
- **Stereo**: Triple Spread
- **Tape**: Chrome Oxide, Iron Oxide, To Tape

Two things stand out against our own list:

- **Guitar-voiced Airwindows plugins we never considered.** `PointyGuitar`,
  `PunchyGuitar`, `ChimeyGuitar2` and their `*Deluxe` siblings (10 controls each,
  with `HSpeaker`/`LSpeaker` resonance controls), `Cabs`/`Cabs2`, plus the
  amp-character plugins `FireAmp`, `BassDrive`, `SingleEndedTriode`,
  `HardVacuum`, `Mojo`, `Hombre`, `PowerSag`. For a guitar plugin these are the
  closest upstream equivalents to what this project is, and the current
  candidates note does not mention any of them.
- **Console/buss colour done as a single insert.** `BussColors4`
  (`Color / Input Trim / Output Trim / Dry-Wet`, 8 desk voices: Dark, Rock, Lush,
  Vibe, Holo, Punch, Steel, Tube) and `Desk4`
  (`Overdrive / Hi Choke / Power Sag / Frequency / Output Trim / Dry-Wet`) are
  the single-slot console colours. That matters because, as the author states on
  the Console9 page, Console is "pre-distorted and then un-distorted on the buss
  after summing, for a sound that (in theory) doesn't touch the tones of any
  individual mix element" — i.e. a Console9 Channel→Buss round trip inside one
  slot cancels. `BussColors4` is the standalone colour that survives that
  restriction.

Their curation is also *narrower* than Airwindows: no CStrip/CStrip2, no
Console/Desk family except Buss Colors, no kPlate/kGuitarHall, no
GuitarConditioner/Hype/EveryTrim/Weight/Inflamer, no Console9, no Verbity2 or the
VerbSixes/VerbSevens matrix family, and only ToTape6 of the ToTape line. Treat it as a filter, not a catalogue.

### 2. It shows a different integration strategy

| | Surge XT | This repo |
| --- | --- | --- |
| Module shape | one "Airwindows" effect with a **Type** selector plus 32 parameter slots | one bespoke module per plugin |
| Upstream code | kept verbatim, namespaced | re-derived per plugin into double precision |
| Controls | the plugin's own parameters, whatever the count | at most seven, chosen and renamed |
| Smoothing | 4 ms lag on every parameter, processed in 8-frame subblocks | per-sample smoothed wrapper controls |
| Presets | one factory preset per effect (a `p0` index plus default values) | [EFFECT_DEFAULTS.md](EFFECT_DEFAULTS.md) |

Neither is better in the abstract: the selector approach scales to hundreds of
plugins but cannot offer the grid's fixed eight-position editor, help text,
displays or per-module defaults. It is worth noting only as the alternative we
deliberately rejected for UI reasons, not for DSP reasons. Their
`subblock_factor`/4 ms lag is a cheap trick worth keeping in mind for any future
module with many controls.

The factory presets are not useful as voicing references — they are just "this
effect, at its default values" (`Butter Comp.srgfx` is
`p0="2" p1="0.5" p2="1"`), so their value is curation evidence only.

### 3. …and why not to take the source from there

The vendored snapshot is stale relative to `airwindows/airwindows` `master`.

| Surge vendors | Current upstream |
| --- | --- |
| ADClip7 | ADClip9 |
| BrightAmbience2 | BrightAmbience3 |
| DeRez2 | DeRez5 |
| ChromeOxide, ToTape6, ToVinyl4 | ChromeOxide, ToTape9, ToVinyl4 |
| Galactic | Galactic3 (already ported here) |
| Slew, Slew2 | Slew…Slew4, EverySlew, GoldenSlew, SlewOnly |
| Pressure4 | Pressure4, Pressure5, Pressure6 |
| Density | Density, Density2, Density3 |
| Cojones, ButterComp2, Compresaturator, NonlinearSpace, Air, Cabs | same names, later revisions |

Also note their vendored files are read from Airwindows' **MacVST** `source/`
folders, whereas this repo's generator reads `LinuxVST/src/`, so line-for-line
comparison is not direct. Use `airwindows/airwindows` at a pinned revision, as
`scripts/vendor-airwindows-reverbs.py` already does.

One licence trap: `libs/airwindows/src/*` is MIT (Airwindows copyright), but the
*wrapper* files beside it — `include/airwindows/AirWinBaseClass.h`,
`src/AirWinBaseClass_pluginRegistry.cpp`, and
`src/common/dsp/effects/airwindows/AirWindowsEffect.cpp` — are Surge team code
with no separate MIT grant, so they must be treated as GPL-3. Our own
`vendor-airwindows-reverbs.py` generator already produces an equivalent shim, so
nothing is lost.

## Licence-safe techniques worth reimplementing

Published algorithms, implemented by us, are not derivative code. Three from the
Surge ecosystem that map directly onto known weaknesses here:

1. **Antiderivative anti-aliasing (ADAA).** `sst-waveshapers` (GPL) applies
   first-order ADAA to its non-linear stages. [MODULES.md](MODULES.md) already
   documents that Distortion and Tube Boost "can alias" at high drive with no
   oversampling. ADAA is a published technique (Parker et al., 2016) that halves
   the problem for a first-order shaper at low cost, with no oversampling and
   therefore no added latency. This would improve an existing family rather than
   add a new slot.
2. **Trapezoidal state-variable filter (Cytomic/Simper).** Surge's
   `ParametricEQ3Band` and `sst-filters` use it; the equations are published.
   The grid's Parametric EQ uses RBJ biquads whose coefficients are recomputed
   from the smoothed parameters, which is fine for static settings but gives
   zipper-free-by-construction behaviour only because of the wrapper's
   per-sample smoothing. A TPT SVF stays stable and continuous under fast
   automation and has a cleaner band-pass. Worth considering the next time
   Parametric EQ is touched, not as a new module.
3. **Clipper and mid/side patterns.** `sst-basic-blocks/dsp/Clippers.h` and
   `MidSide.h` are tidy, but the grid already has equivalents
   (`Peak Limiter`, `Utility` width, `Extreme`/`Distortion` clamps). Nothing to
   gain.

Explicitly *not* worth borrowing: Surge's halfband oversampling (heavier than a
guitar module needs), its `BLEP` oscillator machinery (synth-only), and its
`Tunings`/MTS-ESP support — `GridTuner.h` here is a *pitch detector* (MPM key-peak
selection over the last 1536 samples), not a microtuning engine, so
`tuning-library` has no use case here.

## MIT repositories in the org, evaluated

| Repository | What it would give | Verdict |
| --- | --- | --- |
| `FFTConvolver` (MIT, HiFi-LoFi) | partitioned FFT convolution | Not needed yet. `dsp::ImpulseResponse` here is a direct 8192-tap FIR; guitar cab IRs fit comfortably and stay latency-free. Revisit only if long reverb IRs are ever wanted, where the FFT latency would have to be reported. |
| `tuning-library` (MIT) | Scala/MTS-ESP parsing | No use case: see above. |
| `sst-cpputils`, `sst-cmake`, `sst-clap-helpers`, `sst-jucegui` | build/GUI plumbing | Not applicable: iPlug2, CMake and CLAP are already in place. |
| `sst-oscillators-mit` | oscillator DSP | Synth-only. |
| `shortcircuit-xt` (MIT) | sampler | Its DSP dependencies are the GPL `sst-*` libraries. No. |

Adjacent, outside Surge but from the same ecosystem: **`chowdsp_wdf` is
BSD-3-Clause** (verified: `Copyright (c) 2022, Chowdhury-DSP`). It is a wave
digital filter library, i.e. component-level circuit modelling. That is a
different modelling philosophy from NAM captures — a real path to a
component-accurate Tube Screamer/Rat/Fuzz if that is ever wanted. Note the
distinction: `chowdsp_utils` modules are **per-module licensed** and the ones
that matter here (`chowdsp_waveshapers`, `chowdsp_reverb`, `chowdsp_compressor`)
declare **GPLv3**, so Surge's Spring Reverb, Exciter, CHOW and Tape effects are
not usable as code; only `chowdsp_wdf` is permissively licensed.

## What this means for the Airwindows plan

- Nothing here changes the Airwindows licence position: still MIT, still fine.
- Add **`BussColors4`** to the console/outboard row of
  [AIRWINDOWS_CANDIDATES.md](AIRWINDOWS_CANDIDATES.md). It is the standalone
  console colour, it fits the existing header-mode (Style) pattern, and it makes
  a Console9 Channel→Buss round trip unnecessary.
- Add the guitar-voiced family (`PointyGuitar`, `PunchyGuitar`, `ChimeyGuitar2`)
  at least as audition references, and decide explicitly whether `Cabs`/`Cabs2`
  are out of scope because Cabinet IR and NAM captures own that ground.
- Do not import anything from `libs/airwindows/` in Surge; it is stale. Extend
  `scripts/vendor-airwindows-reverbs.py` to a pinned general-purpose generator
  instead (it already seeds the PRNGs and separates control coefficients from
  the sample clock).
