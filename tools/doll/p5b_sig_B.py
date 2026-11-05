#!/usr/bin/env python3
"""tools/doll/p5b_sig_B.py (P5b production, area B, docs/STYLE_P5B.md 4-5): Dune, Steppe and Marsh heads T1-T4 as ADDED views.

The slice's method (tools/doll/p5b_sig.py): every block this script writes is new `when sighelm-<people>,...` views (the
resolver's per-slot people flag) and the local legend characters they use, inserted before a part's first view (or before the
first P5b block already there), so a people's `replace` view is tried before the generic tier views. No drawn view's cells
change. Re-running replaces this script's own blocks only (its own tag); other areas' P5b blocks are left as they are.

  python tools/doll/p5b_sig_B.py            write the blocks into rpg/art/doll/maps
  python tools/doll/p5b_sig_B.py --verify   every original line of each touched map is still there, in order, and every added
                                            line sits inside a P5b block (git show <base>:<file>)
  python tools/doll/p5b_sig_B.py --print    print the blocks

The people's pair on a head is drawn in the helm slot's dye pair (legend `dye` / `dye2`: the compositor moves them to the helm
maker's dominant and accent, fp::Sig dom / acc), so the cloth reads in the people's colours on any helm material; metal,
leather and head cells keep the piece's own ramps (the material reads first, STYLE_P5B 2.3). Light from the top left.

  Dune T1-T2    headwear/cloth/turban: the cream turban (the soft head's Head: the accent) gains its teal brow wrap (dye); T2
                the spiked cap's needle rises out of its crown (the cap under the turban, STYLE_P5B 4)
  Dune T3-T4    headwear/helm/veil (the kulah): a cream turban wound round the bowl's base under the needle spike, a teal
                wrap crossing it and a brass brooch at the front; the mail aventail as drawn
  Steppe T2     headwear/cloth/furhat: the felt crown rises to a peak with a rust knot at its tip (the peaked felt cap; its fur
                lappets the hat's own flaps)
  Steppe T3-T4  headwear/helm/plumed: the pointed helm gains its fur: a fur roll round the brow and sky-blue felt lappets edged
                in fur hanging beside the face over the cheek guards (the fur-lapped cone), the horsetail as drawn
  Marsh T1      headwear/cloth/hood: the hide hood's cowl and brow fringed (moss strands with bone tips) and its crown lowered and
                flattened (a hide hood, never the round cloth hood)
  Marsh T2      headwear/helm/leathercap (T2): a bone crest down the crown and two moss feathers raked back from the band
  Marsh T3      headwear/helm/kettle: the tasselled reed war hat (the kettle's `tassels` views, keyed to the Marsh helm)
  Marsh T4      headwear/helm/warhat: the wide war hat drooping, a moss wrap round its crown, bone-tipped tassels on its cords
                and a reed tuft rising from the crown
"""
import os
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
MAPS = os.path.join(ROOT, 'rpg', 'art', 'doll', 'maps')
BASE = '37331730328157e70cdd6b26c057fc63f5929a7c'   # STYLE_P5B's commit: the maps before P5b
TAG = '(P5b SIG B)'
ANY_TAG = '# (P5b SIG'   # every P5b area's blocks (the slice's `(P5b SIG)`, `(P5b SIG A)`, ...): verify strips them all

# the design alphabet's new characters (local legend lines in each touched part; none is drawn by those parts' own views)
PAIR_LEGEND = ['legend a dye 0', 'legend b dye 1', 'legend c dye 2', 'legend d dye 3', 'legend e dye 4',
               'legend A dye2 0', 'legend B dye2 1', 'legend C dye2 2', 'legend F dye2 3', 'legend I dye2 4']
# a soft head (headwear/cloth/**: the guise path) is no helm piece, so its dye cells take no helm maker's pair: its cloth draws
# in Head (the accent, sigLookOf) and Plume (the dominant: the crest's and the plume's colour), steps 0-4 as '0'-'4'
SOFT_LEGEND = ['legend 0 plume 0', 'legend 1 plume 1', 'legend 2 plume 2', 'legend 3 plume 3', 'legend 4 plume 4']
RELIT = {   # a drawing turned to face the other way: its lit edge back on the light's side
    'b': 'd', 'd': 'b', 'e': 'd', 'B': 'F', 'F': 'B', 'I': 'F', 'w': 'y', 'y': 'w', 'z': 'y', 'g': 'i', 'i': 'g', 'j': 'i',
    'R': 'T', 'T': 'R', 'U': 'T', 'N': 'P', 'P': 'N', '1': '3', '3': '1', '4': '3'}


# ------------------------------------------------------------------------------------------------ helpers
def mirror(rows, relight=True):
    return [''.join(RELIT.get(c, c) if relight else c for c in r[::-1]) for r in rows]


def remap(rows, table, only_rows=None):
    return [''.join(table.get(c, c) if (only_rows is None or y in only_rows) else c for c in r) for y, r in enumerate(rows)]


def keep_only(rows, ys):
    """an overlay holding only rows ys of a grid ('.' everywhere else)"""
    return [r if y in ys else '.' * len(r) for y, r in enumerate(rows)]


def stack(top, rows):
    return top + rows


def blank(w, h):
    return ['.' * w] * h


def put(rows, y, x, s):
    """write string s into rows at (y, x); '.' in s keeps what is there"""
    r = list(rows[y])
    for i, c in enumerate(s):
        if c != '.':
            r[x + i] = c
    rows[y] = ''.join(r)


