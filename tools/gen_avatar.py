#!/usr/bin/env python3
"""Generates AgentScape's original paper-doll atlas: assets/sprites/avatar.png + avatar.json.

Standard library only and fully deterministic. Every layer is drawn procedurally at 32x48 for four
facings (S, E, N, W) x two idle frames, as 8-bit indexed pixels using the reserved placeholder ramps
(indices 16-55) that the renderer swaps for the Appearance colours.

    python3 tools/gen_avatar.py             # writes assets/sprites/avatar.png and avatar.json
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from avatar_lib import (Cv, W, H, skin, hair, torso, legs, feet, base_palette, write_indexed_png)  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'assets', 'sprites')

# Counts must match domain::APPEARANCE_LIMITS (checked by a unit test against avatar.json).
N_HAIR, N_FACIAL, N_TORSO, N_ARMS, N_LEGS = 10, 8, 6, 4, 6

GEO = {
    'S': dict(head=(11, 20), torso=(10, 21), arms=[(7, 9), (22, 24)], legs=[(11, 15), (16, 20)],
              feet=[(10, 15), (16, 21)]),
    'E': dict(head=(12, 19), torso=(12, 19), arms=[(14, 17)], legs=[(13, 18)], feet=[(13, 20)]),
}
GEO['N'] = GEO['S']

OL = 1  # outline index


def geo(f):
    return GEO['E' if f == 'E' else 'S']


# ------------------------------------------------------------------------------------ base body
def head_shape(cv, f, col, shade):
    x0, x1 = geo(f)['head']
    cv.box(x0 + 2, x1 - 2, 5, 5, col)
    cv.box(x0 + 1, x1 - 1, 6, 6, col)
    cv.box(x0, x1, 7, 13, col)
    cv.box(x0 + 1, x1 - 1, 14, 14, col)
    cv.box(x0 + 2, x1 - 2, 15, 15, shade)
    cv.box(x0, x0 + (1 if f == 'E' else 0), 8, 13, shade)


def body_layer(f):
    cv = Cv()
    g = geo(f)
    head_shape(cv, f, skin(4), skin(3))
    nx0 = 14 if f != 'E' else 15
    cv.box(nx0, nx0 + 3, 16, 16, skin(3))                      # neck
    for (a0, a1) in g['arms']:
        cv.box(a0, a1, 17, 29, skin(4))
        cv.vline(a0, 17, 29, skin(3))
    for (l0, l1) in g['legs']:
        cv.box(l0, l1, 30, 43, skin(4))
        cv.vline(l0, 30, 43, skin(3))
    x0, x1 = g['head']
    if f == 'S':
        cv.set(x0 + 2, 10, 12); cv.set(x1 - 2, 10, 12)         # eyes
        cv.set(x0 + 2, 9, skin(2)); cv.set(x1 - 2, 9, skin(2))  # brows hint
    elif f == 'E':
        cv.set(x1 - 2, 10, 12)
        cv.set(x1 + 1, 11, skin(4)); cv.set(x1 + 1, 12, skin(3))  # nose
    return cv.outline(OL)


def hands_layer(f):
    cv = Cv()
    for (a0, a1) in geo(f)['arms']:
        cv.box(max(a0, a1 - 2), a1, 27, 29, skin(5))
    return cv.outline(OL)


# ------------------------------------------------------------------------------------ hair
def hair_layer(f, style):
    cv = Cv()
    x0, x1 = geo(f)['head']
    c, d, l = hair(4), hair(2), hair(6)
    mid0, mid1 = (14, 17) if f != 'E' else (x0 + 2, x0 + 5)

    if f == 'S':
        if style == 0:                                           # buzz cut
            cv.box(x0 + 2, x1 - 2, 4, 5, d); cv.box(x0 + 1, x1 - 1, 6, 6, d)
        elif style == 9:                                         # curly puff
            cv.box(x0 - 1, x1 + 1, 3, 8, c)                       # puff on top; sides frame the face
            cv.box(x0 - 2, x0, 5, 11, c); cv.box(x1, x1 + 2, 5, 11, c)
            for x in range(x0 - 1, x1 + 2, 2):
                cv.set(x, 3, l)
        elif style == 5:                                         # mohawk
            cv.box(mid0, mid1, 0, 8, c); cv.box(mid0 + 1, mid1 - 1, 0, 3, l)
        elif style == 3:                                         # long
            cv.box(x0, x1, 4, 7, c); cv.box(x0 + 1, x1 - 1, 3, 3, c)
            cv.box(x0 - 1, x0, 6, 20, c); cv.box(x1, x1 + 1, 6, 20, c)
            cv.box(x0 + 1, x1 - 1, 4, 4, l)
        elif style == 8:                                         # bob
            cv.box(x0, x1, 4, 8, c); cv.box(x0 + 1, x1 - 1, 3, 3, c)
            cv.box(x0 - 1, x0, 7, 13, c); cv.box(x1, x1 + 1, 7, 13, c)
        else:                                                    # crop family: 1, 2, 4, 6, 7
            cv.box(x0 + 1, x1 - 1, 4, 4, c); cv.box(x0, x1, 5, 7, c)
            cv.box(x0, x0, 8, 9, c); cv.box(x1, x1, 8, 9, c)
            cv.box(x0 + 1, x1 - 1, 4, 4, l)
            if style == 2: cv.box(x0, x0 + 5, 8, 8, c)           # side-swept fringe
            if style == 4:                                       # spikes
                for sx in (x0 + 1, x0 + 4, x1 - 4, x1 - 1):
                    cv.box(sx, sx + 1, 1, 3, c); cv.set(sx, 0, c)
            if style == 7: cv.box(mid0, mid1, 1, 3, c); cv.box(mid0 + 1, mid1 - 1, 0, 0, c)  # bun
            if style == 6: cv.box(x1, x1 + 1, 9, 13, c)           # ponytail tuft at the side
    elif f == 'N':
        end = {0: 10, 1: 12, 2: 12, 3: 20, 4: 12, 5: 0, 6: 12, 7: 12, 8: 14, 9: 11}[style]
        if style == 5:
            cv.box(mid0, mid1, 0, 12, c); cv.box(mid0 + 1, mid1 - 1, 0, 5, l)
        elif style == 9:
            cv.box(x0 - 2, x1 + 2, 3, 12, c)
            for x in range(x0 - 1, x1 + 2, 2): cv.set(x, 3, l)
        else:
            cv.box(x0 + 1, x1 - 1, 4, 4, c); cv.box(x0, x1, 5, end, c)
            if style == 0: cv.box(x0, x1, 5, end, d)
            if style == 3: cv.box(x0 - 1, x1 + 1, 8, 20, c)
            if style == 8: cv.box(x0 - 1, x1 + 1, 8, 14, c)
            if style == 4:
                for sx in (x0 + 1, x0 + 4, x1 - 4, x1 - 1):
                    cv.box(sx, sx + 1, 1, 3, c); cv.set(sx, 0, c)
            if style == 7: cv.box(mid0, mid1, 1, 3, c)
            if style == 6: cv.box(mid0, mid1, 12, 21, c); cv.box(mid0 + 1, mid1 - 1, 13, 20, l)
            if style in (1, 2, 4, 6, 7): cv.box(x0 + 1, x1 - 1, 4, 4, l)
    else:  # E (facing right): back of the head is on the left
        back = {0: 8, 1: 10, 2: 10, 3: 20, 4: 10, 5: 8, 6: 10, 7: 10, 8: 14, 9: 12}[style]
        if style == 0:
            cv.box(x0 + 1, x1 - 2, 4, 5, d); cv.box(x0, x0 + 2, 6, 8, d)
        elif style == 5:
            cv.box(x0 + 2, x0 + 6, 0, 7, c); cv.box(x0 + 3, x0 + 5, 0, 3, l)
        elif style == 9:
            cv.box(x0 - 2, x1 + 1, 3, 11, c)
            for x in range(x0 - 1, x1 + 1, 2): cv.set(x, 3, l)
        else:
            cv.box(x0 + 1, x1 - 1, 4, 4, c); cv.box(x0, x1, 5, 7, c)
            cv.box(x0, x0 + 3, 8, back, c)
            if style == 3: cv.box(x0 - 1, x0 + 3, 8, 20, c)
            if style == 8: cv.box(x0 - 1, x0 + 3, 8, 14, c)
            if style == 2: cv.box(x1 - 4, x1, 8, 8, c)
            if style == 4:
                for sx in (x0 + 1, x0 + 4, x1 - 3):
                    cv.box(sx, sx + 1, 1, 3, c); cv.set(sx, 0, c)
            if style == 7: cv.box(x0 + 2, x0 + 5, 1, 3, c)
            if style == 6: cv.box(x0 - 2, x0 + 1, 10, 18, c); cv.box(x0 - 1, x0, 11, 17, l)
            cv.box(x0 + 1, x1 - 1, 4, 4, l)
    return cv.outline(OL)


def facial_layer(f, style):
    """style 1..7 (0 = none, never rendered). Drawn from the S pattern; E uses its front half."""
    c, d = hair(4), hair(2)
    s = Cv()
    x0, x1 = 11, 20
    if style == 1:                                               # stubble
        for y in (12, 13, 14):
            for x in range(x0 + 2 + (y % 2), x1 - 1, 2): s.set(x, y, d)
    elif style == 2:                                             # moustache
        s.box(13, 18, 12, 12, c)
    elif style == 3:                                             # goatee
        s.box(14, 17, 12, 12, c); s.box(14, 17, 13, 15, c)
    elif style == 4:                                             # full beard
        s.box(x0 + 1, x1 - 1, 12, 15, c); s.box(14, 17, 12, 12, d)
    elif style == 5:                                             # long beard
        s.box(x0 + 1, x1 - 1, 12, 15, c); s.box(13, 18, 16, 20, c); s.box(14, 17, 21, 21, c)
    elif style == 6:                                             # mutton chops
        s.box(x0, x0 + 1, 9, 14, c); s.box(x1 - 1, x1, 9, 14, c); s.box(14, 17, 12, 12, c)
    elif style == 7:                                             # handlebar moustache
        s.box(13, 18, 12, 12, c); s.set(12, 11, c); s.set(19, 11, c); s.set(11, 12, c); s.set(20, 12, c)
    if f == 'N':
        return Cv()
    if f == 'S':
        return s.outline(OL) if style != 1 else s
    cv = Cv()                                                    # E
    for y in range(H):
        for x in range(16, 21):
            if s.get(x, y): cv.set(x - 1, y, s.get(x, y))
    if style in (5,):
        cv.box(15, 18, 16, 21, c)
    return cv.outline(OL) if style != 1 else cv


# ------------------------------------------------------------------------------------ clothing
def torso_layer(f, style):
    cv = Cv()
    t0, t1 = geo(f)['torso']
    c, sh, hi, dk = torso(4), torso(3), torso(6), torso(2)
    cv.box(t0, t1, 16, 29, c)
    cv.vline(t0, 16, 29, sh)
    cv.hline(t0, t1, 29, dk)
    cx0, cx1 = (15, 16) if f != 'E' else (t1 - 1, t1)
    if style == 1 and f != 'N':                                  # v-neck
        for i in range(4):
            cv.box(cx0 - i + (1 if f == 'E' else 0), cx1 + i - (1 if f == 'E' else 0), 16 + i, 16 + i, skin(4))
    elif style == 2:                                             # stripes
        for y in range(16, 29, 3): cv.hline(t0, t1, y, hi)
    elif style == 3 and f != 'N':                                # open vest
        cv.box(cx0, cx1, 17, 29, skin(3)); cv.vline(cx0 - 1, 17, 29, dk); cv.vline(cx1 + 1, 17, 29, dk)
    elif style == 4:                                             # belted
        cv.hline(t0, t1, 26, dk); cv.hline(t0, t1, 27, dk)
        if f == 'S': cv.box(cx0, cx1, 26, 27, 14)
    elif style == 5 and f != 'N':                                # buttons
        cv.vline(cx0, 17, 28, dk)
        for y in (19, 22, 25): cv.set(cx1, y, hi)
    return cv.outline(OL)


def arms_layer(f, style):
    cv = Cv()
    c, sh, hi = torso(4), torso(3), torso(6)
    for (a0, a1) in geo(f)['arms']:
        if style == 0:
            cv.box(a0, a1, 17, 20, c); cv.hline(a0, a1, 20, sh)
        elif style == 1:
            cv.box(a0, a1, 17, 26, c); cv.vline(a0, 17, 26, sh); cv.hline(a0, a1, 26, hi)
        elif style == 2:
            cv.box(a0, a1, 17, 18, c)
        else:
            cv.box(a0 - 1, a1 + 1, 17, 20, c); cv.hline(a0 - 1, a1 + 1, 20, sh)
            cv.box(a0, a1, 21, 24, c); cv.hline(a0, a1, 25, hi)
    return cv.outline(OL)


def legs_layer(f, style):
    cv = Cv()
    c, sh, hi, dk = legs(4), legs(3), legs(6), legs(2)
    ranges = geo(f)['legs']
    if style == 5:                                               # skirt-like flare
        x0 = ranges[0][0]; x1 = ranges[-1][1]
        cv.box(x0 - 1, x1 + 1, 30, 32, c)
        cv.box(x0 - 2, x1 + 2, 33, 36, c)
        cv.box(x0 - 3, x1 + 3, 37, 38, c)
        cv.hline(x0 - 3, x1 + 3, 38, dk)
        return cv.outline(OL)
    bottom = {0: 41, 1: 35, 2: 38, 3: 41, 4: 41}[style]
    for (l0, l1) in ranges:
        cv.box(l0, l1, 30, bottom, c)
        cv.vline(l0, 30, bottom, sh)
        if style == 3: cv.vline(l1 - 1, 30, bottom, hi)
        if style == 4:
            cv.box(l0 - 1, l1 + 1, 34, bottom, c)
        cv.hline(l0 - (1 if style == 4 else 0), l1 + (1 if style == 4 else 0), bottom, dk)
    cv.hline(ranges[0][0], ranges[-1][1], 30, dk)                # waistband
    return cv.outline(OL)


def feet_layer(f):
    cv = Cv()
    c, sh, dk = feet(4), feet(3), feet(1)
    for (f0, f1) in geo(f)['feet']:
        cv.box(f0, f1, 42, 45, c)
        cv.hline(f0, f1, 45, dk)
        cv.vline(f0, 42, 44, sh)
    return cv.outline(OL)


# ------------------------------------------------------------------------------------ items
def item_apprentice_hood(f):
    cv = Cv(); x0, x1 = geo(f)['head']
    b, l = 56, 57
    cv.box(x0 - 1, x1 + 1, 4, 14, b); cv.box(x0, x1, 3, 3, b); cv.box(x0 + 2, x1 - 2, 2, 2, b)
    cv.box(x0, x0 + 1, 4, 14, l) if f != 'E' else cv.box(x0 - 1, x0, 4, 14, l)
    if f == 'S': cv.box(x0 + 2, x1 - 2, 7, 13, 0)               # face opening
    if f == 'E': cv.box(x1 - 4, x1 + 1, 7, 13, 0)
    return cv.outline(OL)


def item_wizard_hat(f):
    cv = Cv(); x0, x1 = geo(f)['head']
    cv.box(x0 - 3, x1 + 3, 6, 6, 56); cv.box(x0 - 2, x1 + 2, 5, 5, 56)
    for i, y in enumerate(range(4, -1, -1)):
        cv.box(x0 + 1 + i, x1 - 1 - i, y, y, 56)
    cv.box(x0 + 1, x1 - 1, 5, 5, 14)                             # gold band
    cv.set(x0 + 4, 3, 63); cv.set(x1 - 4, 2, 63)
    return cv.outline(OL)


def item_witch_hat(f):
    cv = Cv(); x0, x1 = geo(f)['head']
    cv.box(x0 - 4, x1 + 4, 6, 7, 12); cv.box(x0 - 2, x1 + 2, 5, 5, 12)
    for i, y in enumerate(range(4, -1, -1)):
        cv.box(x0 + 1 + i, x1 - 1 - i, y, y, 12)
    cv.box(x0 + 1, x1 - 1, 5, 5, 10)                             # purple band
    cv.set(x0 + 5, 5, 14)                                        # buckle
    cv.set(x1 - 3, 0, 12); cv.set(x1 - 2, 1, 12)
    return cv.outline(OL)


def item_jack_mask(f):
    cv = Cv(); x0, x1 = geo(f)['head']
    head_shape(cv, f, 11, 11)
    cv.box(x0 - 1, x1 + 1, 7, 13, 11)
    cv.box(15, 16, 3, 4, 15)                                     # stem
    if f == 'S':
        for ex in (x0 + 2, x1 - 4):
            cv.box(ex, ex + 2, 8, 9, 63); cv.set(ex + 1, 7, 63)
        cv.box(x0 + 3, x1 - 3, 12, 12, 63); cv.set(x0 + 3, 11, 63); cv.set(x1 - 3, 11, 63)
        cv.set(x0 + 5, 13, 63); cv.set(x1 - 5, 13, 63)
    elif f == 'E':
        cv.box(x1 - 4, x1 - 2, 8, 9, 63); cv.box(x1 - 5, x1 - 1, 12, 12, 63)
    return cv.outline(OL)


def item_skull_mask(f):
    cv = Cv(); x0, x1 = geo(f)['head']
    if f == 'N':
        return cv
    if f == 'S':
        cv.box(x0 + 1, x1 - 1, 8, 15, 2); cv.box(x0 + 2, x1 - 2, 16, 16, 3)
        cv.box(x0 + 2, x0 + 4, 9, 11, 12); cv.box(x1 - 4, x1 - 2, 9, 11, 12)      # sockets
        cv.box(15, 16, 12, 13, 12)                                                 # nose
        for x in range(x0 + 2, x1 - 1, 2): cv.vline(x, 14, 15, 12)                 # teeth
    else:
        cv.box(x1 - 6, x1, 8, 15, 2); cv.box(x1 - 4, x1 - 2, 9, 11, 12); cv.box(x1 - 1, x1, 12, 13, 12)
        for x in range(x1 - 5, x1, 2): cv.vline(x, 14, 15, 12)
    return cv.outline(OL)


def cape(f, main, trim, collar=None):
    cv = Cv()
    if f == 'E':
        cv.box(9, 14, 16, 40, main); cv.vline(9, 16, 40, trim); cv.hline(9, 14, 40, trim)
    else:
        cv.box(8, 23, 16, 40, main); cv.vline(8, 16, 40, trim); cv.vline(23, 16, 40, trim)
        cv.hline(8, 23, 40, trim)
        for x in range(10, 22, 4): cv.vline(x, 20, 38, trim)
    if collar and f != 'E':
        cv.box(8, 10, 13, 16, collar); cv.box(21, 23, 13, 16, collar)
    return cv.outline(OL)


def item_travellers_cape(f): return cape(f, 58, 59)


def item_keepers_cape(f):
    cv = cape(f, 61, 14)
    return cv


def item_vampire_cape(f): return cape(f, 12, 13, collar=12)


def item_token_amulet(f):
    cv = Cv()
    if f == 'N':
        return cv
    if f == 'S':
        cv.set(13, 16, 14); cv.set(14, 17, 14); cv.set(17, 17, 14); cv.set(18, 16, 14)
        cv.box(15, 16, 18, 19, 14); cv.set(15, 18, 63)
    else:
        cv.set(18, 17, 14); cv.box(18, 19, 18, 19, 14); cv.set(18, 18, 63)
    return cv.outline(OL)


def weapon_x(f): return {'S': 6, 'N': 25, 'E': 21}[f]


def item_wooden_stick(f):
    cv = Cv(); x = weapon_x(f)
    cv.vline(x, 16, 30, 6); cv.vline(x + 1, 16, 30, 7)
    return cv.outline(OL)


def item_quill_sword(f):
    cv = Cv(); x = weapon_x(f)
    cv.vline(x, 10, 27, 8); cv.vline(x + 1, 10, 27, 9); cv.set(x, 9, 2)
    cv.hline(x - 2, x + 3, 28, 14); cv.vline(x, 29, 31, 7); cv.vline(x + 1, 29, 31, 7)
    cv.set(x + 2, 12, 2); cv.set(x + 2, 14, 2)                  # feather barbs
    return cv.outline(OL)


def item_prompt_staff(f):
    cv = Cv(); x = weapon_x(f)
    cv.vline(x, 6, 40, 7); cv.vline(x + 1, 6, 40, 6)
    cv.box(x - 1, x + 2, 2, 5, 62); cv.box(x, x + 1, 3, 4, 63)
    cv.set(x - 2, 4, 14); cv.set(x + 3, 4, 14)
    return cv.outline(OL)


def item_reapers_scythe(f):
    cv = Cv(); x = weapon_x(f)
    cv.vline(x, 4, 42, 7); cv.vline(x + 1, 4, 42, 6)
    for i in range(10):                                          # curved blade sweeping to the right
        yy = 4 + (i * i) // 14
        cv.hline(x + 2, x + 2 + i, yy, 8); cv.hline(x + 2 + i - 1, x + 2 + i, yy + 1, 9)
    return cv.outline(OL)


def item_scholar_robe(f):
    cv = Cv()
    t0, t1 = 10, 21
    cv.box(t0, t1, 16, 29, 56)
    if f == 'E':
        t0, t1 = 12, 19
        cv.box(t0 - 1, t1 + 1, 16, 41, 56); cv.vline(t1, 16, 41, 57)
        cv.box(14, 17, 17, 26, 56); cv.hline(14, 17, 26, 14)
    else:
        cv.box(9, 22, 30, 41, 56)
        cv.box(7, 9, 17, 26, 56); cv.box(22, 24, 17, 26, 56)                      # wide sleeves
        cv.hline(7, 9, 26, 14); cv.hline(22, 24, 26, 14)
        if f == 'S': cv.vline(15, 16, 41, 57)
        cv.hline(9, 22, 41, 14)
        cv.hline(10, 21, 25, 14)                                                    # sash
    return cv.outline(OL)


def item_ghostly_sheet(f):
    cv = Cv()
    cv.box(10, 21, 8, 15, 2)
    cv.box(7, 24, 16, 40, 2)
    for x in range(7, 25):                                       # scalloped hem
        cv.set(x, 41, 2)
        cv.set(x, 43 if x % 4 == 0 else 42, 2)
    cv.vline(7, 16, 41, 3); cv.vline(24, 16, 41, 3)
    cv.box(7, 24, 38, 38, 3)
    if f == 'S':
        cv.box(13, 14, 11, 13, 12); cv.box(17, 18, 11, 13, 12)
    elif f == 'E':
        cv.box(17, 18, 11, 13, 12)
    return cv.outline(OL)


def item_codex_tome(f):
    cv = Cv()
    x = {'S': 23, 'N': 3, 'E': 10}[f]
    cv.box(x, x + 5, 21, 31, 61); cv.vline(x, 21, 31, 14)
    cv.box(x + 2, x + 4, 23, 24, 14); cv.vline(x + 5, 22, 30, 60)
    cv.hline(x, x + 5, 31, 59)
    return cv.outline(OL)


def item_candy_pail(f):
    cv = Cv()
    x = {'S': 23, 'N': 3, 'E': 10}[f]
    cv.box(x, x + 5, 25, 31, 11); cv.hline(x, x + 5, 25, 59); cv.vline(x, 26, 31, 13)
    cv.hline(x + 1, x + 4, 23, 5); cv.set(x, 24, 5); cv.set(x + 5, 24, 5)        # handle
    cv.set(x + 1, 25, 63); cv.set(x + 3, 24, 10); cv.set(x + 4, 25, 13)         # candy peeking
    cv.set(x + 2, 28, 63); cv.set(x + 3, 28, 63); cv.set(x + 3, 29, 63)          # grin
    return cv.outline(OL)


def item_scholar_skirt(f):
    cv = Cv()
    x0 = geo(f)['legs'][0][0]; x1 = geo(f)['legs'][-1][1]
    cv.box(x0 - 1, x1 + 1, 30, 32, 56); cv.box(x0 - 2, x1 + 2, 33, 37, 56)
    cv.box(x0 - 3, x1 + 3, 38, 39, 56)
    cv.hline(x0 - 3, x1 + 3, 39, 14)
    for x in range(x0 - 1, x1 + 2, 3): cv.vline(x, 33, 38, 57)                    # pleats
    return cv.outline(OL)


def item_leather_gloves(f):
    cv = Cv()
    for (a0, a1) in geo(f)['arms']:
        cv.box(max(a0, a1 - 2), a1, 26, 29, 58); cv.hline(max(a0, a1 - 2), a1, 26, 59)
    return cv.outline(OL)


def item_cobweb_boots(f):
    cv = Cv()
    for (f0, f1) in geo(f)['feet']:
        cv.box(f0, f1, 41, 45, 64); cv.hline(f0, f1, 45, 3)
        for x in range(f0, f1 + 1, 2): cv.set(x, 42, 2)
        cv.set(f0, 46, 2); cv.set(f1, 46, 3)                                       # trailing silk
    return cv.outline(OL)


ITEM_FUNCS = {
    'apprentice_hood': item_apprentice_hood, 'wizard_hat': item_wizard_hat, 'witch_hat': item_witch_hat,
    'jack_mask': item_jack_mask, 'skull_mask': item_skull_mask,
    'travellers_cape': item_travellers_cape, 'keepers_cape': item_keepers_cape, 'vampire_cape': item_vampire_cape,
    'token_amulet': item_token_amulet,
    'wooden_stick': item_wooden_stick, 'quill_sword': item_quill_sword, 'prompt_staff': item_prompt_staff,
    'reapers_scythe': item_reapers_scythe,
    'scholar_robe': item_scholar_robe, 'ghostly_sheet': item_ghostly_sheet,
    'codex_tome': item_codex_tome, 'candy_pail': item_candy_pail,
    'scholar_skirt': item_scholar_skirt, 'leather_gloves': item_leather_gloves, 'cobweb_boots': item_cobweb_boots,
}
# plain_tunic, plain_trousers and leather_boots have no overlay: they ARE the base Torso/Legs/Feet layers.

# ------------------------------------------------------------------------------------ atlas
FACINGS = ['S', 'E', 'N', 'W']
FRAMES = 2
SPLIT = 30   # rows >= SPLIT (legs and feet) stay put in the idle frame; everything above "breathes"


def idle_frame(cv):
    out = Cv()
    for y in range(H):
        out.p[y] = cv.p[y][:]
    for y in range(SPLIT - 1, 0, -1):                            # shift the upper body down by one row
        out.p[y] = cv.p[y - 1][:]
    out.p[0] = [0] * W
    out.p[SPLIT] = [a if a else b for a, b in zip(cv.p[SPLIT - 1], cv.p[SPLIT])]
    return out


def four_facings(make):
    """make(f) for f in S, E, N -> Cv; W is E mirrored. Returns the 8 cells (facing-major, 2 frames)."""
    cells = []
    for f in FACINGS:
        cv = make(f) if f != 'W' else make('E').mirrored()
        cells.append(cv)
        cells.append(idle_frame(cv))
    return cells


def layers():
    rows = [('body/A', lambda f: body_layer(f)), ('hands/0', hands_layer), ('feet/0', feet_layer)]
    for s in range(N_HAIR): rows.append((f'hair/{s}', lambda f, s=s: hair_layer(f, s)))
    for s in range(1, N_FACIAL): rows.append((f'facial/{s}', lambda f, s=s: facial_layer(f, s)))
    for s in range(N_TORSO): rows.append((f'torso/{s}', lambda f, s=s: torso_layer(f, s)))
    for s in range(N_ARMS): rows.append((f'arms/{s}', lambda f, s=s: arms_layer(f, s)))
    for s in range(N_LEGS): rows.append((f'legs/{s}', lambda f, s=s: legs_layer(f, s)))
    for item_id, fn in ITEM_FUNCS.items(): rows.append((f'item/{item_id}', fn))
    return rows


def build():
    pal = base_palette()
    rows = layers()
    cols = len(FACINGS) * FRAMES
    sheet_w, sheet_h = cols * W, len(rows) * H
    pixels = [[0] * sheet_w for _ in range(sheet_h)]
    index = {}
    for r, (name, make) in enumerate(rows):
        index[name] = r
        for c, cell in enumerate(four_facings(make)):
            for y in range(H):
                pixels[r * H + y][c * W:(c + 1) * W] = cell.p[y]
    return pal, pixels, sheet_w, sheet_h, index


def main():
    os.makedirs(OUT, exist_ok=True)
    pal, pixels, sw, sh, index = build()
    write_indexed_png(os.path.join(OUT, 'avatar.png'), sw, sh, pixels, pal)
    meta = {
        'cell': {'w': W, 'h': H},
        'facings': FACINGS,
        'frames': FRAMES,
        'idleSplitRow': SPLIT,
        'counts': {'hair': N_HAIR, 'facialHair': N_FACIAL, 'torso': N_TORSO, 'arms': N_ARMS, 'legs': N_LEGS},
        'palette': ['#%02x%02x%02x' % c for c in pal],
        'rows': index,
    }
    with open(os.path.join(OUT, 'avatar.json'), 'w') as f:
        json.dump(meta, f, indent=1, sort_keys=True)
        f.write('\n')
    print(f'wrote avatar.png ({sw}x{sh}, {len(index)} layers) and avatar.json')


if __name__ == '__main__':
    main()
