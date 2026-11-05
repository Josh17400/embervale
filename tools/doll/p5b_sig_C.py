#!/usr/bin/env python3
"""tools/doll/p5b_sig_C.py (P5b area C, docs/STYLE_P5B.md 4-5): River, Sun Temple, Sylvan and Starspire wear their people's
heads at T1-T4 as ADDED views, the slice's way (tools/doll/p5b_sig.py): every block is new `when sighelm-<people>,...` views
(the resolver's per-slot people flag) and the local legend characters they use, inserted before a part's first view (a
`replace` view wins in file order). No drawn view's cells change. Re-running replaces this script's own blocks (between its
begin and end comments, tag `(P5b SIG C)`) only; the slice's `(P5b SIG)` blocks and every other line are left as they are.

  python tools/doll/p5b_sig_C.py            write the blocks into rpg/art/doll/maps
  python tools/doll/p5b_sig_C.py --verify   outside this script's blocks every touched map is line for line its BASE version
  python tools/doll/p5b_sig_C.py --print    print the blocks

The views are drawn in an authoring alphabet (below) and written with characters each map leaves unused, so a block never
re-means a character another view of the map draws with:
  Q R S T U   the Head slot 0-4 (a soft head's cloth: Sig::hood or the accent)
  0 1 2 3 4   Plume 0-4 (the crest's colour: Sig::crest or the dominant)
  5 6 7 8 9   Dye 0-4 (a helm part's dominant)        A B C D E   Dye2 0-4 (a helm part's accent)
  v w x y z   the helm's metal 0-4                    J N O P W   Metal2 0-4 (the fittings)
  f g h i j   leather 0-4                             K           ink        q r s t u  skin 0-4
  '.' keeps what is below, '-' erases it

The heads (rows in rpg/core/fingerprint.h kSigRiver kSigSunTemple kSigSylvan kSigStarspire; T5-T6 on existing legend heads):
  River       T1 the chaperon: headwear/cloth/headscarf (its tails behind are the liripipe) on a fat lapis roll (an overlay);
              T2 the chapel (the kettle, E); T3 the morion with its lapis plume nodding back over the comb (an overlay on
              headwear/helm/morion, T3 only: the C12f head step over the chapel); T4 the morion's T4 drawing over a linen
              arming coif (SigTier::coif, E)
  Sun Temple  T1 the turquoise headband on headwear/cloth/circlet, its yellow feather (facing D at the side, R and L at the
              band's front) and its knot at the nape (an overlay); T2 the turquoise cap under a fan of three yellow feathers
              (an overlay on headwear/cloth/cap); T3 the jaguar's head (headwear/helm/masked replaced, T3 up): the spotted
              pelt in the yellow, ears, the turquoise eyes, the nose, the helm's metal band and fangs, the face in the jaws;
              T4 quetzal feather sprays from the jaguar's ears (an overlay, T4 only)
  Sylvan      T1 the leaf circlet (headwear/cloth/circlet replaced: a forest band set with leaves, a two-leaf sprig swept back
              past each temple); T2 the leaf crown (five upright leaves high on the head); T3 the winged helm, T4 the
              messenger wings (E)
  Starspire   the indigo hood at every tier (headwear/cloth/hood, an overlay: its crown cut to a gothic ogive, the face framed
              star white); T2 up the stiff pointed cowl (a row taller, the star clasp); T3 the radiant star crown and T4 the
              crown's T4 drawing worn over the hood (SigTier::coif, E); the crown's trim (a T3 crown worn a tier under its
              own by a census man, ladderLook) keeps inside the head box
"""
import os
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
MAPS = os.path.join(ROOT, 'rpg', 'art', 'doll', 'maps')
BASE = 'a3f5a4c1a8904c9353079f44061c5091782644df'   # fix-p5b-slice (the slice fix r2): the maps before area C
TAG = '(P5b SIG C)'

# the authoring alphabet: character -> (slot, step)
ALPHA = {}
for _i, _c in enumerate('QRSTU'): ALPHA[_c] = ('head', _i)
for _i, _c in enumerate('01234'): ALPHA[_c] = ('plume', _i)
for _i, _c in enumerate('56789'): ALPHA[_c] = ('dye', _i)
for _i, _c in enumerate('ABCDE'): ALPHA[_c] = ('dye2', _i)
for _i, _c in enumerate('vwxyz'): ALPHA[_c] = ('metal', _i)
for _i, _c in enumerate('JNOPW'): ALPHA[_c] = ('metal2', _i)
for _i, _c in enumerate('fghij'): ALPHA[_c] = ('leather', _i)
for _i, _c in enumerate('qrstu'): ALPHA[_c] = ('skin', _i)
ALPHA['K'] = ('ink', 0)


