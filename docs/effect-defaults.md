# Guitar effect defaults

These are electric-guitar insertion settings, designed to identify each effect without immediately overwhelming a dry guitar. They are starting points, not loudness-matched presets or universal settings for every pickup. NAM gain, Guitar EQ, Parametric EQ and Utility remain neutral so acoustic and bass chains can start cleanly too.

Adding/replacing a built-in module applies its settings from `SlotDefaults.h`, including shared parameters left by the previous module. Double-clicking a module dial resets that parameter to the same module-specific value. Existing project/rig recall and Copy/Move/Swap keep the saved settings. Re-selecting Smart Gate preserves its learned noise profile; Learn stays off on insertion. All modules start enabled, with tempo sync off; enabling sync initially uses a quarter note. Other than the output offsets listed below, module output controls start at unity.

## Outboard starting settings

The first [Airwindows outboard modules](airwindows-outboard.md) add these defaults:

| Module | Starting settings | Purpose |
| --- | --- | --- |
| Outboard Channel | Low cut 0%; High cut 100%; Compress 40%; Comp speed 25%; Saturate 15%; Output −2 dB; Mix 100% | Gentle native CStrip2 compression and console colour, with cuts initially open |
| Console Colour | Dark desk; Input trim 0 dB; Output trim 0 dB; Mix 100% | Console convolution/saturation with fixed desk level matching |
| Console EQ | Bands 0 dB; Treble coefficient scale 4750 Hz; Bass scale 300 Hz; High cut 100%; Low cut 0% | Neutral insertion; first Bass adjustment targets the low end of its native range |
| Butter Comp | Compress 35%; Output −1 dB; Mix 100% | Gentle ButterComp2 glue with output-dependent recovery |
| Digital Black Gate | Threshold −55 dB; Mix 100%; Output 0 dB; Zero crossing mode | Darkening closure; header Hysteresis offers CStrip's re-arming behaviour |

Capacitor cuts use native percentages rather than uncalibrated Hz. Low cut 0 is
off; High cut 100 is open. Band gains are nominal ±6 dB density controls with
level-dependent harmonics. Compress and Comp speed use native 0–100%, with no
invented ratio/attack/release units. See the implementation audit for exact mappings.
Digital Black Threshold spans −74…−10 dB, staying inside its native floor. It receives
additional [rate and meter corrections](airwindows-dynamics.md) from the source audit.

These defaults are NAM Grid design choices, not another product's factory
presets. Treat them as starting points and adjust input-dependent thresholds and
makeup to the actual instrument. Source ranges and reset values live in
`EffectParameters.h` and `SlotDefaults.h`.

## Insertion settings

Times are milliseconds unless marked seconds. Percentages are explicit. Controls not listed use their descriptor defaults. The table covers every built-in module in the menu; mode choices remain available after insertion.

