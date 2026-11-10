#!/usr/bin/env python3
"""tools/doll/p5e_royalcloak.py (P5e ROYALCLOAK2; docs/STYLE_P5B.md 2.5 and decision 11, docs/PLAN.md D126): ADDED views only.

The owner, 2026-11-10, on P5d's royal guard cloak (the long noble cloak's T5 views worn on the T4 everyday kit): "some of the
front models don't quite match what's going on in the back. They are really far spread out, and looks like wings not a cape."
The T5 views were cut for the T5 plate's great pauldrons: facing D their shoulder mantle stands 3-6 px past a T4 body's arms
from the collar to the waist and stops there (wings), facing R / L a 6 px slab from the chest to the ground. This script draws
the royal guard's own cloak views, fitted to the T4 body, as `when royalguard replace` views at the top of cloak/noble's views
(the first replacing view whose condition holds wins, so they beat the T5 views for the royal guard and no one else), and the
clasp and chain for cloak/noble_front:

  facing D   one cape hung behind the shoulders (keys rest, idle, wide; one drawing per skirt, `D_VIEWS`): its collar behind
             the neck, its sides behind the upper arms (x -10 / 9 from the neck) and, below the hands, on the shoulder tips'
             line (x -11 / 10) for the long skirts, within 2 px of the legs (x -9 / 8, the T5 cap) on the rows each body's legs
             show; never past the arms. Only a narrow band shows, behind the shoulders and beside the legs; the hem just above
             the boots, two folds from the waist. The run's flare narrower, its hem lifted
  facing R/L hung from behind the neck down the back, 2-3 px behind the shoulder blades and 3 behind the waist, the hem over
             the calves; the walk's wide key kicks the hem a pixel back, the run's flare streams it back from the shoulders in
             two tails (folds along its length). L is each R drawing mirrored and relit from the top left (`relit`)
  facing U   unchanged (the approved T5 back); the kneel, sit, floor, ride and lie keys keep the noble cloak's base views
  the front  (cloak/noble_front, `royalguard`) a small gilt clasp at each collarbone and a short chain between them sagging a
             row at the breastbone (the rank read facing D), under the face, the beard and the pauldrons; facing R / L the near
             clasp at the chest's front

The views do not fit T4's envelope caps (facing D the band beside the legs stands 2 px past them, cap 0; facing R / L the cape
hangs up to 6 columns behind the body's box, cap 4): the cloak's resolved tier stays T5 (doll_resolve ladderOf, D80).
Every block is tagged `# (P5e royalcloak) begin <tag>` .. `end <tag>`; the script strips its own blocks and writes them
again (idempotent); no view drawn before it changes a cell. The cloak's people colour, its gilt trim (Metal 2 on the cloak's
parts) and the gilt border inside its outline come from the compositor as in P5d (doll_paint cloakGilt, doll_ramps).

  python tools/doll/p5e_royalcloak.py                 write the blocks into rpg/art/doll/maps
  python tools/doll/p5e_royalcloak.py --verify        each touched map is BASE's (226212f, P5d) but for this script's blocks
  python tools/doll/p5e_royalcloak.py --print         print the blocks
  python tools/doll/p5e_royalcloak.py --maps DIR      write into DIR (a scratch copy of maps/, the look-and-adjust loop)
then `node tools/doll/pxc.mjs`.
"""
import os
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
MAPS = os.path.join(ROOT, 'rpg', 'art', 'doll', 'maps')
TAG = '(P5e royalcloak)'
BASE = '226212f'   # P5d: the maps before this script
COND = 'royalguard'

NOBLE = ('cloak', 'noble.pxm')
FRONT = ('cloak', 'noble_front.pxm')


# ------------------------------------------------------------------------------------------------ the views
# Each entry: (map, tag, description, [(facing, key, origin x, origin y, layer or None, rows, regions or None), ...]).
# Rows are the grid ('.' keeps what is below); the legend is the map's own (cloak/noble: Q-U cloak 0-4, n accent 1 (the
# lining), N O P metal2 1-3 (the gilt trim)). Regions: R the trim (the dye band).
def grid(rows):
    """a grid's rows ('|' prefix optional), each padded to the widest"""
    rows = [r[1:] if r.startswith('|') else r for r in rows]
    w = max(len(r) for r in rows)
    return [r.ljust(w, '.') for r in rows]


