#!/usr/bin/env python3
"""tools/doll/p5b_sig.py (P5b SIG-ART, docs/STYLE_P5B.md 4 and 9): the style slice's people heads as ADDED views.

Every block this script writes is new `when sighelm-<people>,...` views (the resolver's per-slot people flag, doll_resolve.cpp
sigLookOf / flagsOf) and the local legend characters they use, inserted before a part's first view (a `replace` view wins in
file order, so the people's head is tried before the generic tier views; a part whose flag is absent never reaches them). No
drawn view's cells change. Re-running replaces this script's own blocks (between its begin and end comments) only.

  python tools/doll/p5b_sig.py            write the blocks into rpg/art/doll/maps
  python tools/doll/p5b_sig.py --verify   every original line of each touched map is still there, in order, and every added
                                          line sits inside one of this script's blocks (git show <base>:<file>)
  python tools/doll/p5b_sig.py --print    print the blocks

The heads (hand-placed; the dome shading is computed from each hand-drawn mask so every facing keeps the top-left light):
  Jade T1       the jingasa: headwear/cloth/conical in black lacquer (Head: the accent) with a vermilion mon (Plume) on its
                front face; the cone's own outline and radial ribs kept cell for cell
  Imperial T1   the bronze knob cap (a Montefortino): headwear/helm/nasal at T1 in Metal 2 (the brass-bronze fittings metal):
                (fix r1) an ogive bowl rising two rows to its knob, a short flared neck guard, hinged cheek guards
  Fjordfolk T1  the round leather cap with a fleece brow: headwear/helm/spangen at T1, (fix r1) a low flat-crowned dome in the
                leather, its gore seams in the people's thread (D), on a 2-row fleece roll (Head: the accent) the full head box
                wide, its ends down the temples (the S1 twin of r1: the two T1 caps had one outline)
  Fjordfolk T3  the spangenhelm's mail aventail: an overlay on headwear/helm/spangen at T3 (mail strips beside the face and a
                mail collar under the chin; the nasal and the face stay open); the T4 cone keeps it
  Imperial T3   (fix r1) the transverse crimson crest: a fan across the crown on headwear/helm/crested_crest (wide facing D / U,
                edge-on facing R / L; rest and the bob lag's up)
  Fjordfolk T5  (fix r1) the jarl's gilt horns: an overlay on headwear/helm/horned at T5, the horns grown to the flare and five
                rows over the crown in Metal 2, which the gilt people's T5 helm draws in the gilt; the family's legend comb
                steps aside in the resolver (fp::SigTier::ownGrowth)
  Jade T5       the kabuto legend's kuwagata in gilt: (fix r1) the resolver's (fp::SigTier::giltGrowth), no view re-cut
"""
import os
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
MAPS = os.path.join(ROOT, 'rpg', 'art', 'doll', 'maps')
BASE = '37331730328157e70cdd6b26c057fc63f5929a7c'   # STYLE_P5B's commit: the maps before P5b
TAG = '(P5b SIG)'


# ------------------------------------------------------------------------------------------------ shading helpers
def mirror(rows):
    return [r[::-1] for r in rows]


def shade_dome(mask, out='J', s1='N', s2='O', s3='P', s4='Q', solid='#'):
    """mask rows: `solid` cells are the shell; every other character is kept as written. Outline: a shell cell with a 4-neighbour
    that is not shell. Interior: by its place across its row's shell run (top-left light): the left quarter s3 (its topmost
    cells s4), the middle s2, the right third s1."""
    h = len(mask)
    w = max(len(r) for r in mask)
    m = [list(r.ljust(w, '.')) for r in mask]
    is_s = lambda x, y: 0 <= y < h and 0 <= x < w and mask[y][x:x + 1] == solid
    out_rows = []
    first_interior = {}
    for y in range(h):
        row = []
        xs = [x for x in range(w) if is_s(x, y)]
        lo, hi = (min(xs), max(xs)) if xs else (0, 0)
        for x in range(w):
            if not is_s(x, y):
                row.append(m[y][x])
                continue
            if not (is_s(x - 1, y) and is_s(x + 1, y) and is_s(x, y - 1) and is_s(x, y + 1)):
                row.append(out)
                continue
            t = (x - lo) / max(1, hi - lo)
            if t < 0.30:
                c = s3
                if x not in first_interior:
                    first_interior[x] = y
                    c = s4 if t < 0.25 and y < h // 2 else s3
            elif t < 0.66:
                c = s2
            else:
                c = s1
            row.append(c)
        out_rows.append(''.join(row))
    return out_rows


