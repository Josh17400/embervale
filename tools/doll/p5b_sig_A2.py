#!/usr/bin/env python3
"""tools/doll/p5b_sig_A2.py (P5b production round 2, area A, docs/STYLE_P5B.md 4-5; the owner's rulings of 2026-11-06): area A's
T5-T6 heads as ADDED views.

Area A, round 2: the Highland's and the Heartland's elite tiers (T5-T6), the slice peoples' T6 heads grown from the T5 the owner
approved (the Fjordfolk jarl's horns, the Jade kuwagata, the Imperial Attic crest and its laurel), and the Fjordfolk T4 spectacle
helm's mail drawn in courses (C16n: its checker read as 1 px noise).

As tools/doll/p5b_sig.py and p5b_sig_A.py (whose blocks this one never touches): every block written here is new
`when sighelm-<people>,...` views and the local legend characters they use, inserted before a part's first view or first P5b
block (a `replace` view wins in file order). No drawn view's cells change. Re-running replaces this script's own blocks only.

  python tools/doll/p5b_sig_A2.py            write the blocks into rpg/art/doll/maps
  python tools/doll/p5b_sig_A2.py --verify   every original line of each touched map is still there, in order, and every
                                             added line sits inside a P5b block
  python tools/doll/p5b_sig_A2.py --print    print the blocks

Drawing: each grid below is written in this script's own symbols (SYM), translated per map into that map's legend characters
(the used legend's where it has one, else a free character with a local `legend` line), so no map's legend is redefined.

The heads (rpg/core/fingerprint.h rows: the family and tier pick the part; these views dress it):
  Highland T5-T6  headwear/helm/kettle (replace): the chief's kettle, its gilt (Metal 2: sighelmgilt at T5) finial and brim, the
                  fur band, the eagle feathers (leather browns) behind a gilt brooch; T6 three feathers fanned from the badge
  Heartland T5-T6 headwear/helm/greathelm (overlay): the torse and the mantling on the Heartland's signed great helm; the row signs
                  each helm with its own head (""), so the hero's road keeps its onion, crystal, mask and furnace helm (D83)
  Fjordfolk T4    headwear/helm/spangen (replace, ahead of area A's first views, which stay): the spectacle helm, mail in courses
  Fjordfolk T6    headwear/helm/horned (overlay): the jarl's horns grown, a boar-crested ridge, a garnet
  Imperial T6     headwear/helm/crested (overlay): the Attic crest grown, the laurel in the people's gold (dye2)
  Jade T6         data only: the kabuto legend's T6 horns view (the T5 kuwagata grown), no view drawn here
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..', '..')
MAPS = os.path.join(ROOT, 'rpg', 'art', 'doll', 'maps')
BASE = '37331730328157e70cdd6b26c057fc63f5929a7c'   # STYLE_P5B's commit: the maps before P5b
TAG = '(P5b SIG-A2)'
ANY_P5B = '# (P5b SIG'

# ------------------------------------------------------------------------------------------------ symbols
# metal v w x y z (0-4: the outline to the glint); the gilt a b c d e (Device 0-4: the T5-T6 device's gold); the plume
# Q R S T U (0-4: the people's crest colour, its dominant); leather f g h i j; the dye pair (the maker's thread: D dye 1-3 as
# 1 2 3, dye2 1-3 as 4 5 6); the gem G H (glow 3-4, the living finish's breath); '-' erase, '.' keep
SYM = {}
for i, c in enumerate('vwxyz'):
    SYM[c] = ('metal', i)
for i, c in enumerate('abcde'):
    SYM[c] = ('device', i)
for i, c in enumerate('QRSTU'):
    SYM[c] = ('plume', i)
for i, c in enumerate('fghij'):
    SYM[c] = ('leather', i)
for i, c in enumerate('KLMNO'):
    SYM[c] = ('metal2', i)
for i, c in enumerate('0123'):
    SYM[c] = ('dye', i + 1)
for i, c in enumerate('4567'):
    SYM[c] = ('dye2', i + 1)
SYM['G'] = ('glow', 3)
SYM['H'] = ('glow', 4)
for i, c in enumerate('klmno'):
    SYM[c] = ('cloak', i)
for i, c in enumerate('pqrst'):
    SYM[c] = ('linen', i)
FREE = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ&%$!?;:<>=+*^~_{}[]@'


def read_legends(path):
    """{char: (slot, step)} of the person legend and the map's own legend lines (the map's file-wide local legend)"""
    leg = {}
    uses = []
    for line in open(path, encoding='utf-8'):
        t = line.split()
        if len(t) >= 2 and t[0] == 'use':
            uses += t[1:]
    for u in uses:
        lp = os.path.join(MAPS, 'legend', u + '.legend')
        if os.path.exists(lp):
            for line in open(lp, encoding='utf-8'):
                t = line.split()
                if len(t) == 4 and t[0] == 'legend':
                    leg[t[1]] = (t[2], int(t[3]))
    for line in open(path, encoding='utf-8'):
        if line.startswith(f'# {TAG} begin'):
            break   # (our own block's lines are rewritten on each write)
    in_mine = False
    for line in open(path, encoding='utf-8'):
        if line.startswith(f'# {TAG} begin'):
            in_mine = True
        elif line.startswith(f'# {TAG} end'):
            in_mine = False
            continue
        t = line.split()
        if not in_mine and len(t) == 4 and t[0] == 'legend':
            leg[t[1]] = (t[2], int(t[3]))
    return leg


class Palette:
    """this script's symbols to one map's characters: the map's own character for the (slot, step) where it has one, else a
    character no legend of the map uses, declared once in the map's first A2 block"""
    def __init__(self, path):
        self.leg = read_legends(path)
        self.by = {}
        for ch, ss in self.leg.items():
            self.by.setdefault(ss, ch)
        self.new = []

    def ch(self, sym):
        if sym in '.-':
            return sym
        ss = SYM[sym]
        if ss in self.by:
            return self.by[ss]
        used = set(self.leg) | {c for c, _ in self.new}
        c = next(c for c in FREE if c not in used)
        self.new.append((c, ss))
        self.by[ss] = c
        return c

    def rows(self, rows):
        return [''.join(self.ch(s) for s in r) for r in rows]

    def legend_lines(self):
        return [f'legend {c} {s} {k}' for c, (s, k) in self.new]


