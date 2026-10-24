#!/usr/bin/env python3
"""On-demand per-module waveform comparison of two CLAP binaries.

Renders every benchmark case (all FX modes plus NAM/IR fixtures) through the
explicitly built nam-fx-performance host, once per binary, while automating each
module's continuous controls and the slot's shared dynamics/EQ/level controls.
Outputs must match exactly unless --tolerance is given. See docs/fx-performance.md.
"""
import argparse
from array import array
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("fx_performance", ROOT / "scripts" / "fx-performance.py")
benchmark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(benchmark)

# Shared slot controls (name, first value, second value or None), automated in every case.
SHARED = [("Pre 1 NAM Input", 6., 3.), ("Pre 1 NAM Output", -4., -2.), ("Pre 1 Gate Threshold", -40., -20.),
          ("Pre 1 Gate Release", 300., 150.), ("Pre 1 Gate Reduction", 30., 15.), ("Pre 1 Comp Threshold", -30., -15.),
          ("Pre 1 Comp Release", 300., 150.), ("Pre 1 Comp Makeup", 6., 3.), ("Pre 1 EQ Bass", 5., 2.5),
          ("Pre 1 EQ Mid", -4., -2.), ("Pre 1 EQ Treble", 3., 1.5), ("Pre 1 EQ Low Cut", 120., 60.),
          ("Pre 1 EQ High Cut", 6000., 3000.), ("Pre 1 EQ Presence", 4., 2.), ("Pre 1 IR Output", -6., -3.)]
# Matrix routing is saved state, not automation: every source gets a row to a slot 1
# or routing destination at depth 0. The depths rise after the first pass of knob
# changes, so each render covers both the plain and the modulated control paths.
# Destinations are slot control indices (In, Trim, Gate Threshold, EQ Treble) or
# Slot 16 with a routing destination (0 = A/B blend).
ROUTING = {}
for row, (source, slot, destination) in enumerate([(1, 0, 0), (2, 0, 1), (3, 0, 11), (4, 16, 0), (5, 0, 3)], 1):
    ROUTING.update({f"Mod {row} Source": source, f"Mod {row} Slot": slot, f"Mod {row} Destination": destination})
MODULATION = [("LFO Rate", 3., None), ("Mod 1 Depth", 40., -30.), ("Mod 2 Depth", 60., 30.), ("Mod 3 Depth", 50., -50.),
              ("Macro 1", 60., 30.), ("Mod 4 Depth", 50., -40.), ("Macro 2", 40., 80.), ("Mod 5 Depth", 30., 60.)]
LATCHED = ("Freeze Hold", "Shimmer Freeze", "Noise Learn")