def read_views(path):
    """{(facing, key, when): (rows, origin)} of a part's views ('' when for the base)"""
    out, cur = {}, None
    for line in open(path, encoding='utf-8'):
        s = line.rstrip('\n')
        if s.startswith('view '):
            p = s.split()
            w = s.split(' when ', 1)[1] if ' when ' in s else ''
            key = p[p.index('key') + 1] if 'key' in p else 'rest'
            org = (int(p[p.index('origin') + 1]), int(p[p.index('origin') + 2])) if 'origin' in p else None
            cur = (p[1], key, w)
            out[cur] = ([], org)
            mode = 'g'
        elif s.startswith('regions') or s.startswith('build'):
            mode = 'x'
        elif s.startswith('|') and cur is not None and mode == 'g':
            out[cur][0].append(s[1:].split('|')[0])
    return out


def read_regions(path):
    """{(facing, key, when): region rows} of a part's views that carry a regions plane"""
    out, cur, mode = {}, None, None
    for line in open(path, encoding='utf-8'):
        s = line.rstrip(chr(10))
        if s.startswith('view '):
            p = s.split()
            w = s.split(' when ', 1)[1] if ' when ' in s else ''
            cur = (p[1], p[p.index('key') + 1] if 'key' in p else 'rest', w)
            mode = 'g'
        elif s.startswith('regions'):
            mode = 'r'
            out[cur] = []
        elif s.startswith('build'):
            mode = 'b'
        elif s.startswith('|') and cur is not None and mode == 'r':
            out[cur].append(s[1:].split('|')[0])
    return out


def base_rows(path, facing, key='rest', when=''):
    return read_views(path)[(facing, key, when)]


# ------------------------------------------------------------------------------------------------ the bytes
# (the pool's 2.5 MiB, no compression: area B's cap is 4,000 B) every view is a 35 B record and its RLE plane, where a run of
# clear cells costs one byte and every drawn or erasing cell one: so an overlay draws only what changes, erases ('-') only
# the base's own opaque cells, shares one plane between facings where the drawing allows (the pool stores a plane once), and
# leaves the lying pose (`L lie`, never a facing the game composes standing) to the base drawing
FACINGS = ('D', 'U', 'R', 'L')


def carve(rows, ys, wd=None):
    """'-' on rows ys of a base grid where the base is opaque ('.' elsewhere): the base's own cells there taken out (and the
    hair under them)"""
    wd = wd or len(rows[0])
    return {y: ''.join('-' if y < len(rows) and x < len(rows[y]) and rows[y][x] not in '.-' else '.' for x in range(wd)) for y in ys}


def grid(wd, h, cells):
    """a grid wd x h from {row: string} (strings laid from column 0; '.' keeps)"""
    g = blank(wd, h)
    for y in sorted(cells):
        put(g, y, 0, cells[y].ljust(wd, '.')[:wd])
    return g


def merge(*dicts):
    out = {}
    for d in dicts:
        for y, s in d.items():
            if y in out:
                a = list(out[y].ljust(len(s), '.'))
                for i, c in enumerate(s):
                    if c != '.':
                        a[i] = c
                out[y] = ''.join(a)
            else:
                out[y] = s
    return out


# ------------------------------------------------------------------------------------------------ the bytes
# (the pool's 2.5 MiB, no compression: area B's cap is 4,000 B) every view is a 35 B record and its RLE plane, where a run of
# clear cells costs one byte and every drawn or erasing cell one: so an overlay draws only what changes, erases ('-') only
# the base's own opaque cells, shares one plane between facings where the drawing allows (the pool stores a plane once), and
# leaves the lying pose (`L lie`, never a facing the game composes standing) to the base drawing
FACINGS = ('D', 'U', 'R', 'L')


def carve(rows, ys, wd=None):
    """'-' on rows ys of a base grid where the base is opaque ('.' elsewhere): the base's own cells there taken out (and the
    hair under them)"""
    wd = wd or len(rows[0])
    return {y: ''.join('-' if y < len(rows) and x < len(rows[y]) and rows[y][x] not in '.-' else '.' for x in range(wd)) for y in ys}


def grid(wd, h, cells):
    """a grid wd x h from {row: string} (strings laid from column 0; '.' keeps)"""
    g = blank(wd, h)
    for y in sorted(cells):
        put(g, y, 0, cells[y].ljust(wd, '.')[:wd])
    return g


def merge(*dicts):
    out = {}
    for d in dicts:
        for y, s in d.items():
            if y in out:
                a = list(out[y].ljust(len(s), '.'))
                for i, c in enumerate(s):
                    if c != '.':
                        a[i] = c
                out[y] = ''.join(a)
            else:
                out[y] = s
    return out


# ------------------------------------------------------------------------------------------------ the bytes
# (the pool's 2.5 MiB, no compression: area B's cap is 4,000 B) every view is a 35 B record and its RLE plane, where a run of
# clear cells costs one byte and every drawn or erasing cell one: so an overlay draws only what changes, erases ('-') only
# the base's own opaque cells, shares one plane between facings where the drawing allows (the pool stores a plane once), and
# leaves the lying pose (`L lie`, never a facing the game composes standing) to the base drawing
FACINGS = ('D', 'U', 'R', 'L')


