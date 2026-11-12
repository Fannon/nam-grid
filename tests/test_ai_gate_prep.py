"""GuitarSet preparation must preserve time and pitch at the model's 48 kHz clock."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

PREP_SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "ai-gate" / "prep.py"
AVAILABLE = all(importlib.util.find_spec(name) is not None for name in ("numpy", "scipy", "soundfile"))


@unittest.skipUnless(AVAILABLE, "AI training preparation requires numpy, scipy and soundfile")
class GuitarSetPreparationTests(unittest.TestCase):
    def recordings(self, folder):
        import numpy as np
        import soundfile as sf

        for name, frames in (("00_a.wav", 11025), ("00_b.wav", 13230), ("05_test.wav", 8820)):
            t = np.arange(frames) / 44100
            signal = .2 * np.sin(2 * np.pi * 440 * t) * np.exp(-2 * t)
            sf.write(folder / name, signal, 44100, subtype="FLOAT")

    def invoke(self, source, output, *extra):
        return subprocess.run([sys.executable, "-I", str(PREP_SCRIPT), str(source), str(output), *extra],
                              check=True, capture_output=True, text=True)

    def test_prepared_arrays_keep_time_and_pitch_at_48k(self):
        import numpy as np

        with tempfile.TemporaryDirectory() as directory:
            source, output = Path(directory) / "source", Path(directory) / "output"
            source.mkdir()
            output.mkdir()
            self.recordings(source)
            self.invoke(source, output)
            train, test = np.load(output / "train.npy"), np.load(output / "test.npy")
            self.assertEqual(len(train), 12000 + 14400)
            self.assertEqual(len(test), 9600)
            self.assertEqual(train.dtype, np.float32)
            self.assertEqual(test.dtype, np.float32)
            frequency = np.fft.rfftfreq(12000, 1 / 48000)[np.argmax(np.abs(np.fft.rfft(train[:12000])))]
            self.assertAlmostEqual(frequency, 440, delta=5)
            metadata = json.loads((output / "metadata.json").read_text(encoding="utf-8"))
            self.assertEqual(metadata["sample_rate"], 48000)
            self.assertEqual(metadata["format_version"], 1)
            self.assertEqual([item["offset"] for item in metadata["splits"]["train"]], [0, 12000])
            self.assertEqual([item["frames"] for item in metadata["splits"]["train"]], [12000, 14400])
            self.assertEqual(metadata["splits"]["test"][0]["offset"], 0)
            self.assertEqual(metadata["splits"]["test"][0]["source_sample_rate"], 44100)
            self.assertEqual(metadata["splits"]["test"][0]["source_frames"], 8820)

    def test_inspect_writes_no_training_arrays(self):
        with tempfile.TemporaryDirectory() as directory:
            source, output = Path(directory) / "source", Path(directory) / "output"
            source.mkdir()
            output.mkdir()
            self.recordings(source)
            self.invoke(source, output, "--inspect")
            self.assertEqual(list(output.iterdir()), [])


if __name__ == "__main__":
    unittest.main()
