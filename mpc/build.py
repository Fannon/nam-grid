#!/usr/bin/env python3
"""Build the NAM Grid MPC plugins into deployable "Simon Heimler - VST - <Name>" folders.

Usage:
    python mpc/build.py [--target all|native|arm64|linux64] [--plugin amp ...] [--test]
                        [--arm-tests arm64 linux64] [--emulate] [--jobs N] [--debug] [--verbose]

Each folder under mpc/plugins/ is one plugin (plugin.json, pages.json and its source).
Output goes to build/<platform>/mpc/:
    native/   libraries for this computer, tested with the SDK's host simulator
    obj/      object files per target (incremental, with dependency files)
    out/Simon Heimler - VST - <Name>/   the folders to copy into <drive>/VST2 Plugins/

The SDK's own build.py builds one folder of C++17 sources. These plugins need C++20, the
shared DSP sources and the upstream dependencies, so this script drives zig directly with
the SDK's 64-bit target and flags, then reuses the SDK's skin generator and validator.
"""

import argparse
import concurrent.futures
import glob
import json
import os
import platform
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MPC = os.path.join(ROOT, "mpc")
SDK = os.path.join(ROOT, "mpc-plugin-sdk")
SDK_TOOLS = os.path.join(SDK, "tools")
sys.path.insert(0, SDK_TOOLS)
import build as sdk  # noqa: E402  (the SDK's build.py: zig discovery and version.xml)
sys.path.insert(0, os.path.join(ROOT, "mpc"))
import skins  # noqa: E402  (the SDK's gen_ui.py with grouped columns)

PLATFORM = "windows" if os.name == "nt" else "macos" if sys.platform == "darwin" else "linux"
BUILD = os.path.join(ROOT, "build", PLATFORM, "mpc")
OUT = os.path.join(BUILD, "out")

SHARED_SOURCES = [os.path.join(ROOT, "NeuralAmpModeler", "AdaptiveIR.cpp"),
                  os.path.join(ROOT, "NeuralAmpModeler", "third_party", "fftconvolver", "FFTConvolver.cpp"),
                  os.path.join(ROOT, "NeuralAmpModeler", "third_party", "fftconvolver", "AudioFFT.cpp"),
                  os.path.join(ROOT, "NeuralAmpModeler", "third_party", "fftconvolver", "Utilities.cpp"),
                  os.path.join(ROOT, "AudioDSPTools", "dsp", "dsp.cpp"),
                  os.path.join(ROOT, "AudioDSPTools", "dsp", "RecursiveLinearFilter.cpp"),
                  os.path.join(ROOT, "AudioDSPTools", "dsp", "ImpulseResponse.cpp"),
                  os.path.join(ROOT, "AudioDSPTools", "dsp", "wav.cpp")]
for _folder in ("NAM", os.path.join("NAM", "wavenet")):
    _d = os.path.join(ROOT, "NeuralAmpModelerCore", _folder)
    SHARED_SOURCES += sorted(os.path.join(_d, f) for f in os.listdir(_d) if f.endswith(".cpp"))

INCLUDES = [os.path.join(MPC, "src"), os.path.join(SDK, "framework"), os.path.join(ROOT, "NeuralAmpModeler"),
            os.path.join(ROOT, "eigen"), os.path.join(ROOT, "NeuralAmpModelerCore", "Dependencies", "nlohmann"),
            os.path.join(ROOT, "iPlug2", "IPlug"), os.path.join(ROOT, "iPlug2", "WDL")]
DEFINES = ["NAM_ENABLE_A2_FAST", "NOMINMAX", "EIGEN_RUNTIME_NO_MALLOC", "NDEBUG"]
# -fsigned-char: WDL requires a signed char, which ARM does not use by default.
COMMON = ["-std=c++20", "-fPIC", "-fsigned-char", "-fvisibility=hidden", "-fvisibility-inlines-hidden",
          "-ffunction-sections", "-fdata-sections", "-Wno-deprecated-declarations", "-Wno-unused-value",
          "-Wno-switch", "-Wno-deprecated-enum-enum-conversion"]

