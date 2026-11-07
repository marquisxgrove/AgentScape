#!/usr/bin/env python3
"""Contact-sheet preview of the generated avatar layers (QA aid; not shipped). Mirrors the domain draw
order and palette swap closely enough to judge the art without building the app."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_avatar as G
from avatar_lib import W, H, write_rgba_png, base_palette

pal, pixels, sw, sh, index = G.build()
FAC = {'S': 0, 'E': 1, 'N': 2, 'W': 3}

def ramp(rgb):
    return [tuple(min(255, int(c * (0.45 + 0.65 * i / 7))) for c in rgb) for i in range(8)]

def palette_for(colors):
    p = [tuple(c) for c in pal]
    for base, rgb in zip((16, 24, 32, 40, 48), colors):
        for i, c in enumerate(ramp(rgb)): p[base + i] = c
    return p

SKINS = [(236, 190, 150), (198, 140, 96), (141, 94, 62), (255, 220, 190)]
def look(skin=0, hair=(110, 70, 40), torso=(70, 110, 200), legs=(130, 90, 60), feet=(90, 60, 40)):
    return palette_for([SKINS[skin], hair, torso, legs, feet])

ORDER_S = ['cape', 'body', 'legs', 'feet', 'torso', 'arms', 'hands', 'legsI', 'feetI', 'bodyI', 'handsI', 'neckI', 'hair', 'facial', 'headI', 'shieldI', 'weaponI']
ORDER_N = ['weaponI', 'shieldI', 'body', 'legs', 'feet', 'torso', 'arms', 'hands', 'legsI', 'feetI', 'bodyI', 'handsI', 'neckI', 'hair', 'facial', 'headI', 'cape']
SLOT = {'headI': 'head', 'cape': 'cape', 'neckI': 'neck', 'weaponI': 'weapon', 'bodyI': 'body', 'shieldI': 'shield',
        'legsI': 'legs', 'handsI': 'hands', 'feetI': 'feet'}

def cell(name, facing, frame):
    r = index[name]; c = FAC[facing] * 2 + frame
    return [pixels[r * H + y][c * W:(c + 1) * W] for y in range(H)]

def compose(p, hair=1, facial=0, torso=0, arms=1, legs=0, items=None, facing='S', frame=0):
    items = items or {}
    img = [[(0, 0, 0, 0)] * W for _ in range(H)]
    order = ORDER_N if facing == 'N' else ORDER_S
    names = {'body': 'body/A', 'legs': f'legs/{legs}', 'feet': 'feet/0', 'torso': f'torso/{torso}', 'arms': f'arms/{arms}',
             'hands': 'hands/0', 'hair': f'hair/{hair}', 'facial': f'facial/{facial}' if facial else None}
    for key in order:
        if key in SLOT:
            it = items.get(SLOT[key])
            nm = f'item/{it}' if it else None
        else:
            nm = names[key]
        if nm is None or nm not in index: continue
        for y, row in enumerate(cell(nm, facing, frame)):
            for x, i in enumerate(row):
                if i: img[y][x] = p[i] + (255,)
    return img

def sheet(cols, scale=3, bg=(70, 62, 52)):
    rows = (len(cols) + 7) // 8
    out_w, out_h = 8 * (W * scale + 6), rows * (H * scale + 6)
    out = [[bg + (255,)] * out_w for _ in range(out_h)]
    for n, im in enumerate(cols):
        ox, oy = (n % 8) * (W * scale + 6) + 3, (n // 8) * (H * scale + 6) + 3
        for y in range(H):
            for x in range(W):
                px = im[y][x]
                if px[3]:
                    for dy in range(scale):
                        for dx in range(scale): out[oy + y * scale + dy][ox + x * scale + dx] = px
    return out_w, out_h, out

if __name__ == '__main__':
    which = sys.argv[1] if len(sys.argv) > 1 else 'all'
    p = look()
    default = {'weapon': None}
    cells = []
    if which in ('all', 'facings'):
        for f in 'SENW': cells.append(compose(p, items={'weapon': 'wooden_stick'}, facing=f))
        for f in 'SENW': cells.append(compose(p, items={'weapon': 'wooden_stick'}, facing=f, frame=1))
    if which in ('all', 'hair'):
        for s in range(G.N_HAIR): cells.append(compose(p, hair=s))
        for s in range(G.N_HAIR): cells.append(compose(p, hair=s, facing='N'))
        for s in range(G.N_HAIR): cells.append(compose(p, hair=s, facing='E'))
    if which in ('all', 'facial'):
        for s in range(1, G.N_FACIAL): cells.append(compose(p, facial=s))
        for s in range(1, G.N_FACIAL): cells.append(compose(p, facial=s, facing='E'))
    if which in ('all', 'clothes'):
        for s in range(G.N_TORSO): cells.append(compose(p, torso=s))
        for s in range(G.N_ARMS): cells.append(compose(p, arms=s))
        for s in range(G.N_LEGS): cells.append(compose(p, legs=s))
        for s in range(G.N_TORSO): cells.append(compose(p, torso=s, facing='E'))
        for s in range(G.N_LEGS): cells.append(compose(p, legs=s, facing='E'))
    if which in ('all', 'items'):
        for it in G.ITEM_FUNCS:
            slot = {'apprentice_hood': 'head', 'wizard_hat': 'head', 'witch_hat': 'head', 'jack_mask': 'head', 'skull_mask': 'head',
                    'travellers_cape': 'cape', 'keepers_cape': 'cape', 'vampire_cape': 'cape', 'token_amulet': 'neck',
                    'wooden_stick': 'weapon', 'quill_sword': 'weapon', 'prompt_staff': 'weapon', 'reapers_scythe': 'weapon',
                    'scholar_robe': 'body', 'ghostly_sheet': 'body', 'codex_tome': 'shield', 'candy_pail': 'shield',
                    'scholar_skirt': 'legs', 'leather_gloves': 'hands', 'cobweb_boots': 'feet'}[it]
            cells.append(compose(p, items={slot: it}))
        for it in ('apprentice_hood', 'wizard_hat', 'witch_hat', 'jack_mask', 'skull_mask'):
            cells.append(compose(p, items={'head': it}, facing='E'))
    w, h, img = sheet(cells)
    outp = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'build', f'preview_{which}.png')
    os.makedirs(os.path.dirname(outp), exist_ok=True)
    write_rgba_png(outp, w, h, img)
    print('wrote', outp, w, h)
