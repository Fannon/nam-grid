# Rig presets and grid gains

Click the preset name in the header to open the [preset browser](browser.md). It focuses
Presets only and selects the current rig in its folder. Add preset folders, search them
recursively, and select a result before loading it with Enter, double-click or Load.
Save preset as... and Open file... are available in the browser; right-clicking the header
name also offers quick load/save actions and the last used folder's presets. The arrow
buttons step through that folder in filename order and wrap. The name shows the last
saved or loaded filename; subsequent edits are not saved automatically. Saving over an
existing filename asks for overwrite confirmation. Cancel leaves the rig intact.

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
prepares models on the control thread and uses the existing DSP publication and
host-restart policy; it can briefly occupy the editor for large models.

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
