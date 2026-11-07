#!/usr/bin/env python3
"""tools/doll/p5b_fix_r1.py (P5b2 fix round 1, the lead; docs/STYLE_P5B.md 2.4, 4; judge P5b2 r1 items 4, 5, 7): the elite
heads' fixes over the views tools/doll/p5b_sig_A2.py, p5b_sig_B2.py and p5b_sig_C2.py write (their `# (P5b SIG-A2)`,
`# (P5b SIG B2)` and `# (P5b SIG C2)` blocks). Run it after any of those three (it is idempotent: every operation reads the
block as it stands and leaves a block that already holds its rule alone). No other view's cells change.

  python tools/doll/p5b_fix_r1.py            apply, then report
  python tools/doll/p5b_fix_r1.py --report   report only

The operations:
  cap      (C12f, doll_envelope.h: T5 8 rows over the crown, T6 10) a T5-T6 head's view whose crest, plume or horns rise past its
           tier's cap loses the rows just under its tip (the tip's own shape kept, the crest a row shorter); the crown is the
           head box's top, two rows under the helm anchor (the view's origin)
  fill     (C3a) a pocket the head's drawing leaves closed (a 1-5 px gap between two feathers, the jaguar's ears, the comb and
           the brim) takes the outline step of the cells round it: the seam reads as drawn, not a hole to the grass
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MAPS = os.path.join(HERE, '..', '..', 'rpg', 'art', 'doll', 'maps')
BLOCK = re.compile(r'^# \(P5b SIG[- ](A2|B2|C2)\) (begin|end) ([^:\s]+)')
CAP = {5: 8, 6: 10}
# the blocks whose own closed pockets are holes in the figure (rpg_test --doll-hole-cells: the Highland chief's three feathers
# meet over a 1 px gap facing R and L); elsewhere a view's closed clear cells show the face or the hair under the head (a veil's
# eye slit, a mask's mouth) and stay clear
FILL_BLOCKS = {'A2:highland-chief6'}
CUT_TIP = {'C2:river-t5-plume'}   # (cap: the tip's rows go, not the crest's middle)
# the pockets two of a head's parts close between them (rpg_test --doll-hole-cells on the arms matrix's m9 looks, idle frame
# 0): (block, facings, cells relative to the helm anchor), filled in that block's views of those facings (every key) with the
# outline step of the drawn cells round them: the jaguar's ears and the T5 fan meet over a row (Sun Temple), the River
# captain's plume and comb, the Starspire star's stalk on the crown, the Marsh bog king's reed brim over its collar
FILL_CELLS = [
    ('C2:suntemple-t5-fan', 'DU', [(5, -1), (6, -1), (7, -1), (8, -1), (9, -1)]),
    ('C2:suntemple-t5-fan', 'R', [(6, -1)]),
    ('C2:suntemple-t5-fan', 'L', [(9, -1)]),
    ('C2:river-t6-plume', 'D', [(4, -3), (5, -3), (5, -2)]),
    ('C2:river-t6-plume', 'U', [(10, -3), (11, -3), (10, -2)]),
    ('C2:river-t6-plume', 'R', [(2, -4), (2, -3), (2, -2)]),
    ('C2:river-t6-plume', 'L', [(13, -4), (13, -3), (13, -2)]),
    ('C2:river-t6', 'R', [(-2, 3)]),
    ('C2:river-t6', 'L', [(17, 3)]),
    ('C2:starspire-t6-star', 'R', [(10, -4)]),
    ('C2:starspire-t6-star', 'L', [(5, -4)]),
    ('B2:marsh-bogking5', 'L', [(2, 15)]),
    ('B2:marsh-bogking6', 'L', [(2, 15)]),
    ('B2:marsh-bogking5', 'U', [(16, 9), (16, 10)]),
    ('B2:marsh-bogking6', 'U', [(16, 9), (16, 10), (16, 11)]),
]


def read(path):
    return open(path, encoding='utf-8').read().split('\n')


def views_of(lines):
    """[(i, head, grid_i0, grid_i1, regions_i0, regions_i1, block)] for every view inside an A2/B2/C2 block"""
    out = []
    blk = None
    i = 0
    while i < len(lines):
        m = BLOCK.match(lines[i])
        if m:
            blk = (m.group(1) + ':' + m.group(3)) if m.group(2) == 'begin' else None
            i += 1
            continue
        if blk and lines[i].startswith('view '):
            j = i + 1
            while j < len(lines) and not lines[j].startswith('|') and not lines[j].startswith('view ') and not BLOCK.match(lines[j]):
                j += 1
            g0 = j
            while j < len(lines) and lines[j].startswith('|'):
                j += 1
            g1 = j
            r0 = r1 = None
            if j < len(lines) and lines[j].strip() == 'regions':
                r0 = j + 1
                j = r0
                while j < len(lines) and lines[j].startswith('|'):
                    j += 1
                r1 = j
            out.append((i, lines[i], g0, g1, r0, r1, blk))
            i = j
            continue
        i += 1
    return out


def base_origin(lines, facing, key):
    """the origin of the map's base view of a facing and key (no `when`)"""
    for ln in lines:
        m = re.match(r'^view (\S) key (\S+) origin (-?\d+) (-?\d+)\s*$', ln)
        if m and m.group(1) == facing and m.group(2) == key:
            return int(m.group(3)), int(m.group(4))
    return None


