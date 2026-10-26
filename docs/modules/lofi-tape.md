# LoFi Tape Machine

Worn tape for lo-fi guitar sounds: tone that darkens with tape speed, wow, flutter, hiss, wear and dropouts. Find it under **Creative > LoFi Tape Machine**. It adds no saturation; put [Airwindows Tape](distortion.md#airwindows-tape) before or after it when you want that too.

![LoFi Tape Machine editor](../screenshots/modules/lofi-tape-machine.png)

## Starting points

| Sound | Speed (ips) | Wow | Flutter | Noise | Wear | Mix |
| --- | --- | --- | --- | --- | --- | --- |
| Subtle drift | 15 | 10% | 5% | 0% | 10% | 100% |
| Worn cassette | 3.75 | 35% | 25% | 20% | 50% | 100% |
| Damaged tape | 1.875 | 70% | 60% | 35% | 90% | 100% |

Match the level with Output when comparing, and try each setting with a sustained clean chord as well as a distorted riff. The controls and their ranges are in the [module guide](creative.md#lofi-tape-machine).

## Good to know

- Slower **Speed** makes the tape darker and its wow and flutter slower, but it does not change the pitch of your guitar.
- The tape sound is slightly delayed. Partial **Mix** settings, or a clean signal in the other lane, therefore add some chorus and comb colouring; fully wet is the usual starting point.
- **Noise** hiss continues between notes. For silent pauses, lower Noise or place a gate after this module.

## Read the transport display

The display shows the actual pitch movement of the tape, not the pitch of your guitar:

- The **green** trace is wow and the **amber** trace is flutter, both over the last 2.56 seconds on a ±60 cent scale.
- Taller traces mean more pitch movement; more cycles mean faster movement. The number on the right is the largest movement in the window.
- Hiss and dropouts do not appear, because they do not move the pitch. The display dims when Mix is fully dry and clears when the module is bypassed.

LoFi Tape Machine is an original character effect, not a model of a specific tape deck, and it adds no latency.

## Saturation

Drive adds 4x oversampled saturation before the transport, using the Spiral shaper from Airwindows Tape. It leaves head bump and the full Tape processor's softening to the separate Airwindows Tape module. Drive at zero bypasses saturation and retains the original transport sound. Wear continues to control bandwidth and dropouts independently; Mix leaves the dry path unchanged.
