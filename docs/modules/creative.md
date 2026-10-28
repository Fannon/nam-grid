# Creative

Freeze, stutter and lo-fi tape.

| Module | In short |
| --- | --- |
| [Freeze / Hold](#freeze--hold) | Hold adds a captured layer; Crossfade smooths and shortens its loop. |
| [Stutter](#stutter) | Window sets repeats; Gate length and Edge fade carve each pulse. |
| [LoFi Tape Machine](#lofi-tape-machine) | Speed sets tone; Wow/Flutter move pitch, Wear adds dropouts. |

<div id="freeze--hold"></div>

## Freeze / Hold

![Freeze / Hold editor](../screenshots/modules/freeze-hold.png)

Captures a window of your playing and loops it underneath while you keep playing.

| Control | Range | Start | What it does |
| --- | --- | --- | --- |
| **Hold** | Off / On | Off | Capture a fresh window and sustain it. Release to stop the held layer; dry guitar remains audible. |
| **Capture** | 50–2000 ms | 500 ms | Length of the held capture; short windows are more tonal, long windows retain more phrase texture. |
| **Crossfade** | 1–100 ms | 20 ms | Overlap at the loop seam; smooths joins and shortens the repeat period by the overlap length. |
| **High cut** | 200–16000 Hz | 6000 Hz | High-cut frequency of the held layer; lower values soften repeated attacks. |
| **Fade in** | 1–2000 ms | 5 ms | Fade-in time for the held layer, independent of the loop-seam Crossfade. |
| **Fade out** | 1–4000 ms | 5 ms | Fade-out time when Hold is released; the dry guitar continues unchanged. |
| **Held level** | 0–100% | 35% | Add the held layer to the dry signal. This is an additive level, not a dry/wet crossfade. |

**Display:** The captured loop and its crossfaded seam.

**Tips:**

- Turn Hold on while a chord rings; Crossfade smooths the loop.

## Stutter

![Stutter editor](../screenshots/modules/stutter.png)

Gated slices of the signal at a set window, with soft edges.

| Control | Range | Start | What it does |
| --- | --- | --- | --- |
| **Window** | 10–1000 ms | 125 ms | Repeat period of the captured slice; tempo sync substitutes a musical division. |
| **Hold** | Off / On | Off | Keeps the current slice repeating instead of refreshing it every four windows. |
| **Gate length** | 5–100% | 75% | Percentage of each slice that sounds; shorter gates leave more rhythmic silence. |
| **Edge fade** | 0.1–20 ms | 5 ms | Fade at each gate edge; smooths clicks and can soften very short gates. |
| **Mix** | 0–100% | 50% | Balance between dry sound and effect. |

In the header:

- **Tempo sync** locks Window to your DAW tempo; **Division** then sets it in note values. See [tempo sync](README.md#tempo-sync).

**Display:** The slice gate over time.

## LoFi Tape Machine

![LoFi Tape Machine editor](../screenshots/modules/lofi-tape-machine.png)

Tape transport character: speed-dependent bandwidth, wow, flutter, hiss, wear and dropouts. See [LoFi Tape Machine](lofi-tape.md).

| Control | Range | Start | What it does |
| --- | --- | --- | --- |
| **Speed** | 1.875–30 ips | 7.5 ips | Tape speed in inches per second. Slower is darker and changes transport character without retuning the guitar. |
| **Drive** | 0–24 dB | 0 dB | Oversampled Spiral saturation before the tape transport; zero bypasses saturation exactly. |
| **Wow** | 0–100% | 50% | Slow pitch drift; the green trace shows measured wow motion in cents. |
| **Flutter** | 0–100% | 28% | Fast pitch vibration; the amber trace uses the same cents scale as wow. |
| **Noise** | 0–100% | 0% | Independent stereo tape hiss, including between notes. Zero turns hiss off. |
| **Wear** | 0–100% | 0% | Darker tape and irregular level dropouts. Drive adds saturation independently. |
| **Output** | -24 to +6 dB | 0 dB | Final level. Match bypass volume when comparing tones. |
| **Mix** | 0–100% | 100% | Balance between dry sound and effect. |

**Display:** The measured wow and flutter pitch motion of the transport.
