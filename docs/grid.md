# Grid & routing

NAM Grid gives you 16 slots in four rows: **Pre**, parallel lanes **A** and **B**, and **Post**. Sound flows left to right; empty slots simply pass it through.

```text
                                  +-- A1 -> A2 -> A3 -> A4 --+
IN -> Pre1 -> Pre2 -> Pre3 -> Pre4 -+                         +- Post1 -> Post2 -> Post3 -> Post4 -> OUT
                                  +-- B1 -> B2 -> B3 -> B4 --+
```

Any slot can hold a NAM capture, cabinet IR, or built-in effect. Click a slot's name or **+ Add module** to choose one.

## Build your first rig

1. **Start with an amp capture.** Load a `.nam` file in Pre or lane A. The bundled **RiffSmith Clean** already includes an amp and cabinet.
2. **Add a cabinet if your capture needs one.** Place a Cabinet IR after an amp-only capture. You do not need another cabinet after a full amp-and-cab capture.
3. **Shape the sound.** Try a gate, boost, or drive before the capture; EQ, delay, or reverb after it.
4. **Set a healthy level.** Use **IN** for the level reaching the rig, each module's Output for its own gain, and **OUT** for the final level. Match bypass volume before judging a tone.

The included **Clean Default** preset is a ready-made example. Save your own version through the [preset browser](presets.md).

## Parallel lanes and blend

Both lanes receive the same sound from Pre4. **Blend A / B** sets their balance: 0% is A, 50% is both equally, and 100% is B. The blend moves smoothly and can be automated.

- Turn Blend fully toward a modeled lane to hear it alone; otherwise the other lane's sound remains in the mix.
- Try different amps or cabs in each lane, or keep one lane dry and add effects to the other.
- Parallel captures can reinforce or cancel frequencies. If the combined tone sounds thin, adjust their levels or timing and compare in mono.
- Captures use CPU even when Blend favors the other lane. Bypass unused models to save CPU.

**Routing** opens per-lane level, mute/solo, timing, and frequency-split controls. See [Performance controls](performance.md#routing).

## Move and relink modules

Drag cards to rearrange them, or right-click for Copy, Move, Swap, and Clear. Controls and bypass travel with the module; host automation stays with the slot. See [Editor](ui.md#build-and-arrange-a-rig) for drag modifiers.

If a capture or IR moves, its slot keeps the reference and passes audio until you relink it. NAM Grid never silently substitutes a different file. Open the module browser to locate the file again.
