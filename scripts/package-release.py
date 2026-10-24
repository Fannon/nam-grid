#!/usr/bin/env python3
"""Package a built plugin tree as nam-grid-<platform>.zip (see docs/build.md).

The archive holds the CLAP, the VST3 bundle, ThirdPartyNotices.txt and notices/. The
command fails if any of them or a required notice is missing. Zip entries keep their
file modes, which macOS bundles and Linux shared objects need.
"""
import argparse
import shutil
import tempfile
from pathlib import Path

PLATFORMS = ("windows-x64", "macos-arm64", "linux-x64")
CONTENTS = ("nam-grid.clap", "nam-grid.vst3", "ThirdPartyNotices.txt", "notices")
NOTICES = ("NeuralAmpModelerPlugin", "iPlug2", "Roboto font", "Apache License", "Version 2.0, January 2004",
           "Denormal helper (Aether)", "Airwindows", "FFTConvolver", "RiffSmith Clean")
LICENSES = ("CLAP_SDK-LICENSE.txt", "CLAP_HELPERS-LICENSE.txt", "VST3_SDK-LICENSE.txt", "RiffSmith-LICENSE.txt")


def package(build_dir: Path, platform: str, output_dir: Path) -> Path:
    out = build_dir / "out"
    for name in CONTENTS:
        if not (out / name).exists():
            raise SystemExit(f"Missing release output: {name}")
    notices = (out / "ThirdPartyNotices.txt").read_text(encoding="utf-8")
    for required in NOTICES:
        if required not in notices:
            raise SystemExit(f"ThirdPartyNotices.txt lacks: {required}")
    for license in LICENSES:
        if not (out / "notices" / license).is_file():
            raise SystemExit(f"Missing notices/{license}")
    # Stage only the release contents: Linux builds also leave loose resource copies in out/.
    with tempfile.TemporaryDirectory() as staging:
        for name in CONTENTS:
            source, target = out / name, Path(staging) / name
            if source.is_dir():
                shutil.copytree(source, target, symlinks=True)
            else:
                shutil.copy2(source, target)
        output_dir.mkdir(parents=True, exist_ok=True)
        archive = shutil.make_archive(str(output_dir / f"nam-grid-{platform}"), "zip", staging)
    return Path(archive)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", required=True, choices=PLATFORMS)
    parser.add_argument("--build-dir", type=Path, default=Path("build-grid"))
    parser.add_argument("--output-dir", type=Path, help="defaults to the build directory")
    args = parser.parse_args()
    print(package(args.build_dir, args.platform, args.output_dir or args.build_dir))


if __name__ == "__main__":
    main()
