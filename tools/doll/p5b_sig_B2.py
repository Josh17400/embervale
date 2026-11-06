#!/usr/bin/env python3
"""tools/doll/p5b_sig_B2.py (P5b production 2, area B; docs/STYLE_P5B.md 4-5, the owner's 2026-11-06 rulings): the Dune, Steppe and
Marsh elite heads at T5 and T6, drawn the slice's way: every block this script writes is new `when sighelm-<people>,...` views on
the people's own helm family part (the kulah's `veil`, the pointed `plumed`, the reed `warhat`) and their local legend lines, tag
`(P5b SIG B2)`. A `replace` view is placed at the top of the part's views (the first replace view whose condition holds wins); an
overlay that must draw over the part's own generic overlays is placed at the end of the file. No view drawn before P5b2 changes;
fp::SigTier::ownGrowth sets the family's legend growth part (`<family>_legend`) aside at these tiers, so the head is the drawing.

  python tools/doll/p5b_sig_B2.py            write the blocks into rpg/art/doll/maps
  python tools/doll/p5b_sig_B2.py --verify   each touched map is the base's (a22bebc) but for this script's blocks, in order
  python tools/doll/p5b_sig_B2.py --print    print the blocks
  python tools/doll/p5b_sig_B2.py --cost     each block's pool bytes (as pxc.mjs packs them)

Authoring is in head coordinates: x across the 16-column head box (0 at its left, the man's skull 2..13 facing D), y down (the
hair's top, the crown, at 2; the skull's top 3; the brow 9; the eyes 10; the chin 13). The envelope (doll_envelope.h kEnvelope):
T5 eight rows over the crown (y >= -6), T6 ten (y >= -8); the shell two columns past the box (x -2..17), a plume, a crest part or a
flaring part (the war hat) three (x -3..18). Each drawing is a grid of 22 columns, x -3..18, from its first row y0. Facing L is R
mirrored (x -> 15 - x), its lit and shade steps swapped (light from the top left). The alphabet:
  v w x y z   the helm's metal 0-4 (the alloy: the material reads first)
  J N O P W   Metal 2 0-4: the fittings; a cell the regions plane marks R is the T5-T6 gilt (the ladder's trim, Device: gold)
  5 6 7 8 9   Dye 0-4 (the helm maker's dominant)          A B C D E   Dye 2 0-4 (its accent)
  0 1 2 3 4   Plume 0-4 (the crest's and the plume's colour: the dominant)
  F G H I L   Cloth 3 0-3 (undyed: fur, felt)              a b c d     Linen 0-3 (bleached: the heron's white)
  f g h i j   leather 0-4                                  K           ink
  '-' erases what is below (the hair under a brim)
and the regions plane (same grid): R trim (gilt at T5-T6), G a gem (Glow at T5-T6), N a seam (the T6 runes' Glow).
"""
import os
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
MAPS = os.path.join(ROOT, 'rpg', 'art', 'doll', 'maps')
TAG = '(P5b SIG B2)'
BASE = 'a22bebc'   # fix-p5b-prod: the maps before this round

ALPHA = {}
for _i, _c in enumerate('vwxyz'): ALPHA[_c] = ('metal', _i)
for _i, _c in enumerate('JNOPW'): ALPHA[_c] = ('metal2', _i)
for _i, _c in enumerate('56789'): ALPHA[_c] = ('dye', _i)
for _i, _c in enumerate('ABCDE'): ALPHA[_c] = ('dye2', _i)
for _i, _c in enumerate('01234'): ALPHA[_c] = ('plume', _i)
for _i, _c in enumerate('FGHI'): ALPHA[_c] = ('cloth3', _i)
for _i, _c in enumerate('abcd'): ALPHA[_c] = ('linen', _i)
for _i, _c in enumerate('fghij'): ALPHA[_c] = ('leather', _i)
ALPHA['K'] = ('ink', 0)
RELIT = {}
for _ramp in ('vwxyz', 'JNOPW', '56789', 'ABCDE', '01234', 'FGHI', 'abcd', 'fghij'):
    RELIT[_ramp[1]], RELIT[_ramp[3]] = _ramp[3], _ramp[1]
FREE = '!$%&*+,/:;<=>?[]^_`{}~()' + 'klmnopabcdeGHIJKLMNOPVWXYZ' + '0123456789' + 'ABCDEFQRSTUfghijqrstuvwxyz'
X0 = -3   # the grid's first column


def P(*parts):
    return os.path.join(MAPS, *parts)


# ------------------------------------------------------------------------------------------------ authoring
def grid(y0, rows, regs=None):
    """({(y, x): ch}, {(y, x): region}) of a drawing whose first row is y0 (columns from x = -3); '.' and ' ' left out"""
    cs, rs = {}, {}
    for dy, r in enumerate(rows):
        for dx, c in enumerate(r):
            if c not in '. ':
                cs[(y0 + dy, X0 + dx)] = c
    for dy, r in enumerate(regs or []):
        for dx, c in enumerate(r):
            if c not in '. ':
                rs[(y0 + dy, X0 + dx)] = c
    return cs, rs


def mirror(d, relight=True):
    cs, rs = d
    return ({(y, 15 - x): (RELIT.get(c, c) if relight else c) for (y, x), c in cs.items()},
            {(y, 15 - x): c for (y, x), c in rs.items()})


def hair_mask(cs):
    """'-' over the hair above the helm (C3b: no hair over the helm's rim): in each column of the head box and one past it, every
    empty cell from the envelope's top row down to the column's highest shell cell (a plume's strand is no shell: hair may not
    show through the gaps of a crest)"""
    out = dict(cs)
    for x in range(-1, 17):
        shell = [y for (y, xx), c in cs.items() if xx == x and c not in '01234-']
        if not shell:
            continue
        for y in range(-9, min(shell)):
            out.setdefault((y, x), '-')
    return out


