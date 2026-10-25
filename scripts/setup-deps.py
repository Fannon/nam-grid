#!/usr/bin/env python3
"""Fetch pinned SDK headers/sources; preserve iPlug2's tracked placeholder files."""
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SDK_ROOT = ROOT / "iPlug2" / "Dependencies" / "IPlug"
CACHE = ROOT / "build" / "deps"
PINS = [
    ("CLAP_SDK", "https://github.com/free-audio/clap.git", "a47f6badb49d948fd009998f28309cdab78979c9"),
    ("CLAP_HELPERS", "https://github.com/free-audio/clap-helpers.git", "29389e43489b1af44b654ef5a7b4ee0af4bbc13b"),
    ("VST3_SDK", "https://github.com/steinbergmedia/vst3sdk.git", "3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96"),
]


def git(*args):
    subprocess.run(["git", *map(str, args)], check=True)


def apply_dependency_patches():
    for dependency, pattern in (("NeuralAmpModelerCore", "nam-core-*.patch"),
                                ("AudioDSPTools", "audio-dsp-*.patch"),
                                ("iPlug2", "iplug2-*.patch")):
        directory = ROOT / dependency
        for patch in sorted((ROOT / "patches").glob(pattern)):
            # Platform ports leave the Windows and macOS sources unchanged.
            if patch.name.startswith("iplug2-linux-") and not sys.platform.startswith("linux"):
                continue
            applied = subprocess.run(["git", "-C", str(directory), "apply", "--reverse", "--check", str(patch)],
                                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0
            if not applied:
                git("-C", directory, "apply", "--check", patch)
                git("-C", directory, "apply", patch)


def main():
    if not (ROOT / "iPlug2" / "iPlug2.cmake").is_file():
        raise SystemExit("Initialize the pinned git submodules first (see docs/build.md).")
    apply_dependency_patches()
    CACHE.mkdir(parents=True, exist_ok=True)
    for name, url, revision in PINS:
        source = CACHE / name
        if not source.exists():
            git("clone", "--no-checkout", "--filter=blob:none", url, source)
        git("-C", source, "fetch", "--depth", "1", "origin", revision)
        git("-C", source, "checkout", "--detach", revision)
        if name == "VST3_SDK":
            git("-C", source, "submodule", "update", "--init", "base", "pluginterfaces", "public.sdk", "cmake")
        destination = SDK_ROOT / name
        destination.mkdir(exist_ok=True)
        for item in source.iterdir():
            if item.name in (".git", ".gitmodules", "README.md", "readme.txt"):
                continue
            if item.is_dir():
                shutil.copytree(item, destination / item.name, dirs_exist_ok=True,
                                ignore=shutil.ignore_patterns(".git"))
            else:
                shutil.copy2(item, destination / item.name)
        print(f"Prepared {name} @ {revision}")


if __name__ == "__main__":
    main()
