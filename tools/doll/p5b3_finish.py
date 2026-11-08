#!/usr/bin/env python3
"""tools/doll/p5b3_finish.py (P5b round 3, FINISH; docs/STYLE_P5B.md 2, 3, 4, 10.1; judge P5b2 r4 items 1 and 3): ADDED views
only, as tools/doll/p5b_fix_r3.py. Every block this script writes is tagged `# (P5b3 finish) begin <tag>` .. `end <tag>`: new
`when` overlays (a view of their own: the condition plus a flag it already implies, at an explicit origin) that redraw or add
cells; no view drawn before this round changes a cell. Run it LAST, after p5b_fix_r3.py, then `node tools/doll/pxc.mjs`.

  python tools/doll/p5b3_finish.py            write the blocks into rpg/art/doll/maps
  python tools/doll/p5b3_finish.py --verify   each touched map is fc4afc6's but for this script's blocks
  python tools/doll/p5b3_finish.py --print    print the blocks

Never the style slice's frames (Fjordfolk, Imperial and Jade at T1/T3/T5: slice_same 567/567, ruling R1 moves none of them).
The blocks:
  fjordfolk-t2-cone (judge item 3, S1: the T2 Fjordfolk and Heartland men facing D were figure-XOR twins, 21 of 21.44 px,
                  since fix r3 cleared a hair pixel above the cap's rim) the T2 spangen cap's round crown taken in at the
                  shoulders of the dome, facing D and U: the bowl a low cone over the fleece roll (the T3 spangenhelm's own
                  profile, in leather; the Heartland soft cap stays round), the parting carried by the head, no pixel added
                  (a roll a pixel proud of the bowl was tried first: C12f T2 shell 0 columns past the head box, and the T2
                  area 0.05 under the slice's T3, refused it). The slice's T1, T3 and T5 untouched
  dune-t3-wrap    (C12c: the Dune lt2 -> lt3 facing R shrank 542 -> 508 px, 526 -> 490 the woman's: the T2 turban's bulk gone
                  into the T3 kulah's narrow bowl) the kulah's teal turban wound higher round the bowl, a rounded wrap from
                  which the dome's crown and needle spike rise (the T2 turban kept on the steel, the T5-T6 turbans foreshadowed),
                  in every facing; the mail aventail's foot flared a pixel at the back facing R / L (the family's flare to
                  the shoulders, as it is drawn facing D)
  river-t1-plain  (C12d: the River woman's lt1 D 51 colours, 48 allowed) the T1 chaperon drawn plain: the lapis roll in three
                  steps (its 2 px sheen and its darkest step folded into their neighbours) and the linen crown's top step
                  folded into the step below it (2-6 RGB apart): three colours fewer, the same drawing
  suntemple-t2-mantle (C12a: the Sun Temple man s3 lt1 -> lt2 facing R 14 % of the gear pixels differ, 15 wanted: his T2 body
                  is his T1's, so the step is the feather mantle and the cap, and a cap is no gear) the T2 feather mantle grown:
                  each ragged tip a pixel longer over the upper arm (lit, then the dark tip), both shoulders, every facing and
                  key (STYLE_P5B 5: TALL feathered shoulders; the T3 mantle's length foreshadowed)
  marsh-t3-seams  (C14: the Marsh man's T3 moss against iron 11 luma apart at step 2, 4 contacts facing D and 3 facing R; 18
                  luma or 2 contacts wanted) the moss cape's and scarf's edges where they meet the iron drawn as the dark seam
                  (STYLE_P5B 2.7: surfaces meet with intent), the man's signed T3 kit only
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import p5b_fix_r2 as r2   # noqa: E402

P = r2.P
TAG = '(P5b3 finish)'
BASE = 'fc4afc6'

HEAD_RE = re.compile(r'view (\S) key (\S+)(?: origin (-?\d+) (-?\d+))?(?: layer (\S+))?(?: anchor \S+)?(?: when (.*?)( replace)?)?$')


def parse_head(head):
    m = HEAD_RE.match(head)
    if not m:
        raise SystemExit(f'cannot read "{head}"')
    return m


def view_origin(lines, head):
    """a view's origin (its own, or its base's: the view of its facing and key with no condition)"""
    m = parse_head(head)
    if m.group(3) is not None:
        return int(m.group(3)), int(m.group(4))
    for s in lines:
        if s.startswith('view '):
            b = parse_head(s.rstrip())
            if b.group(1) == m.group(1) and b.group(2) == m.group(2) and not b.group(6) and b.group(3) is not None:
                return int(b.group(3)), int(b.group(4))
    raise SystemExit(f'{head}: no origin')


def cells_of(lines, head, mapping):
    """anchor-relative cells {(dx, dy): new} for every cell of the view `head` whose character is a key of `mapping`"""
    ox, oy = view_origin(lines, head)
    marks, grid, regs = r2.find_view(lines, head)
    out = {}
    for y, row in enumerate(grid):
        for x, c in enumerate(row[1:]):
            if c in mapping:
                out[(x - ox, y - oy)] = mapping[c]
    return out


def grid_cells(lines, head, cells):
    """anchor-relative cells for [(row, col, old, new), ...] in the view `head`'s grid (old checked; '.' off the grid)"""
    ox, oy = view_origin(lines, head)
    marks, grid, regs = r2.find_view(lines, head)
    rows = [r[1:] for r in grid]
    out = {}
    for y, x, old, new in cells:
        have = rows[y][x] if 0 <= y < len(rows) and 0 <= x < len(rows[y]) else '.'
        if have != old:
            raise SystemExit(f'{head}: cell ({x},{y}) is {have!r}, expected {old!r}')
        out[(x - ox, y - oy)] = new
    return out


