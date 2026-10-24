# Grid & routing

NAM Grid has 16 slots in four rows: four **Pre** slots, two parallel four-slot
lanes (**A** and **B**), and four **Post** slots.

```text
                                  +-- A1 -> A2 -> A3 -> A4 --+
IN -> Pre1 -> Pre2 -> Pre3 -> Pre4 -+                         +- Post1 -> Post2 -> Post3 -> Post4 -> OUT
                                  +-- B1 -> B2 -> B3 -> B4 --+
                                             blend
```

Any slot takes any module: NAM captures, cabinet IRs or built-in effects
(dynamics, EQ, cabinet IR, boost, filter, swell, tape, modulation, delay,
spring reverb, Wah). Empty slots pass audio through. There are no
cross-connections, sends or a routing graph — sound flows left to right,
through Pre, through both lanes at once, then through Post.

## The lanes and the blend

Both lanes receive the same sound from Pre4. **Blend A / B** sets the lane
balance: 0% is only A, 50% is both equally, 100% is only B. The blend is a
smooth crossfade, so automating it stays clean.

- Two identical or empty lanes stay at unity instead of getting louder.
- To hear only one modeled lane, turn the blend fully toward it — otherwise
  the other lane's dry sound mixes in.
- Both lanes always stay time-aligned, so switching and blend moves don't
  cause timing jumps. Different amp or cab captures can still cancel or
  comb-filter against each other, so audition parallel combinations by ear.
- Captures use CPU even when you only hear the other lane. Bypass unused
  models to save CPU.

## Working with slots

Each slot has the same controls. NAM slots use the capture's raw output level
with independent input and trim gains. Mono captures listen to the summed
stereo input and play back in stereo; use Utility after the last one to adjust
pan or width. See [the editor](ui.md) for selecting, browsing and
rearranging cards, [presets](presets.md) for saving rigs, and the
[module guide](modules/README.md) for every module's controls.

If a slot's file moved or went missing, the slot keeps its path and passes
audio until you relink it — nothing is silently swapped.