def mirror(rows):
    return [r[::-1] for r in rows]


def grid(h, w):
    return [['.'] * w for _ in range(h)]


def rows_of(g):
    return [''.join(r) for r in g]


def put(g, y, x, c):
    if 0 <= y < len(g) and 0 <= x < len(g[0]):
        g[y][x] = c


def run(g, y, x0, x1, chars):
    """chars spread over x0..x1 (its first char at x0, its last at x1, the middle stretched)"""
    n = x1 - x0 + 1
    for i in range(n):
        k = 0 if n == 1 else round(i * (len(chars) - 1) / (n - 1))
        put(g, y, x0 + i, chars[k])


def stamp(g, y0, x0, rows):
    """rows drawn at (y0, x0); '.' in them keeps the grid"""
    for dy, s in enumerate(rows):
        for dx, c in enumerate(s):
            if c != '.':
                put(g, y0 + dy, x0 + dx, c)


def axview(f, k, rows, ax0, ay0):
    return (f, k, rows, None, (-ax0, -ay0))


# ------------------------------------------------------------------------------------------------ authoring in head coordinates
# A replace view is authored on a grid whose top-left cell sits at (ax0, ay0) from the head anchor (the anchor is the head box's
# top-left, 16 columns wide; the bare hair rises to about ay -4 on the man, -2 on the woman; the brow is near ay 5, the eyes 7)
def at(rows, ax0, ay0):
    return rows, (-ax0, -ay0)


def view(f, k, ra, reg=None):
    rows, org = ra
    return (f, k, rows, reg, org)


def cells_view(path, f, k, cells):
    """an overlay from {(base row, base col): char}: its grid spans the cells, its origin the base's moved to match"""
    ys = [y for y, _ in cells]
    xs = [x for _, x in cells]
    y0, x0 = min(ys), min(xs)
    g = grid(max(ys) - y0 + 1, max(xs) - x0 + 1)
    for (y, x), ch in cells.items():
        g[y - y0][x - x0] = ch
    o = base_origin(path, f, k)
    return (f, k, rows_of(g), None, (o[0] - x0, o[1] - y0))


def cells_of(rows, y0, x0):
    """{(row, col): char} of rows drawn at (y0, x0) ('.' left out)"""
    return {(y0 + dy, x0 + dx): c for dy, r in enumerate(rows) for dx, c in enumerate(r) if c != '.'}


# ------------------------------------------------------------------------------------------------ Starspire T1 and T2: the hood
# headwear/cloth/hood in indigo (Sig::hood): an overlay that draws its crown up into a gothic point (an ogive: 2, 4, 6, 8 px
# a row, its shoulders cut back where no hair lies), and frames the face in star white (Plume: Sig::crest), the brow fold
# lit and the opening's two sides. T2 (the stiff cowl) the point a row taller, and a stiff high collar standing beside the jaw
# from the cowl, edged star white, a star clasp under the chin
def hood_point(f, tall):
    """the hood's crown cut to a gothic ogive ('-' erases its rounded shoulders, never a row over the hood: the envelope's
    T1 2 rows); tall (T2 up, the stiff cowl): the point a row higher (T2's 3 rows), the ogive narrower"""
    if f in 'DU':
        rows = [(0, 6, '--QQ--'), (1, 4, '--QUUTSQ--'), (2, 2, '--QTUUUTTSRQ--'), (3, 1, '-' + ''.ljust(14, '.') + '-')]
        if tall:
            rows = [(-1, 8, 'QQ'), (0, 6, '-QUTQ-')]
    elif f == 'R':
        rows = [(0, 7, '--QQ--'), (1, 5, '--QUUTSQ--'), (2, 4, '-QTUUTTSSQ-')]
        if tall:
            rows = [(-1, 9, 'QQ'), (0, 7, '-QUTQ-')]
    else:   # L: the hood's L grid (its face on the left)
        rows = [(0, 6, '--QQ--'), (1, 4, '--QUUTSQ--'), (2, 3, '-QTUUTTSSQ-')]
        if tall:
            rows = [(-1, 8, 'QQ'), (0, 6, '-QUTQ-')]
    c = {}
    for y, a, t in rows:
        for i, ch in enumerate(t):
            if ch != '.':
                c[(y, a + i)] = ch
    if f == 'U':
        c = {k: ('T' if v == 'U' else v) for k, v in c.items()}
    return c


