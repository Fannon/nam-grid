"""On-demand audit automation regressions; no compiler or plugin required."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    "verify_fx_processing", Path(__file__).parents[1] / "scripts" / "verify-fx-processing.py")
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class EventTests(unittest.TestCase):
    def test_knobs_shared_controls_and_modulation_are_automated_in_order(self):
        case = {"controls": {}, "knobs": [{"name": "Wah Position", "minimum": 0., "maximum": 100.}]}
        events = audit.events(case, 1000)
        names = [e["name"] for e in events]
        self.assertEqual(len(set(e["block"] for e in events)), len(events))
        self.assertLess(max(e["block"] for e in events), 1000 * 3 // 4 + 1)
        self.assertEqual(names.count("Pre 1 Wah Position"), 2)
        self.assertEqual([e["value"] for e in events if e["name"] == "Pre 1 Wah Position"], [30., 70.])
        # Modulation starts after every control's first move, then depths change again.
        first_modulation = names.index("LFO Target")
        self.assertGreater(first_modulation, names.index("Pre 1 Wah Position"))
        self.assertEqual(names.count("LFO Target"), 1)
        self.assertEqual(names.count("LFO Depth"), 2)
        # Enumerated targets are sent as display text, not numeric text.
        self.assertIn({"block": events[first_modulation]["block"], "name": "LFO Target", "value": "Reverb mix"}, events)

    def test_latched_capture_cases_render_without_audit_events(self):
        for control in audit.LATCHED:
            self.assertEqual(audit.events({"controls": {control: 1}, "knobs": []}, 1000), [])
        self.assertTrue(audit.events({"controls": {"Freeze Hold": 0}, "knobs": []}, 1000))


if __name__ == "__main__":
    unittest.main()
