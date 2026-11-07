#!/usr/bin/env python3
"""tools/doll/p5b_fix_r3.py (P5b2 fix round 3; docs/STYLE_P5B.md 2, 4, 5; judge P5b2 r3 item 1): ADDED views only, as
tools/doll/p5b_fix_r2.py (whose helpers it uses). Every block this script writes is tagged `# (P5b fix-r3) begin <tag>` ..
`end <tag>`: new `when` overlays (a view of their own: the condition plus a flag it already implies) that redraw or add cells;
no view drawn before this round changes a cell. Run it LAST, after p5b_fix_r2.py, then `node tools/doll/pxc.mjs`.

  python tools/doll/p5b_fix_r3.py            write the blocks into rpg/art/doll/maps
  python tools/doll/p5b_fix_r3.py --verify   each touched map is HEAD~'s (bd1a4e4) but for this script's blocks
  python tools/doll/p5b_fix_r3.py --print    print the blocks

The blocks (C12 the ladder reads, source L; never the style slice's frames: Fjordfolk, Imperial and Jade at T1/T3/T5):
  dune-t3-tail    (C12c: lt2 -> lt3 facing R/L the figure shrank 542 -> 498 px: the T2 turban's wide cloth gone into the T3
                  kulah's narrow bowl) the kulah's turban cloth wound up the dome's back and a pixel proud at the brow, a stub of
                  its tail at the nape (498 -> 508 px; a tail down the back covered the greatsword's raised fists, C3d, and closed
                  a pocket at the neck, C3a: cut back; the rest of the lt2 -> lt3 step is left open)
  imperial-t2-edge (C12b step 4: the Imperial woman's lt1 -> lt2 D step-4 metal 7 -> 5) the T2 near shoulder's lit edge
                  polished down both lames (the woman's: one 3 px glint where two 1 px glints stood; A8 caps a figure at three glints; the man keeps his hinges)
  suntemple-t4-edge, marsh-t6-edge (C12b cloak trim: the Sun Temple man's T4 D 4.9 %, the Marsh woman's T6 D 9.4 %) the
                  signed noble cloak's gilt edge down each side (drawn in Metal 2, marked R only along the hem) the trim it is
  marsh-t3-band   (C14: the Marsh T3 war hat's moss band 11 luma off its iron at step 2) the band and its tassels bound in reed (leather)
  marsh-t3-rushes (C12c: lt2 -> lt3 facing R/L 536 -> 504 px: the T2 reed hat's brim and its reed gone into the T3 war hat)
                  two bulrushes tucked in the war hat's band, leaning back (the T2 hat's reed grown; the T5 bulrush crown
                  foreshadowed): moss stems (plume), brown heads (leather)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import p5b_fix_r2 as r2   # noqa: E402

P = r2.P
TAG = '(P5b fix-r3)'
BASE = 'bd1a4e4'

DUNE3 = 'sighelm-dune,helm.t3,!helm.t4'
# the tail, facing R (the back on the left): from the band down the back of the mail curtain to the neck, its end clear of
# the neck's hollow (a tail down past it closed a pocket there, C3a); the wrap wound up the dome's back and a pixel proud at
# the brow; plume 0-4 the
# turban's cloth (veil.pxm's legend); inside C3b's T3 cap (3 columns past the head box)
TAIL_R = [(8, 1, '-', '0'), (8, 2, '-', '1'), (9, 1, '.', '0'), (9, 2, '-', '0'),
          (4, 7, '-', '0'), (5, 6, '-', '0'), (5, 7, 'v', '1'), (6, 4, '-', '0'), (6, 5, '-', '1'), (7, 3, '-', '0'), (7, 4, '-', '1'),
          (6, 16, '-', '1'), (6, 17, '-', '0'), (7, 17, '-', '1'), (7, 18, '-', '0')]


def draw(base, head, cells, cond, origin):
    """an overlay of `cells` [(row, x, old, new), ...] on the view `head` (old checked against its grid; a cell off the grid,
    above or left of it, grows the grid, the origin moving with it; right or below, the grid grows), keyed `cond`, at
    `origin` (the view's own, or the base's it defaults to)"""
    marks, grid, regs = r2.find_view(base, r2.find_head(base, head))
    rows = [r[1:] for r in grid]
    w = max(len(r) for r in rows)
    up = max([0] + [-c[0] for c in cells])
    left = max([0] + [-c[1] for c in cells])
    w2 = left + max([w] + [c[1] + 1 for c in cells])
    h2 = up + max([len(rows)] + [c[0] + 1 for c in cells])
    src = [['.'] * w2 for _ in range(h2)]
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            src[y + up][x + left] = ch
    out = [['.'] * w2 for _ in range(h2)]
    for y, x, old, new in cells:
        have = src[y + up][x + left]
        if have != old:
            raise SystemExit(f'{head}: cell ({x},{y}) is {have!r}, expected {old!r}')
        out[y + up][x + left] = new
    return (f'view {head[0]} key {head.split()[2]} origin {origin[0] + left} {origin[1] + up} when {cond}', [],
            ['|' + ''.join(r) for r in out], [])


def draws_of(spec):
    """a builder: spec [(head, cells, cond, origin), ...]"""
    def fn(base):
        return [draw(base, *it) for it in spec]
    return fn


def mirror(cells, w):
    """R's cells for L (the base L is R mirrored): x -> w-1-x, the back on the right"""
    return [(y, w - 1 - x, old, new) for y, x, old, new in cells]