def hood_rim(f):
    """the face framed in star white (Plume): the brow fold and the opening's sides (the hood's grid rows)"""
    c = {}
    if f == 'D':
        for x, ch in zip(range(4, 14), '3443333221'):
            c[(5, x)] = ch
        for y in range(6, 13):
            c[(y, 2)] = '3'
            c[(y, 15)] = '1'
        c[(13, 3)] = '3'
        c[(13, 14)] = '1'
    elif f == 'R':
        for x, ch in zip(range(13, 18), '43322'):
            c[(5, x)] = ch
        for y in range(7, 13):
            c[(y, 9)] = '2'
    elif f == 'L':
        for x, ch in zip(range(1, 6), '33443'):
            c[(5, x)] = ch
        for y in range(7, 13):
            c[(y, 9)] = '3'
    return c


def hood_collar(f):
    """(T2 up) the star clasp under the chin, star white (the hood's own cells: the head keeps its outline)"""
    if f == 'D':
        return {(14, 8): '4', (14, 9): '3', (15, 8): '3', (15, 9): '2', (13, 8): '3', (13, 9): '2', (14, 7): '3', (14, 10): '2'}
    if f == 'R':
        return {(13, 11): '3', (14, 12): '2', (14, 11): '2'}
    if f == 'L':
        return {(13, 7): '3', (14, 6): '3', (14, 7): '2'}
    return {}


def hood_collar_old(f):
    if f == 'D':
        return {(10, 0): '0', (11, 0): '3', (12, 0): '3', (11, -1): '0', (12, -1): '0', (13, -1): '0',
                (10, 17): '0', (11, 17): '1', (12, 17): '1', (11, 18): '0', (12, 18): '0', (13, 18): '0',
                (13, 0): '3', (13, 17): '1', (14, 0): 'T', (14, 17): 'R',
                (14, 8): '4', (14, 9): '3', (15, 8): '3', (15, 9): '2', (13, 8): '3', (13, 9): '2', (14, 7): '3', (14, 10): '2'}
    if f == 'R':
        return {(10, 0): '0', (11, 0): '3', (12, 0): '3', (13, 0): '3', (11, -1): '0', (12, -1): '0', (13, -1): '0',
                (14, 0): 'T', (12, 11): '0', (13, 11): '3', (13, 12): '0', (14, 12): '0'}
    if f == 'L':
        return {(10, 18): '0', (11, 18): '1', (12, 18): '1', (13, 18): '1', (11, 19): '0', (12, 19): '0', (13, 19): '0',
                (14, 18): 'R', (12, 7): '0', (13, 7): '3', (13, 6): '0', (14, 6): '0'}
    return {(10, 0): '0', (11, 0): '3', (12, 0): '3', (11, -1): '0', (12, -1): '0', (13, -1): '0',
            (10, 17): '0', (11, 17): '1', (12, 17): '1', (11, 18): '0', (12, 18): '0', (13, 18): '0'}


def hood_views(path, tall):
    out = []
    for f in 'DURL':
        cells = hood_point(f, tall)
        if not tall and f != 'U':
            cells.update(hood_rim(f))
        if tall:
            cells.update(hood_collar(f))
        out.append(cells_view(path, f, 'rest', cells))
    return out


# ------------------------------------------------------------------------------------------------ Sylvan T1 and T2: the leaves
# headwear/cloth/circlet replaced: a forest band (Head: the accent) set with leaf-green leaves (Plume). T1 the circlet at the
# brow, its leaves lying along it, the two end sprigs swept back past the temples (the Sylvan line: curved, swept back). T2
# the leaf crown worn high on the head, five upright leaves (the middle tallest) over the hair, the outer ones swept back
def leaf_circlet():
    d = at(['32.................12',
            '.33.................1.',
            '..42.3.4.33.2.1.21....',
            '...QTSTSTSTSSRSRSRQ...'], -3, 2)
    u = at(['3................2',
            '.32.2.2.2.2.2.2.1.',
            '.QSSSSSSSSSRRRRRQ.'], -1, 4)
    r = at(['32................',
            '.33...............',
            '..342.3.4.3.3.2.3.',
            '...QTSTSTSTSSRSSRQ'], -3, 2)
    l = at(['................12',
            '...............11.',
            '.3.3.3.4.3.4.2431.',
            'QSSTSTSTSTSTSRQ...'], 1, 2)
    return [view('D', 'rest', d), view('U', 'rest', u), view('R', 'rest', r), view('L', 'rest', l), view('L', 'lie', l)]