def overlay(facing, key, cond, cells, layer=None):
    """a view of its own over the part, its grid just big enough for the anchor-relative cells {(dx, dy): char}"""
    x0 = min(dx for dx, dy in cells)
    y0 = min(dy for dx, dy in cells)
    w = max(dx for dx, dy in cells) - x0 + 1
    h = max(dy for dx, dy in cells) - y0 + 1
    g = [['.'] * w for _ in range(h)]
    for (dx, dy), c in cells.items():
        g[dy - y0][dx - x0] = c
    lay = f' layer {layer}' if layer else ''
    return (f'view {facing} key {key} origin {-x0} {-y0}{lay} when {cond}', [], ['|' + ''.join(r) for r in g], [])


def canvas(cells, ax, ay):
    """{(x, y): c} canvas cells of a probe (tools/preview/fingerprint_gallery --figs, EMB_FIGS_DUMP) -> anchor-relative"""
    return {(x - ax, y - ay): c for (x, y), c in cells.items()}


def mirror_rel(cells, axis):
    """R's anchor-relative cells for L (the base L is R mirrored about the column `axis`, relative to the anchor)"""
    return {(axis - dx, dy): c for (dx, dy), c in cells.items()}


# ------------------------------------------------------------------------------------------------ fjordfolk-t2-cone
# The T2 soft cap under the FR1 cap2 overlay (the fleece roll, the nasal, the neck curtain), facing D and U: the round crown
# taken in a pixel at each end of rows 1 and 3 of the dome (canvas, the head anchor at 72, 71: rows 71 and 73), so the bowl
# climbs 4, 6, 8, 10, 14 px from its tip to the roll, the low cone of the T3 spangenhelm (the slice's) in leather; each new
# left end is the lit outline (leather 1). Inside the head box (C12f T2: side 0) and no pixel added (C12f T2 -> T3 0.05).
FJ2 = 'sighelm-fjordfolk,helm.t2,!helm.t3'
FJ2_CONE = {(76, 71): '-', (83, 71): '-', (74, 73): '-', (85, 73): '-', (75, 73): 'g'}


def fj2_cone(base):
    cells = canvas(FJ2_CONE, 72, 71)
    return [overlay(f, 'rest', FJ2 + ',helm.t1', cells) for f in 'DU']


