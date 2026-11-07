#!/usr/bin/env python3
"""tools/doll/p5b_fix_r2.py (P5b2 fix round 2, the lead; docs/STYLE_P5B.md 2, 4; judge P5b2 r2 items 1, 4, 5, 6): ADDED views
only. Every block this script writes is tagged `# (P5b fix-r2) begin <tag>` .. `end <tag>`: new `when` views (a replace view
at the top of the part's views, where the first replacing view whose condition holds wins; an overlay at the end) and their
local legend lines. No view drawn before this round changes a cell. Run it LAST, after tools/doll/p5b_sig_A2.py, p5b_sig_B2.py,
p5b_sig_C2.py and p5b_fix_r1.py (those rebuild their blocks from their own bases and keep the rest of each map; this script
strips its own blocks and writes them again, so it is idempotent), then `node tools/doll/pxc.mjs`.

  python tools/doll/p5b_fix_r2.py            write the blocks into rpg/art/doll/maps
  python tools/doll/p5b_fix_r2.py --verify   each touched map is the base's (ad67d9c, fix r1) but for this script's blocks
  python tools/doll/p5b_fix_r2.py --print    print the blocks

The blocks:
  steppe-t4-lit   (C19 A3, ENKHOBRASS: the helm's top-left 6 px of steps 3-4) the Steppe T4 signature's spiked bowl (plumed
                  variant 2, the owner's T4) facing D with its highlight carried up the left of the cone: the drawn view lit the
                  bowl's left from the brim up but left the cone's upper left dark, so under the horsetail plume (the head box's
                  top) the helm's top-left quadrant held 4 lit px. Two cells of row 8 step up (x 2 -> 3): the highlight now
                  sweeps from the cone's top down its left flank. Signed Steppe T4 helms only (sighelm-steppe).
  sylvan-leaf5-tip (C19 A7, THAERSTEEL: 2-4 speculars of 2-6 px) the Sylvan T5 signature's great leaf pauldron (leaf.5, the
                  right shoulder, facing D): the last leaf's gilt tip held a lone 1 px step-4 glint (the compositor's glints
                  keep a drawn step-3 trim point); under the signed Sylvan T5 body the tip's outer two cells are lit together, a
                  2 px glint on the leaf's lit (outer) half. (Dimming the tip instead moved the glass pass's glints: four on the
                  winged helm.)
  (item 4, C12b) the elite heads' fittings marked as the trim they are, or drawn as trim (TRIM_SPECS, MORE_SPECS: overlays
                  that copy a view's own cells with region R, G or N): the Steppe khan's cheek lames, the Marsh bog king's
                  crown, band and antler tips, the River morion's comb and bowl foot, the Sun Temple jaguar's teeth, eagle's
                  brow and fan, the Dune T3 brooch, the Starspire band, finial and cheeks, the Sylvan T6 wing tips and veins,
                  the Imperial T6 crest holder; T5-T6 gems facing R (River, Starspire, Sun Temple), the Highland chief's T6
                  brooch and band runes (C13), the signed noble cloak's clasp, chain and drape hem.
  (item 4, C12b step 4) PIXEL_SPECS: the Imperial T2 cheek-guard hinges and shoulder lames polished (step 4).
  (item 6, C12f) PIXEL_SPECS: the Sun Temple T6 fan's centre gaps closed; the Starspire T5 cheek pieces a pixel further round
                  the face and their tips a pixel longer; its hood's edges a row longer.
  Never the style slice's frames (Fjordfolk, Imperial and Jade at T1/T3/T5: slice_same 567/567).
"""
import os
import re
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
MAPS = os.path.join(ROOT, 'rpg', 'art', 'doll', 'maps')
TAG = '(P5b fix-r2)'
BASE = 'ad67d9c'   # fix r1: the maps before this round


def P(*parts):
    return os.path.join(MAPS, *parts)


def read_lines(path):
    return open(path, encoding='utf-8').read().replace('\r\n', '\n').split('\n')


def find_view(lines, head):
    """the body of the view whose header line is exactly `head`: (marks, grid, regions), each a list of lines"""
    for i, s in enumerate(lines):
        if s.rstrip() == head:
            j = i + 1
            marks, grid, regs = [], [], []
            while j < len(lines) and lines[j].startswith('mark '):
                marks.append(lines[j])
                j += 1
            while j < len(lines) and lines[j].startswith('|'):
                grid.append(lines[j])
                j += 1
            if j < len(lines) and lines[j].strip() == 'regions':
                j += 1
                while j < len(lines) and lines[j].startswith('|'):
                    regs.append(lines[j])
                    j += 1
            return marks, grid, regs
    raise SystemExit(f'no view "{head}"')


