#!/usr/bin/env python3
"""tools/doll/p5b_fix_dune3.py (P5b round 3, the DUNE T3 kit; docs/STYLE_P5B.md 5 "Dune"; judge P5b2 r4 items 1b and 2): ADDED
views only, as tools/doll/p5b_fix_r3.py. Every block this script writes is tagged `# (P5b fix-dune3) begin <tag>` .. `end <tag>`;
no view drawn before this round changes a cell. Run it LAST (after p5b_fix_r3.py), then `node tools/doll/pxc.mjs`.

  python tools/doll/p5b_fix_dune3.py            write the blocks into rpg/art/doll/maps
  python tools/doll/p5b_fix_dune3.py --verify   each touched map is fc4afc6's but for this script's blocks (and fix-r3's)
  python tools/doll/p5b_fix_dune3.py --print    print the blocks

Every view is keyed on the Dune T3 kit (`sigbody-dune,armour.t3,!armour.t4`, or the kulah's `sighelm-dune,helm.t3,!helm.t4`):
the slice's kits, the other peoples and the Dune's other tiers draw as before.

The blocks:
  dune3-guard-sleeves (B11 SLIVER / HIPW / AREA, B12 BOOT on the drawn walk and run: the greatsword's, the staff's and the
                  crossbow's guard) the wide sleeves' bell hung 1-3 rows past the kaftan's hem over the legs, cutting them into
                  1-2 px slivers and narrowing the hip row: the bell ends at the hem (its lowest rows off, the mouth closed)
  dune3-bow-sleeves (C3d "the grip outside the fist": the bow's cast and draw facing R, the drawn walk) the near sleeve was traced
                  down the bowstring (FORGE-LIMBS's generator read the string as a forearm) and hung over the far fist on the bow:
                  the near bell rides the drawing forearm and stops short of the bow hand; at the full draw the far sleeve's
                  mouth and the near sleeve's run over the bow arm end at the wrist
  dune3-staff-carry (C3d: the staff carried in the run facing L) the near bell's mouth past the wrist hid the far fist on the
                  haft: cut at the wrist, as P5b2 cut the spear's (`when spear`)
  dune3-kaftan-slit (B12 BOOT: the man's staff run, the far heel a 2x2 crumb beside the staff's butt) the kaftan's split
                  (the Dune hem) opens at the back of the bell in the run's flare and the heel shows whole
  dune3-kulah     (C12c lt2 -> lt3 facing R / L: the figure shrank 542 -> 508 px; C3d: the mace's windup fist behind the head;
                  C3d h2: the staff's windup facing U floated over the spike) the kulah's turban: (R / L) its tail ('adhaba) let
                  down behind the aventail to the shoulders, on layer hairBack, behind the body and both arms (doll_paint: an
                  overlay naming a layer of its own draws there), P5b fix-r3's wrap cells over the windup fist taken off
                  (tools/doll/p5b_fix_r3.py); (U) the cloth wound up the dome's back to the spike's collar
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import p5b_fix_r2 as r2   # noqa: E402

P = r2.P
TAG = '(P5b fix-dune3)'
BASE = 'fc4afc6'

DUNE3B = 'sigbody-dune,armour.t3,!armour.t4'
DUNE3H = 'sighelm-dune,helm.t3,!helm.t4'


def head_parts(head):
    m = re.match(r'view (\S) key (\S+) origin (-?\d+) (-?\d+)((?: layer \S+)?(?: anchor \S+)?)\s*$', head)
    if not m:
        raise SystemExit(f'cannot read "{head}"')
    return m.group(1), m.group(2), int(m.group(3)), int(m.group(4)), m.group(5)


def base_head(base, facing, key):
    hs = [t.rstrip() for t in base if re.match(rf'view {facing} key {key} origin ', t) and ' when ' not in t]
    if len(hs) != 1:
        raise SystemExit(f'{facing} {key}: {len(hs)} base views')
    return hs[0]


def replace(base, facing, key, rows, cond, origin=None, regs=None):
    """a `replace` view of the base view (facing, key): `rows` its grid, at `origin` (the base's), its layer and anchor kept"""
    f, k, ox, oy, rest = head_parts(base_head(base, facing, key))
    if origin:
        ox, oy = origin
    w = max(len(r) for r in rows)
    grid = ['|' + r.ljust(w, '.') for r in rows]
    rg = ['|' + r.ljust(w, '.') for r in regs] if regs else []
    return (f'view {f} key {k} origin {ox} {oy}{rest} when {cond} replace', [], grid, rg)


def cut(base, facing, key, keep, cond):
    """the base view's first `keep` rows, its last kept row the bell's closed mouth (every drawn cell an outline)"""
    marks, grid, regs = r2.find_view(base, base_head(base, facing, key))
    rows = [r[1:] for r in grid[:keep]]
    rows[-1] = ''.join('v' if c not in '. ' else '.' for c in rows[-1])
    rg = [r[1:] for r in regs[:keep]] if regs else None
    return replace(base, facing, key, rows, cond, regs=rg)


def drop_cols(base, facing, key, n, cond):
    """the base view without its first `n` columns (facing L the mouth past the wrist), the new first column closed"""
    f, k, ox, oy, rest = head_parts(base_head(base, facing, key))
    marks, grid, regs = r2.find_view(base, base_head(base, facing, key))
    rows = [r[1 + n:] for r in grid]
    rows = [('v' + r[1:]) if r and r[0] not in '. ' else r for r in rows]
    return replace(base, facing, key, rows, cond, origin=(ox - n, oy))


def views_of(spec):
    def fn(base):
        return [it(base) for it in spec]
    return fn


# ---- dune3-guard-sleeves: (side, facing, key, rows kept), both bodies (their grids are one drawing)
GUARD = [('l', 'R', 'gs_guard', 6), ('r', 'R', 'gs_guard', 7), ('r', 'R', 'st_guard', 7), ('l', 'R', 'bw_xlow', 7),
         ('r', 'L', 'gs_guard', 7), ('r', 'L', 'st_guard', 7), ('l', 'L', 'bw_xlow', 8)]

# ---- dune3-bow-sleeves (both bodies)
# bw_low facing R: the near (right) forearm level at the chest, its cuff at x 3-4 (the shoulder anchor's), the bow hand at
# x 6-9, y 7-9: the bell hangs from the cuff and opens under the forearm, x 1-4, to y 7
BOW_LOW_R = ['..vv.',
             '..vy.',
             '..vx.',
             '.vyx.',
             'vyxw.',
             'vxwwv',
             '.vvv.']
# bw_full facing R: the near sleeve's run over the bow arm (x 3-14) ends at the bow wrist, x 11, its mouth closed
BOW_FULL_R = ['vv........vvv',
              'vy..vvvvvvyxv',
              'vx..xxxxxxxxv',
              'vx..xxxxxxxxv',
              'vw..vvvvwwwwv',
              'vv......vvvvv',
              'vv...........']
# the far (left) sleeve's mouth at the full draw: its cells over the bow hand (x 12-14, y -2..1) off
BOW_FULL_L = ['vv...',
              'vx...',
              'vx...',
              'vx...',
              '.v...',
              '.vwwv',
              '.vwwv',
              '..vvv']


# the staff's run carry facing L (the man's pM): the spear's cut (P5b2, `view L key pM origin 6 -2 when spear replace`), cell
# for cell
STAFF_PM_L = ['v......',
              'vvvvvvv',
              'vxxxxwv',
              'vxxxwwv',
              'v...vvv']


# ---- dune3-kulah: the turban's tail ('adhaba), facing R (the back on the left), in the T3 kulah's grid (origin 3 1, the head
# box at columns 3-18; C3b lets a plume 3 columns past it): from the knot behind the band (P5b fix-r3's wrap, (2,8)) down the
# back of the aventail to the shoulders, on layer hairBack (behind the body and both arms: a fist raised behind the head, near
# or far, stays before it): outline 0 on the open edge, the lit fold 3 / 2, the shadow 1 against the mail, the end frayed. Rows
# 0-8 lie under the base kulah's erase cells: the tail starts at row 9. plume 0-4 the turban's cloth (veil.pxm's legend, the
# dune-kulah block)
TAIL_R = [(9, 0, '0'), (9, 1, '3'), (10, 0, '0'), (10, 1, '3'), (11, 0, '0'), (11, 1, '2'), (12, 0, '0'), (12, 1, '3'),
          (13, 0, '0'), (13, 1, '3'), (14, 0, '0'), (14, 1, '2'), (14, 2, '1'), (15, 0, '0'), (15, 1, '2'), (15, 2, '1'),
          (16, 1, '0'), (16, 2, '0')]
# facing U (the back toward us): the cloth wound up the dome's back to the spike's collar, a pixel each side of it (row 3)
COLLAR_U = [(3, 7, '0'), (3, 12, '0')]


def overlay(base, facing, key_head, cells, cond, layer=None):
    """a `when` overlay of the view `key_head` (its facing, key and origin): '.' but the cells [(row, x, char), ...], on its own
    layer when given; the grid as wide as the view's (or the cells'), as tall as the cells need"""
    head = r2.find_head(base, key_head)
    m = re.match(r'view (\S) key (\S+) origin (-?\d+) (-?\d+)', head)
    marks, grid, regs = r2.find_view(base, head)
    w = max([len(grid[0]) - 1] + [x + 1 for y, x, c in cells])
    h = max(y for y, x, c in cells) + 1
    rows = [['.'] * w for _ in range(h)]
    for y, x, c in cells:
        rows[y][x] = c
    lay = f' layer {layer}' if layer else ''
    return (f'view {m.group(1)} key {m.group(2)} origin {m.group(3)} {m.group(4)}{lay} when {cond}', [],
            ['|' + ''.join(r) for r in rows], [])


def mirror(cells, w):
    return [(y, w - 1 - x, c) for y, x, c in cells]


KULAH_BASE = 'helm.t3,!helm.t4,!variant-1,!variant-2 replace'

# ---- dune3-kaftan-slit (B12 BOOT "visible sliver" on the man's drawn run, run1 facing R and L): the staff's butt crosses the
# trailing far boot, and the bell's hem over its top left the heel a 2x2 crumb between them. The Dune's kaftan is split (STYLE_P5B
# 5, hem SPLIT): in the run's flare its back opens in a slit two pixels across at the hem (the man's flare view, origin 12 0 facing
# R, the back on the left; L mirrored), its edges outlined, and the heel shows whole through it
def slit(base, facing, x0):
    """the man's flare view with the hem's slit at columns x0, x0 + 1 (row 4 open, its sides outlined; row 3 over it the top)"""
    marks, grid, regs = r2.find_view(base, base_head(base, facing, 'flare'))
    rows = [list(r[1:]) for r in grid]
    rg = [list(r[1:]) for r in regs] if regs else None
    rows[3][x0] = rows[3][x0 + 1] = 'v'
    rows[4][x0 - 1] = rows[4][x0 + 2] = 'v'
    rows[4][x0] = rows[4][x0 + 1] = '.'
    if rg:
        rg[4][x0] = rg[4][x0 + 1] = '.'
    return replace(base, facing, 'flare', [''.join(r) for r in rows], DUNE3B, regs=[''.join(r) for r in rg] if rg else None)


def plan_specs():
    out = []
    for body in ('man', 'woman'):
        for side in ('l', 'r'):
            spec = [(lambda b, F=F, K=K, n=n: cut(b, F, K, n, DUNE3B)) for s, F, K, n in GUARD if s == side]
            out.append((P('armour', body, f'sleeve_wide_{side}.pxm'), 'dune3-guard-sleeves',
                        "the Dune T3 kit's wide sleeves in the drawn guard end at the kaftan's hem (B11 slivers, hip row)", views_of(spec)))
        out.append((P('armour', body, 'sleeve_wide_r.pxm'), 'dune3-bow-sleeves',
                    "the Dune T3 kit's near sleeve at the bow's low hold and full draw off the bow hand (C3d)",
                    views_of([lambda b: replace(b, 'R', 'bw_low', BOW_LOW_R, DUNE3B, origin=(-1, -1)),
                              lambda b: replace(b, 'R', 'bw_full', BOW_FULL_R, DUNE3B)])))
        out.append((P('armour', body, 'sleeve_wide_l.pxm'), 'dune3-bow-sleeves',
                    "the Dune T3 kit's far sleeve at the bow's full draw off the bow hand (C3d)",
                    views_of([lambda b: replace(b, 'R', 'bw_full', BOW_FULL_L, DUNE3B)])))
    out.append((P('armour', 'man', 'sleeve_wide_l.pxm'), 'dune3-staff-carry',
                "the Dune T3 kit's near bell cut at the wrist in the staff's run carry facing L (C3d), as the spear's",
                views_of([lambda b: replace(b, 'L', 'pM', STAFF_PM_L, 'staff,' + DUNE3B, origin=(6, -2))])))
    out.append((P('armour', 'man', 'skirt_bell.pxm'), 'dune3-kaftan-slit',
                "the Dune T3 kit's kaftan split at the back of the hem in the man's run (B12 boot sliver beside the staff's butt)",
                views_of([lambda b: slit(b, 'R', 4), lambda b: slit(b, 'L', 16)])))
    out.append((P('headwear', 'helm', 'veil.pxm'), 'dune3-kulah',
                "the Dune T3 kulah's turban tail let down the back to the shoulders behind the body (C12c lt2 -> lt3; C3d), the "
                "cloth wound to the spike's collar facing U (C3d h2)",
                views_of([lambda b: overlay(b, 'R', 'R key rest origin 3 1 when ' + KULAH_BASE, TAIL_R, DUNE3H + ',helm.t1,helm.t2', 'hairBack'),
                          lambda b: overlay(b, 'L', 'L key rest origin 2 1 when ' + KULAH_BASE, mirror(TAIL_R, 21), DUNE3H + ',helm.t1,helm.t2', 'hairBack'),
                          lambda b: overlay(b, 'U', 'U key rest origin 2 1 when ' + KULAH_BASE, COLLAR_U, DUNE3H + ',helm.t2')])))
    return out


def plan():
    return [(path, tag, desc, fn, 'end') for path, tag, desc, fn in plan_specs()]


def merged_plan():
    """one block per (path, tag): the builders of a path and tag joined"""
    seen = {}
    order = []
    for path, tag, desc, fn, place in plan():
        k = (os.path.normcase(path), tag)
        if k not in seen:
            seen[k] = [path, tag, desc, [fn], place]
            order.append(k)
        else:
            seen[k][3].append(fn)
    out = []
    for k in order:
        path, tag, desc, fns, place = seen[k]
        out.append((path, tag, desc, (lambda b, fns=fns: [v for f in fns for v in f(b)]), place))
    return out


def build(path, text):
    old_tag, old_plan = r2.TAG, r2.plan
    r2.TAG, r2.plan = TAG, merged_plan
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


def r3_strip(lines):
    old = r2.TAG
    r2.TAG = '(P5b fix-r3)'
    try:
        return r2.own_strip(lines)
    finally:
        r2.TAG = old


def verify():
    import subprocess
    bad = 0
    for path in touched():
        rel = os.path.relpath(path, r2.ROOT).replace(os.sep, '/')
        r = subprocess.run(['git', '-C', r2.ROOT, 'show', f'{BASE}:{rel}'], capture_output=True, text=True, encoding='utf-8')
        # (the kulah's map also carries P5b fix-r3's blocks, whose Dune T3 wrap this round trims in tools/doll/p5b_fix_r3.py; its
        # own --verify checks them: here both scripts' blocks are left out)
        old = r3_strip(own_strip(r.stdout.replace('\r\n', '\n').split('\n')))
        now = r3_strip(own_strip(open(path, encoding='utf-8').read().replace('\r\n', '\n').split('\n')))
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
