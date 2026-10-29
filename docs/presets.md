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

Factory presets are a short list of established starting points, not a bank of
variations. Each one is a familiar use that the module's insertion default does
not already cover: slapback, dotted-eighth repeats, a parked wah, a Leslie at
chorale or fast speed. Where the insertion default already is the usual starting
point, a module has fewer presets or none. Recipes use the module's own ranges;
similarly named controls in other plugins do not imply identical processing or
knob values. Threshold, drive and output trims assume ordinary interface gain
staging, with normal playing peaking around -9 dBFS.

| Modules | Factory starting points |
| --- | --- |
| Pedal Comp | Light always-on, Snappy clean picking, Long lead notes, Acoustic strumming |
| Smooth / Punch Compressor, Butter Comp, Peak Limiter | Clean guitar leveling, Even bass notes; Tight picking, Parallel punch; Thick parallel; Louder, controlled |
| Guitar EQ, Cut EQ, Parametric EQ | Scooped mids, Mid-forward lead, Dark jazz clean; Tight high gain, Bass cleanup; Low-mid mud cut, Harshness cut |
| Filter, Wah, Auto Swell | Touch wah; Cocked wah, throaty and lead bite; Slow chord bloom |
| Boost, Tube Boost, Distortion | Tight amp push, Warm push; Warm tube edge; Hard rhythm, Octave-up fuzz |
| Pedal Drive | Screamer boost, Blues breakup, Rodent distortion, Sustaining fuzz, Bass drive with clean lows |
| Digital Delay, Analog Echo, Tape Echo | Slapback, Dotted eighth, Ducked lead, Quarter-note ping-pong; Warm lead repeats, Dark modulated wash; Tape slapback, Worn tape repeats |
| Multi-head Tape, Reverse, Dual and Diffused Delay | Rolling three heads; Reverse half notes; Quarter and dotted eighth; Echo into cloud |
| Room, Dense Plate, Hall, Cloud, Spring Reverb, Shimmer | Live room; Short bright plate, Singing lead plate; Big dark hall; Long ducked cloud; Small amp spring, Surf drip; Dark octave-down pad |
| Airwindows Plate, Airwindows Guitar Hall, Airwindows Tape | Compact guitar plate, Long dark plate; Close guitar hall, Distant dark hall; Light tape rounding, Thick parallel tape |
| Galactic Space, Cosmic Echo | Slow dark space; Clustered echoes, Slow swell, Stereo echo groups, Chord bloom |
| Chorus, Juno Chorus, Flanger, Phaser | Subtle clean width, Lush 80s chorus; Wide two voices; Slow jet sweep, Light comb motion; Slow vintage phase, Deep stereo phase |
| Auto Tremolo, Vibrato, Uni-Vibe, Rotary | Amp tremolo, Hard eighth-note chop; Seasick warble; Vibrato mode swirl; Slow chorale, Fast tremolo |
| Mono Octaver, Stereo Doubler, Stutter, Bitcrusher | Octave-down blend, Synth bass; Wide loose double; Sixteenth-note chops; Broken small sampler |
| LoFi Tape Machine | Subtle drift, Worn cassette, Damaged tape (the [guide's starting points](modules/lofi-tape.md#starting-points); the last two include tape hiss) |

User presets only:

- **Noise Gate, Noise Gate (Smart), Digital Black Gate:** set the threshold to your noise floor and quiet notes; learn the Smart Gate's profile from your own input.
- **Utility:** balance, polarity, channel choice and alignment depend on the rig.
- **Outboard Channel, Console Colour, Console EQ, Ring Modulator, Frequency Shifter, Freeze / Hold:** the insertion default is the usual starting point, or the setting depends on the key, the mix or a live trigger.

Synced recipes select both Tempo sync and Division. Unsynced recipes restore
free time/rate; switching recipes cannot accidentally inherit sync, feedback,
mode, hold or output edits from the previous sound. Mix assumes ordinary insert
use. Set Mix for your routing, especially with **Keep dry level** or on a send.
Mono Octaver expects single notes. The Uni-Vibe vibrato recipe explicitly sets
its hidden Mix to 100% so only the phase-shifted signal is heard.
