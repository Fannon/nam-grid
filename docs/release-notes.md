# NAM Grid 0.2.1

A maintenance update for 0.2.0, with compatibility fixes, more reliable Pedal Drive insertion and clearer browsing and modulation controls. Replace the plugin files; rigs, projects and automation keep working.

## Fixed

- **Projects from 0.2.0 preview builds load again.** Those previews used state schema 5 before the second modulation bank existed. 0.2.1 gives the bank its own schema and fills in its defaults when opening those files.
- **The library index survives restarts.** With **Startup rescan** off, NAM Grid no longer replaces the saved index with an empty one.
- **Cancelling an active library scan keeps the index.** A cancelled scan no longer starts again or publishes stale results.
- **Browser search is ready for typing.** Opening Add module, a module's browser or the preset browser focuses search immediately.
- **Second-bank Via labels display correctly** for LFO 2, Env 2 and Seq 2.

## Changed

- **Pedal Drive adds no reported latency.** Selecting it no longer changes the plugin's latency, avoiding host rebuilds that could undo the selection. Stacked pedals no longer add 64 samples each. Its minimum-phase antialiasing filters preserve bandwidth and alias rejection; partial Mix and Low keep settings can sound slightly different because of the phase change.
- **Pedal Drive uses about 20% less CPU on Apple Silicon** through vectorized oversampling filters.
- **Grid cards and the module header show the selected mode**, including Pedal Drive style and Cosmic Echo topology.
- **Modulation uses compact knobs and aligned controls.** Source menus are alphabetical, and a renamed macro keeps its number, such as **Macro 1: Test**.
- **Documentation:** full-width navigation on every page, a linked Module guide section, a cleaner home page, and a link to the KVR Audio discussion thread.

## Downloads

- `nam-grid-windows-x64.zip`: CLAP and VST3 for 64-bit Windows.
- `nam-grid-macos-arm64.zip`: CLAP and VST3 for Apple Silicon Macs (macOS 11 or newer). The Mac build isn't signed yet, so macOS will ask you to confirm it once; see [allow the plugin](https://fannon.github.io/nam-grid/getting-started#macos-allow-the-plugin).
- `nam-grid-linux-x64.zip`: CLAP and VST3 for 64-bit Linux (Ubuntu 22.04 or newer works well). Linux is still an early preview.

Existing 0.1.x projects, presets and automation still load unchanged. Pedal Drive and the numbered modulation sources keep their 0.2.0 settings.

## Good to know

- Windows and Mac are well tested; Linux works but has seen less time in real DAWs, so start with a moderate buffer size and see how it feels.
- There's no Audio Unit, AAX or standalone app in this release.
- Each download includes license notes (`ThirdPartyNotices.txt` and `notices/`) — just keep them alongside the plugins if you pass them on.
- Something not right? Tell us your OS, DAW, plugin format, sample rate and buffer size at <https://github.com/Fannon/nam-grid/issues>, or use the [KVR Audio discussion thread](https://www.kvraudio.com/forum/viewtopic.php?t=634501).