def carve(rows, ys, wd=None):
    """'-' on rows ys of a base grid where the base is opaque ('.' elsewhere): the base's own cells there taken out (and the
    hair under them)"""
    wd = wd or len(rows[0])
    return {y: ''.join('-' if y < len(rows) and x < len(rows[y]) and rows[y][x] not in '.-' else '.' for x in range(wd)) for y in ys}


def grid(wd, h, cells):
    """a grid wd x h from {row: string} (strings laid from column 0; '.' keeps)"""
    g = blank(wd, h)
    for y in sorted(cells):
        put(g, y, 0, cells[y].ljust(wd, '.')[:wd])
    return g


def merge(*dicts):
    out = {}
    for d in dicts:
        for y, s in d.items():
            if y in out:
                a = list(out[y].ljust(len(s), '.'))
                for i, c in enumerate(s):
                    if c != '.':
                        a[i] = c
                out[y] = ''.join(a)
            else:
                out[y] = s
    return out


# ------------------------------------------------------------------------------------------------ the bytes
# (the pool's 2.5 MiB, no compression: area B's cap is 4,000 B) every view is a 35 B record and its RLE plane, where a run of
# clear cells costs one byte and every drawn or erasing cell one: so an overlay draws only what changes, erases ('-') only
# the base's own opaque cells, shares one plane between facings where the drawing allows (the pool stores a plane once), and
# leaves the lying pose (`L lie`, never a facing the game composes standing) to the base drawing
FACINGS = ('D', 'U', 'R', 'L')


def carve(rows, ys, wd=None):
    """'-' on rows ys of a base grid where the base is opaque ('.' elsewhere): the base's own cells there taken out (and the
    hair under them)"""
    wd = wd or len(rows[0])
    return {y: ''.join('-' if y < len(rows) and x < len(rows[y]) and rows[y][x] not in '.-' else '.' for x in range(wd)) for y in ys}


def grid(wd, h, cells):
    """a grid wd x h from {row: string} (strings laid from column 0; '.' keeps)"""
    g = blank(wd, h)
    for y in sorted(cells):
        put(g, y, 0, cells[y].ljust(wd, '.')[:wd])
    return g


def merge(*dicts):
    out = {}
    for d in dicts:
        for y, s in d.items():
            if y in out:
                a = list(out[y].ljust(len(s), '.'))
                for i, c in enumerate(s):
                    if c != '.':
                        a[i] = c
                out[y] = ''.join(a)
            else:
                out[y] = s
    return out


# ------------------------------------------------------------------------------------------------ the bytes
# (the pool's 2.5 MiB, no compression: area B's cap is 4,000 B) every view is a 35 B record and its RLE plane, where a run of
# clear cells costs one byte and every drawn or erasing cell one: so an overlay draws only what changes, erases ('-') only
# the base's own opaque cells, shares one plane between facings where the drawing allows (the pool stores a plane once), and
# leaves the lying pose (`L lie`, never a facing the game composes standing) to the base drawing
FACINGS = ('D', 'U', 'R', 'L')


def carve(rows, ys, wd=None):
    """'-' on rows ys of a base grid where the base is opaque ('.' elsewhere): the base's own cells there taken out (and the
    hair under them)"""
    wd = wd or len(rows[0])
    return {y: ''.join('-' if y < len(rows) and x < len(rows[y]) and rows[y][x] not in '.-' else '.' for x in range(wd)) for y in ys}


def grid(wd, h, cells):
    """a grid wd x h from {row: string} (strings laid from column 0; '.' keeps)"""
    g = blank(wd, h)
    for y in sorted(cells):
        put(g, y, 0, cells[y].ljust(wd, '.')[:wd])
    return g


def merge(*dicts):
    out = {}
    for d in dicts:
        for y, s in d.items():
            if y in out:
                a = list(out[y].ljust(len(s), '.'))
                for i, c in enumerate(s):
                    if c != '.':
                        a[i] = c
                out[y] = ''.join(a)
            else:
                out[y] = s
    return out


# ------------------------------------------------------------------------------------------------ the bytes
# (the pool's 2.5 MiB, no compression: area B's cap is 4,000 B) every view is a 35 B record and its RLE plane, where a run of
# clear cells costs one byte and every drawn or erasing cell one: so an overlay draws only what changes, erases ('-') only
# the base's own opaque cells, shares one plane between facings where the drawing allows (the pool stores a plane once), and
# leaves the lying pose (`L lie`, never a facing the game composes standing) to the base drawing
FACINGS = ('D', 'U', 'R', 'L')


def carve(rows, ys, wd=None):
    """'-' on rows ys of a base grid where the base is opaque ('.' elsewhere): the base's own cells there taken out (and the
    hair under them)"""
    wd = wd or len(rows[0])
    return {y: ''.join('-' if y < len(rows) and x < len(rows[y]) and rows[y][x] not in '.-' else '.' for x in range(wd)) for y in ys}


def grid(wd, h, cells):
    """a grid wd x h from {row: string} (strings laid from column 0; '.' keeps)"""
    g = blank(wd, h)
    for y in sorted(cells):
        put(g, y, 0, cells[y].ljust(wd, '.')[:wd])
    return g


def merge(*dicts):
    out = {}
    for d in dicts:
        for y, s in d.items():
            if y in out:
                a = list(out[y].ljust(len(s), '.'))
                for i, c in enumerate(s):
                    if c != '.':
                        a[i] = c
                out[y] = ''.join(a)
            else:
                out[y] = s
    return out


