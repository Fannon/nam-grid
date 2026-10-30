"""Verify factory-resource compression without loading a plugin."""
import importlib.util
import json
from pathlib import Path
import re
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("embed", ROOT / "scripts/embed-default-content.py")
embed = importlib.util.module_from_spec(spec)
spec.loader.exec_module(embed)


def payload(source, name):
    body = re.search(rf"{name}Data\[.*?\] = \{{(.*?)\}};", source, re.S).group(1)
    return bytes(int(byte, 16) for byte in re.findall(r"0x([0-9a-f]{2})", body))


class EmbeddedContentTests(unittest.TestCase):
    def test_factory_files_roundtrip_and_shrink(self):
        folder = ROOT / "NeuralAmpModeler/resources/defaults"
        capture = (folder / "riffsmith-clean.nam").read_bytes()
        preset = (folder / "Clean Default.ngrid").read_bytes()
        source = embed.generate(capture, preset)
        compressed_capture = payload(source, "CleanCapture")
        compressed_preset = payload(source, "CleanPreset")
        self.assertEqual(zlib.decompress(compressed_capture), capture)
        decoded_preset = zlib.decompress(compressed_preset)
        self.assertEqual(json.loads(decoded_preset), json.loads(preset))
        self.assertLess(len(decoded_preset), len(preset))
        self.assertLess(len(compressed_capture) + len(compressed_preset), (len(capture) + len(preset)) // 2)

    def test_capture_bytes_are_opaque_and_preset_strings_survive(self):
        capture = b'\x00\xff { "weights": [1.00000] }\r\n'
        preset = b'{\n "name": "a  b \\u00e4", "value": 0.25, "flag": true\n}'
        source = embed.generate(capture, preset)
        self.assertEqual(zlib.decompress(payload(source, "CleanCapture")), capture)
        self.assertEqual(json.loads(zlib.decompress(payload(source, "CleanPreset"))), json.loads(preset))
        self.assertEqual(source, embed.generate(capture, preset))


if __name__ == "__main__":
    unittest.main()
