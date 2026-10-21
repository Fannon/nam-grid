# Reverb and delay audition build

The first audition package has 48 built-in FX choices. The subsequent
[Cosmic Echo prototype](COSMIC_ECHO.md) adds the 49th, with three styles.
Five additions in this first package are intentionally
available side by side for listening before deciding what earns a permanent
place. Room, Dense Plate, Cloud and Hall also have updated cores. These are
audition candidates, not a claim that source topology establishes sound quality.

## What changed

| Module | Current core / useful comparison |
| --- | --- |
| Room / Hall | Signalsmith-inspired multistage diffusion and eight-line Householder FDN; different time scales, early-reflection weights and bass/treble decay shaping |
| Dense Plate | Clouds/Griesinger/Dattorro-style two-branch tank; compare its smoothness and attack with Airwindows Plate |
| Cloud | JPverb-inspired rotating stereo allpass loop; an adaptation, not a faithful JPverb port |
| Multi-head Tape | New four-head echo: Single, Dual, Syncopated and All heads. The last head alone supplies feedback |
| Diffused Delay | New Greyhole-inspired echo/allpass cascade; Diffusion moves from a clear repeat to stereo reflection clusters |
| Airwindows Plate | SDK-free kPlate140 port retaining its nonlinear input and native sustain/resolution behavior |
| Airwindows Guitar Hall | SDK-free kGuitarHall2 port retaining its early-reflection selection and internal processing-rate behavior |
| Galactic Space | SDK-free Galactic3 port for an evolving, detuned ambient bed; compare especially with Cloud and Shimmer |

The three Airwindows cores retain their double-precision sample loops, with
deterministic reset and a fixed 32-sample coefficient clock. Their original
dry/wet law is replaced by the grid's linear Mix and output trim. Plate's main
pre-delay is external and measured in host-rate milliseconds. The vendor script
pins its upstream revision and reproduces the headers without a VST2 SDK.
Source revisions and MIT notices are included beside the cores and in the
distributed third-party notices.

## Defaults and ranges

Defaults are listed in [EFFECT_DEFAULTS.md](EFFECT_DEFAULTS.md). The shared
conventional delay Time range is **5–2000 ms**, opening up doubling/slapback and
longer ambient repeats. Synced notes can reach **8 seconds** at the minimum
supported tempo. Multi-head Time always means the last head: the read positions
are 1/4, 1/2, 3/4 and full Time. Dual selects the half/full pair; Syncopated selects
quarter/three-quarter/full. Output weights sum to one and extra heads do not
increase feedback gain.

The updated Room/Plate/Cloud/Hall controls span **0.15–30 s nominal Decay**,
**25–200% Size**, **0–200 ms Pre-delay**, **800 Hz–12 kHz Damping**,
**0–500 Hz wet Low cut** and **0–100% Modulation/Mix**. Decay describes a nominal
mid-band target; damping, allpass group delay and interpolation affect the actual
frequency-dependent decay. Their Size defaults were adjusted for the new tanks.
Diffuser/FDN delay changes crossfade fixed read positions; Plate/Cloud's long
tank reads retain pitch movement when Size changes.

Diffused Delay adds **0–100% Diffusion**. Zero bypasses the smear in its output and
feedback paths; higher values add reflection clusters. Delay Feedback remains
bounded at **82%**. Both new delays offer tempo sync and Modulation; their stereo
topologies have no unused Ping-pong switch.

| Airwindows control | Exposed range and interpretation |
| --- | --- |
| Plate Input | −18–0 dB; nonlinear tank excitation |
| Plate Sustain | 0–100%; native regeneration, not seconds |
| Plate Pre-delay / Low cut | 0–200 ms / 0–500 Hz; wet send only |
| Hall Sustain | 0–95%; avoids the native maximum-feedback endpoint |
| Hall Brightness / Early level / Position | 0–100%; Position selects native discrete early-reflection configurations |
| Space Replace | 5–100%; lower retains the existing wash longer; avoids infinite hold |
| Space Brightness / Size | 5–100%; retains useful nonzero endpoints |
| Space Detune | 0–100%; native stereo pitch motion |
| Resolution, all three | 25–100%; mapped into each core's native undersampling control, so equal percentages do not imply equal processing rates |
| Mix / Output, all three | 0–100% / −18 to +6 dB |

