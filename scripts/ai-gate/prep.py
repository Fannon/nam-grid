"""Reads GuitarSet pickup-mix WAVs, cleans their floor and writes 48 kHz arrays.

Self-contained: run with `python -I` because it reads downloaded files.
Usage: prep.py <guitarset dir> <output dir> [--inspect]
"""
import json
import sys
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import istft, resample_poly, stft

TARGET_RATE = 48000


def clean_floor(x, rate):
    """Non-causal spectral expander against the file's own stationary floor."""
    f, t, s = stft(x, rate, nperseg=2048, noverlap=1536)
    magnitude = np.abs(s)
    floor = np.percentile(magnitude, 3, axis=1, keepdims=True)
    # Smooth the floor over frequency: a 3rd percentile of a sustained note is still a note.
    kernel = np.ones(31) / 31
    floor = np.convolve(np.pad(floor[:, 0], 15, mode="edge"), kernel, mode="valid")[:, None]
    ratio = magnitude / (4 * floor + 1e-12)
    gain = np.clip(ratio - 1, 0, 1)
    gain = gain * gain * (3 - 2 * gain)
    # Keep bins next to strong ones in time: protects decays and attacks.
    spread = np.maximum.reduce([gain, np.roll(gain, 1, 1), np.roll(gain, -1, 1)])
    _, y = istft(s * spread, rate, nperseg=2048, noverlap=1536)
    return y[: len(x)].astype(np.float32), floor[:, 0]


def main():
    source, output = Path(sys.argv[1]), Path(sys.argv[2])
    inspect = "--inspect" in sys.argv
    files = sorted(source.rglob("*.wav"))
    print(len(files), "files")
    train, test = [], []
    records = {"train": [], "test": []}
    offsets = {"train": 0, "test": 0}
    if not inspect:
        output.mkdir(parents=True, exist_ok=True)
    for index, path in enumerate(files):
        x, rate = sf.read(path, dtype="float32")
        if x.ndim > 1:
            x = x.mean(1)
        assert rate == 44100
        y, floor = clean_floor(x, rate)
        if inspect and index % 60 == 0:
            w = rate // 10
            e = lambda v: np.sort([np.sqrt(np.mean(v[i : i + w] ** 2)) for i in range(0, len(v) - w, w)])
            db = lambda v: np.round(20 * np.log10(v + 1e-9), 1)
            print(path.name, "peak", db(np.abs(x).max()), "quietest 100 ms rms", db(e(x)[:3]), "->", db(e(y)[:3]),
                  "removed rms", db(np.sqrt(np.mean((x - y) ** 2))), "signal rms", db(np.sqrt(np.mean(x**2))))
        if not inspect:
            split = "test" if path.name.startswith("05_") else "train"
            # The model/filter/noise clock is 48 kHz; GuitarSet recordings are 44.1 kHz.
            prepared = resample_poly(y, 160, 147).astype(np.float32)
            records[split].append({"file": path.relative_to(source).as_posix(),
                                   "source_sample_rate": rate, "source_frames": len(x),
                                   "offset": offsets[split], "frames": len(prepared)})
            offsets[split] += len(prepared)
            (test if split == "test" else train).append(prepared)
    if not inspect:
        np.save(output / "train.npy", np.concatenate(train))
        np.save(output / "test.npy", np.concatenate(test))
        metadata = {"format_version": 1, "sample_rate": TARGET_RATE,
                    "preprocessing": "clean-floor-v1-resample-poly-v1", "splits": records}
        (output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        print("train h", offsets["train"] / TARGET_RATE / 3600,
              "test h", offsets["test"] / TARGET_RATE / 3600)


if __name__ == "__main__":
    main()