def box_erase(rows, box):
    """(fix r1, the pool's bytes) an erase only where hair can lie: inside the head box's columns (box: (first, last)) and
    never on the grid's top row (three rows over the crown: no hair reaches it); every other '-' becomes '.' (kept)"""
    out = []
    for y, r in enumerate(rows):
        out.append(''.join('.' if c == '-' and (y == 0 or x < box[0] or x > box[1]) else c for x, c in enumerate(r)))
    return out


def remap(rows, table):
    return [''.join(table.get(c, c) for c in r) for r in rows]


def paint(rows, cells):
    """cells: {(row, col): char}"""
    rr = [list(r) for r in rows]
    for (y, x), c in cells.items():
        rr[y][x] = c
    return [''.join(r) for r in rr]


# ------------------------------------------------------------------------------------------------ the heads
def shallow(rows):
    """the cone made shallow and low (T1's 2 rows over the crown): its apex kept, two of its upper courses (the third and the
    fifth) taken out so the slope flares to the brim like a jingasa's, the whole cone set two rows lower; the brim, the ribs
    and the lower courses row for row as drawn"""
    return ['.' * len(rows[0])] * 2 + [rows[0], rows[1], rows[3]] + rows[5:]


def jingasa(base_views):
    """headwear/cloth/conical: the cone in black lacquer (Head: F H I J V = steps 0-4) and the vermilion mon (Plume: W X Z =
    steps 1 2 3) on the front face. base_views: {(facing, key): rows} of the cone's own views."""
    lac = {'Q': 'F', 'R': 'H', 'S': 'I', 'T': 'J', 'U': 'V'}
    # the mon: a round 2x2 on the cone's front face (D: below the apex, left of the rib; R: the right flank; L: the left flank;
    # U is the back: none). Lacquer's sheen: the lit ridge (U -> V) kept as the cone's own highlight
    mon = {
        ('D', 'rest'): {(6, 8): 'Z', (6, 9): 'X', (7, 8): 'X', (7, 9): 'W'},
        ('R', 'rest'): {(6, 13): 'Z', (6, 14): 'X', (7, 13): 'X', (7, 14): 'W'},
        ('L', 'rest'): {(6, 5): 'Z', (6, 6): 'X', (7, 5): 'X', (7, 6): 'W'},
        ('L', 'lie'): {(6, 5): 'Z', (6, 6): 'X', (7, 5): 'X', (7, 6): 'W'},
        ('U', 'rest'): {},
    }
    views = []
    for (f, k), rows in base_views.items():
        g = remap(shallow(rows), lac)
        g = paint(g, mon.get((f, k), {}))
        views.append((f, k, g, None))
    legend = ['legend F head 0', 'legend H head 1', 'legend I head 2', 'legend J head 3', 'legend V head 4',
              'legend W plume 1', 'legend X plume 2', 'legend Z plume 3']
    return legend, views


# Imperial T1: the knob cap. '#': the bronze shell; '-' erases the hair under the bowl's rows; '.' keeps what is below.
KNOB_D = [   # (fix r1) the bowl rises to an ogive under the knob (4, 8, 10, 12 columns down from the knob): the Montefortino's
             # acorn, never the fleece cap's low round dome (the judge's S1 no-shield twin: the two caps differed by the knob alone)
    '--------------------',
    '---------##---------',
    '--------####--------',
    '------########------',
    '-----##########-----',
    '----############----',
    '---##############---',
    '---##############---',
    '...##############...',
    '...###........###...',
    '...###........###...',
    '...###........###...',
    '....#..........#....',
]
KNOB_R = [   # (a round bowl seen side on: the knob over its crown, a short flared lip at the nape, the hinged cheek guard
             # before the ear; never a brim, so it never reads as the Jade's jingasa) (fix r1: the ogive, as facing D)
    '---------------------',
    '---------##----------',
    '--------####---------',
    '------########-------',
    '-----##########------',
    '----############-----',
    '---##############----',
    '---##############----',
    '...##############....',
    '...##......###.......',
    '...........###.......',
    '...........###.......',
    '............#........',
]
# the rim row (8) carries the dye seam (the binding, the people's pair) inside its outline
def knob_regions(mask, band=7):
    """the bowl's lowest lit row (7, above the rim's outline) carries the dye seam: the brow band in the people's pair"""
    h, w = len(mask), len(mask[0])
    reg = [['.'] * w for _ in range(h)]
    xs = [x for x in range(w) if mask[band][x] == '#']
    for x in xs[1:-1]:
        reg[band][x] = 'D'
    return [''.join(r) for r in reg]


