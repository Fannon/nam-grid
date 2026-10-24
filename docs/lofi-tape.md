# LoFi Tape Machine

Choose **Creative > LoFi Tape Machine** in any grid slot. This is a separate
transport/degradation effect for worn tape and lo-fi guitar sounds. Use
**Distortion > Airwindows Tape** before or after it when saturation is wanted.
The new module does not duplicate that saturation processor.

| Control | Range / starts at | Behavior |
| --- | --- | --- |
| Speed | 1.875–30 ips / 7.5 | Slower tape has less high-frequency bandwidth and slower wow/flutter cycles. Does not retune the average pitch. |
| Wow | 0–100% / 50% | Slow, smooth pitch wander with a small irregular drift component. |
| Flutter | 0–100% / 28% | Faster pitch vibration with a secondary harmonic. |
| Noise | 0–100% / 0% | Band-limited, independent stereo hiss; 0 turns it off. Hiss continues between notes. |
| Wear | 0–100% / 0% | Darkens the sound and adds smooth, randomly spaced level dropouts. |
| Mix | 0–100% / 100% | Blends immediate dry input with processed tape. |
| Output | −24 to +6 dB / 0 | Level after the blend. |

The transport uses a 10 ms wet delay, modulated by up to 5 ms of wow and 0.35 ms
of flutter. Both channels share the transport and dropout envelope; their audio
histories and hiss remain independent. Noise off preserves a centered mono image
and does not crossfeed either channel. Slower speed changes tone and motion rather
than performing a lasting playback-speed pitch shift.

The module adds no latency — the delay is part of the effect. Partial dry/wet blends and parallel clean lanes
can therefore produce chorus/comb coloration. Full wet is the starting point for
a tape-machine treatment. This is an original character effect, not a measured
cassette/deck emulation or magnetic-hysteresis model. No Dusk/CHOWTape DSP is copied.

## Transport display

Two traces above the controls show the actual read-head pitch modulation:

- **Green Wow:** slow pitch drift.
- **Amber Flutter:** rapid pitch vibration.

Both use the same **2.56-second window and ±60-cent vertical scale**. Taller
traces mean stronger pitch motion; more cycles mean faster modulation. The number
at the right is the peak absolute pitch displacement in the displayed history.
Pitch is calculated from changes in each read-head delay, so automation and the
irregular wow component are represented. The display measures the modulation,
not the incoming guitar pitch. It stays flat with the corresponding control at
zero once prior history scrolls away. Noise and dropouts are not pitch motion.

History is captured at 100 Hz using fixed storage on audio and delivered through
the existing meter queue. No UI timer invents movement when audio is stopped.
The display dims at dry Mix, clears on bypass, and indicates when it is awaiting
audio. The two components are shown separately, before Mix.

## Starting settings

- **Worn cassette:** Speed 3.75, Wow 35, Flutter 25, Noise 20, Wear 50, Mix 100.
- **Subtle drift:** Speed 15, Wow 10, Flutter 5, Noise 0, Wear 10, Mix 100.
- **Damaged tape:** Speed 1.875, Wow 70, Flutter 60, Noise 35, Wear 90, Mix 100.

These are suggested settings, not bundled preset files. Adjust Output to match
levels when comparing, and audition with a sustained clean chord as well as
distorted guitar. Add Airwindows Tape separately for saturation.

## Compatibility

Its controls work with Copy/Move/Swap, scenes and
`.ngrid` presets like any other module.
