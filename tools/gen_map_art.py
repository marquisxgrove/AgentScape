#!/usr/bin/env python3
"""Generates the original Promptmoor town map (assets/sprites/promptmoor.png, 512x334).

Standard-library only and deterministic. Draws a simple pixel-art town: grass, a dirt square and
paths, and four buildings (library, tavern, obelisk, costumer) plus a graveyard patch, positioned to
match the hotspot polygons in src/game/hotspots.cpp. Palette follows art-direction.md.
"""
import os, struct, zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "sprites")
W, H = 380, 300

def rgb(h):
    h = h.lstrip("#"); return tuple(int(h[i:i+2], 16) for i in (0, 2, 4))

GRASS = rgb("#4a5d32"); GRASS2 = rgb("#3f5029"); DIRT = rgb("#8a7650"); DIRT2 = rgb("#7a6744")
STONE = rgb("#5d5447"); STONE_D = rgb("#3e3529"); STONE_L = rgb("#7a6f5e")
ROOF = rgb("#6e3b2e"); ROOF_D = rgb("#512a20"); WOOD = rgb("#6b5436"); PARCH = rgb("#c8b88a")
OBSID = rgb("#2b2440"); GRAVE = rgb("#55504a"); OUTLINE = rgb("#1e1912")

class Lcg:
    def __init__(s, seed): s.s = seed & 0xFFFFFFFF
    def n(s): s.s = (1664525*s.s + 1013904223) & 0xFFFFFFFF; return s.s >> 16

def new_img(c): return [[c + (255,) for _ in range(W)] for _ in range(H)]

def rect(px, x, y, w, h, c):
    for yy in range(max(0,y), min(H, y+h)):
        row = px[yy]
        for xx in range(max(0,x), min(W, x+w)):
            row[xx] = c + (255,)

def border(px, x, y, w, h, c):
    rect(px, x, y, w, 1, c); rect(px, x, y+h-1, w, 1, c)
    rect(px, x, y, 1, h, c); rect(px, x+w-1, y, 1, h, c)

def speckle(px, rng, amt):
    for yy in range(H):
        row = px[yy]
        for xx in range(W):
            if rng.n() % 11 == 0:
                r,g,b,a = row[xx]
                d = amt if rng.n() % 2 else -amt
                row[xx] = (max(0,min(255,r+d)), max(0,min(255,g+d)), max(0,min(255,b+d)), 255)

def building(px, x, y, w, h, wall, roofc, roofd):
    rect(px, x, y+h//3, w, h-h//3, wall); border(px, x, y+h//3, w, h-h//3, OUTLINE)
    # gable roof
    for i in range(h//3):
        roww = int(w * (i+1)/(h//3))
        rx = x + (w - roww)//2
        rect(px, rx, y+i, roww, 1, roofc if i % 3 else roofd)
    # door + window
    rect(px, x+w//2-6, y+h-18, 12, 18, WOOD); border(px, x+w//2-6, y+h-18, 12, 18, OUTLINE)
    rect(px, x+8, y+h//3+8, 10, 10, PARCH); rect(px, x+w-18, y+h//3+8, 10, 10, PARCH)

def main():
    os.makedirs(OUT, exist_ok=True)
    rng = Lcg(2026)
    px = new_img(GRASS)
    # grass variation
    for yy in range(H):
        for xx in range(W):
            if (xx//8 + yy//8) % 2 == 0 and rng.n() % 3 == 0:
                px[yy][xx] = GRASS2 + (255,)
    # central dirt square + cross paths
    rect(px, 110, 140, 170, 120, DIRT)
    rect(px, 176, 18, 32, 270, DIRT); rect(px, 30, 150, 320, 30, DIRT)
    for _ in range(4000):
        x = rng.n() % W; y = rng.n() % H
        if px[y][x][:3] == DIRT: px[y][x] = DIRT2 + (255,)
    # buildings (footprints match hotspots.cpp)
    building(px, 40, 50, 100, 90, STONE, ROOF, ROOF_D)       # Grand Codex
    building(px, 250, 55, 100, 88, WOOD, ROOF, ROOF_D)       # Tavern
    building(px, 55, 180, 100, 85, STONE_L, ROOF, ROOF_D)    # Costumer
    # Hiscores obelisk
    rect(px, 182, 34, 26, 92, OBSID); border(px, 182, 34, 26, 92, OUTLINE)
    rect(px, 178, 122, 34, 7, STONE_D)
    # Graveyard patch — drawn always; the hotspot itself is season-gated
    rect(px, 240, 178, 110, 88, GRASS2)
    for gx in (252, 277, 302, 327):
        rect(px, gx, 214, 13, 28, GRAVE); border(px, gx, 214, 13, 28, OUTLINE)
        rect(px, gx+3, 209, 7, 7, GRAVE)
    speckle(px, rng, 6)
    # subtle frame
    border(px, 0, 0, W, H, STONE_D)
    write_png(os.path.join(OUT, "promptmoor.png"), px)
    print("wrote", os.path.join(OUT, "promptmoor.png"))

def write_png(path, px):
    raw = b"".join(b"\x00" + b"".join(struct.pack("BBBB", *p) for p in row) for row in px)
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag+data) & 0xFFFFFFFF)
    out = b"\x89PNG\r\n\x1a\n"
    out += chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0))
    out += chunk(b"IDAT", zlib.compress(raw, 9))
    out += chunk(b"IEND", b"")
    open(path, "wb").write(out)

if __name__ == "__main__":
    main()