def knob_top(rows, cols):
    """the knob's crown row lit by hand (a 1-row run is all outline to shade_dome): its left cells Q P, its right J"""
    a, b = cols
    return paint(rows, {(1, a + 1): 'Q', (1, a + 2): 'P'})


def knob_cap():
    # the bronze in Metal 2 (the fittings metal: fp::Sig::trim sets the look's trim to the legion brass): J N O P Q = 0-4
    legend = ['legend J metal2 0', 'legend Q metal2 4']
    views = []
    d = paint(shade_dome(KNOB_D), {(1, 9): 'Q', (2, 9): 'Q', (2, 10): 'P'})
    r = paint(shade_dome(KNOB_R), {(1, 9): 'Q', (2, 9): 'Q', (2, 10): 'P'})
    lmask = mirror(KNOB_R)
    l = paint(shade_dome(lmask), {(1, 10): 'Q', (2, 10): 'Q', (2, 11): 'P'})
    d, r, l = box_erase(d, (2, 17)), box_erase(r, (3, 18)), box_erase(l, (2, 17))
    views.append(('D', 'rest', d, knob_regions(KNOB_D)))
    views.append(('U', 'rest', d, knob_regions(KNOB_D)))
    views.append(('R', 'rest', r, knob_regions(KNOB_R)))
    views.append(('L', 'rest', l, knob_regions(lmask)))
    views.append(('L', 'lie', l, knob_regions(lmask)))
    return legend, views


# Fjordfolk T1: the round leather cap ('#', leather) over a fleece roll ('*', Head: the accent). (fix r1) A low, flat-crowned
# dome (10, 12, 14 columns, its crown on the hair's: the Imperial's knob cap is an ogive two rows up under its knob) on a
# fleece roll the full head box wide, past the hair at the temples, its ends hanging a row down beside the brow over the hair:
# the round, heavy Fjordfolk outline at T1, in the figure area of P5b's first cap (the ladder's T2 and T3 steps)
FUR_D = [
    '--------------------',
    '--------------------',
    '--------------------',
    '-----##########-----',
    '----############----',
    '---##############---',
    '..****************..',
    '..****************..',
    '...**..........**...',
]
FUR_R = [
    '---------------------',
    '---------------------',
    '---------------------',
    '-----##########------',
    '----############-----',
    '---##############----',
    '...****************..',
    '...****************..',
    '....**..........**...',
]
# the cap's four gores meet at the crown: their seams stitched in the people's thread (region D, the dye seam: woad and
# wool), the leather between them (D, U: the front gore's two seams; R, L: one seam)
FUR_SEAMS = {'D': [(3, 8), (4, 7), (5, 7), (3, 11), (4, 12), (5, 12)], 'R': [(3, 10), (4, 10), (5, 11)]}


def fleece(mask):
    """the fleece roll: curls in two offset courses (V J lit on the left half, I H shaded on the right), its ends rounded off
    (H); (fix r1) the ends hang a row lower beside the brow: two curls each, the outer lit on the left, shaded on the right"""
    out = []
    for y, row in enumerate(mask):
        r = []
        xs = [x for x, c in enumerate(row) if c == '*']
        half = len(row) // 2
        for x, c in enumerate(row):
            if c != '*':
                r.append(c)
                continue
            i = x - xs[0]
            n = len(xs)
            left = i < n * 0.5
            if y >= 8:
                r.append(('J' if x == xs[0] else 'H') if x < half else ('I' if x != xs[-1] else 'F'))
            elif i == 0 or i == n - 1:
                r.append('H')
            elif y == 6:   # the roll's lit top: curls, the light on the left
                r.append(('JVIJ' if left else 'IJHI')[i % 4])
            else:          # its shaded underside: the curls' shadows between them
                r.append(('IHJH' if left else 'HFIH')[(i + 1) % 4])
        out.append(''.join(r))
    return out