def view(f, d, key='rest'):
    """a view from head-coordinate cells: its grid spans them, its origin puts the head anchor on (0, 0)"""
    cs, rs = d
    cs = hair_mask(cs)
    ys = [y for y, _ in cs]
    xs = [x for _, x in cs]
    y0, x0 = min(ys), min(xs)
    w, h = max(xs) - x0 + 1, max(ys) - y0 + 1
    g = [['.'] * w for _ in range(h)]
    for (y, x), c in cs.items():
        g[y - y0][x - x0] = c
    reg = None
    if rs:
        reg = [['.'] * w for _ in range(h)]
        for (y, x), c in rs.items():
            if (y, x) not in cs or cs[(y, x)] == '-':
                raise SystemExit(f'a region at head ({x}, {y}) with no drawn cell')
            reg[y - y0][x - x0] = c
        reg = [''.join(r) for r in reg]
    return (f, key, [''.join(r) for r in g], (-x0, -y0), reg)


class Canvas:
    """head-coordinate cells laid stroke by stroke: put(y, x, s) writes s from (x, y) rightwards ('.' keeps what is there);
    reg(y, x, s) the regions plane the same way; d() the (cells, regions) pair the views take"""
    def __init__(self):
        self.cs, self.rs = {}, {}

    def put(self, y, x, s, reg=None):
        for i, c in enumerate(s):
            if c != '.':
                self.cs[(y, x + i)] = c
                if reg:
                    self.rs[(y, x + i)] = reg
        return self

    def reg(self, y, x, s):
        for i, c in enumerate(s):
            if c != '.':
                self.rs[(y, x + i)] = c
        return self

    def gilt(self):
        """every Metal 2 cell gilt (R): the T5-T6 trim in Device"""
        for k, c in self.cs.items():
            if c in 'JNOPW' and k not in self.rs:
                self.rs[k] = 'R'
        return self

    def d(self):
        return dict(self.cs), dict(self.rs)



def pockets(g):
    """the clear cells of a view grid that no path of clear cells joins to its border (a hole inside the drawing, C3a)"""
    h, w = len(g), max(len(r) for r in g)
    clear = lambda y, x: x >= len(g[y]) or g[y][x] in '.-'   # ('-' erases: clear in the figure)
    seen, st = set(), []
    for y in range(h):
        for x in range(w):
            if (y in (0, h - 1) or x in (0, w - 1)) and clear(y, x):
                seen.add((y, x)); st.append((y, x))
    while st:
        y, x = st.pop()
        for yy, xx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
            if 0 <= yy < h and 0 <= xx < w and (yy, xx) not in seen and clear(yy, xx):
                seen.add((yy, xx)); st.append((yy, xx))
    return [(y, x) for y in range(h) for x in range(w) if clear(y, x) and (y, x) not in seen]


def four(d, r, u=None):
    """the four facings from D and R drawings (U: the back, default D's; L: R mirrored)"""
    return [view('D', d), view('U', u if u is not None else d), view('R', r), view('L', mirror(r))]


def check_envelope(name, views, tier, flare):
    """the drawing inside its tier's head envelope (rows over the crown y 2, columns past the 16 px box)"""
    rows = {5: 8, 6: 10}[tier]
    side = 3 if flare else 2
    for f, k, g, (ox, oy), reg in views:
        for gy, r in enumerate(g):
            for gx, c in enumerate(r):
                if c in '.-':
                    continue
                y, x = gy - oy, gx - ox
                plume = c in '01234'
                if 2 - y > rows:
                    raise SystemExit(f'{name} {f}: ({x}, {y}) {2 - y} rows over the crown (T{tier}: {rows})')
                cap = 3 if plume else 2
                if x < -cap or x > 15 + cap:
                    raise SystemExit(f'{name} {f}: ({x}, {y}) past the head box (T{tier}: {cap})')


# ================================================================================================ the heads
# Light from the top left: a plane's lit step on its left and top, its shade on the right and below; every shape closed by its
# own step 0 (the selout), never ink, so the doll's outline pass rims it.

