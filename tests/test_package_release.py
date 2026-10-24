"""Release archive contents and notice checks; no compiler or plugin required."""
import importlib.util
import os
from pathlib import Path
import stat
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location("package_release", Path(__file__).parents[1] / "scripts" / "package-release.py")
packaging = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packaging)


def build_tree(root: Path) -> Path:
    out = root / "out"
    (out / "nam-grid.vst3" / "Contents" / "x86_64-linux").mkdir(parents=True)
    plugin = out / "nam-grid.vst3" / "Contents" / "x86_64-linux" / "nam-grid.so"
    plugin.write_bytes(b"\x7fELF")
    (out / "nam-grid.clap").write_bytes(b"\x7fELF")
    for binary in (plugin, out / "nam-grid.clap"):
        binary.chmod(binary.stat().st_mode | stat.S_IXUSR)
    (out / "ThirdPartyNotices.txt").write_text("\n".join(packaging.NOTICES), encoding="utf-8")
    (out / "notices").mkdir()
    for license in packaging.LICENSES:
        (out / "notices" / license).write_text("license", encoding="utf-8")
    (out / "resources" / "ttf").mkdir(parents=True)  # Linux builds leave loose resource copies.
    (out / "resources" / "ttf" / "Roboto-Regular.ttf").write_bytes(b"font")
    return root


class PackageTests(unittest.TestCase):
    def test_archive_holds_only_release_contents(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = packaging.package(build_tree(Path(directory)), "linux-x64", Path(directory))
            self.assertEqual(archive.name, "nam-grid-linux-x64.zip")
            with zipfile.ZipFile(archive) as contents:
                names = contents.namelist()
                self.assertFalse(any(name.startswith("resources") for name in names))
                for required in ("nam-grid.clap", "ThirdPartyNotices.txt", "notices/RiffSmith-LICENSE.txt",
                                 "nam-grid.vst3/Contents/x86_64-linux/nam-grid.so"):
                    self.assertIn(required, names)
                if os.name != "nt":  # Windows file modes have no executable bit to keep.
                    mode = contents.getinfo("nam-grid.clap").external_attr >> 16
                    self.assertTrue(mode & stat.S_IXUSR)

    def test_missing_notice_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root = build_tree(Path(directory))
            (root / "out" / "ThirdPartyNotices.txt").write_text("iPlug2", encoding="utf-8")
            with self.assertRaises(SystemExit):
                packaging.package(root, "linux-x64", root)
            (root / "out" / "ThirdPartyNotices.txt").write_text("\n".join(packaging.NOTICES), encoding="utf-8")
            (root / "out" / "notices" / "VST3_SDK-LICENSE.txt").unlink()
            with self.assertRaises(SystemExit):
                packaging.package(root, "linux-x64", root)


if __name__ == "__main__":
    unittest.main()
