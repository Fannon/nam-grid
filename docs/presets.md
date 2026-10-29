# Rig presets

A rig preset saves the complete 16-slot setup: modules, capture/IR references, controls, routing, modulation, macro values and names, and performance scenes.

## Load and save

Click the preset name in the header to open the [preset browser](browser.md). Search your preset folders, then choose **Load**, press Enter, or double-click. The arrows beside the preset name step through the current folder.

- **New** (page with a plus) starts an empty rig after confirmation.
- **Save** (disk icon) opens **Save as...**, so an ordinary click never overwrites a file.
- **Shift-click** or **Alt-click Save** overwrites the loaded preset. Right-clicking also offers **Save over** and **Save as...**.
- Edits after loading or saving are not written automatically.

**Empty** and **Clean Default** are included in the default `Documents/NAM Grid/Presets` folder. Add other preset folders in **Settings**.

Fresh installs also include a **Drive Examples** subfolder with a starting rig for each
Pedal Drive style, Flat/Tight/Warm clean boosts and two split-band bass rigs.
Existing users can choose **Settings > Add drive presets...**, then select a
folder. NAM Grid creates the subfolder, preserves existing files and indexes the chosen
folder. These examples contain effects only; add your amp and cabinet captures where
needed. Loading one replaces the current rig. See [Pedal Drive](modules/pedal-drive.md).

Presets use `.ngrid` files. They reference NAM and IR files by path rather than embedding them, so keep those files in place when moving or sharing a rig. If a referenced file is missing, relink it through the module browser; NAM Grid will not silently substitute another file.

## Switching while you play

Choose **Settings > Preset switch**:

- **Short fade** (default): a brief fade out and in, about 25 ms total. Best when CPU headroom is limited.
- **Crossfade:** a 40 ms blend without the gap. Both rigs run briefly, so allow extra CPU headroom.

Old delay and reverb tails end with the transition. Large rigs can take a moment to prepare. A rig with different latency may also cause your DAW to restart audio processing briefly.

## Presets, projects, and scenes

- A **module preset** (the **Presets** menu in each module editor) changes one module's controls. Factory module presets are built in; your own are saved in `Module Presets` inside your preset folder. See [Shape a module](ui.md#shape-a-module).
- A **rig preset** can replace modules and captures as well as controls.
- A **DAW project** recalls the current rig and host automation as part of the project.
- A **performance scene** changes controls within the current modules; it cannot load a different capture or module.

Your library folders are personal settings and are not replaced by a rig or project. **IN**, **A/B** and **OUT** are part of the rig. Tuner activation, reference, and mute are also left unchanged by rigs and scenes. Frozen or held audio is not saved.

## Module presets

Every built-in module except NAM and Cabinet IR has a **Presets** menu. It works
with user presets even when the module has no factory presets. NAM captures and
IR files are chosen in the browser and saved with the complete rig instead.

Choose a factory starting point, adjust it, then use **Save current as...** to keep
your own copy. User presets are JSON files inside `Module Presets/<module>` in
your first preset folder. Presets from all configured preset folders appear in
the menu; **Show presets folder** opens the saving location.

Choosing a preset changes that module's controls, including its mode, tempo
options and any shared EQ knobs it uses. Controls omitted by a recipe return to
that module's insertion defaults. Pedal Comp keeps Pickup unless the recipe
specifies an instrument. Bypass, unrelated controls, other slots, routing,
modulation assignments and scenes stay as set. Presets do not create modulation
assignments: for example, the Wah recipes are parked tones until you assign a
pedal or modulation source to Position.

### Saving a sound for later

Factory recipes can be revised, renamed, reordered or removed in updates.
Selecting one copies its settings into ordinary parameters. A project or `.ngrid`
rig stores the resulting values; it never looks up the factory recipe again.
Changing the factory catalog therefore cannot change a saved rig or project.
The button's matching name is only a description of current values, so it can
change or disappear when the catalog changes without affecting the sound.