def set_cells(grid, cells):
    """grid rows ('|'-prefixed) with (row, col, old, new) cells changed; old is checked"""
    rows = [list(r[1:]) for r in grid]
    for y, x, old, new in cells:
        if rows[y][x] != old:
            raise SystemExit(f'cell ({x},{y}) is {rows[y][x]!r}, expected {old!r}')
        rows[y][x] = new
    return ['|' + ''.join(r) for r in rows]


def overlay_of(grid, regs, cells):
    """an overlay grid the base's size: '.' everywhere but the cells (row, col, old, new), each with its base region"""
    rows = [list(r[1:]) for r in grid]
    rr = [list(r[1:]) for r in regs] if regs else None
    out = [['.'] * len(r) for r in rows]
    oreg = [['.'] * len(r) for r in rows]
    for y, x, old, new in cells:
        if rows[y][x] != old:
            raise SystemExit(f'cell ({x},{y}) is {rows[y][x]!r}, expected {old!r}')
        out[y][x] = new
        if rr:
            oreg[y][x] = rr[y][x]
    return ['|' + ''.join(r) for r in out], ['|' + ''.join(r) for r in oreg]


# ------------------------------------------------------------------------------------------------ the blocks
def legend_of(lines):
    """the map's characters -> (slot, step): its `use` legends, then its own `legend` lines (in file order)"""
    leg = {}
    for s in lines:
        if s.startswith('use '):
            for nm in s.split()[1:]:
                for t in open(os.path.join(MAPS, 'legend', nm + '.legend'), encoding='utf-8'):
                    m = re.match(r'legend (\S) (\S+) (\d)', t)
                    if m:
                        leg[m.group(1)] = (m.group(2), int(m.group(3)))
        m = re.match(r'legend (\S) (\S+) (\d)', s)
        if m:
            leg[m.group(1)] = (m.group(2), int(m.group(3)))
    return leg


def region_overlay(base, head, spans, region='R', extra='helm.t1', cond=None, force=False):
    """an overlay on the view `head` (a replace or base view): the cells of `spans` that are drawn at step 1 or more (never an
    outline, never a cell that already has a region), each its own character with `region`; the compositor's finish then draws
    them in the tier's trim (R) or gem (G) slot. A span is (row, x0, x1) inclusive, or (row, x0, x1, slots[, steps]) keeping
    only cells of those slots (and steps). The overlay keeps the view's origin and condition; `extra`, a flag the condition
    already implies, keeps it a view of its own (one view per facing, key and when)"""
    leg = legend_of(base)
    marks, grid, regs = find_view(base, head)
    rows = [r[1:] for r in grid]
    rr = [r[1:] for r in regs] if regs else None
    w = max(len(r) for r in rows)
    out = [['.'] * w for _ in rows]
    oreg = [['.'] * w for _ in rows]
    n = 0
    for sp in spans:
        y, x0, x1 = sp[0], sp[1], sp[2]
        slots = sp[3] if len(sp) > 3 else None
        steps = sp[4] if len(sp) > 4 else None
        if y >= len(rows):
            continue
        for x in range(max(0, x0), min(x1, w - 1) + 1):
            c = rows[y][x] if x < len(rows[y]) else '.'
            if c in '.- ' or out[y][x] != '.':
                continue
            sl, st = leg.get(c, ('', 0))
            if st < 1 or (slots and sl not in slots) or (steps and st not in steps):
                continue
            if not force and rr and y < len(rr) and x < len(rr[y]) and rr[y][x] not in '.':
                continue
            out[y][x] = c
            oreg[y][x] = region
            n += 1
    if n == 0:
        return None   # (nothing of the spans drawn there: a back view's teeth)
    return (overlay_head(head, extra, cond), [], ['|' + ''.join(r) for r in out], ['|' + ''.join(r) for r in oreg])


def overlay_head(head, extra, cond=None):
    """the overlay's header for the view `head`: its facing, key and origin; its condition and `extra` (a flag the condition
    already implies, so the overlay is a view of its own), or `cond` for a base view (no condition of its own)"""
    m = re.match(r'view (\S) key (\S+)(?: origin (-?\d+) (-?\d+))?(?: layer \S+)?(?: when (.*?)( replace)?)?$', head)
    if not m:
        raise SystemExit(f'cannot read "{head}"')
    f, key, ox, oy, when = m.group(1), m.group(2), m.group(3), m.group(4), m.group(5)
    org = f' origin {ox} {oy}' if ox is not None else ''
    if cond:
        return f'view {f} key {key}{org} when {cond}'
    if not when:
        raise SystemExit(f'"{head}" is a base view: give the overlay its condition')
    return f'view {f} key {key}{org} when {when},{extra}'