# ---- DUNE T5: the amir's gilt turban helm (STYLE_P5B 4: "gilt kulah legend, turban, falcon plume"; FLUX dune_full_elite_*,
# dune_helmets_2's gilt turban helm). The kulah of T3-T4 grown: its needle spike and finial in gilt rise out of the steel bowl; a
# fat cream turban wound round the bowl's base (the accent) with the teal wrap crossing it (the dominant), a gilt mirror plate at its
# front holding a gem, and from it the falcon plume (teal, the plume) sweeping up beside the spike; the mail aventail (the helm's
# own metal, ringed) falls from under the turban round the face to the shoulders.
def dune5():
    d = grid(-6, [
        #  idx = x + 3   (x -3 .. 18)
        '..........W...........',   # -6  the needle's tip
        '..........PN......3...',   # -5
        '..........PN.....342..',   # -4  the falcon plume
        '..........PN.....32...',   # -3
        '.........PWON...341...',   # -2  the finial
        '.........NOOJ..341....',   # -1
        '........vyzxwv.31.....',   #  0  the steel bowl
        '.......vyyxxwwv31.....',   #  1
        '.....FII34IHHH31HGF...',   #  2  the turban's top fold, the wrap's end, the plume's root
        '...FII343HHGHPWNHHGF..',   #  3  the clasp
        '..FIH343HHGHHPGNHHHGF.',   #  4  the mirror plate's gem
        '..FH343HHHGHHNONHHHGF.',   #  5
        '..F343HHHGGGHHGHHHGGF.',   #  6
        '...F21GGGGGGGGGGGGGF..',   #  7  the turban's underside
        '...vyyxv........vxwv..',   #  8  the aventail (mail) from under it
        '...vxwxv........vwxv..',   #  9
        '...vyxwv........vxwv..',   # 10
        '...vxwxv........vwxv..',   # 11
        '...vyxwxv......vxwxv..',   # 12
        '...vxwxwxvvvvvvxwxwv..',   # 13  the mail collar under the chin
        '....vwxwxwxwxwxwxwv...',   # 14
        '.....v.v.v.v.v.v.v....',   # 15  its ragged hem
    ], [
        '..........R...........',
        '..........RR..........',
        '..........RR..........',
        '..........RR..........',
        '.........RRRR.........',
        '.........RRRR.........',
        '......................',
        '......................',
        '......................',
    ])
    # the clasp's regions laid by its cells (the gilt ring R, the gem G)
    cs, rs = d
    for (y, x), c in cs.items():
        if c in 'NOPW' and y >= 3:
            rs[(y, x)] = 'R'
    rs[(4, 11)] = 'G'
    cs[(4, 11)] = 'O'
    r = grid(-6, [
        '..........W...........',   # -6
        '..........PN.......3..',   # -5
        '..........PN......342.',   # -4
        '..........PN......32..',   # -3
        '.........PWON....341..',   # -2
        '.........NOOJ...341...',   # -1
        '........vyzxwv..31....',   #  0
        '.......vyyxxwwv.31....',   #  1
        '.....FIIIHHH34HH31F...',   #  2  the wrap rises to the front
        '...FIIHHHH343HHHPWNF..',   #  3  the clasp at the brow
        '..FIIHHH343HHHHHPONF..',   #  4
        '..FHHH343HHHHHHGNONF..',   #  5
        '..FH343HHHHGGHHHHGGF..',   #  6
        '..F21GGGGGGGGGGGGGF...',   #  7
        '..FIGvyxwv............',   #  8  the turban's tail and the aventail behind
        '..FHGvxwxv............',   #  9
        '..FHGvwxwxv...........',   # 10
        '...FGvxwxwv...........',   # 11
        '...FvyxwxwxV..........'.replace('V', 'v'),   # 12
        '...vxwxwxwxwv.........',   # 13
        '...vwxwxwxwxv.........',   # 14
        '....v.v.v.v.v.........',   # 15
    ])
    cs, rs = r
    for (y, x), c in list(cs.items()):
        if c in 'JNOPW':
            rs[(y, x)] = 'R'
    rs[(4, 14)] = 'G'
    u = grid(-6, [
        '..........W...........',   # -6
        '..........PN..........',   # -5
        '..3.......PN..........',   # -4  the plume's tip, seen from behind
        '.243......PN..........',   # -3
        '.143.....PWON.........',   # -2
        '..243....NOOJ.........',   # -1
        '...13...vyzxwv........',   #  0
        '....13.vyyxxwwv.......',   #  1
        '.....FIIIIHHHHHHGF....',   #  2
        '...FIIHHHGHHHGHHHGF...',   #  3  the folds cross at the back
        '..FIHHHGHHHHHGHHHHGF..',   #  4
        '..FHHHGHH4321HHGHHGF..',   #  5  the wrap's end tucked in
        '..FHHGHHHHHHHHHHGHGF..',   #  6
        '...FGGGGGGGGGGGGGGF...',   #  7
        '...vyxwxwxwxwxwxwxv...',   #  8  the aventail over the nape
        '...vxwxwxwxwxwxwxwv...',   #  9
        '...vyxwxwxwxwxwxwxv...',   # 10
        '...vxwxwxwxwxwxwxwv...',   # 11
        '...vyxwxwxwxwxwxwxv...',   # 12
        '...vxwxwxwxwxwxwxwv...',   # 13
        '....vwxwxwxwxwxwxv....',   # 14
        '.....v.v.v.v.v.v.v....',   # 15
    ])
    cs, rs = u
    for (y, x), c in list(cs.items()):
        if c in 'JNOPW':
            rs[(y, x)] = 'R'
    return four(d, r, u)


# ---- DUNE T6: the sultan's jewelled turban (STYLE_P5B 4: "turban-wrapped onion, aventail"; the kulah family's legend: the bowl
# swells into the onion over the turban). The gilt crescent (the alem) crowns a great steel onion dome ribbed in gilt that rises
# out of a fuller turban girt with a gilt band; a great jewel at the turban's front holds a short falcon-feather tuft (the
# sorguc) at the dome's foot; the aventail falls longer, its hem gilt. (At T6 the ladder's living finish lights the gilt.)
def dune6_crown(c, tuft_x, tuft_side):
    c.put(-8, 5, 'P....N').put(-7, 5, 'WP..ON').put(-6, 6, 'NPON')            # the crescent alem, its horns up
    c.put(-5, 7, 'PN')                                                        # its stem
    c.put(-4, 6, 'vyxv').put(-3, 5, 'vyzxwv').put(-2, 4, 'vyzyxxwv').put(-1, 3, 'vyzyyxxxwv').put(0, 3, 'vyyyxxxwwv')
    c.put(1, 4, 'vyyxxwwv')                                                   # the onion dome
    for y in range(-4, 2):
        c.put(y, 7, 'O', 'R')                                                 # its gilt ribs
    for y in range(-2, 1):
        c.put(y, 5, 'P', 'R').put(y, 10, 'N', 'R')
    t = ['.3', '342', '3421', '.21'] if tuft_side > 0 else ['3.', '243', '1243', '12.']
    for i, s in enumerate(t):
        c.put(-1 + i, tuft_x - (len(s) - 1 if tuft_side < 0 else 0), s)     # the sorguc's feathers
    return c


