# Cosmic Echo

Cosmic Echo adapts six modes from David Braun's MIT
`re.valhallaSupermassive` reconstruction in `reverbs.lib`, pinned at
`grame-cncm/faustlibraries fadc1f7da4a903de25554e538c7a454a2f130299`.
The upstream function explicitly declares author David Braun and license MIT.
See the adjacent LICENSE.txt and distributed third-party notices.

This is a C++ adaptation of an independent reconstruction, not Valhalla's source
or an assertion of identical sound. Sean Costello's published algorithm designs
and Valhalla Supermassive are design references. No Faust runtime, generated
helper code or additional SDK is used.

- Gemini: sixteen modulated delays, four butterfly rotation
  stages, alternating stereo injection and normalized output projection.
- Sagittarius: eight serial stereo matrix allpasses, removing
  the instantaneous eighth-power feedthrough, with the model's 32-sample wet
  output delay. Feedback controls allpass coefficient and affects attack.
- Virgo: four delays, high/low cuts inside feedback, a four-way
  tensor scattering matrix and output from the first stereo pair.

- Centaurus: four serial four-channel matrix allpasses with quadrature
  modulation, fourth-power feedthrough removal and a 32-sample output delay.
- Great Annihilator: a plain four-channel delay/scattering stage followed by
  three matrix allpasses in a global feedback loop; first-stage lines add
  32 samples and the global gain follows Feedback directly.
- Lyra: four scattering delays, normalized output sums before the matrix,
  and high/low cuts at the output rather than inside the loop.

The port preserves the cubic Warp law, density-to-rotation angle, model's
33-sample line offset, moving first-order Thiran interpolation, quadrature
modulation phases, feedback/injection laws and mode-specific filter placement.
Filters, ring updates and feedback delays are written directly in C++; no
bit-identical generated-Faust equivalence is claimed. Impulse tests check the
zero-feedback, zero-warp arrival times, including Sagittarius's eight-stage delay.

Deliberate integration differences:

- Linear outer Mix, matching the grid, replaces the source's equal-power mix.
- Main controls are Time, Warp, Feedback, Density, High cut, Motion and Mix. Style
  is in the header. Modulation rate is fixed at 0.3 Hz, stereo width at full,
  low cut at 80 Hz. They are documented choices, not hidden inactive parameters.
- Time spans 20–8000 ms, Feedback 0–98%, High cut 200 Hz–16 kHz; other controls span
  0–100%. The ranges avoid the zero-time and infinite-feedback endpoints.
  Tempo sync is not exposed. The longer Time range reserves
  eight seconds per line, increasing memory relative to the source's two-second limit.
- Smoothed controls update coefficients every 32 samples; the oscillator and
  all feedback clocks advance per sample. Interpolation reads both integer
  taps at the current position as in the source, rather than using a tap saved
  before a moving read head changed position.
- One sixteen-line bank is prepared off audio, including when either four-line mode is selected
  so automation can switch to either larger style. Storage is about 47.2 MiB at
  48 kHz or 188.7 MiB at 192 kHz per selected module. Unselected modules do not
  allocate this bank. There is no core duplication during style transitions.
- Style changes fade the wet path down over 20 ms, logically clear the lines,
  and fade the new style up over 20 ms. Dry stereo is preserved. Old tails are
  discarded; this is intentionally not a seamless tail-preserving morph.
  Filled-sample counts prevent stale buffer reads without large clears on audio.
- Bypass drain sets feedback to zero and runs long enough to flush eight maximum-length lines (about 65 seconds at the current range). A regression verifies that an old impulse cannot return after re-enabling. Host-reported render tails retain the
  plugin's finite 60-second policy; extreme feedback settings can outlast that
  policy while playing. This needs a release policy if the module is retained.

The display shares the nominal delay-length law with the DSP, shows Density as
scattering spread and modulation as a halo, in a larger dedicated canvas. It does not show a synthetic input/output history.
It is not a measured impulse response or a fabricated LFO animation.

Test coverage includes all six styles at 44.1/48/96/192 kHz, full-range
automation with unequal partitions, exact dry stereo, style-change allocation
guards, default impulse differences and timing, sustained maximum-feedback tests
at both minimum and maximum Time, and old-tail clearing after style changes. Native host
tests exercise enum metadata/automation, style recall in first/last slots and
schema-20 audition preset recall. Musical quality still needs direct listening.

Source:
https://github.com/grame-cncm/faustlibraries/blob/fadc1f7da4a903de25554e538c7a454a2f130299/reverbs.lib

## Aurora Hall (original NAM Grid design)

Aurora Hall appends style ID 6 and key `orion` (retained from its initial working name). The six reconstructed styles above retain their IDs, control laws and DSP. No new host parameters or saved fields are introduced; the existing state-only Style list grows additively. Insertion and editor reset now select Aurora Hall with its guitar-hall recipe; released descriptor defaults and loaded state are unchanged.

Eight main delay loops use clustered, unequal lengths. Warp increases their spread. Each loop contains a short, irregularly sized allpass diffuser. Density maps to both the angle of an orthogonal three-stage butterfly rotation and the allpass coefficient (0–0.65). At zero Density, loops remain separate and each allpass reduces to a plain short delay. At full Density, scattering and diffusion build a fuller tail. The allpass is not crossfaded with dry sound, avoiding comb cancellation at intermediate settings.

The loop highpass is normalized to have a maximum magnitude of one. High cut and low cut act on each of the eight loop states, before scattering on the next sample. Loop gain follows the existing square-root Feedback law and remains below one at the 98% maximum. Stereo injection and output projections each have unit norm. Delay modulation is capped at 1% of each line's base length; the existing Thiran interpolator and 0.3 Hz oscillator are reused. The entire network runs at host rate, with all Aurora Hall timing offsets expressed in seconds. The sixteen-line storage bank is reused without new delay allocations. Logical Clear also clears the diffuser histories.

Design references are orthogonal feedback delay networks and allpass diffusion, as discussed by [Rocchesso and Smith](https://air.unimi.it/handle/2434/1041969) and [Dattorro](https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf). Airwindows [Echojam](https://www.airwindows.com/echojam/) is a conceptual reference for clustered reflections and [kGuitarHall2](https://www.airwindows.com/kguitarhall2/) for guitar-oriented hall texture. Aurora Hall does not port their source, delay tables, nonlinearities or sample-rate conversion, and does not claim to reproduce their sound.

Regression coverage extends the common mode tests (automation, partition independence, exact dry, allocations, timing and transitions), plus sparse/dense impulse behavior at both zero and nonzero Warp, energy through intermediate Density settings, short/long feedback tails and onset consistency across host rates. Musical hall quality still requires listening; these are not acoustic-room or perceptual-equivalence tests.
