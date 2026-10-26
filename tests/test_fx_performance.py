"""On-demand report/worker-failure regressions; no compiler or plugin required."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("fx_performance", Path(__file__).parents[1] / "scripts" / "fx-performance.py")
benchmark = importlib.util.module_from_spec(spec)
spec.loader.exec_module(benchmark)


def measurement(case_id="fx-1", cpu=10., misses=0):
    timing = {"cpu_percent": cpu, "p95_us": 9., "p99_us": 10., "max_us": 11.,
              "max_budget_percent": 20., "deadline_misses": misses}
    return {"rate": 48000, "buffer": 64, "repeat": 0, "timing": timing,
            "signal": timing.copy(), "silence": timing.copy(), "warmup": timing.copy(),
            "process_errors": 0, "nonfinite_samples": 0,
            "case": {"id": case_id, "module": "Empty grid" if case_id == "empty" else "Noise Gate",
                     "mode": "default", "controls": {}}}


class ReportTests(unittest.TestCase):
    def test_modulation_profiles_preserve_baseline_and_distinguish_routing(self):
        cases = [{"id": "empty", "mode": "default"}, {"id": "nam", "mode": "large"}]
        macros = benchmark.modulation_cases(cases, "macros")
        self.assertEqual(macros[0]["id"], "empty")
        self.assertEqual(macros[1]["globals"]["Mod 1 Source"], 4)
        self.assertEqual(macros[1]["globals"]["Mod 1 Slot"], 16)
        self.assertEqual(macros[1]["globals"]["Macro 1"], 50.)
        zero = benchmark.modulation_cases(cases, "depth-zero")
        self.assertEqual(zero[1]["globals"]["Mod 1 Depth"], 0.)
        all_sources = benchmark.modulation_cases(cases, "all")[1]["globals"]
        self.assertEqual([all_sources[f"Mod {i} Source"] for i in range(1, 7)], [1, 2, 3, 8, 9, 10])
        self.assertEqual(benchmark.modulation_cases(cases, "none"), cases)

    def test_crash_keeps_completed_and_marks_missing_tuples(self):
        settings = {"rates": [48000], "buffers": [32, 64], "repeats": 2}
        text = 'plugin diagnostic\n{"rate":48000,"buffer":32,"repeat":0,"timing":{}}\n{"partial":'
        rows = benchmark.parse_worker_output(text, {"id": "test"}, settings, "crashed")
        self.assertEqual(len(rows), 4)
        self.assertNotIn("error", rows[0])
        self.assertEqual([r["error"] for r in rows[1:]], ["crashed"] * 3)

    def test_report_baseline_subtraction_and_errors(self):
        rows = [measurement("empty", 2.), measurement(cpu=8.), measurement("fx-bad")]
        rows[2]["nonfinite_samples"] = 1
        metadata = {"generated_utc": "test", "plugin_sha256": "abc", "settings": {"paced": False}}
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)
            self.assertEqual(benchmark.write_reports(path, metadata, rows), (1, 0))
            report = json.loads((path / "report.json").read_text(encoding="utf-8"))
            self.assertEqual(report["rows"][1]["estimated_module_cpu_percent"], 6.)
            self.assertEqual(report["rows"][2]["status"], "ERROR")
            self.assertIn("Noise Gate", (path / "report.md").read_text(encoding="utf-8"))
            self.assertIn("estimated_module_cpu_percent", (path / "measurements.csv").read_text())

    def test_warmup_deadline_misses_are_not_hidden(self):
        row = measurement()
        row["warmup"]["deadline_misses"] = 1
        metadata = {"generated_utc": "test", "plugin_sha256": "abc", "settings": {"paced": False}}
        with tempfile.TemporaryDirectory() as folder:
            self.assertEqual(benchmark.write_reports(Path(folder), metadata, [row]), (0, 1))
            self.assertEqual(row["status"], "OVERRUN")
            self.assertIsNone(row["estimated_module_cpu_percent"])

    def test_invalid_asset_is_an_error_row_not_a_cpu_result(self):
        row = {"rate": 48000, "buffer": 32, "repeat": 0, "error": "Asset failed to load",
               "case": {"id": "nam", "module": "NAM", "mode": "bad.nam"}}
        metadata = {"generated_utc": "test", "plugin_sha256": "abc", "settings": {"paced": False}}
        with tempfile.TemporaryDirectory() as folder:
            self.assertEqual(benchmark.write_reports(Path(folder), metadata, [row]), (1, 0))

    def test_slimmable_assets_cover_both_quality_modes(self):
        with tempfile.TemporaryDirectory() as folder:
            model = Path(folder) / "test.nam"
            model.write_text('{"architecture":"SlimmableContainer"}')
            cases = benchmark.asset_cases([model], [])
            self.assertEqual([c["controls"]["Full Quality"] for c in cases], [1, 0])
            self.assertEqual([c["quality"] for c in cases], ["A2", "A2 Lite"])
            model.write_text('{"architecture":"WaveNet"}')
            self.assertEqual(len(benchmark.asset_cases([model], [])), 1)

    def test_asset_cases_use_host_module_ids(self):
        with tempfile.TemporaryDirectory() as folder:
            model, ir = Path(folder) / "test.nam", Path(folder) / "test.wav"
            model.write_text('{"architecture":"WaveNet"}')
            ir.write_bytes(b"RIFF")
            cases = benchmark.asset_cases([model], [ir], {"NAM": 40, "Cabinet IR": 41})
            self.assertEqual([c["effect"] for c in cases], [40, 41])

    def test_partial_report_does_not_claim_completion(self):
        metadata = {"generated_utc": "test", "plugin_sha256": "abc", "settings": {"paced": False},
                    "planned_measurements": 100}
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)
            benchmark.write_reports(path, metadata, [measurement()])
            data = json.loads((path / "report.json").read_text(encoding="utf-8"))
            self.assertFalse(data["metadata"]["complete"])
            self.assertIn("PARTIAL RUN: 1 of 100", (path / "report.md").read_text(encoding="utf-8"))

    def test_individual_module_selection_is_exact_and_keeps_baseline(self):
        cases = [{"id": "empty", "module": "Empty grid", "mode": "default"},
                 {"id": "gate", "module": "Noise Gate", "mode": "Natural"},
                 {"id": "smart", "module": "Noise Gate (Smart)", "mode": "trained"}]
        self.assertEqual([c["id"] for c in benchmark.select_cases(cases, ["noise gate"], [])], ["empty", "gate"])
        self.assertEqual([c["id"] for c in benchmark.select_cases(cases, ["Noise Gate (Smart)"], ["trained"])], ["empty", "smart"])
        self.assertEqual(len(benchmark.select_cases(cases, ["empty"], [])), 1)
        with self.assertRaises(ValueError):
            benchmark.select_cases(cases, ["unknown"], [])

    def test_stacked_cases_preserve_controls_assets_and_keep_one_baseline(self):
        cases = [{"id": "empty", "module": "Empty grid", "mode": "default", "controls": {}},
                 {"id": "model", "module": "NAM", "mode": "model", "controls": {"Full Quality": 1}, "asset": "model.nam"},
                 {"id": "hold", "module": "Freeze", "mode": "held", "controls": {"Freeze Hold": 1}}]
        result = benchmark.stacked_cases(cases, [1, 4, 4, 16])
        self.assertEqual([r["id"] for r in result], ["empty", "model", "model-x4", "model-x16", "hold"])
        self.assertEqual(result[2]["asset"], "model.nam")
        self.assertEqual(result[2]["controls"], {"Full Quality": 1})
        self.assertNotIn("copies", cases[1])
        with self.assertRaises(ValueError):
            benchmark.stacked_cases(cases, [17])

    def test_comparison_uses_matched_repeat_medians_and_validates_settings(self):
        settings = {"rates": [48000], "buffers": [64], "seconds": 3., "warmup_seconds": .5, "repeats": 3, "paced": False}
        metadata = {"settings": settings, "plugin_sha256": "test", "sample_format": "stereo float64"}
        before = [measurement(cpu=value) for value in (8., 100., 8.)]
        after = [measurement(cpu=4.) for _ in range(3)]
        for run in (before, after):
            for repeat, row in enumerate(run):
                row["repeat"] = repeat
                row["case"]["effect"] = 1
        baseline = {"metadata": metadata, "rows": before}
        with tempfile.TemporaryDirectory() as folder:
            result = benchmark.write_comparison(Path(folder), baseline, metadata, after)
            self.assertEqual(result["rows"][0]["before_cpu_percent"], 8.)
            self.assertEqual(result["rows"][0]["cpu_change_percent"], -50.)
            mismatched = dict(metadata, settings=dict(settings, seconds=1.))
            with self.assertRaises(ValueError):
                benchmark.write_comparison(Path(folder), baseline, mismatched, after)
            after[0]["case"]["copies"] = 4
            with self.assertRaises(ValueError):
                benchmark.write_comparison(Path(folder), baseline, metadata, after)
            del after[0]["case"]["copies"]
            after[0]["case"]["globals"] = {"Mod 1 Source": 4}
            with self.assertRaises(ValueError):
                benchmark.write_comparison(Path(folder), baseline, metadata, after)
            del after[0]["case"]["globals"]
            after[0]["error"] = "load failed"
            result = benchmark.write_comparison(Path(folder), baseline, metadata, after)
            self.assertIn("error", result["rows"][0])
            self.assertNotIn("cpu_change_percent", result["rows"][0])


if __name__ == "__main__":
    unittest.main()
