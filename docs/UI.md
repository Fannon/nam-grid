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
appear in the bottom editor. Double-click a card or click its module name in the editor to open
the module menu. Click an empty card's + Add module once to open it. Assigned modules have a larger
top-right bypass toggle; it shares the editor's Enabled / Bypassed parameter.
Clear slot is first in both the module and right-click menus; the latter also has
Copy / Move / Swap. The bottom editor has two unboxed rows of four controls,
with up to seven knobs followed by Enabled. A full-height visualization sits to the right.
The down-arrow beside a built-in FX name lists its category; for NAM captures and IRs it lists files in the same folder.
Previous/next arrows follow the same category or folder. NAM uses Input, Bass, Mid, Treble, Low cut, High cut, Output trim;
its neutral EQ acts after inference, not as controls of the captured amp. Compatible captures show
a **Quality: A2 / Quality: A2 Lite** button beside the file controls. Click to switch; its label
shows the current mode. The module menu also offers A2 Lite. IR uses Level;
see [MODULES.md](MODULES.md) for other controls.
Every loaded module offers a visualization: signal history versus threshold and
numeric GR and an orange reduction bar for dynamics; compressor histories exclude makeup gain and reflect parallel Mix. Other views include relative echo timing/treble sketches for standard and Dual delays,
live input/output level history for NAM captures, a gain-dependent distortion transfer curve with live input-range markers,
the combined response curve for EQ modules and live LP/HP/BP Filter response, and input/output
history for other effects and IRs. Distortion also shows its separate high-cut/mix/output response. Boost and Tube Boost show their gain/saturation curve and the combined Tilt/Low cut response; Tube labels its EQ as a small-signal response. Tape shows its Spiral saturation stage and Bump amount, explicitly excluding its stateful processing from the static curve.
Capture modules show window, overlap and gate timing; Octaver shows layer levels,
Doubler its detuned voices, Frequency Shifter its fixed-Hz sidebands, and Utility its mid/side routing.
Modulation modules show their settings: Chorus/Juno/Vibrato
delay voices, Phaser/Flanger response sweeps, Tremolo gain shape, Vibe stage sweeps,
Rotary horn/drum motion and Ring Mod's carrier. Time-based previews label their time window;
they describe the configured modulation rather than claiming to follow a live audio LFO phase.
Library & settings has one **Visualizations: ON/OFF** preference for all module editors.
Category badges remain visible when visualizations are off. This preference is stored per user,
outside project/rig state. NAM Amp + Cab uses separate Amp and Cab badges in both the card and editor.
All knobs show editable values and units below the dial;
double-click the dial to restore its module-specific default. Adding/replacing a built-in module
applies its electric-guitar starting settings, including parameters shared with the prior module.
Saved presets and slot edits retain their exact values. See [EFFECT_DEFAULTS.md](EFFECT_DEFAULTS.md)
for all module defaults and acoustic/bass adjustments, and [PRESETS.md](PRESETS.md) for rig recall.

Reverb previews share a larger field for early reflections, late decay and damping. Size changes spacing; pre-delay shifts the onset. Native Airwindows sustain/size controls use a relative scale rather than an invented RT60. Shimmer also shows pitch direction and Freeze. These are parameter
sketches, not measured room dimensions or impulse responses.

Rows are Pre, A, B and Post. IN/OUT controls sit outside the grid. Meters measure
the mono sum and only enabled, loaded modules monitor levels. Wiring is behind
the card meters; the global IN meter replaces the first card's duplicate input bar.
Global IN/OUT show peak dB above a smaller, explicitly labeled RMS reading. Peak holds for 1.5 seconds, then falls at 12 dB/second toward the current level. Their numeric levels measure the mono sum after IN gain and after OUT gain.
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

The eight-position editor uses two rows of four controls. Enabled stays in the
last position for assigned modules; empty slots hide it. NAM uses all seven knobs;
Tube Boost leaves one unused.
The opt-in OpenGL readback helper occasionally returns partial frames during
repeated captures; its screenshots are layout artifacts, not proof of host repaint
behavior. Reopen/resize/repaint should still be checked in Bitwig.

The automatable **Tuner** button sits beside Library & settings. When active,
the tuner replaces the lower module editor while the grid stays visible. It shows
note/octave, cents, frequency, A-reference (430–450 Hz), Mute output and a six-second
pitch-history graph. The trace fades with age, highlights ±3 cents and breaks at
silence, note changes or large jumps. Reference changes reinterpret the frequency
history against the new reference. Escape, Close or Tuner OFF disables it and
restores output through a 5 ms ramp. With Mute output ON (the default), the Tuner
parameter doubles as a host-automatable mute, including with the window closed.
Project state retains it; rig presets and performance scenes leave it unchanged.
It measures clean input, including right-only input, and clears the current pitch
after detection stops; old history ages out.

Delays/modulation show a Tempo sync switch in the module-selection strip. When enabled,
Division replaces Time/Rate in bottom position one; manual values remain stored.
The eight bottom positions stay fixed. Utility uses Output, L/R balance, Width, Invert L,
Invert R, Fine delay, one blank position and Enabled. Utility has the neutral gray category color.

Standard/Dual Delay Ping-pong and reverb Stereo wet switches sit beside Tempo sync in the module
strip. Chorus Spread uses position six. Spring and Wah appear under Reverb and
Filter / Volume respectively, with six/five knobs and unused bottom cells blank.
Utility Width is −100 mono / 0 unchanged / +100 wider; its host/state name is
also Width. Unique control names now agree across the editor and host.
Each selected module has a short sound explanation beneath its title. Hover its
controls for audible-effect guidance. Labels, conditional controls and parameter
sketches are detailed in [FX_PARAMETER_REVIEW.md](FX_PARAMETER_REVIEW.md).

Grid toggles have equal 12-pixel top/right insets and a 32-pixel hit area. Bypassed cards use a darker background, subdued text/badges and an unfilled toggle with a minus mark.

Empty slots are inactive add targets. They show no bypass toggle on the card and no Enabled or previous/next controls in the bottom editor; click + Add module or the module picker to fill one.