FACTORY_CAPTURE = [(os.path.join(ROOT, "NeuralAmpModeler", "resources", "defaults", "riffsmith-clean.nam"),
                    os.path.join("Content", "Captures", "riffsmith-clean.nam")),
                   (os.path.join(ROOT, "NeuralAmpModeler", "resources", "defaults", "riffsmith-LICENSE.txt"),
                    os.path.join("Content", "Captures", "riffsmith-LICENSE.txt"))]


VERBOSE = False


def native_target():
    if os.name != "nt":
        return []
    return ["-target", "aarch64-windows-gnu" if platform.machine().lower() in ("arm64", "aarch64") else "x86_64-windows-gnu"]


TARGET_FLAGS = {
    "native": native_target(),
    "arm64": ["-target", "aarch64-linux-gnu.2.31", "-mcpu=cortex_a55"],
    # Diagnostics: the MPC binary's Linux/glibc setup on x86-64, runnable in WSL or CI.
    "linux64": ["-target", "x86_64-linux-gnu.2.31"],
}
# Emulators for --emulate: qemu-user with the Ubuntu cross C library.
EMULATORS = {"arm64": ["qemu-aarch64", "-L", "/usr/aarch64-linux-gnu"], "linux64": []}


class Plugin:
    def __init__(self, folder):
        self.dir = folder
        self.id = os.path.basename(folder)
        with open(os.path.join(folder, "plugin.json"), encoding="utf-8") as fh:
            self.manifest = json.load(fh)
        with open(os.path.join(folder, "pages.json"), encoding="utf-8") as fh:
            self.pages = json.load(fh)
        self.name = self.manifest["name"]
        self.slug = os.path.basename(self.manifest["file64"]).replace("-arm64.so", "")
        self.folder_name = "%s - VST - %s" % (self.manifest["manufacturer"], self.name)
        self.out = os.path.join(OUT, self.folder_name)
        self.sources = sorted(glob.glob(os.path.join(folder, "*.cpp")))
        self.work = os.path.join(BUILD, self.slug)
        self.uses_captures = self.id == "amp"

    def library(self, target):
        if target == "native":
            ext = ".dll" if os.name == "nt" else ".dylib" if sys.platform == "darwin" else ".so"
            return os.path.join(BUILD, "native", self.slug + ext)
        if target == "arm64":
            return os.path.join(self.out, "VST", self.slug + "-arm64.so")
        return os.path.join(BUILD, target, self.slug + ".so")


def all_plugins():
    return [Plugin(d) for d in sorted(glob.glob(os.path.join(MPC, "plugins", "*"))) if os.path.isdir(d)]


def log(msg=""):
    print(msg, flush=True)


def run(argv, env=None, timeout=None):
    p = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env, timeout=timeout)
    return p.returncode, p.stdout.decode("utf-8", errors="replace")


def object_path(target, source):
    rel = os.path.relpath(source, ROOT).replace(os.sep, "_").replace(":", "_")
    return os.path.join(BUILD, "obj", target, rel + ".o")


def up_to_date(obj, dep):
    if not (os.path.isfile(obj) and os.path.isfile(dep)):
        return False
    stamp = os.path.getmtime(obj)
    with open(dep, "r", encoding="utf-8", errors="replace") as fh:
        text = fh.read().replace("\\\n", " ")
    # "target: deps"; Windows drive letters ("C:\...") have no space after their colon.
    _, _, deps = text.partition(": ")
    files, current = [], ""
    for token in deps.split():
        # Make depfiles escape spaces with a backslash.
        if token.endswith("\\") and not token.endswith("\\\\"):
            current += token[:-1] + " "
            continue
        files.append(current + token)
        current = ""
    return all(os.path.isfile(f) and os.path.getmtime(f) <= stamp for f in files)


def is_elf(target):
    return target != "native" or PLATFORM == "linux"