# ------------------------------------------------------------------------------------------------ the bytes
# (the pool's 2.5 MiB, no compression: area B's cap is 4,000 B) every view is a 35 B record and its RLE plane, where a run of
# clear cells costs one byte and every drawn or erasing cell one: so an overlay draws only what changes, erases ('-') only
# the base's own opaque cells, shares one plane between facings where the drawing allows (the pool stores a plane once), and
# leaves the lying pose (`L lie`, never a facing the game composes standing) to the base drawing
FACINGS = ('D', 'U', 'R', 'L')


def carve(rows, ys, wd=None):
    """'-' on rows ys of a base grid where the base is opaque ('.' elsewhere): the base's own cells there taken out (and the
    hair under them)"""
    wd = wd or len(rows[0])
    return {y: ''.join('-' if y < len(rows) and x < len(rows[y]) and rows[y][x] not in '.-' else '.' for x in range(wd)) for y in ys}


def grid(wd, h, cells):
    """a grid wd x h from {row: string} (strings laid from column 0; '.' keeps)"""
    g = blank(wd, h)
    for y in sorted(cells):
        put(g, y, 0, cells[y].ljust(wd, '.')[:wd])
    return g


def merge(*dicts):
    out = {}
    for d in dicts:
        for y, s in d.items():
            if y in out:
                a = list(out[y].ljust(len(s), '.'))
                for i, c in enumerate(s):
                    if c != '.':
                        a[i] = c
                out[y] = ''.join(a)
            else:
                out[y] = s
    return out


# ------------------------------------------------------------------------------------------------ the bytes
# (the pool's 2.5 MiB, no compression: area B's cap is 4,000 B) every view is a 35 B record and its RLE plane, where a run of
# clear cells costs one byte and every drawn or erasing cell one: so an overlay draws only what changes, erases ('-') only
# the base's own opaque cells, shares one plane between facings where the drawing allows (the pool stores a plane once), and
# leaves the lying pose (`L lie`, never a facing the game composes standing) to the base drawing
FACINGS = ('D', 'U', 'R', 'L')


def carve(rows, ys, wd=None):
    """'-' on rows ys of a base grid where the base is opaque ('.' elsewhere): the base's own cells there taken out (and the
    hair under them)"""
    wd = wd or len(rows[0])
    return {y: ''.join('-' if y < len(rows) and x < len(rows[y]) and rows[y][x] not in '.-' else '.' for x in range(wd)) for y in ys}


def grid(wd, h, cells):
    """a grid wd x h from {row: string} (strings laid from column 0; '.' keeps)"""
    g = blank(wd, h)
    for y in sorted(cells):
        put(g, y, 0, cells[y].ljust(wd, '.')[:wd])
    return g


def merge(*dicts):
    out = {}
    for d in dicts:
        for y, s in d.items():
            if y in out:
                a = list(out[y].ljust(len(s), '.'))
                for i, c in enumerate(s):
                    if c != '.':
                        a[i] = c
                out[y] = ''.join(a)
            else:
                out[y] = s
    return out


# ------------------------------------------------------------------------------------------------ the bytes
# (the pool's 2.5 MiB, no compression: area B's cap is 4,000 B) every view is a 35 B record and its RLE plane, where a run of
# clear cells costs one byte and every drawn or erasing cell one: so an overlay draws only what changes, erases ('-') only
# the base's own opaque cells, shares one plane between facings where the drawing allows (the pool stores a plane once), and
# leaves the lying pose (`L lie`, never a facing the game composes standing) to the base drawing
FACINGS = ('D', 'U', 'R', 'L')


def carve(rows, ys, wd=None):
    """'-' on rows ys of a base grid where the base is opaque ('.' elsewhere): the base's own cells there taken out (and the
    hair under them)"""
    wd = wd or len(rows[0])
    return {y: ''.join('-' if y < len(rows) and x < len(rows[y]) and rows[y][x] not in '.-' else '.' for x in range(wd)) for y in ys}


def grid(wd, h, cells):
    """a grid wd x h from {row: string} (strings laid from column 0; '.' keeps)"""
    g = blank(wd, h)
    for y in sorted(cells):
        put(g, y, 0, cells[y].ljust(wd, '.')[:wd])
    return g


def merge(*dicts):
    out = {}
    for d in dicts:
        for y, s in d.items():
            if y in out:
                a = list(out[y].ljust(len(s), '.'))
                for i, c in enumerate(s):
                    if c != '.':
                        a[i] = c
                out[y] = ''.join(a)
            else:
                out[y] = s
    return out


# ------------------------------------------------------------------------------------------------ the bytes
# (the pool's 2.5 MiB, no compression: area B's cap is 4,000 B) every view is a 35 B record and its RLE plane, where a run of
# clear cells costs one byte and every drawn or erasing cell one: so an overlay draws only what changes, erases ('-') only
# the base's own opaque cells, shares one plane between facings where the drawing allows (the pool stores a plane once), and
# leaves the lying pose (`L lie`, never a facing the game composes standing) to the base drawing
FACINGS = ('D', 'U', 'R', 'L')


def carve(rows, ys, wd=None):
    """'-' on rows ys of a base grid where the base is opaque ('.' elsewhere): the base's own cells there taken out (and the
    hair under them)"""
    wd = wd or len(rows[0])
    return {y: ''.join('-' if y < len(rows) and x < len(rows[y]) and rows[y][x] not in '.-' else '.' for x in range(wd)) for y in ys}


