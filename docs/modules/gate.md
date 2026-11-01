# Gate

Noise gating and learned noise reduction.

| Module | In short |
| --- | --- |
| [Noise Gate](#noise-gate) | Threshold opens the gate; Release and Reduction shape the gaps. |
| [Noise Gate (Smart)](#noise-gate-smart) | Learn the noise alone; Sensitivity and Reduction protect sustain. |
| [Digital Black Gate](#digital-black-gate) | Zero-crossing gate darkens quiet tails; Hysteresis adds the strip's re-arming behaviour. |

## Noise Gate

![Noise Gate editor](../screenshots/modules/noise-gate.png)

A gate with Natural (gentle 2:1 expansion, 1.5 ms opening, 40 ms hold) and Tight (deep closure, 0.3 ms opening, 15 ms hold) modes.

| Control | Range | Start | What it does |
| --- | --- | --- | --- |
| **Threshold** | -90 to -10 dB | -55 dB | Input level that opens the gate; set it above the noise and below your quietest notes. |
| **Release** | 20–500 ms | 150 ms | Gate closing time; longer values preserve note tails. |
| **Reduction** | 0–80 dB | 60 dB | Attenuation when the gate is closed; less keeps natural room tone. |
| **Attack** | 0–20 ms | 0 ms | Gate opening time; zero follows the original Natural or Tight mode timing. |
| **Hold** | 0–300 ms | 0 ms | Time before closure; zero follows the original Natural or Tight mode hold. |
| **Hysteresis** | 0–12 dB | 3 dB | Closing threshold below the opening threshold; higher values reduce chatter. |

In the header:

- **Mode** (Natural, Tight; starts at Natural) Natural closes gently; Tight clamps gaps more firmly. Threshold, Release and Reduction remain independent.

**Display:** The static gate curve with a dot at the live input, and the input/output history.

**Tips:**

- Set Threshold with the guitar's volume up and strings muted, just above the noise.

## Noise Gate (Smart)

![Noise Gate (Smart) editor](../screenshots/modules/noise-gate-smart.png)

Learns your rig's noise profile and reduces it frequency by frequency, with an optional final gate. See [Noise Gate (Smart)](noise-gate-smart.md) for setup and tips.

| Control | Range | Start | What it does |
| --- | --- | --- | --- |
| **Learn** | Off / On | Off | Learn while playing only the unwanted noise, then turn Learn off before playing guitar. |
| **Sensitivity** | -6 to +12 dB | 1.5 dB | Raise the learned noise threshold; too much can reduce quiet sustain. |
| **Reduction** | 0–48 dB | 24 dB | Maximum reduction in noisy bands. Less can retain more natural room tone and note tails. |
| **Attack** | 0.2–20 ms | 2 ms | Speed at which the denoiser lets new signal through; short settings preserve pick attacks. |
| **Release** | 20–500 ms | 120 ms | Recovery toward noise reduction after notes; longer settings preserve sustain. |
| **Gate threshold** | -100 to 0 dB | -100 dB | Optional final gate after denoising. Minimum is Off; raise carefully to avoid cutting sustain. |
| **Mix** | 0–100% | 100% | Balance between dry sound and effect. |

**Display:** Gain reduction, input and output over time, and the gate threshold.

**Tips:**

- Learn with the guitar connected and strings muted, for a few seconds, then turn Learn off.

## Digital Black Gate

![Digital Black Gate editor](../screenshots/modules/digital-black-gate.png)

A gate after Airwindows DigitalBlack that darkens notes as they close, or the firmer gate from Airwindows CStrip in Hysteresis mode.

| Control | Range | Start | What it does |
| --- | --- | --- | --- |
| **Threshold** | -74 to -10 dB | -55 dB | Level that opens the gate. |
| **Hold** | 0–200 ms | 0 ms | Keeps the gate open this long after the signal last crossed Threshold, so noise near Threshold does not make it chatter. 0 ms is the original Airwindows gate. |
| **Output** | -18 to +6 dB | 0 dB | Final level. Match bypass volume before judging the effect. |
| **Mix** | 0–100% | 100% | Balance between dry sound and the gate with its darkened closing texture. |

In the header:

- **Mode** (Zero crossing, Hysteresis; starts at Zero crossing) Zero crossing darkens notes as they close; Hysteresis is a firmer gate that re-arms cleanly. Switching fades smoothly.

**Display:** Input against output with the threshold, and the attenuation history.

**Tips:**

- If noise near Threshold makes the gate chatter, raise Hold to 30 to 80 ms or set Threshold a few dB higher.