Resolution has native rate steps, Hall Position has discrete configurations, and
Galactic Size resizes a sounding tank. Automation can therefore produce audible
transitions, pitch changes or noise despite control smoothing. These behaviors
are described in the tooltips; listening must decide whether to retain, restrict
or replace the native controls. They are not advertised as transparent automation.

## Displays

Multi-head Tape's sketch shows the selected taps and feedback repeats. Diffused
Delay shows widening echo clusters as Diffusion increases. These are parameter
sketches, excluding nonlinear saturation and actual moving tape pitch. The
conventional reverb sketches show nominal size, damping and decay on a 30-second
axis. Airwindows modules show actual input/output histories, rather than deriving
a fictitious RT60 from their native percentages. Global/live meters remain
available during listening.

## Local audition files

The current Windows Release CLAP/VST3 builds are in `build-grid/out/`. Generated
files are in `build-grid/fx-audition/`: nine one-module `.ngrid` presets, matching
synthetic plucked-chord WAVs, fully wet impulse WAVs and `measurements.csv`.
The presets leave library folders and tuner settings local to the instance.
The WAVs are stereo float32, 48 kHz, eight seconds, and are not normalized.
The source is synthetic, so use real clean, sustained, distorted and bass guitar
before selecting winners. Match wet loudness when comparing tone and density.

Reproduce from a configured Release build:

```powershell
cmake --build build-grid --target nam-grid-clap nam-grid-vst3 nam-fx-audition
& .\build-grid\NeuralAmpModeler\nam-fx-audition.exe .\build-grid\out\nam-grid.clap .\build-grid\fx-audition
```

One local 48 kHz/128-frame run measured the following. CPU is wall time inside
the complete plugin processing callback, as a percentage of the audio duration,
for one occupied slot. It excludes synthetic input generation and file writing.
It is a single offline run, not a DAW/underrun guarantee or a controlled comparison
against the earlier cores. Room/Hall's extra diffusion costs more than the old
shared FDN; this pass should not be presented as a CPU improvement for those two.

| Module | Callback CPU | Estimated default wet RT60 |
| --- | ---: | ---: |
| Room | 3.22% | 0.70 s |
| Dense Plate | 1.28% | 2.54 s |
| Cloud | 2.19% | 5.46 s |
| Hall | 3.27% | 2.79 s |
| Multi-head Tape | 1.14% | 2.91 s |
| Diffused Delay | 1.83% | 3.84 s |
| Airwindows Plate | 1.23% | 3.72 s |
| Airwindows Guitar Hall | 0.90% | 2.63 s |
| Galactic Space | 0.80% | 9.68 s |

RT60 is extrapolated from a −5 to −25 dB fit to the stereo impulse energy decay,
truncated at eight seconds. It is a sanity check, especially for non-exponential
or very long ambient tails; delay RT60 is descriptive, not a user control.

## Verification and remaining choices

DSP checks cover 44.1/48/96/192 kHz, full-range automation, tiny/uneven buffer
partitions, exact dry stereo at zero Mix, finite output and no audio allocations
or deletions. Additional tests hold the longest native Airwindows feedback
settings for sixteen seconds at 48/96 kHz, at full and reduced resolution.
Impulse checks verify every head pattern against the displayed timing and verify
that diffusion adds a stereo cluster. Default impulse decay was measured through
the actual CLAP plugin; it is not inferred solely from coefficients.

The Windows Release CLAP/VST3 targets built successfully. All five CTest suites
passed (480.05 s total, including the expanded native CLAP host/editor suite at
462.34 s). After adding the sixteen-second maximum-feedback tests, the focused
stages suite passed again in 19.99 s. Pinned Airwindows header/license
regeneration reproduced the generated files exactly.

The expanded native host suite checks module creation, visible controls, enum
automation and state recall. Final DAW listening, multi-slot low-buffer stress,
macOS and VST3-host behavior remain release checks. Audition first, then remove
redundant modules or promote convincing styles. The subsequently authorized
Supermassive reconstruction prototype is described in [COSMIC_ECHO.md](COSMIC_ECHO.md).
