# NAM Grid for Akai MPC

The MPC standalone plugins (Amp, Pedals, Delay, Cosmic Echo, Lo-Fi, Mod, Console), built with the
[inMusic MPC plugin SDK](https://github.com/inmusic-software/mpc-plugin-builder-sdk)
(`../mpc-plugin-sdk`). Installation, library folders, pages and limits are in
[docs/mpc.md](../docs/mpc.md); build and test details in
[docs/build.md](../docs/build.md#mpc).

```sh
python mpc/build.py          # build/<platform>/mpc/out/Simon Heimler - VST - NG <Name>/
python mpc/build.py --test   # MPC plugin tests and the SDK host simulator
python mpc/build.py --target native --gallery tmp/mpc-gallery   # headless SVG previews
```

| Path | Contents |
| --- | --- |
| `plugins/<name>/` | One plugin: its section spec and programs (`*.cpp`), `plugin.json` (name, uid, binary; permanent identity) and `pages.json` (one page per section). |
| `src/MPCEngine.h` | The shared plugin: parameters, state, library, worker thread and audio. |
| `src/MPCLayout.h` | Knob mapping, curves and value text of NAM Grid modules. |
| `src/MPCLibrary.h` | The fixed capture and IR folders. |
| `tests/mpc_tests.cpp` | Behaviour tests of every plugin through the VST2 entry point. |
| `tests/fixtures/` | Reviewed parameter, option-assignment and saved-state compatibility baselines. Never regenerate them to silence a failure. |
| `tests/inspect.cpp` and `gallery.py` | Live parameter snapshots and a deterministic SVG gallery, using the generated skin geometry and assets. |
| `build.py` | Cross-compiles with zig, lays out the pages, generates skins and validates each plugin folder. |
| `skins.py` | The SDK's skin generator with a panel, heading and fitted Q-Link focus rectangle per column. |
| `nam_kit.py` | Knobs, faders, toggles and selector buttons drawn like the desktop editor, replacing the SDK kit's PNGs. |
