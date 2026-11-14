#!/usr/bin/env python3
"""Deterministic SVG previews from the MPC inspector's live parameter snapshots.

Uses the generated skin geometry and bitmap assets; no browser, network, UI windows or
image libraries are needed. Device font rendering and touch behavior require hardware.
Run through `mpc/build.py --gallery tmp/<task>/gallery` after building native libraries.
"""
import base64
import hashlib
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

import skins

SVG = "http://www.w3.org/2000/svg"
ET.register_namespace("", SVG)


def element(parent, tag, **attributes):
    return ET.SubElement(parent, "{" + SVG + "}" + tag, {k.replace("_", "-"): str(v) for k, v in attributes.items()})


def rectangle(parent, x, y, width, height, fill, **attrs):
    return element(parent, "rect", x=x, y=y, width=width, height=height, fill=fill, **attrs)


def text(parent, x, y, width, height, value, colour="#e8ebef", size=23):
    # A nested SVG clips labels to the same bounds as the native component.
    clip = element(parent, "svg", x=x, y=y, width=width, height=height, overflow="hidden")
    label = element(clip, "text", x=0, y=size, fill=colour, font_size=size, font_family="Titillium Web, sans-serif")
    label.text = value


def asset(path):
    return "data:image/png;base64," + base64.b64encode(Path(path).read_bytes()).decode("ascii")


def render_page(build, plugin, page, state, params):
    names = {p["name"]: p for p in params["params"]}
    live = {p["staticName"]: p for p in state["params"].values()}
    shown = {name: plugin.shown[str(p["index"])] for name, p in names.items() if str(p["index"]) in plugin.shown}
    controls, headings, anchors = build.page_layout(page, names, plugin.show_output, shown)
    accent = "#" + plugin.pages["colour"][2:]
    root = ET.Element("{" + SVG + "}svg", {"width": "1280", "height": "720", "viewBox": "0 0 1280 720"})
    rectangle(root, 0, 0, 1280, 720, "#000")
    text(root, 10, 2, 1260, 28, plugin.name + " / " + page["name"] + " / " + state["label"], "#b7bec8", 16)
    body = element(root, "g", transform="translate(0 32)")
    rectangle(body, 0, 0, 1280, 628, "#0e1013")
    visible = []
    if page.get("library"):
        for x in (15, 655):
            rectangle(body, x, 4, 614, 608, "#171a1e")
        for label in skins.library_labels():
            value = live[label["param"]]["display"] if "param" in label else label["text"]
            text(body, *label["bounds"], value, accent if label.get("accent") else "#e8ebef", label.get("size", 22))
    else:
        used = sorted({i % 4 for i, c in enumerate(controls) if c != "empty"})
        for col in used:
            if anchors[col] and not live[anchors[col]]["name"]:
                continue
            rectangle(body, *map(int, skins.COLUMN_BOUNDS[col].split()), "#171a1e")
            heading = headings[col]
            if heading.startswith("@"):
                heading = live[heading[1:]]["display"]
            text(body, skins.gen_ui.SLOT_X[col] + 15, 8, 248, 32, heading, accent, 22)
        folder = Path(plugin.out) / "Plugin Skins"
        assets = {}
        definitions = element(root, "defs")

        def image(name, x, y, width, height, frame=None):
            if name not in assets:
                assets[name] = "asset-" + str(len(assets))
                element(definitions, "image", id=assets[name], x=0, y=0, width=width,
                        height=height * (127 if frame is not None else 1), href=asset(folder / name))
            target = body
            if frame is not None:
                target = element(body, "svg", x=x, y=y, width=width, height=height,
                                 viewBox="0 %d %d %d" % (frame * height, width, height), overflow="hidden")
                x = y = 0
            element(target, "use", x=x, y=y, href="#" + assets[name])

        for i, control in enumerate(controls):
            if control == "empty":
                continue
            value = live[control["param"]]
            if not value["name"]:
                continue
            visible.append(control["param"])
            row, col = divmod(i, 4)
            x, y = skins.gen_ui.SLOT_X[col], skins.gen_ui.SLOT_Y[row]
            metadata = names[control["param"]]
            if control["widget"] == "knob":
                image("knob_uni.png", x + 8, y + 8, 90, 90, round(value["norm"] * 126))
                text(body, x + 110, y + 12, 156, 36, value["name"], accent)
                text(body, x + 110, y + 52, 156, 42, value["display"], size=27)
            elif control["widget"] == "switch":
                if metadata["kind"] == "bool":
                    text(body, x + 15, y + 12, 170, 36, value["name"], accent)
                    text(body, x + 15, y + 52, 170, 42, value["display"], size=27)
                    image("toggle_on.png" if value["norm"] >= .5 else "toggle_off.png", x + 196, y + 34, 64, 32)
                else:
                    choices = metadata["choices"]
                    width, offsets = skins.gen_ui.SEGMENTS[len(choices)]
                    selected = round(value["norm"] * (len(choices) - 1))
                    text(body, x + 15, y + 8, 248, 34, value["name"], accent, 22)
                    for n, (caption, offset) in enumerate(zip(choices, offsets)):
                        image("seg%d_%s.png" % (len(choices), "on" if n == selected else "off"), x + offset, y + 54, width, 40)
                        text(body, x + offset + 5, y + 57, width - 10, 34, caption, size=20)
            else:
                text(body, x + 15, y + 12, 248, 36, value["name"], accent)
                text(body, x + 15, y + 52, 248, 42, value["display"], size=27)
                rectangle(body, x + 15, y + 89, 248, 4, "#4e555f")
        if "empty" in page:
            hint = page["empty"]
            count = len(names[hint["param"]]["choices"])
            if round(live[hint["param"]]["norm"] * (count - 1)) == hint["choice"]:
                text(body, 38, 130, 560, 42, hint["title"], accent, 28)
                text(body, 38, 182, 560, 70, hint["text"])
    x = 8
    for tab in plugin.pages["pages"]:
        width = max(88, len(tab["name"]) * 14 + 30)
        rectangle(root, x, 668, width, 44, "#31353b" if tab["name"] == page["name"] else "#0e1013", stroke="#4e555f")
        text(root, x + 12, 676, width - 24, 32, tab["name"], size=20)
        x += width + 8
    return ET.tostring(root, encoding="unicode"), visible


