# NAM Grid 0.2.0

The drive update. NAM Grid gains **Pedal Drive** with ten guitar and bass voices, a second modulation bank with numbered sources, four always-available macros on the main page, a clearer editor and a more flexible Parametric EQ. Just replace the plugin files; rigs, projects and automation from 0.1.x keep working.

## What's new

### Pedal Drive

- **Ten drive voices:** Screamer, Rodent, Bass Edge, Parallel OD, Sustain Fuzz, Cascaded Preamp, Treble Drive, Blues OD, Transparent OD and Vintage Fuzz, from light breakup to dense fuzz.
- **Character** is a style-specific dial with its own label and function, such as Focus, Bite, Bias or Clipping. It starts centered; double-click returns to the original voice.
- **Low keep** and **Split** keep bass fundamentals clean within the same slot, so a bass rig does not need a second lane. A live display shows Drive, Character and the tone shaping together.
- **Antialiasing** keeps high-gain voices clean; Pedal Drive reports 64 samples of latency for it.
- **Example rigs** for every voice, plus clean boost and split-band bass setups. They are included on fresh installs, and existing users can add them with **Settings > Add drive presets...**.

The existing Distortion, Tube Boost and Boost modules keep their original sounds.

### More modulation

- **Two source banks:** numbered LFOs, envelope followers and sequencers, each with independent rate, shape, timing and step controls. Use any combination in the matrix and keep the same four macros.
- **Two listening points:** Env 1 and Env 2 can follow different slots in the grid, so one can react to your playing while the other follows a processed signal.
- Existing projects, presets and automation keep their original parameter identities; the new controls append after them.

### Macros and editor

- **Four macro knobs below IN** on the main page. Unassigned macros are dimmed; double-click a label to name it, and hide the bank in **Settings > Main macros** if you prefer.
- **A clearer header:** Settings, Tuner and Modulation now have icons, New and Save are compact, the preset selector is wider, and Library & settings is now simply **Settings**.
- **More room for controls:** module Enabled is a circle in the header, matching the grid node, and all eight dial positions are available for sound controls.
- **Two-row module editors:** up to eight dials sit in two consistent rows, with Output before Mix, and the parameter area has its own subtle background and divider.
- **Meters in the display footer:** module input, output and gain now share the visualization footer, with wider black displays and the same spacing as the grid.
- **Clearer routing and settings:** the Routing page's crossover display fills the space, library folders are colour-coded by kind, and category badges and bypass align with the slot.

### Parametric EQ

- Independent **Low Q** and **High Q** for resonant cuts.
- A **low-shelf** option with its own gain.
- Existing presets keep their original cut response.

### Documentation

- A new manual site at <https://fannon.github.io/nam-grid/> with a Getting started guide, Troubleshooting & FAQ, and player-focused deep dives for Noise Gate (Smart), LoFi Tape Machine, Cosmic Echo and Pedal Drive.

## Downloads

- `nam-grid-windows-x64.zip`: CLAP and VST3 for 64-bit Windows.
- `nam-grid-macos-arm64.zip`: CLAP and VST3 for Apple Silicon Macs (macOS 11 or newer). The Mac build isn't signed yet, so macOS will ask you to confirm it once; see [allow the plugin](https://fannon.github.io/nam-grid/getting-started#macos-allow-the-plugin).
- `nam-grid-linux-x64.zip`: CLAP and VST3 for 64-bit Linux (Ubuntu 22.04 or newer works well). Linux is still an early preview.

New to NAM Grid? Everything is included to make sound right away: one clean amp-and-cab capture (RiffSmith Clean) and starter rigs. [Getting started](https://fannon.github.io/nam-grid/getting-started) gets you installed, and the [module guide](https://fannon.github.io/nam-grid/modules/) covers every module.

## Compatibility

0.2.0 keeps NAM Grid's plugin identity and every existing parameter ID, name and range. Rigs, DAW projects and automation from 0.1.x load unchanged. Pedal Drive is a new module, so existing rigs keep their previous drives exactly as they were.

## Good to know

- Windows and Mac are well tested; Linux works but has seen less time in real DAWs, so start with a moderate buffer size and see how it feels.
- There's no Audio Unit, AAX or standalone app in this release.
- Each download includes license notes (`ThirdPartyNotices.txt` and `notices/`) — just keep them alongside the plugins if you pass them on.
- Something not right? Tell us your OS, DAW, plugin format, sample rate and buffer size at <https://github.com/Fannon/nam-grid/issues>.
