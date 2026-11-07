#!/usr/bin/env python3
"""Generates the original art for The Wilds (coding-sessions spec) into assets/sprites/.

Standard-library only and deterministic, like gen_map_art.py:
  wilds.png  300x200  the clearing south of Promptmoor, seen from the side (feet stand on y = 160)
  bugs.png    64x160  five bug monsters, one per row, two 32x32 frames each (order = game::bestiary())
  wisp.png    32x16   the familiar's wisp, two 16x16 frames
  tree.png    40x60   woodcutting spot
  rock.png    36x28   mining spot (a boulder with copper flecks)
  pond.png   128x24   fishing spot, two 64x24 frames (ripples)
Palette follows art-direction.md (earthy, desaturated, dark outlines).
"""
import math, os, struct, zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "sprites")

def rgb(h):
    h = h.lstrip("#"); return tuple(int(h[i:i+2], 16) for i in (0, 2, 4))

OUTLINE = rgb("#1e1912")
GRASS = rgb("#4a5d32"); GRASS2 = rgb("#3f5029"); DIRT = rgb("#8a7650"); DIRT2 = rgb("#7a6744")
FOREST = rgb("#24331c"); FOREST2 = rgb("#2e4224"); TRUNK = rgb("#4a3a26"); WOOD = rgb("#6b5436")
LEAF = rgb("#3f6b2c"); LEAF2 = rgb("#56853a"); STONE = rgb("#6f675b"); STONE_D = rgb("#4b453c"); STONE_L = rgb("#8c8474")
COPPER = rgb("#c47a3a"); WATER = rgb("#2f5f8a"); WATER_L = rgb("#5b8fbf"); FOAM = rgb("#bcd6e8")
CLEAR = (0, 0, 0, 0)

class Lcg:
    def __init__(s, seed): s.s = seed & 0xFFFFFFFF
    def n(s): s.s = (1664525*s.s + 1013904223) & 0xFFFFFFFF; return s.s >> 16

class Img:
    def __init__(s, w, h, fill=CLEAR):
        s.w, s.h = w, h
        s.px = [[fill if len(fill) == 4 else fill + (255,) for _ in range(w)] for _ in range(h)]
    def set(s, x, y, c, a=255):
        if 0 <= x < s.w and 0 <= y < s.h: s.px[y][x] = (c[0], c[1], c[2], a)
    def get(s, x, y): return s.px[y][x]
    def rect(s, x, y, w, h, c, a=255):
        for yy in range(y, y+h):
            for xx in range(x, x+w): s.set(xx, yy, c, a)
    def ellipse(s, cx, cy, rx, ry, c, a=255):
        for yy in range(int(cy-ry)-1, int(cy+ry)+2):
            for xx in range(int(cx-rx)-1, int(cx+rx)+2):
                if ((xx+0.5-cx)/rx)**2 + ((yy+0.5-cy)/ry)**2 <= 1.0: s.set(xx, yy, c, a)
    def outline(s, c=OUTLINE, x0=0, y0=0, w=None, h=None):
        """Draws a 1px outline around every opaque shape inside the box (4-neighbour)."""
        w = s.w if w is None else w; h = s.h if h is None else h
        add = []
        for yy in range(y0, y0+h):
            for xx in range(x0, x0+w):
                if s.px[yy][xx][3] != 0: continue
                for dx, dy in ((1,0),(-1,0),(0,1),(0,-1)):
                    nx, ny = xx+dx, yy+dy
                    if x0 <= nx < x0+w and y0 <= ny < y0+h and s.px[ny][nx][3] != 0 and s.px[ny][nx][:3] != c:
                        add.append((xx, yy)); break
        for xx, yy in add: s.set(xx, yy, c)
    def blit(s, other, ox, oy):
        for yy in range(other.h):
            for xx in range(other.w):
                p = other.px[yy][xx]
                if p[3]: s.set(ox+xx, oy+yy, p[:3], p[3])
    def save(s, name):
        raw = b"".join(b"\x00" + b"".join(struct.pack("BBBB", *p) for p in row) for row in s.px)
        def chunk(tag, data):
            return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag+data) & 0xFFFFFFFF)
        out = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", s.w, s.h, 8, 6, 0, 0, 0))
        out += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
        path = os.path.join(OUT, name)
        open(path, "wb").write(out)
        print("wrote", path)