def dune6():
    c = Canvas()
    dune6_crown(c, 10, +1)
    c.put(2, 0, 'FIIIIIIHHHHHHGGF')                                           # the turban, fuller
    c.put(3, -2, 'FIIIH01234HHHHHHGGF')
    c.put(4, -2, 'FIH0123HHPWWONHHHGF')
    c.put(5, -2, 'FH0123HHHNOGGONHHGF')
    c.put(6, -2, 'F0123HHHGHNOONHHGGF')
    c.put(7, -1, 'NOPOPOPOPOPOPOPON', 'R')                                    # its gilt girdle
    c.put(4, 7, 'PWWON', 'R').put(5, 7, 'NOGGON', 'R').put(6, 8, 'NOON', 'R')   # the great jewel
    c.reg(5, 9, 'GG')
    for y in range(8, 13):
        c.put(y, -1, 'vyxwv' if y % 2 == 0 else 'vxwxv').put(y, 13, 'vxwxv' if y % 2 == 0 else 'vwxwv')   # the aventail
    c.put(13, -1, 'vyxwxwxvvvvvvxwxwv').put(14, -1, 'vxwxwxwxwxwxwxwxwv')
    c.put(15, 0, 'NOPOPOPOPOPOPOPN', 'R').put(16, 1, 'N.N.N.N.N.N.N.N', 'R')   # the gilt hem
    d = c.d()
    c = Canvas()
    dune6_crown(c, 12, +1)
    c.put(2, 0, 'FIIIIIHHHHHHHGGF')
    c.put(3, -2, 'FIIIHHH0123HHPWNF')
    c.put(4, -2, 'FIHHH0123HHHHNOGNF')
    c.put(5, -2, 'FHHH0123HHHHHHNONF')
    c.put(6, -2, 'FH0123HHHHGHHHHGGF')
    c.put(7, -1, 'NOPOPOPOPOPOPOPON', 'R')
    c.put(3, 13, 'PWN', 'R').put(4, 13, 'NOGN', 'R').put(5, 14, 'NON', 'R')
    c.reg(4, 15, 'G')
    for y in range(8, 15):
        c.put(y, -2, 'FIG').put(y, 1, 'vyxwxv' if y % 2 == 0 else 'vxwxwv')                                  # the tail, the aventail
    c.put(15, -2, 'FG').put(15, 0, 'NOPOPON', 'R').put(16, 1, 'N.N.N', 'R')
    r = c.d()
    c = Canvas()
    dune6_crown(c, 5, -1)
    c.put(2, 0, 'FIIIIIIHHHHHHGGF').put(3, -2, 'FIIIHHHGHHHHGHHHGGF').put(4, -2, 'FIHHHGHHHHHHGHHHHGF')
    c.put(5, -2, 'FHHHGHH01234HGHHHGF').put(6, -2, 'FHHGHHHHHHHHHHGHHGF').put(7, -1, 'NOPOPOPOPOPOPOPON', 'R')
    for y in range(8, 15):
        c.put(y, -1, ('vyxwxwxwxwxwxwxwxv' if y % 2 == 0 else 'vxwxwxwxwxwxwxwxwv'))
    c.put(15, 0, 'NOPOPOPOPOPOPOPN', 'R').put(16, 1, 'N.N.N.N.N.N.N.N', 'R')
    u = c.d()
    return four(d, r, u)


# ---- STEPPE T5: the khan's tall plumed helm (STYLE_P5B 4: "plumed legend, fur lappets"; FLUX steppe_full_elite_*,
# steppe_helmets_2). The pointed helm of T3-T4 grown tall: a steel cone ribbed in gilt rising to a gilt cup and spire; out of the
# cup the horsetail (the plume: the people's rust) bursts and falls all round the cone to the brow, so the head is one tall point;
# a thick roll of undyed fur round the brow, a gilt brow plate with its gem; the lamellar cheek and neck flaps grown long, laced
# in sky-blue silk (the accent), and the people's fur roll at the neck (STYLE_P5B 5: "a fur roll at the neck") as the helm's
# fur-lined collar. The horsetail's lit strands face the light (the top left: C14's lit edge).
def lamella(c, y0, y1, x, w, lit, glow=False):
    """a lamellar flap: plate rows (the metal, lit on the left or the right) laced in sky blue (Dye 2) between them"""
    for y in range(y0, y1 + 1):
        if (y - y0) % 2 == 0:
            mid = ('y' + 'x' * (w - 3)) if lit else ('x' * (w - 3) + 'w')
        else:
            mid = 'B' * (w - 2)                                                 # (B8: one sky blue, the 48-colour figure)
        c.put(y, x, 'v' + mid + 'v')
        if glow and (y - y0) % 2 == 1:
            c.reg(y, x + 1, 'N' * (w - 2))                                    # (T6) the laces lit by the living finish
    return c


def mane(c, y, x0, x1, lit_side='L', hi=True):
    """one row of the horsetail's strands from x0 to x1: lit strands toward the light (the left), shaded toward the right"""
    n = x1 - x0 + 1
    if n <= 0:
        return c
    lit = '34334333' if hi else '33323333'
    shade = '22121'   # (no step 0: B8's 48 colours a figure)
    s = ''
    for i in range(n):
        f = i / max(1, n - 1)
        if lit_side == 'L':
            s += lit[i % len(lit)] if f < 0.55 else shade[i % len(shade)]
        else:
            s += '2' if f < 0.5 else shade[i % len(shade)]
    if lit_side == 'L' and n >= 2 and x1 >= 13:
        s = s[:-1] + '1'
    c.put(y, x0, s)
    return c


def steppe5_cone(c):
    c.put(-2, 6, 'vyxv').put(-1, 5, 'vyzxwv').put(0, 5, 'vyyxwv').put(1, 4, 'vyyxxwwv').put(2, 4, 'vyyxxwwv')
    c.put(3, 3, 'vyyyxxwwwv').put(4, 2, 'vyyyxxxwwwwv')
    for y in range(-1, 5):
        c.put(y, 7, 'O', 'R')                                                 # the gilt rib
    return c


CONE5 = {-4: (6, 9), -3: (7, 8), -2: (6, 9), -1: (5, 10), 0: (5, 10), 1: (4, 11), 2: (4, 11), 3: (3, 12), 4: (2, 13)}


def fur_collar(c, y, x0, x1, rows=2, flat=False):
    w = x1 - x0 + 1
    c.put(y, x0, (('fHHH' if flat else 'fHII') + ('HHHH' if flat else 'HHGH') * 6)[:w - 1] + 'f')
    if rows >= 3:
        c.put(y + 1, x0, ('fHIH' + 'GHHG' * 6)[:w - 1] + 'f')
    c.put(y + rows - 1, x0 + 1, (('H.HH.' if flat else 'H.GH.') * 6)[:w - 2])
    return c


