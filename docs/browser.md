# Library browser

Find effects, NAM captures, cabinet IRs, and rig presets in one place. You can listen before deciding: slot choices are auditions until you apply them.

![Library browser showing modules](screenshots/browser-modules.png)

## Open the right browser

- Click **+ Add module** or a module name to choose something for that slot.
- Click the browser icon in a module editor to jump to its current category or folder.
- Click the preset name in the header to browse complete rigs.
- Open **Library & settings > Browse library** to search all sound content.

The slot browser offers **Modules**, **NAM captures**, and **IR files**. The preset browser handles complete `.ngrid` rigs; it does not change individual slots.

## Browse and audition

Choose a folder or category, then select a result. The details pane describes the selection and shows capture metadata or a module's controls.

For modules, captures, and IRs:

1. Select a result to hear it in the current slot.
2. Adjust the module below the browser while comparing.
3. Choose **Apply**, press Enter twice from search, or double-click to keep it.
4. Choose **Cancel** or press Escape to restore the original module and settings.

Each effect you audition keeps its temporary adjustments during that visit. Capture and IR changes retain comparable gain settings. Closing the plugin window also cancels an unapplied audition.

Use **Open file...** for a capture, IR, or preset outside your library. This does not add its folder as a permanent source.

## Search

Search matches filenames, paths, preset names, and available NAM metadata. All ordinary words must match; common Latin accents are ignored, so `grun` finds `Grün`.

| Example | Finds |
| --- | --- |
| `vox clean` | Items containing both words |
| `"silver clean" -bright` | That phrase, excluding `bright` |
| `maker:vox model:ac30` | Matching capture maker and model |
| `path:"amp heads" -name:test` | A folder phrase, excluding test filenames |

Use `name:` for filenames and `gear:` for capture type. Relevance sorting puts exact filename matches first; **Sort: name** switches to alphabetical order.

**Ctrl/Cmd+F** focuses search. Use Up/Down, Page Up/Down, Home/End to move through results. Enter accepts a choice; while editing search, the first Enter finishes the edit and the second accepts. Escape first leaves search editing, then cancels the browser.

## Presets

Preset choices do not change your sound until you choose **Load**, press Enter, or double-click. **Save as...** creates a rig file; **Delete...** permanently removes the selected preset after confirmation. See [Rig presets](presets.md).

## Manage folders

NAM, IR, and preset sources are separate lists. Add or remove them in **Library & settings**; **Manage folders** in the browser opens the same page. **Rescan** refreshes external changes.

Folders are scanned only for their own file type: `.nam`, `.wav`, or `.ngrid`. Folder choices are saved per user, not inside rigs or DAW projects. Missing or invalid files are reported without silently substituting another sound.

## Excluding files with `.ignore`

Put a `.ignore` text file in a library folder or subfolder to hide files from scans, browsing, and previous/next navigation. Its patterns work like `.gitignore`. On Windows, `.ignore.txt` is accepted too. Rescan after editing it.

```gitignore
# Hide work folders and exports
Catalog/
_Export/

# Hide direct-input captures, except one
*DI.nam
!Keep DI.nam

# Hide only this top-level file
/notes.nam
```

- One pattern per line; blank lines and `#` comments are ignored.
- `*` matches within a name, `?` one character, and `**` any number of folders.
- A trailing `/` matches folders only. A leading or middle `/` anchors the rule to the `.ignore` folder.
- `!` re-includes a match from an earlier rule. Deeper `.ignore` files and later rules take priority.
- Matching ignores letter case. Escape a leading `#` or `!` with `\`.

Rules apply only inside the configured library folder containing the `.ignore` file. A file above that folder is not read.
