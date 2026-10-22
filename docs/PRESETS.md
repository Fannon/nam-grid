# Rig presets and grid gains

Click the preset name in the header to open the [preset browser](BROWSER.md). It focuses
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

## Gain placement

- **IN**, at the first row's input, is gain before the whole grid. It also precedes
  the tuner analysis and envelope-follower input. It ranges from -40 to +40 dB.
- **OUT**, at the final row's output, is the existing final master gain.
- **A / B**, on the right between the parallel lanes, selects lane balance: 0% A, 50% equal
  lanes, 100% B. Lane gain/mute/solo remain on Grid / Control.

Input/output changes use a 5 ms smoothing time constant. The per-NAM Input and Output trim
controls remain independent of the new global IN knob. Moving the controls uses
the side gutters. Cards have more space horizontally and vertically, taller
meters with a gap around the card, and a colored outline instead of a top strip.
Meters show the peak of the mono sum `(L + R) / 2`; the overall input/output meters
also show the average level of that mono signal. Bypassed and empty slots do not
monitor. All three controls are host-automatable and included in
project state and presets.

Click any grid tile or the module's browser icon to choose an effect, NAM capture or IR.
The current item and its category/folder are selected. The quick module menu remains on
the module name, and **Clear slot** is first in that menu and the right-click edit menu.
Shared capture sources and separate preset sources are managed in the browser; scanning
preferences remain in **Library & settings**.

Current state schema **25** has **3,547 host parameters**, including one contiguous
bank of 201 FX controls per slot, 44 performance controls and three tuner controls. It aligns unique
host/editor names and includes six Distortion styles and the new reverb/delay
audition modules, including Cosmic Echo. Learned noise profiles and
capture/IR content identities are included; active learning is cleared. Prototype
IDs, names and enum ranges are allowed to change before the first public release.
Older state readers remain best effort, without a project/automation compatibility
guarantee. See [FX_PARAMETER_REVIEW.md](FX_PARAMETER_REVIEW.md) for the current policy.

## Compact editor and visualizations

The window is 1000 x 720, down from 1000 x 800. Cards have slightly more height;
unused space above and below the grid is reduced, and the grid help line is gone.
Two unboxed rows of four controls leave a larger visualization to the right.
The tuner temporarily replaces this lower editor while leaving the grid visible. Long card/editor titles
end in an ellipsis; the card tooltip retains the complete file path.

All knob values and units sit below the dial, including IN, OUT and A/B. Click a
value to type it, or double-click the dial to restore its default. A/B shows a
percentage. The larger header logo is vertically centered with the menu buttons.
The editor leaves 24 px outer margins. Its unboxed layout gives the visualization
more height; selected modules show a short sound explanation beneath the title.

Every loaded module has an optional visualization on the right of the editor.
Noise Gate, Noise Gate (Smart), compressors and Peak Limiter show live input/output
level history against a dashed threshold line, plus numeric gain reduction. Gray
is input, the module color is output; the history covers about 2.4 seconds. For
Noise Gate (Smart), the line represents its optional final broadband gate and is
hidden when Gate is Off; spectral reduction still appears in the GR readout.
Digital/Analog/Tape Delay offer an echo timing sketch driven by Time,
Feedback and Mix; gray is dry input and colored stems are echoes. Synchronized
delays use the current host tempo, falling back to 120 BPM before tempo is available.
NAM captures show vector category icons for amp head, amp plus cabinet, cabinet,
pedal, outboard rack or space; unknown captures use a neutral capture icon. These
categories use existing metadata/filename inference. Guitar EQ, Cut EQ and
Parametric EQ show their combined settled frequency response, including enabled
cuts and bell/notch mode. The log axis spans 20 Hz to 20 kHz (bounded below Nyquist),
and the vertical scale is -24 to +24 dB. The curve uses the audio filters' actual
coefficient classes and is cached until controls or sample rate change.
Filter has Low-pass, High-pass and Band-pass modes. Envelope at zero is static;
raising it lets the input level move cutoff. Mode changes use a 5 ms smoothing time constant.
Its live response uses the selected mode and the applied
cutoff and resonance reported by the DSP. The cutoff follows the actual envelope;
solid is left, with a faint right-channel curve when the channels differ. While
inactive, it shows the base cutoff. Telemetry uses the existing meter queue and
does not allocate on audio or change filter processing.
Other modules, including IRs,
show the same gray-input/colored-output history on a -60 to 0 dB scale. Live plots
reuse existing mono meter packets; they do not capture audio or add DSP processing.
Use **Hide / Graph** to toggle the display for that slot. This is a temporary
editor preference and does not change automation, audio or the saved rig.

The global IN meter occupies the first card's input-meter position; that card's
duplicate input bar is hidden. Its output meter remains independent.

Within NAM/IR categories, shared folder levels are skipped up to the first branch
or file. Distinct source folders and folders alongside direct files remain visible.
Folders precede files at every level; each group is sorted alphabetically without
case sensitivity. Selection tags still refer to the original library snapshot.
