# LoFi Tape Machine

Worn tape for lo-fi guitar sounds: tone that darkens with tape speed, wow, flutter, hiss, wear and dropouts. Find it under **Creative > LoFi Tape Machine**. It adds no saturation; put [Airwindows Tape](modules/distortion.md#airwindows-tape) before or after it when you want that too.

![LoFi Tape Machine editor](screenshots/modules/lofi-tape-machine.png)

## Starting points

| Sound | Speed (ips) | Wow | Flutter | Noise | Wear | Mix |
| --- | --- | --- | --- | --- | --- | --- |
| Subtle drift | 15 | 10% | 5% | 0% | 10% | 100% |
| Worn cassette | 3.75 | 35% | 25% | 20% | 50% | 100% |
| Damaged tape | 1.875 | 70% | 60% | 35% | 90% | 100% |

Match the level with Output when comparing, and try each setting with a sustained clean chord as well as a distorted riff. The controls and their ranges are in the [module guide](modules/creative.md#lofi-tape-machine).

## What the controls do

- **Speed** is the tape speed in inches per second. Slower tape is darker, and its wow and flutter move more slowly. It does not change the pitch of your guitar.
- **Wow** is slow, smooth pitch drift with a little irregular wander. **Flutter** is a faster pitch vibration.
- **Noise** adds tape hiss, independent in each channel, that continues between notes.
- **Wear** darkens the sound and adds randomly spaced level dropouts.
- **Mix** blends the dry signal with the tape. The tape sound is slightly delayed, so partial blends, or a clean signal in the other lane, add some chorus and comb colouring. Fully wet is the usual starting point.

## Read the transport display

The display shows the actual pitch movement of the tape, not the pitch of your guitar:

- The **green** trace is wow and the **amber** trace is flutter, both over the last 2.56 seconds on a ±60 cent scale.
- Taller traces mean more pitch movement; more cycles mean faster movement. The number on the right is the largest movement in the window.
- Hiss and dropouts do not appear, because they do not move the pitch. The display dims when Mix is fully dry and clears when the module is bypassed.

LoFi Tape Machine is an original character effect, not a model of a specific tape deck, and it adds no latency.
