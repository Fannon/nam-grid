# Flat editor and library

The active CMake CLAP/VST3 editor uses a black background, regular Roboto text, dark flat
surfaces and restrained mint accents. No Gateway bitmap panel, Michroma title, textured file
selector or animated settings overlay is instantiated. Routing stays fixed.

The compact header contains the logo, **Modulation**, the scene buttons **1 2 3**, the
preset browser with its arrows, **New** and **Save**, Tuner and Library & settings. IN gain
sits before the first grid card; OUT gain, A/B balance and the **Routing** button occupy the
right gutter. Routing replaces the module editor below the grid until you select a slot,
close it or press Escape (see [performance.md](performance.md)). Each of the sixteen cards has two parts.
Its header row shows the slot ID, the category badges (Amp and Cab for an amp + cab capture)
and the bypass switch; below it, a name button shows the loaded file or effect, or
**+ Add module** on an empty card. Slim input-left/output-right meters sit beside the card.
Tile IDs carry their module colour, the same hue as the module head below, and the selected
card brightens its own hue rather than fading to a neutral outline, so selection stays
readable next to the grey Dynamics modules.

Click the header row to select a slot without opening anything. Click the name button (or
double-click anywhere on the card) to open the browser; accepting a choice or closing it
reveals that slot's bottom editor. Empty cards open Modules; loaded cards focus the current
FX category or NAM/IR folder and select the current item. Drag a loaded card onto another
card to rearrange the grid. As in most DAWs:

- **Drag** swaps two loaded cards, or moves a card into an empty one.
- **Shift-drag** replaces the target card and empties the source.
- **Alt/Option-drag** duplicates the card, replacing the target. Ctrl/Cmd-drag does the same,
  as in Ableton Live and REAPER.

The target card names the action while you drag. The bypass switch and right-click edit menu
remain direct actions. Click the module name in the editor for the quick module menu; its
browser icon opens the full browser. Clear slot is first in both the module and right-click
menus; the latter also has Copy / Move / Swap. The bottom editor has two unboxed rows of four controls,
with up to seven knobs followed by Enabled. Output, where a module has one, always sits
bottom-left below the first control (NAM, IR and every built-in); the other controls keep
their order around it. A full-height visualization sits to the right.
The browser icon beside a built-in FX name opens its category; for NAM captures and IRs it opens the current folder.
Previous/next arrows follow the same category or folder. NAM uses Input, Bass, Mid and Treble, then Output, Low cut and High cut,
keeping Input and Output in the left column;
its neutral EQ acts after inference, not as controls of the captured amp. Compatible captures show
a **Quality: A2 / Quality: A2 Lite** button beside the file controls. Click to switch; its label
shows the current mode. The module menu also offers A2 Lite. IR uses Level;
see [modules.md](modules.md) for other controls.
Every loaded module offers a visualization. Views aim to show how controls interact
with each other and with your playing, using the module's actual DSP math:

- **Dynamics:** signal history versus threshold, numeric GR and an orange reduction bar.
  Compressors, gates and the limiter also show their static/closing curve with a dot at
  the current input peak, so threshold, knee, Mix and upstream gain all visibly move it.
  Gates shade the 3 dB band between opening and closing. Compressor histories exclude
  makeup gain and reflect parallel Mix.
- **Program-dependent compressors:** Butter Comp has no static curve. It shows detector
  gain reduction over time (0–24 dB, hanging from the top like a console GR meter) beside
  the gray IN and coloured OUT level histories.
- **Outboard Channel:** one view per stage. The cut response is measured by running the
  module's own DSP on a quiet impulse with compression and saturation off, drawn over the
  live output spectrum. The saturation curve is the strip's static Spiral stage at the
  current Saturate, with a dot at the input peak. A slim gain-reduction history shows the
  compressor; how fast it recovers reflects Comp speed, which the title also names.