def fur_regions(mask, seams):
    """the gore seams' D cells (the people's thread); every other cell none"""
    h, w = len(mask), len(mask[0])
    reg = [['.'] * w for _ in range(h)]
    for y, x in seams:
        if mask[y][x] == '#':
            reg[y][x] = 'D'
    return [''.join(r) for r in reg]


def fur_cap():
    legend = ['legend F head 0', 'legend H head 1', 'legend I head 2', 'legend J head 3', 'legend V head 4']
    lt = dict(out='f', s1='f', s2='g', s3='h', s4='i')   # a dark, oiled cap: a step under the jerkin, apart from the skin
    views = []
    for f, mask in (('D', FUR_D), ('U', FUR_D), ('R', FUR_R), ('L', mirror(FUR_R)), ('L lie', mirror(FUR_R))):
        g = shade_dome(mask, **lt)
        seams = FUR_SEAMS['D'] if f[0] in 'DU' else FUR_SEAMS['R']
        if f[0] == 'L':
            seams = [(y, len(mask[0]) - 1 - x) for y, x in seams]
        g = paint(g, {(y, x): 'g' for y, x in seams if g[y][x] in 'hij'})
        top = next(y for y, row in enumerate(mask) if '#' in row)   # the crown's lit cells (a 1-row run is all outline)
        g = paint(g, {(top, xs): c for xs, c in zip([i for i, ch in enumerate(mask[top]) if ch == '#'][1:3], 'ji')})
        g = box_erase(fleece(g), (3, 18) if f[0] == 'R' else (2, 17))
        fac, key = (f.split() + ['rest'])[:2]
        views.append((fac, key, g, fur_regions(mask, seams)))
    return legend, views


# Fjordfolk T3: the mail aventail (an overlay: '.' keeps the helm and the face). Mail: x / w in offset courses, v outline
AV_D = [
    '...vxv........vxv...',
    '...vwv........vwv...',
    '...vxv........vxv...',
    '...vwxv......vxwv...',
    '..vxwxvv....vvxwxv..',
    '..vvvvvvvvvvvvvvvv..',
]
AV_R = [
    '..vxwxv..............',
    '..vwxwxv.............',
    '..vxwxwxv............',
    '..vwxwxwxv...........',
    '..vxwxwxwv...........',
    '..vwxwxwxwv..........',
    '...vvvvvvvv..........',
]


# T4: the aventail grown to the shoulders (two courses longer, the collar a pixel wider each side), under the spike
AV4_D = [
    '...vxv........vxv...',
    '...vwv........vwv...',
    '...vxv........vxv...',
    '...vwxv......vxwv...',
    '..vxwxvv....vvxwxv..',
    '.vxwxwxwvvvvwxwxwxv.',
    '.vwxwxwxwxwxwxwxwxv.',
    '..vvvvvvvvvvvvvvvv..',
]
AV4_R = [
    '.vxwxv...............',
    '.vwxwxv..............',
    '.vxwxwxv.............',
    '.vwxwxwxv............',
    'vxwxwxwxwv...........',
    'vwxwxwxwxwv..........',
    'vxwxwxwxwxv..........',
    '.vvvvvvvvvv..........',
]


def aventail4():
    pad = ['.' * 20] * 10
    padr = ['.' * 21] * 10
    d = pad + AV4_D
    r = padr + AV4_R
    l = padr + [row[::-1].replace('x', 'q').replace('w', 'x').replace('q', 'w') for row in AV4_R]
    u = pad + ['..vxwxwxwxwxwxwxv...', '..vwxwxwxwxwxwxwv...', '..vxwxwxwxwxwxwxv...', '..vwxwxwxwxwxwxwv...',
               '..vxwxwxwxwxwxwxv...', '.vwxwxwxwxwxwxwxwv..', '.vxwxwxwxwxwxwxwxv..', '..vvvvvvvvvvvvvvv...']
    return [], [('D', 'rest', d, None), ('U', 'rest', u, None), ('R', 'rest', r, None), ('L', 'rest', l, None)]