def events(case, blocks):
    """Two passes: move every control away from its default, then to a second value."""
    if any(case["controls"].get(name, 0) >= .5 for name in LATCHED):
        return []  # The host reserves its single event slot for the capture/learn trigger.
    knobs = [("Pre 1 " + k["name"], k["minimum"] + .3 * (k["maximum"] - k["minimum"]),
              k["minimum"] + .7 * (k["maximum"] - k["minimum"])) for k in case.get("knobs", [])]
    controls = SHARED + knobs
    sequence = [(name, first) for name, first, _ in controls] + [(name, first) for name, first, _ in MODULATION]
    sequence += [(name, second) for name, _, second in controls + MODULATION if second is not None]
    # Leave settling time between changes, and silence at the end for tails.
    spacing = max(1, (blocks * 3 // 4) // (len(sequence) + 1))
    return [{"block": spacing * (index + 1), "name": name, "value": value if isinstance(value, str) else round(value, 6)}
            for index, (name, value) in enumerate(sequence)]


def render(host, plugin, case, rate, buffer, seconds, warmup, folder):
    path = folder / "render.f64"
    blocks = int(-(-(seconds + warmup) * rate // buffer))
    settings = {"rates": [rate], "buffers": [buffer], "seconds": seconds, "warmup_seconds": warmup, "repeats": 1,
                "paced": False, "render_path": str(path), "parameter_events": events(case, blocks)}
    (folder / "case.json").write_text(json.dumps({**case, "globals": ROUTING}), encoding="utf-8")
    (folder / "settings.json").write_text(json.dumps(settings), encoding="utf-8")
    run = subprocess.run([str(host), "--run", str(plugin), str(folder / "case.json"), str(folder / "settings.json")],
                         capture_output=True, text=True, timeout=300)
    lines = run.stdout.splitlines()
    result = json.loads(lines[-1]) if lines else {"error": run.stderr.strip() or "no output"}
    if run.returncode or result.get("error") or result.get("process_errors") or result.get("nonfinite_samples"):
        raise RuntimeError(f"{case['id']} {rate}/{buffer}: {result.get('error') or result}")
    samples = array("d")
    samples.frombytes(path.read_bytes())
    path.unlink()  # Only this audit's own generated file.
    return samples, len(settings["parameter_events"])


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--host", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rates", type=int, nargs="+", default=[44100, 48000])
    parser.add_argument("--buffers", type=int, nargs="+", default=[32, 37, 128])
    parser.add_argument("--seconds", type=float, default=2.)
    parser.add_argument("--warmup", type=float, default=.5)
    parser.add_argument("--nam", type=Path, action="append", default=[])
    parser.add_argument("--ir", type=Path, action="append", default=[])
    parser.add_argument("--filter", action="append", default=[])
    parser.add_argument("--module", action="append", default=[])
    parser.add_argument("--tolerance", type=float, default=0., help="maximum absolute sample difference")
    args = parser.parse_args()
    host = (args.host or benchmark.find_executable(ROOT / "build-grid", "nam-fx-performance")).resolve()
    before, after = (benchmark.plugin_binary(p.resolve()) for p in (args.before, args.after))
    folder = args.output.resolve()
    folder.mkdir(parents=True, exist_ok=False)
    fixtures = folder / "fixtures"
    subprocess.run([str(host), "--fixtures", str(fixtures)], check=True, capture_output=True)
    nams = [fixtures / n for n in ("a2-small.nam", "slimmable.nam")]
    nams += [ROOT / "NeuralAmpModelerCore" / "example_models" / n for n in ("lstm.nam", "wavenet.nam")] + args.nam
    irs = [fixtures / n for n in ("ir-2048.wav", "ir-16384.wav")] + args.ir
    cases = json.loads(subprocess.check_output([str(host), "--catalog"], text=True, encoding="utf-8"))
    modules = json.loads(subprocess.check_output([str(host), "--modules"], text=True, encoding="utf-8"))
    cases = benchmark.select_cases(cases + benchmark.asset_cases(nams, irs, modules), args.module, args.filter)
    rows, failures = [], 0
    try:
        for index, case in enumerate(cases):
            for rate in args.rates:
                for buffer in args.buffers:
                    row = {"case": case["id"], "module": case["module"], "mode": case["mode"], "rate": rate,
                           "buffer": buffer}
                    try:
                        a, count = render(host, before, case, rate, buffer, args.seconds, args.warmup, folder)
                        b, _ = render(host, after, case, rate, buffer, args.seconds, args.warmup, folder)
                        if len(a) != len(b):
                            raise RuntimeError("render lengths differ")
                        difference = [abs(x - y) for x, y in zip(a, b)]
                        maximum = max(difference)
                        mismatches = sum(d > args.tolerance for d in difference)
                        row.update(samples=len(a), events=count, max_error=maximum, mismatched_samples=mismatches,
                                   first_mismatch=next((i for i, d in enumerate(difference) if d > args.tolerance), None),
                                   exact=maximum == 0., passed=mismatches == 0)
                    except Exception as error:  # Keep auditing other cases; a failure is never a pass.
                        row.update(error=str(error), passed=False)
                    failures += not row["passed"]
                    rows.append(row)
                    status = "ok" if row["passed"] else "FAIL"
                    detail = row.get("error") or f"max error {row['max_error']:.3g}, {row['events']} automation events"
                    print(f"[{index + 1}/{len(cases)}] {status} {case['module']} / {case['mode']} {rate}/{buffer}: {detail}",
                          flush=True)
    finally:
        expected = len(cases) * len(args.rates) * len(args.buffers)
        digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
        report = {"before_sha256": digest(before), "after_sha256": digest(after), "tolerance": args.tolerance,
                  "rates": args.rates, "buffers": args.buffers, "seconds": args.seconds, "warmup": args.warmup,
                  "complete": len(rows) == expected, "pass": len(rows) == expected and not failures,
                  "exact": sum(r.get("exact", False) for r in rows), "rows": rows}
        (folder / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"{len(rows)}/{expected} renders compared, {report['exact']} bit-identical, {failures} failed")
    return int(not report["pass"])


if __name__ == "__main__":
    sys.exit(main())
