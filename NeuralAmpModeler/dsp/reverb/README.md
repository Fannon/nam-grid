# Reverb cores

These are rate-scaled double-precision adaptations for nam-grid's control and
preparation lifecycle, with different stereo/decay/range choices. They are not
bit-identical ports or physical simulations. The adjoining LICENSE.txt preserves
the permissive source notices.

- DiffusionFDN follows Signalsmith Basics' multistage diffuser + Householder FDN
  structure. It uses normalized matrices, deterministic tap spacing, two-channel
  mapping, per-line RT60 feedback gains and contractive bass/treble shelving.
  Source: Signalsmith-Audio/basics `369e906e03760ec219d07b06fa06dc4fd099f377`,
  `include/signalsmith-basics/reverb.h` (MIT).
- PlateTank follows the Griesinger/Dattorro topology in Emilie Gillet's Clouds:
  four input allpasses and two cross-coupled, modulated tank branches. It replaces
  the fixed 32 kHz / 12-bit storage and native regeneration knob with floating
  storage, host-rate scaling and a nominal decay-time control.
  Source: pichenettes/eurorack `f4cfa7697095a28d292bff758beb51deb49b5a86`,
  `clouds/dsp/fx/reverb.h` (MIT).
- CloudTank and the diffused delay use rotating stereo allpasses informed by
  Julian Parker's JPverb/Greyhole. They use bounded, rate-scaled storage and
  nam-grid-specific delay-time, gain and damping calibration. They are not
  faithful reproductions of the full Faust filters/prime-delay tables.
  Source: grame-cncm/faustlibraries `fadc1f7da4a903de25554e538c7a454a2f130299`,
  `reverbs.lib` JPverb/Greyhole section (explicit MIT declarations).

All allocations occur in Reset. Diffuser/FDN and pre-delay changes coalesce into
two-read crossfades; the Plate/Cloud long tank paths retain smoothed moving reads,
so Size automation can bend their tails. Coefficients use a sample-clock quantum.
The public Decay is a nominal
mid-band RT60 target, rather than a guarantee for every individual frequency.
Airwindows' separately credited audition ports retain native sustain/resolution
controls and live output histories rather than claiming calibrated decay times.

CosmicReverb is the later three-style ambient echo/reverb prototype, adapting
David Braun's MIT Faust Supermassive reconstruction. See [COSMIC.md](COSMIC.md)
for its distinct line/interpolation laws, fixed controls, source and transition
policy; it does not use the conventional Decay/Size wrapper above.
