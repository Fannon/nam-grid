#!/usr/bin/env python3
"""Ensure shipped Mach-O plugins reserve writable Lanczos tables without file bytes."""
import subprocess
import sys

for binary in sys.argv[1:]:
    symbols = subprocess.check_output(["/usr/bin/nm", "-m", binary], text=True)
    for name in ("sTableE", "sDeltaTableE"):
        matches = [line for line in symbols.splitlines()
                   if "LanczosResamplerIdLi1ELm12EE" in line and line.endswith(name)]
        assert len(matches) == 1, (binary, name, matches)
        assert "(__DATA,__bss)" in matches[0], (binary, matches[0])
    print(f"{binary}: both Lanczos tables use zero-fill storage")
