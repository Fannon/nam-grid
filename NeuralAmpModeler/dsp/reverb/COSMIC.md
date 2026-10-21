# Cosmic Echo prototype

Cosmic Echo adapts three modes from David Braun's MIT
`re.valhallaSupermassive` reconstruction in `reverbs.lib`, pinned at
`grame-cncm/faustlibraries fadc1f7da4a903de25554e538c7a454a2f130299`.
The upstream function explicitly declares author David Braun and license MIT.
See the adjacent LICENSE.txt and distributed third-party notices.

This is a C++ adaptation of an independent reconstruction, not Valhalla's source
or an assertion of identical sound. Sean Costello's published algorithm designs
and Valhalla Supermassive are design references. No Faust runtime, generated
helper code or additional SDK is used.

- Smooth follows Gemini: sixteen modulated delays, four butterfly rotation
  stages, alternating stereo injection and normalized output projection.
- Bloom follows Sagittarius: eight serial stereo matrix allpasses, removing
  the instantaneous eighth-power feedthrough, with the model's 32-sample wet
  output delay. Feedback controls allpass coefficient and affects attack.
- Sparse follows Virgo: four delays, high/low cuts inside feedback, a four-way
  tensor scattering matrix and output from the first stereo pair.

The port preserves the cubic Warp law, density-to-rotation angle, model's
33-sample line offset, moving first-order Thiran interpolation, quadrature
modulation phases, feedback/injection laws and mode-specific filter placement.
Filters, ring updates and feedback delays are written directly in C++; no
bit-identical generated-Faust equivalence is claimed. Impulse tests check the
zero-feedback, zero-warp arrival times, including Bloom's eight-stage delay.

Deliberate integration differences:

- Linear outer Mix, matching the grid, replaces the source's equal-power mix.
- Main controls are Time, Warp, Feedback, Density, Tone, Motion and Mix. Style
  is in the header. Modulation rate is fixed at 0.3 Hz, stereo width at full,
  low cut at 80 Hz. They are documented choices, not hidden inactive parameters.
- Time spans 5–2000 ms, Feedback 0–98%, Tone 200 Hz–16 kHz; other controls span
  0–100%. Avoid the zero-time and infinite-feedback endpoints in this prototype.
  Tempo sync is not exposed: eight-second synced values would exceed the
  source's two-second line scale or require substantially more memory.
- Smoothed controls update coefficients every 32 samples; the oscillator and
  all feedback clocks advance per sample. Interpolation reads both integer
  taps at the current position as in the source, rather than using a tap saved
  before a moving read head changed position.
- One sixteen-line bank is prepared off audio, including when Sparse is selected
  so automation can switch to either larger style. Storage is about 12.3 MiB at
  48 kHz or 49.2 MiB at 192 kHz per selected module. Unselected modules do not
  allocate this bank. There is no core duplication during style transitions.
- Style changes fade the wet path down over 20 ms, logically clear the lines,
  and fade the new style up over 20 ms. Dry stereo is preserved. Old tails are
  discarded; this is intentionally not a seamless tail-preserving morph.
  Filled-sample counts prevent stale buffer reads without large clears on audio.
- Bypass drain sets feedback to zero. Host-reported render tails retain the
  plugin's finite 60-second policy; extreme feedback settings can outlast that
  policy while playing. This needs a release policy if the module is retained.

The display shares the nominal delay-length law with the DSP, shows Density as
scattering spread, and puts actual input/output level histories below the sketch.
It is not a measured impulse response or a fabricated LFO animation.

Test coverage includes all three styles at 44.1/48/96/192 kHz, full-range
automation with unequal partitions, exact dry stereo, style-change allocation
guards, default impulse differences and timing, sustained maximum-feedback tests
at both minimum and maximum Time, and old-tail clearing after style changes. Native host
tests exercise enum metadata/automation, style recall in first/last slots and
schema-20 audition preset recall. Musical quality still needs direct listening.

Source:
https://github.com/grame-cncm/faustlibraries/blob/fadc1f7da4a903de25554e538c7a454a2f130299/reverbs.lib