def mirror(rows):
    return [r[::-1] for r in rows]


RELIT = {'w': 'y', 'y': 'w', 'b': 'd', 'd': 'b', 'S': 'T', 'T': 'S', 'g': 'i', 'i': 'g'}


def relight(rows):
    """facing L from facing R's drawing: mirrored, and its lit and shaded steps swapped, so the light stays top-left"""
    return [''.join(RELIT.get(c, c) for c in r[::-1]) for r in rows]


def regions_of(rows, table):
    return [''.join(table.get(c, '.') for c in r) for r in rows]


def overlay(base, top):
    """top over base ('.' in top keeps the base); both lists of strings, top's size wins"""
    out = []
    for y in range(max(len(base), len(top))):
        b = base[y] if y < len(base) else ''
        t = top[y] if y < len(top) else ''
        w = max(len(b), len(t))
        b, t = b.ljust(w, '.'), t.ljust(w, '.')
        out.append(''.join(tc if tc != '.' else bc for bc, tc in zip(b, t)))
    return out




# ================================================================================================ the Highland chief (T5-T6)
# headwear/helm/kettle at T5 and T6 (replace): the T3-T4 kettle grown into the chief's: the crown a row taller under a gilt
# finial, its fur band (the T3-T4 Highland kettle's own), a broad brim rimmed in gilt, the mail aventail at the cheeks; the
# eagle feathers (in the people's crest green, the T3-T4 feather grown: STYLE_P5B 4 "two eagle feathers", T6 "three, the
# chief's") rising behind a gilt clan brooch on the figure's left (the side of the T2 badge). T6: the third feather, the
# brooch's cairngorm (the gem: the living finish's breath), a gilt ridge up the crown's front, the brim a column wider.
# The crown: 22 columns (the kettle's 20 and one each side: origin x 3), rows from base row -6 (origin y 7)
HLC5_D = [
    '......................',   # -6
    '......................',
    '......................',
    '......................',
    '......................',
    '..........vv..........',   # -1 the finial
    '.........vedv.........',   # 0
    '.........vcbv.........',
    '.......vvvwwvvv.......',   # 2 the crown
    '......vzzyyyxxwv......',
    '.....vzyyyyxxxxwv.....',
    '.....vyyyyyxxxxwv.....',
    '....vzyyyyyxxxxwwv....',   # 6
    '....ghihghihghihgh....',   # 7 the fur band
    '..vdeeeeeeeeddddddcv..',   # 8 the brim, gilt (T3's width: C3b's T5 brim cap)
    '..vabcccccccbbbbbbav..',   # 9 its rim
    '...vyw..........wxv...',   # 10 the aventail
    '...vwy..........xwv...',
    '...vyw..........wxv...',
    '...vwy..........xwv...',
    '...vyxv........vwxv...',
    '...vvvv........vvvv...',   # 15
]
HLC6_D = [
    '......................',
    '......................',
    '......................',
    '......................',
    '......................',
    '......................',   # -1
    '......................',
    '......................',
    '.......vvvwwvvv.......',   # 2
    '......vzzyyyxxwv......',
    '.....vzyyyyxxxxwv.....',
    '.....vyyyyyxxxxwv.....',
    '....vzyyyyyxxxxwwv....',
    '....ghihghihghihgh....',   # 7
    'vdeeeeeeeeeedddddddddv',   # 8 the brim a column wider each side, its ends turned down
    'vabbcccccccbbbbbbbbbav',   # 9
    'vv.vyw..........wxv.vv',
    '...vwy..........xwv...',
    '...vyw..........wxv...',
    '...vwy..........xwv...',
    '...vyxv........vwxv...',
    '...vvvv........vvvv...',
]
# the eagle feathers (the people's crest green, lit on their left edge) in a bundle behind the gilt clan brooch on the band
HLF5_D = [   # (inside T5's 2 columns past the head box: C3b reads the kettle's every pixel as its brim)
    '......................',   # -6
    '................QQSQ..',   # -5 the long feather's tip, curling back
    '............Q..QUTSQ..',   # -4 the second's tip
    '...........QUQ.QUTSQ..',
    '...........QUSQQUTSQ..',
    '............QTSQUTSQ..',
    '............QUTSQUTSQ.',   # 0
    '.............QTSQTSQ..',
    '.............QTSQTSQ..',
    '..............QSQTSQ..',   # 3 the bundle
    '..............QTQTQ...',
    '..............QSQSQ...',
    '..............QQQQ....',
    '..............NONO....',   # 7 the clan brooch (gilt)
]
HLF6_D = [   # (rows -8..2: two rows over the T5 grid) the chief's three feathers fanned from the clan badge on the crown's front
    '...........QQ.........',   # -8
    '..........QUSQ....QQ..',
    '..........QUTQ...QUSQ.',
    '....QQ....QUTQ..QUTSQ.',
    '...QUSQ...QUTQ..QUTQ..',
    '...QUTSQ..QUTQ.QUTSQ..',
    '....QUTSQ.QUTQ.QUTQ...',   # -2
    '.....QUTSQQTSQQUTSQ...',
    '......QUTSQTSQUTSQ....',   # 0
    '.......QQTSQSQQQ......',
    '.........KOGNK........',   # 2 the badge and its cairngorm on the crown's front
]
# facing R (23 columns, rows -6..15: origin 4 7): the crown, its brim reaching further before the face; the feathers on the far
# side (the figure's left), seen above the crown and leaning back
HLC5_R = [
    '.......................',
    '.......................',
    '.......................',
    '.......................',
    '.......................',
    '...........vv..........',   # -1
    '..........vedv.........',
    '..........vcbv.........',
    '........vvvwwvvv.......',   # 2
    '.......vzzyyyxxwv......',
    '......vzyyyyyxxxwv.....',
    '......vyyyyyxxxxwv.....',
    '.....vyyyyyyyxxxwwv....',
    '.....ghihghihghihgh....',   # 7
    '...vdeeeeeeeeeeddddcv..',   # 8 (T3's width: C3b's T5 brim cap)
    '...vabccccccccbbbbbav..',   # 9
    '....vyxw...............',
    '....vxwx...............',
    '....vyxw...............',
    '....vxwx...............',
    '....vvvv...............',
    '.......................',
]
HLC6_R = [
    '.......................',
    '.......................',
    '.......................',
    '.......................',
    '.......................',
    '.......................',
    '.......................',
    '.......................',
    '........vvvwwvvv.......',
    '.......vzzyyyxxwv......',
    '......vzyyyyyxxxwv.....',
    '......vyyyyyxxxxwv.....',
    '.....vyyyyyyyxxxwwv....',
    '.....ghihghihghihgh....',
    '.vdeeeeeeeeeeeeddddddcv',
    '.vabbcccccccccbbbbbbbav',
    '.vv.vyxw.............vv',
    '....vxwx...............',
    '....vyxw...............',
    '....vxwx...............',
    '....vvvv...............',
    '.......................',
]
HLF5_R = [
    '.......................',   # -6
    '..QQSQ.................',   # -5 both feathers leaning back, side by side
    '..QUTSQQ...............',
    '...QUTQUSQ.............',
    '....QUTQTSQ............',
    '.....QUTQTSQ...........',
    '......QTSQTSQ..........',   # 0
    '.......QTSQSQ..........',
    '........QSQQ...........',
    '.........QQ............',
]
HLF6_R = [   # (rows -8..2) the fan from the side: upright, back, further back
    '..........QQ...........',   # -8
    '.........QUSQ..........',
    '.....QQ..QUTQ..........',
    '....QUSQ.QUTQ..........',
    '.QQ.QUTSQQUTQ..........',
    'QUSQ.QUTSQUTQ..........',
    'QUTSQQUTSQTQ...........',   # -2
    '.QUTSQQUTSQQ...........',
    '..QUTSQUTSQ............',   # 0
    '...QQQQQTSQ............',
    '........QQKON..........',   # 2 the badge on the crown's front
]
GILT_M2 = {'a': 'K', 'b': 'L', 'c': 'M', 'd': 'N', 'e': 'O'}   # the gilt as Metal 2 (the T5 helm's fittings drawn gilt: sighelmgilt)
FEATHERS = {'leather': {'Q': 'f', 'R': 'g', 'S': 'h', 'T': 'i', 'U': 'j'}, 'plume': {},
            'linen': {'Q': 'p', 'R': 'q', 'S': 'r', 'T': 's', 'U': 't'}, 'dye2': {'Q': 'v', 'R': '4', 'S': '5', 'T': '6', 'U': '7'}}
