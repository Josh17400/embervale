#!/usr/bin/env python3
# tools/doll/p5_marks.py: (P5 MARKS, docs/FIX_BATCH_P5.md 3, 5.3; STYLE_P2 30) the peoples' fingerprint on the doll's maps.
# The fingerprint lands only on finish regions (R rims, N seams and laces, P the lit panel); this tool RE-CUTS the region
# planes of the views whose pieces lack the N cells section 3.3's rules land on, and never touches a pixel: every changed
# view keeps its cells, origin, mask, build row, puts and marks; only views that already carry a region plane are touched,
# and a region cell is only ever ADDED where the plane had '.' (an R, P, G or W a drawing marked is never re-cut).
#
#   python tools/doll/p5_marks.py              write the re-cuts (idempotent: a re-cut view is re-derived from its cells)
#   python tools/doll/p5_marks.py --check      exit 1 when a map is not as this tool writes it
#   python tools/doll/p5_marks.py --verify     against git's 80215d7 (or --base REV): every view of every map keeps its
#                                              header, cells, build row, puts and marks; a changed region cell was '.'
#                                              and is now N; no view gained a region plane; no view added or removed
#                                              outside finish/mark.pxm and the plaid's filigree views
#   python tools/doll/p5_marks.py --boxes      the P boxes (w x h of a view's P cells) of every armour, helm, shield and
#                                              cloak view: the sizes the mark tile is drawn at (counts per size)
#   python tools/doll/p5_marks.py --table      rpg/core/fingerprint.h against section 3.5's separations (exit 1 if not)
#   python tools/doll/p5_marks.py --landing    the base views of armour / helm / shield / cloak parts with neither R nor N
#
# The re-cuts (each a rule on the view's own cells, so a redrawn view re-cuts itself):
#   lacing   armour/{man,woman}/lamellar, lamellar_hang, man/mail_hang: the lamellar's lacing rows (a row of the piece whose
#            cells are its lace step: the drawing's own darker course line under each row of plates) take N; mail_hang's
#            lowest course (the hauberk's hem row) takes N;
#   brow     headwear/helm/<family>.pxm and <family>_legend.pxm with no N today: the brow band's lower row (the lowest row
#            of the helm's metal above the face opening / the rim) takes N, so the lining under the rim carries the dye.
# The leather coat's laces (decision 2) are N cells tools/doll/p4_leather.py emits with its lace crossings.
import os, re, subprocess, sys
from collections import Counter, defaultdict

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
MAPS = os.path.join(REPO, 'rpg', 'art', 'doll', 'maps')
BEGIN_TAG = '# (P5 MARKS) region re-cut by tools/doll/p5_marks.py'
VIEW_DIRECTIVES = ('regions', 'build', 'put', 'mark')


def rel(p):
    return os.path.relpath(p, REPO).replace(os.sep, '/')


# ------------------------------------------------------------------------------------------------------- the parser
class View:
    __slots__ = ('start', 'end', 'header', 'facing', 'key', 'when', 'grid', 'regions', 'reg_at', 'other')

    def __init__(self):
        self.grid, self.regions, self.other, self.reg_at = [], None, [], -1


def strip_row(ln):
    row = ln.rstrip()[1:]
    bar = row.find('|')
    if bar >= 0: row = row[:bar]
    return row.replace(' ', '.')


def parse(lines):
    """the views of a map: each with its line span [start, end), header, grid rows, region rows (None: no plane)."""
    views, i, n = [], 0, len(lines)
    while i < n:
        t = lines[i].strip()
        if not t.startswith('view '):
            i += 1; continue
        V = View(); V.start = i; V.header = lines[i].rstrip()
        tk = t.split()
        V.facing = tk[1]
        V.key = tk[tk.index('key') + 1] if 'key' in tk else 'rest'
        V.when = tk[tk.index('when') + 1] if 'when' in tk else ''
        j, block = i + 1, 'grid'
        while j < n:
            ln = lines[j].rstrip()
            s = ln.strip()
            if ln.startswith('|'):
                if block == 'regions': V.regions.append(strip_row(ln))
                elif block == 'build': V.other.append(('build', ln))
                else: V.grid.append(strip_row(ln))
            elif s == 'regions':
                block = 'regions'; V.regions = []; V.reg_at = j
            elif s == 'build':
                block = 'build'; V.other.append(('build', ln))
            elif s.split(' ')[0] in ('put', 'mark'):
                V.other.append(('dir', ln))
            else:
                break
            j += 1
        V.end = j
        views.append(V)
        i = j
    return views


