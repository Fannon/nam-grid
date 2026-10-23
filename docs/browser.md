# Library browser

Click a grid tile or the browser icon beside the current module name. Empty cells open
**Modules**; loaded effects open their FX category with the current effect selected.
NAM and IR cells open their own category and current folder, selecting the exact file
even if another library entry has the same name or content. The tile's bypass switch
and right-click Clear / Copy / Move / Swap menu remain direct actions.

Click the header preset name to open **Presets** only. A cell browser offers Modules,
NAM captures and IR files, and cannot load a complete rig. **Library & settings > Browse
library** opens all categories, targeting the currently selected cell for NAM, IR and FX.

The page has a folder/category list, results and file details. Choose a folder to search
it and its subfolders, or choose **All folders / All categories**. The existing file
or FX category is focused when opening from a tile or browser icon. A current file
outside configured roots gets a temporary background scan of its folder, without
adding that folder to the library.

Hover the browser title, search field or file-details header for usage hints,
search syntax and full paths. FX details include a short sound description and control
summary. Settings guidance and calibration explanations
are available on the corresponding headers and controls as tooltips.

Search updates while typing. It matches all words across filenames, full paths and
available NAM metadata (name, maker, model and gear); preset names are also searchable.
Common Latin accents fold, so `grun` finds `Grün`. Examples:

- `vox clean` matches both terms, even in different fields.
- `"silver clean" -bright` requires the phrase and excludes bright.
- `maker:vox model:ac30` limits terms to those metadata fields.
- `path:"amp heads" -name:test` searches a folder phrase while excluding test filenames.

`name:` means filename; `gear:` is also available for NAM metadata. Unclosed quotes
are tolerated while typing. Relevance sorting prioritizes exact filename matches,
then filename prefixes, substrings, and metadata/path matches. **Sort: name** switches
to alphabetical order. NAM also has an **All gear** filter. Results use single-line
rows; FX categories appear alongside their names. File details retain the full path
and metadata to distinguish identical filenames.

The FX, NAM and IR browsers leave the parameter editor and visualization visible below them.
Selecting a result auditions its sound; adjust the parameters below while comparing
FX, NAM captures or IRs. Each choice remembers its edits during this visit. A new FX
starts with its musical defaults; NAM/IR files of the same kind retain gain settings
for comparison. The current FX keeps its edits when the browser opens.

Up/Down, Page Up/Down and Home/End move the selection. Enter or double-click accepts
the auditioned module and its parameters. Closing the browser
or pressing Escape restores the original module and parameters. Project autosaves and
rig exports retain the original slot until acceptance. Closing the plugin editor also
cancels the audition. Load failures leave the browser open with the error.

Enter finishes an active search edit first; press Enter again to accept. Ctrl/Cmd+F
focuses search. Escape cancels an active search edit, then closes the browser.
Rig presets use a full-height inspection layout; selecting one changes audio only
after Enter or double-click. Switching to Presets cancels any slot audition.

**+ Add folder** in NAM or IR adds a shared capture source. In Presets it adds a
separate preset source. **Remove** removes the selected configured root from the
library, leaving its files on disk; select a root rather than a nested folder to enable
it. Each source list supports 32 roots. Both lists persist in project state and the
per-user library preference. Rig presets do not replace these library choices.
Current folders outside the library are temporary and cannot be removed as sources.

Scans recurse in the background, ignore directory links, deduplicate overlapping paths
and retain separate copies of identical captures. Preset sources index `.ngrid` files
with a valid, bounded preset envelope; invalid files count as unreadable. IR decoding
and full rig/DSP validation happen on load. **Rescan** refreshes external filesystem
changes; the existing startup rescan preference applies to all sources.

The layout uses the familiar folder/results/info structure of preset browsers such as
u-he's Diva, with a smaller feature set. File kinds and caller permissions are explicit;
a future saved FX-slot preset kind is reserved separately from today's built-in Modules.
