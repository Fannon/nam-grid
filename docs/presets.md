# Rig presets and grid gains

Click the preset name in the header to open the [preset browser](browser.md). It focuses
Presets only and selects the current rig in its folder. Search your preset folders
recursively, and select a result before loading it with Enter, double-click or Load.
Preset folders are a separate list from NAM and IR folders, scanned only for `.ngrid`
files, and are added in Library & settings (the browser's **Manage folders** opens it).
NAM Grid ships no preset files; every listed preset is a `.ngrid` file in one of your
preset folders. On first start the default `Documents/NAM Grid/Presets` folder receives
**Empty**, the empty grid (see [ui.md](ui.md#settings-and-default-folders)). Save as..., Open file... and Delete... are available in the
browser; right-clicking the header name also offers quick load/save actions and the last
used folder's presets. The arrow buttons step through that folder in filename order and wrap.

Clicking the header **Save** button opens Save as, so a click never overwrites a preset by
accident. Shift- or Alt-click writes over the loaded preset's file without asking; right-click
offers both **Save over** *file* and **Save as...**. For a rig that has no file yet, every
form of Save opens Save as. **New** asks for confirmation, then starts an
empty grid named Untitled. It keeps library folders, tuner settings and the current preset
folder for the arrows and Save as. The name shows the last saved or loaded filename;
subsequent edits are not saved automatically. Save preset as asks before overwriting an
existing filename. Cancel leaves the rig intact.

Presets include all 16 module types, NAM/IR file references, slot controls, quality,
input/output gain, lane balance, routing, shared modulation, macros and stored
performance scenes. They use readable JSON with a format/version marker and the
same validated rig-state loader as project recall. Invalid or unsupported files
are rejected before changing the rig. A failed file replacement preserves the
previous preset. Presets are limited to 1 MiB.

Library folders and tuner activation/reference/mute remain local to the current instance.
Project state and automation retain the Tuner switch; rig and scene recall do not change it.
Captured/held audio is not stored; Freeze Hold and Shimmer Freeze are released on
recall. Models and IRs are referenced at their existing paths, not embedded or
copied. Keep those files available when sharing a preset. Missing references
remain selected for relinking through the normal module chooser.

Performance scenes are different: they change controls within the existing
modules, whereas a rig preset can replace module types and models. Loading a rig
prepares it in a second grid while the current rig keeps playing unchanged, then
switches. **Preset switch** in Library & settings chooses how:

- **Short fade** (default) fades the old rig out, then the new one in, about 25 ms in
  total with a few milliseconds near silence. Only one rig runs at a time.
- **Crossfade** blends both rigs over 40 ms with equal power, without a gap. Both
  rigs run during that time, so the CPU load briefly doubles; prefer Short fade
  when a rig is already close to the CPU limit.

The old rig's delay and reverb tails end with the fade. A rig whose total latency
differs from the current one still asks the host to restart processing. Without
running audio, and for host project loads, the rig applies directly. Preparing
large models can briefly occupy the editor.

Algorithm selections are retained in rigs, projects and scenes and are changed
in the editor rather than through automation. Adding new modes preserves saved
choices and the other controls' automation mappings. Presets from builds before
0.1.0 are rejected and must be rebuilt. See
[parameter compatibility](development.md#parameter-and-project-compatibility).

## Gain placement

- **IN** is before the whole grid, tuner and envelope follower.
- **OUT** is the final master gain.
- **A/B** selects the lane balance: 0% A, 50% equal, 100% B.

All three are automated and saved with the rig; per-slot input/trim controls remain
independent. See [ui.md](ui.md) for the editor and [grid.md](grid.md) for routing.