def grid(wd, h, cells):
    """a grid wd x h from {row: string} (strings laid from column 0; '.' keeps)"""
    g = blank(wd, h)
    for y in sorted(cells):
        put(g, y, 0, cells[y].ljust(wd, '.')[:wd])
    return g


def merge(*dicts):
    out = {}
    for d in dicts:
        for y, s in d.items():
            if y in out:
                a = list(out[y].ljust(len(s), '.'))
                for i, c in enumerate(s):
                    if c != '.':
                        a[i] = c
                out[y] = ''.join(a)
            else:
                out[y] = s
    return out


# ------------------------------------------------------------------------------------------------ the bytes
# (the pool's 2.5 MiB, no compression: area B's cap is 4,000 B) every view is a 35 B record and its RLE plane, where a run of
# clear cells costs one byte and every drawn or erasing cell one: so an overlay draws only what changes, erases ('-') only
# the base's own opaque cells, shares one plane between facings where the drawing allows (the pool stores a plane once), and
# leaves the lying pose (`L lie`, never a facing the game composes standing) to the base drawing
FACINGS = ('D', 'U', 'R', 'L')


def carve(rows, ys, wd=None):
    """'-' on rows ys of a base grid where the base is opaque ('.' elsewhere): the base's own cells there taken out (and the
    hair under them)"""
    wd = wd or len(rows[0])
    return {y: ''.join('-' if y < len(rows) and x < len(rows[y]) and rows[y][x] not in '.-' else '.' for x in range(wd)) for y in ys}


def grid(wd, h, cells):
    """a grid wd x h from {row: string} (strings laid from column 0; '.' keeps)"""
    g = blank(wd, h)
    for y in sorted(cells):
        put(g, y, 0, cells[y].ljust(wd, '.')[:wd])
    return g


def merge(*dicts):
    out = {}
    for d in dicts:
        for y, s in d.items():
            if y in out:
                a = list(out[y].ljust(len(s), '.'))
                for i, c in enumerate(s):
                    if c != '.':
                        a[i] = c
                out[y] = ''.join(a)
            else:
                out[y] = s
    return out


# ------------------------------------------------------------------------------------------------ Dune
TURBAN = os.path.join(MAPS, 'headwear', 'cloth', 'turban.pxm')
VEIL = os.path.join(MAPS, 'headwear', 'helm', 'veil.pxm')
# T1-T2: the turban wound two rows lower (T1's head rows: 2 over the crown; the drawn turban stands 4): its top two rows taken
# out, its third the new crown's outline (two columns in each side, so the top rounds), its fourth the crown's lit wrap; the
# lowest two wraps in the people's teal (Plume steps 1-4 as '1'-'4': a soft head's dominant), the wound shading kept step for
# step, the brooch (metal 2) as drawn; and the headcloth's TAIL (STYLE_P5B 5 Dune: SCARF, the headcloth's tail) falling from
# under the turban at the left ear to the shoulder, cream with a teal end (facing D at the figure's left, R / L behind the
# head, U down the back)
TEAL = {'T': '3', 'U': '4', 'S': '2', 'R': '1'}
CROWN = {   # facing: (row 3, row 4) of the lowered crown
    'D': ('....QQQQQQQQQQ....', '.QQTUUUTTSSSSRRQ..'),
    'R': ('.....QQQQQQQQQQ.....', '..QQTUUUTTSSSSSRQQ..'),
}
TAIL = {   # facing: {row: cells} (Head: Q outline, T S R the cloth; Plume '2' '1' the teal end): T1 only (the T2 turban sheds
           # it: T2's head stays under T3's by the ladder's 5 %)
    'D': {10: '...............TQ.', 11: '...............SQ.', 12: '...............2Q.', 13: '...............QQ.'},
    'U': {10: '...........QTSQ....', 11: '...........QSRQ....', 12: '...........Q21Q....', 13: '............QQ.....'},
    'R': {9: 'QTS.................', 10: 'QTSQ................', 11: 'QSRQ................', 12: 'Q21Q................',
          13: '.QQ.................'},
}
SLIM = {   # the turban one column in at its widest rows (5-8): T1's head inside the hair's own width (the ladder's T1 area)
    'D': {5: '-Q..............Q-', 6: '-Q..............Q-', 7: '-Q..............Q-', 8: '-Q..............Q-'},
    'R': {5: '-Q..................', 6: '-Q..................', 7: '-Q..................', 8: '-Q..................'},
}


def dune_turban():
    views = []
    V = read_views(TURBAN)
    for f in FACINGS:
        rows = V[(f, 'rest', '')][0]
        wd = len(rows[0])
        c2, c3 = CROWN['D' if f in 'DU' else 'R']
        crown = {3: c2, 4: c3}   # (placed symmetrically: L takes R's, lit from the left)
        band = {y: remap(rows, TEAL, {7, 8})[y] for y in (7, 8)}
        slim = SLIM['D'] if f in 'DU' else SLIM['R'] if f == 'R' else {y: r[::-1] for y, r in SLIM['R'].items()}
        cells = merge(carve(rows, (0, 1, 2, 3, 4)), crown, band, slim)
        views.append((f, 'rest', grid(wd, 10, cells), None, None))
    return SOFT_LEGEND, views


