# NAM Grid 0.1.0

The first release of NAM Grid: a 16-slot guitar rig in one CLAP / VST3 plugin.
Combine Neural Amp Modeler captures, cabinet IRs and 54 built-in effects across
four Pre slots, two parallel four-slot lanes with A/B blend, and four Post slots.
It includes rig presets, a library browser, a tuner, macros, modulation and
performance scenes. See the [README](https://github.com/Fannon/nam-grid#readme)
for installation and a first rig, and the
[module guide](https://github.com/Fannon/nam-grid/blob/main/docs/modules/README.md) for
every module's controls.

## Downloads

- `nam-grid-windows-x64.zip`: CLAP and VST3 for 64-bit Windows.
- `nam-grid-macos-arm64.zip`: CLAP and VST3 for Apple Silicon Macs. These builds are
  not code-signed or notarized; follow the README to clear the download quarantine.
- `nam-grid-linux-x64.zip`: CLAP and VST3 for 64-bit Linux (glibc 2.35 or newer, such as
  Ubuntu 22.04+, Debian 12+ or Fedora 36+), with an X11 editor that also runs under
  XWayland. Linux support is a preview: see Known limits.

Each archive includes `ThirdPartyNotices.txt` and `notices/`; keep them with the
plugins when redistributing. The plugin brings one clean amp-and-cab capture (RiffSmith
Clean, MIT) and the Empty and Clean Default rig presets, written into its new library
folders on first start. Other NAM captures and impulse responses are external files.

## Compatibility

0.1.0 establishes NAM Grid's permanent plugin identity (CLAP
`io.github.fannon.nam-grid`, vendor Simon Heimler) and parameter layout. Projects,
automation and `.ngrid` presets from earlier preview builds do not load and must be
recreated; remove old preview binaries from your plugin folders and rescan. From
0.1.0 onward, new versions keep existing parameters, modules and saved rigs
compatible.

## Known limits

Automated Windows tests exercise DSP, state recall, CLAP hosting and the native
editor; macOS CI builds and runs the non-editor tests. Linux builds pass the same DSP,
state and contract tests and an X11 editor check, but have had little testing in real
Linux DAWs; the editor uses a community iPlug2 port. Use moderate buffer sizes
first and report problems with your OS, DAW, plugin format, sample rate and buffer
size at <https://github.com/Fannon/nam-grid/issues>. Audio Unit, AAX and a standalone
application are not provided.
