# LoFi Tape Machine

Choose **Creative > LoFi Tape Machine** in any grid slot. This is a separate
transport/degradation effect for worn tape and lo-fi guitar sounds. Use
**Distortion > Airwindows Tape** before or after it when saturation is wanted.
The new module does not duplicate that saturation processor.

| Control | Range / default | Behavior |
| --- | --- | --- |
| Speed | 1.875–30 ips / 7.5 | Slower tape has less high-frequency bandwidth and slower wow/flutter cycles. Does not retune the average pitch. |
| Wow | 0–100% / 25% | Slow, smooth pitch wander with a small irregular drift component. |
| Flutter | 0–100% / 20% | Faster pitch vibration with a secondary harmonic. |
| Noise | 0–100% / 20% | Band-limited, independent stereo hiss; 0 turns it off. Hiss continues between notes. |
| Wear | 0–100% / 25% | Darkens the sound and adds smooth, randomly spaced level dropouts. |
| Mix | 0–100% / 100% | Blends immediate dry input with processed tape. |
| Output | −24 to +6 dB / 0 | Level after the blend. |

The transport uses a 10 ms wet delay, modulated by up to 5 ms of wow and 0.35 ms
of flutter. Both channels share the transport and dropout envelope; their audio
histories and hiss remain independent. Noise off preserves a centered mono image
and does not crossfeed either channel. Slower speed changes tone and motion rather
than performing a lasting playback-speed pitch shift.

The host reports zero compensation latency, like the existing vibrato/doubler:
the delay is part of the effect. Partial dry/wet blends and parallel clean lanes
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

## Integration and verification

The module appends effect ID 43, preserving existing IDs including Noise Print 64.
Its named controls use the normal slot bank for Copy/Move/Swap, scenes and
`.ngrid` presets. Preview compatibility is described in [development.md](development.md).

All delay memory is allocated in Reset outside audio. Bypass drains with silent
input and disables hiss generation before suspending after one second.

Automated coverage is in `tests/LoFiTapeTests.h` and `tests/LoFiTapeClapTests.inc`:
speed-dependent high-frequency loss, wow/flutter sidebands, dropouts, noise-off
silence and noise-on output, deterministic reset, stereo isolation, exact dry Mix,
sample-rate scaling at 44.1/48/88.2/96/192 kHz, host block partition invariance,
zero audio allocations/deletions, bypass drain, measured visualization values,
schema migration, malformed-state rejection, automation and editor gestures.

The module reports zero compensation latency. Use [build.md](build.md) for
tests and [fx-performance.md](fx-performance.md) to measure the current build.
Real-guitar listening and DAW acceptance remain separate checks.