def dune_tail():
    views = []
    for f in FACINGS:
        tail = TAIL.get(f) or {y: mirror([s])[0] for y, s in TAIL['R'].items()}
        views.append((f, 'rest', grid(18 if f in 'DU' else 20, 14, tail), None, None))
    return [], views


# T2: the turban over the spiked cap: the cap's steel crown and the stub of its spike one row over the turban's (3 rows over
# the crown, T2's cap): a lit steel knob, its dark side right, at the crown's centre (D and U one plane, R and L one plane)
def dune_spike():
    views = []
    for f in FACINGS:
        wd = 18 if f in 'DU' else 20
        views.append((f, 'rest', grid(wd, 2, {1: '.' * (7 if f in 'DU' else 8) + 'vyxv'}), None, None))
    return [], views


# T3-T4: the kulah's turban band, rows 6-8 of the veil's grid (the bowl's widest courses down to its rim): the people's teal
# (Plume as '0'-'4': the helm's crest colour, the dominant; a plain slot, so B8's 48-colour budget never snaps it as it does a
# dye) wound round the bowl one column past it each side, its wraps' shadow lines (1) running down to the right, a brass
# brooch (metal 2) at the front where they cross; the needle and the bowl's crown (rows 0-5) stay steel over it. D and U one
# plane; R the roll side on, the brooch at the front; L the R mirrored and re-lit
BAND_D = {6: '....034332221210....', 7: '...03312433222110...', 8: '..0333312NO2221210..'}
BAND_R = {6: '.....034332221210....', 7: '....03312433222PO0...', 8: '...0333312433222N10..'}


def dune_kulah():
    views = []
    for f in FACINGS:
        band = BAND_D if f in 'DU' else BAND_R if f == 'R' else {y: mirror([s])[0] for y, s in BAND_R.items()}
        views.append((f, 'rest', grid(20 if f in 'DU' else 21, 9, band), None, None))
    return SOFT_LEGEND, views


# ------------------------------------------------------------------------------------------------ Steppe
FURHAT = os.path.join(MAPS, 'headwear', 'cloth', 'furhat.pxm')
PLUMED = os.path.join(MAPS, 'headwear', 'helm', 'plumed.pxm')
# T1: the felt crown two rows lower (the hat stands 4 rows over the crown; T1's cap is 2): rows 0-1 taken out, row 2 the
# crown's top (rounded), row 3 its lit course; the fur ear flaps grown a row and flared out to the jaw (the herder's hat that
# ties under the chin: the Steppe outline beside the face at T1). T2 (the peaked felt cap): row 0 out but for a rust knot
# (Plume: the dominant, a crest's 6 rows), the crown climbing to a peak on rows 1-3 (3 over the crown, T2's cap); the flaps
# as T1's
FELT_CROWN = {
    1: {'D': {2: '....QQQQQQQQQQ....', 3: '...QTUUUTTSSSRQ...'},
        'R': {2: '.....QQQQQQQQQQ.....', 3: '....QTUUUTTSSSRQ....'},
        'L': {2: '.......QQQQQQQQQQ...', 3: '......QTUUUTTSSSRQ..'}},
    2: {'D': {0: '........12........', 1: '.......QUTQ.......', 2: '.....QTUUTSRQ.....', 3: '...QTUUTTSSSRRQ...'},
        'R': {0: '.........12.........', 1: '........QUTQ........', 2: '......QTUUTSRQ......', 3: '....QTUUTTSSSRRQ....'},
        'L': {0: '...........12.......', 1: '..........QUTQ......', 2: '........QTUUTSRQ....', 3: '......QTUUTTSSSRRQ..'}},
}
FLAPS = {   # T1: the fur brim a column in each side (inside the head's width: the ladder's T1 area) and the ear flaps a row
            # longer, to the jaw
    'D': {6: '-f..............f-', 7: '-f..............f-', 8: '-f..............f-',
          13: '.fhgf........fggf.', 14: '..ff..........ff..'},
    'R': {5: '-f..................', 6: '-f..................', 7: '-f..................', 8: '-f..................',
          13: '......fhggf.........', 14: '.......ff...........'},
    'L': {5: '..................f-', 6: '..................f-', 7: '..................f-', 8: '..................f-',
          13: '.........fhggf......', 14: '...........ff.......'},
}
FLAP_END = {   # T2: the lappets end at the jaw (row 11, their hem in the fur's dark step)
    'D': '.ffff........ffff.', 'R': '......ffff..........', 'L': '.........ffff.......'}


def steppe_felt(tier):
    views = []
    V = read_views(FURHAT)
    for f in FACINGS:
        rows = V[(f, 'rest', '')][0]
        wd = len(rows[0])
        k = 'D' if f in 'DU' else f
        cut = carve(rows, (0, 1)) if tier == 1 else merge(carve(rows, (0, 12, 13)), {11: FLAP_END[k]})
        cells = merge(cut, FELT_CROWN[tier][k], FLAPS[k] if tier == 1 else {})
        views.append((f, 'rest', grid(wd, 15, cells), None, None))
    return SOFT_LEGEND, views