MARSH3 = 'sighelm-marsh,helm.t3,!helm.t4'
# the bulrushes facing R (the back on the left): a stem of moss (plume R S) leaning back from the band, its brown head
# (leather g h); the kettle's legend: Q-U plume 0-4, person f-j leather 0-4; '-' erase rows over the crown (row 3)
RUSH_R = [(-2, 5, '.', 'S'), (-1, 5, '.', 'g'), (0, 5, '-', 'h'), (1, 6, '-', 'R'), (2, 7, '-', 'R'),
          (-1, 11, '.', 'S'), (0, 11, '-', 'g'), (1, 11, '-', 'h'), (2, 10, '-', 'R'),
          (1, 14, '-', 'S'), (2, 13, '-', 'R')]
RUSH_D = [(-2, 5, '.', 'S'), (-1, 5, '.', 'g'), (0, 5, '-', 'h'), (1, 6, '-', 'R'), (2, 6, '-', 'R'),
          (-1, 14, '.', 'S'), (0, 14, '-', 'g'), (1, 14, '-', 'h'), (2, 13, '-', 'R')]

# (C12b, the cloak's trim 6-30 % at T4, 10-30 % at T5-T6, Metal 2 marked R: the Sun Temple man's T4 D 6 of 122 px, the Marsh
# woman's T6 D 6 of 64) the signed noble cloak's gilt edge (its Metal 2 line down each side, drawn but marked R only along the
# lower hem) marked as the trim it is, its whole length, in every facing and key of the view the look draws
def cloak_edges(view_cond, extra):
    def fn(base):
        out = []
        for t in base:
            if t.startswith('view ') and t.rstrip().endswith(f'when {view_cond} replace'):
                n = len(r2.find_view(base, t.rstrip())[1])
                v = r2.region_overlay(base, t.rstrip(), r2.all_rows(n, ('metal2',)), 'R', '', extra + ',' + view_cond)
                if v:
                    out.append(v)
        return out
    return fn


EDGE_SPECS = [
    (P('cloak', 'noble.pxm'), 'suntemple-t4-edge', "the Sun Temple T4 signed cloak's gilt edges, the trim they are (C12b)",
     cloak_edges('cloak.t4,!cloak.t5,skirt-bell', 'sigcloak-suntemple,armour.t1,!skirt-strips')),
    (P('cloak', 'noble.pxm'), 'marsh-t6-edge', "the Marsh T6 signed cloak's gilt edges, the trim they are (C12b)",
     cloak_edges('cloak.t5,cloak.t6', 'sigcloak-marsh,armour.t1,!mythic')),
]