FEATHER = FEATHERS['leather']   # (the eagle's browns read as feathers against the grass; the crest green read as leaves)


def recol(rows, table):
    return [''.join(table.get(c, c) for c in r) for r in rows]


def highland(tier):
    cd, fd = (HLC5_D, HLF5_D) if tier == 5 else (['.' * 22] * 2 + HLC6_D, HLF6_D)
    cr, fr = (HLC5_R, HLF5_R) if tier == 5 else (['.' * 23] * 2 + HLC6_R, HLF6_R)
    top = 0 if tier == 5 else 2   # the T6 grid two rows taller
    cd, cr = recol(cd, GILT_M2), recol(cr, GILT_M2)
    fd, fr = recol(recol(fd, GILT_M2), FEATHER), recol(recol(fr, GILT_M2), FEATHER)
    h = 22 + top
    d = stack(grid(cd, 22), grid(fd, 22, h))
    uc = grid(cd, 22)
    for y in range(16 + top, 21 + top):   # facing U: the aventail a mail curtain across the nape, in courses
        uc[y] = '...v' + ('xy'[y % 2] * 14) + 'v...'
    uc[21 + top] = '...vvvvvvvvvvvvvvvv...'
    u = stack(uc, mirror(grid(fd, 22, h)))
    r = stack(grid(fr, 23, h), grid(cr, 23))                       # the feathers behind the crown
    lc = relight(grid(cr, 23))
    if tier == 5:
        lc[13] = lc[13][:10] + '.ON' + lc[13][13:]                # the brooch on the near side of the band
    l = stack(lc, relight(grid(fr, 23, h)))                        # facing L, the near side: before it
    reg = {'K': 'R', 'L': 'R', 'M': 'R', 'N': 'R', 'O': 'R', 'G': 'G', 'H': 'G'}
    mk = lambda x: [('crown', x, 9 + top)]
    oy = 7 + top
    return [('D', 'rest', d, regions_of(d, reg), (3, oy), mk(10)), ('U', 'rest', u, regions_of(u, reg), (3, oy), mk(10)),
            ('R', 'rest', r, regions_of(r, reg), (4, oy), mk(11)), ('L', 'rest', l, regions_of(l, reg), (3, oy), mk(11)),
            ('L', 'lie', l, regions_of(l, reg), (3, oy), mk(11))]


