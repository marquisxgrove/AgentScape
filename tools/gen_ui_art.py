#!/usr/bin/env python3
"""Generates AgentScape's original 9-slice UI art (assets/ui/*.png).

Standard library only, fully deterministic (fixed-seed LCG), so the PNGs can be regenerated and
reviewed. Colours derive from the palette in .kiro/steering/art-direction.md (STONE_DARK, STONE,
PARCHMENT) plus lighter/darker bevel shades of them.

    python3 tools/gen_ui_art.py            # writes assets/ui/
"""
import os
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "ui")


def hex_rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


# Palette (art-direction.md) and derived bevel shades.
STONE_DARK = hex_rgb("#3e3529")
STONE = hex_rgb("#5d5447")
PARCHMENT = hex_rgb("#c8b88a")
OUTLINE = hex_rgb("#1e1912")
STONE_LIGHT = hex_rgb("#7a6f5e")
STONE_SHADOW = hex_rgb("#2e271e")
PARCH_EDGE = hex_rgb("#8a7650")
PARCH_LIGHT = hex_rgb("#e0d3a8")


class Lcg:
    def __init__(self, seed):
        self.s = seed & 0xFFFFFFFF

    def next(self):
        self.s = (1664525 * self.s + 1013904223) & 0xFFFFFFFF
        return self.s >> 16


def shade(rgb, delta):
    return tuple(max(0, min(255, c + delta)) for c in rgb)


def write_png(path, w, h, pixels):
    """pixels: list of rows of (r, g, b, a)."""
    raw = b"".join(b"\x00" + b"".join(struct.pack("BBBB", *p) for p in row) for row in pixels)

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def speckle(rng, base, spread):
    # Per-pixel independent noise, so the centre tiles seamlessly.
    r = rng.next() % 100
    if r < 12:
        return shade(base, -spread)
    if r < 20:
        return shade(base, spread)
    if r < 23:
        return shade(base, -2 * spread)
    return base


def panel(size, border, fill, light, dark, inner, seed, spread):
    """Raised outer bevel, recessed inner edge, speckled fill."""
    rng = Lcg(seed)
    px = []
    for y in range(size):
        row = []
        for x in range(size):
            d = min(x, y, size - 1 - x, size - 1 - y)  # distance to the outer edge
            top_left = (x <= y and x < size - 1 - y) or (y < x and y < size - 1 - x)
            if d == 0:
                c = OUTLINE
            elif d == 1:
                c = light if top_left else dark
            elif d == border - 2:
                c = inner
            elif d == border - 1:
                c = dark if top_left else light  # recessed: shadow on the top-left inner edge
            else:
                c = speckle(rng, fill, spread)
            row.append(c + (255,))
        px.append(row)
    return px


def button(size, border, fill, pressed, seed):
    rng = Lcg(seed)
    px = []
    for y in range(size):
        row = []
        for x in range(size):
            d = min(x, y, size - 1 - x, size - 1 - y)
            top_left = (x <= y and x < size - 1 - y) or (y < x and y < size - 1 - x)
            hi, lo = (STONE_SHADOW, STONE_LIGHT) if pressed else (STONE_LIGHT, STONE_SHADOW)
            if d == 0:
                c = OUTLINE
            elif d < border - 1:
                c = hi if top_left else lo
            else:
                c = speckle(rng, fill, 4)
            row.append(c + (255,))
        px.append(row)
    return px


def main():
    os.makedirs(OUT, exist_ok=True)
    # 48x48 panels, 8 px border, 32x32 tiling centre.
    write_png(os.path.join(OUT, "stone_panel.png"), 48, 48,
              panel(48, 8, STONE, STONE_LIGHT, STONE_SHADOW, STONE_DARK, 7, 6))
    write_png(os.path.join(OUT, "parchment_panel.png"), 48, 48,
              panel(48, 8, PARCHMENT, PARCH_LIGHT, PARCH_EDGE, PARCH_EDGE, 11, 5))
    # 24x24 buttons, 4 px border, 16x16 centre.
    write_png(os.path.join(OUT, "button.png"), 24, 24, button(24, 4, STONE, False, 3))
    write_png(os.path.join(OUT, "button_hover.png"), 24, 24, button(24, 4, shade(STONE, 14), False, 3))
    write_png(os.path.join(OUT, "button_pressed.png"), 24, 24, button(24, 4, shade(STONE, -8), True, 3))
    print("wrote", OUT)


if __name__ == "__main__":
    main()
