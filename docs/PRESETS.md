# Rig presets and grid gains

Click the preset name in the header for **Save preset as...**, **Load preset file...**
and the `.ngrid` presets in the last used folder. The arrow buttons step through
that folder in filename order and wrap at either end. The name shows the last saved
or loaded filename; it does not automatically save subsequent edits. Saving to an
existing filename asks for overwrite confirmation. Cancel leaves the rig intact.

Presets include all 16 module types, NAM/IR file references, slot controls, quality,
input/output gain, lane balance, routing, shared modulation, macros and stored
performance scenes. They use readable JSON with a format/version marker and the
same validated rig-state loader as project recall. Invalid or unsupported files
are rejected before changing the rig. A failed file replacement preserves the
previous preset. Presets are limited to 1 MiB.

Library folders and tuner reference/mute remain local to the current instance.
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

Click **+ Add module** once on an empty card; double-click a loaded card to replace
it. **Clear slot** is first in both module and right-click menus. NAM and cabinet
IR file pickers are followed by a divider, then FX categories. Indexed files are
split into **NAM captures** and **Cabinet IRs**. Shared library folders and rescanning
remain in **Library & settings**. The header search/filter bar is removed pending
a dedicated browser.

Current state schema **21** has **3,098 host parameters**, including one contiguous
bank of 173 FX controls per slot and 44 performance controls. It aligns unique
host/editor names and includes six Distortion styles and the new reverb/delay
audition modules, including Cosmic Echo. Learned noise profiles and
capture/IR content identities are included; active learning is cleared. Prototype
IDs, names and enum ranges are allowed to change before the first public release.
Older state readers remain best effort, without a project/automation compatibility
guarantee. See [FX_PARAMETER_REVIEW.md](FX_PARAMETER_REVIEW.md) for the current policy.

## Compact editor and visualizations

The window is 1000 x 720, down from 1000 x 800. Cards have slightly more height;
unused space above and below the grid is reduced, and the grid help line is gone.
The eight parameter cells share the entire editor width evenly. Their dials are
smaller, labels larger, and the selected slot ID is larger. Long card/editor titles
end in an ellipsis; the card tooltip retains the complete file path.

All knob values and units sit below the dial, including IN, OUT and A/B. Click a
value to type it, or double-click the dial to restore its default. A/B shows a
percentage. The larger header logo is vertically centered with the menu buttons.
The parameter panel leaves equal 24 px margins at the left, right and bottom.
The redundant footer description is removed; its space gives the header and
mini display more height.

Every loaded module has an optional mini display in the editor's top right.
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