def parse_head(h):
    m = re.match(r'^view (\S)(?: key (\S+))?(?: origin (-?\d+) (-?\d+))?(.*)$', h)
    facing, key = m.group(1), m.group(2) or 'rest'
    origin = (int(m.group(3)), int(m.group(4))) if m.group(3) else None
    cond = ''
    w = re.search(r' when (\S+)', h)
    if w:
        cond = w.group(1)
    return facing, key, origin, cond


def tier_of(cond):
    t = None
    for tok in cond.split(','):
        m = re.match(r'^helm\.t(\d)$', tok)
        if m:
            t = int(m.group(1)) if t is None else max(t, int(m.group(1)))
    return t


def cap_view(lines, v, report):
    i, head, g0, g1, r0, r1, blk = v
    facing, key, origin, cond = parse_head(head)
    tier = tier_of(cond)
    if tier not in CAP or blk == 'A2:imperial-legate6':   # (the legate's crest is redrawn in p5b_sig_A2.py: a transverse crest)
        return None
    if origin is None:
        origin = base_origin(lines, facing, key)
    if origin is None:
        return None
    rows = [l[1:] for l in lines[g0:g1]]
    top = next((y for y, r in enumerate(rows) if r.strip('. -')), None)
    if top is None:
        return None
    over = origin[1] + 2 - top
    k = over - CAP[tier]
    if report:
        print('  cap %-40s %s %-6s T%d over %2d (cap %d)%s' % (blk, facing, key, tier, over, CAP[tier], ' -> cut %d' % k if k > 0 else ''))
    if k <= 0:
        return None
    # the crest's most redundant rows go (each the row of the crest zone, under its tip and over the crown, whose drawn cells
    # differ least from the row under it: a plume's straight stalk, a fan's even middle), the tip's own row and the crown's
    # kept: grid and regions alike; the origin and the marks move up with the rows
    crown = origin[1] + 2
    w = max(len(r) for r in rows)
    def mask(y):
        r = rows[y].ljust(w, '.')
        return [c not in '. -' for c in r]
    cand = []
    for y in range(top + 1, max(top + 1, min(crown - 1, len(rows) - 1))):
        a, b = mask(y), mask(y + 1)
        cand.append((sum(1 for x in range(w) if a[x] != b[x]), -y, y))
    cand.sort()
    cut = sorted(c[2] for c in cand[:k])
    if blk in CUT_TIP:   # (a curled plume: a middle row moved its curl onto the comb and closed a pocket under it; the tip goes)
        cut = list(range(top, top + k))
    if len(cut) < k:
        return None
    # (the regions block sits after the grid: its rows go first, so the grid's indexes hold)
    for a, b in ((r0, r1), (g0, g1)):
        if a is None:
            continue
        for y in sorted(cut, reverse=True):
            del lines[a + y]
    return k, cut[0]