# ------------------------------------------------------------------------------------------------ dune-t3-wrap
DUNE3 = 'sighelm-dune,helm.t3,!helm.t4'
# canvas cells read off the probe (the head anchor at 72, 71; facing R the back on the left): the wrap's new rows round the
# bowl (plume 0-4, the kulah band's legend '0'-'4'; lit on the left), and facing R the aventail's foot at the back (metal:
# veil.pxm's v-z, steps 0-4)
WRAP_R = {(74, 74): '1', (75, 74): '3', (84, 74): '0', (72, 75): '1',
          (73, 75): '1', (74, 75): '3', (84, 75): '1', (85, 75): '0',
          (72, 76): '1', (87, 76): '0', (86, 75): '0', (71, 77): '1'}
FOOT_R = {(71, 84): 'v'}   # (C3b: a shell pixel 1 column past the 16 px head box at T3; the turban's plume cells may reach 3)
WRAP_D = {(76, 74): '1', (83, 74): '0',
          (74, 75): '1', (75, 75): '3', (84, 75): '1', (85, 75): '0',
          (73, 76): '1', (86, 76): '0'}


def dune_wrap(base):
    r = canvas({**WRAP_R, **FOOT_R}, 72, 71)
    d = canvas(WRAP_D, 72, 71)
    # L is R mirrored: the R view's grid (origin 3 1, 21 wide) against L's (origin 2 1): x_L = 20 - x_R, so relative to the
    # anchor dx_L = (20 - (dx_R + 3)) - 2 = 15 - dx_R
    cond = DUNE3 + ',helm.t2,helm.t1'
    return [overlay('R', 'rest', cond, r), overlay('L', 'rest', cond, mirror_rel(r, 15)),
            overlay('D', 'rest', cond, d), overlay('U', 'rest', cond, d)]


# ------------------------------------------------------------------------------------------------ river-t1-plain
RIV1 = 'sighelm-river,!helm.t2'


def river_plain(base):
    out = []
    for f in 'DURL':
        roll = r2.find_head(base, next(t.rstrip() for t in base if t.startswith(f'view {f} key rest') and t.rstrip().endswith(f'when {RIV1}')))
        crown = next(t.rstrip() for t in base if t.startswith(f'view {f} key rest origin') and ' when ' not in t)
        drawn = cells_of(base, roll, {c: c for c in set(''.join(r2.find_view(base, roll)[1])) if c not in '.|'})
        cells = {k: v for k, v in cells_of(base, crown, {'U': 'T'}).items() if k not in drawn}   # (the crown where the roll leaves it)
        cells.update(cells_of(base, roll, {'*': '&', '!': '$'}))
        out.append(overlay(f, 'rest', RIV1 + ',helm.t1', cells))
    return out


# ------------------------------------------------------------------------------------------------ suntemple-t2-mantle
# The feather mantle's views (feather.2, both shoulders, every facing, standing keys): its foot row of ragged tips ('5', cloth1 0) a
# step lit ('6') and a dark tip a pixel under each: the feathers a pixel longer over the upper arm
SUN2 = 'sigbody-suntemple,armour.t2,!armour.t3'


def sun_mantle(base):
    out = []
    for t in base:
        if not t.startswith('view ') or ' when ' in t:
            continue
        head = t.rstrip()
        m = parse_head(head)
        if m.group(2) == 'lie':   # (lying, the feathers lie along the body: unchanged)
            continue
        marks, grid, regs = r2.find_view(base, head)
        rows = [r[1:] for r in grid]
        drawn = [y for y, r in enumerate(rows) if r.strip('.')]
        if not drawn:
            continue
        foot = max(drawn)
        cells = []
        for x, c in enumerate(rows[foot]):
            if c == '5':
                cells.append((foot, x, '5', '6'))
                cells.append((foot + 1, x, '.', '5'))
        if cells:
            out.append(overlay(m.group(1), m.group(2), SUN2, grid_cells(base, head, cells), m.group(5)))
    return out


# ------------------------------------------------------------------------------------------------ marsh-t3-seams
MARSH3 = 'sigbody-marsh,armour.t3,!armour.t4,!woman'
# the probe (marsh:3, the man): the cape's moss edge cells against the pauldron, the wide sleeve and the skirt, and the
# scarf's against the mail and the shield rim, each at the view that draws it: a lit outline cell (step 0, lifted to step 1 by
# the selout, so still moss against iron) drawn in ink, a drawn step-1 cell in the scarf's own step 0 (no clear neighbour: kept)
CAPE_D = 'view D key rest origin 12 2 when cloak.t3,!cloak.t5 replace'
CAPE_R = 'view R key rest origin 8 1 when cloak.t3,!cloak.t5,!walk,!woman,armour.t1 replace'
SCARF_D = 'view D key rest origin 5 -1 layer headwear when layer-scarf,!beard replace'
SCARF_R = 'view R key rest origin 7 0 layer headwear when layer-scarf,!beard replace'