def steppe5():
    c = Canvas()
    c.put(-6, 6, '2332').put(-5, 5, '233321')                                 # the horsetail's burst
    for y, (l, r) in CONE5.items():
        ox0, ox1 = {-4: (3, 12), -3: (2, 13), -2: (2, 13), -1: (1, 14), 0: (1, 14), 1: (0, 15), 2: (0, 15), 3: (0, 15), 4: (0, 15)}[y]
        mane(c, y, ox0, l - 1, hi=False)
        mane(c, y, r + 1, ox1, 'R', hi=False)
    c.put(-4, 6, 'PPOO').put(-3, 7, 'PO')                                     # the gilt cup and spire
    steppe5_cone(c)
    c.put(5, -1, 'fHHHHHHHHHHHHHHHHf').put(6, -1, 'fHHHHHHHHHHHHHHHHf')        # the fur roll
    c.put(7, -1, 'f.H.f.....H.f.H.f.')
    c.put(6, 6, 'PPOO').put(7, 6, 'OOOO')                                     # the brow plate and its gem
    c.reg(7, 7, 'HH')
    lamella(c, 7, 13, -1, 4, True)
    lamella(c, 7, 13, 13, 4, False)
    fur_collar(c, 14, -1, 16, 2, True)                                        # the fur roll at the neck
    c.gilt()
    d = c.d()
    c = Canvas()
    c.put(-6, 6, '2332').put(-5, 4, '3333321')
    back = {-4: (2, 10, 11), -3: (1, 9, 10), -2: (0, 10, 10), -1: (-1, 11, 11), 0: (-2, 11, 11), 1: (-2, 0, 0), 2: (-2, 0, 0),
            3: (-2, 0, 0), 4: (-2, 0, 0)}
    for y, (l, r) in CONE5.items():
        lx, rx0, rx1 = back[y]
        mane(c, y, lx, l - 1, hi=False)
        if rx0:
            mane(c, y, max(rx0, r + 1), rx1, 'R', hi=False)
    c.put(5, -2, '3').put(6, -2, '3').put(7, -2, '2')                          # the horsetail's ends behind the fur
    c.put(-4, 6, 'PPOO').put(-3, 7, 'PO')
    steppe5_cone(c)
    c.put(5, -1, 'fHHHHHHHHHHHHHHHHf').put(6, -1, 'fHHHHHHHHHHHHHHHHf').put(7, 0, 'H.f.H.f....f.H.f')
    c.put(6, 11, 'PPOO').put(7, 12, 'OOO')                                    # the brow plate at the front
    c.reg(7, 13, 'H')
    lamella(c, 7, 13, -2, 7, True)                                            # the neck flap
    fur_collar(c, 14, -2, 13, 2, True)
    c.gilt()
    r = c.d()
    c = Canvas()
    c.put(-6, 6, '2332').put(-5, 5, '233321')
    for y, (ox0, ox1) in {-4: (3, 12), -3: (2, 13), -2: (2, 13), -1: (1, 14), 0: (1, 14), 1: (0, 15), 2: (0, 15), 3: (0, 15),
                          4: (0, 15)}.items():
        mane(c, y, ox0, ox1, hi=False)                                        # from behind the horsetail hides the cone
    c.put(-4, 6, 'PPOO').put(-3, 7, 'PO')
    c.put(5, -1, 'fHHHHHHHHHHHHHHHHf').put(6, -1, 'fHHHHHHHHHHHHHHHHf').put(7, -1, 'f.H.f.H.f.H.f.H.f.')
    lamella(c, 7, 13, -1, 18, True)                                           # the neck curtain
    fur_collar(c, 14, -1, 16, 2, True)
    c.gilt()
    u = c.d()
    return four(d, r, u)


# ---- STEPPE T6: the khan of khans. The spire grows taller out of a great fur crown (tufted, two columns past the head), the
# horsetail bursts bigger and falls wider round the cone, three gilt ribs run up it, the brow plate's gem is larger, the lamellar
# flaps are five plates wide, and the fur at the neck is a great three-row collar over the shoulders.
def steppe6_cone(c):
    c.put(-3, 6, 'vyxv').put(-2, 6, 'vyxv').put(-1, 5, 'vyzxwv').put(0, 5, 'vyyxwv').put(1, 4, 'vyyxxwwv').put(2, 4, 'vyyxxwwv')
    c.put(3, 3, 'vyyyxxwwwv')
    for y in range(-3, 4):
        c.put(y, 7, 'O', 'R')
    for y in range(0, 4):
        c.put(y, 5, 'P', 'R').put(y, 10, 'N', 'R')
    return c


CONE6 = {-6: (6, 9), -5: (7, 8), -4: (7, 8), -3: (6, 9), -2: (6, 9), -1: (5, 10), 0: (5, 10), 1: (4, 11), 2: (4, 11), 3: (3, 12)}
OUT6 = {-6: (2, 13), -5: (1, 14), -4: (1, 14), -3: (0, 15), -2: (0, 15), -1: (-1, 16), 0: (-1, 16), 1: (-2, 17), 2: (-2, 17),
        3: (-2, 17)}


def steppe6():
    c = Canvas()
    c.put(-8, 6, '2332').put(-7, 4, '23443321')
    for y, (l, r) in CONE6.items():
        mane(c, y, OUT6[y][0], l - 1)
        mane(c, y, r + 1, OUT6[y][1], 'R')
    c.put(-6, 6, 'PWON').put(-5, 7, 'PN').put(-4, 7, 'PN')
    steppe6_cone(c)
    c.put(4, -2, 'fHHII').put(4, 12, 'HHGGf')                               # the fur crown's tufted top (closed: C3a)
    c.put(5, -2, 'fHIIIIIHHHHHHHGGGGf').put(6, -2, 'fHIHHGHHHGHHHGHHGGf').put(7, -2, 'fHGHGHHGHHGHHGHGHGf')
    c.put(7, 5, 'PWWON').put(8, 5, 'NOOON').reg(8, 6, 'GGG')
    lamella(c, 8, 13, -2, 5, True, True)
    lamella(c, 8, 13, 13, 5, False, True)
    fur_collar(c, 14, -2, 17, 3)
    c.gilt()
    d = c.d()
    c = Canvas()
    c.put(-8, 6, '2332').put(-7, 3, '234433321')
    back = {-6: (1, 10, 11), -5: (0, 9, 10), -4: (-1, 9, 10), -3: (-2, 10, 10), -2: (-3, 0, 0), -1: (-3, 0, 0), 0: (-3, 0, 0),
            1: (-3, 0, 0), 2: (-3, 0, 0), 3: (-3, 0, 0)}
    for y, (l, r) in CONE6.items():
        lx, rx0, rx1 = back[y]
        mane(c, y, lx, l - 1)
        if rx0:
            mane(c, y, max(rx0, r + 1), rx1, 'R')
    c.put(4, -3, '34').put(5, -3, '3').put(6, -3, '3').put(7, -3, '2').put(8, -3, '1')
    c.put(-6, 6, 'PWON').put(-5, 7, 'PN').put(-4, 7, 'PN')
    steppe6_cone(c)
    c.put(4, -2, 'fHHII').put(4, 12, 'HGf')
    c.put(5, -2, 'fHIIIIIHHHHHHHGGGGf').put(6, -2, 'fHIHHGHHHGHHHGHHGGf').put(7, -2, 'fHGHGHHGHHGHHGHGHGf')
    c.put(7, 12, 'PWON').put(8, 13, 'NON').reg(8, 14, 'G')
    lamella(c, 8, 13, -2, 8, True, True)
    fur_collar(c, 14, -2, 14, 3)
    c.gilt()
    r = c.d()
    c = Canvas()
    c.put(-8, 6, '2332').put(-7, 4, '23443321')
    for y, (ox0, ox1) in OUT6.items():
        mane(c, y, ox0, ox1)
    c.put(-6, 6, 'PWON').put(-5, 7, 'PN').put(-4, 7, 'PN')
    c.put(4, -2, 'fHHII').put(4, 12, 'HHGGf')                               # the fur crown's tufted top (closed: C3a)
    c.put(5, -2, 'fHIIIIIHHHHHHHGGGGf').put(6, -2, 'fHIHHGHHHGHHHGHHGGf').put(7, -2, 'fHGHGHHGHHGHHGHGHGf')
    lamella(c, 8, 13, -2, 20, True, True)
    fur_collar(c, 14, -2, 17, 3)
    c.gilt()
    u = c.d()
    return four(d, r, u)


