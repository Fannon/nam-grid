# nam-grid preview

A fixed 16-slot guitar effects grid with NAM captures, cabinet IRs, parallel lanes,
rig presets, a tuner, and built-in effects. See the
[README](https://github.com/Fannon/nam-grid#readme) and
[build guide](https://github.com/Fannon/nam-grid/blob/main/docs/BUILD.md) for setup and usage.

The release archives contain the CLAP and VST3 plugins plus third-party notices.
Retain the notices when copying or redistributing the plugins. Model captures
and impulse responses are external files and are not included.

This is preview software. The automated Windows tests exercise DSP, state recall,
CLAP hosting, and the native editor. Real DAW listening and low-buffer sessions,
VST3 host validation, and macOS validation are required before claiming stable
support. A successful CI build alone does not establish those outcomes.

Keep the existing plugin IDs when upgrading: they identify nam-grid in saved
projects. Remove duplicate copies of older grid binaries from plugin scan paths.
This fork does not supply the upstream NAM installers, standalone application,
Audio Unit, or Linux LV2 plugin.
