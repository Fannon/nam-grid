"""Skins for the NAM Grid MPC plugins: the SDK's gen_ui.py with grouped columns.

Every page is a 4 x 4 grid whose columns are the Q-Link groups (bank direction Column: on an
MPC with four Q-Links, the bank buttons step through the columns, each read top to bottom).
On top of the SDK's generator this adds, per used column, a panel with a heading, and makes
each column's Q-Link focus rectangle the same size as its panel, so the grouping is visible
before a control is touched and the highlight fits it.
"""

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SDK_TOOLS = os.path.join(ROOT, "mpc-plugin-sdk", "tools")
if SDK_TOOLS not in sys.path:
    sys.path.insert(0, SDK_TOOLS)
import gen_ui  # noqa: E402

PANEL_COLOUR = "ff171a1e"  # The desktop editor's surface colour.
# One rectangle per column, enclosing its heading and its four slots with an 8 px margin.
COLUMN_BOUNDS = ["%d 4 %d 608" % (x - 8, gen_ui.SLOT_W + 16) for x in gen_ui.SLOT_X]
_headings = {}
_handles = {}
_anchors = {}
_original_label = gen_ui.label_data


def _label_data(label_type, colour, height=30.0, justification="left", text=None):
    # A little smaller than the SDK's 26/30 px, so names like "Gate threshold" fit beside a knob.
    if label_type in ("Name", "Value"):
        height = {26.0: 23.0, 30.0: 27.0, 24.0: 22.0}.get(float(height), height)
    return _original_label(label_type, colour, height, justification, text)
_original_tab = gen_ui.tab_component
_original_build = gen_ui.build_tui


def _tab_component(page, style):
    comp = _original_tab(page, style)
    children = dict(comp)["componentsData"]
    used = sorted({i % 4 for i, ctl in enumerate(page.controls) if ctl.widget != "empty"})
    texts = _headings.get(page.name, [""] * 4)
    anchors = _anchors.get(page.name, [None] * 4)
    extra = []
    for c in used:
        # A column whose controls the current module does not use disappears with them: its
        # panel and heading follow the visibility (name) of the column's anchor control.
        anchor = anchors[c]
        show = "HideOnInvalidName" if anchor else "Show"
        remap = [("Data", _handles[anchor])] if anchor else []
        # Panels first, so they are drawn behind the controls; they ignore touches.
        extra.append(gen_ui.entry("panel %d" % c, "columnPanel", gen_ui.obj(("version", 1), ("handleName", "Data")),
                                  COLUMN_BOUNDS[c], "No", show, "Always", remap=remap))
        bounds = "%d 8 %d 32" % (gen_ui.SLOT_X[c] + 15, gen_ui.SLOT_W - 30)
        if texts[c].startswith("@"):
            # A live heading: the value of a parameter, such as the chosen voice or style.
            extra.append(gen_ui.entry("heading %d" % c, "Label", _original_label("Value", style["accent"], 22.0, "left"),
                                      bounds, "No", "Show", "Always", remap=[("Data", _handles[texts[c][1:]])]))
        elif texts[c]:
            data = _original_label("Text", style["accent"], 22.0, "left", texts[c])
            extra.append(gen_ui.entry("heading %d" % c, "Label", data, bounds, "No", show, "Always", remap=remap))
    children[0:0] = extra
    return comp


def _build_tui(*args, **kwargs):
    tui = _original_build(*args, **kwargs)
    page_data = dict(tui)["pageData"]
    definitions = dict(dict(page_data)["componentDefinitions"])["localComponentDefinitions"]
    definitions.append(gen_ui.obj(("key", "columnPanel"),
                                  ("value", gen_ui.component_definition([], [], PANEL_COLOUR, ignore_mouse=True))))
    return tui


gen_ui.tab_component = _tab_component
gen_ui.label_data = _label_data
gen_ui.build_tui = _build_tui
gen_ui.QLINK_BOUNDS = COLUMN_BOUNDS


def generate(params_json, ui_json, out_dir, kit_cache, headings, handles, anchors=None):
    """Runs gen_ui.py. `headings` maps page names to four column headings; a heading "@<name>"
    shows that parameter's value, with `handles` mapping names to "Parameter N". Returns True on success."""
    _headings.clear()
    _headings.update(headings)
    _handles.clear()
    _handles.update(handles)
    _anchors.clear()
    _anchors.update(anchors or {})
    try:
        code = gen_ui.main(["--params", params_json, "--ui", ui_json, "--out", out_dir, "--kit-cache", kit_cache, "--quiet"])
    except SystemExit as e:
        code = e.code
    return code in (0, None)