def apply_caps(path, report):
    lines = read(path)
    changed = 0
    while True:
        vs = views_of(lines)
        done = True
        for v in vs:
            i, head, g0, g1, r0, r1, blk = v
            facing, key, origin, cond = parse_head(head)
            explicit = origin is not None
            if origin is None:
                origin = base_origin(lines, facing, key)   # (an overlay on the base's origin: written out when its rows move)
            if origin is None:
                continue
            res = cap_view(lines, v, report and not changed)
            if res:
                k, first = res
                ox, oy = origin
                eol = '\r' if lines[i].endswith('\r') else ''
                hd = lines[i].rstrip('\r')
                if explicit:
                    hd = re.sub(r' origin (-?\d+) (-?\d+)', ' origin %d %d' % (ox, oy - k), hd, count=1)
                else:
                    hd = re.sub(r'^(view \S(?: key \S+)?)', lambda m: '%s origin %d %d' % (m.group(1), ox, oy - k), hd, count=1)
                lines[i] = hd + eol
                # the view's marks under the cut rows move up with them
                for j in range(i + 1, g0):
                    m = re.match(r'^mark (\S+) (-?\d+) (-?\d+)(\r?)$', lines[j])
                    if m and int(m.group(3)) > first:
                        lines[j] = 'mark %s %s %d%s' % (m.group(1), m.group(2), int(m.group(3)) - k, m.group(4))
                changed += k
                done = False
                break
        if done:
            break
    if changed:
        open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines))
    return changed


# (C12f's column caps, doll_envelope.h) a T5-T6 head part's reach past the 16-column head box (from the helm anchor's column):
# the River morion's boat brim and the Fjordfolk jarl's horns 3 columns, the Highland chief's brim and the Sun Temple and
# Starspire shells 2 (T6) and 2 (T5); the most redundant column of the reach goes, the tip's own kept (as the rows above)
COL_CAP = {'A2:fjordfolk-jarl6': 3, 'A2:highland-chief6': 2, 'C2:river-t5': 3, 'C2:river-t6': 3, 'C2:suntemple-t6-fan': 2,
           'C2:starspire-t5': 2, 'C2:starspire-t6': 2}


def cap_cols_view(lines, v):
    """returns (cols deleted left of the box, right of it) after deleting them, else None"""
    i, head, g0, g1, r0, r1, blk = v
    if blk not in COL_CAP:
        return None
    facing, key, origin, cond = parse_head(head)
    if origin is None:
        origin = base_origin(lines, facing, key)
    if origin is None:
        return None
    rows = [l[1:].rstrip('\r') for l in lines[g0:g1]]
    w = max(len(r) for r in rows)
    rows = [r.ljust(w, '.') for r in rows]
    drawn = [x for x in range(w) if any(r[x] not in '. -' for r in rows)]
    if not drawn:
        return None
    x0, x1 = origin[0], origin[0] + 15
    cap = COL_CAP[blk]
    kl = max(0, (x0 - drawn[0]) - cap)
    kr = max(0, (drawn[-1] - x1) - cap)
    if not kl and not kr:
        return None
    def col(x):
        return [r[x] not in '. -' for r in rows]
    def redundant(lo, hi, k):
        cand = []
        for x in range(lo, hi):
            a, b = col(x), col(x + 1)
            cand.append((sum(1 for y in range(len(rows)) if a[y] != b[y]), x))
        cand.sort()
        return [c[1] for c in cand[:k]]
    cut = []
    if kl:
        cut += redundant(drawn[0] + 1, x0 - 1, kl)   # (between the reach's tip and the box)
    if kr:
        cut += redundant(x1 + 1, drawn[-1] - 1, kr)
    if len(cut) < kl + kr:
        return None
    for a, b in ((g0, g1), (r0, r1)):
        if a is None:
            continue
        for y in range(a, b):
            eol = '\r' if lines[y].endswith('\r') else ''
            r = list(lines[y][1:].rstrip('\r').ljust(w, '.'))
            for x in sorted(cut, reverse=True):
                del r[x]
            lines[y] = '|' + ''.join(r) + eol
    left = sum(1 for x in cut if x < x0)
    return left, len(cut) - left