def compile_sources(zig, target, sources, args, label, extra_defines=(), obj_target=None):
    """Compiles changed sources to objects; returns False on failure."""
    opt = ["-O0", "-g"] if args.debug else ["-O2"]
    base = zig + ["c++"] + TARGET_FLAGS[target] + COMMON + opt + ["-I" + i for i in INCLUDES]
    base += ["-D" + d for d in list(DEFINES) + list(extra_defines)]
    if is_elf(target):
        base.append("-D_LARGEFILE64_SOURCE")  # readdir64/stat64 for the library scan
    jobs = []
    for source in sources:
        obj = object_path(obj_target or target, source)
        dep = obj[:-2] + ".d"
        if up_to_date(obj, dep):
            continue
        os.makedirs(os.path.dirname(obj), exist_ok=True)
        jobs.append((source, base + ["-c", source, "-o", obj, "-MMD", "-MF", dep]))
    if jobs:
        log("[%s] compiling %d of %d %s sources" % (target, len(jobs), len(sources), label))
    failures = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for (source, _), (code, out) in zip(jobs, pool.map(lambda j: run(j[1]), jobs)):
            if code != 0:
                failures.append((source, out))
            elif VERBOSE and out.strip():
                log(out.rstrip())
    for source, out in failures:
        log("ERROR compiling %s:\n%s" % (os.path.relpath(source, ROOT), out.rstrip()))
    return not failures


def link(zig, target, objects, output, args, shared=True):
    os.makedirs(os.path.dirname(output), exist_ok=True)
    argv = zig + ["c++"] + TARGET_FLAGS[target] + (["-shared"] if shared else []) + ["-o", output] + objects
    if shared and is_elf(target):
        # -z nodelete: zig's shared-library startup does not run __cxa_finalize on dlclose, so
        # static destructors registered by the C++ code would run from unmapped memory when the
        # host exits (a crash after the plugin is removed). Keep the library mapped after unload.
        argv.append("-Wl,-z,nodelete")
    if shared and target != "native":
        argv += ["-Wl,--gc-sections", "-Wl,--version-script=" + os.path.join(SDK_TOOLS, "exports.map")]
        if not args.debug:
            argv.append("-Wl,-s")
    code, out = run(argv)
    if code != 0:
        log("ERROR linking %s:\n%s" % (os.path.relpath(output, ROOT), out.rstrip()))
        return False
    return True


def build_target(zig, target, plugins, args):
    if not compile_sources(zig, target, SHARED_SOURCES, args, "shared"):
        return False
    shared = [object_path(target, s) for s in SHARED_SOURCES]
    for plugin in plugins:
        if not compile_sources(zig, target, plugin.sources, args, plugin.name):
            return False
        library = plugin.library(target)
        if not link(zig, target, [object_path(target, s) for s in plugin.sources] + shared, library, args):
            return False
        log("[%s] %s (%s)" % (target, os.path.relpath(library, ROOT), sdk.human_size(library)))
    return True


def host_sim(zig, target="native"):
    suffix = "" if target == "native" else "-" + target
    exe = os.path.join(BUILD, "tools", "host_sim" + suffix + (".exe" if os.name == "nt" and target == "native" else ""))
    src = os.path.join(SDK_TOOLS, "host-sim", "host_sim.cpp")
    if os.path.isfile(exe) and os.path.getmtime(exe) >= os.path.getmtime(src):
        return exe
    os.makedirs(os.path.dirname(exe), exist_ok=True)
    code, out = run(zig + ["c++", "-std=c++17", "-O2"] + TARGET_FLAGS[target] + ["-I" + os.path.join(SDK, "framework"),
                    "-I" + os.path.dirname(src), "-o", exe, src])
    if code != 0:
        log("ERROR building host_sim:\n" + out)
        return None
    return exe


def plugin_env(plugin, **extra):
    env = dict(os.environ)
    env["MPC_PLUGIN_FOLDER"] = plugin.out  # The native library finds Content/ in the assembled folder.
    env.update(extra)
    return env


