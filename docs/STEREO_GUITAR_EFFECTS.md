# Stereo guitar effects

This increment adds two module types and stereo behavior to existing effects.
The grid and eight-position editor remain fixed. No new dependency or non-MIT
implementation is included. All added controls are automated and saved per slot.

## Delay, chorus and reverb

- **Ping-pong** is a switch beside the delay's Tempo sync switch, available on
  Digital, Analog and Tape. The wet send folds to mono, starts on the left and
  crosses to the other side on each repeat. The stereo dry signal stays in place.
  Turning the switch off restores independent L/R echoes. Both delay reads occur
  before either feedback write, so there is no block-sized cross-feedback delay.
  The feedback matrix interpolates smoothly and has maximum row sum one; existing
  82% feedback and filtered/saturating loop bounds remain in force.
- **Chorus Spread** occupies bottom position six. 0% uses coincident LFOs, 100%
  offsets the right voice by half a cycle. The default is 75%. Spread changes are
  smoothed, and the dry path does not move. Tempo sync still controls the common rate.
- **Stereo wet** is a header switch for Room, Plate, Cloud and Spring, default on.
  Existing reverbs now excite two differently sized networks from a contractive
  stereo input matrix. Either input excites both outputs; duplicated mono input
  produces decorrelated tails. The switch scales only the wet side component;
  off folds the wet signal to mono without changing dry placement. The existing
  seven knobs remain visible. This changes existing reverb voicing in this
  unreleased prototype; schema migration is not a promise of identical audio.

Place spatial effects after the last mono NAM capture. A subsequent mono NAM
averages L/R and necessarily collapses their stereo result. A mono host output
also folds L/R; the plugin does not compensate away intentional echo/reverb time.

## Spring Reverb

Six knobs: **Decay** (.3–4 s), **Dwell** (0–12 dB), **Damping** (800–12000 Hz),
**Drip**, **Mix**, **Output** (±12 dB), then Enabled. One bottom position is unused.

Three unequal tanks have 47/62/79 ms base travel times and sixteen stretched
allpass sections each. Drip changes their dispersion coefficient, producing a
different chirped impulse train rather than generic modulation. The wet input
has a 130 Hz cut and a soft driven excitation; Dwell controls that drive. Each
feedback loop includes high-frequency damping and bounded exponential decay.
Different tank contributions feed the two outputs. This is a small spring-inspired
algorithm, not a measured emulation of a named tank. Decay is nominal, not a
guarantee of identical RT60 at every frequency/Drip setting. Strong Dwell can alias;
no mandatory oversampling is introduced.

Mathematical inspiration: dispersive allpasses in waveguide structures, discussed
in [Gamper, Parker and Välimäki's spring-reverb research](https://research.aalto.fi/en/publications/automated-calibration-of-a-parametric-spring-reverb-model/)
and the [DAFx spring simulation overview](https://dafx.de/paper-archive/details/omUNRAXkKDeEOxMkIyPwQg).
The implementation is original and does not reproduce their calibration procedure
or copy source code. Buffer capacities and allpass stretch times scale with host rate.

## Wah

Five knobs: **Position**, **Resonance**, **Range**, **Mix**, **Output**, plus Enabled.
Map the Position host parameter to an expression controller in Bitwig, or draw its
automation. There is no separate MIDI-learn layer or automatic pedal bypass.

Position follows a logarithmic heel-to-toe sweep. Range shifts the heel between
250–500 Hz and the toe between 1.7–3.4 kHz; Resonance covers Q 1–6. A trapezoidal
state-variable band-pass mixes with a little broadband body for a voiced wah.
Control smoothing is sample based. It is an original guitar voicing, not a circuit
clone or a renamed envelope low-pass. Mathematical filter reference:
[Simper, Linear Trapezoidal Integrated SVF](https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf).
Only filter equations inform the implementation; no third-party code is imported.

## Utility Stereo

The former Width knob becomes **Stereo**, −100…+100%, default **0**:

- −100: remove the side component, yielding `(L+R)/2` on both channels.
- 0: unity, preserving the original channels.
- +100: double the side component while retaining the same mid signal.

It does not create stereo from identical L/R and does not add delay. Widening
changes the stereo balance and may raise peaks, while the mathematical mono fold
remains unchanged. Changes ramp; the Level, Balance and polarity controls follow it.
Schema 8 Width values migrate by subtracting 100, including later slots. Schema 9
has 1262 parameters: 284 inherited + 16 × 61 family controls + two tuner preferences.

## Verification

Automated DSP checks cover 44.1/48/88.2/96/192 kHz, variable blocks including one
frame, stereo reference renders, allocation/Eigen guards, parameter extremes,
bypass/reset, repeat-side ordering, generated stereo energy, centered Utility
behavior, Wah frequency response and distinct/decaying spring impulses. CLAP checks
cover new module recall, visible controls, automation, schema-8 migration and existing
tuner/tempo/lifecycle behavior. Windows CLAP/VST3 builds are checked; no Bitwig,
macOS, VST3-host listening or subjective sound-quality approval is claimed.

Manual audition:

1. Try each spatial effect after amp/cab at mono guitar input. Hear separate
   repeat positions, a spreading chorus and reverb tails without moving the dry note.
2. Test Ping-pong with sync at straight/dotted/triplet divisions. Automate the switch
   and tempo while playing; confirm there is no unstable feedback or callback pause.
3. Play muted transients through Spring at low/high Drip. Match level before judging
   Dwell; test long decay, quiet notes and brighter damping on clean/acoustic guitar.
4. Map Wah Position to a controller, sweep slowly and quickly, and check resonance
   at both heel/toe. Mix 0 should be dry; compare enabled/bypassed loudness.
5. Exercise Utility at −100/0/+100 with a stereo source, then a mono source. Save,
   reopen, switch host rate/buffer size and verify both stereo and mono host outputs.

Current offline CPU checkpoint (Windows Release, 128-frame blocks, fastest of
three measured 128,000-sample renders after warmup, percent of one core including
the full grid, default controls):

| Case | 48 kHz | 96 kHz |
| --- | ---: | ---: |
| Empty grid | 0.29% | 0.58% |
| Digital Delay | 0.54% | 1.07% |
| Chorus / default Spread | 0.67% | 1.36% |
| Room / stereo wet | 1.80% | 3.58% |
| Spring Reverb | 1.57% | 3.10% |
| Wah | 0.67% | 1.33% |

All report zero host compensation latency. This is neither a Bitwig meter nor a
worst-case deadline measurement. Raw output is in ignored
`build-grid/spatial-guitar-cpu.csv`. Reproduce with `nam-clap-bench <plugin.clap>
fx-26 1 - -` (Spring) or `fx-27` (Wah), `fx-18` (Chorus), `fx-15` (Room).