**Save current as...** also saves the current module's control values rather than
a factory name or index. Use it to retain a factory starting point independently
of future catalog changes. Live held audio and a Smart Gate's learned noise
profile are not part of a module preset; complete rigs/projects retain the noise
profile. DSP improvements follow the separate [compatibility contract](development.md#parameter-and-project-compatibility).

### Choosing useful starting points

Factory presets demonstrate distinct uses instead of filling the menu with
small variations. They use the module's own ranges and defaults; similarly named
controls in other plugins do not imply identical processing or knob values.
Most modules offer a restrained option and a more pronounced one. Pedal Drive
and Cosmic Echo also demonstrate each of their styles.

| Modules | Factory starting points emphasize |
| --- | --- |
| Pedal Comp, Smooth Compressor, Punch Compressor, Butter Comp, Peak Limiter | Picking, leveling, sustain, parallel compression and peak control. Threshold and output still depend on input level. |
| Guitar EQ, Cut EQ, Parametric EQ, Console EQ | Broad musical contours, guitar/bass cleanup and a low shelf. Exact problem-frequency notches need tuning to the recording. |
| Boost, Tube Boost, Distortion, Pedal Drive | Flat, tight or warm pushes; breakup, rhythm and fuzz; preserved clean lows for bass. Output is a starting trim, not automatic loudness matching. |
| Airwindows Tape, LoFi Tape Machine, Outboard Channel, Console Colour | Light rounding/glue and stronger parallel colour; clean versus worn tape motion. LoFi recipes start with Noise off. |
| Digital Delay, Analog Echo, Tape Echo | Slap, lead repeats, dotted notes, dark/wavering repeats and ducking where available. |
| Multi-head Tape, Dual Delay, Reverse Delay, Diffused Delay | Head patterns and stereo rhythm, quarter/dotted-eighth pairs, reversed phrases and echo clouds. |
| Room, Dense Plate, Hall, Cloud, Spring Reverb, Shimmer | Small ambience, lead space, long beds, surf splash and rising/falling pitch tails. |
| Airwindows Plate, Airwindows Guitar Hall, Galactic Space, Cosmic Echo | Compact versus broad spaces, early reflections, slowly evolving washes and contrasting echo/reverb networks. |
| Chorus, Juno Chorus, Flanger, Phaser | Subtle width, deep motion, centered vintage chorus, parked comb and different phase stages. |
| Auto Tremolo, Vibrato, Rotary, Uni-Vibe | Gentle pulses, synchronized chops, slow/fast cabinets, delayed pitch motion and chorus/vibrato contrast. |
| Wah, Filter, Auto Swell | Parked vowel tones, touch-sensitive filtering and soft attack/slow chord bloom. Swell threshold may need adjustment for your input. |
| Mono Octaver, Stereo Doubler, Ring Modulator, Frequency Shifter, Bitcrusher | Low-octave support, tight/loose doubles, metallic tones, moving sidebands and light/heavy digital grain. Mono Octaver expects single notes. |
| Freeze / Hold, Stutter | Soft held layers and musical slices. Factory recipes leave Hold off; trigger it while playing. |
| Noise Gate, Noise Gate (Smart), Digital Black Gate | User presets only. Set threshold to your noise floor and quiet notes; learn Smart Gate's profile from your own input. |
| Utility | User presets only. Balance, polarity, channel choice and alignment depend on the rig. |

Synced recipes select both Tempo sync and Division. Unsynced recipes restore
free time/rate; switching recipes cannot accidentally inherit sync, feedback,
mode, hold or output edits from the previous sound. Mix assumes ordinary insert
use. Set Mix for your routing, especially with **Keep dry level** or on a send.

### References behind the recipes

These references inform the design choices; the recipes are original NAM Grid
settings, not imported factory banks or claims of matching the referenced gear.

- [Klanghelm DC8C manual](https://klanghelm.com/docs/DC8C3-manual.pdf): contrasting leveling/punch/transient uses and factory presets that leave calibration and global settings alone.
- [Klanghelm MJUC guide](https://klanghelm.com/docs/MJUC-manual.pdf) and [SDRR 2 manual](https://klanghelm.com/docs/SDRR2-manual.pdf): gentle versus forward dynamics, saturation character, parallel blending and level comparison.
- [Cableguys ShaperBox 3 manual](https://downloads.cableguys.com/Cableguys-ShaperBox-3-Manual.pdf): purpose-based quick presets and distinct rhythmic treatments; module recipes stay scoped to the module.
- Open-source [ChowTape manual](https://chowdsp.com/manuals/ChowTapeManual.pdf): tape speed, wear, wow/flutter and separate factory/user presets. These are independent dimensions, rather than one universal vintage setting.
- Open-source Airwindows [kPlate140](https://www.airwindows.com/kplate140/), [kGuitarHall2](https://www.airwindows.com/kguitarhall2/) and [Galactic3](https://www.airwindows.com/galactic3/): use the actual algorithms' controls; Resolution changes apparent size as well as tone.
- [Valhalla Supermassive](https://valhalladsp.com/shop/reverb/valhalla-supermassive/): low Warp for distinct echoes, more Warp/Density for smooth spaces, and different attack/decay behaviors by mode. Cosmic Echo uses its documented Faust-derived and original NAM Grid networks.
- [Strymon El Capistan](https://www.strymon.net/support/elcapistan/elcapistan-brochure/): tape-head rhythm, bandwidth and mechanical motion as separate choices.
- [BOSS CE-2W manual](https://static.roland.com/assets/media/pdf/CE-2W_e01_W.pdf): restrained/deeper chorus and vibrato as distinct uses of modulation.