# (C14: Accent against Metal 11 luma apart at step 2, 16-17 contacts, on the Marsh T3 facing D and R) the war hat's moss band
# (Accent) ran between two rows of the hat's iron, 11 luma off it: the band bound in reed (leather browns), the moss kept in
# the cape, the scarf and the new bulrushes
BAND = {'m': 'f', 'n': 'g', 'o': 'h', 'p': 'i'}


def band_cells(base, head, row):
    """the band's row and the tassels' rows under the brim (10-11): accent cells to leather"""
    marks, grid, regs = r2.find_view(base, r2.find_head(base, head))
    return [(y, x, c, BAND[c]) for y in (row, 10, 11) for x, c in enumerate(grid[y][1:]) if c in BAND]


def band_of(spec):
    def fn(base):
        return [draw(base, head, band_cells(base, head, row), cond, org) for head, row, cond, org in spec]
    return fn


BAND_SPECS = [
    (P('headwear', 'helm', 'kettle.pxm'), 'marsh-t3-band', "the Marsh T3 war hat's band bound in reed (C14 accent against iron)",
     [(f + ' key rest when ' + MARSH3 + ' replace', 7, MARSH3 + ',helm.t1', o) for f, o in (('D', (2, 1)), ('U', (2, 1)), ('R', (3, 1)), ('L', (2, 1)))]),
]

DRAW_SPECS = [
    (P('headwear', 'helm', 'veil.pxm'), 'dune-t3-tail', "the Dune T3 kulah's turban tail down the back (C12c lt2 -> lt3 R)",
     [('R key rest origin 3 1', TAIL_R, DUNE3 + ',helm.t2', (3, 1)),
      ('L key rest origin 2 1', mirror(TAIL_R, 21), DUNE3 + ',helm.t2', (2, 1))]),
    (P('headwear', 'helm', 'kettle.pxm'), 'marsh-t3-rushes', "the Marsh T3 war hat's two bulrushes (C12c lt2 -> lt3 R)",
     [('R key rest when ' + MARSH3 + ' replace', RUSH_R, MARSH3 + ',helm.t2', (3, 1)),
      ('L key rest when ' + MARSH3 + ' replace', mirror(RUSH_R, 21), MARSH3 + ',helm.t2', (2, 1)),
      ('D key rest when ' + MARSH3 + ' replace', RUSH_D, MARSH3 + ',helm.t2', (2, 1)),
      ('U key rest when ' + MARSH3 + ' replace', RUSH_D, MARSH3 + ',helm.t2', (2, 1))]),
]

IMP_T2 = 'sigbody-imperial,armour.t2,!armour.t3'
PIXEL_SPECS = [
    # (C12b step 4: the Imperial woman's T1 -> T2 D 7 -> 5: her hair hides the cheek guards' polished hinges the man shows,
    # so the figure's three glints (envelope A8's cap) are the near shoulder's two lames and the shield boss) the near shoulder's
    # lit edge polished down its two lames (the shadow cell between them lit): one glint of 3 px where two of 1 px stood
    (P('armour', 'shared', 'pauldron_layered2_r.pxm'), 'imperial-t2-edge', "the Imperial T2 near shoulder's lit edge polished down both lames (C12b step 4)",
     [('D key rest origin 2 2', [(2, 1, 'w', 'z')], '', IMP_T2 + ',woman')]),
]


def plan():
    return [(path, tag, desc, r2.pixels_of(spec), 'end') for path, tag, desc, spec in PIXEL_SPECS] +         [(path, tag, desc, draws_of(spec), 'end') for path, tag, desc, spec in DRAW_SPECS] +         [(path, tag, desc, fn, 'end') for path, tag, desc, fn in EDGE_SPECS] +         [(path, tag, desc, band_of(spec), 'end') for path, tag, desc, spec in BAND_SPECS]


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
    import subprocess
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