# ================================================================================================ the Heartland elite (T5-T6)
# headwear/helm/greathelm (fam.great) at T5 and T6 (an overlay on the drawn great helm): the knight's heraldry over the barrel
# helm (STYLE_P5B 4: the great helm, the census knights'; heartland_1350_topfhelm): a torse twisted in the people's red and
# white on the crown, the mantling (red, its white lining showing at the edges) falling from it down both sides of the helm
# to dagged ends at the cheeks, and the crest, a red plume. T6: a crown (Metal 2: the helm's fittings) on the torse with a
# gem (the living finish's breath), the plume a row taller, the mantling longer and dagged twice. Drawn on the Heartland's
# signed T5-T6 helm only: the hero's road keeps its gilt onion and crystal (D83: a relic metal's own helm wins)
# D: 22 columns (origin 3 _: a column each side of the helm's 20), rows from base row -7 (origin y 8)
HH5_D = [
    '......................',   # -7
    '......................',
    '..........QQ..........',   # -5 the plume
    '.........QUSQ.........',
    '.........QUTQ.........',
    '........QUTSQ.........',
    '........QUTSQ.........',   # -1
    '.........QTQ..........',   # 0
    '.......02266220.......',   # 1 the torse, twisted in 2 px turns
    '......0662266221......',   # 2
    '.....012......110.....',   # 3 the mantling leaves the torse's ends
    '....012........110....',
    '...0125........5110...',   # 5
    '..0125..........5110..',
    '.01256..........51110.',
    '.0125............5110.',
    '.01256..........51110.',
    '.0125............5110.',   # 10
    '.01256..........51110.',
    '00125............51100',   # 12
    '0.10.5..........5.010.',   # 13 the dags
    '0..0..............0..0',
]
HH6_D = [
    '..........QQ..........',   # -7 the plume a row taller
    '.........QUSQ.........',
    '.........QUTQ.........',
    '........QUTSQ.........',
    '........QUTSQ.........',
    '.......KQUTSQK........',   # -2 the crown's points
    '.......KOQTQNK........',
    '......KOOKGKNNK.......',   # 0 the crown and its gem
    '......02266226........',   # 1 the torse
    '.....066226622110.....',
    '....012..........110..',   # 3
    '...012............110.',
    '..0125............5110',
    '.01256............5111',
    '.0125.............5110',
    '.01256............5111',
    '.0125.............5110',   # 9
    '.01256............5111',
    '.0125.............5110',
    '00125.............5110',   # 12
    '0.125.............5110',
    '0.10.5..........5.0100',   # 14 dagged twice
    '0..0..............0..0',
]
# facing R (23 columns, origin 4 8): the mantling falls behind the head (the left) from the torse; the plume over the crown
HH5_R = [
    '.......................',
    '.......................',
    '...........QQ..........',
    '..........QUSQ.........',
    '..........QUTQ.........',
    '.........QUTSQ.........',
    '.........QUTSQ.........',
    '..........QTQ..........',
    '........0226622........',   # 1
    '......016622662........',
    '.....0125..............',   # 3
    '....01256..............',
    '...01256...............',
    '..01256................',
    '..0125.................',
    '.01256.................',
    '.0125..................',   # 9
    '.01256.................',
    '.0125..................',
    '00125..................',
    '0.10.5.................',   # 13
    '0..0...................',
]
HH6_R = [
    '...........QQ..........',
    '..........QUSQ.........',
    '..........QUTQ.........',
    '.........QUTSQ.........',
    '.........QUTSQ.........',
    '........KQUTSQK........',
    '........KOQTQNK........',
    '.......KOOKGKNNK.......',
    '.......02266226........',   # 1
    '.....0166226622........',
    '....0125...............',
    '...01256...............',
    '..01256................',
    '.01256.................',
    '.0125..................',
    '01256..................',
    '0125...................',
    '01256..................',
    '0125...................',
    '0125...................',
    '0.125..................',
    '0.10.5.................',
    '0..0...................',
]
RELIT_DYE = {'2': '1', '1': '0', '6': '5'}   # the far side's mantling in its shade