# ------------------------------------------------------------------------------------------------ facing D
# Drawn on x -11..10 from the neck anchor (grid column 11) and rows -2..17 (grid row 2): the cape seen from the front hangs
# BEHIND the body, so what shows is its collar over the shoulders' back, a band down each side where the body is narrower
# than the shoulder tips, and the hem between and beside the legs. Its sides never pass the shoulder tips (x -11 / 10, the
# T4 pauldrons' line); on the rows the legs show (each body's skirt ends at its own row) they keep within 2 px of the legs
# (x -9 / 8: the T5 cap, 2 px past a leg facing D). One drawing per skirt (the row the legs show from) and the tall mass
# (the narrow shoulders): `cape_d` paints each from its sides, row by row.
def cape_d(sides, hem, hem_row=17):
    """sides: {row: (left x, right x)} for rows -2..hem_row-1; hem: (left x, right x) of the bottom outline (row hem_row). The outline Q
    on each side, the light T inside the left edge, the shade R inside the right, the cloth S, and two folds from the waist
    that lean out toward the hem (R with a lit T ridge on their left); the compositor gilds the cloth beside the outline"""
    rows = []
    top = min(sides)
    for y in range(top, hem_row + 1):
        line = ['.'] * 22
        if y == hem_row:
            for x in range(hem[0], hem[1] + 1):
                line[x + 11] = 'Q'
        else:
            l, r = sides[y]
            for x in range(l, r + 1):
                line[x + 11] = 'S'
            if y == top:
                for x in range(l, r + 1):
                    line[x + 11] = 'Q'
            else:
                line[l + 11] = 'Q'
                line[r + 11] = 'Q'
                if l + 1 < r:
                    line[l + 12] = 'T'
                    line[r + 10] = 'R'
                if y >= 9:
                    lean = (y - 9) // 3
                    for fx in (-4 - lean, 3 + lean):
                        if l + 2 < fx < r - 1:
                            line[fx + 11] = 'R'
                            if line[fx + 10] == 'S':
                                line[fx + 10] = 'T'
        rows.append(''.join(line))
    return rows


COLLAR = {-2: (-6, 5), -1: (-8, 7), 0: (-9, 8), 1: (-10, 9)}
D_OY = 2   # the grid row of the neck


def sides_of(top, lower, from_row, **rows):
    """the collar (rows -2..1) and the shoulder sides `top` (rows 2..from_row-1), then `lower` (rows from_row..16); rows
    r<n>=(l, r) set row n (m<n> row -n)"""
    s = dict(COLLAR)
    for y in range(2, 17):
        s[y] = top if y < from_row else lower
    for k, v in rows.items():
        s[-int(k[1:]) if k[0] == 'm' else int(k[1:])] = v
    return s


SH = (-10, 9)      # inside the shoulder tips (most T4 bodies' pauldrons reach x -11 / 10)
TIP = (-11, 10)    # on the shoulder tips' line
LEG = (-9, 8)      # within 2 px of the legs (x -7..6)
D_VIEWS = [   # (extra condition, rows): the first that holds wins
    ('mass-tall', cape_d(sides_of(LEG, LEG, 2, r1=(-9, 8)), LEG)),    # Dune, Starspire: the narrow shoulders
    ('skirt-strips', cape_d(sides_of(SH, TIP, 10), (-10, 9))),                    # the legs from row 18: the sides on the tips' line
    ('skirt-bell', cape_d(sides_of(SH, TIP, 10, r16=LEG), LEG)),       # the legs from row 16
    ('skirt-hauberk', cape_d(sides_of(SH, LEG, 12), LEG)),                        # the legs from row 14
    ('skirt-tassets', cape_d(sides_of(SH, LEG, 12), LEG)),                        # the legs from row 12-14
    (None, cape_d(sides_of(SH, LEG, 10), LEG)),                                   # split skirts and the rest: the legs from row 10
]