# ---- MARSH T5: the bog king's war hat (STYLE_P5B 4: "reed hat legend"; 5: "moss and bone", the motif the reeds; FLUX
# marsh_full_elite_*, marsh_helmets_*). The reed hats of T3-T4 grown and crowned: a steel skullcap on a band of bog hide, out of
# its crown a fan of reeds and bulrushes (the T4 hat's tuft grown: moss stalks, the cattails' brown heads in the leather, the
# outer stalks tipped in bone), the reed brim (the moss: the plume) the widest of any head, drooping at its ends into ragged
# grass that hangs beside the face, bone knotted to its tips; and the reed war-dress's grass collar over the shoulders, bone teeth
# strung on it. On the T3 reed hat's own part (fam.kettle: no generic T5 overlay draws over its face)
def marsh_brim(c, y0, more=False):
    c.put(y0, 0, '0333232323232110')
    c.put(y0 + 1, -2, '033232323232322110')
    c.put(y0 + 2, -3, '0232121212121212211100')
    c.put(y0 + 3, -3, '23210').put(y0 + 3, 14, '01210')
    c.put(y0 + 4, -3, '3210').put(y0 + 4, 15, '0121')
    c.put(y0 + 5, -3, '21').put(y0 + 5, 17, '11')                               # the grass's ends (no pocket: C3a)
    c.put(y0 + 6, -2, 'H').put(y0 + 6, 17, 'G')                                 # bone knotted to the grass (inside the brim's columns)
    if more:
        c.put(y0 + 6, -3, '1').put(y0 + 6, 18, '1').put(y0 + 7, -3, '21').put(y0 + 7, 17, '11').put(y0 + 8, -2, 'H').put(y0 + 8, 17, 'G')
    return c


def reed(c, x, top_y, head='cattail', base_y=-1):
    """one reed stalk (the moss, two wide: its lit and its shaded side) from the crown (x, base_y) up to top_y, under a cattail's
    brown head (the leather, three rows) or a bone point"""
    for y in range(top_y, base_y + 1):
        c.put(y, x, '32')
    if head == 'cattail':
        c.put(top_y - 3, x, 'hg').put(top_y - 2, x, 'gf').put(top_y - 1, x, 'hf').put(top_y - 4, x, '3.')
    else:
        c.put(top_y - 2, x, 'I.').put(top_y - 1, x, 'IH')
    return c


def marsh_reeds(c, full=True):
    """the bulrush crown: T5 five stalks (the outer two bone-tipped); T6 the centre bulrush alone between the antlers"""
    if full:
        reed(c, 1, -1, 'bone', 3)
        reed(c, 13, -1, 'bone', 3)
        reed(c, 4, -1)
        reed(c, 10, -1)
        reed(c, 7, -2)
    else:
        reed(c, 7, -4)                                                        # (T6) the tall centre bulrush between the antlers
    return c


def marsh_cap(c, profile=False):
    if profile:
        c.put(0, 4, 'vyzyxxwv').put(1, 3, 'vyyyxxxwwv').put(2, 3, 'vyyyxxxwwv')
    else:
        c.put(0, 5, 'vyzxwv').put(1, 4, 'vyyxxwwv').put(2, 3, 'vyyyxxxwwv')
    c.put(3, 3, 'fhhhhhhhgf')                                                 # the hide band
    return c


def grass_collar(c, x0, x1, teeth=True):
    w = x1 - x0 + 1
    c.put(14, x0, ('0' + '3323' * 6)[:w - 2] + '10')
    c.put(15, x0, ('2.1.' * 6)[:w])
    if teeth:
        for x in range(x0 + 2, x1 - 1, 3):
            c.put(15, x, 'I')
            c.put(16, x, 'G')
    return c


def marsh5():
    c = Canvas()
    marsh_reeds(c)
    marsh_cap(c)
    marsh_brim(c, 4)
    grass_collar(c, -1, 16)
    d = c.d()
    c = Canvas()
    marsh_reeds(c)
    marsh_cap(c, True)
    marsh_brim(c, 4)
    grass_collar(c, -2, 14)
    r = c.d()
    c = Canvas()
    marsh_reeds(c)
    marsh_cap(c)
    marsh_brim(c, 4)
    for y in range(8, 12):
        c.put(y, 1, '2.1.2.1.2.1.2.1')                                        # the reed fringe down the nape
    grass_collar(c, -1, 16, False)
    u = c.d()
    return four(d, r, u)