def pixel_overlay(base, head, cells, extra='helm.t1', cond=None):
    """an overlay on the view `head` redrawing the cells [(row, x, old, new[, region]), ...] (old is checked), each keeping its
    region (or the one given)"""
    marks, grid, regs = find_view(base, head)
    rows = [r[1:] for r in grid]
    rr = [r[1:] for r in regs] if regs else None
    w = max(len(r) for r in rows)
    out = [['.'] * w for _ in rows]
    oreg = [['.'] * w for _ in rows]
    for cell in cells:   # (a cell under the grid's last row grows the overlay's grid down: a tip a pixel longer)
        y = cell[0]
        while y >= len(rows):
            rows.append('.' * w)
            out.append(['.'] * w)
            oreg.append(['.'] * w)
    up = max([0] + [-c[0] for c in cells])   # (a cell over the grid's first row grows it up: the origin moves down as many rows)
    for _ in range(up):
        rows.insert(0, '.' * w)
        out.insert(0, ['.'] * w)
        oreg.insert(0, ['.'] * w)
    for cell in cells:
        y, x, old, new = cell[:4]
        y += up
        if rows[y][x] != old:
            raise SystemExit(f'{head}: cell ({x},{y}) is {rows[y][x]!r}, expected {old!r}')
        out[y][x] = new
        if len(cell) > 4:
            oreg[y][x] = cell[4]
        elif rr and y < len(rr) and x < len(rr[y]):
            oreg[y][x] = rr[y][x]
    nh = overlay_head(head, extra, cond)
    if up:
        m = re.search(r' origin (-?\d+) (-?\d+)', nh)
        if not m:
            raise SystemExit(f'{head}: growing up needs an origin')
        nh = nh.replace(m.group(0), f' origin {m.group(1)} {int(m.group(2)) + up}', 1)
    return (nh, [], ['|' + ''.join(r) for r in out], ['|' + ''.join(r) for r in oreg])


def find_head(base, sub):
    hs = [t.rstrip() for t in base if t.startswith('view ') and (t.rstrip() == sub or t.rstrip() == 'view ' + sub)]
    if not hs:
        raise SystemExit(f'no view "{sub}"')
    return hs[0]


def marks_of(spec):
    """a builder: spec [(head, spans, region, extra, cond, force), ...] (region_overlay2 per head)"""
    def fn(base):
        out = []
        for it in spec:
            head, spans = it[0], it[1]
            region = it[2] if len(it) > 2 else 'R'
            extra = it[3] if len(it) > 3 else 'helm.t1'
            cond = it[4] if len(it) > 4 else None
            force = it[5] if len(it) > 5 else False
            v = region_overlay(base, find_head(base, head), spans, region, extra, cond, force)
            if v:
                out.append(v)
        return out
    return fn


def pixels_of(spec):
    """a builder: spec [(head, cells, extra, cond), ...] (pixel_overlay per head)"""
    def fn(base):
        return [pixel_overlay(base, find_head(base, it[0]), it[1], it[2] if len(it) > 2 else 'helm.t1', it[3] if len(it) > 3 else None)
                for it in spec]
    return fn


def regions_of(spec):
    """a builder: spec [(head, spans[, region]), ...] -> one overlay per head"""
    def fn(base):
        out = []
        for it in spec:
            hs = [t.rstrip() for t in base if t.startswith('view ') and (t.rstrip() == it[0] or t.rstrip() == 'view ' + it[0])]
            if not hs:
                raise SystemExit(f'no view "{it[0]}"')
            v = region_overlay(base, hs[0], it[1], it[2] if len(it) > 2 else 'R')
            if v:
                out.append(v)
        return out
    return fn


def all_rows(n, slots=None, steps=None):
    return [(y, 0, 99) + ((slots,) if slots else ()) + ((steps,) if steps else ()) for y in range(n)]



def steppe_t4_lit(base):
    head = 'view D key rest origin 2 4 when helmfam-plumed,variant-2 replace'
    marks, grid, regs = find_view(base, head)
    g = set_cells(grid, [(8, 7, 'x', 'y'), (8, 8, 'x', 'y')])
    return [('view D key rest origin 2 4 when sighelm-steppe,helm.t4,!helm.t5,variant-2 replace', marks, g, regs)]


