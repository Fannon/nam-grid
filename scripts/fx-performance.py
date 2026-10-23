#!/usr/bin/env python3
"""Run only on demand: isolate CLAP scenarios and write CPU/deadline reports.

Requires an explicitly built Release nam-fx-performance and nam-grid CLAP.
No audio device is opened. See docs/fx-performance.md for measurement limits.
"""
import argparse
from collections import defaultdict
import csv
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def positive(value):
    number = float(value)
    if not math.isfinite(number) or number <= 0:
        raise argparse.ArgumentTypeError("must be a finite positive number")
    return number


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def find_executable(build, name):
    suffix = ".exe" if os.name == "nt" else ""
    for parent in (build / "NeuralAmpModeler", build / "NeuralAmpModeler" / "Release"):
        candidate = parent / (name + suffix)
        if candidate.is_file():
            return candidate.resolve()
    raise ValueError(f"Build {name} in Release first, or supply --host (see docs/fx-performance.md)")


def plugin_binary(path):
    if path.is_file():
        return path
    mac_binary = path / "Contents" / "MacOS" / "nam-grid"
    if mac_binary.is_file():
        return mac_binary
    raise ValueError(f"CLAP binary not found: {path}")


def asset_cases(nam_paths, ir_paths, modules={"NAM": 0, "Cabinet IR": 6}):
    """File-module cases; `modules` maps names to IDs as printed by the host's --modules."""
    cases = []
    for index, asset in enumerate(nam_paths):
        with asset.open(encoding="utf-8-sig") as stream:
            model = json.load(stream)
        architecture = model.get("architecture", "unknown")
        # Quality only changes inference for slimmable architectures.
        slimmable = "slimmable" in architecture.lower()
        qualities = (1, 0) if slimmable else (1,)
        for quality in qualities:
            quality_name = ("A2" if quality else "A2 Lite") if slimmable else "fixed"
            cases.append({"id": f"nam-{index}-q{quality}", "module": "NAM", "effect": modules["NAM"],
                          "controls": {"Full Quality": quality}, "asset": str(asset),
                          "architecture": architecture, "asset_sha256": sha256(asset),
                          "quality": quality_name, "mode": f"{asset.name} ({architecture}; {quality_name})"})
    for index, asset in enumerate(ir_paths):
        cases.append({"id": f"ir-{index}", "module": "Cabinet IR", "effect": modules["Cabinet IR"],
                      "controls": {}, "asset": str(asset), "asset_sha256": sha256(asset), "mode": asset.name})
    return cases


def parse_worker_output(output, scenario, settings, failure=None):
    """Keep completed rows after a crash; mark every unfinished tuple as failed."""
    parsed = {}
    for line in (output or "").splitlines():
        try:
            row = json.loads(line)
            key = (row["rate"], row["buffer"], row["repeat"])
        except (ValueError, KeyError, TypeError):
            continue  # plugin diagnostics may also appear on stdout
        parsed[key] = row
    rows = []
    for rate in settings["rates"]:
        for buffer in settings["buffers"]:
            for repeat in range(settings["repeats"]):
                row = parsed.get((rate, buffer, repeat), {
                    "rate": rate, "buffer": buffer, "repeat": repeat,
                    "error": failure or "Worker returned no measurement"})
                row["case"] = scenario
                rows.append(row)
    return rows


def select_cases(cases, modules, filters):
    names = {c["module"].casefold() for c in cases}
    requested = {m.casefold() for m in modules}
    if "empty" in requested:
        requested.remove("empty")
        requested.add("empty grid")
    unknown = requested - names
    if unknown:
        raise ValueError(f"Unknown module(s): {', '.join(sorted(unknown))}. Use --list for names.")
    selected = [c for c in cases if c["id"] == "empty" or (
        (not requested or c["module"].casefold() in requested) and
        (not filters or any(term.casefold() in f"{c['id']} {c['module']} {c['mode']}".casefold() for term in filters)))]
    if len(selected) == 1 and (filters or (requested and requested != {"empty grid"})):
        raise ValueError("No cases matched the module/filter selection")
    return selected


