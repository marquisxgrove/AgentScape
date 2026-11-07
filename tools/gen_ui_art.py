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
    gen_skill_icons()
    print("wrote", OUT)


# --- Kiroween overlays (appended; call gen_kiroween() or just run this file) ------------------
def gen_kiroween():
    import os as _os
    sprites = _os.path.join(ROOT, "assets", "sprites")
    _os.makedirs(sprites, exist_ok=True)

    # Jack-o'-lantern, 16x16, 8-bit-ish palette, transparent background.
    PUMPKIN = hex_rgb("#ff7518"); PUMPKIN_D = hex_rgb("#c8560f"); STEM = hex_rgb("#4a5d32")
    GLOW = hex_rgb("#ffd84a"); OUTLINE = hex_rgb("#1e1912")
    S = 16
    px = [[(0, 0, 0, 0) for _ in range(S)] for _ in range(S)]
    def disc(cx, cy, r, c):
        for y in range(S):
            for x in range(S):
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                    px[y][x] = c + (255,)
    disc(7.5, 9, 6.6, OUTLINE)
    disc(7.5, 9, 5.8, PUMPKIN)
    for x in range(S):  # vertical ribs
        for y in range(S):
            if px[y][x][3] and ((x in (4, 11)) ):
                px[y][x] = PUMPKIN_D + (255,)
    px[2][7] = STEM + (255,); px[3][7] = STEM + (255,); px[3][8] = STEM + (255,)
    # face: two eyes + grin in glow
    for (x, y) in [(5, 7), (6, 7), (9, 7), (10, 7)]:
        px[y][x] = GLOW + (255,)
    for (x, y) in [(5, 10), (6, 11), (7, 11), (8, 11), (9, 11), (10, 10)]:
        px[y][x] = GLOW + (255,)
    write_png(_os.path.join(sprites, "pumpkin.png"), S, S, px)

    # Fog tile: 64x64 soft translucent blobs, tiles seamlessly, grey-white.
    F = 64
    rng = Lcg(99)
    fog = [[(200, 205, 215, 0) for _ in range(F)] for _ in range(F)]
    for _ in range(14):
        cx = rng.next() % F; cy = rng.next() % F; r = 8 + rng.next() % 14
        for y in range(F):
            for x in range(F):
                # wrap-aware distance so the tile is seamless
                dx = min(abs(x - cx), F - abs(x - cx)); dy = min(abs(y - cy), F - abs(y - cy))
                d = (dx * dx + dy * dy) ** 0.5
                if d < r:
                    a = int(40 * (1 - d / r))
                    r0, g0, b0, a0 = fog[y][x]
                    fog[y][x] = (r0, g0, b0, min(90, a0 + a))
    write_png(_os.path.join(sprites, "fog.png"), F, F, fog)

    # Bat: 12x8, two implicit frames side by side (so 24x8), transparent bg, near-black.
    B = hex_rgb("#1a1522")
    bw, bh = 24, 8
    bat = [[(0, 0, 0, 0) for _ in range(bw)] for _ in range(bh)]
    def wing(ox, up):
        body = [(5, 3), (6, 3), (5, 4), (6, 4)]
        if up:
            pts = [(1, 1), (2, 2), (3, 3), (4, 3), (7, 3), (8, 3), (9, 2), (10, 1)]
        else:
            pts = [(1, 5), (2, 4), (3, 3), (4, 3), (7, 3), (8, 3), (9, 4), (10, 5)]
        for (x, y) in body + pts:
            bat[y][ox + x] = B + (255,)
    wing(0, True); wing(12, False)
    write_png(_os.path.join(sprites, "bat.png"), bw, bh, bat)
    print("wrote kiroween sprites in", sprites)


