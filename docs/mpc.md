# NAM Grid on Akai MPC

> **Experimental:** the MPC plugins are new. Save your project before you add them, and see [Known limits](#known-limits).

Seven plugins bring NAM Grid to standalone Akai MPCs. Their names start with **NG** (NAM Grid) to fit the MPC's effect displays. They use the same DSP as the desktop plugin, one job each, so the MPC's own insert chain does the routing:

| Plugin | What it does |
| --- | --- |
| **NG Pedals** | One pedal (Clean Boost, Tube Boost and every Pedal Drive and Pedal Distortion voice) with optional compression. |
| **NG Amp** | A noise gate (standard, Smart or AI), a NAM capture and a cabinet IR. |
| **NG Delay** | Digital, analog, tape, multi-head, diffused, reverse and dual delays. |
| **NG Cosmic** | Cosmic Echo's ambient echoes and reverbs, one style at a time. |
| **NG Lo-Fi** | LoFi Tape Machine, Airwindows Tape and Bitcrusher. |
| **NG Mod** | Juno Chorus, Uni-Vibe, Rotary and Harmonic Tremolo. |
| **NG Console** | Console Colour, Outboard Channel and Butter Comp for buses and finished mixes. |

A full guitar rig uses a track's four insert slots: **Pedals → Amp → Delay → Cosmic Echo**. Each plugin is also useful alone, for example Cosmic Echo on a synth track. Lo-Fi, Mod and Console suit any track, return or submix. They are separate from the desktop plugin; desktop `.ngrid` rigs do not load in them.

They run on the 64-bit MPCs (MPC One G2, MPC Live III, MPC XL and later). The 32-bit MPCs do not list them: their CPUs are too slow for NAM captures.

## Install

1. Update the MPC to **MPC OS 3.10.1** or later.
2. On the MPC, open **Preferences > Enhancements**, then switch on **Enable Enhancements** and **External Plugin Support**.
3. Copy the plugin folders you want (**Simon Heimler - VST - NG Amp** and the others) into a folder named exactly `VST2 Plugins` at the top of a drive: a USB stick, an SD card or the internal drive.
4. Restart the MPC. The plugins appear among the effect plugins under **External Plugins**.

To update, replace the folders and restart the MPC. With developer mode on, the build can also install over the network; see [build.md](build.md#mpc).

## Add captures and IRs

NG Amp looks for files in a **NAM Grid** folder at the top of the drive that holds `VST2 Plugins`:

```text
<drive>/
  VST2 Plugins/Simon Heimler - VST - NG Amp/   the plugin
  NAM Grid/
    Captures/   your .nam captures, in folders
    IRs/        your cabinet impulse responses (.wav), in folders
```

A **NAM Grid** folder on any other drive (SD card, USB drive) is found too; its folders show the drive's name, such as `MPC_SD: Amps`. Your files keep their place when you update the plugin. A clean factory capture is included, so the Amp plays straight away. Organise files into folders: **Folder** picks a folder and starts at its first file, and **File** steps through that folder. A favourites folder such as `_Top10` sorts first. Files copied while the Amp is loaded appear when you next turn **Folder**.

A project remembers each file by its folder and name, so adding files never changes which capture a project uses. If you move or rename a file, the File display shows **Missing:** and the Amp plays dry until the file is back.

Open **LIBRARY** in NG Amp to see the complete capture and IR file names, their folders,
and whether each is loading, ready, missing or failed. A failed load includes a short
reason and leaves that section dry. After repairing a file, turn Folder or rescan to
retry it. **Normalize** on this page shows the applied gain, or says when a capture has
no loudness metadata. The page only displays information; choose files on AMP and CAB.

## Pages and Q-Links

Every page is four columns of four controls, and each column is one Q-Link bank. A panel with a heading marks each column; the first heading shows the module, voice or style that is playing, and the MPC's highlight fits the panel of the active bank. Knobs, labels and colours follow the desktop editor: each plugin's labels use its module category's colour. On/off settings are toggles, green when on and red when off; other settings with two to four states are selector buttons. On an MPC with four Q-Links, the bank buttons step through the columns from left to right, each read top to bottom; touching a control selects its column. Most pages use the same order:

1. The four main controls of the module.
2. Four more controls: usually Output and Mix.
3. Setup: the module, voice or style, **On**, or **Folder** and **File**, then options such as **Tempo sync**, **Learn** or **Mode**.
4. Any further options, or the plugin's **Output**.

Knobs show the name of the control they hold; places a module does not use are empty, and a column the module does not use at all disappears. Each option has a fixed place in its plugin, so switching modules never moves an option or changes what a switch does. The knobs keep the desktop editor's order, except where another grouping suits the Q-Links better. **Output** for the whole plugin only appears on NG Cosmic (next to Mix) and NG Mod; the other plugins' modules have their own Output or Level.
The Amp's Low cut at its minimum and High cut at its maximum show **Off**, because those settings bypass the filter.

### NG Amp

- **AMP** (the first page): **LEVELS** has Input, Output, Low cut and High cut; **TONE** has Bass, Mid and Treble; **CAPTURE** has Folder, File, **Quality**, which switches slimmable (A2) captures between Lite (the default on the MPC) and Full, and **Normalize**, which levels every capture to −18 dB from its loudness metadata so captures can be compared at the same volume (on by default); **GATE / CAB** chooses the gate, holds its main control (Threshold, Sensitivity or Reduction) and switches the cabinet on.
- **GATE:** **Off** (the default), **Noise Gate**, **Smart Gate** or **AI Gate**, with the chosen gate's controls: the gate itself in the first column, its final gate and Mix in the second. For Smart Gate, mute the strings, switch **Learn** on for one to three seconds, then off. The learned noise is saved with the project and kept while you try another gate.
- **CAB:** the cabinet IR, off by default because many captures include a cabinet. **LEVELS** has Input, Output and the cuts, as on the AMP page; **MIX** has Mix, Pre-delay and **Length** (Cabinet or Long IR); **IR** has Cab On, Folder and File.
- **LIBRARY:** full file names, folders, loading/error status and normalization availability. A file may be ready while its cabinet section is switched off.

### NG Pedals

- **PEDAL:** **Voice** chooses **None**, **Clean Boost**, **Tube Boost** or any Pedal Drive or Pedal Distortion voice. Every voice has its gain and output in the first column. Another voice of the same pedal keeps your settings, like a voice switch; another pedal, or None in between, returns to that pedal's last settings. The **COMP** column switches on Pedal Comp before the pedal, with Sustain, Level and Blend: a clean boost with compression is Clean Boost plus Comp On.
- **COMP:** every Pedal Comp control, including Attack, Body / Pick and Pickup.

### NG Delay

**Type** chooses the delay; each starts from its own starting settings, and returns to your last settings when you come back to it. **Tempo sync** follows the MPC's tempo and turns Time into **Division**.

### NG Cosmic

**Style** chooses the space. Styles share the knob settings, so you can compare them directly.

### NG Lo-Fi, Mod and Console

**Type** chooses the module; each starts from its own starting settings, and returns to your last settings when you come back to it. The options hold each module's switches: Juno Chorus **Mode**, Uni-Vibe **Vibrato mode**, **Tempo sync** for Uni-Vibe and Harmonic Tremolo (following the MPC's tempo), and the Console Colour **Desk**.

Each plugin has built-in programs; your own presets are saved with the MPC's preset menu. The [module guide](modules/README.md) describes every control: [Noise Gate (AI)](modules/gate.md#noise-gate-ai), [Noise Gate (Smart)](modules/noise-gate-smart.md), [Pedal Comp](modules/dynamics.md#pedal-comp), [Pedal Drive](modules/pedal-drive.md), [Pedal Distortion](modules/distortion.md#pedal-distortion), [Delay](modules/delay.md), [Cosmic Echo](modules/cosmic-echo.md), [LoFi Tape Machine](modules/lofi-tape.md), [Modulation](modules/modulation.md), [Airwindows Tape](modules/distortion.md#airwindows-tape), [Bitcrusher](modules/distortion.md#bitcrusher), [Console Colour](modules/distortion.md#console-colour), [Outboard Channel](modules/dynamics.md#outboard-channel) and [Butter Comp](modules/dynamics.md#butter-comp). Values and ranges are the same as on the desktop.

## CPU

NAM captures use the most CPU. Watch the MPC's CPU meter while you build a rig:

- One capture per track is the safe start. Captures start at Lite quality; Full and larger capture sizes use more.
- NG Amp works in mono, like the capture: it sums a stereo input, so its gate and cabinet run once rather than per channel, and sends the result to both sides.
- Captures recorded at 48 kHz are converted to the MPC's 44.1 kHz, which costs some CPU and adds a fraction of a millisecond of delay.
- Pedal Drive, Pedal Distortion and Cosmic Echo cost more than the gates, delays, Lo-Fi, Mod and Console modules.
- A section that is off uses almost no CPU once its fade has finished.

## Known limits

- The desktop's parallel lanes, modulation, scenes, tuner, displays, module presets and library browser are not available.
- Bypassed delays and reverbs stop at once; trails are off.
- The plugins have had little testing on MPC hardware so far. Report problems with the MPC model and OS version.

For performance reports, include the MPC model, OS version, audio buffer size, the exact
plugin chain, capture name and Lite/Full setting, gate type, cabinet setting, and whether
the input was silent or playing. Compare NG Amp alone with the complete
Pedals → Amp → Delay → Cosmic chain, then compare Lite and Full at the same settings.
Record the MPC's CPU reading and any audible dropouts. Emulated CPU readings do not
predict how many instances the MPC can run.

## Build from source

The MPC build uses the [inMusic MPC plugin SDK](https://github.com/inmusic-software/mpc-plugin-builder-sdk), pinned as the `mpc-plugin-sdk` submodule:

```sh
python mpc/build.py            # every plugin: native test libraries, arm64, skins, validation
python mpc/build.py --test     # also the MPC plugin tests and the SDK host simulator
```

The first build installs the zig cross compiler into the SDK's `.sdk-tools/` (network needed once). Output goes to `build/<platform>/mpc/`; the plugin folders are in `build/<platform>/mpc/out/`. See [build.md](build.md#mpc) for the build and test details.

### Implementation notes

- [`mpc/src/MPCEngine.h`](../mpc/src/MPCEngine.h) wraps the SDK's `mpc::Plugin`: a plugin is a fixed series of sections, each a desktop `NAMStage`, declared in [`mpc/plugins/`](../mpc/plugins/) with its Type menu, On switch and files. Plugin IDs, parameter ids, state formats, section ids, module keys and menu order are permanent once released: append, never reorder or rename.
- [`mpc/src/MPCLayout.h`](../mpc/src/MPCLayout.h) maps each module's controls onto the generic knobs and options from `effect_presentation`, and reproduces the desktop parameter curves and value text.
- Knob and option names are dynamic. A dispatcher wrapper reports them, and the plugin calls `audioMasterUpdateDisplay` from its worker thread when they change. Unused places report an empty name, which the skin hides (`HideOnInvalidName`).
- The build lays out each page from `pages.json` (a section's standard page, or explicit columns and headings) in four Q-Link columns. [`mpc/skins.py`](../mpc/skins.py) runs the SDK's generator with a panel and heading per column and focus rectangles that fit the panels. The SDK infers parameter kinds by probing display text, so each plugin describes its menus and its module-dependent controls in a manifest the build merges into `params.json`.
- A worker thread scans the library, loads captures and IRs and publishes prepared modules through `NAMStage`'s lock-free exchange. The audio thread only reads atomics.
- Projects save each section's module key, every control as a named value, the file reference and the Smart Gate profile as JSON in the plugin chunk. The entire chunk is validated before applying any parameters; malformed chunks, other plugins' state and unsupported schema versions are rejected without changing the current settings. Unknown module keys in a supported schema load as the section's default; missing values take the module's starting settings. A project load clears remembered settings from the previous project. Parameter-only chunks restore options before interpreting knobs, so tempo divisions keep their meaning.
- NG Amp processes in mono (`PluginSpec::mono`) and has a 5 Hz DC blocker after the capture (`SectionSpec::dcBlock`). The library also scans `NAM Grid` on `/media/*` and `/nvme` (`NAM_GRID_MPC_DRIVES` overrides the list for tests); references from those drives are saved as `drive:<name>`. The cabinet convolver's multiply-accumulate is already vectorised with NEON by clang on arm64.
- `plugin.json` has no `file32`, so 32-bit MPCs ignore the plugins; the build accepts the SDK validator's error about it.
- The libraries are linked with `-z nodelete`, because zig's shared-library startup does not run static destructors when the host unloads the plugin. On Linux, file access avoids libc++ `std::filesystem` stat calls, which fail in 32-bit builds for files with large inode numbers.
- Option assignments are explicit in each section spec. The MPC tests compare plugin identity, host parameter order/ranges/flags, module keys and control assignments with the reviewed fixtures in `mpc/tests/fixtures/`, and load their original saved states. Adding a module must not move an existing option. Automatable Type menu ranges are frozen too; growing one requires a compatibility design that preserves existing normalized automation.
- The incremental object cache records the effective compiler command and toolchain identity beside each object. Changing optimization flags, definitions, targets or compiler versions rebuilds the affected objects; failed compiles cannot leave a usable cache entry.

Generate repeatable UI previews with:

```sh
python mpc/build.py --target native --gallery tmp/mpc-gallery
```

Open `index.svg` in that folder. The gallery captures each module/voice and tempo-sync
layout from the running engine, plus empty, long-name, missing-file and error states.
It uses the skin's geometry and bitmap assets, with no browser, network or native editor
automation. Font rendering and touch behavior on an MPC still need hardware testing.
The MPC workflow uploads the gallery alongside the deployable plugins.
