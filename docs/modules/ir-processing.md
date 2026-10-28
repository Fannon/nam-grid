# How IRs are processed

A **Cabinet IR** slot convolves your audio with a mono or stereo WAV impulse response.
Use it for a cabinet and microphone, a recorded space, a resonant object or an
experimental texture. Mono files apply the same response independently to both
inputs, preserving their original sound. Stereo files apply the left response to
the left input and the right response to the right input. After a mono capture,
a stereo IR spreads the signal into left and right, which suits stereo room and
reverb responses. Mono IRs sound exactly as before. Four-channel true stereo files
are not supported. Both lengths accept 16/24/32-bit PCM and 32-bit float WAV files.

## Length

- **Cabinet** keeps the original limit of 8192 samples at the project sample rate
  (about 171 ms at 48 kHz). Existing projects and presets keep this setting and
  their original sound.
- **Long IR** keeps up to 30 seconds at the project rate. Choose it for reverb or
  any response whose late detail matters. Files longer than 30 seconds are cut at
  that boundary. The display shows the retained response's peak envelope and
  duration; it does not estimate an RT60 decay time.

Both lengths resample WAV files to the project rate and preserve quiet tails
within their limit. Very short responses use direct convolution. Longer ones use
FFT convolution; Long IR uses larger partitions for the late tail to keep the
cost manageable. Processing adds no buffering latency. The response's own onset
and the Pre-delay control can still delay the wet sound.

Stereo responses use slightly more CPU and more memory than mono responses.
Long responses also need more memory and CPU. Late-tail work is spread across
audio callbacks to reduce peaks. Use a shorter response or a larger host buffer
if a Long IR leaves too little processing headroom in your rig.

Loading, length changes and reset preparation happen outside the audio callback.
Replacing an IR fades through the transition. The WAV file itself is never edited
or embedded in the rig.

## Wet controls and blending

Input feeds convolution. Pre-delay and the 12 dB/octave cuts shape its wet output.
Output sets the wet level, and Mix linearly blends
that result with the unchanged dry input. At 0% Mix the dry signal is exact,
regardless of the wet controls. At 100% only the processed response is heard.
Input and Output are linear gain controls; Input affects new excitation while
Output can change an existing tail. Pre-delay automation can bend the wet pitch.

For cabinets, keep 100% Mix and start at the original -18 dB Output. For a reverb
insert, choose Long IR, lower Mix and add some Pre-delay to separate the tail from
the attack. On a reverb send, keep 100% Mix. Cut low end to avoid rumble, or lower
High cut to darken a bright tail. Blend unusual responses with dry audio and
adjust Pre-delay to explore parallel cancellation and resonant sound shaping.

See [Captures & cabinets](captures.md#cabinet-ir) for ranges and starting settings.