# --- Skill icons (examine card): ten 11x11 pictograms in a 110x11 strip ------------------------------
SKILL_ICON_ART = {
    # '#' = ink outline, 'o' = gold fill, '.' = transparent
    'coding': [
        "...........", "...#...#...", "..#.....#..", ".#...o...#.", "#....o....#", ".#...o...#.",
        "..#.....#..", "...#...#...", "...........", "...........", "...........",
    ],
    'reasoning': [  # light bulb
        "....###....", "...#ooo#...", "..#ooooo#..", "..#ooooo#..", "...#ooo#...", "....#o#....",
        "....###....", "....#.#....", "....###....", "...........", "...........",
    ],
    'writing': [  # pencil
        "........##.", ".......#oo#", "......#oo#.", ".....#oo#..", "....#oo#...", "...#oo#....",
        "..#oo#.....", ".#oo#......", ".##........", "#..........", "...........",
    ],
    'research': [  # magnifying glass
        "...####....", "..#....#...", ".#......#..", ".#......#..", ".#......#..", "..#....#...",
        "...####.#..", ".........#.", "..........#", "...........", "...........",
    ],
    'creativity': [  # four-point star
        ".....#.....", "....#o#....", "....#o#....", "..###o###..", "#ooooooooo#", "..###o###..",
        "....#o#....", "....#o#....", ".....#.....", "...........", "...........",
    ],
    'tool_use': [  # hammer
        "..#####....", "..#ooo#....", "..#ooo#....", "..#####....", "....#o#....", "....#o#....",
        "....#o#....", "....#o#....", "....###....", "...........", "...........",
    ],
    'context': [  # a page of text
        ".#########.", ".#.......#.", ".#.#####.#.", ".#.......#.", ".#.#####.#.", ".#.......#.",
        ".#.#####.#.", ".#.......#.", ".#########.", "...........", "...........",
    ],
    'reliability': [  # shield with a tick
        ".#########.", ".#ooooooo#.", ".#ooooooo#.", ".#ooooooo#.", ".#ooooooo#.", "..#ooooo#..",
        "...#ooo#...", "....#o#....", ".....#.....", "...........", "...........",
    ],
    'speed': [  # lightning bolt
        ".....####..", "....#oo#...", "...#oo#....", "..#oooooo#.", ".....#oo#..", "....#oo#...",
        "...#oo#....", "..##o#.....", "..#........", "...........", "...........",
    ],
    'value': [  # coin
        "...#####...", "..#ooooo#..", ".#oo#o#oo#.", "#ooo#o#ooo#", "#oo#ooo#oo#", "#ooo#o#ooo#",
        ".#oo#o#oo#.", "..#ooooo#..", "...#####...", "...........", "...........",
    ],
}
SKILL_ICON_ORDER = ['coding', 'reasoning', 'writing', 'research', 'creativity', 'tool_use', 'context',
                    'reliability', 'speed', 'value']


def gen_skill_icons():
    ink = hex_rgb("#1e1912") + (255,)
    gold = hex_rgb("#c9962e") + (255,)
    clear = (0, 0, 0, 0)
    size = 11
    # the shield gets a tick drawn in ink over its fill
    art = {k: [list(r) for r in v] for k, v in SKILL_ICON_ART.items()}
    for (x, y) in [(3, 3), (4, 4), (5, 5), (6, 4), (7, 3), (8, 2)]:
        art['reliability'][y][x] = '#'
    rows = [[clear] * (size * len(SKILL_ICON_ORDER)) for _ in range(size)]
    for n, name in enumerate(SKILL_ICON_ORDER):
        grid = art[name]
        assert len(grid) == size and all(len(r) == size for r in grid), name
        for y in range(size):
            for x in range(size):
                ch = grid[y][x]
                if ch == '#':
                    rows[y][n * size + x] = ink
                elif ch == 'o':
                    rows[y][n * size + x] = gold
    write_png(os.path.join(OUT, "skill_icons.png"), size * len(SKILL_ICON_ORDER), size, rows)


if __name__ == "__main__" and os.environ.get("AS_KIROWEEN"):
    gen_kiroween()


if __name__ == "__main__":
    main()
