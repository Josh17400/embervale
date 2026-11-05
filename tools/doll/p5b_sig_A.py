#!/usr/bin/env python3
"""tools/doll/p5b_sig_A.py (P5b production, area A, docs/STYLE_P5B.md 4-5): area A's people heads as ADDED views.

Area A: the Fjordfolk T4 fix (the owner, 2026-11-05: the borrowed spiked cone "overly pointy and his eyes are too far
apart"), the Heartland and the Highland at T1-T4, and the slice peoples' planned T2 and T4 heads (Jade, Imperial).

As tools/doll/p5b_sig.py (the slice's script, whose blocks this one never touches): every block written here is new
`when sighelm-<people>,...` views (the resolver's per-slot people flag) and the local legend characters they use, inserted
before a part's first view or first P5b block (a `replace` view wins in file order). No drawn view's cells change.
Re-running replaces this script's own blocks (between its begin and end comments) only.

  python tools/doll/p5b_sig_A.py            write the blocks into rpg/art/doll/maps
  python tools/doll/p5b_sig_A.py --verify   every original line of each touched map is still there, in order, and every
                                            added line sits inside a P5b block (this script's or the slice's)
  python tools/doll/p5b_sig_A.py --print    print the blocks

The heads (hand-placed; the dome shading computed from each hand-drawn mask so every facing keeps the top-left light):
  Fjordfolk T4  the Viking spectacle helm (Gjermundbu): headwear/helm/spangen at T4, a low rounded cap on a ridge band
                with a knob, a riveted brow band, a spectacle guard round each eye (the eyes at the face's own spacing, as
                under the T3 nasal) joined by a short nasal, and a mail aventail at the sides and under the chin. Between
                the T3 nasal spangenhelm (pointed) and the T5 gilt horns
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from p5b_sig import paint, read_views  # noqa: E402  (the slice's helpers; its blocks are never written here)

ROOT = os.path.join(HERE, '..', '..')
MAPS = os.path.join(ROOT, 'rpg', 'art', 'doll', 'maps')
BASE = '37331730328157e70cdd6b26c057fc63f5929a7c'   # STYLE_P5B's commit: the maps before P5b
TAG = '(P5b SIG-A)'
ANY_P5B = '# (P5b SIG'   # this script's blocks and the slice's (and any other P5b area's: '(P5b SIG-<area>)')


# ------------------------------------------------------------------------------------------------ helpers
def shade_dome(mask, out='v', s1='w', s2='x', s3='y', s4='z', solid='#'):
    """a dome lit from the top left (the slice's shade_dome, a step lighter so a dark alloy still reads round): outline where
    a shell cell has a 4-neighbour off the shell; inside, by its place across its row's run: the left third s3 (the first two
    lit rows' leftmost cells s4), to three quarters s2, the rest s1"""
    h, w = len(mask), max(len(r) for r in mask)
    is_s = lambda x, y: 0 <= y < h and 0 <= x < len(mask[y]) and mask[y][x] == solid
    rows, lit_rows = [], 0
    for y in range(h):
        xs = [x for x in range(len(mask[y])) if is_s(x, y)]
        lo, hi = (min(xs), max(xs)) if xs else (0, 0)
        row, first = [], True
        for x in range(len(mask[y])):
            if not is_s(x, y):
                row.append(mask[y][x])
                continue
            if not (is_s(x - 1, y) and is_s(x + 1, y) and is_s(x, y - 1) and is_s(x, y + 1)):
                row.append(out)
                continue
            t = (x - lo) / max(1, hi - lo)
            if t < 0.34:
                row.append(s4 if first and lit_rows < 2 else s3)
                first = False
            elif t < 0.75:
                row.append(s2)
            else:
                row.append(s1)
        if any(c in (s3, s4) for c in row):
            lit_rows += 1
        rows.append(''.join(row))
    return rows


def box_erase(rows, box):
    """(the slice's pool saving) an erase only where hair can lie: inside the head box's columns (box: (first, last)) and
    never on the grid's top row; every other '-' becomes '.' (kept)"""
    return [''.join('.' if c == '-' and (y == 0 or x < box[0] or x > box[1]) else c for x, c in enumerate(r))
            for y, r in enumerate(rows)]


def regions_of(mask, table):
    """a regions plane from a mask: each mask character in table becomes its region letter, the rest '.'"""
    return [''.join(table.get(c, '.') for c in r) for r in mask]


def mirror(rows):
    return [r[::-1] for r in rows]


def render(mask, metal=('v', 'w', 'x', 'y', 'z'), m2=('S', 'N', 'O', 'P', 'T'), centre=None, rivets=()):
    """a head mask to legend characters. Mask cells:
      '#' the shell (Metal, shaded as a dome: shade_dome, the light top-left)   'r' a ridge band (Metal 2, lit left column)
      'b' the brow band (Metal 2: lit left of the centre, shaded right; rivets at the given columns)
      'k' a knob (the outline)   'n' the nasal (Metal 2, lit left column)   's' the spectacle (Metal: lit left of the face's
      centre column, shaded right)   'm' mail (Metal 2 / 1 in offset courses)   'v' the outline   '-' erase   '.' keep"""
    h, w = len(mask), max(len(r) for r in mask)
    mask = [r.ljust(w, '.') for r in mask]
    centre = w / 2 if centre is None else centre
    shell = [''.join('#' if c in '#rb' else '.' for c in r) for r in mask]   # the band rides the shell: the dome's last row lit
    lit = shade_dome(shell, out=metal[0], s1=metal[1], s2=metal[2], s3=metal[3], s4=metal[4])
    out = []
    for y, r in enumerate(mask):
        row = []
        rcols = [x for x, c in enumerate(r) if c == 'r']
        ncols = [x for x, c in enumerate(r) if c == 'n']
        for x, c in enumerate(r):
            if c == '#':
                row.append(lit[y][x])
            elif c == 'r':
                row.append(m2[3] if x == rcols[0] else m2[1])
            elif c == 'n':
                row.append(m2[3] if x == ncols[0] else m2[1])
            elif c == 'b':
                row.append(m2[4] if x in rivets else (m2[2] if x < centre else m2[1]))
            elif c == 'k':
                row.append(metal[0])
            elif c == 's':
                row.append(metal[3] if x < centre else metal[2])
            elif c in 'mM':
                row.append(metal[2] if (x + y) % 2 == 0 else metal[1])
            else:
                row.append(c)
        out.append(''.join(row))
    return out


def relight_mirror(mask):
    """facing L from facing R's mask: mirrored, then shaded afresh (the light stays top-left)"""
    return mirror(mask)


# ------------------------------------------------------------------------------------------------ Fjordfolk T4: the spectacle helm
# headwear/helm/spangen's grid (D / U 20 columns, origin 2 1; R / L 21 columns, origin 3 1 / 2 1): the face's eyes are rows
# 11-12 (lashes row 10), the left eye columns 6-7 and the right 12-13 facing D (the T3 face, measured on the composed figure:
# irises at 7 and 12, as under the T3 nasal); facing R the eye at 14-15, the nose at 17-18. The cap is lower than the T3
# spangenhelm's point (a rounded crown, its knob two rows under the cone's tip) and wider; the spectacle hangs from the rim.
GJ_D = [
    '--------------------',
    '---------kk---------',
    '-------##rr##-------',
    '-----####rr####-----',
    '----#####rr#####----',
    '---######rr######---',
    '---######rr######---',
    '---######rr######---',
    '---vbbbbbbbbbbbbv---',
    '--vvvvvvvvvvvvvvvv--',
    '..vmvw..vnnv..wvmv..',
    '..vmvw..vnnv..wvmv..',
    '..vmvw..vnnv..wvmv..',
    '..vmvwwwv..vwwwvmv..',
    '..vmmv........vmmv..',
    '.vMMMMvv....vvMMMMv.',
    '..vvvvvvvvvvvvvvvv..',
]
GJ_U = [
    '--------------------',
    '---------kk---------',
    '-------##rr##-------',
    '-----####rr####-----',
    '----#####rr#####----',
    '---######rr######---',
    '---######rr######---',
    '---######rr######---',
    '---vbbbbbbbbbbbbv---',
    '--vvvvvvvvvvvvvvvv--',
    '..vmmmmmmmmmmmmmmv..',
    '..vmmmmmmmmmmmmmmv..',
    '..vmmmmmmmmmmmmmmv..',
    '..vmmmmmmmmmmmmmmv..',
    '..vmmmmmmmmmmmmmmv..',
    '.vMMMMMMMMMMMMMMMMv.',
    '..vvvvvvvvvvvvvvvv..',
]
GJ_R = [
    '---------------------',
    '----------kk---------',
    '--------##rr##-------',
    '------####rr####-----',
    '-----#####rr#####----',
    '----######rr######---',
    '----######rr######---',
    '----######rr######---',
    '---vbbbbbbbbbbbbbbbv-',
    '---vvvvvvvvvvvvvvvvv-',
    '..vmmmmmmmv..w..w.nv.',
    '..vmmmmmmmv..w..w.nv.',
    '..vmmmmmmmv..w..w.nv.',
    '..vmmmmmmmv..wwww....',
    '..vmmmmmmmv..........',
    '..vMMMMMMMv..........',
    '...vvvvvvv...........',
]
GJ_RIVETS_D = (5, 14)
GJ_RIVETS_R = (6, 15)


def gjermundbu():
    legend = ['legend S metal2 0', 'legend T metal2 4']
    reg = {'b': 'R', 'M': 'D'}   # the brow band the helm's trim; the aventail's hem bound in the people's woad
    lm = relight_mirror(GJ_R)
    # (the hair under the cap erased inside the head box, a column wider at the nape facing R / L: a tousled tuft showed there)
    d = box_erase(render(GJ_D, centre=10, rivets=GJ_RIVETS_D), (2, 17))
    u = box_erase(render(GJ_U, centre=10, rivets=GJ_RIVETS_D), (2, 17))
    r = box_erase(render(GJ_R, centre=16, rivets=GJ_RIVETS_R), (2, 18))
    l = box_erase(render(lm, centre=5, rivets=tuple(20 - c for c in GJ_RIVETS_R)), (2, 18))
    return legend, [('D', 'rest', d, regions_of(GJ_D, reg)), ('U', 'rest', u, regions_of(GJ_U, reg)),
                    ('R', 'rest', r, regions_of(GJ_R, reg)), ('L', 'rest', l, regions_of(lm, reg)),
                    ('L', 'lie', l, regions_of(lm, reg))]


# ------------------------------------------------------------------------------------------------ Highland T1: the bonnet
# headwear/cloth/cap's grid (D / U 18 columns, origin 1 1; R / L 20 columns, origin 3 1 / 1 1): the Scots bonnet, a flat
# soft crown wider than the head, tilted to show its lit top, on a diced band round the brow, a toorie on the crown. The crown
# in the people's dominant (Plume: tartan green), the band diced green and saffron, the toorie saffron (Head: the accent).
# Inside the cap's own rows (the T1 envelope: the soft head's 2-row rise), its crown a column over the cap's each side
BONNET_D = [   # (round 2: lower, a column narrower each side: the T1 figure under T5's 1.55, the crown a row over the hair's top)
    '..................',
    '..................',
    '........tt........',
    '....##########....',
    '..##############..',
    '.################.',
    '..uuuuuuuuuuuuuu..',
    '..dddddddddddddd..',
    '..oooooooooooooo..',
]
BONNET_R = [
    '....................',
    '....................',
    '.........tt.........',
    '.....##########.....',
    '...##############...',
    '..################..',
    '...uuuuuuuuuuuuuu...',
    '...dddddddddddddd...',
    '...oooooooooooooo...',
]
# T2: the clansman's badge on the bonnet's left side (the figure's left: the viewer's right facing D, the near side facing L,
# behind the crown facing R): a saffron brooch on the band and an eagle feather rising from it, two rows over the crown
BADGE_D = {(0, 15): 'F', (1, 14): 'V', (2, 13): 'F', (2, 14): 'J', (2, 15): 'F', (3, 13): 'J', (3, 14): 'H', (4, 13): 'J', (5, 13): 'I',
           (7, 13): 'U', (7, 14): 'S'}
BADGE_U = {(1, 3): 'V', (2, 2): 'F', (2, 3): 'J', (2, 4): 'F', (3, 3): 'J', (3, 4): 'H', (4, 4): 'J', (5, 4): 'I',
           (7, 3): 'U', (7, 4): 'S'}
BADGE_R = {(1, 5): 'V', (2, 4): 'F', (2, 5): 'J', (2, 6): 'F'}
BADGE_L = {(1, 6): 'V', (2, 5): 'F', (2, 6): 'J', (2, 7): 'F', (3, 6): 'J', (3, 7): 'H', (4, 6): 'J', (5, 6): 'I',
           (7, 5): 'U', (7, 6): 'S', (7, 7): 'R'}
# and the bonnet's two ribbon tails hanging from the band at the nape (the dominant, its shade): behind the head facing R / L,
# down the back facing U (hidden facing D)
TAILS_R = {(8, 3): 'H', (9, 3): 'H', (10, 3): 'I', (11, 3): 'H', (12, 3): 'H', (13, 3): 'F', (9, 2): 'F', (10, 2): 'H',
           (11, 2): 'H', (12, 2): 'H', (13, 2): 'H', (14, 2): 'F'}
TAILS_U = {(9, 7): 'H', (10, 7): 'I', (11, 7): 'H', (12, 7): 'F', (9, 10): 'H', (10, 10): 'H', (11, 10): 'F'}
BADGE_R.update(TAILS_R)
BADGE_L.update({(y, 19 - x): c for (y, x), c in TAILS_R.items()})
BADGE_U.update(TAILS_U)


DYE = ('F', 'H', 'I', 'J', 'V')    # local legend: the dominant (Plume 0-4: a signed soft head's crest slot, the dominant;
                                   # a guise head's Dye cells take no tier pair: they drew the figure's accent)
HEADC = ('Q', 'R', 'S', 'T', 'U')  # the cap's own legend: Head 0-4 (the soft head's accent)


def bonnet_render(mask):
    """the bonnet's mask to legend characters: '#' the crown (lit top left, its rim the outline), 'u' its shaded underside,
    'd' the diced band (green and saffron, the light on its left), 'o' the band's lower edge, 't' the toorie"""
    shell = [''.join('#' if c in '#tudo' else '.' for c in r) for r in mask]   # (the underside and band under it: the crown's
    lit = shade_dome(shell, out=DYE[0], s1=DYE[1], s2=DYE[2], s3=DYE[3], s4=DYE[4])   # widest row lit, never an outline)
    out = []
    for y, r in enumerate(mask):
        row = []
        ts = [x for x, c in enumerate(r) if c == 't']
        ds = [x for x, c in enumerate(r) if c == 'd']
        for x, c in enumerate(r):
            if c == '#':
                row.append(lit[y][x])
            elif c == 't':   # the toorie: a saffron knot, lit on its left
                row.append((HEADC[4] if x == ts[0] else HEADC[2]) if y == 0 else (HEADC[3] if x == ts[0] else HEADC[1]))
            elif c == 'u':
                row.append(DYE[0] if x in (r.index('u'), r.rindex('u')) else DYE[1])
            elif c == 'd':
                i = x - ds[0]
                if i == 0 or x == ds[-1]:
                    row.append(DYE[0])
                else:
                    row.append((HEADC[3] if x < len(r) / 2 else HEADC[2]) if i % 2 else (DYE[2] if x < len(r) / 2 else DYE[1]))
            elif c == 'o':
                row.append(DYE[0])
            else:
                row.append(c)
        out.append(''.join(row))
    return out


def bonnet():
    legend = [f'legend {c} plume {k}' for k, c in enumerate(DYE)]
    d = bonnet_render(BONNET_D)
    r = bonnet_render(BONNET_R)
    l = bonnet_render(mirror(BONNET_R))
    return legend, [('D', 'rest', d, None), ('U', 'rest', d, None), ('R', 'rest', r, None), ('L', 'rest', l, None),
                    ('L', 'lie', l, None)]


def badge():
    """the T2 overlay: the brooch, the feather and the ribbon tails ('.' keeps the bonnet). Its grid starts two rows over the
    bonnet's (origin y + 2): the eagle feather rises two rows more (T2's ornament rows, 29.3)"""
    TIP_D = {(-2, 17): 'F', (-1, 16): 'V'}
    TIP_L = {(-2, 8): 'F', (-1, 7): 'V'}
    TIP_R = {(-1, 6): 'F', (0, 6): 'V'}
    def grid(cells, w):
        rows = range(-2, 15)
        return [''.join(cells.get((y, x), '.') for x in range(w)) for y in rows]
    up = lambda a, b: {**a, **b}
    return [], [('D', 'rest', grid(up(BADGE_D, TIP_D), 18), None, (1, 3)), ('U', 'rest', grid(BADGE_U, 18), None, (1, 3)),
                ('R', 'rest', grid(up(BADGE_R, TIP_R), 20), None, (3, 3)), ('L', 'rest', grid(up(BADGE_L, TIP_L), 20), None, (1, 3)),
                ('L', 'lie', grid(up(BADGE_L, TIP_L), 20), None, (1, 3))]


# ------------------------------------------------------------------------------------------------ Highland T2: the plaid hood
# an overlay on headwear/cloth/hood's own views (the saffron hood: Head, the accent): the tartan's sett in the people's green
# (Plume, the dominant) as 1 px lines every 5 columns and 6 rows across the hood's cloth, a step under the cloth they cross
# and a step deeper where two lines meet, so the hood reads checked at 1x (the one pattern no other people wears); the
# outline, the face opening and the cowl's edge as drawn
def plaid_overlay(rows, xo, yo):
    """rows: a hood view's grid (Q R S T U = Head 0-4); xo / yo: the sett's phase. Returns the overlay grid ('.' keeps)"""
    step = {'R': 1, 'S': 1, 'T': 2, 'U': 3}
    out = []
    for y, r in enumerate(rows):
        row = []
        for x, c in enumerate(r):
            v, h = (x + xo) % 5 == 0, (y + yo) % 6 == 0
            if c in step and (v or h):
                k = step[c] - (1 if v and h else 0)
                row.append(DYE[max(1, k)])
            else:
                row.append('.')
        out.append(''.join(row))
    return out


def plaid_hood(path):
    base = read_views(path)
    legend = [f'legend {c} plume {k}' for k, c in enumerate(DYE)]
    views = []
    for (f, k), rows in base.items():
        if k not in ('rest', 'lie'):
            continue
        xo = 2 if f in 'DU' else 1 if f == 'R' else 3   # the sett centred on the face (D) and on the crown's seam (R / L)
        views.append((f, k, plaid_overlay(rows, xo, 3), None))
    return legend, views


# ------------------------------------------------------------------------------------------------ Highland T3: the fur-band kettle
# (E) headwear/helm/kettle's own Highland views (the T3 kettle under its fur band and a mail aventail, `furtrim,helm.t3...`; the
# fur band and the feather, `furtrim`), drawn today only where a culture's arms roll the fur trim: copied cell for cell under
# the signed Highland helm's flag. The pool stores an identical plane once (pxc's plane sharing), so a copy costs its view
# record, never its pixels
def view_blocks(path, when):
    """[(header line, body lines)] of the views whose `when` is exactly `when` (with or without replace); the body keeps its
    mark, grid, regions and build lines as written"""
    out, cur = [], None
    for line in open(path, encoding='utf-8'):
        s = line.rstrip(chr(10))
        if s.startswith('view '):
            w = s.split(' when ', 1)[1].split()[0] if ' when ' in s else None
            cur = (s, []) if w == when else None
            if cur:
                out.append(cur)
            continue
        if cur is None:
            continue
        if not s.strip() or s.startswith('#'):
            cur = None
            continue
        cur[1].append(s)
    return out


def copy_views(path, when, new_when, replace):
    lines = []
    for hdr, body in view_blocks(path, when):
        head = hdr.split(' when ', 1)[0]
        lines.append(f'{head} when {new_when}{" replace" if replace else ""}')
        lines += body
    return lines


# ------------------------------------------------------------------------------------------------ Highland T4: the kettle grown
# an overlay on the T3 kettle (shared with T4) at T4: the eagle feather two rows taller and the aventail grown into a mail
# collar under the chin (facing D / U) and a curtain at the nape (facing R / L): the gallowglass's mail. Plume Q-U = 0-4 (the
# kettle's own legend: the dominant), mail v w x as the kettle's. Grids start two rows over the base's (origin y + 2)
# T4: the fur band as T3's, the eagle feather moved to the crown's other side and grown two rows over T3's (so the head's
# outline parts from T3's by the feather's move, never by bulk: the waiting T5's area and head cap T4's), a finial on the
# knob. Rows from base row -3 (origin y + 3); fur h i g (leather), plume Q-U
KET4_D = [   # (the eagle feather a broad vane facing D: its lit edge on the left, the quill down its right)
    '.Q..................',
    'QUQ......vv.........',
    'QUTQ....vzyv........',
    '.QUTQ...............',
    '.QUTQ...............',
    '..QUTQ..............',
    '..QTSQ..............',
    '...QTQ..............',
    '...QSQ..............',
    '....QQ..............',
    '....ghihghihghih....',
]
KET4_R = [   # facing R the feather rides the near side, before the crown's top
    '...............Q.....',
    '..........vv..QUQ....',
    '.........vzyvQUQ.....',
    '.............QTQ.....',
    '............QUQ......',
    '............QTQ......',
    '...........QUQ.......',
    '...........QTQ.......',
    '..........QUQ........',
    '.....................',
    '.....ghihghihghih....',
]


def mail_mirror(rows):
    return [r[::-1].replace('x', '#').replace('w', 'x').replace('#', 'w') for r in rows]


def kettle4():
    return [], [('D', 'rest', KET4_D, None, (2, 4)), ('U', 'rest', mail_mirror(KET4_D), None, (2, 4)),
                ('R', 'rest', KET4_R, None, (3, 4)), ('L', 'rest', mail_mirror(KET4_R), None, (2, 4))]


# ------------------------------------------------------------------------------------------------ Heartland T1: the chapel
# (O) headwear/helm/kettle's own base views as the levy's chapel de fer in boiled leather: the crown and brim in the leather
# (f g h i j = Leather 0-4, cell for cell from the steel's steps), the hatband in the people's heraldic red (Plume: the
# dominant), the brim a column wider each side (the chapel's broad brim; T1's flare rule, 29.3). U shares D's plane, L lie L's
LEATHER = {'v': 'f', 'w': 'g', 'x': 'h', 'y': 'i', 'z': 'j'}


def chapel_view(rows, box):
    g = [list(r) for r in rows]
    h, w = len(g), len(g[0])
    band, brim = 7, 8
    for y in range(h):
        for x in range(w):
            if g[y][x] in LEATHER:
                g[y][x] = LEATHER[g[y][x]]
    xs = [x for x in range(w) if rows[band][x] in 'yzxw']
    for x in xs:   # the hatband: lit left of its middle, shaded right, its last cell the shade
        g[band][x] = 'T' if x < (xs[0] + xs[-1]) / 2 else ('R' if x == xs[-1] else 'S')
    b = [x for x in range(w) if rows[brim][x] != '.' and rows[brim][x] != '-']
    lo, hi = b[0], b[-1]
    if lo > 0:
        g[brim][lo - 1] = 'f'
        g[brim][lo] = 'j'
    if hi < w - 1:
        g[brim][hi + 1] = 'f'
        g[brim][hi] = 'g'
    u = [x for x in range(w) if rows[brim + 1][x] not in '.-']
    if u and u[0] > 0:
        g[brim + 1][u[0] - 1] = 'f'
        g[brim + 1][u[0]] = 'g'
    if u and u[-1] < w - 1:
        g[brim + 1][u[-1] + 1] = 'f'
        g[brim + 1][u[-1]] = 'g'
    return box_erase([''.join(r) for r in g], box)


def chapel(path):
    """(the hatband carries region D, the dye seam: so the view is never `bare` to the fingerprint's procedural hem, which dyed
    the brim's underside on one body and not the other)"""
    base = read_views(path)
    d = chapel_view(base[('D', 'rest')], (2, 17))
    r = chapel_view(base[('R', 'rest')], (2, 18))
    l = chapel_view(base[('L', 'rest')], (2, 18))
    reg = lambda g: [''.join('D' if c in 'RST' else '.' for c in row) for row in g]
    return [], [('D', 'rest', d, reg(d)), ('U', 'rest', d, reg(d)), ('R', 'rest', r, reg(r)), ('L', 'rest', l, reg(l)),
                ('L', 'lie', l, reg(l))]


# ------------------------------------------------------------------------------------------------ Imperial T2: the Montefortino
# the slice's T1 bronze knob cap (a Montefortino: tools/doll/p5b_sig.py, headwear/helm/nasal), copied under T2 (its planes
# shared) with a short crimson horsehair tuft on its knob (an overlay: Plume, the people's crest colour): the recruit's cap
# made the legionary's, under T3's crested helm and its transverse crest (STYLE_P5B 4: "Montefortino with a short tuft")
TUFT_LEGEND = ['legend W plume 0', 'legend X plume 2', 'legend Z plume 3', 'legend U plume 4']
TUFT_D = ['.........WW.........', '........WUZW........', '........WZXW........']
TUFT_R = ['........WW...........', '.......WUZW..........', '........WZXW.........']
TUFT_L = ['............WW.......', '..........WUZW.......', '.........WZXW........']


def tuft():
    return TUFT_LEGEND, [('D', 'rest', TUFT_D, None, (2, 3)), ('U', 'rest', TUFT_D, None, (2, 3)),
                         ('R', 'rest', TUFT_R, None, (3, 3)), ('L', 'rest', TUFT_L, None, (2, 3))]


# ------------------------------------------------------------------------------------------------ the pxm plumbing
def has_view(path, facing, key):
    return (facing, key) in read_views(path)


def block(tag, desc, legend, views, cond, replace):
    lines = [f'# {TAG} begin {tag}: {desc}']
    lines += legend
    for v in views:
        f, k, g, reg = v[:4]
        org = f' origin {v[4][0]} {v[4][1]}' if len(v) > 4 and v[4] else ''
        lines.append(f'view {f} key {k}{org} when {cond}{" replace" if replace else ""}')
        lines += ['|' + r for r in g]
        if reg and any(c != '.' for r in reg for c in r):
            lines.append('regions')
            lines += ['|' + r for r in reg]
    lines.append(f'# {TAG} end {tag}')
    return lines


def plan():
    """[(map path, tag, lines)]"""
    out = []
    spa = os.path.join(MAPS, 'headwear', 'helm', 'spangen.pxm')
    legend, views = gjermundbu()
    views = [v for v in views if has_view(spa, v[0], v[1])]
    out.append((spa, 'fjordfolk-gjermundbu', block(
        'fjordfolk-gjermundbu', 'the Fjordfolk T4 spectacle helm (the owner\'s fix: a rounded cap, the spectacle guard round the '
        'eyes at their own spacing, a mail aventail)', legend, views, 'sighelm-fjordfolk,helm.t4,!helm.t5', True)))
    cap = os.path.join(MAPS, 'headwear', 'cloth', 'cap.pxm')
    legend, views = bonnet()
    views = [v for v in views if has_view(cap, v[0], v[1])]
    lines = block('highland-bonnet', 'the Highland T1-T2 bonnet: a flat green crown, a diced band, a saffron toorie', legend, views,
                  'sighelm-highland,!helm.t3', True)
    legend, views = badge()
    views = [v for v in views if has_view(cap, v[0], v[1])]
    lines += block('highland-badge', 'the Highland T2 bonnet badge: the brooch, the eagle feather and the ribbon tails (an overlay)', legend, views,
                   'sighelm-highland,helm.t2,!helm.t3', False)
    out.append((cap, 'highland-bonnet', lines))
    ket = os.path.join(MAPS, 'headwear', 'helm', 'kettle.pxm')
    lines = [f'# {TAG} begin highland-kettle3: the Highland T3 kettle under its fur band, its aventail, the feather (E: the '
             'kettle\'s own furtrim views, copied under the signed helm\'s flag; the planes shared)']
    lines += copy_views(ket, 'furtrim,helm.t3,!helm.t4,!helmfam-kettle', 'sighelm-highland,helm.t3,!helm.t5', True)
    lines += copy_views(ket, 'furtrim', 'sighelm-highland,helm.t3,!helm.t4', False)
    lines.append(f'# {TAG} end highland-kettle3')
    legend, views = chapel(ket)
    lines += block('heartland-chapel', 'the Heartland T1 chapel: a brimmed kettle in boiled leather, its hatband heraldic red', legend, views,
                   'sighelm-heartland,!helm.t2', True)
    legend, views = kettle4()
    lines += block('highland-kettle4', 'the Highland T4 kettle grown: the fur band, the eagle feather moved and taller, a finial (an overlay)', legend, views,
                   'sighelm-highland,helm.t4,!helm.t5', False)
    out.append((ket, 'highland-kettle3', lines))
    nas = os.path.join(MAPS, 'headwear', 'helm', 'nasal.pxm')
    lines = []
    legend, views = tuft()
    lines += block('imperial-tuft', 'the Imperial T2 Montefortino: a short crimson tuft on the knob (an overlay)', legend, views,
                   'sighelm-imperial,helm.t2,!helm.t3', False)
    out.append((nas, 'imperial-montefortino', lines))
    return out


def strip_blocks(lines, every=False):
    """the lines without this script's blocks (every: without every P5b block, this script's, the slice's and any other
    area's). Another area's block found inside one of ours (p5b_sig.py inserts before the file's first `view` line, which
    may be ours) is kept unless `every`"""
    mine_b, mine_e = f'# {TAG} begin ', f'# {TAG} end '
    is_b = lambda s: s.startswith(ANY_P5B) and ' begin ' in s
    is_e = lambda s: s.startswith(ANY_P5B) and ' end ' in s
    out, skip, nested = [], False, False
    for s in lines:
        if not skip and (s.startswith(mine_b) or (every and is_b(s))):
            skip = True
            continue
        if skip and not nested and (s.startswith(mine_e) or (every and is_e(s))):
            skip = False
            continue
        if skip and not every and is_b(s):
            nested = True
        if not skip or nested:
            out.append(s)
        if nested and is_e(s):
            nested = False
    return out


def mine_on_disk():
    """the maps holding one of this script's blocks now (a block the plan dropped is taken out on the next write)"""
    out = []
    for d, _, fs in os.walk(MAPS):
        for f in fs:
            if f.endswith('.pxm') and f'# {TAG} begin ' in open(os.path.join(d, f), encoding='utf-8').read():
                out.append(os.path.join(d, f))
    return out


def write():
    by_file = {}
    for path, tag, lines in plan():
        by_file.setdefault(path, []).extend(lines)
    for path in mine_on_disk():
        if not any(os.path.samefile(path, p) for p in by_file):
            by_file[path] = []
    for path, add in by_file.items():
        src = open(path, encoding='utf-8').read().split('\n')
        src = strip_blocks(src)
        # before the first view or the first P5b block (the slice's begins with its legend lines: never split it)
        i = next(n for n, s in enumerate(src) if s.startswith('view ') or s.startswith(ANY_P5B))
        text = '\n'.join(src[:i] + add + src[i:])   # (built whole before the file opens: a failure never truncates a map)
        open(path, 'w', encoding='utf-8', newline='\n').write(text)
        print(f'{os.path.relpath(path, ROOT)}: +{len(add)} lines')


def verify():
    bad = 0
    files = sorted({p for p, _, _ in plan()})
    for path in files:
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
        for path, tag, lines in plan():
            print('\n'.join(lines))
        sys.exit(0)
    write()