# ------------------------------------------------------------------------------------------------ facing R and L
# R: x -16..-1 (the anchor at grid column 16), rows -1..17 (grid row 1): hung from behind the neck, its back edge 2-3 px
# behind the back (x -12 at the shoulder blades, -13 from the waist), the hem over the calves; the cells under the body are
# cloth closed by an outline the body hides. The walk's wide key kicks the hem back a pixel; the run's flare streams it back
# from the shoulders in two tails. L: each R drawing mirrored and lit again from the top left (relit: its outer edge in shade).
R_REST = grid([
    '.......QQQQQ....',
    '......QTTSSSQ...',
    '.....QTSSSSSSQ..',
    '.....QTSSSSSSSQ.',
    '.....QTSRSSSSSQ.',
    '.....QTSRSSSSSQ.',
    '.....QTSRSSSSSQ.',
    '....QTSSRSSSSSQ.',
    '....QTSRSSSSSSQ.',
    '....QTSRSSSSSSQ.',
    '....QTSRSSSSSSQ.',
    '....QTSRSSRSSSQ.',
    '....QTSRSSRSSSQ.',
    '....QTRSSRSSSSQ.',
    '....QTRSSRSSSSQ.',
    '....QTRSSRSSSSQ.',
    '....QTRSSRSSSSQ.',
    '.....QQQQQQQQQ..',
])
R_WIDE = grid([
    '.......QQQQQ....',
    '......QTTSSSQ...',
    '.....QTSSSSSSQ..',
    '.....QTSSSSSSSQ.',
    '.....QTSRSSSSSQ.',
    '.....QTSRSSSSSQ.',
    '.....QTSRSSSSSQ.',
    '....QTSSRSSSSSQ.',
    '....QTSRSSSSSSQ.',
    '....QTSRSSSSSSQ.',
    '....QTSRSSSSSSQ.',
    '...QTSRSSRSSSSQ.',
    '...QTSRSSRSSSSQ.',
    '...QTRSSRSSSSSQ.',
    '...QTRSSRSSSSSQ.',
    '...QTRSSRSSSSSQ.',
    '....QTRSRSSSQQ..',
    '.....QQQQQQQ....',
])
R_FLARE = grid([
    '.......QQQQ.....',
    '......QTTSSQ....',
    '.....QTSSSSSQ...',
    '.....QTSRSSSSQ..',
    '....QTSSRSSSSQ..',
    '....QTSSRSSRSQ..',
    '...QTSRSRSSRSQ..',
    '...QTSRSRSSRSQ..',
    '..QTSSRSRSSRSQ..',
    '..QTSSRSRSSRSQ..',
    '.QTSRSRSRSSRQ...',
    '.QTSRSRSRSQQ....',
    '..QQRQRQQQ......',
    '....Q.Q.........',
])


def relit(rows):
    """an R drawing for L: mirrored and lit again from the top left: the cloth inside each row's left outline takes the
    light (T), the cloth inside its right outline the shade (R); the folds (R) keep their shade"""
    out = []
    for r in rows:
        c = list(r[::-1])
        for i in range(1, len(c) - 1):
            if c[i - 1] == 'Q' and c[i] in 'RS':
                c[i] = 'T'
            elif c[i + 1] == 'Q' and c[i] in 'TS' and c[i - 1] != 'Q':
                c[i] = 'R'
        out.append(''.join(c))
    return out


def side_views(key, rows, ox, oy):
    """the R view and its relit L view (the L origin: the mirrored anchor column)"""
    w = len(rows[0])
    return [('R', key, ox, oy, None, rows, None, None), ('L', key, w - ox, oy, None, relit(rows), None, None)]


D_FLARE = cape_d(sides_of(LEG, (-8, 7), 6, r1=(-9, 8)), (-7, 6), hem_row=15)   # the run: streamed back, the hem lifted


# ------------------------------------------------------------------------------------------------ the front (cloak/noble_front)
# The rank read facing D: a small gilt clasp at each collarbone (Metal 2: the compositor gilds it) and a short chain between
# them, sagging a row at the breastbone, each clasp ringed in the cloak's outline; it lies on the head layer (under the face,
# the beard and the helm's cheeks, which may cover the chain's middle). Facing R / L the near clasp at the chest's front.
# Drawn on x -5..4 (anchor column 5), rows 0..3 below the neck, between the pauldrons; facing R at x 2..5.
FRONT_D = grid([
    '.QQ....QQ.',
    'QPON..NPOQ',
    'QONQONQONQ',
    '.QQ....QQ.',
])
FRONT_R = grid([
    '..QQ.',
    '.QPOQ',
    '.QONQ',
    '..QQ.',
])


