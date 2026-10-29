#!/usr/bin/env python3
"""Compare module control tables with current compiled metadata, never a saved snapshot."""
import argparse
import json
import math
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
NUMBER = r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)"
RANGE = re.compile(r"^\s*(%s)\s*(?:to|[–—-])\s*(%s)\s*(.*?)\s*$" % (NUMBER, NUMBER))
VALUE = re.compile(r"^\s*(%s)\s*(.*?)\s*$" % NUMBER)


def plain(text):
    return re.sub(r"\s+", " ", text.replace("**", "").replace("`", "").replace("−", "-")).strip()


def key(text):
    return plain(text).casefold()


def unit(text):
    return re.sub(r"\s+", "", text).replace("×", "x").casefold()


def control_tables(text):
    """Yield module heading, line number and cells only inside control tables."""
    module, active, fence = None, False, None
    for line_number, line in enumerate(text.splitlines(), 1):
        stripped = line.strip()
        marker = re.match(r"^(`{3,}|~{3,})", stripped)
        if marker:
            token = marker[1][0]
            if fence is None: fence = token
            elif fence == token: fence = None
            continue
        if fence is not None: continue
        if stripped.startswith("## "):
            module = plain(stripped[3:]); active = False
        if not stripped.startswith("|"):
            active = False
            continue
        cells = [plain(cell) for cell in re.split(r"(?<!\\)\|", stripped)[1:-1]]
        if len(cells) >= 3 and [key(c) for c in cells[:3]] == ["control", "range", "start"]:
            active = True
            continue
        if not active or not cells or all(re.fullmatch(r"[: -]+", c) for c in cells): continue
        if len(cells) < 3:
            raise ValueError("line %d: incomplete control row" % line_number)
        yield module, line_number, cells[:3]


def compare_values(control, range_text, start_text):
    if control["binary"]:
        choices = control["choices"]
        expected_start = choices[1 if control["default"] >= .5 else 0]
        if [key(c) for c in range_text.split("/")] != list(map(key, choices)) or key(start_text) != key(expected_start):
            return "expected %s / %s, start %s" % (choices[0], choices[1], expected_start)
        return None
    bounds, start = RANGE.fullmatch(plain(range_text)), VALUE.fullmatch(plain(start_text))
    if not bounds or not start:
        return "unrecognised range/start format: %r / %r" % (range_text, start_text)
    actual = (float(bounds[1]), float(bounds[2]), float(start[1]))
    expected = (control["minimum"], control["maximum"], control["display_default"])
    if not all(math.isclose(a, b, rel_tol=1.e-9, abs_tol=1.e-9) for a, b in zip(actual, expected)):
        return "range/start %s differs from code %s" % (actual, expected)
    if unit(bounds[3]) != unit(control["unit"]) or unit(start[2]) != unit(control["unit"]):
        return "range/start units %r / %r differ from code %r" % (bounds[3], start[2], control["unit"])
    return None


def check_documents(documents, metadata):
    failures, seen, count = [], {}, 0
    for path, text in documents.items():
        try:
            rows = list(control_tables(text))
        except ValueError as error:
            failures.append("%s: %s" % (path, error)); continue
        for module, line, cells in rows:
            where = "%s:%d (%s / %s)" % (path, line, module, cells[0])
            if module not in metadata:
                failures.append(where + ": no compiled module metadata"); continue
            label = key(cells[0])
            matches = [i for i, c in enumerate(metadata[module]) if label in map(key, c["labels"])]
            if not matches and " / " in label:
                matches = [i for i, c in enumerate(metadata[module]) if label.split(" / ")[0] in map(key, c["labels"])]
            if len(matches) != 1:
                failures.append(where + ": control label must match exactly one compiled control"); continue
            index = matches[0]
            visited = seen.setdefault(module, set())
            if index in visited:
                failures.append(where + ": duplicate control row")
            visited.add(index)
            error = compare_values(metadata[module][index], cells[1], cells[2])
            if error: failures.append(where + ": " + error)
            count += 1
    for module, controls in metadata.items():
        for index, control in enumerate(controls):
            if index not in seen.get(module, set()):
                failures.append("%s: missing control table row for %s" % (module, control["labels"][0]))
    return failures, count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", type=Path, required=True, help="current nam-parameter-contract executable")
    parser.add_argument("--plugin", type=Path, required=True, help="current CLAP binary")
    parser.add_argument("--docs", type=Path, default=ROOT / "docs/modules")
    args = parser.parse_args()
    result = subprocess.run([str(args.host.resolve()), str(args.plugin.resolve()), "--module-controls"],
                            capture_output=True, text=True, timeout=30)
    if result.returncode:
        print("Compiled metadata command failed:\n" + result.stderr, file=sys.stderr)
        return 1
    metadata = json.loads(result.stdout.splitlines()[-1])
    documents = {str(p): p.read_text(encoding="utf-8") for p in sorted(args.docs.glob("*.md"))}
    failures, count = check_documents(documents, metadata)
    if failures:
        print("Module docs differ from code:\n" + "\n".join(failures), file=sys.stderr)
        return 1
    print("Module docs: %d controls across %d modules match compiled ranges, units and insertion defaults" % (count, len(metadata)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