def eyes(img, x, y, gap, c=(250, 240, 200)):
    for ex in (x, x+gap):
        img.rect(ex, y, 2, 2, c); img.set(ex+1, y+1, OUTLINE)

# --- The clearing --------------------------------------------------------------------------------------------------
def wilds():
    rng = Lcg(77)
    img = Img(300, 200, GRASS)
    # forest backdrop: a dark band of overlapping crowns with trunks below
    img.rect(0, 0, 300, 74, FOREST)
    for i in range(16):
        cx = (i * 23 + rng.n() % 9) % 312 - 6
        img.rect(cx - 2, 46, 5, 30, TRUNK)
        img.ellipse(cx, 30 + rng.n() % 12, 15 + rng.n() % 6, 18, FOREST2 if i % 2 else LEAF)
    # grass with a checker of darker tufts
    for yy in range(74, 200):
        for xx in range(300):
            if (xx//6 + yy//6) % 2 == 0 and rng.n() % 3 == 0: img.set(xx, yy, GRASS2)
    # the trodden ground the Adventurer stands on (feet at y = 160)
    img.rect(0, 150, 300, 22, DIRT)
    for _ in range(900):
        x = rng.n() % 300; y = 150 + rng.n() % 22
        img.set(x, y, DIRT2)
    # a few bushes and pebbles for depth
    for bx, by in ((20, 120), (262, 112), (240, 186), (40, 188)):
        img.ellipse(bx, by, 12, 7, LEAF); img.ellipse(bx + 4, by - 3, 6, 4, LEAF2)
    for _ in range(30):
        x = rng.n() % 300; y = 176 + rng.n() % 22
        img.set(x, y, STONE_L)
    # speckle, then the scene's frame line
    for yy in range(200):
        for xx in range(300):
            if rng.n() % 13 == 0:
                r, g, b, a = img.px[yy][xx]; d = 5 if rng.n() % 2 else -5
                img.px[yy][xx] = (max(0, min(255, r+d)), max(0, min(255, g+d)), max(0, min(255, b+d)), a)
    img.save("wilds.png")

# --- Bug monsters: 2 frames x 5 rows of 32x32 ----------------------------------------------------------------------
def imp(f):
    img = Img(32, 32); body = rgb("#6a3d9a"); dark = rgb("#4a2a6e")
    up = f == 0
    for side in (-1, 1):   # little wings, up then down
        wx = 16 + side * 9
        img.ellipse(wx, 15 if up else 19, 5, 3 if up else 4, dark)
    img.ellipse(16, 21, 7, 7, body)
    img.rect(11, 12, 2, 3, dark); img.rect(19, 12, 2, 3, dark)   # horns
    img.rect(22, 25, 6, 1, dark); img.rect(27, 23, 1, 5, dark)    # tail ending in a pointer arrow
    img.set(26, 24, dark); img.set(26, 26, dark)
    eyes(img, 13, 19, 4, rgb("#f2d64b"))
    img.outline(); return img

def goblin(f):
    img = Img(32, 32); skin = rgb("#5f8f3a"); cloth = rgb("#6b4a2e")
    o = 0 if f == 0 else 1   # off by one, of course
    img.ellipse(15 + o, 13, 6, 6, skin)
    img.rect(6 + o, 11, 4, 2, skin); img.rect(21 + o, 11, 4, 2, skin)   # ears
    img.rect(11 + o, 19, 9, 9, cloth); img.rect(12 + o, 28, 3, 3, skin); img.rect(17 + o, 28, 3, 3, skin)
    img.rect(21 + o, 15, 2, 12, WOOD); img.ellipse(22 + o, 15, 3, 3, WOOD)   # club
    eyes(img, 12 + o, 12, 5, rgb("#e8d24a"))
    img.outline(); return img

def slime(f):
    img = Img(32, 32); body = rgb("#3a78b5"); light = rgb("#78aee0")
    rx, ry = (10, 8) if f == 0 else (12, 7)
    img.ellipse(16, 30 - ry, rx, ry, body)
    img.rect(16 - rx, 29, rx * 2, 2, body)
    img.ellipse(12, 26 - ry, 3, 2, light)
    for dx in (-6, 2, 7):   # leaking drips
        img.rect(16 + dx, 30, 2, 2 if f == 0 else 1, body)
    eyes(img, 13, 31 - ry - 2, 5, (240, 240, 250))
    img.outline(); return img

def wraith(f):
    img = Img(32, 32); body = rgb("#b9bec4")
    def ghost(ox, alpha):
        g = Img(32, 32)
        g.ellipse(14 + ox, 12, 7, 8, body, alpha)
        g.rect(7 + ox, 12, 15, 12, body, alpha)
        for i in range(4): g.rect(7 + ox + i * 4, 24, 2, 3, body, alpha)   # ragged hem
        g.rect(11 + ox, 10, 2, 3, OUTLINE, alpha); g.rect(16 + ox, 10, 2, 3, OUTLINE, alpha)
        return g
    if f == 1: img.blit(ghost(5, 110), 0, 0)   # sometimes there are two of it
    img.blit(ghost(0, 220), 0, 0)
    img.outline(); return img

def heisenbug(f):
    img = Img(32, 32); shell = rgb("#7a2a20"); spot = rgb("#d8b04a"); a = 255 if f == 0 else 120
    for i, y in enumerate((20, 24, 28)):   # legs
        img.rect(6, y, 4, 1, OUTLINE, a); img.rect(22, y, 4, 1, OUTLINE, a)
    img.ellipse(16, 23, 9, 8, shell, a)
    img.rect(16, 15, 1, 16, OUTLINE, a)
    img.ellipse(16, 14, 4, 3, rgb("#3a2a22"), a)
    img.rect(13, 9, 1, 3, OUTLINE, a); img.rect(19, 9, 1, 3, OUTLINE, a)   # antennae
    for sx, sy in ((12, 21), (20, 22), (13, 26), (19, 27)): img.rect(sx, sy, 2, 2, spot, a)
    if f == 0: img.outline()
    return img

def bugs():
    sheet = Img(64, 160)
    for row, make in enumerate((imp, goblin, slime, wraith, heisenbug)):
        for f in (0, 1): sheet.blit(make(f), f * 32, row * 32)
    sheet.save("bugs.png")

# --- The familiar's wisp, 2 frames of 16x16 ------------------------------------------------------------------------
def wisp():
    sheet = Img(32, 16)
    for f in (0, 1):
        g = Img(16, 16)
        g.ellipse(8, 8, 7 if f else 6, 7 if f else 6, rgb("#5fb6e0"), 70)
        g.ellipse(8, 8, 4, 4, rgb("#8fd3f0"), 200)
        g.ellipse(8, 8, 2, 2, (250, 252, 255))
        sheet.blit(g, f * 16, 0)
    sheet.save("wisp.png")

# --- Skilling spots ------------------------------------------------------------------------------------------------
def tree():
    img = Img(40, 60)
    img.rect(16, 34, 8, 26, TRUNK); img.rect(14, 57, 12, 3, TRUNK)
    for cx, cy, r in ((20, 18, 14), (11, 26, 9), (29, 26, 9), (20, 8, 8)):
        img.ellipse(cx, cy, r, r * 0.9, LEAF)
    for cx, cy in ((15, 14), (25, 20), (18, 26)): img.ellipse(cx, cy, 3, 2, LEAF2)
    img.outline(); img.save("tree.png")

def rock():
    img = Img(36, 28)
    img.ellipse(18, 17, 16, 11, STONE); img.rect(3, 22, 30, 5, STONE)
    img.ellipse(14, 12, 6, 3, STONE_L); img.rect(4, 25, 28, 2, STONE_D)
    for x, y in ((10, 18), (20, 15), (25, 21), (15, 22)): img.rect(x, y, 2, 2, COPPER)
    img.outline(); img.save("rock.png")

def pond():
    sheet = Img(128, 24)
    for f in (0, 1):
        g = Img(64, 24)
        g.ellipse(32, 12, 30, 10, WATER); g.ellipse(32, 11, 24, 6, WATER_L)
        r = 6 if f == 0 else 10
        for t in range(0, 360, 12):   # a ripple ring where the line is cast
            x = 34 + r * math.cos(math.radians(t)); y = 11 + r * 0.4 * math.sin(math.radians(t))
            g.set(int(x), int(y), FOAM)
        g.outline(); sheet.blit(g, f * 64, 0)
    sheet.save("pond.png")

def main():
    os.makedirs(OUT, exist_ok=True)
    wilds(); bugs(); wisp(); tree(); rock(); pond()

if __name__ == "__main__":
    main()
