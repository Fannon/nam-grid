# Airwindows module candidates: outboard and replacements

Reviewed 2026-10-21 against the Airwindows catalog ([airwindows.com](https://www.airwindows.com/plugins/),
[github.com/airwindows/airwindows](https://github.com/airwindows/airwindows), MIT license).
This note extends the [FX source review](FX_SOURCE_REVIEW_20261020.md) with a
console/outboard focus and re-triages possible replacements for modules already
shipped. No source has been copied yet. All candidates follow the established
pattern in [MODULES.md](MODULES.md): SDK-free double-precision adaptation into
`dsp/airwindows/`, explicit sample-rate audit, deterministic seeds, smoothed
wrapper controls, license beside the header, and an update to the shipped
[third-party notices](../NeuralAmpModeler/installer/ThirdPartyNotices.txt).

The grid currently ports four Airwindows cores (Tape, kPlate140, kGuitarHall2,
Galactic3). The largest remaining category gap is **outboard**: there is no
console channel strip, no console color, no thickener, no exciter, no clip
guard, and only two generic compressors.

## New module candidates

| Candidate | License | What it would add | Cost / risk | Verdict |
| --- | --- | --- | --- | --- |
| **Console EQ** (colorized EQ) — **CStrip** EQ section + **Console9Channel** color | MIT | A third EQ family voice between Guitar EQ (fixed voiced bands) and Parametric EQ (surgical): CStrip's lossless subtractive three-band with adjustable shelf frequencies (Bass, Mid, Treble, BassFrq, TrebFrq) plus its special guitar-shaping Lowpass, and an optional **Color** knob engaging the Console9Channel encode/decode kernels. Bit-identical passthrough when the EQ is flat and Color is at zero. | Low/medium. The EQ sections are self-contained; combining them with console color is a new pairing (upstream ships them separately), so it is labeled an adaptation. The usual rate audit applies. | **Adopt** as a standalone Console EQ module |
| **Outboard Channel** (single mode, no selector) — **CStrip** dynamics + **Console9Channel** color | MIT | With the EQ shipped as Console EQ and the gate as its own module below, what remains of CStrip's console signal flow fits one seven-knob editor without any mode switch: **Highpass, Compress, Comp Speed, Color, Fader, Output** (TimeLag is dropped; Utility already owns fine delay). Highpass feeds the strip's ButterComp; **Color** (0–100%) blends the Console9Channel encode/decode kernels, so zero color is a pure dynamics strip and higher settings add the mixing-desk voice on a continuous, automatable knob. The old Console-only mode remains reachable because CStrip's sections "switch themselves out of the circuit": Compress at minimum bypasses ButterComp, leaving color and fader alone. Console5/7Channel are one-parameter kernel alternatives for the audition. | Low/medium. One coherent signal path from two upstream codebases; ButterComp and the console kernels need the usual rate audit, and Color's gain staging must be loudness-matched at its default. | **Adopt** as one Outboard Channel module, no modes |
| **Digital Black Gate** (third gate module) — CStrip's gate section / Digital Black | MIT | A distinct third gate voice beside Noise Gate (Natural/Tight) and Noise Gate (Smart): zero-crossing-locked opening for click-free action, and closure that blends a rectified soft-fill (`1−cos`) so quiet passages darken rather than chop. Controls: Threshold, Wet, Output. | Low. Two-parameter core. It is **not** a replacement for the existing Noise Gate: fixed ~6.4 ms release and no hold or reduction depth control. The per-sample `gate` variable gives an exact gain-reduction tap for the panel meter with no behavior change; Noise Gate (Smart)'s engine stays untouched. | **Adopt** as a standalone Dynamics module |
| **Console buss color** (Console9Buss, later) | MIT | Not a slot module: upstream consoles are Channel+Buss systems, and the channel half above is only half the sound. A small global console stage after Post4 (Fader-style gain plus Buss encode/decode) would complete the illusion for full-rig presets. ConsoleX itself (~36 controls, JUCE assumptions) stays excluded, like ToTape9. | Low per se, but it is a new output-stage feature rather than a slot module, touching presets, state and the OUT meter path. | **Defer**; decide when the channel module ships |
| **ADT** | MIT | Two calibrated micro delay taps, one inverted, with saturation. Fattens a guitar without low-end buildup or added sustain. Zero latency, tiny DSP, 2–4 controls. Best power-to-weight ratio of any candidate. | Low. Explicit rate audit required like every port. | **Adopt** |
| **Exciter** | MIT | Harmonic exciter via saturation. Fills a real category gap: the grid has no exciter today, and Cut EQ/Pencece shelves cannot generate harmonics. Superseded by the **Air4** row below, which the full-catalog sweep identified as the better exciter pick. | Low/medium. Audit which upstream exciter variant is current and how its bands split. | **Adopt** — as Air4 |
| **Butter Comp** — **ButterComp2** port | MIT | The very simple, guitar-optimized compressor: Compress, Output, Dry/Wet. Very soft bipolar squeeze (each wave half handled separately); the gentlest one-knob glue. The same engine already planned inside the Outboard Channel — the standalone module is the bare effect for slots that want only compression. | Low. Three controls, tiny DSP, usual rate audit. Gain reduction is natively available: the core applies the compressor as a per-sample multiplier (`totalmultiplier`), so the adaptation records it in a metering tap — exact GR for the panel, no behavior change, following the existing gate/compressor reduction convention. | **Adopt** as a standalone simple compressor |
| **Density** — **Density** port | MIT | The second very simple, guitar-optimized compressor: Density, Highpass, Out Level, Dry/Wet. Bipolar thickness — positive settings give fat, tubey sustain push; negative settings thin the sound and push it back (useful on reverb returns). | Low. Four controls, small DSP, usual rate audit. No scalar gain exists (the core is an iterated sine waveshaper), so there is no exact GR: the panel shows either a smoothed wet/dry envelope estimate (labeled as an estimate) or a static transfer-curve display like the Distortion module's shaper curve. Newer generations (Density3) exist; audition to pick before freezing semantics. | **Adopt** as a standalone simple compressor |
| **Compresaturator** — optional fifth compressor | MIT | Driven character compression: Drive, Clamp, Expand, Output, Dry/Wet. Saturation-fed tank compression with no threshold; the most experimental voicing here. | Low/medium. Five controls; timing re-verified at 44.1/48/96 kHz; same GR-meter caveat. | **Optional** — adopt only if the audition wants a driven voice beyond Butter Comp and Density |
| **Pressure4** — dropped | MIT | The author's "golden ear favorite" (Pressure, Speed, Mewiness, Output) with diade-bridge stereo linking. Its value was as a mode option in the previously proposed combined module; as a standalone it is neither simple nor guitar-focused, and the compressor portfolio target is four to five modules. | — | **Drop** from the candidate list |
| **Limiter & Clipper** (one combined module) — existing Peak Limiter core + **ClipOnly3**/**ADClip9** clip stage | MIT (clip stage) | The grid already ships an original Peak Limiter (Ceiling/Release, instant attack, no lookahead). A combined module would add a clipper stage after the limiter with a header mode: **Limiter**, **Clip**, or **Limiter + Clip** cascade. The clip stage adapts Airwindows clipping: ClipOnly3 is the transparent zero-parameter anti-clip that only engages near ceiling, while ADClip9 (5 parameters) is the loudness-maximizing variant. Controls: Ceiling, Release, Drive (clip saturation), Mix, Output. | Low/medium. Peak Limiter core exists; the clip stage is small. ADClip9's native parameters would be collapsed into the Drive/Ceiling mapping — an interpretation, like the kPlate140 control mapping. FinalClip (fixed +6 dB seed of ADClip8) and OneCornerClip (5 parameters, softer corner) are alternative clip cores if the audition prefers them. | **Adopt** as one Limiter & Clipper module |
| **Boost Character** — Cojones merged into the existing Boost as a Character dial | MIT | Cojones is not the one-knob clean boost first assumed: the source shows five parameters (Breathy, Cojones, Body, Output, Wet) reconstructing the signal from a smoothed body average, instantaneous detail and a top band, with signal-dependent band selection — a detail-recovering "attitude" voicing that adds no saturation but is not strictly linear. Merged design: **Gain, Tilt, High pass, Character, Output**, where Character 0% is bit-identical to today's Boost (state-compatible default) and higher settings blend the Cojones path in parallel — no native control is remapped. | Low/medium. Parallel blend, not parameter collapse. Caveats: Cojones' body/breathy bands overlap Tilt's 800 Hz shelves, so the audition decides whether they stack or fight (and whether to cap the usable Character range); MODULES.md's "adds no saturation, use Tape when nonlinear character is wanted" needs rewording once Character engages. Fallback if they fight: standalone Cojones module (five knobs fit). | **Adopt** merged into Boost as a Character dial |
| **Air4** | MIT | High-frequency exciter with a safety net — the exciter category pick, replacing the vaguer "Exciter" row: Air (high-shelf air generation), Gnd, DarkF and Ratio (high-frequency limiting that kicks the air down when energy gets high, preventing the fizz problem exciters cause on high-gain guitar). Explicit host-rate scaling in the source. | Low. Four controls, fits the editor; usual rate audit (it divides by 44.1 kHz and rescales). For a taming-only alternative, Slew/PlatinumSlew (one-parameter slew limiters) tame fizz after high-gain NAM stages without adding harmonics; they partially overlap Cut EQ's high cut. | **Adopt** as the exciter; Slew stays optional |
| **BrightAmbience3** | MIT | Compact ambience generator — extruded bright reverb with inter-aural stereo delays, 4 parameters. A distinct texture between delay and reverb that no current module provides; explicitly undersampled for high rates, so porting posture matches the kPlate family. | Low/medium. Small DSP; overlaps nothing shipped; audition against Cosmic Echo's Sparse style. | **Audition** alongside the verb set |
| **NonlinearSpace** | MIT | Level-dependent room: feedback increases with loudness (liveness), giving a space that "leans in" when pushed — M/S processing inside. Six parameters including a compile-time-style Sample Rate size control that must not be switched live (bypass-and-restore semantics needed, like existing mode changes). | Medium. The dynamic-feedback behavior is genuinely new to the reverb family; the size control's not-live-switchable nature requires the grid's mode-change fade/clear handling. Rate behavior is conversion-based, needing the usual audit. | **Audition** if BrightAmbience3 leaves ambient depth gaps |
| **SoftGate** | MIT | A de-esser/noise-reduction gate: closes only partially, rolling off highs as it shuts (Bright Threshold, Darken Speed, Base Silence). Unlike the existing gates' depth control, its closure is spectral — quiet passages get darker, not just quieter. | Low. Three controls, tiny DSP. Distinct from Digital Black Gate's soft-fill closure (amplitude darkening) and from Noise Gate (Smart)'s learned spectral reduction. | **Optional** — a fourth gate voice only if the audition shows demand |

Skipped in this pass: ToTape9 (nine controls; already rejected in
[MODULES.md](MODULES.md)), ConsoleX (control count and GUI assumptions), and
anything overlapping existing families (IronOxide, DeRez2 — see below).
Standalone compressor or clipper modules are also skipped: the two-mode
compressor and the combined limiter/clipper above cover their categories with
fewer editors to maintain, and the existing Smooth/Punch compressors stay.

## Replacement candidates: are they better than what ships?

These were checked as possible *replacements* for existing implementations.
The question is quality per category, not novelty.

| Candidate | Existing module | Would it be better? | Verdict |
| --- | --- | --- | --- |
| **IronOxide5** | Airwindows Tape, Tape Echo, LoFi Tape Machine | No. The shipped Tape is a deliberate two-control, no-flutter design; wow/flutter already lives in Tape Echo and transport/dropouts in LoFi Tape Machine. An earlier row called IronOxide GPL-licensed — a **correction from the full-catalog sweep**: the airwindows repository license is MIT and IronOxide5 is ported there like the rest, so adaptation would be permitted. It stays skipped on coverage: its flutter/skew/detorque controls overlap all three existing tape modules at once without a new behavior. | **Skip** on coverage, not license |
| **DeRez4 / DeRez5** | Bitcrusher | No. Bitcrusher already covers bit reduction, sample-hold rate reduction and a reconstruction low-pass with tone/mix/output. An earlier row called DeRez2 GPL-licensed — a **correction from the full-catalog sweep**: it is MIT like the rest, so adaptation would be permitted. It stays skipped on coverage: the smoothed derez behavior is reachable by lowering sample-hold rate and closing Tone. The newer DeRez4/5 generations would need the same verdict. | **Skip** on coverage, not license |
| **kPlateB** | Airwindows Plate (kPlate140 port) | No. kPlateB is an earlier generation of the same plate design; the shipped kPlate140 port already supersedes it with revised nonlinear input and undersampling behavior. A second plate of the same lineage would double the audition surface without a new texture. | **Skip** |
| **MatrixVerb / MV / older Airwindows reverbs** | Room, Dense Plate, Cloud, Hall, Cosmic Echo | No. The grid already holds four reverb families plus Cosmic Echo, and no older Airwindows reverb offers a missing behavior. The one true gap — **gated reverb** — is not addressed by any of them; it is better served natively by reusing the existing Room/Hall cores with a gate-shaped envelope, avoiding another 44.1 kHz-era port. | **Skip**; consider a native gated-reverb preset/module instead |
| **kPlate140 / kGuitarHall2 / Galactic3** | (already shipped) | — | Already ported |

**Overall:** no replacement beats the existing implementations. Every worthwhile
Airwindows addition here is a *new category* (outboard/console, thickener,
exciter, clip guard, character compressor), not an upgrade to an existing slot.
The one reverb-shaped exception is VerbSevens, below.

## VerbSevens: a new reverb candidate (October 2026)

[VerbSevens](https://www.airwindows.com/verbsevens/) (released 2026-10-18) is
reviewed separately because it is neither a clean category gap nor a plain
replacement. Source inspected at `airwindows/airwindows` `master`,
`plugins/LinuxVST/src/VerbSevens/` (MIT).

What the source shows:

- **Zero parameters** (`kNumParameters = 0`). It is a calibrated reference
  test-bed for reverb matrices that does double duty as a large ambient pad.
  Matrix size is chosen at compile time (`THREEBYTHREE` … `SIXBYSIX`); the
  shipped configuration feeds a Householder 4×4 into a Conference 6×6 — close
  to twelve million echoes before feedback, unity-gain paths, phase-flipped
  rather than attenuated.
- **A topology the grid does not have**: pure comb delay lines and matrices
  with **no allpass filters**. Room/Hall (Signalsmith-inspired), Dense Plate
  (Dattorro tank), Cloud (JPverb loop) and Cosmic Echo all rely on allpass
  diffusion. A Conference-matrix core is genuinely new vocabulary.
- **Rate behavior looks portable**: explicit `overallscale` scaling from the
  host rate plus an internal bezier-interpolated coefficient clock — the same
  undersampling-clock pattern the kPlate140/kGuitarHall2/Galactic3 ports
  already handle. Delay storage is large (dozens of rate-scaled lines per
  channel), so memory budgeting follows the Shimmer per-slot commit approach.
- **Character caveat, from the author**: the highs "don't die away or alter";
  it turns "any noise no matter how percussive into a total wash". There is no
  damping, no decay control, and regeneration can sustain indefinitely.

Consequences for a grid module:

- With zero upstream controls, a wrapper can offer only Mix/Output plus input
  conditioning (Low cut) without altering the DSP. Any Decay/Damping control
  would be a departure from the upstream algorithm — unlike the kPlate140
  ports, whose Sustain/Resolution map to native controls.
- Its fixed "giant undamped wash" voicing overlaps the ambient end of Cloud,
  Galactic Space and Cosmic Echo's Bloom style. It would audition as a
  fourth ambient texture, not replace any of them.
- A genuine secondary use: as a **calibrated reference**, it fits the repo's
  measurement-driven audition method (see the energy-decay fits in
  [COSMIC_ECHO.md](COSMIC_ECHO.md)). It could validate decay measurements
  even if it never ships as a module.

| Candidate | License | What it would add | Cost / risk | Verdict |
| --- | --- | --- | --- | --- |
| **VerbSevens** | MIT | Conference-matrix wash reverb with no allpasses; a voicing no current module has. Also a calibrated reference for decay measurements. | Medium. Huge delay memory per channel; zero native controls, so the wrapper adds only Mix/Output/Low cut or departs from upstream; character overlaps Cloud/Galactic Space/Cosmic Echo. | **Audition last**, alongside Cloud and Galactic Space; adopt only if the wash texture earns a permanent slot |

## Porting notes

- Precedent: Tape was hand-adapted into `dsp/airwindows/AirwindowsTape.h` with
  license beside it; the reverb family is reproduced by the pinned vendor
  script `scripts/vendor-airwindows-reverbs.py`. The simpler candidates here
  (the Outboard Channel combination, Console EQ, Digital Black Gate, ADT, the
  Butter Comp and Density compressors, the clip stage, the Boost Character
  merge, Slew) follow the Tape pattern:
  small enough for focused hand adaptation with explicit rate scaling.
- Sample-rate discipline: airwindows.com tags rate-sensitive plugins; each
  candidate gets the same explicit rate audit that Tape and the reverb ports
  received before any audition.
- All six new-category modules are zero-latency unless the audit finds
  otherwise; compensation latency reporting follows the grid convention.
- Audition protocol: mirror [REVERB_DELAY_AUDITION.md](REVERB_DELAY_AUDITION.md) —
  side-by-side slots, matched loudness, benchmark figures at 128-frame blocks,
  defaults captured in [EFFECT_DEFAULTS.md](EFFECT_DEFAULTS.md).

## Suggested priority

1. **Outboard Channel** — the Outboard category anchor; largest musical gap.
   CStrip's ButterComp plus Console9Channel color as one no-mode module:
   Highpass, Compress, Comp Speed, Color, Fader, Output.
2. **Digital Black Gate** — the third gate voice, small effort; CStrip's gate
   section as a standalone Dynamics module.
3. **Console EQ** — CStrip's lossless EQ with an optional console color knob;
   the third EQ family voice.
4. **ADT** — smallest effort, most immediate guitar benefit.
5. **Butter Comp and Density** — the two very simple, guitar-optimized
   compressors, joining Smooth/Punch for four compressor modules overall;
   optional Compresaturator would make five.
6. **Limiter & Clipper** — the combined module extends the existing Peak Limiter
   with an Airwindows clip stage; useful in nearly every rig.
7. **Air4** — the exciter with high-frequency limiting; then Slew, BrightAmbience3
   and SoftGate as appetite allows. The Console buss stage remains deferred until
   the channel module ships.
