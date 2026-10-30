import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("setup_deps", ROOT / "scripts" / "setup-deps.py")
setup_deps = importlib.util.module_from_spec(spec)
spec.loader.exec_module(setup_deps)


def git(directory, *args):
    return subprocess.run(["git", "-C", str(directory), *args], check=True, capture_output=True, text=True).stdout


class PatchStackTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.repo = self.root / "dependency"
        self.repo.mkdir()
        setup_deps.CACHE = self.root / "cache"
        setup_deps.CACHE.mkdir()
        git(self.repo, "init", "-q")
        git(self.repo, "config", "user.email", "test@example.com")
        git(self.repo, "config", "user.name", "Test")
        source = self.repo / "source.txt"
        source.write_text("one\ntwo\nthree\n")
        git(self.repo, "add", ".")
        git(self.repo, "commit", "-q", "-m", "pin")
        # The second patch rewrites the line the first one changed, as stacked patches do.
        self.patches = []
        for index, text in enumerate(["one\nTWO\nthree\n", "one\nTWO!\nthree\n"]):
            source.write_text(text)
            patch = self.root / ("%03d.patch" % index)
            patch.write_text(git(self.repo, "diff"))
            git(self.repo, "add", ".")
            self.patches.append(patch)
        git(self.repo, "reset", "-q", "--hard")

    def tearDown(self):
        self.temp.cleanup()

    def apply_all(self):
        for patch in self.patches:
            git(self.repo, "apply", str(patch))

    def test_applied_overlapping_stack_is_recognized(self):
        self.apply_all()
        # A per-patch reverse check cannot see the first patch under the second.
        first = subprocess.run(["git", "-C", str(self.repo), "apply", "--reverse", "--check", str(self.patches[0])],
                               capture_output=True)
        self.assertNotEqual(first.returncode, 0)
        self.assertTrue(setup_deps.matches_patched_tree(self.repo, self.patches))

    def test_clean_and_partial_trees_need_patching(self):
        self.assertFalse(setup_deps.matches_patched_tree(self.repo, self.patches))
        git(self.repo, "apply", str(self.patches[0]))
        self.assertFalse(setup_deps.matches_patched_tree(self.repo, self.patches))

    def test_crlf_overlapping_stack_is_recognized(self):
        # Reproduce Git for Windows checkout and Python's native text writes,
        # including CRLF patch context applied against canonical LF index blobs.
        git(self.repo, "config", "core.autocrlf", "true")
        (self.repo / "source.txt").write_bytes(b"one\r\ntwo\r\nthree\r\n")
        for patch in self.patches:
            patch.write_bytes(patch.read_bytes().replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))
        self.apply_all()
        self.assertTrue(setup_deps.matches_patched_tree(self.repo, self.patches))
        (self.repo / "source.txt").write_bytes(b"one\r\nlocal edit\r\nthree\r\n")
        self.assertFalse(setup_deps.matches_patched_tree(self.repo, self.patches))

    def test_added_files_must_exist(self):
        (self.repo / "added.txt").write_text("new\n")
        git(self.repo, "add", "-N", "added.txt")
        patch = self.root / "added.patch"
        patch.write_text(git(self.repo, "diff"))
        git(self.repo, "rm", "-q", "--cached", "added.txt")
        self.assertTrue(setup_deps.matches_patched_tree(self.repo, [patch]))
        (self.repo / "added.txt").unlink()
        self.assertFalse(setup_deps.matches_patched_tree(self.repo, [patch]))


if __name__ == "__main__":
    unittest.main()