def plan():
    d = []
    for key in ('rest', 'idle', 'wide'):
        for extra, rows in D_VIEWS:
            d.append(('D', key, 11, D_OY, None, rows, None, extra))
    d.append(('D', 'flare', 11, D_OY, None, D_FLARE, None, None))
    for key, rows, ox, oy in (('rest', R_REST, 16, 1), ('idle', R_REST, 16, 1), ('wide', R_WIDE, 16, 1), ('flare', R_FLARE, 16, 1)):
        d += side_views(key, rows, ox, oy)
    f = [('D', 'rest', 5, 0, 'head', FRONT_D, None, None)] + [v for v in side_views('rest', FRONT_R, -1, 0)]
    return [(NOBLE, 'royal-cape', "the royal guard's cape fitted to the T4 body, facing D, R and L (P5e)", d),
            (FRONT, 'royal-clasp', "the royal guard's gilt clasps and chain at the collar (P5e)", f)]


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
    for facing, key, ox, oy, layer, rows, regs, extra in views:
        cond = COND + (',' + extra if extra else '')
        head = f'view {facing} key {key} origin {ox} {oy}' + (f' layer {layer}' if layer else '') + f' when {cond} replace'
        w = max(len(r) for r in rows)
        lines.append(head)
        lines += ['|' + r.ljust(w, '.') for r in rows]
        if regs and any(c != '.' for r in regs for c in r):
            lines.append('regions')
            lines += ['|' + r.ljust(w, '.') for r in regs]
    lines.append(f'# {TAG} end {tag}')
    return lines


def build(rel, text):
    src = own_strip(text.replace('\r\n', '\n').split('\n'))
    while src and src[-1] == '':
        src.pop()
    top = []
    for m, tag, desc, views in plan():
        if m != rel:
            continue
        top += block(tag, desc, views) + ['']
    i = next(n for n, s in enumerate(src) if s.startswith('view ') or s.startswith('# (P5'))
    return src[:i] + top + src[i:] + [''], top


def touched():
    out = []
    for m, *_ in plan():
        if m not in out:
            out.append(m)
    return out


def base_text(rel):
    r = subprocess.run(['git', '-C', ROOT, 'show', f'{BASE}:rpg/art/doll/maps/{rel[0]}/{rel[1]}'], capture_output=True, text=True,
                       encoding='utf-8')
    if r.returncode:
        raise SystemExit(f'git show {BASE}:{rel} failed')
    return r.stdout


def write(maps):
    for rel in touched():
        path = os.path.join(maps, *rel)
        new, add = build(rel, open(path, encoding='utf-8').read())
        open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(new))
        print(f'{rel[0]}/{rel[1]}: {len([s for s in add if s])} lines in {TAG} blocks')


def verify(maps):
    bad = 0
    for rel in touched():
        path = os.path.join(maps, *rel)
        old = own_strip(base_text(rel).replace('\r\n', '\n').split('\n'))
        now = own_strip(open(path, encoding='utf-8').read().replace('\r\n', '\n').split('\n'))
        norm = lambda ls: [s for s in ls if s.strip()]
        if norm(old) != norm(now):
            print(f'FAIL {rel[0]}/{rel[1]}: outside this script\'s blocks the map differs from {BASE}')
            bad += 1
        else:
            print(f'ok   {rel[0]}/{rel[1]}: only {TAG} blocks added')
    return bad


if __name__ == '__main__':
    maps = MAPS
    if '--maps' in sys.argv:
        maps = sys.argv[sys.argv.index('--maps') + 1]
    if '--verify' in sys.argv:
        sys.exit(1 if verify(maps) else 0)
    if '--print' in sys.argv:
        for rel in touched():
            print('\n'.join(build(rel, open(os.path.join(maps, *rel), encoding='utf-8').read())[1]))
        sys.exit(0)
    write(maps)