def sylvan_leaf5_tip(base):
    out = []
    for key, org in (('rest', '6 3'), ('up', '6 4')):
        marks, grid, regs = find_view(base, f'view D key {key} origin {org}')
        g, r = overlay_of(grid, regs, [(12, 4, 'w', 'y')])
        out.append((f'view D key {key} when sigbody-sylvan,armour.t5,!armour.t6', [], g, r))
    return out


# (item 4, C12b: a T5 (rare) or T6 helm's trim 10-36 % of its pixels; T3 3-36 %, T4 6-36 %) the elite heads' fittings marked as the
# trim they are (R: the finish draws a signed T5-T6 helm's in the gilt, Device; T3-T4 in Metal 2)
def rows(ys, slots=None, steps=None, x0=0, x1=99):
    return [(y, x0, x1) + ((slots,) if slots or steps else ()) + ((steps,) if steps else ()) for y in ys]


M2 = ('metal2',)
TRIM_SPECS = [
    # the Steppe khan's lamellar cheek guards: two gilt lames a side (T5 rows 18 and 22, T6 rows 19 and 21)
    (P('headwear', 'helm', 'plumed.pxm'), 'steppe-t56-lames', 'the Steppe T5-T6 cheek guards, two gilt lames a side (C12b)',
     [(f'{h} when sighelm-steppe,helm.t5,!helm.t6 replace', rows((18, 22), ('metal',)))
      for h in ('D key rest origin 1 9', 'R key rest origin 2 9', 'L key rest origin 1 9')] +
     [(f'{h} when sighelm-steppe,helm.t6 replace', rows((19, 21), ('metal',)))
      for h in ('D key rest origin 2 9', 'R key rest origin 3 9', 'L key rest origin 1 9')]),
    # the Marsh bog king: his kettle crown and its band gilt under the reed brim; at T6 the antlers' tips too
    (P('headwear', 'helm', 'kettle.pxm'), 'marsh-t56-crown', "the Marsh T5-T6 bog king's gilt crown and band (C12b)",
     [(f'{h} when sighelm-marsh,helm.t5,!helm.t6 replace', rows((9, 10, 11, 12), ('metal', 'leather'), x0=5, x1=16))
      for h in ('D key rest origin 3 9', 'U key rest origin 3 9', 'R key rest origin 3 9', 'L key rest origin 3 9')] +
     [(f'{h} when sighelm-marsh,helm.t6 replace', rows((9, 10, 11, 12), ('metal', 'leather'), x0=5, x1=16) +
       (rows((1,), ('cloth3',)) if h[0] in 'DU' else []))
      for h in ('D key rest origin 3 9', 'U key rest origin 3 9', 'R key rest origin 3 9', 'L key rest origin 3 9')]),
    # the River captain's morion: its gilt comb and plume holder (Metal 2) the trim they are
    (P('headwear', 'helm', 'morion.pxm'), 'river-t56-comb', "the River T5-T6 morion's gilt comb, the trim it is (C12b)",
     [(f'{h} when sighelm-river,helm.t5,!helm.t6 replace', rows(range(16), M2))
      for h in ('D key rest origin 3 5', 'U key rest origin 3 5', 'R key rest origin 3 5', 'L key rest origin 3 5')] +
     [(f'{h} when sighelm-river,helm.t6 replace', rows(range(16), M2))
      for h in ('D key rest origin 3 6', 'U key rest origin 3 6', 'R key rest origin 3 6', 'L key rest origin 3 6')]),
    # the Sun Temple jaguar's teeth and fangs (T3-T5) and the eagle's brow (T6): gilt fittings (Metal 2 at T3-T4)
    (P('headwear', 'helm', 'masked.pxm'), 'sun-t36-teeth', "the Sun Temple jaguar's teeth (T3-T5) and the eagle's brow (T6) as trim (C12b)",
     [(f'{h} when sighelm-suntemple,helm.t3,!helm.t6 replace', rows(range(8, 16), ('metal',)))
      for h in ('D key rest origin 2 2', 'U key rest origin 2 2', 'R key rest origin 2 2', 'L key rest origin 2 2')] +
     [(f'{h} when sighelm-suntemple,helm.t6 replace', rows(range(8, 13), ('metal',)))
      for h in ('D key rest origin 1 3', 'R key rest origin 1 3', 'L key rest origin 2 3')]),
    # the Sun Temple fan: gold-tipped feathers and the band edged in gold (T5 its top and bottom rows; T6 the band)
    (P('headwear', 'helm', 'masked_legend.pxm'), 'sun-t56-fan', 'the Sun Temple T5-T6 fan: gold-tipped feathers, the band edged in gold (C12b)',
     [(f'{h} when sighelm-suntemple,helm.t5,!helm.t6 replace', rows((0, 1), ('plume',)) + rows((2, 5), ('dye',)))
      for h in ('D key rest origin 3 6', 'U key rest origin 3 6', 'D key breeze origin 3 6', 'U key breeze origin 3 6')] +
     [(f'{h} when sighelm-suntemple,helm.t5,!helm.t6 replace', rows((0, 1), ('plume',)) + rows((3, 5), ('dye',)))
      for h in ('R key rest origin 3 6', 'L key rest origin -2 6')] +
     [(f'{h} when sighelm-suntemple,helm.t6 replace', rows((0, 1), ('plume',)) + rows((3, 4), ('dye',)))
      for h in ('D key rest origin 2 8', 'U key rest origin 2 8', 'D key breeze origin 2 8', 'U key breeze origin 2 8')] +
     [(f'{h} when sighelm-suntemple,helm.t6 replace', rows((0, 1), ('plume',)) + rows((3, 4, 5), ('dye',)))
      for h in ('R key rest origin 2 8', 'L key rest origin -1 8')]),
    # the Dune T3 turban's brooch (Metal 2) the trim it is
    (P('headwear', 'helm', 'veil.pxm'), 'dune-t3-brooch', "the Dune T3 turban's brooch, the trim it is (C12b)",
     [(f'{f} key rest when sighelm-dune,helm.t3,!helm.t4', rows(range(12), M2)) for f in 'DURL']),
    # the Starspire knight's ogive: its star-white band and finial gilt
    (P('headwear', 'helm', 'crown.pxm'), 'star-t56-band', "the Starspire T5-T6 ogive's band and finial gilt (C12b)",
     [(f'{h} when sighelm-starspire,helm.t5,!helm.t6 replace', rows(range(16), M2) + rows((0, 1, 2), ('metal',)))
      for h in ('D key rest origin 1 6', 'U key rest origin 1 6', 'R key rest origin 2 6', 'L key rest origin 1 6')] +
     [(f'{h} when sighelm-starspire,helm.t6 replace', rows(range(16), M2) + rows((0, 1, 2, 3), ('metal',)))
      for h in ('D key rest origin 1 7', 'U key rest origin 1 7', 'R key rest origin 2 7', 'L key rest origin 1 7')]),
    # the Sylvan T6 helm: its wing leaves' lit tips gilt; the spray's gilt veins (Metal 2) the trim they are
    (P('headwear', 'helm', 'winged.pxm'), 'sylvan-t6-wings', "the Sylvan T6 wing leaves' tips gilt (C12b)",
     [(f'{h} when sighelm-sylvan,helm.t6 replace', rows(range(16), ('dye',), (4,)))
      for h in ('D key rest origin 3 3', 'U key rest origin 3 3', 'R key rest origin 3 3', 'L key rest origin 1 3')]),
    (P('headwear', 'helm', 'winged_legend.pxm'), 'sylvan-t6-veins', "the Sylvan T6 spray's gilt veins, the trim they are (C12b)",
     [(f'{h} when sighelm-sylvan,helm.t6 replace', rows(range(12), M2))
      for h in ('D key rest origin -1 8', 'U key rest origin -1 8', 'R key rest origin -1 8', 'L key rest origin -2 8',
                'D key breeze origin -1 8', 'U key breeze origin -1 8')]),
    # the Imperial T6 legate: the crest's gilt holder (the crest box's base on the bowl)
    (P('headwear', 'helm', 'crested.pxm'), 'imperial-t6-holder', "the Imperial T6 crest's gilt holder (C12b)",
     [(f'{h} when sighelm-imperial,helm.t6', rows((10,), ('plume',)))
      for h in ('D key rest origin 3 12', 'U key rest origin 3 12', 'R key rest origin 3 13', 'L key rest origin 2 13')]),
]


