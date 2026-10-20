# First-release module coverage

Reviewed 2026-10-20. The goal is a complete small guitar chain, not parity with
commercial processors' model counts.

## Sources and confidence

- [IK's official TONEX Board effects inventory](https://cdn.shopify.com/s/files/1/1052/1303/4828/files/TONEX_Board_Effects_List.pdf?v=1791982179),
  version 1.0.0 dated 2026-09-02: 76 entries and their origins/descriptions.
  [IK's product page](https://www.ikmultimedia.com/products/tonexboard/) links this PDF.
  It does **not** establish every TONEX 2.1 parameter, range or pedal identity.
- User-supplied **Ampero II Stomp Effect List, firmware 2.5.0** (91 pages),
  `Ampero II Stomp_Effect List_EN_Firmware V2.5.0.1785406068293.pdf`.
- User-supplied **Pulze Effects List, firmware 1.4.0** (46 pages),
  `Pulze_Effects List_EN_Firmware V1.4.0.1762764415226.pdf`.
  These are the main Hotone references, also listed on its
  [official support index](https://cdn.hotoneaudio.com/support/1).
  They describe controls and some ranges. The Pulze-family list is used as
  inspiration for the user's Pulze Jr; model-specific availability is not assumed.
  The Stage 1.7.0 manual was also inspected, but is supplementary to these supplied files.

The supplied AmpliTube/retailer cross-reference is useful inspiration, not a
verified TONEX 2.1 control specification. No commercial implementation or manual
artwork was copied. New DSP is original MIT code; the existing Airwindows Tape
adaptation and permissive notices remain unchanged.

## Amp selection

Hotone covers US clean/tweed, British clean/chime and crunch, boutique clean/drive,
modern high gain and bass, plus separate preamp/power-amp/cab sections. Examples
in the supplied manuals include Fender, VOX, Marshall, Matchless/Dumble, Mesa/Soldano/5150
and Ampeg families.

For nam-grid, keep **one NAM module**, with captures supplying those sounds. A
useful audition matrix is one clean, chime, crunch, boutique edge-of-breakup,
high-gain and optional bass capture. Test DI captures with an IR, and amp+cab
captures without an additional cabinet. This is a coverage recommendation, not
a claim that these captures are bundled or that a capture exposes the original
amplifier's controls. Bass/mid/treble and cuts remain post-capture processing.

The Pulze list is a useful simplicity benchmark: compact effect selections and
musical controls, with independent delay modulation and wet tone shaping. The
Stomp list exposes greater depth, including compressor blend, spring variants,
stereo effects and trails. We borrow the useful decisions, not the inventories.

## Focused coverage decisions

| Area | Current coverage | First-release assessment |
| --- | --- | --- |
| Amp/pedal/cab tone | A2 NAM, IR loader, clean Boost and Tube Boost | Enough categories; audition good captures rather than add a drive catalog |
| Dynamics | Two gates, two compressors with attack and blend | Enough; blend provides parallel compression without a new type |
| EQ/filter | Guitar EQ, cuts, one-band parametric, envelope filter | Useful core; add an expression/automatable wah next |
| Level/movement | Master, trims, Boost, Auto Swell | Add a dedicated Volume module for pedal automation |
| Modulation | Chorus, flanger, phaser, tremolo, vibrato | Enough initially; rotary/Uni-Vibe character is a later improvement |
| Delay | Digital/ducking, analog and tape, independent motion/output | Add host tempo sync; slapback already fits existing time ranges |
| Reverb | Room, dense plate, cloud; modulation and wet low-cut | Spring is the clearest missing guitar voice; do not fake it by renaming Room |
| Parallel-chain utility | Blend and automatic model conversion-delay alignment | Add polarity and deliberate fine delay; cabinet/model phase can still cancel |
| Stereo | Mono grid duplicated at output | Important next architecture step for wide ambience/ping-pong, not another mono preset |
| Pitch/shimmer | Not implemented | Defer; evaluate sound quality, CPU and pitch-window delay explicitly |
| Tuning | Host/external tuner only | A compact tuner/mute would complete everyday use |

These priorities are our design judgment, not a ranking published by IK/Hotone.

## This increment

Eight control positions form four aligned pairs beneath the four grid columns.
Positions 1–7 accept controls; position 8 is always Enabled. Seven is a ceiling,
not a target. Module types remain in the menu and unused cells remain empty.

- NAM: Input, Bass, Mid, Treble, High pass, Low pass, Output trim, Enabled.
- Compressors: retain the two ratio/knee characters; add Attack and wet/dry Mix.
  Smooth/Punch retain independent 8/25 ms attack defaults.
- Delays: add independent modulation amount to Analog/Tape, and output trim to all
  three. Digital remains a clean/ducking delay. Character no longer ties Tape's
  saturation amount to its wobble amount.
- Reverbs: add modulation amount and input low-cut affecting only the wet path.
- Tube Boost: original soft, bias-adjustable saturation with Tilt/Low cut before
  saturation, dry/wet Mix and separate Output. Bias introduces asymmetric color;
  the 5 Hz DC blocker removes resulting DC. This is tube-inspired voicing, not a
  modeled Tube Driver or a claim of circuit accuracy. Strong drive can alias;
  no mandatory oversampling or lookahead is introduced.

Spring, wah/volume, polarity/fine delay, tuner, stereo and sync are identified
follow-ups, not implemented features. Stereo requires preserving channels around
mono NAM stages; it should receive its own routing/lifecycle review.