def leaf_crown():
    d = at(['........34........',
            '..3.....43.....2..',
            '.343...3443...321.',
            '..343.343332.321..',
            '..QTTTTSSSSSSRRQ..'], -1, -1)
    u = at(['........33........',
            '..3.....32.....2..',
            '.332...3332...221.',
            '..332.332222.221..',
            '..QSSSSSSSSSSRRQ..'], -1, 0)
    r = at(['.........34.......',
            '3.....3..43..3....',
            '.43..343.443.32...',
            '...343343332332...',
            '...QTTTSSSSSSSRQ..'], -1, -1)
    l = at(['.......43.........',
            '....2..34..3.....2',
            '...23.344.343..32.',
            '...233243343332...',
            '..QTTTSSSSSSSRQ...'], 0, -1)
    return [view('D', 'rest', d), view('U', 'rest', u), view('R', 'rest', r), view('L', 'rest', l), view('L', 'lie', l)]


# ------------------------------------------------------------------------------------------------ Sun Temple T1: the headband
# headwear/cloth/circlet replaced: a turquoise band (Head: Sig::hood) two rows deep round the brow, and one yellow feather
# (Plume: Sig::crest) upright at its side, behind the left temple, its tip two rows over the hair
FEATHER = ['.0.', '040', '043', '.0.']


def headband():
    p = os.path.join(MAPS, 'headwear', 'cloth', 'circlet.pxm')
    out = []
    # D (the circlet's grid, origin 0 -4: its band row 1 at ay 5): the band at rows 0-1, its gem's lower half the band's edge
    d = cells_of(['QUTTTTTTSSSSSSRRQ', 'QSSSSSRRRRRRRRRRQ', '........QQ.......'], 0, -1)
    d.update(cells_of(FEATHER, -4, 15))
    out.append(cells_view(p, 'D', 'rest', d))
    u = cells_of(['QTTTTSSSSSSSRRRQ', 'QSSSRRRRRRRRRRRQ'], -1, 0)
    u.update(cells_of(FEATHER, -5, 0))
    out.append(cells_view(p, 'U', 'rest', u))
    r = cells_of(['QQUTTTTTSSSSSSRRQ', 'QSSSSSRRRRRRRRRRQ', 'QQ...............', '.Q...............'], -1, -2)   # (the band's knot at the nape)
    r.update(cells_of(FEATHER, -5, 13))   # (facing R and L the feather stands at the band's front: apart from the Sylvan sprigs swept back)
    out.append(cells_view(p, 'R', 'rest', r))
    l = cells_of(['QTTTTTTSSSSSSRRQQ', 'QSSSSSRRRRRRRRRSQ', '...............QQ', '...............Q.'], -1, 0)
    l.update(cells_of(FEATHER, -5, -1))
    out.append(cells_view(p, 'L', 'rest', l))
    return out


# ------------------------------------------------------------------------------------------------ Sun Temple T2: the fan
# headwear/cloth/cap in turquoise (Sig::hood), an overlay: its crown quilted (diamond seams, Head 1) and a fan of three yellow
# feathers (Plume) rising from the crown, the middle one upright, the outer two leaning out: the sun's rays
FAN_MID = ['.00.', '0440', '0430', '.00.']
FAN_L = ['00...', '0440.', '.0430', '..00.']
FAN_R = ['...00', '.0220', '0210.', '.00..']


def fan_cap():
    p = os.path.join(MAPS, 'headwear', 'cloth', 'cap.pxm')
    out = []
    for f, mid, left, right, q in (('D', 8, 4, 11, (2, 5)), ('U', 7, 3, 10, (2, 4)), ('R', 10, 6, 13, (2, 7)), ('L', 8, 4, 11, (2, 6))):
        cells = {}
        cells.update(cells_of(FAN_L, -3, left - 3))
        cells.update(cells_of(FAN_R, -3, right + 1))
        cells.update(cells_of(FAN_MID, -3, mid))
        out.append(cells_view(p, f, 'rest', cells))
    return out