def dump_params(sim, plugin):
    """Writes params.json: names as reported in static-name mode, menus from the plugin's manifest."""
    os.makedirs(plugin.work, exist_ok=True)
    manifest_path = os.path.join(plugin.work, "menus.json")
    if os.path.isfile(manifest_path):
        os.remove(manifest_path)
    p = subprocess.run([sim, plugin.library("native"), "dump"], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       env=plugin_env(plugin, NAM_GRID_MPC_STATIC_NAMES="1", NAM_GRID_MPC_MANIFEST=manifest_path),
                       timeout=300)
    if p.returncode != 0:
        log(p.stderr.decode("utf-8", errors="replace"))
        log("ERROR: host_sim dump failed for %s (exit %d)" % (plugin.name, p.returncode))
        return None
    data = json.loads(p.stdout.decode("utf-8", errors="replace"))
    with open(manifest_path, encoding="utf-8") as fh:
        manifest = json.load(fh)
    kinds = manifest["kinds"]
    plugin.show_output = manifest["showOutput"]
    # host_sim infers kinds from the current display text: it misses menus over 16 entries and
    # takes module-dependent controls for fixed lists. The plugin's own description wins.
    for prm in data["params"]:
        kind = kinds.get(str(prm["index"]))
        if kind == "float":
            prm["kind"], prm["choices"] = "float", []
        elif kind is not None:
            prm["kind"], prm["choices"] = "choice", kind
    path = os.path.join(plugin.work, "params.json")
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(data, fh, indent=2)
        fh.write("\n")
    return path


def widget(name, params):
    """The control for a parameter, by its static name and kind."""
    if name == "empty":
        return "empty"
    p = params[name]
    if name.endswith(" Type"):
        return {"param": name, "widget": "slider", "overlay": "menu"}
    # Settings that always mean the same thing and have two to four states are switches.
    if p["kind"] in ("bool", "choice") and 2 <= len(p["choices"]) <= 4:
        return {"param": name, "widget": "switch"}
    if " Knob " in name:
        return {"param": name, "widget": "knob", "polarity": "unipolar"}
    if name == "Output":
        return {"param": name, "widget": "fader"}
    return {"param": name, "widget": "slider"}


def section_columns(section, names, show_output):
    """A section's page: main knobs, more knobs, choosing, options. Each column is a Q-Link group."""
    knobs = ["%s Knob %d" % (section, k) for k in range(1, 9)]
    select = [n for n in ("%s Type" % section, "%s On" % section, "%s Folder" % section, "%s File" % section) if n in names]
    options = ["%s Opt %d" % (section, o) for o in range(1, 4) if "%s Opt %d" % (section, o) in names]
    extras = (["Output"] if show_output else []) + (["Rescan"] if "%s Folder" % section in names else [])
    for column in (options, select):
        while extras and len(column) < 4:
            column.append(extras.pop(0))
    if extras:
        raise SystemExit("ERROR: the %s page has no room for %s" % (section, extras))
    columns = [knobs[:4], knobs[4:], select, options]
    third = "FILE" if "%s Folder" % section in select else "SELECT" if select else ""
    # The first heading names the module, voice or style the section is playing.
    first = "@%s Type" % section if "%s Type" % section in names else section.upper()
    headings = [first, "", third, "OPTIONS" if options else ""]
    return columns, headings


def page_layout(page, names, show_output):
    """(controls in reading order, column headings) for one page of pages.json."""
    if "columns" in page:
        columns = [list(c) for c in page["columns"]]
        headings = list(page.get("headings", [""] * 4))
    else:
        columns, headings = section_columns(page["section"], names, show_output)
    columns = [[n if n in names else "empty" for n in c] + ["empty"] * (4 - len(c)) for c in columns]
    columns += [["empty"] * 4] * (4 - len(columns))
    if any(len(c) > 4 for c in columns) or len(columns) > 4:
        raise SystemExit("ERROR: page %s has more than 4 x 4 controls" % page["name"])
    headings = (headings + [""] * 4)[:4]
    headings = [h if any(n != "empty" for n in columns[i]) else "" for i, h in enumerate(headings)]
    return [widget(columns[c][r], names) for r in range(4) for c in range(4)], headings


