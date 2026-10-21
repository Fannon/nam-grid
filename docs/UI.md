# Flat editor and library

The active CMake CLAP/VST3 editor uses a black background, regular Roboto text, dark flat
surfaces and restrained mint accents. No Gateway bitmap panel, Michroma title, textured file
selector or animated settings overlay is instantiated. Routing stays fixed. This unreleased prototype does not guarantee old parameter IDs or project compatibility.

The compact header contains the logo, preset folder browser and arrows, Tuner,
Grid / Control and Library & settings. IN gain sits before the first grid card;
OUT gain and A/B balance occupy the right gutter. Sixteen cards show slot ID, file/effect name,
type, enabled state and slim input-left/output-right meters beside the card. Tile IDs carry
their module colour, the same hue as the module head below, and the selected card brightens
its own hue rather than fading to a neutral outline, so selection stays readable next to the
grey Dynamics modules. Click to select; only the selected module's controls
appear in the bottom editor. Double-click a card or use Choose / Change in the editor to open
the module menu. Click an empty card's + Add module once to open it. Its dot and the
editor's Enabled / Bypassed switch share the same parameter.
Clear slot is first in both the module and right-click menus; the latter also has
Copy / Move / Swap. The bottom editor always has eight equal-width positions,
with up to seven knobs followed by Enabled. NAM uses Input, Bass, Mid, Treble, High pass, Low pass, Output trim;
its neutral EQ acts after inference, not as controls of the captured amp. Compatible captures show
a **Quality: A2 / Quality: A2 Lite** button beside the file controls. Click to switch; its label
shows the current mode. The module menu also offers A2 Lite. IR uses Level;
see [MODULES.md](MODULES.md) for other controls.
Every loaded module offers a mini display: signal history versus threshold and
numeric GR for dynamics, echo timing for standard delays, vector category icons
for NAM captures, a gain-dependent distortion transfer curve with live input-range markers,
the combined response curve for EQ modules and live LP/HP/BP Filter response, and input/output
history for other effects and IRs. Modulation modules show their settings: Chorus/Juno/Vibrato
delay voices, Phaser/Flanger response sweeps, Tremolo gain shape, Vibe stage sweeps,
Rotary horn/drum motion and Ring Mod's carrier. Time-based previews label their time window;
they describe the configured modulation rather than claiming to follow a live audio LFO phase.
Library & settings has one **Visualizations: ON/OFF** preference for all module editors.
Category badges remain visible when visualizations are off. This preference is stored per user,
outside project/rig state. NAM Amp + Cab uses separate Amp and Cab badges in both the card and editor.
All knobs show editable values and units below the dial;
double-click the dial to restore its default. See [PRESETS.md](PRESETS.md).

Rows are Pre, A, B and Post. IN/OUT controls sit outside the grid. Meters measure
the mono sum and only enabled, loaded modules monitor levels. Wiring is behind
the card meters; the global IN meter replaces the first card's duplicate input bar.
Global IN/OUT show RMS dB plus a peak reading held for 1.5 seconds, then falling at 12 dB/second
toward the average. Their numeric levels measure the mono sum before IN gain and after OUT gain.
Colors are semantic: amber amps, blue cabinets/IR, an amber-to-blue gradient for amp+cab,
green pedals, light gray dynamics/gates, rose EQ, coral Boost/Tape, periwinkle
modulation/filter, cyan delay, magenta Creative and violet reverb. Unclassified NAM files remain neutral.
Selection brightens the border toward white, retaining the Amp-to-Cab border gradient.
Knobs and switches use a neutral monochrome palette.

The bottom editors are independent containers with bounds enclosing their children. Their
parameter bindings remain attached to their physical slots; selection changes visibility,
never audio or automation ownership. The main card bounds do not clip the bottom panel.

## Multiple sources

Library & settings provides Add folder, Remove and Rescan all. Up to 32 roots are retained
in project state and the per-user preference. Startup rescan is optional (default on); when off, use Rescan all. More than seven roots scroll. Missing sources
report an error while readable sources still populate. Canonical duplicate roots and overlapping
file paths are deduplicated; identical source basenames receive distinct menu names. Root and
nested folder names remain in the index. The header search/filter bar is removed
pending a dedicated browser. The scanner runs on its existing background worker;
latest request wins, and menus retain immutable snapshots while open.
The scanner computes SHA-1 content hashes for captures/IRs. The hash is a recall aid, not a
menu filter: byte-identical files keep their own entry, so a pack folder stays complete when a
curated copy of the same capture exists elsewhere, and repeats are only counted. Overlapping
roots and repeated roots still index each physical path once. Slots, projects, rig presets and
Copy/Move/Swap retain the content hash. Recall keeps the saved or explicitly chosen path while
that file still holds the remembered content, so picking one of several identical captures is
never redirected to another copy. The hash only takes over when the file was renamed, moved or
replaced, and completed rescans relink slots whose file disappeared or changed; a different file
at the old path is never silently substituted. Missing indexed matches retain the original
path/hash for a future rescan.