| Module | Starting settings | Intended guitar use |
| --- | --- | --- |
| Distortion | Soft clip; Drive 9 dB; Tone 5.5 kHz; Bias 0%; Mix 100%; Output −6 dB | Moderate soft drive, with room to increase gain |
| Boost | Gain +3 dB; Tilt 0 dB; high-pass off | Mild push into a capture |
| Tube Boost | Drive 6 dB; Asymmetry 25%; Mix 100%; Output −3 dB; Tilt 0 dB | Warmer push with output compensation |
| Airwindows Tape | Drive 0 dB; Bump 25%; Mix 100%; Output 0 dB | Subtle saturation, restrained low-end bump |
| Bitcrusher | 10 bits; 16 kHz sample rate; Tone 6.5 kHz; Mix 50% | Audible grit while retaining the guitar attack |
| Noise Gate | Natural mode; Threshold −55 dB; Release 150; Reduction 60 dB | General noise-floor cleanup; set threshold to the rig |
| Noise Gate (Smart) | Learn off; Sensitivity +1.5 dB; Reduction 24 dB; Attack 2; Release 120; Gate threshold off; Mix 100% | Preserve the auditioned learned-noise behavior |
| Smooth Compressor | Threshold −24 dB; Release 150; Makeup +3 dB; Attack 10; Mix 100% | Moderate leveling/sustain |
| Punch Compressor | Threshold −18 dB; Release 100; Makeup +2 dB; Attack 25; Mix 80% | Preserve pick transients with parallel compression |
| Peak Limiter | Ceiling −1 dB; Release 75; Clip 0% | Catch peaks; no default input boost or clipping |
| Guitar EQ | Bass/Mid/Treble flat | Neutral tone shaping |
| Cut EQ | High-pass 80 Hz; Low-pass 10 kHz; Presence flat | Electric-guitar cleanup |
| Parametric EQ | 800 Hz; Gain 0 dB; Q 0.7; Notch off; cuts off | Neutral band, ready to locate a problem |
| Filter | Low-pass; 2.5 kHz; Q 0.71; Envelope 0 octaves | A static, gently resonant filter; envelope optional |
| Wah | Position 50%; Resonance Q 3; Register 50%; Mix 100% | Middle pedal position for manual/automated sweeps |
| Chorus | Rate 0.8 Hz; Depth 35%; Texture 0%; Mix 40%; Spread 75% | Gentle stereo movement |
| Juno Chorus | Mode I; Mix 50%; Brightness 80% | Classic chorus before the more intense combined mode |
| Flanger | Rate 0.25 Hz; Depth 65%; Feedback 45%; Feedback sign 30%; Mix 50% | Slow, recognizable sweep |
| Phaser | Rate 0.35 Hz; Depth 65%; Feedback 15%; Sweep center 20%; Mix 50% | Broad, moderate sweep |
| Auto Tremolo | Rate 4 Hz; Depth 60%; Pulse / feel 0%; Mix 100% | Clear volume movement; no initial envelope acceleration |
| Vibrato | Rate 4.5 Hz; Depth 15%; Texture 0% | Shallow pitch movement; vibrato DSP is fully wet |
| Rotary | Slow 0.8 Hz; Fast 6 Hz; Slow selected; Ramp time 1 s; Drive 1.5 dB; Spread 80%; Mix 100% | Slow rotating speaker, modest grit |
| Uni-Vibe | Rate 1.2 Hz; Depth 65%; Sweep color 35%; Mix 50%; Vibrato mode off | Classic chorus-mode throb |
| Ring Modulator | Carrier 110 Hz; Depth 100%; Tone 6 kHz; Mix 30% | Audible metallic color with a substantial dry anchor |
| Mono Octaver | Dry 100%; Sub 50%; Two-octave sub 0%; Tone 1 kHz | Single-note octave support |
| Stereo Doubler | Detune 6 cents; Delay 15; Spread 100%; Mix 25% | Width without obvious doubling |
| Frequency Shifter | Shift +2 Hz; Feedback 10%; Delay 20; Sideband +100%; Tone 8 kHz; Spread 0%; Mix 30% | Gentle moving interference rather than an initially inaudible zero shift |
| Digital Delay | Time 400; Feedback 30%; Tone 10 kHz; Ducking 0%; Mix 20% | Clean supporting repeats |
| Analog Echo | Time 320; Feedback 35%; Tone 3.5 kHz; Age 35%; Modulation 10%; Mix 22% | Darker, softer repeats |
| Tape Echo | Time 375; Feedback 35%; Tone 4.5 kHz; Saturation 30%; Modulation 20%; Mix 22% | Warm echoes with modest motion |
| Reverse Delay | Time 500; Tone 6 kHz; Mix 30% | A clearly audible reverse gesture |
| Dual Delay | Time 400; Ratio 0.75×; Tap balance 50%; Feedback 30%; Tone 6.5 kHz; Mix 25% | Related 400/300 ms taps |
| Room | Decay 0.8 s; Size 80%; Damping 6.5 kHz; Pre-delay 8; Motion 10%; Mix 12%; Wet low-cut 120 Hz | Small supportive room |
| Dense Plate | Decay 2.4 s; Size 100%; Damping 6.5 kHz; Pre-delay 20; Motion 15%; Mix 18%; Wet low-cut 120 Hz | Smooth sustain with pick separation |
| Cloud | Decay 6 s; Size 150%; Damping 5 kHz; Pre-delay 30; Motion 40%; Mix 30%; Wet low-cut 150 Hz | A distinctly larger ambient texture |
| Hall | Decay 3.2 s; Size 120%; Damping 6 kHz; Pre-delay 25; Motion 20%; Mix 22%; Wet low-cut 120 Hz | Spacious, defined tail |
| Multi-head Tape | Time 500; Feedback 30%; Tone 5 kHz; Saturation 25%; Motion 12%; Mix 25%; Dual heads | Rhythmic taps at 250/500 ms; Time is the last head |
| Diffused Delay | Time 450; Feedback 45%; Tone 6.5 kHz; Diffusion 60%; Motion 15%; Mix 25% | Echo clusters with a blurred stereo tail |
| Airwindows Plate | Input 0 dB; Sustain 50%; Resolution 100%; Pre-delay 15; Low cut 100 Hz; Mix 18% | Native kPlate140 tank with standard outer mix |
| Airwindows Guitar Hall | Sustain 50%; Resolution 100%; Brightness 50%; Early level 50%; Position 50%; Mix 20% | Native kGuitarHall2 guitar-focused space |
| Galactic Space | Replace 50%; Brightness 50%; Detune 35%; Size 60%; Resolution 100%; Mix 25% | Galactic3's slowly evolving ambient pad |
| Cosmic Echo | Virgo; Time 280; Warp 51.6%; Feedback 30.9%; Density 49.2%; High cut 7461 Hz; Motion 29.5%; Mix 18% | Sparse ambient echoes with a restrained wet blend |
| Spring Reverb | Decay 1.8 s; Dwell +3 dB; Damping 5 kHz; Drip 60%; Mix 18% | Recognizable amp-style spring |
| Shimmer | Mix 25%; Decay 5 s; Damping 6 kHz; Pitch +12 st; Amount 25%; Modulation 30%; Freeze off | Octave ambience with a moderate initial blend |
| Freeze / Hold | Hold off; Capture 500; Crossfade 20; Tone 6 kHz; Held level 35% | Ready to capture a played chord without latching silence |
| Stutter | Window 125; Gate length 75%; Edge fade 5; Mix 50% | Eighth-sixteenth-style rhythmic texture at typical tempos; sync optional |
| LoFi Tape Machine | Speed 7.5 ips; Wow 15%; Flutter 10%; Noise 5%; Wear 20%; Mix 100% | Low-fidelity character without prominent initial hiss or wobble |
| Utility | Output 0 dB; L/R balance/Width 0%; polarity normal; Fine delay 0 | Transparent routing |
| Auto Swell | Rise 350; Threshold −48 dB; Release 150 | Clearly audible, playable swell; Rise still reaches 2000 |

