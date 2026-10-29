"""Regression tests for the compiled-metadata module documentation check."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("check_module_docs", ROOT / "scripts/check-module-docs.py")
docs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(docs)

TABLE = """## Example

| Control | Range | Start | What it does |
| --- | --- | --- | --- |
| **Rate** | 0.05–10 Hz | 0.8 Hz | Speed. |
| **Hold** | Off / On | Off | Capture. |
| **Q** | 0.1–4 | 0.71 | Resonance. |
"""


def metadata():
    return {"Example": [
        {"labels": ["Rate"], "minimum": .05, "maximum": 10., "default": .8,
         "display_default": .8, "unit": "Hz", "binary": False},
        {"labels": ["Hold"], "minimum": 0., "maximum": 1., "default": 0.,
         "display_default": 0., "unit": "", "binary": True, "choices": ["Off", "On"]},
        {"labels": ["Q"], "minimum": .1, "maximum": 4., "default": .7071067811865476,
         "display_default": .71, "unit": "", "binary": False}]}


class ModuleDocsTests(unittest.TestCase):
    def check(self, text=TABLE, controls=None):
        return docs.check_documents({"example.md": text}, metadata() if controls is None else controls)

    def test_current_values_and_display_rounding(self):
        self.assertEqual(([], 3), self.check())

    def test_wrong_range_fails(self):
        failures, _ = self.check(TABLE.replace("0.05–10 Hz", "0.05–11 Hz"))
        self.assertTrue(any("differs from code" in f for f in failures))

    def test_wrong_start_and_units_fail(self):
        for old, new in [("0.8 Hz", "0.9 Hz"), ("0.71", "0.70"), ("0.8 Hz", "0.8 ms"), ("| Off |", "| On |")]:
            with self.subTest(change=new):
                self.assertTrue(self.check(TABLE.replace(old, new))[0])

    def test_numeric_notation(self):
        c = {"minimum": -12., "maximum": 12., "default": 0., "display_default": 0., "unit": "dB", "binary": False}
        self.assertIsNone(docs.compare_values(c, "−12 to +12 dB", "0 dB"))
        c.update(minimum=1., maximum=20., default=2.5, display_default=2.5, unit=":1")
        self.assertIsNone(docs.compare_values(c, "1–20:1", "2.5:1"))
        c.update(minimum=.25, maximum=2., default=1., display_default=1., unit="x")
        self.assertIsNone(docs.compare_values(c, "0.25–2×", "1×"))

    def test_binary_display_labels_come_from_plugin(self):
        c = {"binary": True, "default": 1., "choices": ["Fast", "Slow"]}
        self.assertIsNone(docs.compare_values(c, "Fast / Slow", "Slow"))
        self.assertIsNotNone(docs.compare_values(c, "Off / On", "On"))

    def test_unknown_duplicate_missing_and_malformed_rows_fail(self):
        for text in [TABLE.replace("**Rate**", "**Unknown**"),
                     TABLE + "| **Rate** | 0.05–10 Hz | 0.8 Hz | Duplicate. |\n",
                     TABLE.replace("| **Hold** | Off / On | Off | Capture. |\n", ""),
                     TABLE.replace("0.05–10 Hz", "fast-ish")]:
            with self.subTest(text=text):
                self.assertTrue(self.check(text)[0])

    def test_missing_module_table_fails(self):
        self.assertTrue(self.check("# No table\n")[0])

    def test_prose_example_tables_and_fences_are_not_control_tables(self):
        text = """# Examples
| Style | Range | Start |
| --- | --- | --- |
| **Rate** | not a range | not a start |
```markdown
## Example
| Control | Range | Start |
| --- | --- | --- |
| Fake | 1–2 | 1 |
```
""" + TABLE
        self.assertEqual(([], 3), self.check(text))

    def test_conditional_label_alias_and_whitespace(self):
        text = TABLE.replace("**Rate**", "**Rate / Speed**").replace("| Control | Range | Start |", "|Control|Range|Start|")
        self.assertEqual(([], 3), self.check(text))


if __name__ == "__main__":
    unittest.main()
