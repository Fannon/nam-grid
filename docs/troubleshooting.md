# Troubleshooting & FAQ

Quick answers to common problems and questions. If yours is not here, [report it](#report-a-problem).

## NAM Grid does not appear in my DAW

- Check that the plugin is in a folder your DAW scans; see [install the plugin](getting-started.md#install-the-plugin). For VST3, the whole `nam-grid.vst3` bundle must be there.
- Rescan plugins in your DAW. Some hosts only scan new folders after you add them to the plugin paths.
- NAM Grid is 64-bit only. On a Mac, it needs Apple Silicon and macOS 11 or newer.
- **macOS:** clear the quarantine flag once after downloading; see [allow the plugin](getting-started.md#macos-allow-the-plugin).
- **Linux:** NAM Grid needs glibc 2.35 or newer (Ubuntu 22.04 or newer) and an X11 or XWayland session.

## I hear nothing

- Make sure the track receives your guitar: the right input is selected and input monitoring is on.
- The **Tuner** mutes the output by default. Close it, or turn off **Mute output**.
- Check **IN** and **OUT**, and the lane levels, mute and solo on the [Routing page](grid.md#routing).
- A noise gate with a high threshold can cut quiet playing. Bypass it to check.

## Crackles, dropouts or high CPU

- Raise your DAW's buffer size. Small buffers keep latency low but leave less time for processing.
- Every loaded capture uses CPU, even when A/B favours the other lane. Bypass or clear the ones you do not need.
- Compatible A2 captures offer **Quality: A2 Lite** in the module header, which uses less CPU.
- **Library & settings > Preset switch > Crossfade** briefly runs two rigs at once. Use **Short fade** when CPU is tight.

## A capture or IR is missing

Rigs refer to captures and IRs by their location on disk; they do not contain copies. When a file has moved, its slot keeps the reference and passes your sound through unprocessed. NAM Grid never silently substitutes a different file.

- If the file is still inside one of your library folders, **Rescan all** in **Library & settings**. NAM Grid recognizes a moved or renamed file by its content and relinks it automatically.
- Otherwise add its new folder to the library and rescan, or open the module browser on that slot and select the file again.

When you share a rig, include its captures and IRs.

## My stereo effects sound mono

NAM captures are mono, so they combine both channels into one. Place stereo effects such as chorus, stereo delays and reverbs after the last capture. The Utility module's **Width** cannot create stereo from a mono signal.

## Two lanes sound thin together

Parallel captures and cabinets can cancel each other's frequencies. Adjust the lane levels, or shift one lane in time with **Lane timing** on the [Routing page](grid.md#routing), and check the blend in mono as well as stereo.

## Projects from a preview build do not load

Builds before 0.1.0 used different plugin and parameter IDs, so their projects and presets cannot be recalled; please rebuild them. Remove the old plugin files and rescan your DAW. From 0.1.0 onward, projects, automation and rigs stay compatible with new versions.

## Questions

### What is a NAM capture?

A `.nam` file: a [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerPlugin) model trained to sound like a specific amp, pedal or complete rig. *Full rig* captures include a cabinet. *Amp head* or *direct* captures do not, so follow them with a cabinet IR.

### What is a cabinet IR?

An impulse response: a short `.wav` recording of how a speaker cabinet and microphone shape the sound. NAM Grid plays it with no added latency; see [Cabinet IR processing](modules/ir-processing.md).

### What is the difference between a rig preset, a project and a scene?

A **rig preset** (`.ngrid`) stores the whole grid and can be loaded anywhere. Your **DAW project** saves the current rig automatically. A **scene** switches controls within the current rig, without loading different modules. See [rig presets](presets.md#presets-projects-and-scenes).

### Does NAM Grid add latency?

Built-in effects and cabinet IRs add none. NAM captures can report a small latency when they run at a sample rate different from your project's; your DAW compensates for it.

### Where are my settings stored?

**Library & settings > Settings folder** opens it. It holds your library folders and display preferences, not your rigs.

| Platform | Settings folder |
| --- | --- |
| Windows | `%APPDATA%\NAM Grid` |
| macOS | `~/Library/Application Support/NAM Grid` |
| Linux | `$XDG_CONFIG_HOME/nam-grid` (usually `~/.config/nam-grid`) |

Your library starts in `Documents/NAM Grid`, with `NAM Captures`, `IRs` and `Presets` folders; see [library folders](browser.md#library-folders).

### Is there an Audio Unit, AAX or standalone version?

Not yet. NAM Grid is available as CLAP and VST3.

## Report a problem

Please [open a bug report](https://github.com/Fannon/nam-grid/issues/new/choose) on GitHub and include:

- your NAM Grid version, operating system, DAW and its version, and plugin format (CLAP or VST3),
- sample rate and buffer size, for audio problems,
- the modules involved and the steps to reproduce the problem.

A rig preset that shows the problem helps a lot, if you can share it.
