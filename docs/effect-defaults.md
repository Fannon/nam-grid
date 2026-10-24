# Starting settings

Every built-in module starts with settings for electric guitar, designed to make the effect
recognizable without overwhelming the dry guitar. They are starting points, not
loudness-matched presets. NAM captures, Guitar EQ, Parametric EQ and Utility start neutral,
so acoustic and bass chains can start cleanly too. The [module guide](modules/README.md)
lists each module's starting value next to every control.

- Adding or replacing a module applies its starting settings, including controls it shares
  with the previous module in that slot. Double-clicking a knob resets it to the same value.
- Saved rigs, projects and Copy/Move/Swap keep their exact settings.
- Every module starts enabled with Tempo sync off; turning sync on starts at a quarter note.
  Output controls start at unity except where a module's guide lists an offset, such as
  Distortion's -6 dB.
- Re-selecting Noise Gate (Smart) keeps its learned noise profile; Learn starts off.
- NAM captures start at unity input and output with flat EQ, cuts off and full quality.
  Cabinet IRs start at -18 dB Output, the usual cabinet headroom; IRs vary in level, so use
  the level strip below the editor to match it.

These are NAM Grid design choices, not another product's factory presets. Thresholds and
makeup depend on your instrument and interface: set them by ear and with the level strip.

## Acoustic and bass adjustments

- For acoustic guitar, begin with neutral EQ and a low Room or Dense Plate mix. Reduce or
  bypass gates if they trim finger noise or long sustain. Compression thresholds depend on
  the pickup or microphone level.
- For bass, lower Cut EQ's Low cut to roughly 30–45 Hz or turn it off; 80 Hz is an
  electric-guitar choice. Reverb Low cuts affect only the wet signal. Keep delay and reverb
  blends low so low notes stay distinct.
- Mono Octaver follows single notes and needs a clear fundamental. On bass, start with less
  sub level, and check tracking in your register.
- Gain, compression and gate defaults cannot compensate for different interfaces or pickup
  outputs. Check the input and output levels and adjust thresholds or trims. There is no
  instrument selector or hidden automatic EQ.