# ---- MARSH T6: the bog king crowned in bone (STYLE_P5B 4: "+ bone brow and antler tines"). T5's war hat with a stag's antlers
# rising from the skullcap's sides (bone, three tines a side) about the bulrushes, a brow of bone (a jaw's teeth) laid across the
# forehead under the brim, longer grass at the brim's ends, and a skull hung at the front of the grass collar.
def marsh_antlers(c, left=True, right=True):
    """a stag's antlers of bone from the skullcap's sides: the beam swept out and up, a brow tine out to the brim's columns
    (the T6 figure's room: C12f's T5-to-T6 step), the bez and the crown tines up"""
    if left:
        c.put(-8, 0, 'I').put(-8, 3, 'I').put(-7, 0, 'H').put(-7, 3, 'I').put(-6, 0, 'HG').put(-6, 3, 'H').put(-5, 1, 'HHG')
        c.put(-7, -2, 'I').put(-6, -2, 'I').put(-5, -2, 'HIH').put(-4, -2, 'HIHHH').put(-3, -1, 'HGHH')
        c.put(-2, 0, 'GHG').put(-1, 2, 'HG').put(0, 3, 'HG')
    if right:
        c.put(-8, 15, 'G').put(-8, 12, 'H').put(-7, 15, 'G').put(-7, 12, 'H').put(-6, 14, 'GF').put(-6, 12, 'G').put(-5, 12, 'GGF')
        c.put(-7, 17, 'G').put(-6, 17, 'G').put(-5, 15, 'GGG').put(-4, 13, 'GGGGG').put(-3, 13, 'GGGF')
        c.put(-2, 13, 'GGF').put(-1, 12, 'GF').put(0, 11, 'GF')
    return c


def marsh6():
    c = Canvas()
    marsh_antlers(c)
    marsh_reeds(c, False)
    marsh_cap(c)
    marsh_brim(c, 4, True)
    c.put(7, 3, 'FIHIHIHIHGF')                                                 # the bone brow (a jaw's teeth)
    c.put(8, 4, 'K.K.K.K.K').put(8, 5, 'H.H.H.H.')
    grass_collar(c, -2, 17)
    c.put(15, 6, 'FIHF').put(16, 6, 'IKKH').put(17, 6, 'HIHG')                # the skull at its front
    d = c.d()
    c = Canvas()
    c.put(-8, 6, 'I').put(-7, 6, 'H').put(-7, 9, 'I').put(-6, 6, 'HG').put(-6, 9, 'H').put(-5, 7, 'HHG')     # the near antler
    c.put(-4, 5, 'I').put(-4, 8, 'H').put(-3, 5, 'HG').put(-3, 8, 'H').put(-2, 5, 'HGHG').put(-1, 5, 'GHHG')
    reed(c, 2, -4)
    marsh_cap(c, True)
    marsh_brim(c, 4, True)
    c.put(7, 9, 'IHIHIF').put(8, 10, 'H.H.')
    grass_collar(c, -2, 14)
    c.put(15, 10, 'FIH').put(16, 10, 'IKH').put(17, 10, 'HG')
    r = c.d()
    c = Canvas()
    marsh_antlers(c)
    marsh_reeds(c, False)
    marsh_cap(c)
    marsh_brim(c, 4, True)
    for y in range(8, 13):
        c.put(y, 1, '2.1.2.1.2.1.2.1' if y < 12 else 'H.G.H.G.H.G.H.G')
    grass_collar(c, -2, 17, False)
    u = c.d()
    return four(d, r, u)


# the crest sockets (`mark crown`, head coordinates by facing): the Steppe T6 spire's top, where the library's cast finial (crest
# finial: on a culture alloy's kit an empty view, alloyshape) would sit on any other piece the khan's helm draws on
CROWN = {'steppe-khan6': {'D': (8, -4), 'U': (8, -4), 'R': (8, -4), 'L': (7, -4)}}


def plan():
    """(path, tag, description, views, condition, replace, place)"""
    return [
        (P('headwear', 'helm', 'veil.pxm'), 'dune-amir5', 'the Dune T5 amir: the gilt kulah under a cream turban and teal wrap, '
         'the mirror plate, the falcon plume, the mail aventail', dune5(), 'sighelm-dune,helm.t5,!helm.t6', True, 'top'),
        (P('headwear', 'helm', 'veil.pxm'), 'dune-sultan6', 'the Dune T6 sultan: the gilt crescent, the onion dome, the fuller turban '
         'girt in gilt, the great jewel and its feather tuft, the gilt hem of the aventail', dune6(), 'sighelm-dune,helm.t6', True, 'top'),
        (P('headwear', 'helm', 'plumed.pxm'), 'steppe-khan5', 'the Steppe T5 khan: the tall gilt-ribbed cone, the horsetail bursting '
         'from its cup, the fur roll, the brow plate, the long lamellar flaps laced in sky silk', steppe5(),
         'sighelm-steppe,helm.t5,!helm.t6', True, 'top'),
        (P('headwear', 'helm', 'plumed.pxm'), 'steppe-khan6', 'the Steppe T6 khan of khans: the taller spire, the great fur crown, '
         'the horsetail to the shoulders, three gilt ribs, the flaps to the collarbone', steppe6(), 'sighelm-steppe,helm.t6', True, 'top'),
        (P('headwear', 'helm', 'kettle.pxm'), 'marsh-bogking5', 'the Marsh T5 bog king: the steel skullcap on bog hide, the reeds and '
         'bulrushes, the wide drooping reed brim, the grass collar strung with bone', marsh5(), 'sighelm-marsh,helm.t5,!helm.t6', True, 'top'),
        (P('headwear', 'helm', 'kettle.pxm'), 'marsh-bogking6', 'the Marsh T6 bog king crowned in bone: the antlers, the bone '
         'brow, the skull on the grass collar', marsh6(), 'sighelm-marsh,helm.t6', True, 'top'),
    ]


# ------------------------------------------------------------------------------------------------ the pxm plumbing
def own_strip(lines):
    out, skip = [], False
    for s in lines:
        if s.startswith(f'# {TAG} begin '):
            skip = True
            continue
        if s.startswith(f'# {TAG} end '):
            skip = False
            continue
        if not skip:
            out.append(s)
    return out


def used_chars(lines):
    used = set()
    for s in lines:
        if s.startswith('|'):
            used.update(s[1:])
        elif s.startswith('legend '):
            used.add(s.split()[1])
    for lg in ('person', 'woman'):
        for s in open(os.path.join(MAPS, 'legend', lg + '.legend'), encoding='utf-8'):
            if s.startswith('legend '):
                used.add(s.split()[1])
    return used


