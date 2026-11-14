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
_notes = {}
_original_label = gen_ui.label_data


def _label_data(label_type, colour, height=30.0, justification="left", text=None):
    # A little smaller than the SDK's 26/30 px, so names like "Gate threshold" fit beside a knob.
    if label_type in ("Name", "Value"):
        height = {26.0: 23.0, 30.0: 27.0, 24.0: 22.0}.get(float(height), height)
    return _original_label(label_type, colour, height, justification, text)
_original_tab = gen_ui.tab_component
_original_build = gen_ui.build_tui


def set_field(obj, key, value):
    for i, (name, _) in enumerate(obj):
        if name == key:
            obj[i] = (key, value)
            return
    obj.append((key, value))


def text_entry(name, bounds, text=None, handle=None, colour="ffe8ebef", height=22.0, invalidating=None):
    data = _original_label("Value" if handle else "Text", colour, height, "left verticallyCentred", text)
    return gen_ui.entry(name, "Label", data, bounds, "No", "Hide" if handle or invalidating else "Show", "Always",
                        remap=[("Data", handle)] if handle else [], invalidating=invalidating)


def library_labels():
    """Native skin labels shared with the deterministic gallery renderer."""
    labels = []
    for section, title, x in (("Amp", "CAPTURE", 38), ("Cab", "CABINET IR", 678)):
        labels.extend([
            {"name": section + " title", "bounds": [x, 10, 560, 32], "text": title, "accent": True},
            {"name": section + " status", "bounds": [x, 48, 560, 32], "param": section + " Info 1", "accent": True},
            {"name": section + " reason", "bounds": [x, 82, 560, 30], "param": section + " Info 2", "size": 20},
            {"name": section + " file heading", "bounds": [x, 121, 560, 26], "text": "FILE", "accent": True, "size": 18},
        ])
        for line in range(10):
            labels.append({"name": section + " name " + str(line), "bounds": [x, 150 + 22 * line, 560, 22],
                           "param": section + " Info " + str(line + 3), "size": 17})
        labels.append({"name": section + " folder heading", "bounds": [x, 374, 560, 24], "text": "FOLDER", "accent": True, "size": 18})
        for line in range(3):
            labels.append({"name": section + " folder " + str(line), "bounds": [x, 403 + 23 * line, 560, 23],
                           "param": section + " Info " + str(line + 13), "size": 17})
        labels.append({"name": section + " browse hint", "bounds": [x, 550, 560, 32],
                       "text": "Choose Folder and " + ("Capture on AMP." if section == "Amp" else "IR on CAB."), "size": 20})
    labels.extend([
        {"name": "normalize heading", "bounds": [38, 480, 560, 26], "text": "NORMALIZE", "accent": True, "size": 18},
        {"name": "normalize status", "bounds": [38, 510, 560, 28], "param": "Normalize Status", "size": 20},
    ])
    return labels


def _tab_component(page, style):
    comp = _original_tab(page, style)
    children = dict(comp)["componentsData"]
    # Hide generic controls (and their fixed segment captions) when the current module
    # leaves their name empty. Conditional notes keep the SDK's normal Hide behavior.
    for child in children:
        name = dict(dict(child)["componentData"])["name"]
        for ctl in page.controls:
            if ctl.param and (name == ctl.param.name or name.startswith(ctl.param.name + " seg ")):
                set_field(dict(child)["bounds"], "showWhenDataModelInvalid", "HideOnInvalidName")
                if name != ctl.param.name:
                    set_field(child, "handle remapping", gen_ui.obj(("version", 1), ("map", [gen_ui.obj(("key", "Data"), ("value", ctl.param.handle))])))
                break
    notes = _notes.get(page.name, {})
    if notes.get("library"):
        for c, x in enumerate((15, 655)):
            children.append(gen_ui.entry("library panel " + str(c), "columnPanel", gen_ui.obj(("version", 1), ("handleName", "Data")),
                                         "%d 4 614 608" % x, "No", "Show", "Always"))
        for label in library_labels():
            handle = _handles.get(label.get("param"))
            children.append(text_entry(label["name"], " ".join(map(str, label["bounds"])), label.get("text"), handle,
                                       style["accent"] if label.get("accent") else style["text"], label.get("size", 22)))
        return comp
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
                                      bounds, "No", "Hide" if anchor else "Show", "Always",
                                      remap=[("Data", _handles[texts[c][1:]])],
                                      invalidating=["FilterEmptyStrings/" + _handles[anchor]] if anchor else None))
        elif texts[c]:
            data = _original_label("Text", style["accent"], 22.0, "left", texts[c])
            extra.append(gen_ui.entry("heading %d" % c, "Label", data, bounds, "No", show, "Always", remap=remap))
    children[0:0] = extra
    if notes.get("empty"):
        hint = notes["empty"]
        condition = "IndexedEnabling/%d/%d/%s" % (hint["choice"], hint["count"], _handles[hint["param"]])
        children.extend([
            text_entry("empty title", "38 130 560 42", hint["title"], colour=style["accent"], height=28, invalidating=[condition]),
            text_entry("empty help", "38 182 560 70", hint["text"], colour=style["text"], height=23, invalidating=[condition]),
        ])
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


def generate(params_json, ui_json, out_dir, kit_cache, headings, handles, anchors=None, notes=None):
    """Runs gen_ui.py. `headings` maps page names to four column headings; a heading "@<name>"
    shows that parameter's value, with `handles` mapping names to "Parameter N". Returns True on success."""
    _headings.clear()
    _headings.update(headings)
    _handles.clear()
    _handles.update(handles)
    _anchors.clear()
    _anchors.update(anchors or {})
    _notes.clear()
    _notes.update(notes or {})
    try:
        code = gen_ui.main(["--params", params_json, "--ui", ui_json, "--out", out_dir, "--kit-cache", kit_cache, "--quiet"])
    except SystemExit as e:
        code = e.code
    return code in (0, None)