def apply_col_caps(path):
    lines = read(path)
    n = 0
    while True:
        done = True
        for v in views_of(lines):
            res = cap_cols_view(lines, v)
            if res:
                left, right = res
                i = v[0]
                facing, key, origin, cond = parse_head(v[1])
                explicit = origin is not None
                if origin is None:
                    origin = base_origin(lines, facing, key)
                eol = '\r' if lines[i].endswith('\r') else ''
                hd = lines[i].rstrip('\r')
                if explicit:
                    hd = re.sub(r' origin (-?\d+) (-?\d+)', ' origin %d %d' % (origin[0] - left, origin[1]), hd, count=1)
                else:
                    hd = re.sub(r'^(view \S(?: key \S+)?)', lambda m: '%s origin %d %d' % (m.group(1), origin[0] - left, origin[1]), hd, count=1)
                lines[i] = hd + eol
                for j in range(i + 1, v[2]):
                    m = re.match(r'^mark (\S+) (-?\d+) (-?\d+)(\r?)$', lines[j])
                    if m:
                        lines[j] = 'mark %s %d %s%s' % (m.group(1), int(m.group(2)) - left, m.group(3), m.group(4))
                n += left + right
                done = False
                break
        if done:
            break
    if n:
        open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines))
    return n


def legend_of(lines):
    """{char: (slot, step)} of the map's person legend(s) and its own legend lines"""
    leg = {}
    uses = []
    for ln in lines:
        t = ln.split()
        if len(t) >= 2 and t[0] == 'use':
            uses += t[1:]
    for u in uses:
        lp = os.path.join(MAPS, 'legend', u + '.legend')
        if os.path.exists(lp):
            for ln in read(lp):
                t = ln.split()
                if len(t) >= 4 and t[0] == 'legend' and t[3].lstrip('-').isdigit():
                    leg[t[1]] = (t[2], int(t[3]))
    for ln in lines:
        t = ln.split()
        if len(t) >= 4 and t[0] == 'legend' and t[3].lstrip('-').isdigit():
            leg[t[1]] = (t[2], int(t[3]))
    return leg


def fill_view(lines, v, leg, maxpx=5):
    """fill the view's own closed pockets (4-connected clear cells that never reach the grid's edge, maxpx or fewer) with the
    outline step of the cells round them (the lowest step among their drawn neighbours; its regions cell left clear)"""
    i, head, g0, g1, r0, r1, blk = v
    rows = [list(l[1:].rstrip('\r')) for l in lines[g0:g1]]
    if not rows:
        return 0
    w = max(len(r) for r in rows)
    for r in rows:
        r += ['.'] * (w - len(r))
    h = len(rows)
    clear = lambda x, y: rows[y][x] in '. '
    seen = [[False] * w for _ in range(h)]
    n = 0
    for y0 in range(h):
        for x0 in range(w):
            if not clear(x0, y0) or seen[y0][x0]:
                continue
            st = [(x0, y0)]
            seen[y0][x0] = True
            cells = []
            border = False
            while st:
                x, y = st.pop()
                cells.append((x, y))
                if x in (0, w - 1) or y in (0, h - 1):
                    border = True
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if 0 <= nx < w and 0 <= ny < h and clear(nx, ny) and not seen[ny][nx]:
                        seen[ny][nx] = True
                        st.append((nx, ny))
            if border or len(cells) > maxpx:
                continue
            nb = []
            for x, y in cells:
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if 0 <= nx < w and 0 <= ny < h and not clear(nx, ny) and rows[ny][nx] != '-':
                        nb.append(rows[ny][nx])
            nb = [c for c in nb if c in leg]
            if not nb:
                continue
            ch = min(nb, key=lambda c: (leg[c][1], -nb.count(c)))
            for x, y in cells:
                rows[y][x] = ch
                n += 1
    if n:
        for k, y in enumerate(range(g0, g1)):
            eol = '\r' if lines[y].endswith('\r') else ''
            lines[y] = '|' + ''.join(rows[k]) + eol   # (every row at the view's width)
    return n