def known_legend(lines):
    """{(slot, step): char} the part already draws with: person.legend's (a part that `use person`s it) and its own legend lines
    (which win over the person legend's)"""
    known, local = {}, {}
    if any(s.strip() == 'use person' for s in lines):
        for s in open(os.path.join(MAPS, 'legend', 'person.legend'), encoding='utf-8'):
            if s.startswith('legend '):
                c, slot, step = s.split()[1:4]
                local[c] = (slot, int(step))
    for s in lines:
        if s.startswith('legend '):
            c, slot, step = s.split()[1:4]
            local[c] = (slot, int(step))
    for c, k in local.items():
        known.setdefault(k, c)
    return known


def block(tag, desc, views, cond, replace, used, known):
    need = sorted({c for v in views for r in v[2] for c in r if c not in '.-'})
    pool = [c for c in dict.fromkeys(FREE) if c not in used]
    assign, new = {}, []
    for c in need:
        if c not in ALPHA:
            raise SystemExit(f'{tag}: {c!r} is not in the authoring alphabet')
        if ALPHA[c] in known:
            assign[c] = known[ALPHA[c]]
            continue
        if not pool:
            raise SystemExit(f'{tag}: no free legend character left')
        assign[c] = pool.pop(0)
        known[ALPHA[c]] = assign[c]
        new.append(c)
    lines = [f'# {TAG} begin {tag}: {desc}']
    lines += [f'legend {assign[c]} {ALPHA[c][0]} {ALPHA[c][1]}' for c in new]
    for f, k, g, org, reg in views:
        lines.append(f'view {f} key {k} origin {org[0]} {org[1]} when {cond}{" replace" if replace else ""}')
        if tag in CROWN:   # the crest's socket (a crest part anchored at `crown`), from head coordinates
            cx, cy = CROWN[tag][f]
            lines.append(f'mark crown {cx + org[0]} {cy + org[1]}')
        lines += ['|' + ''.join(assign.get(c, c) for c in r) for r in g]
        if reg:
            lines.append('regions')
            lines += ['|' + r for r in reg]
    lines.append(f'# {TAG} end {tag}')
    return lines, {assign[c] for c in new}


def build(path, text):
    """the map's new lines: the file's without this script's blocks, this round's `top` blocks before the first view (or the first
    P5b block already there) and its `end` blocks after the last line"""
    src = own_strip(text.replace('\r\n', '\n').split('\n'))
    while src and src[-1] == '':
        src.pop()
    used = used_chars(src)
    known = known_legend(src)
    top, end = [], []
    for p, tag, desc, views, cond, rep, place in plan():
        if os.path.normcase(p) != os.path.normcase(path):
            continue
        lines, chars = block(tag, desc, views, cond, rep, used, known)
        used |= chars
        (top if place == 'top' else end).extend(lines)
    i = next(n for n, s in enumerate(src) if s.startswith('view ') or s.startswith('# (P5b SIG'))
    return src[:i] + top + src[i:] + end + [''], top + end


def touched():
    out = []
    for p, *_ in plan():
        if p not in out:
            out.append(p)
    return out


def base_text(path):
    rel = os.path.relpath(path, ROOT).replace('\\', '/')
    r = subprocess.run(['git', '-C', ROOT, 'show', f'{BASE}:{rel}'], capture_output=True, text=True, encoding='utf-8')
    if r.returncode:
        raise SystemExit(f'git show {BASE}:{rel} failed')
    return r.stdout


def write():
    for path in touched():
        new, add = build(path, base_text(path))
        open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(new))
        print(f'{os.path.relpath(path, ROOT)}: +{len(add)} lines')


def verify():
    bad = 0
    for path in touched():
        rel = os.path.relpath(path, ROOT).replace('\\', '/')
        old = own_strip(base_text(path).replace('\r\n', '\n').split('\n'))
        now = own_strip(open(path, encoding='utf-8').read().replace('\r\n', '\n').split('\n'))
        while old and old[-1] == '':
            old.pop()
        while now and now[-1] == '':
            now.pop()
        if old != now:
            print(f'FAIL {rel}: outside this script\'s blocks the map differs from {BASE}')
            bad += 1
        else:
            print(f'ok   {rel}: only {TAG} blocks added')
    return bad


def rle_len(cs):
    n, i = 0, 0
    while i < len(cs):
        if cs[i] == 0:
            k = 0
            while i < len(cs) and cs[i] == 0 and k < 128:
                k += 1
                i += 1
            n += 1
        else:
            j = i
            while j < len(cs) and j - i < 128 and cs[j] != 0:
                j += 1
            n += 1 + (j - i)
            i = j
    return n


def cost():
    tot = 0
    for p, tag, desc, views, cond, rep, place in plan():
        planes, b = set(), 0
        for f, k, g, org, reg in views:
            w = max(len(r) for r in g)
            key = tuple(r.ljust(w, '.') for r in g)
            b += 35
            if key not in planes:
                planes.add(key)
                b += 2 * rle_len([0 if c == '.' else 1 for r in key for c in r])
        b += 2 + len(cond)
        tot += b
        print(f'{tag:28s} {len(views)} views  ~{b} B')
    print(f'{"total":28s} ~{tot} B')


if __name__ == '__main__':
    for p, tag, desc, views, cond, rep, place in plan():
        tier = 6 if 'helm.t6' in cond and '!helm.t6' not in cond else 5
        check_envelope(tag, views, tier, 'warhat' in p or 'kettle' in p)
        for f, k, g, org, reg in views:
            # (a pocket over the head, the face or the body beneath, x 0..15 from the crown down, is no hole: the figure fills it)
            holes = [(y, x) for y, x in pockets(g) if not (0 <= x - org[0] <= 15 and y - org[1] >= 2)]
            if holes:
                raise SystemExit(f'{tag} {f}: a clear pocket inside the drawing at head ' +
                                 ', '.join(f'({x - org[0]}, {y - org[1]})' for y, x in holes[:6]))
    if '--verify' in sys.argv:
        sys.exit(1 if verify() else 0)
    if '--cost' in sys.argv:
        cost()
        sys.exit(0)
    if '--print' in sys.argv:
        for path in touched():
            print('\n'.join(build(path, base_text(path))[1]))
        sys.exit(0)
    write()