PLUMELESS = {c: '.' for c in 'QRSTUKLMNOG'}   # the plume and the crown: the family's own legend growth draws its crest


def heartland(tier):
    d0 = recol(HH5_D if tier == 5 else HH6_D, PLUMELESS)
    r0 = recol(HH5_R if tier == 5 else HH6_R, PLUMELESS)
    d = grid(d0, 22)
    # facing U: the mantling falls over the helm's back from the torse, a seam down its middle
    u = [r for r in grid(mirror(d0), 22)]
    top = 10 if tier == 5 else 10
    for y in range(top, len(u) - 2):
        row = list(u[y])
        xs = [x for x, c in enumerate(row) if c != '.']
        if len(xs) >= 2:
            for x in range(xs[0] + 1, xs[-1]):
                if row[x] == '.':
                    row[x] = '0' if x in (10, 11) else ('1' if x > 11 else '2')
        u[y] = ''.join(row)
    r = grid(r0, 23)
    l = relight(r)
    l = [''.join(RELIT_DYE.get(c, c) if x > 9 else c for x, c in enumerate(row)) for row in l]
    reg = {'K': 'R', 'L': 'R', 'M': 'R', 'N': 'R', 'O': 'R', 'G': 'G'}
    return [('D', 'rest', d, regions_of(d, reg), (3, 8), None), ('U', 'rest', u, regions_of(u, reg), (3, 8), None),
            ('R', 'rest', r, regions_of(r, reg), (4, 8), None), ('L', 'rest', l, regions_of(l, reg), (3, 8), None),
            ('L', 'lie', l, regions_of(l, reg), (3, 8), None)]


# ================================================================================================ the Fjordfolk jarl (T6)
# headwear/helm/horned at T6 (an overlay): the T5 jarl's horns (the slice's, which the owner signed) grown at the saga's top:
# a column further out and two rows higher, thicker at the root, the tips curling in; a boar-crested ridge along the dome
# (the Sutton Hoo crest) and a garnet at the brow (the living finish's breath). Metal 2 (the T6 alloy's fittings: K L M N O
# its steps 0-4). D: 26 columns (origin 5 6: the base's 24 and one each side), rows from base row -5
FJ6_HORN_L = [   # the left horn facing D (columns 0-9), rows -5..8; (P5b2 fix r1, judge item 4: C12f's cap, 3 columns past the
    # head box, the head box from column 5) the sweep two columns in from the first drawing's, the tip curling in as before
    '......KK..',
    '.....KOMK.',
    '....KOMK..',
    '...KOMK...',
    '...KONK...',
    '..KONK....',   # 0
    '..KONMK...',
    '..KONMK...',
    '..KONMLK..',
    '..KONMLK..',
    '..KONMLK..',   # 5
    '..KONMLLK.',
    '...KONMLLK',
    '...KKMLLK.',   # 8 the root at the dome's side
]
HORN_SHADE = {'O': 'L', 'L': 'O', 'N': 'M', 'M': 'N'}   # the right horn: its lit edge the inner one


def fj6_d():
    w = 26
    g = [['.'] * w for _ in range(14)]
    for y, row in enumerate(FJ6_HORN_L):
        for x, c in enumerate(row):
            if c != '.':
                g[y][x] = c
                g[y][w - 1 - x] = HORN_SHADE.get(c, c)
    rows = [''.join(r) for r in g]
    # the ridge (a boar at its front end) up the dome's middle (the base dome's columns 9-14: here 10-15), the garnet at the brow
    ridge = {3: '...........KOOK.', 4: '..........KONMLK', 5: '............ON..', 6: '............ON..', 7: '............ON..',
             8: '............ON..', 9: '............ON..', 10: '............ON..', 11: '............ON..', 12: '............ON..',
             13: '............GH..'}
    for y, r in ridge.items():
        rows[y] = overlay([rows[y]], [r.ljust(w, '.')])[0]
    return rows