def fill_cells(lines, v, leg, cells):
    """set the view's clear cells at the anchor-relative cells to the outline step of its drawn cells round them"""
    i, head, g0, g1, r0, r1, blk = v
    facing, key, origin, cond = parse_head(head)
    if origin is None:
        origin = base_origin(lines, facing, key)
    if origin is None:
        return 0
    rows = [list(l[1:].rstrip('\r')) for l in lines[g0:g1]]
    w = max(len(r) for r in rows)
    for r in rows:
        r += ['.'] * (w - len(r))
    drawn = [c for r in rows for c in r if c in leg]
    n = 0
    for dx, dy in cells:
        x, y = origin[0] + dx, origin[1] + dy
        if not (0 <= x < w and 0 <= y < len(rows)) or rows[y][x] not in '. ':
            continue
        nb = []
        for ny in (y - 1, y, y + 1):
            for nx in (x - 1, x, x + 1):
                if (nx, ny) != (x, y) and 0 <= nx < w and 0 <= ny < len(rows) and rows[ny][nx] in leg:
                    nb.append(rows[ny][nx])
        pool = nb or drawn
        if not pool:
            continue
        rows[y][x] = min(pool, key=lambda c: (leg[c][1], -pool.count(c)))
        n += 1
    if n:
        for k, y in enumerate(range(g0, g1)):
            eol = '\r' if lines[y].endswith('\r') else ''
            old = lines[y].rstrip('\r')
            lines[y] = '|' + ''.join(rows[k]) + eol   # (every row at the view's width)
    return n


def apply_fills(path):
    lines = read(path)
    leg = legend_of(lines)
    n = 0
    for v in views_of(lines):
        if v[6] in FILL_BLOCKS:
            n += fill_view(lines, v, leg)
        facing = parse_head(v[1])[0]
        for blk, fs, cells in FILL_CELLS:
            if v[6] == blk and facing in fs:
                n += fill_cells(lines, v, leg, cells)
    if n:
        open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines))
    return n


def report_caps():
    for dp, dn, fn in os.walk(MAPS):
        for f in sorted(fn):
            if f.endswith('.pxm'):
                lines = read(os.path.join(dp, f))
                for v in views_of(lines):
                    cap_view(list(lines), v, True)


def main():
    if '--report' in sys.argv:
        report_caps()
        return
    total = filled = 0
    for dp, dn, fn in os.walk(MAPS):
        for f in sorted(fn):
            if f.endswith('.pxm'):
                n = apply_caps(os.path.join(dp, f), False)
                if n:
                    print('cap: %s: %d row(s) cut' % (os.path.relpath(os.path.join(dp, f), MAPS), n))
                    total += n
                c = apply_col_caps(os.path.join(dp, f))
                if c:
                    print('cap: %s: %d column(s) cut' % (os.path.relpath(os.path.join(dp, f), MAPS), c))
                    total += c
                m = apply_fills(os.path.join(dp, f))
                if m:
                    print('fill: %s: %d cell(s)' % (os.path.relpath(os.path.join(dp, f), MAPS), m))
                    filled += m
    print('p5b_fix_r1: %d row(s) and column(s) cut, %d pocket cell(s) filled' % (total, filled))


if __name__ == '__main__':
    main()
