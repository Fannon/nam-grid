# Rig presets

A rig preset saves the complete 16-slot setup: modules, capture/IR references, controls, routing, modulation, macros, and performance scenes.

## Load and save

Click the preset name in the header to open the [preset browser](browser.md). Search your preset folders, then choose **Load**, press Enter, or double-click. The arrows beside the preset name step through the current folder.

- **New** starts an empty rig after confirmation.
- **Save** opens **Save as...**, so an ordinary click never overwrites a file.
- **Shift-click** or **Alt-click Save** overwrites the loaded preset. Right-clicking also offers **Save over** and **Save as...**.
- Edits after loading or saving are not written automatically.

**Empty** and **Clean Default** are included in the default `Documents/NAM Grid/Presets` folder. Add other preset folders in **Library & settings**.

Presets use `.ngrid` files. They reference NAM and IR files by path rather than embedding them, so keep those files in place when moving or sharing a rig. If a referenced file is missing, relink it through the module browser; NAM Grid will not silently substitute another file.

## Switching while you play

Choose **Library & settings > Preset switch**:

- **Short fade** (default): a brief fade out and in, about 25 ms total. Best when CPU headroom is limited.
- **Crossfade:** a 40 ms blend without the gap. Both rigs run briefly, so allow extra CPU headroom.

Old delay and reverb tails end with the transition. Large rigs can take a moment to prepare. A rig with different latency may also cause your DAW to restart audio processing briefly.

## Presets, projects, and scenes

- A **rig preset** can replace modules and captures as well as controls.
- A **DAW project** recalls the current rig and host automation as part of the project.
- A **performance scene** changes controls within the current modules; it cannot load a different capture or module.

Your library folders are personal settings and are not replaced by a rig or project. Tuner activation, reference, and mute are also left unchanged by rigs and scenes. Frozen or held audio is not saved.

## Gain placement

- **IN** sets the level before the grid, tuner, and envelope follower.
- **A/B** balances the parallel lanes: 0% A, 50% equal, 100% B.
- **OUT** sets the final level sent to your DAW.

All three can be automated and are saved with the rig. See [Grid & routing](grid.md) for signal flow and [Performance controls](performance.md) for scenes.
