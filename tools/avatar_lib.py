"""Shared canvas/palette helpers for the AgentScape paper-doll generator (standard library only)."""
import struct, zlib

W, H = 32, 48

# --- Palette -----------------------------------------------------------------------------------
# Index 0 is transparent. 16..55 are the reserved recolourable ramps (placeholders in the PNG; the
# renderer swaps them from the Appearance). Everything else is a fixed colour used by items.
def rgb(h):
    h = h.lstrip('#'); return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))

FIXED = {
    1: '#1e1912',   # outline
    2: '#f0ead6',   # bone / white
    3: '#b8b2a4',   # light grey
    4: '#7d776b',   # mid grey
    5: '#4a453c',   # dark grey
    6: '#9a7648',   # wood light
    7: '#6b5436',   # wood dark
    8: '#d6dde6',   # metal light
    9: '#8793a3',   # metal dark
    10: '#6b2fa0',  # kiroween purple
    11: '#ff7518',  # pumpkin
    12: '#1a1522',  # near black
    13: '#a63a2c',  # red
    14: '#e0b43c',  # gold
    15: '#4a8a3c',  # green
    56: '#2f4a8a',  # deep blue
    57: '#5a86c8',  # light blue
    58: '#7a4a2a',  # leather
    59: '#4e2e1a',  # dark leather
    60: '#e8dcc0',  # cream
    61: '#6e1f1f',  # dark red
    62: '#8e5cc4',  # light purple
    63: '#ffd84a',  # glow yellow
    64: '#c9c2b0',  # cobweb
    65: '#d9a066',  # tan
}
SKIN0, HAIR0, TORSO0, LEGS0, FEET0 = 16, 24, 32, 40, 48   # 8 shades each, darkest..lightest

def skin(s): return SKIN0 + s
def hair(s): return HAIR0 + s
def torso(s): return TORSO0 + s
def legs(s): return LEGS0 + s
def feet(s): return FEET0 + s

def base_palette():
    """256 entries of (r,g,b); index 0 is a placeholder for 'transparent'. All entries are DISTINCT so
    the renderer can recover indices from decoded RGBA pixels."""
    pal = [(0, 0, 0)] * 256
    pal[0] = (255, 0, 255)
    for i, h in FIXED.items():
        pal[i] = rgb(h)
    # Placeholder ramps: five clearly different hues so the raw atlas is readable, 8 shades each.
    ramps = {SKIN0: (224, 172, 105), HAIR0: (110, 70, 40), TORSO0: (70, 110, 200),
             LEGS0: (130, 90, 60), FEET0: (90, 60, 40)}
    for base, (r, g, b) in ramps.items():
        for i in range(8):
            m = 0.45 + 0.65 * i / 7
            pal[base + i] = (min(255, int(r * m)), min(255, int(g * m)), min(255, int(b * m)))
    # Remaining unused indices get unique dummy colours (never drawn) so uniqueness holds.
    used = set(pal[i] for i in range(256) if i == 0 or i in FIXED or 16 <= i < 56)
    n = 0
    for i in range(256):
        if i == 0 or i in FIXED or 16 <= i < 56:
            continue
        while True:
            c = (n % 251, (n // 251) % 251, 3)
            n += 1
            if c not in used:
                used.add(c); pal[i] = c; break
    assert len(set(pal)) == 256, 'palette entries must be unique'
    return pal

# --- Canvas ------------------------------------------------------------------------------------
class Cv:
    def __init__(self):
        self.p = [[0] * W for _ in range(H)]

    def set(self, x, y, c):
        if 0 <= x < W and 0 <= y < H:
            self.p[y][x] = c

    def get(self, x, y):
        return self.p[y][x] if 0 <= x < W and 0 <= y < H else 0

    def box(self, x0, x1, y0, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.set(x, y, c)

    def hline(self, x0, x1, y, c): self.box(x0, x1, y, y, c)
    def vline(self, x, y0, y1, c): self.box(x, x, y0, y1, c)

    def outline(self, col=1):
        src = [row[:] for row in self.p]
        for y in range(H):
            for x in range(W):
                if src[y][x] == 0:
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        nx, ny = x + dx, y + dy
                        if 0 <= nx < W and 0 <= ny < H and src[ny][nx] not in (0, col):
                            self.p[y][x] = col
                            break
        return self

    def mirrored(self):
        c = Cv(); c.p = [row[::-1] for row in self.p]; return c

    def shifted_down(self, n=1):
        c = Cv()
        for y in range(H - n):
            c.p[y + n] = self.p[y][:]
        return c

    def empty(self): return all(v == 0 for row in self.p for v in row)

# --- PNG writer (8-bit indexed with tRNS for index 0) ---------------------------------------------
def write_indexed_png(path, width, height, rows, palette):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
    raw = b''.join(b'\x00' + bytes(r) for r in rows)
    plte = b''.join(bytes(c) for c in palette)
    out = b'\x89PNG\r\n\x1a\n'
    out += chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 3, 0, 0, 0))
    out += chunk(b'PLTE', plte)
    out += chunk(b'tRNS', b'\x00')            # only index 0 is transparent
    out += chunk(b'IDAT', zlib.compress(raw, 9))
    out += chunk(b'IEND', b'')
    open(path, 'wb').write(out)

def write_rgba_png(path, width, height, rows):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
    raw = b''.join(b'\x00' + b''.join(bytes(px) for px in r) for r in rows)
    out = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
    out += chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    open(path, 'wb').write(out)
