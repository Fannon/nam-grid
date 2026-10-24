# Dynamics

Gates, compressors and limiting. Each module shows its editor with the starting settings while guitar plays; see the
[module guide](README.md) for how the editor, its display and the level strip work.

| Module | In short |
| --- | --- |
| [Noise Gate](#noise-gate) | Threshold opens the gate; Release and Reduction shape the gaps. |
| [Noise Gate (Smart)](#noise-gate-smart) | Learn the noise alone; Sensitivity and Reduction protect sustain. |
| [Digital Black Gate](#digital-black-gate) | Zero-crossing gate darkens quiet tails; Hysteresis adds the strip's re-arming behaviour. |
| [Smooth Compressor](#smooth-compressor) | Gentle leveling; Attack preserves pick, Makeup restores level. |
| [Punch Compressor](#punch-compressor) | Firmer compression; slow Attack and parallel Mix retain punch. |
| [Butter Comp](#butter-comp) | Gentle polarity-sensitive glue with output-dependent recovery; Mix retains dry attack. |
| [Outboard Channel](#outboard-channel) | Capacitor cuts into CStrip2 compression and Spiral colour; Output matches level. |
| [Peak Limiter](#peak-limiter) | Fast peak control; Ceiling and Release set grip and recovery. |

## Noise Gate

![Noise Gate editor](../screenshots/modules/noise-gate.png)

A gate with Natural (gentle 2:1 expansion, 1.5 ms opening, 40 ms hold) and Tight (deep closure, 0.3 ms opening, 15 ms hold) modes.

**Threshold** (-90 dB to -10 dB; starts at -55 dB) Input level that opens the gate; set it above the noise and below your quietest notes.

**Release** (20 ms to 500 ms; starts at 150 ms) Gate closing time; longer values preserve note tails.

**Reduction** (0 dB to 80 dB; starts at 60 dB) Attenuation when the gate is closed; less keeps natural room tone.

In the header:

- **Mode** (Natural, Tight; starts at Natural) Natural closes gently; Tight clamps gaps more firmly. Threshold, Release and Reduction remain independent.

**Display:** The static gate curve with a dot at the live input, and the input/output history.

**Tips:**

- Set Threshold with the guitar's volume up and strings muted, just above the noise.

## Noise Gate (Smart)

![Noise Gate (Smart) editor](../screenshots/modules/noise-gate-smart.png)

Learns your rig's noise profile and reduces it frequency by frequency, with an optional final gate. See [Noise Gate (Smart)](../noise-gate-smart.md) for learning and measurements.

**Learn** (off / on; starts at off) Learn while playing only the unwanted noise, then turn Learn off before playing guitar.

**Sensitivity** (-6 dB to 12 dB; starts at 1.5 dB) Raise the learned noise threshold; too much can reduce quiet sustain.

**Reduction** (0 dB to 48 dB; starts at 24 dB) Maximum reduction in noisy bands. Less can retain more natural room tone and note tails.

**Attack** (0.2 ms to 20 ms; starts at 2 ms) Speed at which the denoiser lets new signal through; short settings preserve pick attacks.

**Release** (20 ms to 500 ms; starts at 120 ms) Recovery toward noise reduction after notes; longer settings preserve sustain.

**Gate threshold** (-100 dB to 0 dB; starts at -100 dB) Optional final gate after denoising. Minimum is Off; raise carefully to avoid cutting sustain.

**Mix** (0% to 100%; starts at 100%) Blend dry and processed sound; higher settings make the effect more prominent. Output, when present, follows the blend.

**Display:** Gain reduction, input and output over time, and the gate threshold.

**Tips:**

- Learn with the guitar connected and strings muted, for a few seconds, then turn Learn off.

## Digital Black Gate

![Digital Black Gate editor](../screenshots/modules/digital-black-gate.png)

Airwindows DigitalBlack's darkening gate, or CStrip's re-arming hysteresis gate in Hysteresis mode. Mode changes fade through dry. Hold is a NAM Grid addition; see [Airwindows dynamics](../airwindows-dynamics.md).

**Threshold** (-74 dB to -10 dB; starts at -55 dB) Level that opens the gate. The -74 to -10 dB range stays inside the native threshold mapping; neither mode has hold or reduction controls.

**Hold** (0 ms to 200 ms; starts at 0 ms) Keeps the gate open this long after the signal last crossed Threshold, so noise near Threshold no longer chatters. A NAM Grid addition; 0 ms is the native gate.

**Output** (-18 dB to 6 dB; starts at 0 dB) Final gain after dry/wet blending. Match bypass loudness before judging the effect.

**Mix** (0% to 100%; starts at 100%) Blend the gate and its darkened closing texture with the original signal. Zero restores the dry sound before Output.

In the header:

- **Mode** (Zero crossing, Hysteresis; starts at Zero crossing) Zero crossing is DigitalBlack's darkening closure. Hysteresis uses CStrip's re-arming gate. Changes fade through dry.

**Display:** Input against output with the threshold, and the attenuation history.

**Tips:**

- If noise near Threshold makes the gate chatter, raise Hold to 30 to 80 ms or set Threshold a few dB higher.

## Smooth Compressor

![Smooth Compressor editor](../screenshots/modules/smooth-compressor.png)

Gentle 2.5:1 compression with a soft 9 dB knee for leveling and sustain.

**Threshold** (-48 dB to 0 dB; starts at -24 dB) Level above which compression reduces dynamics.

**Release** (40 ms to 600 ms; starts at 150 ms) Gain recovery time after a loud note.

**Makeup** (0 dB to 18 dB; starts at 3 dB) Level compensation after compression; compare with bypass at similar loudness.

**Attack** (0.5 ms to 50 ms; starts at 10 ms) How quickly compression catches a note. Slower attack lets more pick transient through.

**Mix** (0% to 100%; starts at 100%) Blend dry and processed sound; higher settings make the effect more prominent. Output, when present, follows the blend.

**Display:** The compression curve with a dot at the live input, and the history before makeup.

**Tips:**

- Lower Threshold until the gain reduction reads 3 to 6 dB on normal playing, then match loudness with Makeup.

## Punch Compressor

![Punch Compressor editor](../screenshots/modules/punch-compressor.png)

Firmer 4:1 compression with a 3 dB knee and a slower default attack, so the pick transient passes. Mix gives parallel compression.

**Threshold** (-48 dB to 0 dB; starts at -18 dB) Level above which compression reduces dynamics.

**Release** (40 ms to 600 ms; starts at 100 ms) Gain recovery time after a loud note.

**Makeup** (0 dB to 18 dB; starts at 2 dB) Level compensation after compression; compare with bypass at similar loudness.

**Attack** (0.5 ms to 50 ms; starts at 25 ms) How quickly compression catches a note. Slower attack lets more pick transient through.

**Mix** (0% to 100%; starts at 80%) Blend dry and processed sound; higher settings make the effect more prominent. Output, when present, follows the blend.

**Display:** The compression curve with a dot at the live input, and the history before makeup.

**Tips:**

- Keep Attack around 20 to 30 ms to let the pick through, and use Mix to bring back dynamics.

## Butter Comp

![Butter Comp editor](../screenshots/modules/butter-comp.png)

A native port of Airwindows ButterComp2: polarity-sensitive glue compression with output-dependent recovery and automatic gain.

**Compress** (0% to 100%; starts at 35%) ButterComp2 intensity and automatic gain recovery, with output-dependent release and independent left/right detectors.

**Output** (-18 dB to 6 dB; starts at -1 dB) Final gain after dry/wet blending, in dB. Match bypass loudness before judging compression.

**Mix** (0% to 100%; starts at 100%) Parallel dry/wet blend. Output follows this blend. The gain-reduction meter excludes automatic gain recovery and Output.

**Display:** Gain reduction, input and output over time.

## Outboard Channel

![Outboard Channel editor](../screenshots/modules/outboard-channel.png)

Airwindows CStrip2: Capacitor cuts into compression and Spiral saturation. The cuts are native percentages, not calibrated Hz. See [Airwindows outboard](../airwindows-outboard.md).

**Low cut** (0% to 100%; starts at 0%) Native Capacitor high-pass amount, with sample-rate pole compensation. Zero is off. Percentage is not a cutoff in Hz.

**High cut** (0% to 100%; starts at 100%) Native Capacitor low-pass openness, with sample-rate pole compensation. 100 is open; lower values darken. Percentage is not a cutoff in Hz.

**Compress** (0% to 100%; starts at 20%) CStrip2 drive into its polarity-sensitive compressor. This percentage controls intensity, not threshold or ratio.

**Comp speed** (0% to 100%; starts at 25%) CStrip2 native recovery speed. Higher values recover faster; this is not attack or release in milliseconds.

**Output** (-18 dB to 6 dB; starts at 0 dB) Final gain after dry/wet blending, in dB. Match bypass loudness before judging compression.

**Saturate** (0% to 100%; starts at 15%) Native unity-to-Spiral output section: increases drive and blends saturation. Output independently matches final loudness.

**Mix** (0% to 100%; starts at 100%) Parallel dry/wet blend. Output follows this blend. The gain-reduction meter excludes automatic gain recovery and Output.

**Display:** The cuts over the output spectrum, the saturation curve and gain reduction.

## Peak Limiter

![Peak Limiter editor](../screenshots/modules/peak-limiter.png)

A zero-lookahead linked peak limiter. Clip soft-clips from 3 dB below the ceiling first, so fast transients need less gain reduction.

**Ceiling** (-24 dB to 0 dB; starts at -1 dB) Peak target. This zero-lookahead limiter cannot guarantee an intersample true-peak ceiling.

**Release** (10 ms to 500 ms; starts at 75 ms) How quickly gain recovers after a loud peak; short settings can sound more aggressive.

**Clip** (0% to 100%; starts at 0%) Soft-clips peaks from 3 dB below the ceiling before limiting, so the gain moves less on fast transients. Adds harmonics; 0% is off.

**Display:** The limiter curve with a dot at the live input, and the history.

**Tips:**

- Use it last in Post to protect your DAW from peaks; keep Clip low for clean tones.