def walk(sub):
    out = []
    for d, _, fs in os.walk(os.path.join(MAPS, sub)):
        for f in fs:
            if f.endswith('.pxm'): out.append(os.path.join(d, f))
    return sorted(out)


def part_role(lines):
    for ln in lines:
        t = ln.split()
        if t[:1] == ['role']: return t[1] if len(t) > 1 else ''
    return ''


# ------------------------------------------------------------------------------------------------------- measures
def p_box(V, letter='P'):
    if V.regions is None: return None
    xs, ys = [], []
    for y, r in enumerate(V.regions):
        for x, c in enumerate(r):
            if c == letter and y < len(V.grid) and x < len(V.grid[y]) and V.grid[y][x] not in '.-':
                xs.append(x); ys.append(y)
    if not xs: return None
    return (max(xs) - min(xs) + 1, max(ys) - min(ys) + 1)


def boxes():
    cnt, where = Counter(), defaultdict(list)
    for sub in ('armour', 'headwear/helm', 'shield', 'cloak', 'forge'):
        for f in walk(sub):
            lines = open(f, encoding='utf-8').read().split('\n')
            for V in parse(lines):
                b = p_box(V)
                if b:
                    cnt[b] += 1
                    if len(where[b]) < 4: where[b].append('%s %s %s %s' % (rel(f)[len('rpg/art/doll/maps/'):], V.facing, V.key, V.when))
    for b, c in sorted(cnt.items(), key=lambda kv: (-kv[1], kv[0])):
        print('%2dx%-2d %5d   %s' % (b[0], b[1], c, ' | '.join(where[b])))
    return 0


# ------------------------------------------------------------------------------------------------------- the table
def lab(v):
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = [lin(x) for x in ((v >> 16) & 255, (v >> 8) & 255, v & 255)]
    X = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047; Y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    Z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883
    f = lambda t: t ** (1 / 3) if t > 0.008856 else 7.787 * t + 16 / 116
    X, Y, Z = f(X), f(Y), f(Z)
    return (116 * Y - 16, 500 * (X - Y), 200 * (Y - Z))


def dE(a, b):
    A, B = lab(a), lab(b)
    return sum((x - y) ** 2 for x, y in zip(A, B)) ** 0.5


def ramps():
    """steps 1-3 of the six leathers (kLeather), bronze, iron, steel, gilded (kMetalKind) and brass (kTrim[0]) as the code
    holds them (rpg/art/doll/doll_ramps.cpp)."""
    src = open(os.path.join(REPO, 'rpg', 'art', 'doll', 'doll_ramps.cpp'), encoding='utf-8').read()
    hexes = lambda s: [int(h, 16) for h in re.findall(r'0x([0-9A-Fa-f]{6})', s)]
    out = {}
    m = re.search(r'const uint32_t kLeather\[6\]\[5\] = \{(.*?)\n\};', src, re.S)
    L = hexes(m.group(1))
    for i in range(6):
        for s in (1, 2, 3): out['leather%d.%d' % (i, s)] = L[i * 5 + s]
    m = re.search(r'const uint32_t kMetalKind\[12\]\[5\] = \{(.*?)\n\};', src, re.S)
    rows = [hexes(ln)[:5] for ln in m.group(1).split('\n') if ln.strip().startswith('{0x')]
    for name, k in (('bronze', 2), ('iron', 3), ('steel', 4), ('gilded', 5)):
        for s in (1, 2, 3): out['%s.%d' % (name, s)] = rows[k][s]
    m = re.search(r'const uint32_t kTrim\[4\]\[5\] = \{(.*?)\n\};', src, re.S)
    T = hexes(m.group(1))
    for s in (1, 2, 3): out['brass.%d' % s] = T[s]
    return out


def table_rows():
    src = open(os.path.join(REPO, 'rpg', 'core', 'fingerprint.h'), encoding='utf-8').read()
    rows = re.findall(r'\{"(\w+)",\s*"(\w+)",\s*0x([0-9A-Fa-f]{6}),\s*0x([0-9A-Fa-f]{6}),\s*(\d+),\s*(\d+)\}', src)
    return [(n, m, int(a, 16), int(b, 16), int(r), int(p)) for n, m, a, b, r, p in rows]


