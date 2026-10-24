# Noise Gate (Smart)

A noise gate that learns your rig's noise: play nothing, press **Learn**, and it reduces
that noise frequency by frequency while you play, instead of only opening and closing.

## Try it

1. Add **Dynamics > Noise Gate (Smart)** in a Pre slot, preferably before gain/NAM stages.
2. Leave the guitar/interface at normal playing settings. Mute the strings without
   turning the guitar volume down. Switch **Learn** on for 1–3 seconds, then
   off. Audio continues while learning. Digital silence and captures shorter than
   about 300 ms do not replace an existing valid print.
3. Start with the defaults. Increase **Sensitivity** to suppress more bands; lower
   it if sustain loses detail. **Reduction** limits attenuation, **Attack** opens
   bands, **Release** closes them, and **Mix** blends the spectral result.
   **Gate threshold** adds an optional final broadband gate: its minimum (-100 dB)
   displays **Off**, the default. Raise it just above the noise floor to silence
   pauses. It uses the same Release knob, a fixed 0.5 ms opening ramp, 20 ms hold,
   5 ms peak-detector decay and 3 dB hysteresis. Enabled occupies the last editor
   position, filling all eight positions with seven controls and bypass. The GR
   readout estimates band gain plus gate attenuation, capped at 120 dB; it is not
   a measurement of perceptual noise reduction.
4. Compare against bypass on low notes, quiet picking and decaying chords. Relearn
   after changing pickups, guitar volume, input gain or recording conditions.

Completed snapshots and the seven controls survive project recall and Copy/Move/Swap.
An in-progress Learn is saved/copied as off. During a new capture, the last completed
print remains the recallable snapshot. With Gate Off, untrained, settled Mix=0 and
Reduction=0 pass the exact finite input. Learning transitions to dry over 5 ms and
opens the final gate. Very large/invalid samples are bounded to prevent poisoning
the filter histories.

## How it listens

The gate listens in dozens of frequency bands, learned from your rig's own
noise during Learn. Stereo channels share their open/close decisions, so the
image doesn't wobble. A confidence guard relaxes suppression on strong notes,
keeping sustain natural, then returns to the learned profile for quiet playing.
The optional final broadband gate works independently of the spectral profile, and
it also works without learning. It detects the original input's maximum L/R peak,
applies one linked gain after the spectral mix, and closes linearly to exact zero
over Release after the hold. Either channel can open it, including anti-phase input.
An enabled gate starts closed after reset/recall to avoid an initial noise burst.
Turning Gate Off reopens it over 0.5 ms. Keep the module before delay/reverb to
preserve their tails. The standalone Noise Gate offers Natural and Tight modes.

Noise overlapping a wanted note can't be fully removed, and strong filtering can
slightly change the tone.

The module adds no latency, though like any filter it isn't phase-neutral.

Reset preserves the completed print but clears envelopes/histories. If you change
sample rate, relearning gives the best results.

## Cost and persistence

An ordinary gate is cheaper when only silence
between notes matters; assess pick attacks and long decays at matched levels.

Snapshots survive project recall, rig-preset save/load and Copy/Move/Swap. Learn
is transient: it is saved off, suppressed on rig-preset load and excluded from
performance scenes. Scenes adjust controls; the profile remains attached to
the slot. Profiles saved by older builds that this version no longer understands
are rejected without harming the current sound.