NAM captures start at unity input/output, flat EQ with cuts off, and full quality. IR level retains −18 dB as the existing reference; actual IR normalization varies, so use the I/O readouts to set it.

## Acoustic and bass adjustments

- For acoustic guitar, begin with neutral EQ and a low Room/Plate mix; reduce or bypass gates if they trim finger noise or long sustain. Compression threshold depends on pickup/microphone level.
- For bass, lower Cut EQ's high-pass to roughly 30–45 Hz or disable it; 80 Hz is an electric-guitar choice. Reverb low-cuts affect only the wet signal. Prefer low delay/reverb blends to keep low notes distinct.
- Octaver follows single notes and expects a clear fundamental. Bass may need less sub level, or dry-only until the register and tracking are checked.
- Distortion, gain, compression and gate defaults cannot compensate for different interfaces or pickup outputs. Check actual input/output levels and adjust threshold or trim. No new instrument selector or hidden automatic EQ is applied.

## Reverb visualization

Room, Plate, Cloud, Hall, Spring and Shimmer show a parameter sketch. Tank geometry follows Size and the algorithm's spatial scale; Plate is flatter, Spring uses a coil driven by Dwell/Drip, and Shimmer shows pitch/amount direction. Mix changes wet prominence; modulation changes the reflected-point field. A shared logarithmic 0–30 s axis shows pre-delay and the nominal decay to −60 dB. A second, shorter trace indicates treble damping. Shimmer Freeze holds the tail open. The three Airwindows reverbs show live input/output histories instead, since their native controls do not specify a calibrated decay time.

Conventional delay Time now spans 5–2000 ms (synced notes can reach 8 s).
The four updated reverb cores offer 0.15–30 s Decay, 25–200% Size and 0–200 ms
Pre-delay.

This is an explanatory parameter view, not a measured impulse response or an estimate of an actual room's dimensions. Damping's relative trace is illustrative. Live audio metering remains on the grid.