# T3-T4: the pointed helm's lamellar guards (the people's horizontal rows, STYLE_P5B 5 Steppe): its cheek guards grown into
# flaps of laced plates, a course of plates (lit left, the turn, the shade right) over each lacing row (the outline step),
# flaring a column outward each course down to the jaw; facing R / L the neck guard behind the ear in the same courses, the
# front cheek guard as drawn. The helm's own metal: no new colour. (Facing U the cape hangs over the nape: the base's)
LAM_D = {13: '..vyyv........vxwv..', 14: '..vvvv........vvvv..', 15: '.vyyxv........vxwwv.', 16: '.vvvvv........vvvvv.',
         17: '.vyxxv........vxwwv.', 18: '..vvv..........vvv..'}
LAM_R = {13: '...vyyyxxv...........', 14: '...vvvvvvv...........', 15: '..vyyyxxxv...........', 16: '..vvvvvvvv...........',
         17: '..vyyxxxwv...........', 18: '...vvvvvv............'}


def steppe_lamellar():
    views = []
    for f in ('D', 'R', 'L'):
        lam = LAM_D if f == 'D' else LAM_R if f == 'R' else {y: mirror([s])[0] for y, s in LAM_R.items()}
        views.append((f, 'rest', grid(20 if f == 'D' else 21, 19, lam), None, None))
    return [], views


# ------------------------------------------------------------------------------------------------ Marsh
HEADSCARF = os.path.join(MAPS, 'headwear', 'cloth', 'headscarf.pxm')
KETTLE = os.path.join(MAPS, 'headwear', 'helm', 'kettle.pxm')
WARHAT = os.path.join(MAPS, 'headwear', 'helm', 'warhat.pxm')
# T1: the bog cap (STYLE_P5B 5 Marsh "From: the Tollund bog cap"): the hide kerchief (headwear/cloth/headscarf; Head: the
# accent, bone) fringed in moss (Plume '0'-'4': the dominant): strands over the brow from its hem, ragged strands hanging
# beside the face to the jaw from its side ends (bone-tipped, Head 'T'), its knot's tails behind turned to a hank of moss
# strands. (The hood the plan names is the size of a third of the figure: with the body's band it measured 40.6 % people
# colour, over S2's 30; the cap keeps the fringe, the moss and the hide at 20-25 %)
FRINGE = {
    'D': {7: '....3232212.....', 9: '..2..........2..', 10: '..1..........1..', 11: '..T..........T..'},
    'U': {10: '.....12.21......', 11: '.....1..2.......', 12: '.....2..1.......', 13: '.....T..2.......', 14: '........T.......'},
    'R': {7: '...........322121..', 10: '.2.2...............', 11: '.1.1...............', 12: '.2.21..............',
          13: '.1.T2..............', 14: '.T..T..............'},
}


def marsh_cap1():
    views = []
    for f in FACINGS:
        fr = FRINGE.get(f) or {y: s[::-1] for y, s in FRINGE['R'].items()}
        views.append((f, 'rest', grid(16 if f in 'DU' else 19, 15, fr), None, None))
    return SOFT_LEGEND, views


# T2: the rawhide cap: the kettle (form2) at T2, which a T2 band-1 helm draws in the leather (sighelmleather: the hide), under
# a bone crest (dye2: the accent) running over the crown from brow to nape a row over the cap's top, its vertebrae notched
# (lit and shade courses); two moss feathers (Plume: the dominant, a crest's 6 rows at T2) tucked into the cap at the right
# temple and raked up and out (facing R / L: back over the nape). The kettle's own T2 is its base view (origin 2 1)
NASAL = os.path.join(MAPS, 'headwear', 'helm', 'nasal.pxm')
CAP2 = {   # over the nasal's T2 leather cap (its `!helm.t3,helm.t2 replace` views: origin 2 3 facing D / U, 3 3 facing R)
    'D': {0: '..................3.', 1: '.................32.', 2: '........AFCB....32..', 3: '.........FB....32...',
          4: '.........CB...32....', 5: '.........FB..21.....', 6: '.............1......'},
    'R': {0: '.3...................', 1: '.23..................', 2: '..23....AFCB.........', 3: '...23....FB..........',
          4: '....32...............', 5: '.....12..............', 6: '......1..............'},
}


def marsh_cap():
    views = []
    for f in FACINGS:
        t = CAP2['D' if f in 'DU' else 'R']
        if f == 'L':
            t = {y: mirror([s])[0] for y, s in t.items()}
        org = (2, 3) if f in 'DUL' else (3, 3)
        views.append((f, 'rest', grid(20 if f in 'DU' else 21, 7, t), None, org))
    return PAIR_LEGEND + SOFT_LEGEND, views


def marsh_tassels():
    """the kettle's tasselled reed hat (its `tassels` views, cell for cell: the pool stores the planes once) for the Marsh T3"""
    views = []
    regs = read_regions(KETTLE)
    for (f, k, w), (rows, org) in sorted(read_views(KETTLE).items()):
        if w == 'tassels replace':
            views.append((f, k, rows, regs.get((f, k, w)), org))
    return [], views


# T4: the wide war hat (warhat T4) drooping at the brim's ends, a moss wrap round its crown (Plume), and a reed tuft rising from
# the crown: bulrushes, moss stems (Plume) with brown heads (the leather), leaning with the wind
HAT4_D = {0: '.......h....h.......', 1: '.......g..h.g.......', 2: '........3.g.........', 3: '.........32.........',
          7: '...33332222211......', 8: '..333332222221111...', 11: 'vv................vv', 12: '.v................v.'}
HAT4_R = {0: '........h....h.......', 1: '........g..h.g.......', 2: '.........3.g.........', 3: '..........32.........',
          7: '....33332222211......', 8: '...333332222221111...', 11: '.vv................vv', 12: '..v................v.'}


