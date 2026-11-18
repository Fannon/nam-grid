#!/usr/bin/env python3
"""Render docs/public/logo.svg into the standalone app's Windows icon (nam-grid.ico).

The logo is a grid of rounded squares, so this script rasterizes those <rect> elements
itself instead of needing an SVG library. Cell edges snap to whole pixels, which keeps the
gaps crisp at 16 and 24 px; corners are antialiased by supersampling. Each size is stored
as a PNG entry, which Windows reads from Vista on.

  python scripts/make-app-icon.py            # writes NeuralAmpModeler/resources/nam-grid.ico
"""
import argparse
import math
import struct
import xml.etree.ElementTree as ET
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)
SUPERSAMPLING = 4


def read_cells(svg_path):
    tree = ET.parse(svg_path)
    view_box = [float(v) for v in tree.getroot().get("viewBox").split()]
    cells = []
    for rect in tree.getroot().iter("{http://www.w3.org/2000/svg}rect"):
        colour = rect.get("fill").lstrip("#")
        cells.append({
            "x": float(rect.get("x")), "y": float(rect.get("y")),
            "w": float(rect.get("width")), "h": float(rect.get("height")),
            "r": float(rect.get("rx", 0)),
            "rgb": tuple(int(colour[i:i + 2], 16) for i in (0, 2, 4)),
            "opacity": float(rect.get("fill-opacity", 1)),
        })
    return view_box[2], cells


def inside_rounded(px, py, x0, y0, x1, y1, radius):
    cx = min(max(px, x0 + radius), x1 - radius)
    cy = min(max(py, y0 + radius), y1 - radius)
    return (px - cx) ** 2 + (py - cy) ** 2 <= radius ** 2


def render(size, extent, cells):
    scale = size / extent
    pixels = bytearray(size * size * 4)  # RGBA, transparent background
    snap = lambda v: math.floor(v * scale + .5)
    for cell in cells:
        x0, y0 = snap(cell["x"]), snap(cell["y"])
        x1, y1 = snap(cell["x"] + cell["w"]), snap(cell["y"] + cell["h"])
        radius = min(cell["r"] * scale, (x1 - x0) / 2, (y1 - y0) / 2)
        for py in range(y0, y1):
            for px in range(x0, x1):
                hits = sum(inside_rounded(px + (i + .5) / SUPERSAMPLING, py + (j + .5) / SUPERSAMPLING,
                                          x0, y0, x1, y1, radius)
                           for i in range(SUPERSAMPLING) for j in range(SUPERSAMPLING))
                alpha = round(255 * cell["opacity"] * hits / SUPERSAMPLING ** 2)
                offset = (py * size + px) * 4
                pixels[offset:offset + 4] = bytes((*cell["rgb"], alpha))
    return pixels


def png(size, pixels):
    rows = b"".join(b"\0" + bytes(pixels[y * size * 4:(y + 1) * size * 4]) for y in range(size))
    chunk = lambda kind, data: (struct.pack(">I", len(data)) + kind + data
                                + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def ico(images):
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = len(header) + 16 * len(images)
    entries, data = b"", b""
    for size, image in images:
        entries += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(image), offset + len(data))
        data += image
    return header + entries + data


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--svg", type=Path, default=ROOT / "docs" / "public" / "logo.svg")
    parser.add_argument("--output", type=Path, default=ROOT / "NeuralAmpModeler" / "resources" / "nam-grid.ico")
    args = parser.parse_args()
    extent, cells = read_cells(args.svg)
    images = [(size, png(size, render(size, extent, cells))) for size in SIZES]
    args.output.write_bytes(ico(images))
    print(f"Wrote {args.output} ({', '.join(str(s) for s in SIZES)} px)")


if __name__ == "__main__":
    main()