- **Measured IN > OUT:** NAM captures, Console Colour and Digital Black plot recent input
  peaks against output peaks. Their real
  saturation/compression curve appears from playing; the diagonal is unity gain. NAM
  adds its post-capture EQ response with the same live output spectrum fill as the EQs,
  and the capture's input calibration when present.
- **Cabinet IR:** the file's frequency response relative to its peak, computed once per file.
- **Drive:** transfer curves with live input-range markers. Distortion's High cut/Mix/Output
  follow the shaper. Tube Boost's Tilt and High pass run before its saturator, so with
  Tilt or High pass active it also draws the curve for 100 Hz and 3 kHz content.
- **EQ / Filter:** every module in the category draws its settled response over the same
  live output spectrum. Parametric EQ shades its Q bandwidth; Filter shades the cutoff range
  its Envelope can reach, with the full shift at -12 dBFS input peaks. Console EQ's curve is
  measured by running its own DSP on a quiet impulse; its band density is level-dependent,
  so louder notes add harmonics the curve does not show.
- **Utility:** a goniometer of the live output, mid upward and side across, auto-scaled so
  the shape reads at any level. Mono is a vertical line; Width spreads it, Balance tilts it,
  and inverting one channel turns it horizontal. Beside it, a correlation meter (−1 to +1,
  amber when negative) and left/right RMS bars show the same in numbers.
- **Bitcrusher:** one cycle of a 220 Hz reference at 100% wet: the quantized, sample-held
  staircase filled to zero over the gray dry sine, plus a line for the High cut result
  when High cut is below its maximum. Mix and Output only blend and scale this shape.
- **Delays:** echo timing and treble on a fixed dB scale. Modulation depth is given in
  cents, because it is heard as pitch. Digital Delay marks each repeat's ducked level
  while playing at -12 dBFS. Reverse Delay shows that each window plays backwards during
  the next one; Stutter shows a new slice every four windows.
- **Pitch and spectrum:** Frequency Shifter draws its feedback cascade (each pass shifts
  again by Shift); Ring Mod shows the kept partials and inharmonic sidebands of a 220 Hz
  reference; Octaver draws the square sub-octaves its Tone control rounds.
- **Modulation:** Chorus/Vibrato delay voices with the resulting pitch deviation;
  Phaser/Flanger response sweeps (Flanger notes that negative feedback also darkens);
  Uni-Vibe's response (notches in Chorus mode, flat in Vibrato mode); Tremolo gain
  shape; Rotary speed ramp and horn Doppler depth; Juno delay voices.

Time-based previews label their time window; they describe the configured modulation
rather than claiming to follow a live audio LFO phase.
Library & settings has one **Visualizations: ON/OFF** preference for all module editors.
**Preset switch: Short fade / Crossfade** sets how rig presets change while audio plays
(see [presets.md](presets.md)); it is also a per-user preference.
Category badges remain visible when visualizations are off. This preference is stored per user,
outside project/rig state. NAM Amp + Cab uses separate Amp and Cab badges in both the card and editor.
All knobs show editable values and units below the dial;
double-click the dial to restore its module-specific default. Adding/replacing a built-in module
applies its electric-guitar starting settings, including parameters shared with the prior module.
Saved presets and slot edits retain their exact values. See [effect-defaults.md](effect-defaults.md)
for all module defaults and acoustic/bass adjustments, and [presets.md](presets.md) for rig recall.

Mode controls with three or more choices open a dropdown listing every option,
with the current choice checked. Cosmic Echo, Distortion and Juno Chorus also
include short sound descriptions. Selecting the current choice or dismissing
the menu keeps the setting;
two-choice modes toggle on click.

