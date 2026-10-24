# NAM Grid 0.1.1

A small polish update to [0.1.0](https://github.com/Fannon/nam-grid/releases/tag/v0.1.0). Just replace the plugin files; your rigs and projects keep working as they are.

## What's new

- **Tidier header:** every box has the same height and outline, the rig preset name has more room for longer names, and **Modulation** now sits next to **Tuner**.
- **Compact module editors:** modules with only a few controls keep them together, filled from the left, instead of spreading them across both rows.
- **Clearer signal path:** the IN and OUT lines now reach their knobs, and the stereo meters have a little more space next to the first and last modules.
- **Help at hand:** Library & settings links to the documentation, the source code and the issue tracker.

## Downloads

- `nam-grid-windows-x64.zip`: CLAP and VST3 for 64-bit Windows.
- `nam-grid-macos-arm64.zip`: CLAP and VST3 for Apple Silicon Macs (macOS 11 or newer). The Mac build isn't signed yet, so macOS will ask you to confirm it once — see the README.
- `nam-grid-linux-x64.zip`: CLAP and VST3 for 64-bit Linux (Ubuntu 22.04 or newer works well). Linux is still an early preview.

New to NAM Grid? Everything is included to make sound right away: one clean amp-and-cab capture (RiffSmith Clean) and two starter rigs, Empty and Clean Default. The [README](https://github.com/Fannon/nam-grid#readme) gets you installed, and the [module guide](https://github.com/Fannon/nam-grid/blob/main/docs/modules/README.md) covers every module.

## Good to know

- Windows and Mac are well tested; Linux works but has seen less time in real DAWs, so start with a moderate buffer size and see how it feels.
- There's no Audio Unit, AAX or standalone app in this release.
- Each download includes license notes (`ThirdPartyNotices.txt` and `notices/`) — just keep them alongside the plugins if you pass them on.
- Something not right? Tell us your OS, DAW, plugin format, sample rate and buffer size at <https://github.com/Fannon/nam-grid/issues>.
