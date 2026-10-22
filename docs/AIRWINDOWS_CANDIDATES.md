# Airwindows module candidates: outboard, dynamics and replacements

Reviewed 2026-10-21 against the Airwindows catalog ([airwindows.com](https://www.airwindows.com/plugins/),
[github.com/airwindows/airwindows](https://github.com/airwindows/airwindows), MIT license)
at `e68f75154ed0520fc5757fec9e7792dd1747e5ee` (2026-10-20, `master`) — the
revision already pinned by `scripts/vendor-airwindows-reverbs.py`. This note
extends the [FX source review](FX_SOURCE_REVIEW_20261020.md) with a
console/outboard focus and re-triages possible replacements for modules already
shipped. The first six priorities are now implemented; see
[the implementation audit](AIRWINDOWS_OUTBOARD.md) for source corrections,
defaults, mappings and visualizations. Remaining candidates follow the established
pattern in [MODULES.md](MODULES.md): SDK-free double-precision adaptation into
`dsp/airwindows/`, explicit sample-rate audit, deterministic seeds, smoothed
wrapper controls, license beside the header, and an update to the shipped
[third-party notices](../NeuralAmpModeler/installer/ThirdPartyNotices.txt).

**Revision 2.** The first draft of this note contained parameter counts and
behaviour descriptions that did not survive a line-by-line read of the source.
Every control list below is now read from `getParameterName`, every count from
`kNumParameters`, and every rate/latency statement from the processing loop. The
single most important correction is the console-colour row: see
[Console Colour](#console-colour) and the note under
[what changed](#what-changed-in-revision-2).

At the start of this review, the grid ported four Airwindows cores (Tape, kPlate140, kGuitarHall2,
Galactic3). The largest remaining category gap is **outboard**: there is no
console colour, no channel strip, no exciter, no clip guard, and only two
generic compressors. A second gap is **dynamics character**: Smooth and Punch
are both fixed-ratio soft-knee compressors sharing one engine.

In short: three modules close the outboard gap (Console Colour, Console EQ,
Outboard Channel); the compressor set grows from two to five plus two optional;
the gate set from two to three, with Soft Gate as an optional fourth; and the
grid gains an exciter, a clip guard, a comb tone-shaper and two cheap ambience
auditions (kPlate240, VerbThic).

## Source facts

Read from the pinned revision. `params` are the exact upstream names;
`rate` says whether the core divides by `getSampleRate()` (portable as-is),
is rate-blind (needs compensation added), or uses an internal simulated-rate
control.

| Plugin | params | Upstream names | Rate | Latency |
| --- | ---: | --- | --- | --- |
| `CStrip` | 12 | Treble, Mid, Bass, Lowpass, TrebFrq, BassFrq, Hipass, Gate, Compres, CompSpd, TimeLag, OutGain | scales (`overallscale`, `compscale`) | 0 (TimeLag adds up to 700 samples on purpose) |
| `CStrip2` | 10 | Treble, Mid, Bass, TrebFrq, BassFrq, LowCap, HiCap, Compres, CompSpd, Output | scales for comp; **Capacitor is rate-blind** | 0 |
| `BussColors4` | 4 | Color (8-way), Input Trim, Output Trim, Dry/Wet | convolution taps scale, clamped 1.0–4.5× | 0 |
| `Console9Channel` | 2 | Pan, Fader | rate-blind | 0 |
| `Console9Buss` | 2 | Pan, Fader | rate-blind | 0 |
| `DigitalBlack` | 2 | Thresh, Dry/Wet | scales (`release/overallscale`) | 0 |
| `ADT` | 6 | Headrm, A Delay, A Level, B Delay, B Level, Output | **rate-blind** (taps `pow(x,4)*4790`) | taps ≥ 1 sample; page says 1 sample declared |
| `ButterComp2` | 3 | Compress, Output, Dry/Wet | scales | 0 |
| `Pressure6` | 2 | Compres, Ratio | scales | 0 |
| `PurestSquish` | 4 | Squish, BassBlm, Output, Dry/Wet | scales | 0 |
| `Logical4` | 5 | Threshold, Ratio, Speed, MakeupGn, Dry/Wet | scales | 0 |
| `Compresaturator` | 5 | Drive, Clamp, Expand, Output, Dry/Wet | scales | 0 |
| `Density` / `Density3` | 4 | Density, Highpass, Out Level, Dry/Wet | scales | 0 |
| `Air4` | 4 | Air, Gnd, DarkF, Ratio | scales | 0 |
| `Slew4` | 1 | Compres | scales | 0 |
| `OneCornerClip` | 5 | Input, Pos Thr, Neg Thr, Voicing, Dry/Wet | scales (`voicing/overallscale`) | **0** |
| `ADClip9` | 5 | Boost, Match, Noise, Ceiling, Mode | scales | **1–4 samples**, undeclared |
| `ClipOnly3`, `ClipSoftly`, `FinalClip` | 0 | — | scales | **1–4 samples**, undeclared |
| `BrightAmbience3` | 4 | Start, Length, Feedback, Dry/Wet | internal cycle undersampling ≤ 4 | 0 |
| `NonlinearSpace` | 6 | SmpRate (7-way), Livenes, Treble, Bass, Nonlin, Dry/Wet | **simulated-rate control only; never calls `getSampleRate()`** | 0 |
| `SoftGate` | 3 | Thresh, Darken, Silence | scales | 0 |
| `Cojones` | 5 | Breathy, Cojones, Body, Output, Dry/Wet | rate-blind | 0 |
| `VerbThic`, `VerbTiny` | 5 | Replace, Derez, Filter, Wider, Dry/Wet | scales | 0 |
| `VerbSevens`, `VerbSixes` | 0 | — | scales + internal coefficient clock | 0 |
| `kPlate240` | 5 | Input, Regen, DeRez, Predlay, Wetness | generated port already handles this | 0 |

Two consequences that apply to every row:

- **No upstream parameter carries a unit.** Every display (`dB`, `%`, `ms`,
  `Hz`) in the wrapper is a mapping decision, like kPlate140's control mapping.
  Record each one in [FX_PARAMETER_REVIEW.md](FX_PARAMETER_REVIEW.md) and
  [EFFECT_DEFAULTS.md](EFFECT_DEFAULTS.md) rather than leaving it in the port.
- **Several cores are not rate-portable and say nothing about it.** `ADT`,
  `Cojones`, `Console9`, and `CStrip2`'s Capacitor have no rate term at all. A
  port must either add compensation (a documented deviation) or state the tone
  drift. This is the same audit Tape and the reverb family received.

## Outboard candidates

### Console Colour

| Candidate | License | What it would add | Cost / risk | Verdict |
| --- | --- | --- | --- | --- |
| **Console Colour** — **BussColors4** port | MIT | The missing outboard colour, as a single insert: eight desk voices (**Dark**/Focusrite, **Rock**/SSL, **Lush**/Neve, **Vibe**/Elation, **Holo**/Precision 8, **Punch**/API, **Steel**/Calibre, **Tube**/Manley) built from a 34-tap console convolution, a sag buffer and the Console-era saturation, with **Input Trim** and **Output Trim** to hit it at the level it wants. Controls: header **Desk** mode plus Input Trim, Output Trim, Dry/Wet. Bit-transparent at Dry/Wet 0. | Low. Small core, zero latency, discrete voice fits the existing header-mode pattern (`Cosmic Echo` Style, `Multi-head Tape` Heads). Desk changes must fade/clear like other mode changes, and the eight voices need loudness matching at default trim. | **Adopt** — the outboard anchor, and it replaces the invented Console9 "Color" knob of revision 1 |

Why not Console9: the author describes Console as "pre-distorted and then
un-distorted on the buss after summing, for a sound that (in theory) doesn't
touch the tones of any individual mix element". `Console9Channel` is a ×0.618
gain plus a `1−exp(log1p(−x)·1.618)` curve; `Console9Buss` is the reciprocal
0.618 exponent plus ×2. **Run in series inside one slot they cancel.** A
continuous 0–100 % "Color" knob built that way does nothing except clamp at ±1
once the send exceeds unity, which is the opposite of a console voice. The
console only has a sound in three situations: one half used alone, something
non-linear between encode and decode, or encode on the slot and decode after
summing. Only the last is upstream-correct, and it is a new output-stage
feature, not a slot module (see [Console buss stage](#console-buss-stage)).

Single-insert alternatives if the BussColors audition disappoints: **Desk4**
(Overdrive, Hi Choke, Power Sag, Frequency, Output Trim, Dry/Wet — a desk
summing amplifier rather than a console colour) and **PurestWarm3** /
**Mojo** (warmth/saturation). `Console5Channel`/`Console7Channel` (1 control
each) are kernel references, not modules.

### Console EQ

| Candidate | License | What it would add | Cost / risk | Verdict |
| --- | --- | --- | --- | --- |
| **Console EQ** — **CStrip2** EQ section + Capacitor | MIT | A third EQ family voice between Guitar EQ's three fixed voiced bands and Parametric EQ's one surgical bell: **Treble, Mid, Bass** (upstream DSP range ±6 dB on the `A*12−6` mapping, with adjustable **Treble Freq** 1–16 kHz and **Bass Freq** 300 Hz–16 kHz) plus the **Capacitor** cascaded high/low cut pair, a different filter flavour from the RBJ cuts in Cut EQ and Parametric EQ. Controls: Treble, Mid, Bass, Treble Freq, Bass Freq, High cut, Low cut — seven, all native. | Low/medium. The three bands are **not** linear: each is recombined through `sin()` (boost) or `1−cos()` (cut), six transcendentals per stereo sample, so this is a density EQ that adds harmonics rather than a lossless shelf. "Bit-identical when flat" is still true — all three bands at exactly 0 sets `engageEQ = false` — but it costs like a saturator, not like a biquad. Capacitor is rate-blind, its knobs are raw one-pole coefficients rather than frequencies, and it needs both compensating and a Hz mapping invented. | **Adopt** |

Notes:

- Use `CStrip2`, not `CStrip`. `CStrip2` (2023-03) is the refinement of the
  2016 `CStrip`: it drops the gate and the timing trim, replaces the Hipass and
  the special Lowpass with Capacitor, and keeps ButterComp bit-identical.
- Do not carry `CStrip`'s **Lowpass** into this module. Upstream places it
  last in the chain on purpose — "it goes after the gate and compressor just so
  it can further soften any artifacts the dynamics might have produced" — so
  moving it into an EQ module inverts the design intent. Note also that it is
  engaged for every setting below its maximum.
- No Output knob, matching Guitar EQ and Cut EQ. The bands are unity at 0 dB.
- **Do not trust CStrip2's own displays.** Its `getParameterDisplay` still
  describes `CStrip`'s older parameter order — it calls the frequency controls
  "Lowpass/TrebFrq/BassFrq/Hipass", labels slots 8–9 "Gate/Compres", and claims
  ±12 dB for the bands while the DSP computes ±6 dB. Read the processing loop,
  not the editor.

### Outboard Channel

| Candidate | License | What it would add | Cost / risk | Verdict |
| --- | --- | --- | --- | --- |
| **Outboard Channel** — **CStrip2** dynamics path | MIT | The grid's channel strip, taken from the same core as Console EQ so the two audition as one console: Capacitor **Low cut** and **High cut** into **ButterComp** (**Compress**, **Comp Speed**), then CStrip2's **Spiral** console saturation section, then **Output**. Controls: Low cut, High cut, Compress, Comp Speed, Saturate, Output, Mix — seven, all native, no spare position and no mode switch. This is the strip's whole native chain apart from the EQ, which ships as Console EQ. | Low/medium. One coherent upstream path (Capacitor → ButterComp → EQ → output stage); nothing is glued together that upstream keeps apart, and no gate is included — the shipped gates belong at the head of a rig and CStrip2 has none anyway. ButterComp and Capacitor need the usual rate audit, and the Output remap below must be documented. | **Adopt** |

Why the seventh control is **High cut**, and not something else:

- It is the other half of the native Capacitor stage. Both cuts are
  `pow(param, 2)` one-pole coefficients rotated through the same six-sample
  ladder, so exposing the low pass costs **no extra rate-audit work** — the
  compensation that Low cut already needs covers both.
- It has a job in this position: upstream puts Capacitor *before* ButterComp, so
  a high cut keeps pick fizz and cab sizzle out of the detector instead of
  letting them clamp the compressor. A strip that can do that reads as a console
  channel rather than a compressor with a rumble filter.
- It completes the native filter pair, so the module can stand alone late in a
  chain without reaching for Cut EQ as well.

Considered and rejected for the position: **Ratio** and **Attack** (no such term
exists in ButterComp, so either would be an invented algorithm rather than a
mapping); **Density** (upstream's Output knob is one control whose last third
crossfades into Density, so exposing it alongside Saturate splits the same knob
twice and duplicates the optional Density3 voice); **Gate** (CStrip2 has none,
gates belong at the head of a rig, and the grid already ships two gate modules
and plans two more); and **leaving the position empty**, which is legitimate — several
modules use fewer than seven — but wastes a real upstream control that costs
nothing to carry.

The one deliberate mapping: upstream `CStrip2`'s single **Output** knob is a
three-part control — 0–⅓ pad, ⅓–⅔ crossfade from unity into **Spiral** (the
console-channel saturation), ⅔–1 crossfade from Spiral into **Density**. The
module splits it: **Output** takes the pad and level part, **Saturate** takes the
Spiral crossfade, and the Density third is dropped (Density is available
separately, see [Density](#optional-and-dropped)). That is a control remap of
the kind kPlate140's Sustain/Resolution already use, and it is why this is
labelled an adaptation rather than a port.

If the audition wants the EQ on the strip, prefer chaining Console EQ in the
previous slot over widening this module past seven controls.

### Menu placement

**No new catalog group.** A group is not just menu organisation here: the
sibling button is tooltipped "Effects in the same category as this module" and
lists only the current group, and the `<` / `>` buttons step through that group
([ModuleMenu.h](../NeuralAmpModeler/ModuleMenu.h), `ShowSiblings` and `Step` in
[NAMGridControls.h](../NeuralAmpModeler/NAMGridControls.h)). The group is
therefore the one-click A/B set, and it should follow **the decision the player
is making** — "which EQ voice", "which compressor", "which character" — not the
plugin's provenance. Since each of these modules is an alternative for the same
slot, a three-entry "Console / Outboard" group would isolate every one of them
from exactly the modules it has to beat.

| Module | Group | Joins | Why |
| --- | --- | --- | --- |
| Console EQ | EQ / Filter | Guitar EQ, Cut EQ, Parametric EQ, Synth Filter, Wah | it is a voiced EQ; those are its competitors |
| Console Colour | Distortion | Distortion, Boost, Tube Boost, Airwindows Tape, Bitcrusher | the non-linear character set — the same decision as "Tape or Boost?" |
| Outboard Channel | Dynamics | Noise Gate, Smooth, Punch, Butter, Pressure, Squish, Peak Limiter | it is ButterComp plus filters, so the compressor comparison belongs beside it |

Consequences:

- **EQ / Filter** grows to six and **Distortion** to six — both healthy, and no
  module loses its comparison set.
- **Dynamics** reaches twelve with Digital Black Gate, Soft Gate and the three
  new compressors — the longest group (Modulation has nine). If that becomes
  uncomfortable, split along the same principle — **Gate** (Natural/Tight,
  Smart, Digital Black, Soft Gate) and **Dynamics** (compressors plus
  limiters) — rather than minting a group by provenance. That split is a
  presentation change only: `ModuleGroupFor` derives membership, nothing is
  persisted, and `tests/library_tests.cpp` iterates the table.
- **Creative** stays as it is. Freeze, Stutter and LoFi Tape Machine are
  performance tools; console colour is a tone decision, not a performance one.
- Outboard Channel next to Butter Comp is also self-documenting: the strip's
  compressor is a related design, so the sibling audition answers "do I want the
  bare glue or the whole strip?".

### Console buss stage

| Candidate | License | What it would add | Cost / risk | Verdict |
| --- | --- | --- | --- | --- |
| **Console buss stage** — Console9Channel on slots + Console9Buss after Post 4 | MIT | The upstream-correct console: each slot's send is pre-distorted, and a single decode stage after summing un-distorts the mix. This is the only arrangement in which Console does what it is designed to do. | Medium. Not a slot module: it touches presets, project state, the OUT meter path and the fixed eight-position editor, and it only makes sense if the enable state lives per slot. | **Defer**; it is a separate feature, not part of this candidate list |

## Dynamics candidates

### Compressor portfolio

The grid ships two compressors that share one engine: **Smooth** (2.5:1,
9 dB knee) and **Punch** (4:1, 3 dB knee), both Threshold/Release/Makeup/Attack/Mix.
Neither has a ratio control and neither is program-dependent. The Airwindows
compressors are useful precisely because they are *character* devices with few
controls; a fourth lookalike would be wasted. Target: **five adopted compressors
with five distinct jobs**, plus two optional.

| Module | Controls | Job it covers | Distinct because |
| --- | --- | --- | --- |
| Smooth Compressor (shipped) | Threshold, Release, Makeup, Attack, Mix | clean low-ratio workhorse | soft knee, 2.5:1 |
| Punch Compressor (shipped) | Threshold, Release, Makeup, Attack, Mix | firmer, more transient-forward | 4:1, 3 dB knee |
| **Butter Comp** — ButterComp2 | Compress, Output, Mix | the gentlest squeeze; the "always-on" glue | halves handled separately, output-dependent release; related to, but different from, CStrip2’s strip compressor |
| **Pressure** — Pressure6 | Compress, Mix, Output | coloured, stereo-linked sustain | **the only stereo-linked compressor here** (element driven by `max(|L|,|R|)`) with a μ-law element and a sine output stage |
| **Squish** — PurestSquish | Squish, Bass Bloom, Output, Mix | modern, clean levelling that keeps the low end | adaptive per-channel μ compression with internal auto-makeup and a bass-bloom control that holds the lows out of the squish |
| *Console Comp* — Logical4 (optional) | Threshold, Ratio, Speed, Makeup, Mix | a normal compressor with explicit ratio | the only candidate with all three of threshold-in-dB, ratio and speed; **three parallel ButterComp instances** interpolated by Ratio, so ~3× the CPU |
| *Compresaturator* (optional) | Drive, Clamp, Expand, Output, Mix | driven, experimental tank compression | saturation fed into the detector; no threshold in the usual sense |

**Yes to Pressure6.** It is the cheapest character compressor in the catalog —
two native controls — and it is the only one that is *stereo-linked*, which
matters for a grid that feeds coupled stereo effects but mostly compresses
dual-mono today. Two caveats to record up front:

- Upstream's **"Ratio" is not a ratio.** `Ratio` (`B`) is used as
  `wet = B*0.9`; there is no ratio term anywhere in the core. The wrapper should
  label it **Mix** and treat it as one, and the fixed 0.9 ceiling means the
  upstream effect is never 100 % wet — either map the wrapper Mix differently or
  document the 90 % limit.
- There is **no output gain** upstream and the final stage is
  `sin(clamp(x·wet))`, so the module needs a wrapper Output and a loudness
  match at default. That is a normal wrapper addition, as on other ports.

### Gates and other dynamics

The grid ships two gate modules: Noise Gate (Natural/Tight) and Noise Gate
(Smart). Adding two more is fine as long as each has a different mechanism.

| Module | Controls | Job | Distinct because |
| --- | --- | --- | --- |
| Noise Gate (Natural/Tight) (shipped) | Threshold, Release, Reduction, Mode | general amplitude gating | envelope gate |
| Noise Gate (Smart) (shipped) | learn, Sensitivity, Reduction, Attack, Release, Mix | noise-floor-specific | learned per-band reduction |
| **Digital Black Gate** — DigitalBlack | Threshold, Mix, Output; header Mode Plain / Hysteresis | click-free gating of quiet passages | opens and closes only on zero crossings, and closes by **blending a rectified `1−cos` fill** instead of dropping to silence, so the tail darkens rather than chops |
| *Soft Gate* — SoftGate (optional) | Threshold, Darken, Silence, Mix, Output | de-esser-style gating | closure is **spectral**: highs roll off as the gate shuts |

Notes:

- Take `DigitalBlack` as the core, not `CStrip`'s gate. They are different
  implementations: `DigitalBlack` uses one threshold, a 6000-sample zero-cross
  clamp and a rate-compensated `release = 0.0064/overallscale`, while `CStrip`'s
  version adds on/off hysteresis, a 220.9-sample clamp and a **fixed**
  `release = 0.028331119964586` that is not rate-scaled (roughly a 53 ms close
  at 44.1 kHz and 24 ms at 96 kHz). The hysteresis variant is worth keeping as a
  header **Mode** — it is the only behaviour `DigitalBlack` lacks — but the rate
  audit must fix the unscaled release.
- `Threshold` upstream is `pow(A,4)/3 + 0.00018`, a linear amplitude with no
  unit. Map it to dB in the wrapper and record the curve.
- Neither gate has hold or reduction depth, so neither replaces the existing
  gates; keep Reduction out of their editors rather than faking it.
- The proposed `gate` gain-reduction tap was corrected during implementation:
  both cores also blend a rectified fill, so opening is not applied gain.
  The panel measures actual attenuation before Output; see the
  [dynamics audit](AIRWINDOWS_DYNAMICS.md), including additional gate-counter
  and Pressure6 recovery rate corrections.

**Digital Black Gate is committed, and Soft Gate stays optional.** The obvious
objection is that a gate is used once per rig, so a fourth voice is wasted menu
space. It does not apply here, because this one is not a cleaner version of an
existing gate — it is a different mechanism with its own sound:

- The shipped gates act on an envelope and close the amplitude. This one opens
  and closes only on zero crossings and closes by blending a rectified `1−cos`
  fill, so a decaying note **darkens and softens into a residue** instead of
  being chopped. That is usable as an effect after a fuzz or for a sputter,
  which is a musical job no shipped module has.
- The header **Mode** adds CStrip's hysteresis gate at no extra control cost, so
  the module covers both Airwindows gate behaviours in one editor.
- It is cheap: two native parameters plus the wrapper Mix and Output, one exact
  gain-reduction tap, no delay lines.
- Position is not a problem. Gates go at the head of a rig, so the module does
  not compete for a slot with anything later in the chain, and it needs no
  parameter that a compressor family would have used.

Soft Gate remains the optional fourth voice: its spectral darkening is closer to
what Noise Gate (Smart) already does than to what this adds.

### Optional and dropped

| Candidate | License | What it would add | Cost / risk | Verdict |
| --- | --- | --- | --- | --- |
| **Density3** — moved to Distortion | MIT | Not a compressor. The author describes Density as "`sin()` based overdrive combined with a very gentle highpass", i.e. a level-dependent dirt-shaper with a bipolar character (positive fattens, negative thins and adds a gate-like drying in Density3). Controls: Density, Highpass, Output, Dry/Wet. | Low. Four controls, small DSP. Overlaps Distortion, Tube Boost and the planned Boost Character rather than the compressor family. | **Optional as a Distortion voice**; drop from the compressor set |
| **Logical4** | MIT | A compressor with a real ratio control, plus a power-sag stage. | Medium: three parallel ButterComp instances, each with per-sample `pow()`, so roughly 3× ButterComp2 CPU for a control set that is the closest to the two shipped compressors. | **Optional** — audition it against Smooth/Punch before adopting |
| **Compresaturator** | MIT | Driven tank compression: Drive, Clamp, Expand, Output, Dry/Wet. | Low/medium. Five controls; the most experimental voicing in the set; overlaps Pressure. | **Optional** |
| **Point** | MIT | Not a compressor: a **transient shaper** (Input Trim, Point, Reaction Speed) that leans on pick attack without changing level. | Low. Three native controls. The grid has no transient shaper at all — a real gap for guitar that no shipped module or candidate covers. | **Audition** as a new Dynamics candidate, separately from the compressor list |
| **Pressure4 / Pressure5** | MIT | Superseded. Pressure5 has six controls (Pressre, Speed, Mewines, PawClaw, Output, Dry/Wet) and the same family character as Pressure6 with more to map. | — | **Drop**; Pressure6 is the refined, minimal member of the family |
| **VariMu** | MIT | Intensity, Speed, Output, Dry/Wet. | — | **Drop** — same soft, level-dependent job as Pressure6, one more editor for no new behaviour |
| **StoneFireComp, Pop3, DrumSlam, BlockParty, NCSeventeen** | MIT | Drum/buss clippers and loudness tools (10, 8, 3, 2, 2 controls). | — | **Drop** — control count, drum focus, or overlap with Peak Limiter |
| **PurestWarm3, Mojo, SingleEndedTriode, HardVacuum, PowerSag2, Inflamer** | MIT | Tube/console warmth and sag colours. | — | **Optional** — only after Console Colour and the compressor set are auditioned |

## Tone and character candidates

These fill the preamp/character end rather than the dynamics end, and both are
cheap: six and four native controls.

| Candidate | License | What it would add | Cost / risk | Verdict |
| --- | --- | --- | --- | --- |
| **ADT** | MIT | Two calibrated taps at up to 4790 samples (108 ms at 44.1 kHz) with independent levels that go negative for cancellation, plus headroom trim and output. A comb/tone shaper that reinforces and cancels at chosen times, not a modulated doubler. Controls: Headroom, Delay A, Level A, Delay B, Level B, Output — six, all native. | Low/medium. **Rate-blind**: tap lengths are `pow(x,4)*4790` samples with no rate term, so the times halve at 96 kHz unless compensated. The core also applies a `sin(x·|x|)/|x|` **Spiral pre-shape** — the comment says it expects a ConsoleBuss afterwards — so using it standalone adds a fixed console-side colour. | **Adopt**, with the pre-shape decision documented |
| **Air4** | MIT | The exciter: **Air** (high-shelf harmonic generation), **Gnd** (ground/body), **DarkF** and **Ratio** (high-frequency limiting that pulls the air back as energy rises). Four native controls plus Output. | Low. Rate-scaled in the source. The high-frequency limiting is what keeps an exciter from fizzing on high-gain guitar, which is why it wins over the two-control `Exciter` and over `Slew4`. | **Adopt** — the exciter category pick |

## Clip guard and Boost

| Candidate | License | What it would add | Cost / risk | Verdict |
| --- | --- | --- | --- | --- |
| **Limiter & Clipper** — existing Peak Limiter + **OneCornerClip** stage | MIT (clip stage) | The grid's Peak Limiter (Ceiling/Release, instant attack, no lookahead) with a clipper after it. `OneCornerClip` is the right core: it has real thresholds (**Pos Thr**, **Neg Thr**, asymmetric on purpose, so it generates even harmonics), a **Voicing** softness control and Input/Dry-Wet — unlike `ClipOnly3`/`ClipSoftly`/`FinalClip`, which are zero-parameter, and unlike `ADClip9`. Controls: Ceiling, Release, Clip blend, Voicing, Mix, Output. Clip at 0 % must be bit-identical to the shipped Peak Limiter, following the Boost Character pattern. | Low/medium. `OneCornerClip` has **no internal delay**, so the module stays zero-latency — this is the decisive reason to prefer it over ADClip9 and the zero-parameter cores, which all delay by `spacing` (1 sample at 44.1 kHz, 2 at 96 kHz, up to 4 at 192 kHz) *without declaring it*, which would comb against the grid's dry path at Mix < 100 %. `Voicing` is divided by `overallscale` upstream, so its feel changes with rate. | **Adopt** |

On `ADClip9`, kept for the record in case an audition prefers it: its five
parameters are Boost, Match, Noise, Ceiling, Mode where **Mode only selects
gain staging** (Boost / Match / ClipOnly). In the clip block the *threshold* is
`noiseLevel` — the **Noise** control — while **Ceiling** is only the level the
clamp holds at. That looks like a slip upstream, so any Ceiling-to-threshold
mapping would be an invention rather than a collapse.

### Boost Character

| Candidate | License | What it would add | Cost / risk | Verdict |
| --- | --- | --- | --- | --- |
| **Boost Character** — Cojones merged into the existing Boost as a Character dial | MIT | Cojones is a **differentiator-average band restructurer**: it tracks running sample differences, picks the strongest band from six, and rebuilds the signal from a smoothed body average plus the instantaneous detail, so it recovers transient detail and adds "attitude" without saturating. Merged design: **Gain, Tilt, High pass, Character, Output**, Character 0 % bit-identical to today's Boost. | Low/medium. The merge is not a parameter collapse; it is a parallel blend. The caveat is that Cojones is **rate-blind** and operates in the sample-difference domain, so it will not behave like a tilt shelf next to Tilt's 800 Hz pair — expect the two to stack unpredictably at high Character and cap the usable range instead of assuming they fight. | **Adopt** the merge; keep standalone Cojones as the fallback if the blend is unclear |

## Ambience candidates

| Candidate | License | What it would add | Cost / risk | Verdict |
| --- | --- | --- | --- | --- |
| **BrightAmbience3** | MIT | Compact bright ambience with inter-aural stereo delays. Start, Length, Feedback, Dry/Wet. A texture between delay and reverb that no shipped module provides. | Low/medium. Small DSP; internally undersampled with a cycle counter clamped to 4, so it keeps its 44.1 kHz character at higher rates — the same posture as the kPlate family. | **Adopt** if the verb set wants a fifth texture; audition against Cosmic Echo's sparse styles |
| **VerbThic** / **VerbTiny** | MIT | The Verbity lineage with real controls: Replace, Derez, Filter, Wider, Dry/Wet. Modulation and resolution behaviour that the shipped verbs do not have. | Low/medium. Five native controls, rate-scaled, and it fits the editor — unlike the zero-parameter matrix plugins below. | **Adopt as the next reverb audition** |
| **NonlinearSpace** | MIT | Level-dependent room: feedback rises with loudness so the space "leans in" when pushed. SmpRate (7-way), Liveness, Treble, Bass, Nonlin, Dry/Wet. | Medium. **The whole core runs from the SmpRate parameter and never calls `getSampleRate()`**, so it is host-rate-blind, not merely awkward to switch: delay lengths and coefficients are recomputed each block, which means SmpRate needs the grid's bypass-and-restore fade. Adding host-rate scaling is a deviation. | **Audition** with header Size, only if BrightAmbience3 and VerbThic leave a gap |
| **VerbSevens / VerbSixes** | MIT | Zero-parameter matrix reverbs. VerbSevens is a compile-time matrix test-bed (3×3 … 6×6, plus a mono variant) that doubles as a large undamped wash with no allpasses. | Medium/high for a module: no controls to wrap, dozens of rate-scaled delay lines per channel, no damping or decay control, and a fixed voicing that overlaps Cloud and Galactic Space. VerbSixes (2025-06) is the earlier sibling. | **Reference only** — use as a calibrated decay-measurement reference (see [COSMIC_ECHO.md](COSMIC_ECHO.md)); do not ship |
| **kPlate240** | MIT | The other half of the 2024-06-23 kPlate release: same generation as the shipped kPlate140, 240-second tank instead of 140. Input, Regen, DeRez, Predlay, Wetness — the same mapping the shipped Air Plate already uses. | Very low. `scripts/vendor-airwindows-reverbs.py` already generates it; this is one more entry in the port table. | **Audition** — cheapest new texture available, and the plate family is not closed until it is heard |

## Replacements: are they better than what ships?

| Candidate | Existing module | Would it be better? | Verdict |
| --- | --- | --- | --- |
| **IronOxide5** | Airwindows Tape, Tape Echo, LoFi Tape Machine | No. The shipped Tape is a deliberate two-control, no-flutter design; wow/flutter lives in Tape Echo and transport/dropouts in LoFi Tape Machine. IronOxide5 has seven controls (Input Trim, Tape High, Tape Low, Flutter, Noise, Output Trim, Inv/Dry/Wet) that overlap all three at once without a new behaviour. | **Skip** on coverage |
| **DeRez4 / DeRez5** | Bitcrusher | No. Bitcrusher covers bit reduction, sample-hold rate reduction and a reconstruction low-pass with tone/mix/output; DeRez5's six controls (Derez, Trim, Square, Bright, Bassis, Output) reach the same places. | **Skip** on coverage |
| **kPlateB / kPlateA / kPlateC / kPlateD** | Airwindows Plate (kPlate140) | No, and not a generation question: A–D (2023) are four sibling voicings of the short plate, kPlate140/240 (2024) are the 140 s and 240 s versions. The ported kPlate140 is the current design; kPlate240 (above) is the only one worth adding. | **Skip**; audition kPlate240 instead |
| **MatrixVerb / MV / older Airwindows reverbs** | Room, Dense Plate, Cloud, Hall, Cosmic Echo | No. The one true gap — gated reverb — is not addressed by any of them; it is better served natively by reusing the shipped Room/Hall cores with a gate-shaped envelope. | **Skip** |
| **Cabs / Cabs2** | Cabinet IR | No. `Cabs2` is a two-control static cab filter; Cabinet IR and NAM captures own that ground. | **Skip**, reviewed for completeness |
| **PointyGuitar / PunchyGuitar / ChimeyGuitar2** (+ `*Deluxe`) | (none) | Not adoptable as modules — ten controls each, above the editor limit — but these are Airwindows' own guitar-voiced processors, including `HSpeaker`/`LSpeaker` resonance controls. Worth an audition as a voicing reference for Guitar EQ and Cut EQ defaults. | **Skip as modules, keep as references** |
| **kPlate140 / kGuitarHall2 / Galactic3** | (already shipped) | — | Already ported |

**Overall:** no replacement beats a shipped implementation. Every worthwhile
Airwindows addition here is a *new category* — console colour, channel strip,
exciter, clip guard, character compression, transient shaping — not an upgrade
to an existing slot.

## Skipped in this pass

`ToTape9` (nine controls, already rejected in [MODULES.md](MODULES.md)),
`ConsoleX`/`ConsoleX2` (control count, GUI assumptions), the console families
`Console0`–`Console8` (Channel + Buss pairs; see the buss stage above), the
amp-sim plugins (`FireAmp`, `LeadAmp`, `BigAmp`, `GrindAmp`, `BassDrive`, `LilAmp`,
`MidAmp`) and `GuitarConditioner` (capture modelling owns that ground), the
`PurestConsole*` series (encode/decode pairs again), `Pafnuty`, `StoneFire`
(ten controls), and everything overlapping a shipped family (`IronOxide5`,
`DeRez4/5`, `Dattorro`, `Chamber`, `Galactic`).

## Porting notes

- Precedent: Tape was hand-adapted into `dsp/airwindows/AirwindowsTape.h` with
  license beside it; the reverb family is reproduced by the pinned vendor script
  `scripts/vendor-airwindows-reverbs.py`. The candidates here follow the Tape
  pattern (focused hand adaptation, explicit rate scaling) with one addition:
  **generalise the vendor script** to take a plugin list and emit the same
  shape it already emits for the reverbs — SDK-free, double-precision, PRNGs
  seeded, control coefficients separated from the sample clock. That removes the
  hand-adaptation drift that produced revision 1's errors.
- Seed handling is not uniform. The reverb generator rewrites `fpdL`/`fpdR`;
  ButterComp2 (and its relatives) instead carry `static int noisesourceL/R`
  inside the processing loop, so the generator needs a second seeding rule or
  those become shared state across slots.
- Sample-rate discipline, per the table above: `ADT`, `Cojones`, `Console9` and
  `CStrip2`'s Capacitor get compensation added or an explicit warning;
  `NonlinearSpace` gets host-rate scaling added on top of its SmpRate control;
  everything else is audited at 44.1/48/88.2/96/192 kHz like Tape.
- Latency: `OneCornerClip` is zero-latency and stays so. `ADClip9`,
  `ClipOnly3`, `ClipSoftly` and `FinalClip` delay by 1–4 samples without
  declaring it, which combs against the grid's dry path at Mix < 100 %; do not
  use them as the clip core unless the dry path is delayed to match or the
  latency is reported.
- Mono/stereo posture: the established Airwindows pattern runs dual-mono and
  keeps the left result. That is wrong for **Pressure6** (stereo-linked by
  design) and pointless for `Console9` (pan). Decide per module and state it:
  Pressure6 should join the coupled-stereo effects, like Doubler.
- CPU: "tiny DSP" is optimistic here. ButterComp does four `pow()` per sample,
  Logical4 runs three of them, and CStrip2's EQ does six `sin`/`cos` per stereo
  sample when engaged. Budget each candidate in the [MODULES.md](MODULES.md)
  benchmark table (Airwindows Tape is 0.49 % at 48 kHz) *before* adoption.
- No upstream control has a unit, so every label, unit and curve in the wrapper
  is a mapping decision and belongs in
  [FX_PARAMETER_REVIEW.md](FX_PARAMETER_REVIEW.md) and
  [EFFECT_DEFAULTS.md](EFFECT_DEFAULTS.md).
- Audition protocol: mirror [REVERB_DELAY_AUDITION.md](REVERB_DELAY_AUDITION.md)
  — side-by-side slots, matched loudness, benchmark figures at 128-frame blocks,
  defaults captured in [EFFECT_DEFAULTS.md](EFFECT_DEFAULTS.md).

## Suggested priority

1. **Outboard Channel** — CStrip2's dynamics path, the Outboard category anchor.
2. **Console Colour** — BussColors4; the cheapest real gap in the whole list, and
   the module that makes the outboard family sound like a console rather than a
   pile of separate utilities.
3. **Console EQ** — CStrip2's three bands plus Capacitor, the third EQ voice.
4. **Butter Comp** — three controls, with output-dependent recovery; compare its
   related but different compressor design with the strip in step 1.
5. **Digital Black Gate** — the third gate mechanism, including the hysteresis
   mode.
6. **Pressure** — Pressure6; cheapest character compressor, and the only
   stereo-linked one.
7. **ADT** — comb/tone shaping with polarity flips; six native controls.
8. **Air4** — the exciter with high-frequency limiting.
9. **Limiter & Clipper** — OneCornerClip behind the shipped Peak Limiter; useful
   in nearly every rig and zero-latency by construction.
10. **Squish**, then **Soft Gate**, **kPlate240**, **VerbThic**,
    **BrightAmbience3**, and **NonlinearSpace** as appetite allows.
11. Optional auditions if the set still feels incomplete: **Logical4** (explicit
    ratio), **Compresaturator** (driven), **Point** (transient shaping),
    **Density3** (dirt shaping), **PurestWarm3/Mojo** (warmth).

The Console buss stage stays deferred until the channel half ships.

## What changed in revision 2

Kept for the record, because these were the errors a source read caught:

- **Console9 Channel→Buss is not a colour.** It cancels by design. Replaced by
  `BussColors4` as the console-colour module; the console buss is documented as
  an output-stage feature rather than a slot module.
- **CStrip has neither a Color nor a Fader control**, and has 12 parameters.
  The strip is now built on `CStrip2` (10 parameters), which already drops the
  gate and the timing trim and already contains the Spiral console saturation.
- **CStrip's EQ is a non-linear density EQ** (±6 dB per band, `sin`/`1−cos` per
  band), not a lossless subtractive EQ, and its Lowpass belongs at the end of
  the chain, not inside an EQ module.
- **`ADT` has 6 controls**, not 2–4, and is rate-blind with a console pre-shape.
- **`DigitalBlack` has 2 controls** (Thresh, Dry/Wet); the "~6.4 ms release" was
  a per-sample coefficient, not a time constant. CStrip's gate is a different
  implementation whose release is not rate-compensated.
- **Density is an overdrive**, per the author, not a compressor.
- **ClipOnly3/ADClip9/ClipSoftly/FinalClip delay by 1–4 samples** without
  declaring it, so `OneCornerClip` (zero-latency, real thresholds) is the clip
  core instead.
- **`NonlinearSpace` never reads the host rate at all**; it is not just "not
  live-switchable".
- **VerbSevens is one member of a family** (VerbSixes, VerbThic, VerbTiny,
  PocketVerbs), and `kPlate240` shipped alongside the ported kPlate140.
- **The grid does have a thickener**: Stereo Doubler (Pitch/Frequency), which is
  why ADT is described as comb/tone shaping instead.
- **The grid does have two compressors but no ratio anywhere**, which is what
  the portfolio table above is organised around.
- **No new menu category.** Because a category is the sibling/step A/B set, the
  colour goes to Distortion, the EQ to EQ / Filter and the strip to Dynamics.