# ------------------------------------------------------------------------------------------------ River T3: the lapis plume
# (the ladder: the chapel at T2 is the bigger head, so the T3 morion carries the guild's plume) an overlay on
# headwear/helm/morion's own views at T3: an ostrich plume in lapis (Plume: Sig::crest) set behind the comb and nodding back
PLUME_D = ['..01100...',
           '.0342110..',
           '.03442110.',
           '..0032110.',
           '....021110',
           '.....02110',
           '......000.']
PLUME_R = ['...01100..',
           '..0124430.',
           '.01234440.',
           '.01122300.',
           '01211.....',
           '0120......',
           '.00.......']


def river_plume():
    # head coordinates: D the comb's top at (ax 7, ay -1); R the comb from ax 5 to 10
    relit = {'3': '2', '2': '3', '4': '1', '1': '4'}
    d = axview('D', 'rest', PLUME_D, 6, -4)
    u = axview('U', 'rest', [''.join(relit.get(c, c) for c in r) for r in PLUME_D], 0, -4)
    r = axview('R', 'rest', PLUME_R, 0, -4)
    lrows = [''.join(relit.get(c, c) for c in x[::-1]) for x in PLUME_R]
    l = axview('L', 'rest', lrows, 15 - (0 + len(PLUME_R[0]) - 1), -4)
    return [d, u, r, l]


# ------------------------------------------------------------------------------------------------ Sun Temple T3-T4: the jaguar
# headwear/helm/masked replaced (the masked family's place, every tier from T3): the jaguar warrior's head. The pelt in the
# feather yellow (Plume: Sig::crest) spotted in rosettes, two round ears over the crown, the cat's turquoise eyes (Dye: the
# helm's dominant) and dark nose on the brow, the helm's own metal as the brow band under the upper jaw with its two fangs, the
# man's face in the open jaws, the cheeks and the lower jaw framing it, the pelt hanging down the back of the neck
JAG_D = [
    '....00........00....',
    '...0K30......03K0...',
    '..0K320000000023K0..',
    '.03332K3222K2232210.',
    '.0432K322K2232K2210.',
    '.03328K233332K82210.',
    '.032233444444332210.',
    '.02123344KK44332110.',
    '.0122233K33K3322110.',
    '.021vwxyyyxxxwwv120.',
    '.012.z........z.210.',
    '.021............120.',
    '.0K2............2K0.',
    '.021............120.',
    '.012............210.',
    '..0121........1210..',
    '...012z......z210...',
    '....011222222110....',
    '.....0000000000.....',
]
JAG_R = [
    '.....00.............',
    '....0K30............',
    '...0K320000000......',
    '..0332K322222100....',
    '.034K23222K222210...',
    '.0332K223222238K20..',
    '.032K2232223344440..',
    '.0322K3222233444KK0.',
    '.0K2232222233333K0..',
    '.0212vwxyyyxxwwv0...',
    '.0122110...z..z.....',
    '.0K2211.............',
    '.021120.............',
    '.0122K0.............',
    '.021210.......0z0...',
    '.01221110....02210..',
    '..0111100.....000...',
    '...00000............',
]
JAG_U = [
    '....00........00....',
    '...0K30......03K0...',
    '..0K320000000023K0..',
    '.03332K3222K2232210.',
    '.0432K322K2232K2210.',
    '.03K2223K2223K22210.',
    '.022K3222K2232K2210.',
    '.03222K3222K322K210.',
    '.02K2232K22232K2210.',
    '.022K2222K2222K2110.',
    '..0222K2222K222210..',
    '...01122222221110...',
    '....000000000000....',
]


def jaguar():
    fix = lambda rows: [r.replace(' ', '1') for r in rows]
    d, r, u = fix(JAG_D), fix(JAG_R), fix(JAG_U)
    relit = {'3': '2', '2': '3', 'y': 'w', 'w': 'y'}
    l = [''.join(relit.get(c, c) for c in x[::-1]) for x in r]
    w = len(r[0])
    lx0 = 15 - (w - 1 - 2)   # R starts at ax -2
    return [axview('D', 'rest', d, -2, -2), axview('U', 'rest', u, -2, -2), axview('R', 'rest', r, -2, -2),
            axview('L', 'rest', l, lx0, -2), axview('L', 'lie', l, lx0, -2)]

