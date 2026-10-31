"""Permanent IDs stay unique, contiguous and append-only.

Two branches can each append the "next" parameter ID, module ID, state schema or host
parameter segment. Merged, the tree may still compile with two names on one ID, so these
checks read the sources directly:

* within the tree: UtilityParam and SlotEffect values are unique and contiguous, and there is
  one parameter descriptor per UtilityParam;
* against main (origin/main, or NAM_GRID_COMPARE_REF), when available: every ID, descriptor
  storage name, algorithm identity and host-parameter segment on main is unchanged, and
  everything new is appended after it with a newer state schema.
"""
import os
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARAMETERS = "NeuralAmpModeler/EffectParameters.h"
MODULES = "NeuralAmpModeler/SlotDynamics.h"
LAYOUT = "NeuralAmpModeler/ParameterLayout.h"


def enum_values(text, start):
    """(name, value) pairs of an enum whose body follows `start`, in source order."""
    body = text[text.index(start):]
    body = body[body.index("{") + 1:body.index("};")]
    return [(name, int(value)) for name, value in re.findall(r"^\s*(\w+)\s*=\s*(\d+)\s*,", body, re.M)]


def descriptors(text):
    """(storage name, introducedIn) per parameter descriptor, in table order."""
    body = text[text.index("kEffectParameters{"):]
    body = body[:body.index("}}};") + 4]
    rows = re.findall(r'\{\{?"([^"]+)",\s*"[^"]*",([^}]*)\}', body)
    result = []
    for name, rest in rows:
        fields = [f.strip() for f in rest.split(",")]
        # initial, minimum, maximum, step, unit, position[, exponential, power, introducedIn]
        result.append((name, int(float(fields[-1])) if len(fields) >= 9 else 1))
    return result


def identities(text):
    """Algorithm identity arrays: name -> [(value, key)]."""
    return {name: [(int(v), k) for v, k in re.findall(r'\{(\d+),\s*"([^"]+)"\}', items)]
            for name, items in re.findall(r"inline constexpr AlgorithmIdentity (\w+)\[\]\{(.*?)\};", text, re.S)}


def schema(text):
    return int(re.search(r"kEffectStateSchema = (\d+);", text).group(1))


def segments(text):
    line = re.search(r"inline constexpr Segment segments\[\]\{(.*?)\};", text).group(1)
    return [tuple(map(int, pair)) for pair in re.findall(r"\{(\d+),\s*(\d+)\}", line)]


def read(path, ref=None):
    if ref is None:
        return (ROOT / path).read_text(encoding="utf-8")
    return subprocess.run(["git", "show", f"{ref}:{path}"], cwd=ROOT, check=True, capture_output=True,
                          text=True, encoding="utf-8").stdout


def compare_ref():
    ref = os.environ.get("NAM_GRID_COMPARE_REF", "origin/main")
    try:
        subprocess.run(["git", "rev-parse", "--verify", "--quiet", ref + "^{commit}"], cwd=ROOT, check=True,
                       capture_output=True)
        return ref
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None


class ParameterIdTests(unittest.TestCase):
    def test_ids_unique_and_contiguous(self):
        text = read(PARAMETERS)
        params = enum_values(text, "enum UtilityParam")
        self.assertEqual([v for _, v in params], list(range(len(params))),
                         "UtilityParam IDs must be unique and contiguous; was an ID taken twice?")
        self.assertEqual(len(descriptors(text)), len(params), "One descriptor per UtilityParam")
        modules = enum_values(read(MODULES), "enum class SlotEffect")
        self.assertEqual([v for _, v in modules], list(range(len(modules))),
                         "SlotEffect IDs must be unique and contiguous")
        names = [name for name, _ in descriptors(text)]
        self.assertEqual(len(names), len(set(names)), "Descriptor storage names must be unique")

    def test_append_only_against_main(self):
        ref = compare_ref()
        if ref is None:
            self.skipTest("origin/main is not available")
        old_text, new_text = read(PARAMETERS, ref), read(PARAMETERS)
        old_params, new_params = enum_values(old_text, "enum UtilityParam"), enum_values(new_text, "enum UtilityParam")
        self.assertEqual(new_params[:len(old_params)], old_params, f"UtilityParam IDs on {ref} changed or were reused")
        old_desc, new_desc = descriptors(old_text), descriptors(new_text)
        self.assertEqual([n for n, _ in new_desc[:len(old_desc)]], [n for n, _ in old_desc],
                         f"Descriptor storage names on {ref} changed")
        old_schema, new_schema = schema(old_text), schema(new_text)
        self.assertGreaterEqual(new_schema, old_schema)
        for name, introduced in new_desc[len(old_desc):]:
            self.assertGreater(introduced, old_schema,
                               f'"{name}" must use a state schema newer than {ref}\'s {old_schema}')
        old_modules = enum_values(read(MODULES, ref), "enum class SlotEffect")
        # Explicitly retired before release; keep the numeric reservation so other
        # module IDs stay unchanged. This is not permission to retire released IDs.
        old_modules = [("Reserved63", value) if (name, value) == ("SignalMonitor", 63)
                       else (name, value) for name, value in old_modules]
        new_modules = enum_values(read(MODULES), "enum class SlotEffect")
        self.assertEqual(new_modules[:len(old_modules)], old_modules, f"SlotEffect IDs on {ref} changed or were reused")
        old_ids, new_ids = identities(old_text), identities(new_text)
        for name, values in old_ids.items():
            self.assertEqual(new_ids.get(name, [])[:len(values)], values, f"Algorithm identities {name} changed")
        old_segments, new_segments = segments(read(LAYOUT, ref)), segments(read(LAYOUT))
        self.assertEqual(new_segments[:len(old_segments)], old_segments,
                         f"Host parameter segments on {ref} changed; append a new segment instead")


if __name__ == "__main__":
    unittest.main()