def aventail():
    views = []
    pad = ['.' * 20] * 10
    padr = ['.' * 21] * 10
    d = pad + AV_D
    r = padr + AV_R
    # L: the R mask mirrored, its courses re-lit (x the lit course on the left); U: the back, a full mail curtain
    l = padr + [row[::-1].replace('x', 'q').replace('w', 'x').replace('q', 'w') for row in AV_R]
    u = pad + ['..vxwxwxwxwxwxwxv...', '..vwxwxwxwxwxwxwv...', '..vxwxwxwxwxwxwxv...', '..vwxwxwxwxwxwxwv...',
               '..vxwxwxwxwxwxwxv...', '..vvvvvvvvvvvvvvv...']
    views.append(('D', 'rest', d, None))
    views.append(('U', 'rest', u, None))
    views.append(('R', 'rest', r, None))
    views.append(('L', 'rest', l, None))
    return [], views


# Jade T5: the kabuto legend's kuwagata in gilt. The T5 horn views' cells re-coloured: the horns' metal steps 1-4 in the gilt
# (Device: the T5 device's gold, sigLookOf), their outline and the metal-2 root as drawn, the regions as drawn. (A region
# re-cut, R on the horns, lost to D93's trim band, which draws a T5 R run in Metal 2)
KUWA = {'w': 'k', 'x': 'l', 'y': 'H', 'z': 'I'}


def kuwagata_recut(path):
    cond = 'helm.t5,alloyshape,helmfam-kabuto,crest-horns'
    views = []
    for hdr, g, reg in read_when_views(path, cond):
        p = hdr.split()
        fac, key = p[1], p[p.index('key') + 1]
        org = (int(p[p.index('origin') + 1]), int(p[p.index('origin') + 2])) if 'origin' in p else None
        views.append((fac, key, remap(g, KUWA), reg, org))
    return views


# (fix r1) Imperial T3: the transverse crest (STYLE_P5B 4: "crested, transverse crimson crest"), a crimson horsehair brush
# across the crown from side to side on its steel holder: facing D / U a wide low fan (12 columns, the crest's arc), facing
# R / L edge-on, a narrow upright brush. Plume steps Q R S T U = 0-4 (the crest colour: the Imperial's crimson), the holder
# in the helm's metal (v x w). T4's centurion fan (plumed, variant 1) is the same crest grown: taller, its ends down the sides
CREST3_D = [   # a fan: the bristle tips widest, gathered into the holder on the crown (never a cap's flat block on the dome)
    '...QQQQQQQQQQ...',
    '..QTUTUTTSSRRQ..',
    '...QTUTSTSRRQ...',
    '.....QQSRQQ.....',
    '......vxwv......',
]
CREST3_R = [
    '.......QQ.......',
    '......QTSQ......',
    '......QUSQ......',
    '......QTRQ......',
    '.....vxxwv......',
]


def crest3():
    views = []
    l = ['.' + row[:-1] for row in CREST3_R]   # facing L: the crown a column right of R's (the T3 views' own placement), the light kept
    for key, oy in (('rest', 2), ('up', 1)):   # up: the bob lag, a row lower (the crest channel's walk and run frames)
        for f, g in (('D', CREST3_D), ('U', CREST3_D), ('R', CREST3_R), ('L', l)):
            views.append((f, key, g, None, (0, oy)))
    return [], views


# (fix r1) Fjordfolk T5: the jarl's gilt horns (decision 2: horns at T5-T6), an overlay on headwear/helm/horned's own views:
# each horn grown from the temple out to the flare (3 columns past the head box, T5's cap) and up to two rows over the crown,
# its tip turned in, in Metal 2 (J N O P Q = 0-4), which the gilt people's T5 helm draws in the gilt (doll_paint toGilt:
# Metal 2 -> Device, the T5 device's gold). Facing D / U both horns; facing R / L the far horn behind the dome and the near
# one before the brow. Every base horn pixel is covered (the leather horn under it never shows)
# (fix r1) the horns drawn as rows from base row -2 (five rows over the crown, inside T5's 8) down to the root on the dome's
# side at base row 8; '.' keeps the base. Each horn sweeps out from the temple to the flare (3 columns past the head box) and
# rises, its tip curling in over the brow: the aurochs horn of the icon, never the Jade's kuwagata (a V from the brow's centre)
HORN_TOP = -3   # the first row of the drawings below, in the base view's rows
HORN_L_ROWS = [   # facing D, the left horn, columns 0-7 of the base grid (head box columns 4-19)
    '....JJ..',
    '...JQJ..',
    '..JPOJ..',
    '.JPOJ...',
    '.JPOJ...',
    '.JPOJ...',
    '.JPONJ..',
    '.JPONJ..',
    '.JPONNJ.',
    '.JOONNJ.',
    '..JONNNJ',
    '...JJNNJ',
]
HORN_R_ROWS = [   # facing R, both horns, columns 0-24 of the base grid (head box columns 5-20): the far horn behind the dome,
                  # the near one before the brow
    '....JJ..............JJ...',
    '...JQJ..............JQJ..',
    '...JPOJ............JPOJ..',
    '..JPOJ..............JPOJ.',
    '..JPOJ..............JPOJ.',
    '..JPOJ..............JPOJ.',
    '..JPONJ.............JPOJ.',
    '..JPONJ............JPONJ.',
    '..JOONNJ..........JPONNJ.',
    '..JONNNJ.........JPONNJ..',
    '...JNNNJ........JONNNJ...',
]