def check_comparison_settings(baseline, metadata):
    for field in ("rates", "buffers", "seconds", "warmup_seconds", "repeats", "paced"):
        if baseline["metadata"]["settings"][field] != metadata["settings"][field]:
            raise ValueError(f"Comparison requires matching {field}")
    if baseline["metadata"].get("sample_format") != metadata.get("sample_format"):
        raise ValueError("Comparison requires matching sample formats")


def write_comparison(folder, baseline, metadata, rows):
    """Compare medians of repeated matched cases, including the empty-grid cost."""
    check_comparison_settings(baseline, metadata)
    grouped = []
    for run in (baseline["rows"], rows):
        groups = defaultdict(list)
        for row in run:
            groups[(row["case"]["id"], row["rate"], row["buffer"])].append(row)
        grouped.append(groups)
    if set(grouped[0]) != set(grouped[1]):
        raise ValueError("Comparison requires the same selected cases, sample rates and buffers")
    measurements = []
    for key, before_rows in grouped[0].items():
        after_rows = grouped[1][key]
        old, case = before_rows[0]["case"], after_rows[0]["case"]
        if (old["effect"], old["controls"], old.get("asset_sha256")) != (
                case["effect"], case["controls"], case.get("asset_sha256")):
            raise ValueError(f"Comparison requires matching module controls/assets: {key[0]}")
        result = {"module": case["module"], "mode": case["mode"], "case_id": key[0], "rate": key[1], "buffer": key[2]}
        valid = all("timing" in r and not r.get("error") and not r.get("process_errors", 0)
                    and not r.get("nonfinite_samples", 0) for r in before_rows + after_rows)
        if not valid or len(before_rows) != metadata["settings"]["repeats"] or len(after_rows) != len(before_rows):
            result["error"] = "Missing/failed measurements; no CPU improvement calculated"
        else:
            for prefix, run in (("before", before_rows), ("after", after_rows)):
                result[prefix + "_cpu_percent"] = statistics.median(r["timing"]["cpu_percent"] for r in run)
                result[prefix + "_p99_us"] = statistics.median(r["timing"]["p99_us"] for r in run)
                result[prefix + "_deadline_misses"] = sum(r["timing"]["deadline_misses"] for r in run)
                result[prefix + "_warmup_misses"] = sum(r["warmup"]["deadline_misses"] for r in run)
                increments = [r.get("estimated_module_cpu_percent") for r in run]
                result[prefix + "_module_cpu_percent"] = (
                    statistics.median(increments) if all(v is not None for v in increments) else None)
            before_cpu, after_cpu = result["before_cpu_percent"], result["after_cpu_percent"]
            result["cpu_change_percent"] = 100. * (after_cpu / before_cpu - 1.) if before_cpu else None
            result["cpu_change_points"] = after_cpu - before_cpu
        measurements.append(result)
    comparison = {"before_plugin_sha256": baseline["metadata"]["plugin_sha256"],
                  "after_plugin_sha256": metadata["plugin_sha256"], "settings": metadata["settings"], "rows": measurements}
    (folder / "comparison.json").write_text(json.dumps(comparison, indent=2, allow_nan=False), encoding="utf-8")
    lines = ["# FX performance comparison", "", "Matched module/mode/rate/buffer settings and asset hashes. "
             "CPU values are medians of repeat means, including the grid/plugin overhead. "
             "Negative change means lower callback CPU. Empty-grid rows reveal shared-overhead changes. "
             "JSON also includes estimated module CPU after subtracting each run's empty-grid baseline.", "",
             f"Before plugin: `{comparison['before_plugin_sha256']}`", "",
             f"After plugin: `{comparison['after_plugin_sha256']}`", "",
             "| Module / mode | Hz | Frames | Before CPU % | After CPU % | Change % | Before/after p99 µs | Misses before/after |",
             "|---|---:|---:|---:|---:|---:|---:|---:|"]
    for row in measurements:
        label = (row["module"] + " / " + row["mode"]).replace("|", "\\|")
        if "error" in row:
            lines.append(f"| {label} | {row['rate']} | {row['buffer']} | error | error | n/a | n/a | n/a |")
        else:
            change = f"{row['cpu_change_percent']:+.1f}" if row["cpu_change_percent"] is not None else "n/a"
            lines.append(f"| {label} | {row['rate']} | {row['buffer']} | {row['before_cpu_percent']:.3f} | "
                         f"{row['after_cpu_percent']:.3f} | {change} | {row['before_p99_us']:.1f} / {row['after_p99_us']:.1f} | "
                         f"{row['before_deadline_misses']} / {row['after_deadline_misses']} |")
    lines += ["", "Repeated comparisons under similar background load are needed to judge improvements. "
              "These timings do not measure audio-driver xruns or certify pop-free DAW playback.", ""]
    (folder / "comparison.md").write_text("\n".join(lines), encoding="utf-8")
    fields = list(dict.fromkeys(k for row in measurements for k in row))
    with (folder / "comparison.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader(); writer.writerows(measurements)
    return comparison


def write_reports(folder, metadata, rows):
    complete = len(rows) == metadata.get("planned_measurements", len(rows))
    metadata["complete"] = complete
    baselines = {}
    for row in rows:
        if row["case"]["id"] == "empty" and "timing" in row:
            baselines[(row["rate"], row["buffer"], row["repeat"])] = row["timing"]["cpu_percent"]
    for row in rows:
        if "timing" in row:
            baseline = baselines.get((row["rate"], row["buffer"], row["repeat"]))
            row["estimated_module_cpu_percent"] = (
                max(0., row["timing"]["cpu_percent"] - baseline) if baseline is not None else None)
            row["status"] = "ERROR" if row.get("error") or row["process_errors"] or row["nonfinite_samples"] else (
                "OVERRUN" if row["timing"]["deadline_misses"] or row["warmup"]["deadline_misses"] else (
                    "LOW HEADROOM" if row["timing"]["max_budget_percent"] > 50. else "OK"))
        else:
            row["status"] = "ERROR"
    report = {"metadata": metadata, "rows": rows}
    (folder / "report.json").write_text(json.dumps(report, indent=2, allow_nan=False), encoding="utf-8")
    fields = ["module", "mode", "case_id", "asset", "rate", "buffer", "repeat", "status", "cpu_percent",
              "estimated_module_cpu_percent", "signal_cpu_percent", "silence_cpu_percent", "p95_us", "p99_us", "max_us",
              "deadline_us", "deadline_misses", "warmup_max_us", "warmup_deadline_misses", "first_callback_us",
              "load_ms", "activation_ms", "latency_samples", "output_peak", "nonfinite_samples", "process_errors",
              "paced_late_blocks", "paced_max_lateness_us", "error"]
    with (folder / "measurements.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            case = row["case"]
            flat = {k: v for k, v in row.items() if k in fields}
            flat.update(module=case["module"], mode=case["mode"], case_id=case["id"], asset=case.get("asset", ""))
            flat.update({k: v for k, v in row.get("timing", {}).items() if k in fields})
            for phase in ("signal", "silence"):
                flat[phase + "_cpu_percent"] = row.get(phase, {}).get("cpu_percent", "")
            flat["warmup_max_us"] = row.get("warmup", {}).get("max_us", "")
            flat["warmup_deadline_misses"] = row.get("warmup", {}).get("deadline_misses", "")
            flat["paced_late_blocks"] = row.get("paced_completion", {}).get("late_blocks", "")
            flat["paced_max_lateness_us"] = row.get("paced_completion", {}).get("max_lateness_us", "")
            writer.writerow(flat)
    errors = [r for r in rows if r["status"] == "ERROR"]
    overruns = [r for r in rows if r["status"] == "OVERRUN"]
    lines = ["# nam-grid FX performance", "", f"Generated: {metadata['generated_utc']}", "",
             f"Plugin SHA256: `{metadata['plugin_sha256']}`", "",
             "Run complete." if complete else f"PARTIAL RUN: {len(rows)} of {metadata['planned_measurements']} planned measurements.", "",
             f"{len(rows)} measurements; {len(errors)} errors; {len(overruns)} rows with deadline misses (including warm-up).", "",
             "CPU is elapsed time inside the actual stereo CLAP process callback / buffer duration × 100. "
             "100% consumes one audio thread's entire deadline, not the machine's total CPU. "
             "Includes the empty grid and plugin overhead; estimated module CPU subtracts the matching empty-grid mean "
             "and is an approximate comparison between separate processes.", "",
             "Each row uses 75% continuous stereo signal and 25% silence/tail, plus separate warm-up. "
             "Worst values retain scheduling outliers and every repeat. Startup/state preparation times are separate from DSP. "
             "OK means finite output and no observed callback overruns; it does not certify a pop-free DAW session.", "",
             f"Timing: {'paced (also records OS wake-up lateness)' if metadata['settings']['paced'] else 'unpaced offline throughput with per-callback deadline comparison'}.", "",
             "## Per-module summary", "",
             "Max CPU is the highest row mean across modes, rates, buffers and repeats. P99/worst are maxima across rows. "
             "Misses include measured and warm-up callbacks. Full per-mode results and signal/silence splits are in measurements.csv and report.json.", "",
             "| Module | Cases | Max CPU % | Est. module % | Max p99 µs | Worst µs | Misses | Errors |",
             "|---|---:|---:|---:|---:|---:|---:|---:|"]
    if "rates" in metadata["settings"]:
        settings = metadata["settings"]
        lines[4:4] = [f"Machine: {metadata.get('cpu', 'unknown')}; {metadata.get('platform', 'unknown')}.", "",
                      f"Matrix: {settings['rates']} Hz; {settings['buffers']} frames; {settings['repeats']} repeats; "
                      f"{settings['seconds']} s measured + {settings['warmup_seconds']} s warm-up per repeat. "
                      f"Format: {metadata.get('sample_format', 'stereo float64')}.", ""]
    modules = sorted({r["case"]["module"] for r in rows})
    for module in modules:
        group = [r for r in rows if r["case"]["module"] == module]
        good = [r for r in group if "timing" in r]
        maximum = lambda key: max((r["timing"][key] for r in good), default=0.)
        delta = max((r["estimated_module_cpu_percent"] or 0. for r in good), default=0.)
        misses = sum(r["timing"]["deadline_misses"] + r["warmup"]["deadline_misses"] for r in good)
        cpu_text = f"{maximum('cpu_percent'):.2f}" if good else "n/a"
        delta_text = f"{delta:.2f}" if any(r["estimated_module_cpu_percent"] is not None for r in good) else "n/a"
        lines.append(f"| {module} | {len({r['case']['id'] for r in group})} | {cpu_text} | "
                     f"{delta_text} | {maximum('p99_us'):.1f} | {maximum('max_us'):.1f} | {misses} | "
                     f"{sum(r['status'] == 'ERROR' for r in group)} |")
    lines += ["", "## Highest CPU measurements", "", "| Module / mode | Rate | Buffer | Repeat | CPU % | p99 µs | Worst µs | Status |",
              "|---|---:|---:|---:|---:|---:|---:|---|"]
    for row in sorted((r for r in rows if "timing" in r), key=lambda r: r["timing"]["cpu_percent"], reverse=True)[:25]:
        timing, case = row["timing"], row["case"]
        label = (case["module"] + " / " + case["mode"]).replace("|", "\\|")
        lines.append(f"| {label} | {row['rate']} | {row['buffer']} | {row['repeat'] + 1} | {timing['cpu_percent']:.2f} | "
                     f"{timing['p99_us']:.1f} | {timing['max_us']:.1f} | {row['status']} |")
    if errors:
        lines += ["", "## Errors", ""]
        for row in errors[:30]:
            lines.append(f"- {row['case']['id']} at {row['rate']} Hz/{row['buffer']} frames: "
                         f"{row.get('error', 'Non-finite output or CLAP process error')}")
    lines += ["", "## Limits", "",
              "This does not open ASIO/CoreAudio, measure driver xruns/DPC latency, listen for clicks, "
              "exercise UI/automation or concurrent asset loading, or model a complete rig. "
              "A single module's cost is not a complete DAW project budget. Even paced mode uses an ordinary "
              "OS thread, so wake-up lateness is host/scheduler evidence rather than a measured device xrun. "
              "Steady-state algorithm modes are covered; knob extremes and every combination of tempo divisions "
              "with algorithm switches are not exhaustive. See docs/fx-performance.md.", ""]
    (folder / "report.md").write_text("\n".join(lines), encoding="utf-8")
    return len(errors), len(overruns)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build-grid")
    parser.add_argument("--host", type=Path, help="Release nam-fx-performance executable")
    parser.add_argument("--plugin", type=Path, help="CLAP binary (Mac: executable inside the .clap bundle)")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--rates", type=int, nargs="+", default=[44100, 48000, 96000])
    parser.add_argument("--buffers", type=int, nargs="+", default=[32, 64, 128])
    parser.add_argument("--seconds", type=positive, default=3., help="measured audio duration per tuple/repeat (75%% signal, 25%% silence)")
    parser.add_argument("--warmup", type=positive, default=.5, help="warm-up audio seconds, also checked for overruns")
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--nam", type=Path, action="append", default=[], help="add a real NAM capture (repeatable)")
    parser.add_argument("--ir", type=Path, action="append", default=[], help="add a real WAV IR (repeatable)")
    parser.add_argument("--filter", action="append", default=[], help="case-insensitive module/mode/id substring (OR; baseline always included)")
    parser.add_argument("--module", action="append", default=[], help="exact module name; repeatable; includes all modes and the empty-grid baseline")
    parser.add_argument("--compare", type=Path, help="previous report.json with matching cases/settings; writes comparison.md/.csv/.json")
    parser.add_argument("--list", action="store_true", help="list selected cases without loading the plugin")
    parser.add_argument("--paced", action="store_true", help="pace callbacks in wall time and record scheduling lateness; can take hours")
    parser.add_argument("--fail-on-overrun", action="store_true", help="also exit 1 on deadline misses (errors always fail)")
    parser.add_argument("--timeout", type=positive, help="override per-case wall time timeout in seconds")
    args = parser.parse_args(argv)
    if args.repeats < 1 or args.repeats > 100 or any(r < 8000 or r > 384000 for r in args.rates) or any(b < 1 or b > 8192 for b in args.buffers):
        parser.error("repeats must be 1..100, rates 8000..384000 Hz, buffers 1..8192 frames")
    build = args.build_dir.resolve()
    host = (args.host or find_executable(build, "nam-fx-performance")).resolve()
    if not host.is_file():
        parser.error(f"Host not found: {host}")
    cache = build / "CMakeCache.txt"
    if cache.is_file() and "CMAKE_BUILD_TYPE:STRING=Debug" in cache.read_text(encoding="utf-8"):
        parser.error("Use a Release build for meaningful CPU results")
    folder = (args.output or build / "fx-performance" / datetime.now().strftime("%Y%m%d-%H%M%S-%f")).resolve()
    folder.mkdir(parents=True, exist_ok=True)
    fixtures = folder / "fixtures"
    subprocess.run([str(host), "--fixtures", str(fixtures)], check=True)
    cases = json.loads(subprocess.check_output([str(host), "--catalog"], text=True, encoding="utf-8"))
    nam_paths = [fixtures / name for name in ("a2-small.nam", "a2-large.nam", "slimmable.nam")]
    nam_paths += [ROOT / "NeuralAmpModelerCore" / "example_models" / name for name in ("lstm.nam", "wavenet.nam")]
    ir_paths = [fixtures / name for name in ("ir-2048.wav", "ir-16384.wav")]
    modules = json.loads(subprocess.check_output([str(host), "--modules"], text=True, encoding="utf-8"))
    cases += asset_cases(nam_paths + [p.resolve() for p in args.nam], ir_paths + [p.resolve() for p in args.ir], modules)
    cases = select_cases(cases, args.module, args.filter)
    if args.list:
        for case in cases:
            print(f"{case['id']:20} {case['module']}: {case['mode']}")
        return 0
    plugin = (args.plugin or build / "out" / "nam-grid.clap").resolve()
    binary = plugin_binary(plugin)
    settings = {"rates": list(dict.fromkeys(args.rates)), "buffers": list(dict.fromkeys(args.buffers)),
                "seconds": args.seconds, "warmup_seconds": args.warmup, "repeats": args.repeats, "paced": args.paced}
    settings_path = folder / "settings.json"
    settings_path.write_text(json.dumps(settings), encoding="utf-8")
    baseline = json.loads(args.compare.read_text(encoding="utf-8")) if args.compare else None
    def git(*command):
        result = subprocess.run(["git", *command], cwd=ROOT, capture_output=True, text=True)
        return result.stdout.strip()
    count = len(settings["rates"]) * len(settings["buffers"]) * settings["repeats"]
    metadata = {"generated_utc": datetime.now(timezone.utc).isoformat(), "platform": platform.platform(),
                "cpu": platform.processor(), "logical_cpus": os.cpu_count(), "host": str(host), "host_sha256": sha256(host),
                "plugin": str(binary), "plugin_sha256": sha256(binary), "source_commit": git("rev-parse", "HEAD"),
                "source_status": git("status", "--short"), "submodules": git("submodule", "status"),
                "settings": settings, "cases": cases, "sample_format": "stereo float64", "planned_measurements": len(cases) * count}
    if baseline is not None:
        check_comparison_settings(baseline, metadata)
        identities = lambda selected: {c["id"]: (c["effect"], c["controls"], c.get("asset_sha256")) for c in selected}
        if identities(baseline["metadata"]["cases"]) != identities(cases):
            raise ValueError("Comparison requires matching selected cases, module controls and asset hashes")
    rows = []
    timeout = args.timeout or max(120., count * (args.seconds + args.warmup) * 3. + 60.)
    start = time.monotonic()
    print(f"{len(cases)} cases x {count} measurements. Reports: {folder}", flush=True)
    try:
        with (folder / "raw.jsonl").open("w", encoding="utf-8") as raw:
            for index, case in enumerate(cases):
                print(f"[{index + 1}/{len(cases)}] {case['module']}: {case['mode']}", flush=True)
                case_path = folder / "case.json"
                case_path.write_text(json.dumps(case), encoding="utf-8")
                failure = None
                try:
                    worker = subprocess.run([str(host), "--run", str(binary), str(case_path), str(settings_path)],
                                            capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=timeout)
                    output = worker.stdout
                    if worker.returncode:
                        failure = f"Worker failed/crashed: exit {worker.returncode}; {worker.stderr[-2000:]}"
                    if worker.stderr or failure:
                        with (folder / "worker.log").open("a", encoding="utf-8") as log:
                            log.write(f"{case['id']}: {failure or ''}\n{worker.stderr}\n")
                except subprocess.TimeoutExpired as error:
                    output = error.stdout or ""
                    if isinstance(output, bytes):
                        output = output.decode("utf-8", errors="replace")
                    failure = f"Worker timed out after {timeout:.1f}s"
                completed = parse_worker_output(output, case, settings, failure)
                # A teardown crash after all rows is still a failure.
                if failure and all("error" not in r for r in completed):
                    completed[-1]["error"] = failure
                    completed[-1]["process_errors"] = completed[-1].get("process_errors", 0) + 1
                rows.extend(completed)
                for row in completed:
                    raw.write(json.dumps(row, allow_nan=False) + "\n")
                raw.flush()
    finally:
        metadata["elapsed_seconds"] = time.monotonic() - start
        errors, overruns = write_reports(folder, metadata, rows)
        print(f"Report: {folder / 'report.md'}\nErrors: {errors}; rows with overruns: {overruns}", flush=True)
        if baseline is not None:
            write_comparison(folder, baseline, metadata, rows)
            print(f"Comparison: {folder / 'comparison.md'}", flush=True)
    return 1 if errors or (args.fail_on_overrun and overruns) else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(2)
