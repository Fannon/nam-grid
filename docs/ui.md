# Editor

Build your rig in the grid, then use the panel below it to shape the selected module. The main controls stay visible while you browse sounds, so you can compare tones without losing your place.

![NAM Grid editor with a complete rig and Tape Echo selected](screenshots/grid.png)

## Find your way around

| Area | What you can do |
| --- | --- |
| **Header** | Open modulation and scenes, load or save rigs, tune, and manage your library. |
| **Grid** | Arrange modules across Pre, lanes A/B, and Post. See [Grid & routing](grid.md). |
| **Module editor** | Adjust the selected module, switch its mode, and view its response. |
| **IN / OUT** | Set the level entering and leaving the whole rig. |
| **A/B** | Balance the two parallel lanes. **Routing** opens their level, timing, and split controls. |

## Build and arrange a rig

- Click **+ Add module** or a loaded module's name to open the browser.
- Click a card's header to select it without opening the browser.
- Use the card's switch to bypass it. Right-click for **Clear**, **Copy**, **Move**, and **Swap**.
- Drag a card onto another to move or swap them. **Shift-drag** replaces the target and empties the source; **Alt/Option-drag** or **Ctrl/Cmd-drag** copies it.
- Dragging never copies or deletes files on disk. Host automation stays with the slot.

The browser can audition sounds before you commit. **Apply** keeps your choice; **Cancel** or Escape restores the original module and settings. See [Library browser](browser.md).

## Shape a module

Every module editor uses the same layout:

- **Top row:** module name, browser, previous/next, and category.
- **Options:** tempo sync and sound-specific choices such as **Style**, **Mode**, or **Heads**. Click a two-choice option to toggle it; longer lists open a menu.
- **Knobs:** up to four controls share one row. Larger modules place **Output** at the start of the second row and **Mix** at the end; **Enabled** is always at the bottom right.
- **Display:** a visual guide to the module's response or your playing.
- **Level strip:** input and output levels, overall gain, and peak-to-average (crest) change.

Drag a knob or click its value to type a number. Double-click restores its starting value. Right-click a knob to assign modulation; see [Performance controls](performance.md).

Previous/next stays within the current effect category or capture/IR folder. Adding a module gives you useful electric-guitar starting settings; [starting settings](effect-defaults.md) includes acoustic and bass suggestions.

### Displays and meters

Displays help answer practical questions: Is the compressor working? How dark are the repeats? Where is the EQ cutting? Some show live audio; others sketch the current settings. Their labels identify which is which.

Use the **level strip** to compare tones at similar volume:

- **Gain near 0 dB** means the module adds little overall level—useful for a fair bypass comparison.
- **Peak** catches short transients; **RMS** reflects the average level.
- **Crest** falling through a module usually means compression or saturation.
- Red above 0 dBFS warns of a hot level. Between modules this is headroom information; at **OUT**, it is the level sent to your DAW.

Turn all displays on or off in **Library & settings > Visualizations**. This changes only the display, never the sound or saved settings. The [module guide](modules/README.md) explains each module's display.

## Library and default folders

Open **Library & settings** to add NAM capture, cabinet IR, and preset folders. Each folder is scanned for its selected file type; add a mixed folder to more than one list if needed. **Open** reveals a folder in Explorer or Finder. **Remove** stops scanning it without deleting anything.

Use **Rescan all** after adding or changing files. Startup rescan is optional. A [`.ignore` file](browser.md#excluding-files-with-ignore) can hide work folders or unwanted files from the library.

Until you choose your own folders, NAM Grid creates these in your Documents folder:

- `NAM Grid/NAM Captures` — includes the bundled **RiffSmith Clean** capture.
- `NAM Grid/IRs` — a place for your cabinet WAV files.
- `NAM Grid/Presets` — includes **Empty** and **Clean Default**.

Your folder choices belong to you, not to a rig or DAW project. Loading a preset never replaces them. Removing every folder from a list is remembered; defaults are not recreated behind your back.

## Tuner and performance controls

**Tuner** opens the tuner below the grid and mutes the output by default. **Modulation**, **Routing**, and scenes **1 2 3** provide the live controls. See [Tuner](tuner.md) and [Performance controls](performance.md).

## Settings folder

**Library & settings > Settings folder** opens the per-user settings location. This is useful when backing up or troubleshooting your library choices and display preferences.

| Platform | Settings folder |
| --- | --- |
| Windows | `%APPDATA%\NAM Grid` |
| macOS | `~/Library/Application Support/NAM Grid` |
| Linux | `$XDG_CONFIG_HOME/nam-grid` (usually `~/.config/nam-grid`) |