def rows_cells(rows, top):
    return {(top + y, x): c for y, r in enumerate(rows) for x, c in enumerate(r) if c != '.'}


HORN_L = rows_cells(HORN_L_ROWS, HORN_TOP)
HORN_R = rows_cells(HORN_R_ROWS, HORN_TOP)
RELIT = {'P': 'N', 'N': 'P', 'Q': 'P'}   # a horn turned to the other side: its lit edge moves to the light's side


def grid_of(cells, w, h):
    g = [['.'] * w for _ in range(h)]
    for (y, x), c in cells.items():
        g[y][x] = c
    return [''.join(r) for r in g]


def horns5():
    W = 24
    right = {(y, W - 1 - x): RELIT.get(c, c) for (y, x), c in HORN_L.items()}
    t = HORN_TOP
    shift = lambda cells: {(y - t, x): c for (y, x), c in cells.items()}   # grid row 0 = base row HORN_TOP
    d = grid_of(shift({**HORN_L, **right}), W, 9 - t)
    WR = 25
    r = grid_of(shift(HORN_R), WR, 8 - t)
    l = grid_of(shift({(y, WR - 1 - x): RELIT.get(c, c) for (y, x), c in HORN_R.items()}), WR, 8 - t)
    legend = ['legend J metal2 0', 'legend Q metal2 4']
    oy = 1 - t   # the base's origin row 1, the grid starting HORN_TOP rows above the base's row 0
    views = [('D', 'rest', d, None, (4, oy)), ('U', 'rest', d, None, (4, oy)), ('R', 'rest', r, None, (5, oy)),
             ('L', 'rest', l, None, (4, oy)), ('L', 'lie', l, None, (4, oy))]
    return legend, views


# (the T5 comb, horned_legend's growth, steps aside in the resolver: fp::SigTier::ownGrowth, no pool bytes)


# ------------------------------------------------------------------------------------------------ the pxm plumbing
def read_when_views(path, cond):
    """[(header, rows, regions)] of the views whose `when` is exactly cond (with or without replace)"""
    out, cur, mode = [], None, None
    for line in open(path, encoding='utf-8'):
        s = line.rstrip(chr(10))
        if s.startswith('view '):
            w = s.split(' when ', 1)[1].split() if ' when ' in s else []
            cur = (s, [], []) if w and w[0] == cond else None
            if cur:
                out.append(cur)
            mode = 'g'
        elif s.startswith('regions'):
            mode = 'r'
        elif s.startswith('build'):
            mode = 'b'
        elif s.startswith('|') and cur is not None:
            row = s[1:].split('|')[0]
            if mode == 'g':
                cur[1].append(row)
            elif mode == 'r':
                cur[2].append(row)
    return out