# ------------------------------------------------------------------------------------------------ Sun Temple T4: the crest
# (T4 up) an overlay on the jaguar: a crest of quetzal feathers (Dye: the turquoise) tipped in the yellow (Plume) rising from
# the jaguar's crown between its ears, fanned facing D and U, a tall upright tuft edge-on facing R and L
SPRAY = ['0...',
         '40..',
         '340.',
         '.00.']


def jaguar_crest():
    """the jaguar's T4 feather sprays: facing D and U one from each ear, up and out past the head; facing R and L one behind"""
    relit = {'4': '2', '3': '2'}
    mir = lambda rows: [''.join(relit.get(c, c) for c in r[::-1]) for r in rows]
    W = 22
    def pair(left):
        g = [list('.' * W) for _ in range(len(SPRAY))]
        for y, r in enumerate(left):
            for x, c in enumerate(r):
                if c != '.':
                    g[y][x] = c
        for y, r in enumerate(mir(left)):
            for x, c in enumerate(r.rjust(len(SPRAY[0]) + 1, '.')):
                if c != '.':
                    g[y][W - len(SPRAY[0]) - 1 + x] = c
        return [''.join(r) for r in g]
    d = axview('D', 'rest', pair(SPRAY), -3, -4)
    u = axview('U', 'rest', pair([r.replace('4', '3') for r in SPRAY]), -3, -4)
    r = axview('R', 'rest', SPRAY, -3, -4)
    l = axview('L', 'rest', mir(SPRAY), 15 - (-3 + len(SPRAY[0]) - 1), -4)
    return [d, u, r, l]


# ------------------------------------------------------------------------------------------------ River T1: the chaperon
# an overlay on headwear/cloth/headscarf (Sig::hood: the guild's linen): the kerchief's crown and its knotted tails behind (the
# liripipe) are the chaperon's; its brow becomes a fat padded roll (the bourrelet, Plume: Sig::crest, lapis) a pixel past the
# head each side, and facing D the tail's end shows beside the left jaw. Slim and low (T1: the hat sits on the hair, D116's
# rags; the ladder's T1), never a beret, a bonnet or a hood
def chaperon():
    p = os.path.join(MAPS, 'headwear', 'cloth', 'headscarf.pxm')
    out = []
    # D (the headscarf's grid, 16 wide, origin 0 0)
    d = cells_of(['.34433322222111.', '0233222221111100'], 4, 0)
    out.append(cells_view(p, 'D', 'rest', d))
    u = cells_of(['0344333322221110', '0233322222111100'], 4, 0)
    out.append(cells_view(p, 'U', 'rest', u))
    r = cells_of(['0344433332222110', '02333222221111100'], 4, 2)
    r = {(y, x - (1 if y == 5 else 0)): c for (y, x), c in r.items()}
    out.append(cells_view(p, 'R', 'rest', r))
    l = cells_of(['0344433332222110', '02333222221111100'], 4, 1)
    out.append(cells_view(p, 'L', 'rest', l))
    return out


# ------------------------------------------------------------------------------------------------ Starspire: the crown's trim
# (the C12f caps) a census man whose helm is a step under his rank's (ladderLook) wears his T3 head at T2: the sun crown's two
# outer points, one column past the head box each side facing D and U, are taken in (an overlay of erases on the crown's
# own views where helm.t3 no longer holds)
def crown_trim():
    p = os.path.join(MAPS, 'headwear', 'helm', 'crown.pxm')
    cells = {(2, -1): '-', (3, -1): '-', (2, 16): '-', (3, 16): '-'}
    out = []
    for f in 'D':   # (the caps read the idle facing D and R; facing R the crown keeps inside the box)
        g = grid(2, 18)
        for (y, x), c in cells.items():
            g[y - 2][x + 1] = c
        out.append(axview(f, 'rest', rows_of(g), -1, 2))
    return out


# ------------------------------------------------------------------------------------------------ the pxm plumbing
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


def map_chars(path):
    """every character the map's own views and legends use (this script's blocks left out), and the person legends'"""
    used = set()
    for s in strip_blocks(open(path, encoding='utf-8').read().split('\n')):
        if s.startswith('|'):
            used.update(s[1:])
        elif s.startswith('legend '):
            used.add(s.split()[1])
    for lg in ('person', 'woman'):
        for s in open(os.path.join(MAPS, 'legend', lg + '.legend'), encoding='utf-8'):
            if s.startswith('legend '):
                used.add(s.split()[1])
    return used


