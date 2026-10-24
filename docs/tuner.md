# Tuner and tuning history

The **Tuner** button in the header works like selecting a module: while it is lit,
the tuner replaces the lower module editor and the routing grid stays visible. It is a
host-automatable on/off parameter. With **Mute output** on (the default, at the top of
the tuner's control column), the tuner also mutes the output. Click Tuner again, select
any slot, press Escape or click Close to leave it and restore sound through a 5 ms ramp.
Turn Mute output off to hear the processed signal while tuning.

The left section shows the note, cents and frequency, with the **Reference** knob below
Mute output, and a tuning bar across the whole section: ±50 cents, with the ±3 cent window
shaded.

Tuner/mute automation continues when the plugin window closes. A host project
saves the Tuner switch, reference and mute preference. Rig presets and performance
scenes leave them unchanged, so recalling a sound cannot unexpectedly mute or
unmute a live tuner.

The display shows note/octave, signed cents, frequency and a 430–450 Hz reference.
The graph covers the last six seconds, from oldest on the left to now on the
right, over ±25 cents; larger offsets pin to the top or bottom edge. Green indicates
±3 cents, amber ±10 cents, and coral larger errors. The line fades with age and breaks at silence, note changes, long gaps
and jumps over 35 cents. Existing history follows a changed reference.

Detection uses clean input before the grid, including right-only input. Opening, closing or repainting
the editor doesn't affect detection. The current pitch clears
after detection stops; history ages out instead of implying a sustained note.