def read_views(path):
    """{(facing, key): rows} of a part's BASE views (no `when`)"""
    out = {}
    cur = None
    for line in open(path, encoding='utf-8'):
        s = line.rstrip('\n')
        if s.startswith('view '):
            p = s.split()
            if ' when ' in s:
                cur = None
                continue
            key = p[p.index('key') + 1] if 'key' in p else 'rest'
            cur = (p[1], key)
            out[cur] = []
        elif s.startswith('regions') or s.startswith('build'):
            cur = None
        elif s.startswith('|') and cur is not None:
            out[cur].append(s[1:].split('|')[0])
        elif not s.strip():
            pass
    return out


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
    con = os.path.join(MAPS, 'headwear', 'cloth', 'conical.pxm')
    legend, views = jingasa(read_views(con))
    out.append((con, 'jade-jingasa', block('jade-jingasa', 'the Jade T1 jingasa: black lacquer, a vermilion mon (STYLE_P5B 4, 9)',
                                           legend, views, 'sighelm-jade', True)))
    nas = os.path.join(MAPS, 'headwear', 'helm', 'nasal.pxm')
    legend, views = knob_cap()
    views = [v for v in views if has_view(nas, v[0], v[1])]
    out.append((nas, 'imperial-knobcap', block('imperial-knobcap', 'the Imperial T1 bronze knob cap: bowl, knob, neck guard, cheek guards',
                                               legend, views, 'sighelm-imperial,!helm.t2', True)))
    spa = os.path.join(MAPS, 'headwear', 'helm', 'spangen.pxm')
    legend, views = fur_cap()
    views = [v for v in views if has_view(spa, v[0], v[1])]
    lines = block('fjordfolk-furcap', 'the Fjordfolk T1 round leather cap and its fleece brow', legend, views,
                  'sighelm-fjordfolk,helm.t1,!helm.t2', True)
    legend, views = aventail()
    views = [v for v in views if has_view(spa, v[0], v[1])]
    lines += block('fjordfolk-aventail', 'the Fjordfolk T3 spangenhelm\'s mail aventail (an overlay)', legend, views,
                   'sighelm-fjordfolk,helm.t3,!helm.t4', False)
    out.append((spa, 'fjordfolk', lines))
    legend, views = aventail()
    views = [v for v in views if has_view(nas, v[0], v[1])]
    out.append((nas, 'fjordfolk-aventail-cone', block('fjordfolk-aventail-cone', 'the Fjordfolk T4 cone helm keeps the T3 aventail (an overlay, the planes shared)',
                                                      legend, views, 'sighelm-fjordfolk,helm.t4', False)))
    cc = os.path.join(MAPS, 'headwear', 'helm', 'crested_crest.pxm')
    legend, views = crest3()
    out.append((cc, 'imperial-crest3', block('imperial-crest3', 'the Imperial T3 transverse crimson crest (fix r1): wide facing D / U, edge-on facing R / L',
                                             legend, views, 'sighelm-imperial,helm.t3,!helm.t4', True)))
    hor = os.path.join(MAPS, 'headwear', 'helm', 'horned.pxm')
    legend, views = horns5()
    out.append((hor, 'fjordfolk-horns5', block('fjordfolk-horns5', 'the Fjordfolk T5 gilt horns (fix r1): grown to the flare and over the crown (an overlay)',
                                               legend, views, 'sighelm-fjordfolk,helm.t5,!helm.t6', False)))
    # (fix r1) the Jade T5 kuwagata's gilt is the resolver's (fp::SigTier::giltGrowth, doll_paint toGiltGrowth): no view re-cut
    return out


def strip_blocks(lines):
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


def write():
    by_file = {}
    for path, tag, lines in plan():
        by_file.setdefault(path, []).extend(lines)
    for path, add in by_file.items():
        src = open(path, encoding='utf-8').read().split('\n')
        src = strip_blocks(src)
        i = next(n for n, s in enumerate(src) if s.startswith('view '))
        new = src[:i] + add + src[i:]
        open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(new))
        print(f'{os.path.relpath(path, ROOT)}: +{len(add)} lines')


def verify():
    bad = 0
    files = sorted({p for p, _, _ in plan()})
    for path in files:
        rel = os.path.relpath(path, ROOT).replace('\\', '/')
        old = subprocess.run(['git', '-C', ROOT, 'show', f'{BASE}:{rel}'], capture_output=True, text=True, encoding='utf-8').stdout.split('\n')
        now = open(path, encoding='utf-8').read().split('\n')
        if strip_blocks(now) != old:
            print(f'FAIL {rel}: outside its {TAG} blocks the map differs from {BASE[:8]}')
            bad += 1
        else:
            print(f'ok   {rel}: {len(now) - len(old)} lines added, all inside {TAG} blocks; every drawn view as it was')
    return bad


if __name__ == '__main__':
    if '--verify' in sys.argv:
        sys.exit(1 if verify() else 0)
    if '--print' in sys.argv:
        for path, tag, lines in plan():
            print('\n'.join(lines))
        sys.exit(0)
    write()