def fj6_r():
    w = 27
    g = [['.'] * w for _ in range(14)]
    for y, row in enumerate(FJ6_HORN_L):   # the far horn behind the dome (left), the near one before the brow (right)
        for x, c in enumerate(row):
            if c != '.':
                g[y][x] = c
                g[y][w - 1 - x] = HORN_SHADE.get(c, c)
    rows = [''.join(r) for r in g]
    ridge = {6: '..........KOOOOOOK.', 7: '.........KONNNNNNK..', 8: '...........KMMMMK....', 13: '.................GH.'}
    for y, r in ridge.items():
        rows[y] = overlay([rows[y]], [r.ljust(w, '.')])[0]
    return rows


def fjordfolk6():
    d = fj6_d()
    r = fj6_r()
    l = relight(r)
    # (P5b2 fix r1, judge item 5: the trim band 10-36 %) the horns' rim (their outline step) is the trim, their body the metal
    reg = {'K': 'R', 'G': 'G', 'H': 'G'}
    return [('D', 'rest', d, regions_of(d, reg), (5, 6), None), ('U', 'rest', d, regions_of(d, reg), (5, 6), None),
            ('R', 'rest', r, regions_of(r, reg), (6, 6), None), ('L', 'rest', l, regions_of(l, reg), (5, 6), None),
            ('L', 'lie', l, regions_of(l, reg), (5, 6), None)]


# ================================================================================================ the Imperial legate (T6)
# headwear/helm/crested at T6 (an overlay on its T6 view, origin 3 7): the legate's TRANSVERSE crest (P5b2 fix r1, judge item 4:
# the grown Attic fan read as a red faceted gem facing D and rose 14 rows over the crown, the cap 10): the crimson horsehair
# (Plume: the people's crest crimson) set ear to ear on the comb's gilt stalk, a broad low fan facing D and U (18 columns, its
# strands lit from the top-left, its ends hanging past the bowl) and a brush edge-on facing R and L (the crista transversa of the
# reliefs), two rows over the comb's top at most (10 over the crown); the comb's gilt shows under it (sighelmgilt from T5); a
# laurel wreath (the people's gold, dye2) bound round the brow, its leaves rising at the temples past the bowl.
# D: 22 columns (the T6 view's own), rows from base row -5 (origin 3 12)
IM6_D = [
    '......................',   # -5
    '......................',
    '......................',
    '......................',
    '......QQQQQQQQQQ......',   # -1 the fan's crown, 10 rows over the crown (C12f's T6 cap)
    '....QQUUTTTSSSRRQQ....',   # 0
    '...QUUTUTTSTSSRSRRQ...',   # 1 the strands, lit from the top-left
    '..QUTUTTSTSSRSRSRRRQ..',
    '..QUTTSTSSSRSRSRRRRQ..',
    '..QTTSSSSSSRRRRRRRRQ..',   # 4 the root on the bowl (the comb inside it)
    '..QQTSQQ......QQRRQQ..',   # 5 the ends hanging past the bowl, their roots closed on it (C3a: no pocket over the bowl)
    '...QQQQ........QQQQ...',
    '......................',
    '......................',
    '....K............K....',   # 9 the laurel's leaves rising at the temples
    '...KOK..........KMK...',
    '..KONK..........KMLK..',
    '..KNOMK........KMLMK..',
    '...KNOMK......KMLMK...',   # 13 the brow band (the wreath bound round it)
]
IM6_R = [
    '.....................',   # -5 the crest edge-on: a brush standing on the bowl
    '.....................',
    '.....................',
    '.....................',
    '.....................',
    '..........QQQ........',   # 0
    '.........QUTSQ.......',
    '.........QUTSQ.......',
    '.........QUTSRQ......',
    '........QUTSSRQ......',
    '........QTSSRRQ......',   # 5 on the bowl's top
    '.....................',
    '.....................',
    '.....................',
    '.....K...............',   # 9 the laurel at the near temple
    '....KOK..............',
    '...KONK..............',
    '...KNOMK.............',
    '....KNOMK............',
]


LAUREL = {'K': '0', 'L': '4', 'M': '5', 'N': '6', 'O': '7'}   # the laurel in the people's gold (dye2: the accent), its edge the dye


