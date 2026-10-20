#!/usr/bin/env python3
"""Extract the original single-model DSP for a comparison using the same CLAP adapter."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REVISION = "16be869746b8915885c7a35bafdcc0061faeb50e"
OUTPUT = ROOT / "NeuralAmpModeler" / "build-baseline"
OUTPUT.mkdir(exist_ok=True)
for original, name in [("NeuralAmpModeler.cpp", "BaselineNAM.cpp"),
                       ("NeuralAmpModeler.h", "BaselineNAM.h"),
                       ("Unserialization.cpp", "Unserialization.cpp")]:
    source = subprocess.check_output(["git", "-C", str(ROOT), "show", f"{REVISION}:NeuralAmpModeler/{original}"], text=True)
    source = source.replace('../AudioDSPTools/', '../../AudioDSPTools/').replace('../NeuralAmpModelerCore/', '../../NeuralAmpModelerCore/')
    source = source.replace('"NeuralAmpModeler.h"', '"BaselineNAM.h"')
    source = source.replace(': Plugin(info,', ': iplug::Plugin(info,').replace('  Plugin::OnUIOpen();', '  iplug::Plugin::OnUIOpen();')
    (OUTPUT / name).write_text(source)
print(f"Prepared original DSP from {REVISION} in {OUTPUT}")
