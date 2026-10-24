# How cabinet IRs are processed

A **Cabinet IR** slot plays a cabinet impulse response (a mono WAV file) through convolution:
every sample of your signal is shaped by the whole recorded response of the speaker and
microphone.

- **No added latency.** Output for each audio block is delivered immediately, whatever buffer
  size your DAW uses.
- **Short and long IRs are both efficient.** Very short responses are convolved directly;
  longer ones switch automatically to FFT convolution, which keeps the CPU cost low. There is
  no quality setting to choose.
- **Any sample rate.** An IR recorded at another rate is resampled to your project's rate when
  it loads, and its level is matched. Responses are used up to 8192 samples at that rate, far
  longer than a typical cabinet IR.
- **Quiet tails are kept.** The fading end of an IR is preserved rather than cut off, so the
  cabinet sounds the way it was captured.
- **Stereo stays stereo.** The left and right channels each run through the same mono IR, so
  stereo effects before the cabinet keep their width.

Loading or replacing an IR happens away from the audio thread and fades over, so it never
interrupts playback. IR levels vary between libraries: start from the slot's -18 dB Output and
match the level with the strip below the editor. See
[Captures & cabinets](modules/captures.md) for the controls.
