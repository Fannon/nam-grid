# Delay

Echoes from clean digital to tape and diffused.

Settings' **Trails** toggle lets every bypassed delay and reverb ring out: new playing stops reaching it, and its tail continues over the dry signal. It is off by default; see [rig behaviour in Settings](../ui.md#rig-behaviour).

Settings' **Keep dry level** toggle applies to every delay and reverb in the rig. Off (the default), Mix crossfades from dry to wet. On, the dry signal stays at unity and Mix sets the wet level: 100% gives full dry plus full wet. Whole-signal Output trims still affect both, so high Mix settings can raise the total level. Mix 0% stays dry in both laws.

| Module | In short |
| --- | --- |
| [Digital Delay](#digital-delay) | Clean repeats; Ducking leaves room for notes, then reveals echoes. |
| [Analog Echo](#analog-echo) | Age darkens and saturates repeats; Modulation adds pitch drift. |
| [Tape Echo](#tape-echo) | Saturated repeats; Modulation adds tape-like pitch wobble. |
| [Multi-head Tape](#multi-head-tape) | Eight tape-head patterns; Time sets the last head. |
| [Reverse Delay](#reverse-delay) | Plays each window backwards; Time sets its length. |
| [Dual Delay](#dual-delay) | Two delays; Time ratio sets the second rhythm. |
| [Diffused Delay](#diffused-delay) | Repeats smear into stereo clouds as Diffusion rises. |

## Factory starting points

For ordinary support behind your guitar, the insertion sounds are deliberately
moderate; **Reset** restores them. Use **Presets** for a different playing role,
then adjust Mix in the rig. These recipes assume an insert after the amp.

| Module | Useful starting point | Why choose it |
| --- | --- | --- |
| Digital Delay | Quiet ducked support | A low-mix quarter-note echo that stays behind playing and recovers in the gaps; Crossfade follows tempo jumps without a pitch bend. |
| Analog Echo | Warm lead repeats; Bright dotted echoes | Choose soft, dark lead support or a clearer dotted-eighth rhythm with restrained modulation. |
| Tape Echo | Rock lead echo; Clean dotted tape | A quiet, short lead repeat or a brighter synced rhythm with light saturation and wobble. |
| Multi-head Tape | Sparse stereo heads | Half-time on the left and full-time on the right; fewer taps than the rolling and cascade patterns. |
| Reverse Delay | Short reverse texture | A short capture window and modest mix under your dry attack; Reverse half notes gives longer reversed phrases. |
| Dual Delay | Tight stereo double | Two short taps with no feedback, for light width; Quarter and dotted eighth gives an interlocking rhythm instead. |
| Diffused Delay | Gentle pad echoes | Moderate feedback and diffusion soften sustained notes; Echo into cloud is the longer, more diffuse alternative. |

**Timing:** at 120 BPM, a quarter note is 500 ms and a dotted eighth is 375 ms.
Sparse stereo heads puts the left head at half the quarter-note Time (250 ms)
and the right at full Time (500 ms). Tight stereo double instead uses fixed
80/100 ms left/right taps; it is not tempo-synced. Reverse Delay's Time is a
capture-window length, so do not treat it as the time of a conventional first echo.

**Use the differences:** Analog Echo darkens and saturates successive repeats;
Tape Echo has a different saturation and wobble character. The two dotted recipes
share a rhythm so you can compare their color. Multi-head Tape reads several
positions in one tape pattern; Dual Delay has two independently timed taps.
Diffused Delay smears attacks rather than merely turning down the high cut.

Start with modest Feedback, then raise Mix until the first repeats support the
part. Lower High cut if echoes compete with the pick attack; keep modulation low
for tight rhythm. Digital Delay's Ducking makes space while notes sound. A
longer Duck release keeps echoes down through short gaps; a shorter release lets
them return sooner. Check stereo recipes folded to mono as well as in stereo.

These choices draw on the author's personal ValhallaDelay Analog Past,
ChromeTape for Pads and Space 1 settings and Valhalla DSP's
[Controls](https://valhalladsp.com/2019/04/16/valhalladelay-the-controls/),
[Style](https://valhalladsp.com/2019/04/16/valhalladelay-the-style-control/) and
[Diffusion](https://valhalladsp.com/2019/06/13/valhalladelay-the-diffusion-section/)
guides. They adapt playing roles, not normalized preset values or exact sounds.
For example, NAM Grid's Analog Echo does not have ValhallaDelay's ducking, and
Tape Echo does not expose its diffusion controls. Quiet ducked support uses
Digital Delay, while Gentle pad echoes uses Diffused Delay.

## Digital Delay

![Digital Delay editor](../screenshots/modules/digital-delay.png)

Clean repeats with a high cut. Ducking lowers the repeats while you play and brings them back in the gaps.

| Control | Range | Start | What it does |
| --- | --- | --- | --- |
| **Time** | 5–4000 ms | 400 ms | Time to the first repeat. **Time change** chooses whether jumps bend pitch or crossfade. |
| **Feedback** | 0–82% | 30% | Amount returned for another repeat. Higher settings make more repeats and longer tails. |
| **High cut** | 800–12000 Hz | 10000 Hz | High-cut frequency of the repeats. Lower values darken echoes without dulling the dry guitar. |
| **Ducking** | 0–100% | 0% | Ducks repeats while you play; echoes recover in the gaps. |
| **Duck release** | 10–1000 ms | 150 ms | Time for ducked echoes to recover in gaps; 150 ms retains the original recovery. |
| **Output** | -18 to +6 dB | 0 dB | Final level. Match bypass volume when comparing tones. |
| **Mix** | 0–100% | 20% | Dry/wet balance, or wet level with Settings' **Keep dry level** on. |

In the header:

- **Time change** (Glide / Crossfade; starts on Glide) is saved per slot, without host automation. Glide keeps the tape-style pitch bend when Time moves. Crossfade blends the old and new taps over 30 ms when the target changes by more than 1 ms in one control update, including tempo-sync and Division jumps. Changes of 1 ms or less per update still glide, preserving gentle modulation; faster or deeper modulation can crossfade. Both stereo channels and feedback use the same blend. If another jump arrives during a fade, it waits until that fade ends, then fades to the latest target. New inserts and older saved rigs both start on Glide.
- **Tempo sync** locks Time to your DAW tempo; **Division** then sets it in note values. See [tempo sync](README.md#tempo-sync).
- **Ping-pong** (off / on; starts off) Feed the mono sum into the left tap and alternate repeats between left and right. Dry stereo stays unchanged.

**Display:** The repeats on a dB scale. A thin line inside each stem shows how much survives above 5 kHz; ticks show the ducked level.

## Analog Echo

![Analog Echo editor](../screenshots/modules/analog-echo.png)

Darker, softer repeats with gradually worn-in character. Age adds more darkening and saturation.

| Control | Range | Start | What it does |
| --- | --- | --- | --- |
| **Time** | 5–4000 ms | 320 ms | Time to the first repeat. Changing it while repeats sound bends their pitch, like tape. |
| **Feedback** | 0–82% | 35% | Amount returned for another repeat. Higher settings make more repeats and longer tails. |
| **High cut** | 800–12000 Hz | 3500 Hz | High-cut frequency of the repeats. Lower values darken echoes without dulling the dry guitar. |
| **Age** | 0–100% | 35% | More darkening and soft saturation on each repeat. |
| **Output** | -18 to +6 dB | 0 dB | Final level. Match bypass volume when comparing tones. |
| **Modulation** | 0–100% | 10% | Pitch movement of repeats. Zero is stable; higher values add chorus-like wobble. |
| **Mix** | 0–100% | 22% | Dry/wet balance, or wet level with Settings' **Keep dry level** on. |

In the header:

- **Tempo sync** locks Time to your DAW tempo; **Division** then sets it in note values. See [tempo sync](README.md#tempo-sync).
- **Ping-pong** (off / on; starts off) Feed the mono sum into the left tap and alternate repeats between left and right. Dry stereo stays unchanged.

**Display:** The repeats on a dB scale; the thin line inside each stem shows the content above 5 kHz.

## Tape Echo

![Tape Echo editor](../screenshots/modules/tape-echo.png)

Filtered, saturated repeats with tape-style wow and flutter from Modulation.

| Control | Range | Start | What it does |
| --- | --- | --- | --- |
| **Time** | 5–4000 ms | 375 ms | Time to the first repeat. Changing it while repeats sound bends their pitch, like tape. |
| **Feedback** | 0–82% | 35% | Amount returned for another repeat. Higher settings make more repeats and longer tails. |
| **High cut** | 800–12000 Hz | 4500 Hz | High-cut frequency of the repeats. Lower values darken echoes without dulling the dry guitar. |
| **Saturation** | 0–100% | 30% | Soft saturation on repeats. Modulation controls the tape wobble separately. |
| **Output** | -18 to +6 dB | 0 dB | Final level. Match bypass volume when comparing tones. |
| **Modulation** | 0–100% | 20% | Pitch movement of repeats. Zero is stable; higher values add chorus-like wobble. |
| **Mix** | 0–100% | 22% | Dry/wet balance, or wet level with Settings' **Keep dry level** on. |

In the header:

- **Tempo sync** locks Time to your DAW tempo; **Division** then sets it in note values. See [tempo sync](README.md#tempo-sync).
- **Ping-pong** (off / on; starts off) Feed the mono sum into the left tap and alternate repeats between left and right. Dry stereo stays unchanged.

**Display:** The repeats on a dB scale; the thin line inside each stem shows the content above 5 kHz.

## Multi-head Tape

![Multi-head Tape editor](../screenshots/modules/multi-head-tape.png)

Up to four playback heads in eight patterns, including stereo modes; only the last head feeds back. Time sets the last head.

| Control | Range | Start | What it does |
| --- | --- | --- | --- |
| **Time** | 5–4000 ms | 500 ms | Time of the last playback head. Other selected heads occur at one quarter, one half and three quarters of this interval. |
| **Feedback** | 0–82% | 30% | Amount returned for another repeat. Higher settings make more repeats and longer tails. |
| **High cut** | 800–12000 Hz | 5000 Hz | High-cut frequency of the repeats. Lower values darken echoes without dulling the dry guitar. |
| **Saturation** | 0–100% | 25% | Soft saturation on repeats. Modulation controls the tape wobble separately. |
| **Output** | -18 to +6 dB | 0 dB | Final level. Match bypass volume when comparing tones. |
| **Modulation** | 0–100% | 12% | Pitch movement of repeats. Zero is stable; higher values add chorus-like wobble. |
| **Mix** | 0–100% | 25% | Dry/wet balance, or wet level with Settings' **Keep dry level** on. |

In the header:

- **Tempo sync** locks Time to your DAW tempo; **Division** then sets it in note values. See [tempo sync](README.md#tempo-sync).
- **Heads** (Single, Dual, Syncopated, All heads, Early pair, Late pair, Stereo dual, Stereo cascade; starts at Dual) Which playback heads sound. Single is the last head alone, at the full Time; only the last head feeds back.

**Display:** Each head's repeats on a dB scale.

## Reverse Delay

![Reverse Delay editor](../screenshots/modules/reverse-delay.png)

Records a window and plays it backwards one window later.

| Control | Range | Start | What it does |
| --- | --- | --- | --- |
| **Time** | 5–4000 ms | 500 ms | Length of each reversed capture window; not a conventional first-echo delay. |
| **Feedback** | 0–75% | 0% | Returns reversed playback into the next capture, creating repeating reversed phrases. |
| **High cut** | 800–12000 Hz | 6000 Hz | High-cut frequency of the repeats. Lower values darken echoes without dulling the dry guitar. |
| **Output** | -18 to +6 dB | 0 dB | Final level. Match bypass volume when comparing tones. |
| **Mix** | 0–100% | 30% | Dry/wet balance, or wet level with Settings' **Keep dry level** on. |

In the header:

- **Tempo sync** locks Time to your DAW tempo; **Division** then sets it in note values. See [tempo sync](README.md#tempo-sync).

**Display:** The captured windows and when each plays reversed.

## Dual Delay

![Dual Delay editor](../screenshots/modules/dual-delay.png)

Two delay taps; the right tap's time is Time multiplied by Time ratio. Ping-pong feeds them crosswise.

| Control | Range | Start | What it does |
| --- | --- | --- | --- |
| **Time** | 5–4000 ms | 400 ms | Time of the left tap. The right tap is this value multiplied by Time ratio. |
| **Feedback** | 0–82% | 30% | Amount returned for another repeat. Higher settings make more repeats and longer tails. |
| **High cut** | 800–12000 Hz | 6500 Hz | High-cut frequency of the repeats. Lower values darken echoes without dulling the dry guitar. |
| **Time ratio** | 0.25–2× | 0.75× | Right delay time divided by left time. 0.75 gives a dotted-eighth partner to a quarter-note left delay. |
| **Output** | -18 to +6 dB | 0 dB | Final level. Match bypass volume when comparing tones. |
| **Tap balance** | 0–100% | 50% | Balance between the two wet taps; center keeps both at full level. Dry stereo stays unchanged. |
| **Mix** | 0–100% | 25% | Dry/wet balance, or wet level with Settings' **Keep dry level** on. |

In the header:

- **Tempo sync** locks Time to your DAW tempo; **Division** then sets it in note values. See [tempo sync](README.md#tempo-sync).
- **Ping-pong** (off / on; starts off) Cross-feedback mode: feed the mono sum into the left tap, then alternate through left and right delay times.

**Display:** Left and right repeats on a dB scale.

**Tips:**

- Time ratio 0.75 with a quarter-note Time gives the classic dotted-eighth pair.

## Diffused Delay

![Diffused Delay editor](../screenshots/modules/diffused-delay.png)

Repeats smear into stereo reflection clusters as Diffusion rises; zero is a plain echo.

| Control | Range | Start | What it does |
| --- | --- | --- | --- |
| **Time** | 5–4000 ms | 450 ms | Time to the first repeat. Changing it while repeats sound bends their pitch, like tape. |
| **Feedback** | 0–82% | 45% | Amount returned for another repeat. Higher settings make more repeats and longer tails. |
| **High cut** | 800–12000 Hz | 6500 Hz | High-cut frequency of the repeats. Lower values darken echoes without dulling the dry guitar. |
| **Diffusion** | 0–100% | 60% | Smear each repeat through rotating allpass stages. Zero is a clear echo; higher values spread attacks into stereo clouds. |
| **Output** | -18 to +6 dB | 0 dB | Final level. Match bypass volume when comparing tones. |
| **Modulation** | 0–100% | 15% | Pitch movement of repeats. Zero is stable; higher values add chorus-like wobble. |
| **Mix** | 0–100% | 25% | Dry/wet balance, or wet level with Settings' **Keep dry level** on. |

In the header:

- **Tempo sync** locks Time to your DAW tempo; **Division** then sets it in note values. See [tempo sync](README.md#tempo-sync).

**Display:** The repeats with their diffusion clusters.
