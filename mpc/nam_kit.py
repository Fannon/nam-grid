"""NAM Grid look for the MPC skins: knobs, faders, toggles and selector segments drawn like
the desktop editor (grid_ui in NeuralAmpModeler/NAMGridControls.h), at the exact sizes and
frame counts of the SDK's UI kit, so they replace its PNGs one for one.

Knob: a dark disc with a 1 px border, a 2 px track over 270 degrees, a light value arc and a
light pointer. Toggles are green when on and red when off. Selector segments are flat dark
buttons; the chosen one is lighter with a light border, like the desktop's option buttons.
Rendering uses the SDK's own raster primitives (make_ui_kit.py) and is cached per build.
"""

import math
import os
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SDK_TOOLS = os.path.join(ROOT, "mpc-plugin-sdk", "tools")
if SDK_TOOLS not in sys.path:
    sys.path.insert(0, SDK_TOOLS)
import make_ui_kit as kit  # noqa: E402

VERSION = "nam-grid-kit-4"  # Bump when the drawing changes, to refresh the cache.

# The desktop editor's palette (grid_ui), as RGBA.
SURFACE = (23, 26, 30, 255)
RAISED = (49, 53, 59, 255)
BORDER = (78, 85, 95, 255)
MUTED = (183, 190, 200, 255)
TEXT = (232, 235, 239, 255)
ON = (76, 190, 110, 255)    # enabled toggles
OFF = (214, 76, 76, 255)    # disabled toggles
SHADOW = (0, 0, 0, 140)
SWEEP = math.radians(270.0)
FRAMES = 127


def knob(size, bipolar):
    c = kit.Canvas(size, size)
    cx = cy = size / 2.0
    track_out = size / 2.0 - 2.0
    track_in = track_out - max(2.5, size * 0.03)
    body = track_in - max(5.0, size * 0.08)
    kit.arc(c, cx, cy, track_in, track_out, -SWEEP / 2.0, SWEEP / 2.0, BORDER)
    kit.circle(c, cx, cy + 1.0, body + 1.0, SHADOW)
    kit.circle(c, cx, cy, body, BORDER)
    kit.circle(c, cx, cy, body - 1.0, SURFACE)
    rows = []
    for i in range(FRAMES):
        t = i / (FRAMES - 1.0)
        f = c.copy()
        a = -SWEEP / 2.0 + SWEEP * t
        if bipolar:
            if abs(t - .5) > .002:
                kit.arc(f, cx, cy, track_in, track_out, min(0.0, a), max(0.0, a), MUTED)
        elif t > .002:
            kit.arc(f, cx, cy, track_in, track_out, -SWEEP / 2.0, a, MUTED)
        x0, y0 = cx + math.sin(a) * body * .45, cy - math.cos(a) * body * .45
        x1, y1 = cx + math.sin(a) * (body - 3.0), cy - math.cos(a) * (body - 3.0)
        kit.capsule(f, x0, y0, x1, y1, max(2.5, size * .035), TEXT)
        rows.extend(f.rows())
    return rows


def fader(length, thickness, bipolar):
    c = kit.Canvas(length, thickness)
    cy = thickness / 2.0
    pad = thickness / 2.0
    kit.rrect(c, pad, cy - 2.0, length - 2 * pad, 4.0, 2.0, BORDER)
    rows = []
    for i in range(FRAMES):
        t = i / (FRAMES - 1.0)
        f = c.copy()
        x = pad + (length - 2 * pad) * t
        start = pad + (length - 2 * pad) * (.5 if bipolar else 0.)
        lo, hi = min(start, x), max(start, x)
        if hi - lo > .5:
            kit.rrect(f, lo, cy - 2.0, hi - lo, 4.0, 2.0, MUTED)
        kit.circle(f, x, cy + 1.0, pad - 5.0, SHADOW)
        kit.circle(f, x, cy, pad - 5.0, BORDER)
        kit.circle(f, x, cy, pad - 6.0, SURFACE)
        kit.capsule(f, x, cy - (pad - 10.0), x, cy + (pad - 10.0), 2.5, TEXT)
        rows.extend(f.rows())
    return rows


def toggle(on):
    w, h = 64, 32
    c = kit.Canvas(w, h)
    r = (h - 4) / 2.0
    kit.rrect(c, 1, 2, w - 2, h - 3, r, SHADOW)
    kit.rrect(c, 1, 1, w - 2, h - 3, r, ON if on else OFF)
    knob_r = r - 3.0
    cx = (w - 1 - knob_r - 4.0) if on else (1 + knob_r + 4.0)
    kit.circle(c, cx, 1 + r + 1.0, knob_r, SHADOW)
    kit.circle(c, cx, 1 + r, knob_r, TEXT)
    return c.rows()


def segment(width, on):
    h = 40
    c = kit.Canvas(width, h)
    kit.rrect(c, 1, 1, width - 2, h - 2, 3.0, MUTED if on else BORDER)
    kit.rrect(c, 2, 2, width - 4, h - 4, 2.0, RAISED if on else SURFACE)
    return c.rows()


def renderers():
    """Asset file name -> (render function, frame width)."""
    table = {
        "knob_uni.png": (lambda: knob(90, False), 90),
        "knob_bi.png": (lambda: knob(90, True), 90),
        "knob_small_uni.png": (lambda: knob(56, False), 56),
        "knob_small_bi.png": (lambda: knob(56, True), 56),
        "fader_h_uni.png": (lambda: fader(248, 36, False), 248),
        "fader_h_bi.png": (lambda: fader(248, 36, True), 248),
        "toggle_on.png": (lambda: toggle(True), 64),
        "toggle_off.png": (lambda: toggle(False), 64),
    }
    for n, width in ((2, 122), (3, 80), (4, 58)):
        table["seg%d_on.png" % n] = (lambda w=width: segment(w, True), width)
        table["seg%d_off.png" % n] = (lambda w=width: segment(w, False), width)
    return table


def restyle(skins_dir, cache_dir):
    """Replaces the SDK kit PNGs in a generated Plugin Skins folder with the NAM Grid look."""
    cache = os.path.join(cache_dir, VERSION)
    for name, (render, width) in renderers().items():
        target = os.path.join(skins_dir, name)
        if not os.path.isfile(target):
            continue
        cached = os.path.join(cache, name)
        if not os.path.isfile(cached):
            kit.write_png(cached, width, render())
        shutil.copyfile(cached, target)
