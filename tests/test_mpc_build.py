"""MPC incremental builds honor compiler configuration, not only timestamps."""
import argparse
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("mpc_build_tests", ROOT / "mpc/build.py")
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)


class MPCBuildTests(unittest.TestCase):
    def test_flags_and_toolchain_changes_recompile_unchanged_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "source.cpp"
            source.write_text("int sample;", encoding="utf-8")
            calls = []
            version = ["test-zig-1"]

            def compiler(argv, **kwargs):
                if argv[-1] == "version":
                    return 0, version[0]
                calls.append(argv)
                obj = Path(argv[argv.index("-o") + 1])
                dep = Path(argv[argv.index("-MF") + 1])
                obj.write_bytes(b"object")
                dep.write_text(str(obj) + ": " + str(source), encoding="utf-8")
                return 0, ""

            args = argparse.Namespace(debug=False, jobs=1)
            with patch.object(build, "BUILD", directory), patch.object(build, "run", side_effect=compiler):
                for _ in range(2):
                    self.assertTrue(build.compile_sources(["zig"], "native", [str(source)], args, "test"))
                self.assertEqual(len(calls), 1)
                args.debug = True
                self.assertTrue(build.compile_sources(["zig"], "native", [str(source)], args, "test"))
                self.assertEqual(len(calls), 2, "--debug must invalidate release objects")
                self.assertIn("-O0", calls[-1])
                version[0] = "test-zig-2"
                self.assertTrue(build.compile_sources(["zig"], "native", [str(source)], args, "test"))
                self.assertEqual(len(calls), 3, "a new compiler must invalidate existing objects")

    def test_invalid_dependency_file_is_not_a_cache_hit(self):
        with tempfile.TemporaryDirectory() as directory:
            obj, dep = Path(directory) / "sample.o", Path(directory) / "sample.d"
            obj.touch()
            dep.write_text("truncated", encoding="utf-8")
            self.assertFalse(build.up_to_date(str(obj), str(dep)))

    def test_failed_compile_cannot_leave_a_valid_cache_entry(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "source.cpp"
            source.touch()
            fail = [True]
            attempts = []

            def compiler(argv, **kwargs):
                if argv[-1] == "version":
                    return 0, "test-zig"
                attempts.append(argv)
                Path(argv[argv.index("-o") + 1]).touch()
                Path(argv[argv.index("-MF") + 1]).write_text("object: " + str(source), encoding="utf-8")
                return (1, "interrupted compile") if fail[0] else (0, "")

            args = argparse.Namespace(debug=False, jobs=1)
            with patch.object(build, "BUILD", directory), patch.object(build, "run", side_effect=compiler):
                self.assertFalse(build.compile_sources(["zig"], "native", [str(source)], args, "test"))
                fail[0] = False
                self.assertTrue(build.compile_sources(["zig"], "native", [str(source)], args, "test"))
                self.assertEqual(len(attempts), 2)


if __name__ == "__main__":
    unittest.main()