def imperial6():
    d = grid(IM6_D[:14] + recol(IM6_D[14:], LAUREL), 22)
    r = grid(IM6_R[:14] + recol(IM6_R[14:], LAUREL), 21)
    l = relight(r)
    reg = {'K': 'R', 'L': 'R', 'M': 'R', 'N': 'R', 'O': 'R'}
    return [('D', 'rest', d, regions_of(d, reg), (3, 12), None), ('U', 'rest', mirror(d), regions_of(mirror(d), reg), (3, 12), None),
            ('R', 'rest', r, regions_of(r, reg), (3, 13), None), ('L', 'rest', l, regions_of(l, reg), (2, 13), None),
            ('L', 'lie', l, regions_of(l, reg), (2, 13), None)]


# ================================================================================================ the Fjordfolk T4 mail (C16n)
# the owner's spectacle helm (tools/doll/p5b_sig_A.py, the Gjermundbu) re-laid with its mail aventail in courses: the area's
# own views, cell for cell, but the mail's rings in rows (a lit course over a shaded one) where its first drawing laid them in
# a checker (C16n: a_fj2 R 9-10 % 1 px features, the cap 3 %; STYLE_P5B 2.3 "never a checker or dither"). Drawn first in the
# map, so these replace views win; the first drawing stays in the map (never deleted)
A1_TO_SYM = {'S': 'K', 'N': 'L', 'O': 'M', 'P': 'N', 'T': 'O'}


def gjermundbu_courses():
    sys.path.insert(0, HERE)
    import p5b_sig_A as A1   # noqa: E402  (area A's first script: its masks and renderer, never its blocks)
    _, views = A1.gjermundbu()
    masks = {'D': A1.GJ_D, 'U': A1.GJ_U, 'R': A1.GJ_R, 'L': A1.relight_mirror(A1.GJ_R)}
    out = []
    for f, k, rows, reg in views:
        m = masks[f]
        g = []
        for y, r in enumerate(rows):
            row = []
            for x, c in enumerate(r):
                mc = m[y][x] if y < len(m) and x < len(m[y]) else '.'
                if mc in 'mM':
                    run = 1   # the mail's run along the row: a broad curtain in courses (a lit row of rings over a shaded
                    while x - run >= 0 and m[y][x - run] in 'mM':   # one), a strip at the cheek one step down its length
                        run += 1
                    xr = x + 1
                    while xr < len(m[y]) and m[y][xr] in 'mM':
                        run += 1
                        xr += 1
                    row.append(('x' if y % 2 == 0 else 'w') if run >= 3 else ('x' if x < len(r) / 2 else 'w'))
                else:
                    row.append(A1_TO_SYM.get(c, c))
            g.append(''.join(row))
        out.append((f, k, g, reg, None, None))
    return out


def plan():
    """[(map path, tag, desc, views, cond, replace)]"""
    out = []
    ket = os.path.join(MAPS, 'headwear', 'helm', 'kettle.pxm')
    out.append((ket, 'highland-chief5', 'the Highland T5 chief\'s kettle: a gilt finial and rim, the fur band, two eagle feathers '
                'behind a gilt clan brooch', highland(5), 'sighelm-highland,helm.t5,!helm.t6', True))
    out.append((ket, 'highland-chief6', 'the Highland T6 chief\'s kettle: a gilt ridge, the brim wider, three eagle feathers, the '
                'brooch\'s cairngorm', highland(6), 'sighelm-highland,helm.t6', True))
    gh = os.path.join(MAPS, 'headwear', 'helm', 'greathelm.pxm')
    out.append((gh, 'heartland-knight5', 'the Heartland T5 knight\'s heraldry on the great helm: the torse, the red mantling and its '
                'white lining, the red plume (an overlay)', heartland(5), 'sighelm-heartland,helm.t5,!helm.t6', False))
    out.append((gh, 'heartland-knight6', 'the Heartland T6 knight\'s heraldry: a crown and its gem on the torse, the plume taller, '
                'the mantling longer, dagged twice (an overlay)', heartland(6), 'sighelm-heartland,helm.t6', False))
    hor = os.path.join(MAPS, 'headwear', 'helm', 'horned.pxm')
    out.append((hor, 'fjordfolk-jarl6', 'the Fjordfolk T6 jarl: the T5 horns grown, a boar-crested ridge, a garnet at the brow (an '
                'overlay)', fjordfolk6(), 'sighelm-fjordfolk,helm.t6', False))
    spa = os.path.join(MAPS, 'headwear', 'helm', 'spangen.pxm')
    out.append((spa, 'fjordfolk-gjermundbu-courses', 'the Fjordfolk T4 spectacle helm (area A, the owner fix), its mail '
                'aventail in courses (C16n)', gjermundbu_courses(), 'helm.t4,!helm.t5,sighelm-fjordfolk', True))
    cr = os.path.join(MAPS, 'headwear', 'helm', 'crested.pxm')
    out.append((cr, 'imperial-legate6', 'the Imperial T6 legate: the Attic crest grown, the laurel wreath (an overlay)', imperial6(),
                'sighelm-imperial,helm.t6', False))
    return out


def grid(rows, w, h=None):
    rows = [r.ljust(w, '.') for r in rows]
    if h is not None:
        rows += ['.' * w] * (h - len(rows))
    return rows


