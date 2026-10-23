# Tuner and tuning history

The **Tuner** button in the header is a host-automatable on/off parameter. It
replaces the lower module editor with a tuner while keeping the routing grid
visible. With **Mute output** enabled (the default), turning Tuner on also mutes
the output. Turn it off, press Escape or click Close to restore sound through a
5 ms ramp. Disable Mute output to hear the processed signal while tuning.

Tuner/mute automation continues when the plugin window closes. A host project
saves the Tuner switch, reference and mute preference. Rig presets and performance
scenes leave them unchanged, so recalling a sound cannot unexpectedly mute or
unmute a live tuner.

The display shows note/octave, signed cents, frequency and a 430–450 Hz reference.
The graph covers the last six seconds, from oldest on the left to now on the
right, over ±25 cents; larger offsets pin to the top or bottom edge. Green indicates
±3 cents, amber ±10 cents, and coral larger errors. The line fades with age and breaks at silence, note changes, long gaps
and jumps over 35 cents. Existing history follows a changed reference.

Detection uses clean input before the grid, including right-only input. The
existing MPM detector and the trace connection rules come from the MIT sibling
trace-tuner revision `2e1ff89243698b8f4fca33dc4505a2cd749d9e67`.
The compact meter/history arrangement also takes inspiration from the Bitwig
example supplied during development. No Bitwig code or assets are included.

A fixed atomic history ring is written by the detection worker and read by the
UI. The audio callback only enables the detector and queues input; it does not
allocate, take the detector mutex, or draw history. Opening, closing or repainting
the editor does not determine the history sampling rate. The current pitch clears
after detection stops; history ages out instead of implying a sustained note.

Tests cover pitch/reference conversion, wrap/expiry, enable generations, trace
breaks, real worker history, no audio allocations, state recall, the
native editor controls, and automation/mute with the editor destroyed. Listening
and host automation mapping should still be auditioned in the target DAW.