def marsh_cape(base):
    d = grid_cells(base, CAPE_D, [(3, 2, 'Q', 'K'), (8, 0, 'Q', 'K'), (10, 0, 'Q', 'K')])
    r = grid_cells(base, CAPE_R, [(9, 2, 'S', 'K')])
    return [overlay('D', 'rest', 'cloak.t3,!cloak.t5,' + MARSH3, d),
            overlay('R', 'rest', 'cloak.t3,!cloak.t5,!walk,armour.t1,' + MARSH3, r)]


def marsh_scarf(base):
    d = grid_cells(base, SCARF_D, [(1, 5, 'n', 'm')])
    r = grid_cells(base, SCARF_R, [(2, 1, 'm', 'K')])
    return [overlay('D', 'rest', 'layer-scarf,!beard,' + MARSH3, d, 'headwear'),
            overlay('R', 'rest', 'layer-scarf,!beard,' + MARSH3, r, 'headwear')]


SPECS = [
    (P('headwear', 'helm', 'spangen.pxm'), 'fjordfolk-t2-cone', "the Fjordfolk T2 cap's crown taken in to the spangen's low cone, facing D and U (S1 T2: the Heartland soft cap's twin)", fj2_cone),
    (P('headwear', 'helm', 'veil.pxm'), 'dune-t3-wrap', "the Dune T3 kulah's turban wound higher round the bowl, the aventail's foot flared at the back (C12c lt2 -> lt3 R)", dune_wrap),
    (P('headwear', 'cloth', 'headscarf.pxm'), 'river-t1-plain', "the River T1 chaperon plain: the roll in three lapis steps, the crown's top step folded (C12d lt1 colours)", river_plain),
    (P('armour', 'shared', 'pauldron_feather2_r.pxm'), 'suntemple-t2-mantle', "the Sun Temple T2 feather mantle's tips a pixel longer (C12a lt1 -> lt2)", sun_mantle),
    (P('armour', 'shared', 'pauldron_feather2_l.pxm'), 'suntemple-t2-mantle', "the Sun Temple T2 feather mantle's tips a pixel longer (C12a lt1 -> lt2)", sun_mantle),
    (P('cloak', 'cape.pxm'), 'marsh-t3-seams', "the Marsh man's T3 moss cape's dark seam against the iron (C14)", marsh_cape),
    (P('neck', 'scarf.pxm'), 'marsh-t3-seams', "the Marsh man's T3 moss scarf's dark seam against the iron (C14)", marsh_scarf),
]


def plan():
    return [(path, tag, desc, fn, 'end') for path, tag, desc, fn in SPECS]


def build(path, text):
    old_tag, old_plan = r2.TAG, r2.plan
    r2.TAG, r2.plan = TAG, plan
    try:
        return r2.build(path, text)
    finally:
        r2.TAG, r2.plan = old_tag, old_plan


def touched():
    out = []
    for p, *_ in plan():
        if p not in out:
            out.append(p)
    return out


def own_strip(lines):
    old = r2.TAG
    r2.TAG = TAG
    try:
        return r2.own_strip(lines)
    finally:
        r2.TAG = old


def write():
    for path in touched():
        new, add = build(path, open(path, encoding='utf-8').read())
        open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(new))
        print(f'{os.path.relpath(path, r2.ROOT)}: {len([s for s in add if s])} lines in {TAG} blocks')


def verify():
    bad = 0
    for path in touched():
        rel = os.path.relpath(path, r2.ROOT).replace(os.sep, '/')
        r = subprocess.run(['git', '-C', r2.ROOT, 'show', f'{BASE}:{rel}'], capture_output=True, text=True, encoding='utf-8')
        old = own_strip(r.stdout.replace('\r\n', '\n').split('\n'))
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