def make_skins(plugin, params_json):
    with open(params_json, encoding="utf-8") as fh:
        names = {p["name"]: p for p in json.load(fh)["params"]}
    pages, headings = [], {}
    for page in plugin.pages["pages"]:
        controls, page_headings = page_layout(page, names, plugin.show_output)
        pages.append({"name": page["name"], "controls": controls})
        headings[page["name"]] = page_headings
    ui_path = os.path.join(plugin.work, "ui.json")
    with open(ui_path, "w", encoding="utf-8", newline="\n") as fh:
        json.dump({"kit": plugin.pages.get("kit", "amber"), "pages": pages}, fh, indent=2)
        fh.write("\n")
    out = os.path.join(plugin.out, "Plugin Skins")
    if os.path.isdir(out):
        shutil.rmtree(out)
    handles = {name: "Parameter %d" % p["index"] for name, p in names.items()}
    if not skins.generate(params_json, ui_path, out, BUILD, headings, handles):
        log("ERROR: gen_ui.py failed for %s" % plugin.name)
        return False
    # The generated files keep duplicate JSON keys, so they are edited as text.
    tui = os.path.join(out, "TUI.json")
    with open(tui, "r", encoding="utf-8") as fh:
        text = fh.read()
    # Knobs and options a module does not use report an empty name: hide them.
    text = text.replace('"showWhenDataModelInvalid": "Hide"', '"showWhenDataModelInvalid": "HideOnInvalidName"')
    with open(tui, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)
    return True


def validate(plugin):
    """The SDK validator, accepting the deliberate absence of a 32-bit binary."""
    code, out = run([sys.executable, os.path.join(SDK_TOOLS, "validate.py"), plugin.out, "--params",
                     os.path.join(plugin.work, "params.json")])
    errors = [line for line in out.splitlines() if line.startswith("ERROR")]
    # Only 64-bit MPCs are supported: without file32, 32-bit MPCs ignore the plugin.
    unexpected = [line for line in errors if "file32" not in line]
    if unexpected or (code != 0 and not errors):
        log(out.rstrip())
        log("ERROR: validate.py reported problems with %s (see above)." % plugin.folder_name)
        return False
    warnings = [line for line in out.splitlines() if line.startswith("WARN")]
    log("[validate] %s: ok%s" % (plugin.folder_name, "" if not warnings else " (%d warnings)" % len(warnings)))
    for line in warnings:
        log("  " + line)
    return True


