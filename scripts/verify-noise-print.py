"""Compare current Noise Print with the committed pre-optimization DSP.

Run in a C++20 compiler environment (MSVC x64 developer prompt on Windows).
Generated reference source, executable and objects stay in build/<platform>/noise-optimization/.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", default="214efc7", help="git revision of the independent reference processor")
    parser.add_argument("--scalar", action="store_true", help="verify the portable fallback instead of SIMD")
    parser.add_argument("--exact", action="store_true", help="require bit-identical output, learned profiles and meters")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    platform = {"win32": "windows", "darwin": "macos"}.get(sys.platform, "linux")
    output = (root / "build" / platform / ("noise-optimization-scalar" if args.scalar else "noise-optimization")).resolve()
    if root not in output.parents:
        raise RuntimeError("Verification output must remain inside this checkout")
    output.mkdir(parents=True, exist_ok=True)
    reference = subprocess.check_output(
        ["git", "show", f"{args.reference}:NeuralAmpModeler/NoisePrint.h"], cwd=root
    ).decode("utf-8")
    (output / "NoisePrintReference.h").write_text(
        reference.replace("namespace noise_print", "namespace noise_print_reference"),
        encoding="utf-8",
    )
    compiler = os.environ.get("CXX") or shutil.which("cl") or shutil.which("c++")
    if not compiler:
        raise RuntimeError("Set up a C++20 compiler first (or set CXX to its executable)")
    source = root / "tests" / "noise_print_reference.cpp"
    executable = output / ("compare.exe" if os.name == "nt" else "compare")
    if Path(compiler).name.lower() in {"cl", "cl.exe", "clang-cl", "clang-cl.exe"}:
        command = [compiler, "/nologo", "/std:c++20", "/O2", "/EHsc", "/UNDEBUG",
                   f"/I{output}", str(source), f"/Fo{output / 'compare.obj'}", f"/Fe{executable}"]
    else:
        # Keep our executable named 'compare' out of the standard <compare> search.
        command = [compiler, "-std=c++20", "-O2", "-UNDEBUG", "-iquote", str(output), str(source), "-o", str(executable)]
    if args.scalar:
        command.append("/DNAM_NOISE_PRINT_SCALAR" if Path(compiler).name.lower() in {"cl", "cl.exe", "clang-cl", "clang-cl.exe"}
                       else "-DNAM_NOISE_PRINT_SCALAR")
    if args.exact:
        command.append("/DNAM_NOISE_PRINT_REQUIRE_EXACT" if Path(compiler).name.lower() in {"cl", "cl.exe", "clang-cl", "clang-cl.exe"}
                       else "-DNAM_NOISE_PRINT_REQUIRE_EXACT")
    subprocess.run(command, cwd=output, check=True)
    subprocess.run([str(executable)], cwd=output, check=True)


if __name__ == "__main__":
    main()