Reverb previews share a field for early reflections and nominal -60 dB decay. Size changes spacing; pre-delay shifts the onset. Logarithmic time is explicit. Room and Hall add their exact low (<200 Hz) and high (above Damping) decay lines; Plate and Cloud add the decay at 5 kHz, which Damping, Size and Decay set together. Native Airwindows sustain/size controls use a relative scale rather than an invented RT60. Shimmer shows its pitch layers (each tank pass shifts again, quieter by Amount) and Freeze. These are parameter
sketches, not measured room dimensions or impulse responses. Wet Mix is shown as a number; it does not shrink the tank sketch.
Auto Swell draws rise, the 40 ms hold and exponential release on one time axis, beside live IN/OUT history; OUT relative to IN is the swell gain. The hold resets while the input stays above threshold; release begins after the hold runs out and the detected level falls at least 3 dB below threshold (the shaded band). Notes played during the release continue from the current gain rather than swelling again.
All FX share separate title, explanation, chart and footer rows. Frequency/dB
limits, reference signals and measured histories are labeled. See the complete
[module-by-module visualization audit](ui.md).

Rows are Pre, A, B and Post. IN/OUT controls sit outside the grid. Card meters measure
the mono sum and only enabled, loaded modules monitor levels. Wiring is behind
the card meters; the global IN meter replaces the first card's duplicate input bar.
Global IN/OUT are stereo peak meters (left and right bars) after IN gain and after OUT gain.
Their value is the higher channel's peak in dBFS, held for 1.5 seconds, then falling at
12 dB/second. Every meter uses the same scale, from -60 to +6 dBFS, with a line at 0 dBFS.
Level above the line draws red, and the area above it stays red for 2 seconds after an over,
as does the global value. The grid processes in floating point, so an over between modules
is a warning rather than clipping; at OUT it is what the host receives.
Colors are semantic: amber amps, blue cabinets/IR, an amber-to-blue gradient for amp+cab,
green pedals, light gray dynamics/gates, rose EQ, coral Boost/Tape, periwinkle
modulation/filter, cyan delay, magenta Creative and violet reverb. Unclassified NAM files remain neutral.
Selection brightens the border toward white, retaining the Amp-to-Cab border gradient.
Knobs and switches use a neutral monochrome palette. Module header IDs use 22-point text
and descriptions use 14-point text; the description extends across the controls area
when no NAM quality button occupies that row. Tile IDs use 16-point text. Regular UI text uses size 14; compact legends, browser
metadata and helper text use at least size 13. Plot footers have
extra height for their larger labels.

The bottom editors are independent containers with bounds enclosing their children. Their
parameter bindings remain attached to their physical slots; selection changes visibility,
never audio or automation ownership. The main card bounds do not clip the bottom panel.

## Multiple sources