def assemble(plugin):
    os.makedirs(plugin.out, exist_ok=True)
    shutil.copyfile(os.path.join(plugin.dir, "plugin.json"), os.path.join(plugin.out, "plugin.json"))
    if plugin.uses_captures:
        for src, rel in FACTORY_CAPTURE:
            dst = os.path.join(plugin.out, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copyfile(src, dst)
    shutil.copyfile(os.path.join(ROOT, "ThirdPartyNotices.txt"), os.path.join(plugin.out, "ThirdPartyNotices.txt"))
    shutil.copyfile(os.path.join(ROOT, "LICENSE"), os.path.join(plugin.out, "LICENSE.txt"))


def run_host_test(sim, plugin, seconds, target="native", runner=()):
    wav = os.path.join(plugin.work, "test%s.wav" % ("" if target == "native" else "-" + target))
    log("[%s] host_sim test %s (%g s, 128-frame blocks)" % (target, plugin.name, seconds))
    p = subprocess.run(list(runner) + [sim, plugin.library(target), "test", "--seconds", str(seconds), "--wav", wav,
                       "--playing"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=plugin_env(plugin),
                       timeout=3600)
    lines = [line for line in p.stdout.decode("utf-8", errors="replace").rstrip().splitlines()
             if "has an empty name" not in line]
    if p.returncode != 0 or VERBOSE:
        log(os.linesep.join(lines))
    else:
        log("  " + next((line for line in reversed(lines) if line.startswith("Result")), ""))
    return p.returncode == 0


def build_unit_tests(zig, args, target="native"):
    """Builds mpc/tests/mpc_tests.cpp (every plugin, without entry points) against the shared objects."""
    suffix = "" if target == "native" else "-" + target
    exe = os.path.join(BUILD, "tests", "mpc-tests" + suffix + (".exe" if os.name == "nt" and target == "native" else ""))
    source = os.path.join(MPC, "tests", "mpc_tests.cpp")
    if not compile_sources(zig, target, [source], args, "test", ["MPC_GRID_NO_ENTRY"], obj_target="tests" + suffix):
        return None
    objects = [object_path("tests" + suffix, source)] + [object_path(target, s) for s in SHARED_SOURCES]
    if not link(zig, target, objects, exe, args, shared=False):
        return None
    return exe


def run_unit_tests(zig, args, target="native", runner=()):
    exe = build_unit_tests(zig, args, target)
    if exe is None:
        return False
    log("[%s] mpc tests" % target)
    env = dict(os.environ)
    # The Amp's factory capture comes from its assembled folder.
    env["MPC_PLUGIN_FOLDER"] = next(p.out for p in all_plugins() if p.uses_captures)
    scratch = os.path.join(BUILD, "tests", "scratch-" + target)
    p = subprocess.run(list(runner) + [exe, scratch], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env,
                       timeout=3600)
    log(p.stdout.decode("utf-8", errors="replace").rstrip())
    return p.returncode == 0


def main(argv=None):
    global VERBOSE
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--target", choices=["all", "native", "arm64", "linux64"], default="all")
    ap.add_argument("--plugin", nargs="*", help="plugin folder names under mpc/plugins (default: all)")
    ap.add_argument("--test", action="store_true",
                    help="run the MPC plugin tests and the SDK host simulator against the native builds")
    ap.add_argument("--seconds", type=float, default=3.0, help="host_sim render length")
    ap.add_argument("--arm-tests", nargs="*", choices=["arm64", "linux64"], default=[],
                    help="also build the MPC tests and host_sim for these Linux targets")
    ap.add_argument("--emulate", action="store_true",
                    help="run the --arm-tests builds (Linux only; arm64 needs qemu-user and libc6-arm64-cross)")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--debug", action="store_true")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args(argv)
    VERBOSE = args.verbose

    plugins = all_plugins()
    if args.plugin:
        unknown = set(args.plugin) - {p.id for p in plugins}
        if unknown:
            log("ERROR: unknown plugin(s) %s; choose from %s" % (", ".join(sorted(unknown)), ", ".join(p.id for p in plugins)))
            return 2
        plugins = [p for p in plugins if p.id in args.plugin]
    try:
        zig = sdk.find_zig()
    except sdk.BuildError as e:
        log("ERROR: %s" % e)
        return 1
    # The tests cover every plugin, and the Amp's factory capture.
    for plugin in (all_plugins() if args.test or args.arm_tests else plugins):
        assemble(plugin)
    targets = ["native", "arm64"] if args.target == "all" else [args.target]
    if "native" not in targets and (args.test or args.target == "all"):
        targets.insert(0, "native")
    targets += [t for t in args.arm_tests if t not in targets]
    for target in targets:
        if not build_target(zig, target, plugins, args):
            return 1
    sim = host_sim(zig)
    if sim is None:
        return 1
    if "native" in targets:
        for plugin in plugins:
            params = dump_params(sim, plugin)
            if params is None or not make_skins(plugin, params):
                return 1
            sdk.write_version_xml(plugin.out, plugin.manifest, True)
    if args.target == "all":
        for plugin in plugins:
            if not validate(plugin):
                return 1
        log("Plugin folders: %s" % OUT)
        log("Copy them into <drive>/VST2 Plugins/ on the MPC (MPC OS 3.10.1 or later, with Preferences >")
        log("Enhancements > Enable Enhancements and External Plugin Support switched on).")
    if args.test:
        if not run_unit_tests(zig, args):
            return 1
        for plugin in plugins:
            if not run_host_test(sim, plugin, args.seconds):
                return 1
    # The shipped Linux/ARM libraries, through the tests and host_sim built for the same target.
    for target in args.arm_tests:
        target_sim = host_sim(zig, target)
        if target_sim is None:
            return 1
        if not args.emulate:
            if build_unit_tests(zig, args, target) is None:
                return 1
            log("[%s] built the tests and %s" % (target, os.path.relpath(target_sim, ROOT)))
            continue
        runner = EMULATORS[target]
        if runner and shutil.which(runner[0]) is None:
            log("ERROR: --emulate needs %s (Ubuntu: qemu-user, libc6-arm64-cross)" % runner[0])
            return 1
        if not run_unit_tests(zig, args, target, runner):
            return 1
        for plugin in plugins:
            if not run_host_test(target_sim, plugin, 1., target, runner):
                return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