def stack(*layers):
    out = layers[0]
    for t in layers[1:]:
        out = overlay(out, t)
    return out


# ------------------------------------------------------------------------------------------------ the pxm plumbing
def read_views(path):
    """{(facing, key, when): (origin, [rows])} of a map's views (the first of each)"""
    out, cur = {}, None
    for line in open(path, encoding='utf-8'):
        s = line.rstrip('\n')
        if s.startswith('view '):
            t = s.split()
            when = s.split(' when ', 1)[1].split()[0] if ' when ' in s else ''
            org = (int(t[t.index('origin') + 1]), int(t[t.index('origin') + 2])) if 'origin' in t else None
            key = (t[1], t[3], when)
            cur = None
            if key not in out:
                cur = []
                out[key] = (org, cur)
            continue
        if cur is None:
            continue
        if s.startswith('|'):
            cur.append(s[1:])
        elif s.strip() and not s.startswith('mark'):
            cur = None
    return out


def build_blocks(path, items):
    """every block of one map through one palette, the new legend lines in the first block. items: (tag, desc, views, cond,
    replace); views: (facing, key, rows in SYM, regions, origin, marks)"""
    pal = Palette(path)
    blocks = []
    for tag, desc, views, cond, replace in items:
        body = []
        for f, k, g, reg, org, marks in views:
            o = f' origin {org[0]} {org[1]}' if org else ''
            body.append(f'view {f} key {k}{o} when {cond}{" replace" if replace else ""}')
            for name, x, y in marks or []:
                body.append(f'mark {name} {x} {y}')
            body += ['|' + r for r in pal.rows(g)]
            if reg and any(c != '.' for r in reg for c in r):
                body.append('regions')
                body += ['|' + r for r in reg]
        blocks.append((tag, desc, body))
    out = []
    for n, (tag, desc, body) in enumerate(blocks):
        out.append(f'# {TAG} begin {tag}: {desc}')
        if n == 0:
            out += pal.legend_lines()
        out += body
        out.append(f'# {TAG} end {tag}')
    return out


def strip_blocks(lines, every=False):
    """the lines without this script's blocks (every: without every P5b block)"""
    mine_b, mine_e = f'# {TAG} begin ', f'# {TAG} end '
    is_b = lambda s: s.startswith(ANY_P5B) and ' begin ' in s
    is_e = lambda s: s.startswith(ANY_P5B) and ' end ' in s
    out, skip = [], False
    for s in lines:
        if not skip and (s.startswith(mine_b) or (every and is_b(s))):
            skip = True
            continue
        if skip and (s.startswith(mine_e) or (every and is_e(s))):
            skip = False
            continue
        if not skip:
            out.append(s)
    return out


def mine_on_disk():
    out = []
    for d, _, fs in os.walk(MAPS):
        for f in fs:
            if f.endswith('.pxm') and f'# {TAG} begin ' in open(os.path.join(d, f), encoding='utf-8').read():
                out.append(os.path.normpath(os.path.join(d, f)))
    return out


def by_file():
    out = {}
    for path in mine_on_disk():   # (a map whose blocks the plan dropped: written without them)
        out.setdefault(path, [])
    for path, tag, desc, views, cond, replace in plan():
        out.setdefault(os.path.normpath(path), []).append((tag, desc, views, cond, replace))
    return out


def write():
    for path, items in by_file().items():
        src = strip_blocks(open(path, encoding='utf-8').read().split('\n'))
        open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(src))   # (ours out first: the palette reads the rest)
        add = build_blocks(path, items) if items else []
        # before the first view or the first P5b block (a replace view of ours wins over every later one)
        i = next(n for n, s in enumerate(src) if s.startswith('view ') or s.startswith(ANY_P5B))
        open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(src[:i] + add + src[i:]))
        print(f'{os.path.relpath(path, ROOT)}: +{len(add)} lines')


def verify():
    bad = 0
    for path in sorted(set(os.path.normpath(p) for p, *_ in plan()) | set(mine_on_disk())):
        rel = os.path.relpath(path, ROOT).replace('\\', '/')
        old = subprocess.run(['git', '-C', ROOT, 'show', f'{BASE}:{rel}'], capture_output=True, text=True, encoding='utf-8').stdout.split('\n')
        now = open(path, encoding='utf-8').read().split('\n')
        if strip_blocks(now, every=True) != old:
            print(f'FAIL {rel}: outside the P5b blocks the map differs from {BASE[:8]}')
            bad += 1
        else:
            mine = len(now) - len(strip_blocks(now))
            print(f'ok   {rel}: {mine} lines in {TAG} blocks, {len(now) - len(old)} in all P5b blocks; every drawn view as it was')
    return bad


if __name__ == '__main__':
    if '--verify' in sys.argv:
        sys.exit(1 if verify() else 0)
    if '--print' in sys.argv:
        for path, items in by_file().items():
            print('\n'.join(build_blocks(path, items)))
        sys.exit(0)
    write()
