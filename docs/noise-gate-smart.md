# Noise Gate (Smart)

A noise gate that learns your rig's noise. Play nothing, switch on **Learn**, and it reduces that noise frequency by frequency while you play, instead of only opening and closing. Quiet picking and decaying chords stay more natural than with an ordinary gate.

![Noise Gate (Smart) editor](screenshots/modules/noise-gate-smart.png)

## Set it up

1. Add **Dynamics > Noise Gate (Smart)** in a Pre slot, before drives and captures.
2. Keep your guitar and interface at normal playing settings. Mute the strings with your hand, without turning the guitar's volume down.
3. Switch **Learn** on for one to three seconds, then off. Audio keeps playing while it learns.
4. Play with the starting settings first, and compare against bypass on low notes, quiet picking and decaying chords.

Learning ignores digital silence and captures shorter than about 300 ms; the previous noise profile stays in place.

## Fine-tune

- **Sensitivity** sets how far above the learned noise a band must rise to pass. Raise it to suppress more noise; lower it if sustain loses detail.
- **Reduction** limits how much a noisy band is turned down. Less keeps some natural room tone.
- **Attack** and **Release** set how quickly bands open for new notes and close after them. Short attacks keep pick attacks; longer releases keep sustain.
- **Mix** blends the denoised sound with the original.
- **Gate threshold** adds an optional final gate that silences the pauses completely. It is **Off** at its minimum, the default. Raise it to just above the noise floor; it shares the Release knob and has a short hold so it does not chatter.

The **GR** readout estimates the current reduction. It is a guide, not a measurement of how much quieter the noise sounds.

## When to learn again

Learn again after changing pickups, guitar volume or input gain, moving to a different room or interface, or changing the sample rate.

The learned profile is saved with your project and rig preset and travels with Copy, Move and Swap. Learn itself is never saved switched on, and scenes do not change the profile.

## Good to know

- Keep it before delays and reverbs, so their tails are not reduced.
- Noise that overlaps a note cannot be removed completely, and strong settings can slightly change the tone.
- It adds no latency.
- When only the silence between notes matters, the standard [Noise Gate](modules/dynamics.md#noise-gate) does the job with less CPU.