FREE = '!$%&*+,/:;<=>?[]^_`{}~()' + 'kl' + 'DFHIJVWXZ' + '0'


def block(path, tag, desc, views, cond, replace, used_extra=()):
    """views: [(facing, key, rows, regions, origin)] in the authoring alphabet; the map's characters assigned per block"""
    used = map_chars(path) | set(used_extra)
    need = sorted({c for v in views for r in v[2] for c in r if c not in '.-'})
    pool = [c for c in FREE if c not in used]
    assign = {}
    for c in need:
        if c not in ALPHA:
            raise SystemExit(f'{tag}: {c!r} is not in the authoring alphabet')
        assign[c] = pool.pop(0)
    lines = [f'# {TAG} begin {tag}: {desc}']
    lines += [f'legend {assign[c]} {ALPHA[c][0]} {ALPHA[c][1]}' for c in need]
    for f, k, g, reg, org in views:
        o = f' origin {org[0]} {org[1]}' if org else ''
        lines.append(f'view {f} key {k}{o} when {cond}{" replace" if replace else ""}')
        lines += ['|' + ''.join(assign.get(c, c) for c in r) for r in g]
        if reg and any(c != '.' for r in reg for c in r):
            lines.append('regions')
            lines += ['|' + r for r in reg]
    lines.append(f'# {TAG} end {tag}')
    return lines, set(assign.values())


def base_origin(path, facing, key):
    for s in open(path, encoding='utf-8'):
        if s.startswith('view ') and ' when ' not in s:
            p = s.split()
            k = p[p.index('key') + 1] if 'key' in p else 'rest'
            if p[1] == facing and k == key and 'origin' in p:
                i = p.index('origin')
                return int(p[i + 1]), int(p[i + 2])
    return None


def lifted(path, views, up):
    """overlays drawn `up` rows over the base grid's top (and `left` columns before it): their origin moved by that"""
    out = []
    for f, k, g, reg, org in views:
        o = base_origin(path, f, k)
        dx = org[0] if org else 0
        out.append((f, k, g, reg, (o[0] + dx, o[1] + up)))
    return out


def plan():
    """[(map path, tag, lines)]; blocks of one map get distinct characters (each block's legend is the map's)"""
    out = []
    used = {}

    def add(path, tag, desc, views, cond, replace):
        lines, chars = block(path, tag, desc, views, cond, replace, used.get(path, set()))
        used.setdefault(path, set()).update(chars)
        out.append((path, tag, lines))

    hsc = os.path.join(MAPS, 'headwear', 'cloth', 'headscarf.pxm')
    add(hsc, 'river-chaperon', 'the River T1 chaperon: the linen kerchief and its tails on a fat lapis roll (an overlay)',
        chaperon(), 'sighelm-river,!helm.t2', False)
    cap = os.path.join(MAPS, 'headwear', 'cloth', 'cap.pxm')
    add(cap, 'suntemple-fan', 'the Sun Temple T2 quilted cap and its fan of three yellow feathers',
        fan_cap(), 'sighelm-suntemple,helm.t2,!helm.t3', False)
    hood = os.path.join(MAPS, 'headwear', 'cloth', 'hood.pxm')
    add(hood, 'starspire-hood', 'the Starspire deep hood (every tier): its gothic ogive, the face framed star white',
        hood_views(hood, False), 'sighelm-starspire', False)
    add(hood, 'starspire-cowl', 'the Starspire stiff pointed cowl (T2 up, under the crown from T3): the gothic point, the collar, the star clasp',
        hood_views(hood, True), 'sighelm-starspire,helm.t2', False)
    cir = os.path.join(MAPS, 'headwear', 'cloth', 'circlet.pxm')
    add(cir, 'sylvan-leaves', 'the Sylvan T1 leaf circlet: a forest band set with leaves, the end sprigs swept back',
        leaf_circlet(), 'sighelm-sylvan,!helm.t2', True)
    add(cir, 'sylvan-crown', 'the Sylvan T2 leaf crown: five upright leaves high on the head, the outer ones swept back',
        leaf_crown(), 'sighelm-sylvan,helm.t2,!helm.t3', True)
    mor = os.path.join(MAPS, 'headwear', 'helm', 'morion.pxm')
    add(mor, 'river-plume', 'the River T3 morion\'s lapis plume behind the comb (an overlay)',
        river_plume(), 'sighelm-river,helm.t3,!helm.t4', False)
    crn = os.path.join(MAPS, 'headwear', 'helm', 'crown.pxm')
    add(crn, 'starspire-crowntrim', 'the Starspire crown worn a tier under its own (ladderLook): its outer points taken in to the head box',
        crown_trim(), 'sighelm-starspire,helmfam-crown,!helm.t3', False)
    msk = os.path.join(MAPS, 'headwear', 'helm', 'masked.pxm')
    add(msk, 'suntemple-jaguar', 'the Sun Temple jaguar head (T3-T5): the spotted pelt, the ears, the eyes, the fangs, the face in the jaws',
        jaguar(), 'sighelm-suntemple,helm.t3,!helm.t6', True)
    add(msk, 'suntemple-crest', 'the Sun Temple T4 quetzal feather sprays from the jaguar\'s ears (an overlay)',
        jaguar_crest(), 'sighelm-suntemple,helm.t4,!helm.t5', False)
    add(cir, 'suntemple-band', 'the Sun Temple T1 headband: turquoise, two rows deep, one yellow feather upright at its side',
        headband(), 'sighelm-suntemple,!helm.t2', False)
    return out