# (item 4, C12b, batch 2) more of the heads' fittings as trim, the T5-T6 gems that show facing R (C12b: a glow pixel at T5-T6 in
# every facing; C13: the T6 aura lit by them), the T6 runes on the Highland chief's band, the clasps and chains of the signed
# cloaks, the Fjordfolk T3 roll's buckle, the Imperial T2 shoulder lames' glints (C12b: step-4 metal never falls T1 -> T2)
MORE_SPECS = [
    # the River morion's bowl foot gilt (the band over the brim's outline) and a gem at its front (facing R and L)
    (P('headwear', 'helm', 'morion.pxm'), 'river-t56-foot', "the River T5-T6 morion's gilt bowl foot and its front gem (C12b)",
     [(f'{h} when sighelm-river,helm.t5,!helm.t6 replace', rows((10,), ('metal',)), 'R', 'helm.t2')
      for h in ('D key rest origin 3 5', 'U key rest origin 3 5', 'R key rest origin 3 5', 'L key rest origin 3 5')] +
     [(f'{h} when sighelm-river,helm.t6 replace', rows((11,), ('metal',)), 'R', 'helm.t2')
      for h in ('D key rest origin 3 6', 'U key rest origin 3 6', 'R key rest origin 3 6', 'L key rest origin 3 6')] +
     [('R key rest origin 3 5 when sighelm-river,helm.t5,!helm.t6 replace', [(10, 16, 17)], 'G', 'helm.t3', None, True),
      ('L key rest origin 3 5 when sighelm-river,helm.t5,!helm.t6 replace', [(10, 3, 4)], 'G', 'helm.t3', None, True),
      ('R key rest origin 3 6 when sighelm-river,helm.t6 replace', [(11, 16, 17)], 'G', 'helm.t3', None, True),
      ('L key rest origin 3 6 when sighelm-river,helm.t6 replace', [(11, 3, 4)], 'G', 'helm.t3', None, True)]),
    # the Starspire ogive's cheek edges gilt under the band, and the band's front gem facing R and L
    (P('headwear', 'helm', 'crown.pxm'), 'star-t56-cheeks', "the Starspire T5-T6 cheek edges gilt, the band's front gem (C12b)",
     [(f'{h} when sighelm-starspire,helm.t5,!helm.t6 replace', rows((12, 13), ('metal',)), 'R', 'helm.t2')
      for h in ('D key rest origin 1 6', 'U key rest origin 1 6', 'R key rest origin 2 6', 'L key rest origin 1 6')] +
     [(f'{h} when sighelm-starspire,helm.t6 replace', rows((13, 14), ('metal',)), 'R', 'helm.t2')
      for h in ('D key rest origin 1 7', 'U key rest origin 1 7', 'R key rest origin 2 7', 'L key rest origin 1 7')] +
     [('R key rest origin 2 6 when sighelm-starspire,helm.t5,!helm.t6 replace', [(11, 15, 16)], 'G', 'helm.t3', None, True),
      ('L key rest origin 1 6 when sighelm-starspire,helm.t5,!helm.t6 replace', [(11, 2, 3)], 'G', 'helm.t3', None, True),
      ('R key rest origin 2 7 when sighelm-starspire,helm.t6 replace', [(12, 15, 16)], 'G', 'helm.t3', None, True),
      ('L key rest origin 1 7 when sighelm-starspire,helm.t6 replace', [(12, 2, 3)], 'G', 'helm.t3', None, True)]),
    # the Sun Temple T5 band's turquoise stud facing R and L
    (P('headwear', 'helm', 'masked_legend.pxm'), 'sun-t5-stud', "the Sun Temple T5 band's turquoise stud facing R and L (C12b)",
     [('R key rest origin 3 6 when sighelm-suntemple,helm.t5,!helm.t6 replace', [(3, 9, 10)], 'G', 'helm.t3', None, True)]),
    # the Highland chief's T6: the clan brooch's gem facing R and L; the runes along the leather band (N: the T6 rune tile's glow)
    (P('headwear', 'helm', 'kettle.pxm'), 'highland-t6-runes', "the Highland T6 brooch gem facing R and L, the band's runes (C12b, C13)",
     [('R key rest origin 2 8 when sighelm-highland,helm.t6 replace', [(9, 9, 10)], 'G', 'helm.t3', None, True),
      ('L key rest origin 2 8 when sighelm-highland,helm.t6 replace', [(9, 9, 10)], 'G', 'helm.t3', None, True)] +
     [(f'{f} key rest origin 2 8 when sighelm-highland,helm.t6 replace', rows((14,), ('leather',)), 'N', 'helm.t4') for f in 'DURL']),
    # the signed cloak's clasp and chain (Metal 2, never marked R): the trim they are, and the drape's top hem beside the clasps
    # gilt as the cloak's edges are (the Marsh T6, the Sun Temple T4)
    (P('cloak', 'noble_front.pxm'), 'cloak-chain', "the signed noble cloak's gilt clasps and chain, the trim they are (C12b)",
     [('D key rest origin 9 1 layer head', rows(range(6), M2) + [(1, 2, 3, ('cloak',)), (1, 15, 16, ('cloak',)), (2, 2, 3, ('cloak',)), (2, 15, 16, ('cloak',)), (3, 2, 3, ('cloak',)), (3, 15, 16, ('cloak',))], 'R', '',
       'sigcloak-marsh,cloak.t6'),
      ('D key rest origin 9 1 layer head', rows(range(6), M2) + [(1, 2, 3, ('cloak',)), (1, 15, 16, ('cloak',)), (2, 2, 3, ('cloak',)), (2, 15, 16, ('cloak',)), (3, 2, 3, ('cloak',)), (3, 15, 16, ('cloak',))], 'R', '',
       'sigcloak-suntemple,cloak.t4,!cloak.t5')]),
]
IMP_T2 = 'sigbody-imperial,armour.t2,!armour.t3'
SUN6 = 'sighelm-suntemple,helm.t6 replace'
AE = 'Å'   # masked_legend plume 2
C0, C2, C3, C4 = 'À', 'Â', 'Ã', 'Ä'   # crown.pxm metal 0, 2, 3 and 4
PIXEL_SPECS = [
    # (item 6, C12f: the T5 -> T6 head 5 % or more) the Sun Temple T6 fan's gaps between the centre plumes closed (gold-tipped as
    # their neighbours): +2 px facing D and +2 facing R, inside the fan's own outline (no cap moves)
    (P('headwear', 'helm', 'masked_legend.pxm'), 'sun-t6-fan-gaps', "the Sun Temple T6 fan's centre gaps closed (C12f head step)",
     [(f'D key rest origin 2 8 when {SUN6}', [(1, 7, '.', AE, 'R'), (1, 11, '.', AE, 'R')], 'helm.t5'),
      (f'U key rest origin 2 8 when {SUN6}', [(1, 7, '.', AE, 'R'), (1, 11, '.', AE, 'R')], 'helm.t5'),
      (f'D key breeze origin 2 8 when {SUN6}', [(0, 7, '.', AE, 'R'), (0, 11, '.', AE, 'R')], 'helm.t5'),
      (f'U key breeze origin 2 8 when {SUN6}', [(0, 7, '.', AE, 'R'), (0, 11, '.', AE, 'R')], 'helm.t5'),
      (f'R key rest origin 2 8 when {SUN6}', [(1, 4, '.', AE, 'R'), (1, 8, '.', AE, 'R')], 'helm.t5')]),
    # (item 6, C12f: the T4 -> T5 head 5 % or more) the Starspire T5 ogive's cheek pieces a pixel further round the face (two
    # rows) and their tips a pixel longer (the hood and the crystal shoulders hide most of the rest): inside the head box (the
    # T5 cap of 8 rows over the crown holds: a taller tip broke it)
    (P('headwear', 'helm', 'crown.pxm'), 'star-t5-cheeks', "the Starspire T5 cheek pieces a pixel further round the face (C12f head step)",
     [('D key rest origin 1 6 when sighelm-starspire,helm.t5,!helm.t6 replace',
       [(12, 3, C0, C3), (12, 4, '.', C0), (12, 14, C0, C2), (12, 13, '.', C0), (14, 1, C0, C3), (15, 1, '.', C0), (14, 16, C0, C2),
        (15, 16, '.', C0), (13, 3, C0, C3), (13, 4, '.', C0), (13, 14, C0, C2), (13, 13, '.', C0)], 'helm.t4'),
      ('R key rest origin 2 6 when sighelm-starspire,helm.t5,!helm.t6 replace',
       [(12, 16, C0, C2), (12, 15, '.', C0), (13, 15, '.', C0), (13, 16, C0, C2), (14, 16, '.', C0), (13, 2, C0, C3), (13, 3, C0, C3),
        (14, 2, '.', C0), (14, 3, '.', C0)], 'helm.t4'),
      ('L key rest origin 1 6 when sighelm-starspire,helm.t5,!helm.t6 replace',
       [(12, 2, C0, C2), (12, 3, '.', C0), (13, 3, '.', C0), (13, 2, C0, C2), (14, 2, '.', C0)], 'helm.t4')]),
    (P('headwear', 'cloth', 'hood.pxm'), 'star-t5-hood', "the Starspire T5 hood's star-white edges a row longer under the cheek pieces (C12f head step)",
     [('D key rest origin 0 0 when sighelm-starspire', [(14, 2, '.', '%'), (14, 13, '.', '!')], '', 'sighelm-starspire,helm.t5,!helm.t6'),
      ('R key rest origin -2 0 when sighelm-starspire', [(13, 5, '.', '$')], '', 'sighelm-starspire,helm.t5,!helm.t6')]),
    # the Imperial T2 Montefortino's brass cheek-guard hinges (Metal 2, kept by the leather helm: doll_paint helmLeather) polished
    (P('headwear', 'helm', 'nasal.pxm'), 'imperial-t2-hinges', "the Imperial T2 cheek guards' polished brass hinges (C12b step 4)",
     [('D key rest origin 0 -4 when sighelm-imperial,!helm.t3,helm.t2', [(2, 1, '&', 'Q'), (3, 0, '&', 'Q')], 'helm.t1'),
      ('R key rest origin 0 -5 when sighelm-imperial,!helm.t3,helm.t2', [(0, 0, '&', 'Q'), (0, 1, '&', 'Q'), (1, 10, '&', 'Q')], 'helm.t1')]),
    # the Imperial T2 shoulder lames: each lame's lit corner polished (step 4), the banded shoulders read as metal
    (P('armour', 'shared', 'pauldron_layered2_r.pxm'), 'imperial-t2-lames', "the Imperial T2 shoulder lames' polished corners (C12b step 4)",
     [('R key rest origin 0 2', [(1, 1, 'y', 'z'), (1, 2, 'y', 'z'), (1, 3, 'y', 'z'), (3, 1, 'y', 'z'), (3, 2, 'y', 'z')], '', IMP_T2),
      ('D key rest origin 2 2', [(1, 1, 'y', 'z'), (1, 2, 'y', 'z'), (3, 1, 'y', 'z')], '', IMP_T2)]),
    (P('armour', 'shared', 'pauldron_layered2_l.pxm'), 'imperial-t2-lames', "the Imperial T2 shoulder lames' polished corners (C12b step 4)",
     [('D key rest origin 2 2', [(1, 1, 'y', 'z'), (3, 1, 'y', 'z')], '', IMP_T2)]),
]