Both `.nam` and `.wav` extensions are indexed case-insensitively. WAV entries are tagged IR /
Cabinet. NAM captures and Cabinet IRs have separate menu categories. Shared folder
levels are skipped until a branch or file. Every menu level lists folders before
files, with case-insensitive alphabetical ordering within each group and original
snapshot tags retained. Selecting an IR (including via previous/next) uses LoadSlotIR, changing both slot type
and processor; selecting NAM switches back. WAV contents are validated by the existing IR
loader on selection, not by the indexer. Non-IR WAV files can appear as candidates; missing or
invalid loads preserve the currently loaded module. No new decoder or audio dependency is added.
File menu labels contain only the filename stem. The down arrow beside the selected capture/IR
opens a flat list of files of the same type in its physical folder; it does not recurse.
The previous/next buttons stay within the same physical folder and file type for NAM/IRs,
or within the current module-menu category for built-in effects. Both directions wrap.
An empty NAM slot stays empty until a file is chosen. The main chooser can select any category.

`CaptureFolders` is retained in project state. `CaptureFolder` remains a first-root alias
for older readers. Older single-root projects and user preferences migrate automatically.
Malformed arrays, non-string entries, embedded NULs and excessive root counts are rejected
before modifying project state. Removing a library source never deletes a file or unloads a module.

## Older project processing

New instance defaults disable the former shared gate, EQ and IR. Those stages are now chosen
as modules. Their old host parameter IDs and saved values remain readable, for the older formats that remain readable. Older unreleased grid states can require rebuilding after the Pre/Post parameter-name changes. When any inherited processing is enabled, settings exposes Legacy
gate / EQ / IR toggles to disable it. Previous Pre 2 input calibration/output-mode behavior remains
in a flat settings section, separate from musical input gain. This change introduces no DSP latency.

## Verification

Windows CLAP and VST3 build with CMake. The existing library, DSP and CLAP tests cover mixed
sources, overlap, duplicate basenames, filters, nested menu tags, missing roots, preference
migration, folder-list state recall/rejection, native editor selection, knob ownership, bypass,
quality changes and NAM-to-IR-to-NAM cycling. Native settings input interception is exercised.
The editor is also rendered for visual review. Bitwig and macOS operation remain unverified.

On Windows an optional fourth argument to `nam-clap-smoke` writes visual-review BMP artifacts.
This opt-in mode temporarily shows its own editor without activating it, reads only that editor's
OpenGL surface, then closes it; ordinary CTest keeps the editor hidden:

```powershell
# Run from build-grid; paths are relative to that working directory.
.\NeuralAmpModeler\nam-clap-smoke.exe .\out\nam-grid.clap .. .\ui-preview
```

Manual DAW check:

1. Close older plugin windows and rescan/load `nam-grid`. Check black background, readable text,
   selected outline, names, meters and bottom controls at default and scaled sizes.
2. Add one NAM root and a separate IR root; rescan. Navigate both through their menu categories.
   Select a WAV: confirm WAV / IR and Level. Select a NAM: confirm Input / Output trim.
3. Select NAM, gate, compressor and both EQ variants. Tweak controls and automate them; changing
   selection must neither change audio nor redirect automation to a different slot.
4. Open settings. Grid controls must not show through or respond. Close/Escape must return to
   the same selection. Remove a source; its loaded modules must keep processing.
5. Save/reopen the project. Check every library root, module file/type, gain, bypass and quality.
   Open an older project with shared processing; verify its compatibility toggles and sound.

The eight-position editor spreads controls evenly across its width. Enabled
stays in position eight. NAM uses all seven knobs; Tube Boost leaves one unused.
The opt-in OpenGL readback helper occasionally returns partial frames during
repeated captures; its screenshots are layout artifacts, not proof of host repaint
behavior. Reopen/resize/repaint should still be checked in Bitwig.

The Tuner button sits beside Library & settings. Its opaque panel has note/octave,
cents, frequency, A-reference (430–450 Hz) and Mute output. Escape/Close and editor
teardown always restore output through a 5 ms ramp. The reference and mute preference
are saved; panel visibility is transient. It measures clean input, including right-only
input, and displays no stale result after audio stops.

Delays/modulation show a Tempo sync switch in the module-selection strip. When enabled,
Division replaces Time/Rate in bottom position one; manual values remain stored.
The eight bottom positions stay fixed. Utility uses Level, L/R balance, Stereo, Invert L,
Invert R, two blank positions and Enabled. Utility has the neutral gray category color.

Delay Ping-pong and reverb Stereo wet switches sit beside Tempo sync in the module
strip. Chorus Spread uses position six. Spring and Wah appear under Reverb and
Filter / Volume respectively, with six/five knobs and unused bottom cells blank.
Utility Stereo is −100 mono / 0 unchanged / +100 wider; the prior Width scale is migrated.
