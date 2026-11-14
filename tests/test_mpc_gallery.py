"""MPC gallery safety and native page bindings without a browser or compiler."""
import importlib.util
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("mpc_gallery_build_tests", ROOT / "mpc/build.py")
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)
import gallery


class MPCGalleryTests(unittest.TestCase):
    def test_filename_is_text_not_svg_markup(self):
        root = ET.Element("svg")
        gallery.text(root, 0, 0, 560, 30, '<script>alert("capture")</script>&')
        encoded = ET.tostring(root, encoding="unicode")
        self.assertIn("&lt;script&gt;", encoded)
        self.assertNotIn("<script>", encoded)

    def test_library_is_read_only_and_has_every_filename_line(self):
        labels = build.skins.library_labels()
        for section in ("Amp", "Cab"):
            for line in range(3, 13):
                self.assertTrue(any(l.get("param") == section + " Info " + str(line) for l in labels))
        controls, _, _ = build.page_layout({"name": "LIBRARY", "library": True}, {}, False)
        self.assertEqual(controls, ["empty"] * 16, "information labels must not acquire Q-Link assignments")
        for label in labels:
            x, y, w, h = label["bounds"]
            self.assertLessEqual(x + w, 1280)
            self.assertLessEqual(y + h, 628)

    def test_misspelled_page_parameter_fails_instead_of_disappearing(self):
        with self.assertRaises(ValueError):
            build.page_layout({"name": "AMP", "columns": [["Amp Wrong Knob"]]}, {}, False)


if __name__ == "__main__":
    unittest.main()