def write():
    by_file = {}
    for path, tag, lines in plan():
        by_file.setdefault(path, []).extend(lines)
    for path, add in by_file.items():
        src = strip_blocks(open(path, encoding='utf-8').read().split('\n'))
        i = next(n for n, s in enumerate(src) if s.startswith('view '))
        new = src[:i] + add + src[i:]
        open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(new))
        print(f'{os.path.relpath(path, ROOT)}: +{len(add)} lines')


def verify():
    bad = 0
    for path in sorted({p for p, _, _ in plan()}):
        rel = os.path.relpath(path, ROOT).replace('\\', '/')
        old = subprocess.run(['git', '-C', ROOT, 'show', f'{BASE}:{rel}'], capture_output=True, text=True, encoding='utf-8').stdout.split('\n')
        now = open(path, encoding='utf-8').read().split('\n')
        if strip_blocks(now) != old:
            print(f'FAIL {rel}: outside its {TAG} blocks the map differs from {BASE[:8]}')
            bad += 1
        else:
            print(f'ok   {rel}: {len(now) - len(old)} lines added, all inside {TAG} blocks; every drawn view as it was')
    return bad



def rle_len(cells):
    """pxc.mjs's RLE plane length: control < 128 a run of clear cells, >= 128 literal bytes"""
    n, i = 0, 0
    while i < len(cells):
        if cells[i] == 0:
            k = 0
            while i < len(cells) and cells[i] == 0 and k < 128:
                k += 1
                i += 1
            n += 1
        else:
            j = i
            while j < len(cells) and j - i < 128 and cells[j] != 0:
                j += 1
            n += 1 + (j - i)
            i = j
    return n


def cost():
    """each block's pool bytes, estimated as pxc.mjs packs them: 35 B a view record, its RLE plane (identical planes once),
    the condition string once"""
    tot = 0
    for path, tag, lines in plan():
        planes, b, conds, cur = set(), 0, set(), None
        views = []
        for s in lines:
            if s.startswith('view '):
                cur = []
                views.append(cur)
                conds.add(s.split(' when ', 1)[1].replace(' replace', ''))
            elif s.startswith('|') and cur is not None:
                cur.append(s[1:])
            elif s.startswith('regions'):
                cur = None
        for v in views:
            w = max(len(r) for r in v)
            cells = [0 if c == '.' else 1 for r in v for c in r.ljust(w, '.')]
            key = tuple(r.ljust(w, '.') for r in v)
            b += 35
            if key not in planes:
                planes.add(key)
                b += rle_len(cells)
        b += sum(2 + len(c) for c in conds)
        tot += b
        print(f'{tag:28s} {len(views)} views  ~{b} B')
    print(f'{"total":28s} ~{tot} B')


if __name__ == '__main__':
    if '--verify' in sys.argv:
        sys.exit(1 if verify() else 0)
    if '--cost' in sys.argv:
        cost()
        sys.exit(0)
    if '--print' in sys.argv:
        for path, tag, lines in plan():
            print('\n'.join(lines))
        sys.exit(0)
    write()