def table():
    rows = table_rows()
    ok = len(rows) == 13 and rows[0][0] == 'none'
    peoples = rows[1:]
    mats = ramps()
    worst = lambda xs: min(xs, key=lambda t: t[0])
    prim = worst([(dE(a[2], b[2]), a[0], b[0]) for i, a in enumerate(peoples) for b in peoples[i + 1:]])
    sig1 = lambda a, b: max(min(dE(x, y) for y in (b[2], b[3])) for x in (a[2], a[3]))
    sig = worst([(max(sig1(a, b), sig1(b, a)), a[0], b[0]) for i, a in enumerate(peoples) for b in peoples[i + 1:]])
    mat = worst([(dE(a[2], v), a[0], k) for a in peoples for k, v in mats.items()])
    own = worst([(dE(a[2], a[3]), a[0], '') for a in peoples if a[3]])
    motifs = [a[1] for a in peoples]
    bands = all(1 <= a[4] <= a[5] <= 6 and (a[3] or a[4] == a[5]) for a in peoples)
    lines = [('primaries pairwise', prim, 25.0), ('two-tone signatures', sig, 25.0), ('primary vs material steps 1-3', mat, 20.0),
             ('dye2 vs its dye', own, 20.0)]
    for name, (d, a, b), lim in lines:
        good = d >= lim
        ok &= good
        print('%-32s min dE %5.1f (%s %s)  >= %.0f  %s' % (name, d, a, b, lim, 'OK' if good else 'FAIL'))
    good = len(set(motifs)) == 12
    ok &= good and bands
    print('%-32s %s  %s' % ('12 motifs, all different', ' '.join(motifs), 'OK' if good else 'FAIL'))
    print('%-32s %s' % ('bands (1 <= run <= period <= 6)', 'OK' if bands else 'FAIL'))
    print('table: %s' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


# ------------------------------------------------------------------------------------------------------- the marks
# finish/mark.pxm's drawings (STYLE_P2 30.4): the people's motif in THREAD, the filigree's language one rung down the
# ladder. A = the dye at the panel's own step (the lit chest: the thread lit with it), a = the dye one step down (the
# thread's shadow side, bottom-right), B / b = dye2 the same, k = a hole (ink 1: the material one step down). Every thread
# pixel touches another (a shape, never a lone dot); the light is top-left. Squares 2..6 (FIT takes the largest that fits
# the P box, centred: a 3x4 box wears the 3x3); the lines 2x1 3x1 1x2 1x3 for brow bands and slivers; 1x1 is one engraved
# point (k, as finish/etch's), the only fit a 1 px panel can take.
MOTIFS = ['knot', 'plaid', 'oak', 'laurel', 'arab', 'horse', 'reed', 'clouds', 'wave', 'ray', 'vein', 'star']
MARK = {
    # Fjordfolk: the looped square (St John's arms, the Bowen knot) in woad, the over-crossing in bleached wool; small: an X
    'knot': {
        2: 'AB bA', 3: 'A.B .A. b.a', 4: 'A..B .Ab. .BA. b..a',
        5: 'AA.AA A.B.A .B.b. A.b.a AA.aa', 6: 'AA..AA A.BB.A .B..b. .B..b. A.bb.a AA..aa',
        '2x1': 'AB', '3x1': 'ABA', '1x2': 'A B', '1x3': 'A B a'},
    # Highland: the sett, two ground and one red stripe each way (the hem-plaid capelet's 2 + 1), a rounded patch
    'plaid': {
        2: 'AB BB', 3: 'AAB AAB BBb', 4: 'ABAA BbBB ABAa ABaa',
        5: 'AABAA AABAA BBbBB AABAa AABaa', 6: '.AABA. AAABAa BBBbBb AAABaa .AABa. ......',
        '2x1': 'AB', '3x1': 'AAB', '1x2': 'A B', '1x3': 'A A B'},
    # Heartland: the oak leaf, its lobes alternating down the midrib, on its stem; a white acorn at its foot
    'oak': {
        2: 'AA aB', 3: 'A.A AaA .a.', 4: '.AA. AAaA .Aa. B.a.',
        5: '.AA.. AAaA. .AaAA AAaA. B.a..', 6: '...A.. .AAaA. ..AaAA .AAaA. .BAaAA BB.a..',
        '2x1': 'Aa', '3x1': 'AaB', '1x2': 'A a', '1x3': 'A a B'},
    # Imperial: the laurel wreath, open at the top, gold berries at its tips
    'laurel': {
        2: 'B. Aa', 3: 'B.B A.A .a.', 4: 'B..B A..A A..a .aa.',
        5: 'B...B AA.AA A...a .A.a. ..a..', 6: 'B....B AA..AA A....a AA..aa .A..a. ..aa..',
        '2x1': 'BA', '3x1': 'BAB', '1x2': 'B A', '1x3': 'B A a'},
    # Dune: the arabesque lozenge, indigo round a saffron heart
    'arab': {
        2: '.A aB', 3: '.A. ABa .a.', 4: '.AA. ABBa aBBa .aa.',
        5: '..A.. .A.a. A.B.a .a.a. ..a..', 6: '..AA.. .A..a. A.BB.a A.BB.a .a..a. ..aa..',
        '2x1': 'aB', '3x1': 'ABa', '1x2': 'A B', '1x3': 'A B a'},
    # Steppe: the running horse, head up to the right, the red horse-hair tail behind
    'horse': {
        2: 'BA a.', 3: '..A BAA a.a', 4: '...A BAAA .aa. a..a',
        5: '....A BAAAA .AAa. .a.a. a...a', 6: '....AA B...A. .AAAA. .AAAa. .a..a. a....a',
        '2x1': 'BA', '3x1': 'BAA', '1x2': 'A a', '1x3': 'A A a'},
    # Marsh: reeds, stalks of moss under faded-indigo heads, staggered
    'reed': {
        2: 'BB Aa', 3: 'B.B A.A a.a', 4: 'B.B. A.A. A.A. a.a.',
        5: '..B.. B.A.B A.A.A A.A.a a.a.a', 6: '..B... B.A.B. A.A.A. A.A.a. A.a.a. a.a.a.',
        '2x1': 'Aa', '3x1': 'BAa', '1x2': 'B A', '1x3': 'B A a'},
    # Jade: the cloud scroll (the square meander), vermilion round a black-lacquer core
    'clouds': {
        2: 'AA Ba', 3: 'AAA B.A BBa', 4: 'AAAA ...A .B.A .Baa',
        5: 'AAAAA A...A A.B.A A.B.a A.Baa', 6: 'AAAAAA A....A A.BB.A A..B.a AAAB.a ....aa',
        '2x1': 'AB', '3x1': 'AAB', '1x2': 'A B', '1x3': 'A A B'},
    # River: the wave curling over, its white crest
    'wave': {
        2: '.B Aa', 3: '.AA A.B .aa', 4: '.AA. A..A A.B. .aaa',
        5: '.AAA. A...A A.BB. a.... .aaaa', 6: '..AAA. .A...A A..B.A A...B. a..... .aaaaa',
        '2x1': 'AB', '3x1': 'aAB', '1x2': 'B A', '1x3': 'B A a'},
    # Sun Temple: the sun, a teal disc and its magenta rays
    'ray': {
        2: 'AB B.', 3: 'A.A .B. a.a', 4: 'A..A .BB. .BB. a..a',
        5: 'A.A.A .ABA. AABaa .aBa. a.a.a', 6: 'A.A.A. .ABA.. AABaa. .aBa.. a.a.a. ......',
        '2x1': 'AB', '3x1': 'ABA', '1x2': 'A B', '1x3': 'A B a'},
    # Sylvan: the leaf, its amber midrib and veins
    'vein': {
        2: 'AA BA', 3: '.AA ABA BA.', 4: '..AA .ABA ABA. B...',
        5: '..AAA .AABA AABA. ABAa. B....', 6: '...AAA ..AABA .AABAa AABAa. ABaa.. B.....',
        '2x1': 'BA', '3x1': 'BAA', '1x2': 'A B', '1x3': 'A A B'},
    # Starspire: the four-point star, a star-white heart
    'star': {
        2: '.A AB', 3: '.A. ABa .a.', 4: '.A.. ABAa .a.. .a..',
        5: '..A.. ..A.. AABaa ..a.. ..a..', 6: '..A... ..A... AABaa. ..a... ..a... ......',
        '2x1': 'AB', '3x1': 'ABa', '1x2': 'A B', '1x3': 'A B a'},
}
MARK_LEGEND = [('A', 'dye', 2), ('a', 'dye', 1), ('B', 'dye2', 2), ('b', 'dye2', 1), ('k', 'ink', 1)]
MARK_SIZES = (6, 5, 4, 3, 2, '3x1', '2x1', '1x3', '1x2')


def mark_views():
    """[(key, rows)] of every motif in every fit size, in finish/mark.pxm's order."""
    out = []
    for m in MOTIFS:
        for size in MARK_SIZES:
            rows = MARK[m][size].split(' ')
            out.append(('%s%dx%d' % (m, len(rows[0]), len(rows)), rows))
        out.append(('%s1x1' % m, ['k']))
    return out


def shape_errors(key, rows):
    """a mark's craft: rectangular, its letters in the legend, every thread pixel touching another (8-neighbour)."""
    errs, w, h = [], len(rows[0]), len(rows)
    if any(len(r) != w for r in rows): errs.append('%s: ragged rows' % key)
    legal = set('.' + ''.join(c for c, _, _ in MARK_LEGEND))
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c not in legal: errs.append('%s: %r at %d,%d not in the legend' % (key, c, x, y))
            if c in '.k' or w * h == 1: continue
            if not any(0 <= x + dx < w and 0 <= y + dy < h and rows[y + dy][x + dx] != '.' for dx in (-1, 0, 1) for dy in (-1, 0, 1) if dx or dy):
                errs.append('%s: a lone thread pixel at %d,%d' % (key, x, y))
    return errs


MARK_HEAD = """# finish/mark: the people's MARK on P at T1-T4 (FIX_BATCH_P5 3.3; STYLE_P2 30; P5 MARKS): the people's motif in thread,
# the filigree's language one rung down the ladder (thread at T1-T2, engraved at T3-T4, gold at T5-T6 = finish/filigree).
# A FIT tile: the view <motif><w>x<h> with the largest area that fits the part's P box, centred on it (cells off P
# skipped); the resolver asks for it on a T1-T4 piece with a maker (FIN_TILE_MARK), keyed by the slot's people
# (rpg/core/fingerprint.h's motif). A = the dye at the panel's own step, a = one step down (the thread's shadow side),
# B / b = dye2 the same, k = a hole (ink 1: the material one step down). On a metal piece the doll draws the dye cells as
# Ink one step down (engraved); at T4 the mark replaces the generic etch. Shapes, never a lone dot; the light top-left.
# Squares 2..6 (a 3x4 box wears the 3x3), lines 2x1 3x1 1x2 1x3 for brow bands and slivers, 1x1 one engraved point.
# Generated by tools/doll/p5_marks.py (the drawings live there: MARK); never edit by hand."""


def mark_text():
    out = MARK_HEAD.split('\n') + ['role overlay finish.mark']
    out += ['legend %s %s %d' % e for e in MARK_LEGEND]
    out += ['tags tile overlay fit', 'on-region P', '']
    cur = None
    for key, rows in mark_views():
        m = re.match(r'[a-z]+', key).group(0)
        if m != cur:
            cur = m
            out.append('# ' + m + ' (' + MARK_WHO[m] + ')')
        out.append('view D key %s origin 0 0' % key)
        out += ['|' + r for r in rows]
        out.append('')
    return '\n'.join(out)


MARK_WHO = {'knot': 'Fjordfolk', 'plaid': 'Highland', 'oak': 'Heartland', 'laurel': 'Imperial', 'arab': 'Dune', 'horse': 'Steppe',
            'reed': 'Marsh', 'clouds': 'Jade', 'wave': 'River', 'ray': 'Sun Temple', 'vein': 'Sylvan', 'star': 'Starspire'}


# ------------------------------------------------------------------------------------------------------- the plaid filigree
# (P5-2, decision 3) Highland's own T5-T6 motif: the tartan sett as a GILT LATTICE, in finish/filigree.pxm's language (O
# the gilt plane, N the engraved line one step under it, * the lit point; never a lone dot). Drawn on the arabesque's 24
# fit sizes (its masks: the '.' cells and the field keys' PARTY PER BEND V cells are kept exactly, so the plaid lands where
# the arabesque does); the lines run every 3 px each way at 2, 5, 8 (two gilt, one line: the hem-plaid capelet's 2 + 1
# sett, so a T5 Highland piece's lattice and its capelet hem are the same check); bands (h <= 2, w >= 9) one gilt plane
# lit at each end and on every third pixel (the sett in light, no N dots, polish i2's band rule).
PLAID_BEGIN = '# ---- P5 PLAID (generated by tools/doll/p5_marks.py: do not edit by hand)'
PLAID_END = '# ---- P5 PLAID end'


def plaid_from(rows):
    w, h = len(rows[0]), len(rows)
    if h <= 2 and w >= 9:
        top = ''.join('*' if x in (0, w - 1) or x % 3 == 0 else 'O' for x in range(w))
        return [top] + ['O' * w] * (h - 1)
    field = any('V' in r for r in rows)   # a field key: PARTY PER BEND (filigree.pxm's alloys i3 rule), its own bend
    out, lit = [], False
    px = py = 2 if w >= 3 and h >= 3 else 1   # the lines at 2, 5, 8 (a gilt square top-left); at 1 on a slim fit
    for y, r in enumerate(rows):
        o = ''
        for x, c in enumerate(r):
            if c == '.' or (not field and c == 'V'):
                o += c; continue
            if field and x / w + y / h > 0.75:
                o += 'V'; continue
            line = x % 3 == px or y % 3 == py
            if not line and not lit:
                o += '*'; lit = True
            else:
                o += 'N' if line else 'O'
        out.append(o)
    return out


def plaid_views(fil_lines):
    """the plaid views from filigree.pxm's arabesque views (their keys' sizes and masks)."""
    out = []
    for V in parse(fil_lines):
        if not re.match(r'arab\d+x\d+$', V.key): continue
        out.append(('plaid' + V.key[4:], plaid_from(V.grid)))
    return out


def filigree_text(text):
    lines = text.split('\n')
    if PLAID_BEGIN in lines:
        a = lines.index(PLAID_BEGIN); b = lines.index(PLAID_END)
        lines = lines[:a] + lines[b + 1:]
    while lines and lines[-1] == '': lines.pop()
    block = [PLAID_BEGIN,
             '# (P5 MARKS, P5-2, decision 3) Highland\'s own motif, the plaid: the tartan sett as a gilt lattice (lines every 3 px',
             '# each way, two gilt and one engraved: the hem-plaid capelet\'s sett), on the arabesque\'s fit sizes and masks (the',
             '# field keys PARTY PER BEND as theirs); Highland\'s T5-T6 filigree moves to it from the knotwork. tools/doll/p5_marks.py.']
    for key, rows in plaid_views(lines):
        block += ['', 'view D key %s origin 0 0' % key] + ['|' + r for r in rows]
    block.append(PLAID_END)
    return '\n'.join(lines + [''] + block + ['']) + ''


# ------------------------------------------------------------------------------------------------------- the re-cuts
def legend_of(path, lines):
    """char -> (slot, step) for a map: its `use` legends, then its own `legend` lines (in file order: a later line wins)."""
    L = {}
    for ln in lines:
        t = ln.split()
        if t[:1] == ['use']:
            for u in t[1:]:
                for l2 in open(os.path.join(MAPS, 'legend', u + '.legend'), encoding='utf-8'):
                    k = l2.split()
                    if k[:1] == ['legend']: L[k[1]] = (k[2], int(k[3]))
    for ln in lines:
        t = ln.split()
        if t[:1] == ['legend'] and len(t) >= 4: L[t[1]] = (t[2], int(t[3]))
    return L


LACING = ['armour/man/lamellar.pxm', 'armour/woman/lamellar.pxm']
COURSE = ['armour/man/lamellar_hang.pxm', 'armour/woman/lamellar_hang.pxm']
HELMS = ['kettle', 'horned', 'kabuto', 'morion', 'onion', 'sallet', 'veil', 'warhat', 'winged', 'crown', 'nasal', 'plumed', 'crested']
BROW = ['headwear/helm/%s%s.pxm' % (f, s) for f in HELMS for s in ('', '_legend')]
LANDABLE = ('metal', 'metal2', 'leather', 'cloth1', 'cloth2', 'cloth3', 'linen', 'accent', 'head')


def dye_seam_letter():
    """the region the re-cuts write: DOLL's `D` (a dye seam: the fingerprint's N under FIX_FP, '.' to every older reader)
    once tools/doll/pxc.mjs's REGIONS has it; until then None and the re-cuts write nothing. Plain N cannot be used: the
    ladder already reads N whoever made the piece (the T4-T6 trim growth, the floor band, plate lines, iris bands, T6
    runes, the glowing alloys' seams), so N added to old views moved 2276 pre-P5 frames in 21 freeze looks (measured)."""
    src = open(os.path.join(REPO, 'tools', 'doll', 'pxc.mjs'), encoding='utf-8').read()
    m = re.search(r'export const REGIONS = \{([^}]*)\}', src)
    return 'D' if m and re.search(r'D:\s*\d+', m.group(1)) else None


SEAM = dye_seam_letter()


def landable(L, ch):
    """a pixel the dye may take: drawn, a material slot, never the outline (step 0) or a glint (step 4)."""
    e = L.get(ch)
    return bool(e) and e[0] in LANDABLE and 1 <= e[1] <= 3


def has(V, letter):
    return V.regions is not None and any(letter in r for r in V.regions)


def regrid(V):
    w = max(len(r) for r in V.grid)
    return [list(r.ljust(w, '.')) for r in V.regions], w


def recut_lacing(V, L, base):
    """lamellar: the lacing cords (accent cells) take N; a variant drawn without the cord (the brass body, the alloy
    shapes) takes N on its own landable metal in the rows its base view (same facing and key) laces."""
    if SEAM is None or V.regions is None or has(V, 'N') or has(V, SEAM): return None
    R, w = regrid(V)
    acc = lambda c: L.get(c, ('',))[0] == 'accent'
    n = 0
    if any(acc(c) for r in V.grid for c in r):
        cells = [(x, y) for y, row in enumerate(V.grid) for x, c in enumerate(row) if acc(c)]
    else:
        if base is None: return None
        rows = sorted({y for y, row in enumerate(base.grid) for c in row if acc(c)})
        cells = [(x, y) for y in rows if y < len(V.grid) for x, c in enumerate(V.grid[y])
                 if L.get(c, ('',))[0] in ('metal', 'metal2')]
    for x, y in cells:
        if y < len(R) and x < len(V.grid[y]) and landable(L, V.grid[y][x]) and R[y][x] == '.':
            R[y][x] = SEAM; n += 1
    return [''.join(r) for r in R] if n else None


def recut_course(V, L, base):
    """lamellar skirt: the lowest course (the row above the hem band, R) takes N: the lacing that binds the hem."""
    if SEAM is None or V.regions is None or has(V, 'N') or has(V, SEAM): return None
    R, w = regrid(V)
    rows = [y for y, r in enumerate(R) if r.count('R') >= 3]
    if not rows: return None
    y = rows[-1]
    while y - 1 >= 0 and ''.join(R[y - 1]).count('R') >= 3: y -= 1   # the top of the hem band
    y -= 1
    if y < 0: return None
    n = 0
    for x, c in enumerate(V.grid[y]):
        if landable(L, c) and R[y][x] == '.':
            R[y][x] = SEAM; n += 1
    return [''.join(r) for r in R] if n >= 2 else None


def recut_brow(V, L, base):
    """helm: the lining under the brow band (the row under the lowest R row of three cells or more: its landable cells)
    takes the dye seam. A band at the helm's foot with nothing under it keeps no seam (its brass is never re-cut)."""
    if SEAM is None or V.regions is None or has(V, 'N') or has(V, SEAM): return None
    R, w = regrid(V)
    band = [y for y, r in enumerate(R) if ''.join(r).count('R') >= 3]
    if not band: return None
    y = band[-1]
    under = y + 1 < len(V.grid) and [x for x, c in enumerate(V.grid[y + 1]) if landable(L, c) and R[y + 1][x] == '.']
    if under and len(under) >= 3:
        for x in under: R[y + 1][x] = SEAM
        return [''.join(r) for r in R]
    return None


RULES = [(LACING, recut_lacing), (COURSE, recut_course), (BROW, recut_brow)]


def recut_file(relpath, rule):
    path = os.path.join(MAPS, relpath.replace('/', os.sep))
    text = open(path, encoding='utf-8').read()
    lines = text.split('\n')
    L = legend_of(path, lines)
    views = parse(lines)
    bases = {(V.facing, V.key): V for V in views if not V.when}
    n = 0
    for V in views:
        new = rule(V, L, bases.get((V.facing, V.key)))
        if not new: continue
        for k, row in enumerate(new):
            ln = lines[V.reg_at + 1 + k]
            body = ln.rstrip()[1:]
            bar = body.find('|')
            tail = body[bar:] if bar >= 0 else ''
            cur = body[:bar] if bar >= 0 else body
            row = row.rstrip('.').ljust(len(cur), '.')
            lines[V.reg_at + 1 + k] = '|' + row + tail
        n += 1
    return path, '\n'.join(lines), text, n


def recuts(check):
    bad = 0
    for files, rule in RULES:
        for f in files:
            path, new, old, n = recut_file(f, rule)
            if new != old:
                if check:
                    print('p5_marks: %s is not as re-cut (%d views)' % (f, n)); bad += 1
                else:
                    open(path, 'w', encoding='utf-8', newline='\n').write(new)
                    print('p5_marks: re-cut %s (%d views)' % (f, n))
    return bad


MARK_PXM = os.path.join(MAPS, 'finish', 'mark.pxm')
FILIGREE_PXM = os.path.join(MAPS, 'finish', 'filigree.pxm')
P4_FILES = ['armour/%s/%s.pxm' % (b, f) for b in ('man', 'woman') for f in ('padded', 'padded_hang')]


def generated():
    """{path: (new text, old text)} of the files this tool writes: the mark tile, the plaid block, the re-cuts."""
    out = {}
    old = open(MARK_PXM, encoding='utf-8').read() if os.path.exists(MARK_PXM) else ''
    out[MARK_PXM] = (mark_text() + '\n', old)
    old = open(FILIGREE_PXM, encoding='utf-8').read()
    out[FILIGREE_PXM] = (filigree_text(old), old)
    for files, rule in RULES:
        for f in files:
            path, new, old, n = recut_file(f, rule)
            out[path] = (new, old)
    return out


def write(check):
    errs = [e for k, r in mark_views() for e in shape_errors(k, r)]
    for e in errs: print('p5_marks: mark craft: ' + e)
    bad = len(errs)
    for path, (new, old) in generated().items():
        if new == old:
            print('p5_marks: %s up to date' % rel(path)); continue
        if check:
            print('p5_marks: %s is not as generated' % rel(path)); bad += 1
        else:
            open(path, 'w', encoding='utf-8', newline='\n').write(new)
            print('p5_marks: wrote %s' % rel(path))
    return 1 if bad else 0


def verify(base):
    """every map changed since `base`: the same views in the same order with the same header, cells, build row, puts and
    marks; a region plane only where one was; a changed region cell only '.' -> N (the laces) or D (the dye seam);
    new views only in finish/mark.pxm and the plaid block of finish/filigree.pxm."""
    names = subprocess.run(['git', '-C', REPO, 'diff', '--name-only', base, '--', 'rpg/art/doll/maps'], capture_output=True,
                           text=True, check=True).stdout.split()
    names += subprocess.run(['git', '-C', REPO, 'ls-files', '--others', '--exclude-standard', 'rpg/art/doll/maps'],
                            capture_output=True, text=True, check=True).stdout.split()
    brow = {'rpg/art/doll/maps/' + f for f in BROW}
    allowed = {'rpg/art/doll/maps/' + f for files, _ in RULES for f in files} | {'rpg/art/doll/maps/' + f for f in P4_FILES}
    bad, nviews, ncells = 0, 0, 0
    def fail(msg):
        nonlocal bad
        bad += 1
        if bad <= 20: print('p5_marks --verify: ' + msg)
    for name in sorted(set(names)):
        cur = open(os.path.join(REPO, name), encoding='utf-8').read().split('\n')
        if name == 'rpg/art/doll/maps/finish/mark.pxm':
            if cur != (mark_text() + '\n').split('\n'): fail(name + ': not as generated')
            continue
        r = subprocess.run(['git', '-C', REPO, 'show', '%s:%s' % (base, name)], capture_output=True, text=True, encoding='utf-8')
        if r.returncode:
            fail(name + ': a new map (only finish/mark.pxm may be new)'); continue
        old = r.stdout.split('\n')
        A, B = parse(old), parse(cur)
        if name == 'rpg/art/doll/maps/finish/filigree.pxm':
            extra = B[len(A):]
            if any(not v.key.startswith('plaid') for v in extra): fail(name + ': a view added that is not a plaid view')
            B = B[:len(A)]
        elif name not in allowed:
            fail(name + ': not a P5 MARKS map'); continue
        if len(A) != len(B):
            fail('%s: %d views, was %d' % (name, len(B), len(A))); continue
        for a, b in zip(A, B):
            where = '%s %s' % (name, a.header.strip())
            if a.header.strip() != b.header.strip(): fail(where + ': header changed'); continue
            if a.grid != b.grid: fail(where + ': cells changed')
            if a.other != b.other: fail(where + ': build row, put or mark changed')
            if (a.regions is None) != (b.regions is None): fail(where + ': a region plane added or removed'); continue
            if a.regions is None or a.regions == b.regions: continue
            nviews += 1
            h = max(len(a.regions), len(b.regions))
            for y in range(h):
                ra = a.regions[y] if y < len(a.regions) else ''
                rb = b.regions[y] if y < len(b.regions) else ''
                for x in range(max(len(ra), len(rb))):
                    ca = ra[x] if x < len(ra) else '.'
                    cb = rb[x] if x < len(rb) else '.'
                    if ca == cb: continue
                    ncells += 1
                    if cb not in 'ND' or ca != '.':
                        fail('%s: region %s -> %s at %d,%d' % (where, ca, cb, x, y))
    print('p5_marks --verify: %d maps, %d re-cut views, %d region cells -> N, cells / origins / masks / build rows unchanged: %s'
          % (len(set(names)), nviews, ncells, 'PASS' if not bad else 'FAIL (%d)' % bad))
    return 1 if bad else 0


def main():
    a = sys.argv[1:]
    if '--boxes' in a: return boxes()
    if '--table' in a: return table()
    if '--verify' in a: return verify(a[a.index('--base') + 1] if '--base' in a else '80215d7')
    return write('--check' in a)


if __name__ == '__main__':
    sys.exit(main())