def marsh_hat4():
    views = []
    for f in FACINGS:
        t = HAT4_D if f in 'DU' else HAT4_R if f == 'R' else {y: mirror([s])[0] for y, s in HAT4_R.items()}
        views.append((f, 'rest', grid(20 if f in 'DU' else 21, 13, t), None, None))
    return SOFT_LEGEND, views


# ------------------------------------------------------------------------------------------------ the pxm plumbing
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
    """[(map path, tag, lines)]; a part's legend lines once (the first block that names them)"""
    out = []
    legend, views = dune_turban()
    lines = block('dune-turban', 'the Dune T1-T2 turban: wound two rows lower, its teal brow wraps and its tail (STYLE_P5B 4-5, area B)',
                  legend, views, 'sighelm-dune', False)
    legend, views = dune_spike()
    lines += block('dune-spike', 'the Dune T2 turban over the spiked cap: the steel crown over the turban', legend, views,
                   'sighelm-dune,helm.t2', False)
    out.append((TURBAN, 'dune', lines))
    legend, views = dune_kulah()
    out.append((VEIL, 'dune-kulah', block('dune-kulah', 'the Dune T3-T4 kulah: the teal turban band under the needle spike', legend,
                                          views, 'sighelm-dune,helm.t3,!helm.t4', False)))
    legend, views = steppe_felt(1)
    lines = block('steppe-felt1', 'the Steppe T1 felt hat: its crown two rows lower, its fur flaps to the jaw', legend, views,
                  'sighelm-steppe,!helm.t2', False)
    legend, views = steppe_felt(2)
    lines += block('steppe-felt2', 'the Steppe T2 peaked felt cap: the crown to a peak, a rust knot', [], views,
                   'sighelm-steppe,helm.t2', False)
    out.append((FURHAT, 'steppe', lines))
    legend, views = steppe_lamellar()
    out.append((PLUMED, 'steppe-lamellar', block('steppe-lamellar', 'the Steppe T3-T4 pointed helm: its lamellar cheek and neck guards',
                                                 legend, views, 'sighelm-steppe,helm.t3,!helm.t4', False)))
    legend, views = marsh_cap1()
    out.append((HEADSCARF, 'marsh-bogcap', block('marsh-bogcap', 'the Marsh T1 bog cap: the hide kerchief fringed in moss', legend,
                                                 views, 'sighelm-marsh', False)))
    legend, views = marsh_cap()
    out.append((NASAL, 'marsh-cap', block('marsh-cap', 'the Marsh T2 rawhide cap (the nasal T2 leather cap): the bone crest, two moss feathers',
                                          legend, views, 'sighelm-marsh,helm.t2,!helm.t3', False)))
    legend, views = marsh_tassels()
    lines = block('marsh-tassels', 'the Marsh T3 tasselled reed war hat (the kettle tassels views)', legend, views,
                   'sighelm-marsh,helm.t3,!helm.t4', True)
    out.append((KETTLE, 'marsh-kettle', lines))
    legend, views = marsh_hat4()
    out.append((WARHAT, 'marsh-hat4', block('marsh-hat4', 'the Marsh T4 wide reed war hat: drooping, wrapped, a bulrush tuft',
                                            legend, views, 'sighelm-marsh,helm.t4,!helm.t5', False)))
    return out


def strip_blocks(lines, prefix):
    out, skip = [], False
    for s in lines:
        if s.startswith(f'{prefix}') and ' begin ' in s and (prefix != f'# {TAG}' or s.startswith(f'# {TAG} begin ')):
            skip = True
            continue
        if skip and s.startswith(f'{prefix}') and ' end ' in s:
            skip = False
            continue
        if not skip:
            out.append(s)
    return out


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


def all_strip(lines):
    out, skip = [], False
    for s in lines:
        if s.startswith(ANY_TAG) and ' begin ' in s:
            skip = True
            continue
        if s.startswith(ANY_TAG) and ' end ' in s:
            skip = False
            continue
        if not skip:
            out.append(s)
    return out


def write():
    by_file = {}
    skip = set(os.environ.get('P5B_B_SKIP', '').split(','))   # (the look-and-adjust loop: blocks left out, by tag, to cost them)
    for path, tag, lines in plan():
        if tag in skip:
            continue
        by_file.setdefault(path, []).extend(lines)
    for path, add in by_file.items():
        src = own_strip(open(path, encoding='utf-8').read().split('\n'))
        i = next(n for n, s in enumerate(src) if s.startswith('view ') or (s.startswith(ANY_TAG) and ' begin ' in s))
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
        mine = sum(1 for s in now if s.startswith(f'# {TAG} begin '))
        if all_strip(now) != old:
            print(f'FAIL {rel}: outside the P5b blocks the map differs from {BASE[:8]}')
            bad += 1
        elif not mine:
            print(f'FAIL {rel}: none of this script\'s {TAG} blocks is written (run it)')
            bad += 1
        else:
            print(f'ok   {rel}: {len(now) - len(old)} lines added, all inside P5b blocks ({mine} of {TAG}); every drawn view as it was')
    return bad


if __name__ == '__main__':
    if '--verify' in sys.argv:
        sys.exit(1 if verify() else 0)
    if '--print' in sys.argv:
        for path, tag, lines in plan():
            print('\n'.join(lines))
        sys.exit(0)
    write()
