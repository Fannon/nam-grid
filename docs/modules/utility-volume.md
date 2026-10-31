# Utility / Volume

Stereo utilities, volume swells and transparent signal inspection.

| Module | In short |
| --- | --- |
| [Utility](#utility) | Balance, Width and polarity reshape stereo; Fine delay aligns paths. |
| [Signal Monitor](#signal-monitor) | Stereo spectrum with resonance markers, peak/RMS meters, level history, headroom and correlation; audio passes unchanged. |
| [Auto Swell](#auto-swell) | Threshold triggers the fade-in; Release closes the next gap. |

## Utility

![Utility editor](../screenshots/modules/utility.png)

Balance, width, polarity and a short fine delay for aligning parallel lanes.

| Control | Range | Start | What it does |
| --- | --- | --- | --- |
| **Balance** | -100 to +100% | 0% | Attenuate one side of existing stereo; this does not pan a mono voice into new stereo. |
| **Width** | -100 to +100% | 0% | -100 folds to mono, zero preserves stereo, +100 doubles the side signal. Mono input stays mono. |
| **Invert L** | Off / On | Off | Reverse this channel's polarity for phase alignment; not an on/off mute. |
| **Invert R** | Off / On | Off | Reverse this channel's polarity for phase alignment; not an on/off mute. |
| **Fine delay** | 0–20 ms | 0 ms | Delay both channels by a few milliseconds for parallel-path alignment; this is an audible offset. |
| **Channel delay** | -20–20 ms | 0 ms | Positive delays the right channel, negative the left, in addition to Fine delay. |
| **Output** | -60 to +12 dB | 0 dB | Final level. Match bypass volume when comparing tones. |

**Channels** in the header selects Stereo, Swap, Left to both or Right to both (starts at Stereo).

**Display:** A goniometer, the left/right correlation and the channel levels.

**Tips:**

- Width cannot create stereo from a mono signal; place stereo effects after a mono capture.

## Auto Swell

![Auto Swell editor](../screenshots/modules/auto-swell.png)

A volume swell that removes the pick attack and fades each phrase in. Pause below the threshold to re-arm.

| Control | Range | Start | What it does |
| --- | --- | --- | --- |
| **Rise** | 10–2000 ms | 350 ms | Fade-in time after the input crosses threshold; longer settings soften the pick attack. |
| **Threshold** | -72 to -12 dB | -48 dB | Input level that triggers a swell. Set below quiet notes but above background noise. |
| **Release** | 20–800 ms | 150 ms | How quickly the swell closes after the input falls below threshold. |
| **Rise curve** | 0.25–4 | 1 | Shape of the fade-in; one retains the original linear swell, higher values enter more slowly. |
| **Re-arm hold** | 0–500 ms | 40 ms | Time below the closing threshold before a new swell can re-arm; 40 ms retains the original hold. |
| **Mix** | 0–100% | 100% | Blend the swollen signal with dry guitar to retain some pick attack. |

**Display:** The swell envelope, and the input against the output.

## Signal Monitor

![Signal Monitor editor](../screenshots/modules/signal-monitor.png)

Place it anywhere to inspect that point in your rig. It has no sound controls,
factory/user presets or added latency. Enabling or bypassing it does not change
samples. Inserting or replacing modules still uses the normal transition fade.

- **Spectrum:** the louder channel at each frequency, rather than a mono sum.
  Opposite-polarity stereo cannot disappear from the spectrum through cancellation.
  A faint line shows a 6-second average while you play.
- **Resonance markers:** after a few seconds of playing, up to three of the most
  prominent narrow peaks in the average are marked with their frequency. Cabinet,
  room and EQ resonances stay put while you play different notes; a single repeated
  note also marks its own harmonics.
- **Meters:** vertical left/right meters from -60 to +6 dBFS with the sample peak
  (line), 300 ms RMS (fill) and held peak (tick, value above). A held peak at or above
  0 dBFS turns the value and the top of the meter red. These are sample peaks, not
  oversampled true-peak measurements.
- **Level history:** the last 20 seconds of monitoring, as a peak line over the RMS
  fill of the louder channel. Red marks along the top show where samples reached
  0 dBFS; a faint vertical line marks where monitoring resumed after a pause.
- **Headroom:** distance from the highest held peak to 0 dBFS; negative means over full scale.
- **Correlation:** +1 for matching channels, −1 for opposite polarity and near
  zero for unrelated signals. Silence has no useful correlation and reads zero.

Analysis runs only while the monitor is selected, enabled and visualizations are
visible. Peak hold resets when monitoring resumes after a pause; the level history
continues. Hover over each part for a short explanation. Use the global
visualization setting to show or hide the display. Other modules' existing footer
meters remain available for quick input/output comparisons.