def relevant(page, state):
    section = state.get("section", "")
    if not section:
        return True
    if section == "library":
        return bool(page.get("library"))
    if page.get("section", "").lower() == section:
        return True
    return any(name.split(" ")[0].lower() == section for col in page.get("columns", []) for name in col)


def generate(build, snapshots, destination, plugins=None):
    output = Path(destination)
    output.mkdir(parents=True, exist_ok=True)
    records = []
    for plugin in plugins if plugins is not None else build.all_plugins():
        params = json.loads((Path(plugin.work) / "params.json").read_text(encoding="utf-8"))
        manifest = json.loads((Path(plugin.work) / "menus.json").read_text(encoding="utf-8"))
        plugin.show_output, plugin.shown = manifest["showOutput"], manifest["shown"]
        for state in snapshots[plugin.id]:
            for page in plugin.pages["pages"]:
                if not relevant(page, state):
                    continue
                svg, visible = render_page(build, plugin, page, state, params)
                name = re.sub(r"[^a-zA-Z0-9_-]", "-", plugin.id + "-" + page["name"] + "-" + state["label"]) + ".svg"
                (output / name).write_text(svg, encoding="utf-8", newline="\n")
                records.append({"file": name, "plugin": plugin.id, "page": page["name"], "state": state["label"],
                                "visibleControls": visible, "sha256": hashlib.sha256(svg.encode()).hexdigest()})
    index = ET.Element("{" + SVG + "}svg", {"width": "1280", "height": str(((len(records) + 2) // 3) * 270 + 54)})
    rectangle(index, 0, 0, "100%", "100%", "#0e1013")
    text(index, 16, 8, 1248, 40, "NAM Grid MPC gallery - select a preview for full size", size=24)
    for n, record in enumerate(records):
        x, y = (n % 3) * 424 + 8, (n // 3) * 270 + 54
        link = element(index, "a", href=record["file"])
        element(link, "image", x=x, y=y, width=416, height=234, href=record["file"])
        text(link, x, y + 235, 416, 30, record["file"][:-4], size=15)
    (output / "index.svg").write_text(ET.tostring(index, encoding="unicode"), encoding="utf-8", newline="\n")
    (output / "manifest.json").write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")
    return len(records)