def plan():
    """(path, tag, description, builder, place)"""
    return [
        (P('headwear', 'helm', 'plumed.pxm'), 'steppe-t4-lit', 'the Steppe T4 spiked bowl facing D, its highlight carried up the '
         'left of the cone (C19 A3: the brass helm lit top-left)', steppe_t4_lit, 'top'),
        (P('armour', 'shared', 'pauldron_leaf5_r.pxm'), 'sylvan-leaf5-tip', 'the Sylvan T5 great leaf pauldron facing D, the last '
         'leaf\'s gilt tip lit 2 px across (C19 A7: 2-6 px speculars)', sylvan_leaf5_tip, 'end'),
    ] + [(path, tag, desc, regions_of(spec), 'end') for path, tag, desc, spec in TRIM_SPECS] + \
        [(path, tag, desc, marks_of(spec), 'end') for path, tag, desc, spec in MORE_SPECS] + \
        [(path, tag, desc, pixels_of(spec), 'end') for path, tag, desc, spec in PIXEL_SPECS]


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


def block(tag, desc, views):
    lines = [f'# {TAG} begin {tag}: {desc}']
    for head, marks, grid, regs in views:
        lines.append(head)
        lines += marks
        lines += grid
        if regs and any(c != '.' for r in regs for c in r[1:]):
            lines.append('regions')
            lines += regs
    lines.append(f'# {TAG} end {tag}')
    return lines