Library & settings lists every source folder with its type: **NAM**, **IR** or **Presets**.
**+ NAM folder**, **+ IR folder** and **+ Preset folder** add to the matching list, and each
row has **Open** (Explorer or Finder) and **Remove**. A folder is scanned only for its list's file type (`.nam`, `.wav` or
`.ngrid`); add a mixed folder to several lists. This is the only place to add or remove
folders; the browser's **Manage folders** opens it. A `.ignore` file inside a folder
excludes files and subfolders like a `.gitignore` (see
[browser.md](browser.md#excluding-files-with-ignore)). The
status line counts files and folders per type. Up to 32 roots are retained per type in project
state and the per-user preference; the earlier combined NAM/IR list is not migrated.
Startup rescan is optional (default on); when off, use Rescan all, which indexes every list.
More than seven rows scroll. Missing sources
report an error while readable sources still populate. Canonical duplicate roots and overlapping
file paths are deduplicated; identical source basenames receive distinct menu names. Root and
nested folder names remain in the index. The dedicated [browser](browser.md) supplies
live ranked search, folder scoping, metadata details and category-specific entry points. The scanner runs on its existing background worker;
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

Library folders (`CaptureFolders`) are saved in project state.
Malformed arrays, non-string entries, embedded NULs and excessive root counts are rejected
before modifying project state. Removing a library source never deletes a file or unloads a module.

## Settings and default folders

Preferences for new instances live in `settings.json`, in the platform's per-user config folder:

| Platform | Settings folder |
| --- | --- |
| Windows | `%APPDATA%\NAM Grid` |
| macOS | `~/Library/Application Support/NAM Grid` |
| Linux | `$XDG_CONFIG_HOME/nam-grid` (default `~/.config/nam-grid`) |

It holds the NAM, IR and preset folder lists, Startup rescan, Visualizations and Preset
switch. **Settings folder** in Library & settings opens it. The folder and file are created
when missing, and updating the plugin keeps them.

Until you choose folders yourself, each list uses a default folder in your Documents, created
on first start: `Documents/NAM Grid/NAM Captures`, `IRs` and `Presets`. Each gets a `.ignore`
of comments that shows how to [hide files](browser.md#excluding-files-with-ignore); it hides
nothing until you add a pattern. The Presets folder starts with **Empty**, a preset of the
empty grid. Removing every folder from a list is
remembered as a choice, so the default does not come back. Settings from earlier builds
(`nam-grid/library.json` in the local app-data folder) are migrated once; their empty lists
receive the defaults.

The library index is a rebuildable cache, `library-index.json`, in the per-user cache folder:
`%LOCALAPPDATA%\NAM Grid` on Windows, `~/Library/Caches/NAM Grid` on macOS and
`$XDG_CACHE_HOME/nam-grid` (default `~/.cache/nam-grid`) on Linux. For every indexed file it
keeps the size, modification time and what a scan reads: name, maker, model, gear type and
content hash. A rescan still walks your folders, so new, moved and deleted files are found,
but it opens and hashes only files whose size or time changed, so a rescan of a large library
is nearly instant. Deleting the file is safe; the next scan rebuilds it.

`NAM_GRID_HOME` replaces all three locations: settings then live in `$NAM_GRID_HOME/config`,
the index in `$NAM_GRID_HOME/cache` and the default folders in `$NAM_GRID_HOME`. The tests
use it so they never touch your own settings.

## Tuner and performance controls

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
controls for audible-effect guidance. See [modules.md](modules.md) for controls and [development.md](development.md) for parameter ownership.

Grid toggles have equal 12-pixel top/right insets and a 32-pixel hit area. Bypassed cards use a darker background, subdued text/badges and an unfilled toggle with a minus mark.

Empty slots are inactive add targets. They show no bypass toggle on the card and no Enabled or previous/next controls in the bottom editor; click + Add module or the module picker to fill one.

## Copy, move and swap

Right-click a card to Copy, Move or Swap with another slot. Copy/Move replace
the destination; Move empties the source. Controls, bypass and model quality
travel together, while files stay on disk and automation stays attached to the
physical slots. Both processors are prepared before an edit commits. A missing
or invalid resource rejects the edit and keeps both slots unchanged.

## Reading visualizations

Measured IN/OUT histories describe level, not stereo correlation;
mono-sum metering can cancel opposite-polarity stereo. Parameter sketches describe
configured behavior, not an invented live LFO phase or a measured room impulse.
A bypassed module can still show its settings; Enabled controls the audio.
The output spectrum in the EQ / Filter, Outboard Channel and NAM views is a 4096-sample FFT of the selected slot's left output channel, updated with the meter. Each band shows its loudest bin. Its filled area uses a -72 to 0 dBFS scale, labelled on the right and separate from the response curve's dB scale. Bins are about 12 Hz wide at 48 kHz, so the lowest octave is still coarse, and it is not a stereo analyzer.
Measured IN > OUT clouds use the same peak meters as the histories: they show level behavior, not instantaneous waveform transfer.

Frequency plots use logarithmic frequency with labeled limits. Timing sketches
state their window; reverb guides use nominal decay or explicitly relative scales.
Native Airwindows intensity/size controls must not be presented as calibrated RT60.
Distinguish nonlinear transfer curves from small-signal EQ responses and measured
gain reduction. Source pins and mappings are in the implementation notes.

The global Visualizations preference hides plots without changing audio or saved
rig controls. For native screenshot sweeps and editor checks, use [build.md](build.md).
