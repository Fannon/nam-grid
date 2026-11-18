#!/usr/bin/env python3
"""Fetch pinned SDK headers/sources; preserve iPlug2's tracked placeholder files."""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import sys
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SDK_ROOT = ROOT / "iPlug2" / "Dependencies" / "IPlug"
CACHE = ROOT / "build" / "deps"
PINS = [
    ("CLAP_SDK", "https://github.com/free-audio/clap.git", "a47f6badb49d948fd009998f28309cdab78979c9"),
    ("CLAP_HELPERS", "https://github.com/free-audio/clap-helpers.git", "29389e43489b1af44b654ef5a7b4ee0af4bbc13b"),
    ("VST3_SDK", "https://github.com/steinbergmedia/vst3sdk.git", "3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96"),
]
# Steinberg ASIO SDK for the Windows standalone app, used under its GPLv3 option (see
# docs/build.md#standalone-app). Steinberg publishes it only as a download, not a repository.
ASIO_SDK = ("https://download.steinberg.net/sdk_downloads/ASIO-SDK_2.3.4_2025-10-15.zip",
            "d5ebf0c20dd2c5f43771fd0c1418f4b361bf52434ee670097cfa6b3a335e2eca")


def git(*args):
    subprocess.run(["git", *map(str, args)], check=True)


def matches_patched_tree(directory, patches):
    """Whether the work tree equals the pinned commit with the whole patch stack applied.

    Later patches can change lines an earlier one touched, so a single patch cannot be
    reverse-checked on its own. Build the expected tree in a scratch index instead.
    """
    env = dict(os.environ, GIT_INDEX_FILE=str(CACHE / ("%s.patched-index" % directory.name)))
    run = lambda *args, **kwargs: subprocess.run(["git", "-C", str(directory), *map(str, args)], env=env,
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                       **kwargs).returncode == 0
    try:
        # Git's index stores LF even when Windows checks out CRLF. Unlike a
        # work-tree apply, --cached cannot match CRLF patch context against LF blobs.
        return (run("read-tree", "HEAD")
                and all(run("apply", "--cached", "-", input=patch.read_bytes().replace(b"\r\n", b"\n"))
                        for patch in patches)
                and run("diff", "--quiet", "--ignore-submodules"))
    finally:
        Path(env["GIT_INDEX_FILE"]).unlink(missing_ok=True)


def apply_dependency_patches():
    CACHE.mkdir(parents=True, exist_ok=True)
    for dependency, pattern in (("NeuralAmpModelerCore", "nam-core-*.patch"),
                                ("AudioDSPTools", "audio-dsp-*.patch"),
                                ("iPlug2", "iplug2-*.patch")):
        directory = ROOT / dependency
        # Platform ports leave the Windows and macOS sources unchanged.
        patches = [patch for patch in sorted((ROOT / "patches").glob(pattern))
                   if not patch.name.startswith("iplug2-linux-") or sys.platform.startswith("linux")]
        if not patches or matches_patched_tree(directory, patches):
            continue
        # Refuses with git's error if the submodule has other local changes or moved commits.
        for patch in patches:
            git("-C", directory, "apply", "--check", patch)
            git("-C", directory, "apply", patch)


def prepare_asio_sdk(url=ASIO_SDK[0], sha256=ASIO_SDK[1]):
    """Download and unpack the pinned ASIO SDK into build/deps/ASIO_SDK; reuse a verified copy."""
    destination = CACHE / "ASIO_SDK"
    stamp = destination / ".sha256"
    if stamp.is_file() and stamp.read_text().strip() == sha256:
        return destination
    archive = CACHE / url.rsplit("/", 1)[1]
    if not archive.is_file() or hashlib.sha256(archive.read_bytes()).hexdigest() != sha256:
        with urllib.request.urlopen(url, timeout=120) as response:
            archive.write_bytes(response.read())
    actual = hashlib.sha256(archive.read_bytes()).hexdigest()
    if actual != sha256:
        archive.unlink()
        raise SystemExit(f"ASIO SDK download does not match its pin: {actual}")
    shutil.rmtree(destination, ignore_errors=True)
    with zipfile.ZipFile(archive) as contents:
        contents.extractall(destination)
    stamp.write_text(sha256)
    return destination


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
    # Only the Windows standalone app (APP format) uses ASIO.
    if sys.platform == "win32":
        prepare_asio_sdk()
        print("Prepared ASIO_SDK 2.3.4")


if __name__ == "__main__":
    main()