def build(path, text):
    src = own_strip(text.replace('\r\n', '\n').split('\n'))
    while src and src[-1] == '':
        src.pop()
    top, end = [], []
    for p, tag, desc, fn, place in plan():
        if os.path.normcase(p) != os.path.normcase(path):
            continue
        lines = block(tag, desc, fn(src))
        (top if place == 'top' else end).extend(lines + [''])
    i = next(n for n, s in enumerate(src) if s.startswith('view ') or s.startswith('# (P5b'))
    return src[:i] + top + src[i:] + ([''] + end[:-1] if end else []) + [''], top + end


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
        new, add = build(path, open(path, encoding='utf-8').read())
        open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(new))
        print(f'{os.path.relpath(path, ROOT)}: {len([s for s in add if s])} lines in {TAG} blocks')


def verify():
    bad = 0
    for path in touched():
        rel = os.path.relpath(path, ROOT).replace('\\', '/')
        old = own_strip(base_text(path).replace('\r\n', '\n').split('\n'))
        now = own_strip(open(path, encoding='utf-8').read().replace('\r\n', '\n').split('\n'))
        norm = lambda ls: [s for s in ls if s.strip()]
        if norm(old) != norm(now):
            print(f'FAIL {rel}: outside this script\'s blocks the map differs from {BASE}')
            bad += 1
        else:
            print(f'ok   {rel}: only {TAG} blocks added')
    return bad


if __name__ == '__main__':
    if '--verify' in sys.argv:
        sys.exit(1 if verify() else 0)
    if '--print' in sys.argv:
        for path in touched():
            print('\n'.join(build(path, open(path, encoding='utf-8').read())[1]))
        sys.exit(0)
    write()
