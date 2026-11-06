#!/usr/bin/env python3
"""tools/doll/p5b_sig_C2.py (P5b production 2, area C; docs/STYLE_P5B.md 4-5, the owner's rulings of 2026-11-06): River, Sun
Temple, Sylvan and Starspire at T5-T6, the elite tiers, the slice's way: every block is new `when sighelm-<people>,...` views
(the resolver's per-slot people flag) and the local legend characters they use, inserted before a map's first view and before
the T1-T4 blocks of tools/doll/p5b_sig_C.py (a `replace` view wins in file order). No drawn view's cells change. Re-running
replaces this script's own blocks (between its begin and end comments, tag `(P5b SIG C2)`) only.

  python tools/doll/p5b_sig_C2.py            write the blocks into rpg/art/doll/maps
  python tools/doll/p5b_sig_C2.py --verify   outside this script's blocks every touched map is line for line its BASE version
  python tools/doll/p5b_sig_C2.py --print    print the blocks
  python tools/doll/p5b_sig_C2.py --cost     each block's pool bytes, estimated as pxc.mjs packs them

The heads (rows in rpg/core/fingerprint.h kSigRiver kSigSunTemple kSigSylvan kSigStarspire, T5-T6). River, Sylvan and Starspire
draw their whole head at T5-T6 (fp::SigTier::ownHead: the family's own tier overlays and legend views step aside): the head on
the family's part, the moving piece (the plume, the leaf crest, the star) on the family's `_legend` part, whose `breeze` views
stir it on the idle's breathe frame and the walk's passing frames (STYLE_P2 27.7, the living finish). The Sun Temple keeps its
jaguar (tools/doll/p5b_sig_C.py, T3 up) and grows the feather fan behind it on masked_legend; at T6 the eagle replaces it.
  River       T5 the guild captain's etched morion: a tall comb, the boat brim swept up at its points and edged in gilt, a white
              ostrich plume over a lapis one curling back from a gilt socket behind the comb; T6 the morion grown, its comb
              gilt-banded, a gem on the brow, and the grand plume: the linen plume grown, its fall to the shoulder, a lapis
              plume rising behind it (one side, as T5: never a halo or a pair of horns)
  Sun Temple  T5 the jaguar lord: the jaguar head under a half sun of pointed quetzal feathers rising from the crown,
              turquoise to their yellow tips; T6 the eagle lord: the turquoise eagle's head, its yellow hooked beak over the
              brow and its eyes, under the full fan, taller, its root a ring of the yellow
  Sylvan      T5 the open leaf helm: a silver bowl, its brow band gilt, a tall leaf crest veined in forest rising from the
              brow, two leaf wings a side swept up and back from the temples (inside the flare columns); T6 the crest a spray
              of three leaves, a third wing a side, a gem at the brow (the forest hood stays off: its drape over the collar hid
              the glass alloy's tell, C19 A7)
  Starspire   T5 the gothic helm over the indigo hood: an ogive bowl fluted in ribs, a star-white diamond star at its point,
              a brow band in the alloy's polished trim and its gem (the T5 body the row's bell, crystal shoulders and the
              ladder's own layer: a cape or tassets took a_star3's frames past B8's 48 colours); T6 the point a row taller under
              the star arch: seven stars from shoulder to shoulder (never spikes: celestial, symmetric, arched)

A new part pair too: armour/shared/pauldron_feather3_{l,r}.pxm (the medium feather mantle, 20 px across: the Sun Temple T3's
shoulders at their own size; the judge's envelope red `pauldron feather.3 -> .2`), written and verified as this script's own.

Authoring alphabet (written with characters each map leaves unused):
  Q R S T U   the Head slot 0-4 (a soft head's cloth: Sig::hood or the accent)
  0 1 2 3 4   Plume 0-4 (the crest's colour: Sig::crest or the dominant)
  5 6 7 8 9   Dye 0-4 (the piece's dominant)               A B C D E   Dye2 0-4 (the piece's accent)
  v w x y z   the helm's metal 0-4                          J N O P W   Metal2 0-4 (the fittings; gold from T5 on R cells)
  f g h i j   leather 0-4                                   K           ink
  k l m n o   glow 0-4 (the living finish's gem light)
  '.' keeps what is below, '-' erases it
Regions (the finish, contracts 3b): R trim (Metal2, gold from T5), P a lit panel (the T5 filigree), G a gem (Glow from T5),
N a seam (runes at T6)
"""
import os
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
MAPS = os.path.join(ROOT, 'rpg', 'art', 'doll', 'maps')
BASE = 'a22bebc7d09b04b77e5bf34dc4a68c5b7de3450d'   # fix-p5b-prod (fix r2): the maps before this script
TAG = '(P5b SIG C2)'

ALPHA = {}
for _i, _c in enumerate('QRSTU'): ALPHA[_c] = ('head', _i)
for _i, _c in enumerate('01234'): ALPHA[_c] = ('plume', _i)
for _i, _c in enumerate('56789'): ALPHA[_c] = ('dye', _i)
for _i, _c in enumerate('ABCDE'): ALPHA[_c] = ('dye2', _i)
for _i, _c in enumerate('vwxyz'): ALPHA[_c] = ('metal', _i)
for _i, _c in enumerate('JNOPW'): ALPHA[_c] = ('metal2', _i)
for _i, _c in enumerate('fghij'): ALPHA[_c] = ('leather', _i)
for _i, _c in enumerate('klmno'): ALPHA[_c] = ('glow', _i)
ALPHA['K'] = ('ink', 0)
STEP = {c: s for c, (_, s) in ALPHA.items()}
SLOT = {c: sl for c, (sl, _) in ALPHA.items()}
CHAR = {(sl, s): c for c, (sl, s) in ALPHA.items()}

# ------------------------------------------------------------------------------------------------ head coordinates
# The canvas every view is authored on: column 0 is head x -6, row 0 head y -10 (the head anchor is the head box's top-left,
# 16 columns wide). On the man: the crown at y 1 (the envelope counts rows over it: T5 8, T6 10), the hair from y 0, the brow
# at y 9, the eyes at y 10-11 (x 5 and 10), the mouth y 12, the chin y 13; the woman's anchor sits a row lower on her body,
# so her face takes the same rows. The columns past the head box: 2 (a shell), 3 (a flare part: morion, winged); the crest's
# colour (Plume) is an ornament, counted by rows only.
CX0, CY0, CW, CH = -6, -10, 28, 28


def canvas(rows):
    """{(x, y): ch} of rows drawn on the canvas ('.' left out); every row CW wide"""
    out = {}
    for j, r in enumerate(rows):
        if len(r) != CW:
            raise SystemExit(f'canvas row {j} is {len(r)} wide (want {CW}): {r!r}')
        for i, ch in enumerate(r):
            if ch != '.':
                out[(CX0 + i, CY0 + j)] = ch
    return out


def regions(rows):
    return canvas(rows)


RELIGHT = {1: 3, 3: 1, 4: 2}   # a right view mirrored to the left: the light stays top-left


def mirror(cells, relight=True):
    out = {}
    for (x, y), ch in cells.items():
        if relight and ch in STEP and ch != 'K' and SLOT[ch] not in ('glow',):
            ch = CHAR[(SLOT[ch], RELIGHT.get(STEP[ch], STEP[ch]))]
        out[(15 - x, y)] = ch
    return out


def merged(*layers):
    out = {}
    for l in layers:
        out.update(l)
    return out


def view(f, k, cells, reg=None):
    """(facing, key, rows, regions rows, origin) cropped to the cells' box"""
    pts = list(cells) + list((reg or {}).keys())
    x0, x1 = min(x for x, _ in pts), max(x for x, _ in pts)
    y0, y1 = min(y for _, y in pts), max(y for _, y in pts)
    rows = [''.join(cells.get((x, y), '.') for x in range(x0, x1 + 1)) for y in range(y0, y1 + 1)]
    rg = [''.join((reg or {}).get((x, y), '.') for x in range(x0, x1 + 1)) for y in range(y0, y1 + 1)] if reg else None
    return (f, k, rows, rg, (-x0, -y0))


def facings(d, u, r, reg_d=None, reg_u=None, reg_r=None, l=None, reg_l=None, breeze=None):
    """the five rest views (D U R L and L lie) of a head; L the mirror of R unless given; breeze: (d, u) stirred views"""
    if l is None:
        l = mirror(r)
        reg_l = {(15 - x, y): c for (x, y), c in (reg_r or {}).items()}
    out = [view('D', 'rest', d, reg_d), view('U', 'rest', u, reg_u), view('R', 'rest', r, reg_r),
           view('L', 'rest', l, reg_l), view('L', 'lie', l, reg_l)]
    if breeze:
        out += [view('D', 'breeze', breeze[0], reg_d and {k: v for k, v in reg_d.items() if k in breeze[0]}),
                view('U', 'breeze', breeze[1], reg_u and {k: v for k, v in reg_u.items() if k in breeze[1]})]
    return out


def sway(cells, y0, y1, dx=1, dy=0):
    """the stirred view: the rows y0..y1 of the cells moved by (dx, dy), the rest as drawn (a plume's tip, a crest's point)"""
    out = {k: v for k, v in cells.items() if not (y0 <= k[1] <= y1)}
    for (x, y), c in cells.items():
        if y0 <= y <= y1:
            out[(x + dx, y + dy)] = c
    return out


def tips_dip(cells):
    """the stirred view of a fan: each feather's tip (a pixel higher than both its neighbours' tops) dips a row"""
    top = {}
    for (x, y) in cells:
        top[x] = min(top.get(x, 99), y)
    out = dict(cells)
    for x, y in top.items():
        if y < top.get(x - 1, 99) and y < top.get(x + 1, 99):
            del out[(x, y)]
    return out


def clip_cols(cells, lo=-3, hi=18):
    """the flare columns (3 past the 16 px head box): nothing past them"""
    return {k: v for k, v in cells.items() if lo <= k[0] <= hi}


# ================================================================================================ RIVER
# The guild captain (STYLE_P5B 5 River: "an etched morion with a plume and slashed velvet"). The morion of T3-T4 grown: its
# comb a tall crescent, the boat brim wider and swept up at its points (the flare columns), its edge gilt (R: the T5 gold), its
# bowl's lit face a panel for the T5 filigree (the etching). Column 6 of each row is head x 0.
RIVER5_D = canvas([
    '............................',  # -10
    '............................',  # -9
    '............................',  # -8
    '............................',  # -7
    '............................',  # -6
    '.............vv.............',  # -5
    '............vPOv............',  # -4
    '............vzwv............',  # -3
    '............vPOv............',  # -2
    '..........vvvyxvvv..........',  # -1
    '.........vyyyzxxxwv.........',  # 0
    '........vyyyyzxxxxwv........',  # 1
    '.......vzyyyyzxxxxxwv.......',  # 2
    '.......vyyyyyzxxxxxwv.......',  # 3
    '......vzyyyyyzxxxxxxwv......',  # 4
    '..vv..vyyyyyyzxxxxxxwv..vv..',  # 5
    '..vzvvvvvvvvvvvvvvvvvvvvwv..',  # 6
    '...vzyyyyyyyyyxxxxxxxxxwv...',  # 7
    '....vvzyvvvvvvvvvvvvxwvv....',  # 8
    '......vvv..........vvv......',  # 9
    '............................',  # 10
    '............................',  # 11
    '............................',  # 12
    '............................',  # 13
    '............................',  # 14
    '............................',  # 15
    '............................',  # 16
    '............................',  # 17
])
RIVER5_DR = regions([
    '............................',  # -10
    '............................',
    '............................',
    '............................',
    '............................',
    '............................',  # -5
    '............................',  # -4
    '............................',
    '............................',
    '............................',  # -1
    '..........PPP...............',  # 0
    '.........PPPP...............',  # 1
    '........PPPPP...............',  # 2
    '........PPPPP...............',  # 3
    '............................',  # 4
    '............................',  # 5
    '............................',  # 6
    '....RRRRRRRRRRRRRRRRRRRR....',  # 7
    '............................',  # 8
    '............................',  # 9
])
RIVER5_U = canvas([
    '............................',  # -10
    '............................',  # -9
    '............................',  # -8
    '............................',  # -7
    '............................',  # -6
    '.............vv.............',  # -5
    '............vPOv............',  # -4
    '............vywv............',  # -3
    '............vPOv............',  # -2
    '..........vvvxwvvv..........',  # -1
    '.........vyyyxxxwwv.........',  # 0
    '........vyyyyxxxxwwv........',  # 1
    '.......vyyyyyxxxxxwwv.......',  # 2
    '.......vyyyyyxxxxxwwv.......',  # 3
    '......vyyyyyyxxxxxxwwv......',  # 4
    '..vv..vyyyyyyxxxxxxwwv..vv..',  # 5
    '..vzvvvvvvvvvvvvvvvvvvvvwv..',  # 6
    '...vzyyyyyyyyyxxxxxxxxxwv...',  # 7
    '....vvzyyyyyyyxxxxxxxxwvv...',  # 8
    '......vvvvvvvvvvvvvvvvv.....',  # 9
    '............................',  # 10
    '............................',  # 11
    '............................',  # 12
    '............................',  # 13
    '............................',  # 14
    '............................',  # 15
    '............................',  # 16
    '............................',  # 17
])
RIVER5_R = canvas([
    '............................',  # -10
    '............................',  # -9
    '............................',  # -8
    '............................',  # -7
    '............................',  # -6
    '............vvvv............',  # -5
    '..........vvPPOOvv..........',  # -4
    '.........vzyyyyyxxv.........',  # -3
    '.........vyyyyyyxxv.........',  # -2
    '.......vvvPPPOOOOvvv........',  # -1
    '......vzyyyyyyyxxxxxwv......',  # 0
    '.....vzyyyyyyyyxxxxxxwv.....',  # 1
    '.....vyyyyyyyyyxxxxxxwv.....',  # 2
    '...v.vyyyyyyyyyxxxxxxxwv.v..',  # 3
    '...vzvyyyyyyyyyxxxxxxxwvwv..',  # 4
    '...vzyvyyyyyyyyxxxxxxwvxwv..',  # 5
    '....vzyvvvvvvvvvvvvvvvxwv...',  # 6
    '.....vzyyyyyyyyxxxxxxxwv....',  # 7
    '......vvvvvvvvvvvvvvvvv.....',  # 8
    '............................',  # 9
    '............................',  # 10
    '............................',  # 11
    '............................',  # 12
    '............................',  # 13
    '............................',  # 14
    '............................',  # 15
    '............................',  # 16
    '............................',  # 17
])
RIVER5_RR = regions([
    '............................',  # -10
    '............................',
    '............................',
    '............................',
    '............................',
    '............................',  # -5
    '............................',  # -4
    '............................',
    '............................',
    '............................',  # -1
    '.........PPP................',  # 0
    '........PPPP................',  # 1
    '........PPPP................',  # 2
    '............................',  # 3
    '............................',  # 4
    '............................',  # 5
    '............................',  # 6
    '......RRRRRRRRRRRRRRRR......',  # 7
    '............................',  # 8
])
# the plume on morion_legend: a linen ostrich plume (Dye2) over a lapis one (Dye), set in a gilt socket behind the comb, rising
# and curling over to the figure's left (the shield side), its fronds falling to the brim; breeze: it lifts a pixel
RIVER5_PD = canvas([
    '............................',  # -10
    '............................',  # -9
    '............................',  # -8
    '..............CDDDC.........',  # -7
    '.............CDEEEDCC.......',  # -6
    '.............CDDDDCCCC......',  # -5
    '..............8.CCCCCCC.....',  # -4
    '..............87...CCCCB....',  # -3
    '..............OP....7CCBB...',  # -2
    '....................76CBB...',  # -1
    '.....................76BB...',  # 0
    '......................76B...',  # 1
    '.......................6....',  # 2
    '............................',  # 3
    '............................',  # 4
    '............................',  # 5
    '............................',  # 6
    '............................',  # 7
    '............................',  # 8
    '............................',  # 9
    '............................',  # 10
    '............................',  # 11
    '............................',  # 12
    '............................',  # 13
    '............................',  # 14
    '............................',  # 15
    '............................',  # 16
    '............................',  # 17
])
RIVER5_PDB = canvas([
    '............................',  # -10
    '............................',  # -9
    '..............CDDDC.........',  # -8
    '.............CDEEEDCC.......',  # -7
    '.............CDDDDCCCC......',  # -6
    '..............8.CCCCCCC.....',  # -5
    '..............87...CCCCBB...',  # -4
    '..............OP....7CCBB...',  # -3
    '....................76CBB...',  # -2
    '.....................76B....',  # -1
    '......................76....',  # 0
    '............................',  # 1
    '............................',  # 2
    '............................',  # 3
    '............................',  # 4
    '............................',  # 5
    '............................',  # 6
    '............................',  # 7
    '............................',  # 8
    '............................',  # 9
    '............................',  # 10
    '............................',  # 11
    '............................',  # 12
    '............................',  # 13
    '............................',  # 14
    '............................',  # 15
    '............................',  # 16
    '............................',  # 17
])
RIVER5_PU = canvas([
    '............................',  # -10
    '............................',  # -9
    '............................',  # -8
    '.........CDDDC..............',  # -7
    '.......CCDEEEDC.............',  # -6
    '......CCCCDDDDC.............',  # -5
    '.....CCCCCCC.8..............',  # -4
    '....BCCCC...78..............',  # -3
    '...BBCC7....PO..............',  # -2
    '...BBC67....................',  # -1
    '...BB67.....................',  # 0
    '...B67......................',  # 1
    '....6.......................',  # 2
    '............................',  # 3
    '............................',  # 4
    '............................',  # 5
    '............................',  # 6
    '............................',  # 7
    '............................',  # 8
    '............................',  # 9
    '............................',  # 10
    '............................',  # 11
    '............................',  # 12
    '............................',  # 13
    '............................',  # 14
    '............................',  # 15
    '............................',  # 16
    '............................',  # 17
])
RIVER5_PUB = canvas([
    '............................',  # -10
    '............................',  # -9
    '.........CDDDC..............',  # -8
    '.......CCDEEEDC.............',  # -7
    '......CCCCDDDDC.............',  # -6
    '.....CCCCCCC.8..............',  # -5
    '...BBCCCC...78..............',  # -4
    '...BBCC7....PO..............',  # -3
    '...BBC67....................',  # -2
    '....B67.....................',  # -1
    '....67......................',  # 0
    '............................',  # 1
    '............................',  # 2
    '............................',  # 3
    '............................',  # 4
    '............................',  # 5
    '............................',  # 6
    '............................',  # 7
    '............................',  # 8
    '............................',  # 9
    '............................',  # 10
    '............................',  # 11
    '............................',  # 12
    '............................',  # 13
    '............................',  # 14
    '............................',  # 15
    '............................',  # 16
    '............................',  # 17
])
RIVER5_PR = canvas([
    '............................',  # -10
    '............................',  # -9
    '............................',  # -8
    '...........CDDC.............',  # -7
    '........CCDEEEDC............',  # -6
    '.......CCDDDDDC8............',  # -5
    '......CCCCC..87.............',  # -4
    '.....CCCB....PO.............',  # -3
    '....CCCB7...................',  # -2
    '....CCB76...................',  # -1
    '...CCB76....................',  # 0
    '...CB76.....................',  # 1
    '...B76......................',  # 2
    '....6.......................',  # 3
    '............................',  # 4
    '............................',  # 5
    '............................',  # 6
    '............................',  # 7
    '............................',  # 8
    '............................',  # 9
    '............................',  # 10
    '............................',  # 11
    '............................',  # 12
    '............................',  # 13
    '............................',  # 14
    '............................',  # 15
    '............................',  # 16
    '............................',  # 17
])


def river5():
    head = facings(RIVER5_D, RIVER5_U, RIVER5_R, RIVER5_DR, None, RIVER5_RR)
    plume = facings(RIVER5_PD, RIVER5_PU, RIVER5_PR, breeze=(sway(RIVER5_PD, -1, 3, 1), sway(RIVER5_PU, -1, 3, -1)))
    return head, plume


# T6: the morion grown: its comb a row taller and gilt along its crest (Metal 2), a gem on the bowl's brow over the brim (G:
# the living finish); the grand plume on morion_legend (RIVER6_P*)
def river6_head():
    def grow(cells, comb_x):
        out = {(x, y - (1 if y <= -2 and comb_x[0] <= x <= comb_x[1] else 0)): c for (x, y), c in cells.items()}
        for x in range(comb_x[0], comb_x[1] + 1):   # refill the row the comb left
            if (x, -2) not in out and (x, -3) in out:
                out[(x, -2)] = out[(x, -3)]
        return out
    d = grow(RIVER5_D, (5, 10))
    d.update({(7, 5): 'P', (8, 5): 'O', (7, 4): 'O', (8, 4): 'N'})   # the gem in its gilt setting
    u = grow(RIVER5_U, (5, 10))
    r = grow(RIVER5_R, (2, 13))
    reg_d = dict(RIVER5_DR)
    reg_d.update({(7, 5): 'G', (8, 5): 'G'})
    return facings(d, u, r, reg_d, None, RIVER5_RR)


RIVER6_PD = canvas([
    '............................',  # -10
    '............CDDDC...........',  # -9
    '..........CDEEEDDCC.........',  # -8
    '.........CDEDDDDCCCC........',  # -7
    '........8CDDCCCCCCCCC.......',  # -6
    '.......899CC.....CCCCB......',  # -5
    '.......897........CCCBB.....',  # -4
    '........OPO........CCBB7....',  # -3
    '....................CBB76...',  # -2
    '....................CBB76...',  # -1
    '.....................BB76...',  # 0
    '......................B6....',  # 1
])
RIVER6_PDB = canvas([
    '............................',  # -10
    '............CDDDC...........',  # -9
    '..........CDEEEDDCC.........',  # -8
    '.........CDEDDDDCCCCC.......',  # -7
    '........8CDDCCCCCCCCCB......',  # -6
    '.......899CC......CCCBB.....',  # -5
    '.......897.........CCBB7....',  # -4
    '........OPO.........CBB76...',  # -3
    '....................CBB76...',  # -2
    '.....................BB76...',  # -1
    '......................B6....',  # 0
    '............................',  # 1
])
RIVER6_PR = canvas([
    '............................',  # -10
    '.........CDDDC..............',  # -9
    '.......CCDEEEDC.............',  # -8
    '......CCCDDDDDCC8...........',  # -7
    '.....CCCCCCCCC899...........',  # -6
    '....CCCB.......798..........',  # -5
    '...CCCB7.......OPO..........',  # -4
    '...CCB76....................',  # -3
    '...CBB76....................',  # -2
    '...CB76.....................',  # -1
    '...BB76.....................',  # 0
    '...B76......................',  # 1
    '....6.......................',  # 2
])


def river6():
    plume = facings(RIVER6_PD, mirror(RIVER6_PD), RIVER6_PR, breeze=(sway(RIVER6_PD, -1, 3, 1), sway(mirror(RIVER6_PD), -1, 3, -1)))
    return river6_head(), plume


# ================================================================================================ SUN TEMPLE
# The sun lord (STYLE_P5B 5 Sun Temple: "a sun lord under the feather fan"; FLUX sun_temple_full_elite_*, helmets_1-2). T5: the
# jaguar head of T3-T4 (tools/doll/p5b_sig_C.py) under a headdress of quetzal feathers on masked_legend: a half sun of pointed
# feathers rising from the crown, turquoise (Dye) to their yellow tips (Plume), each split from the next by its quill's shade,
# the light from the top left; behind the head (the jaguar's own pixels left clear), so the cat stays the face of it. T6: the
# eagle lord: masked's T6 view replaced by the eagle's head (turquoise feathers, the yellow hooked beak over the brow, its eyes,
# the face in its open beak) under the full fan, taller (T6's 10 rows), its root a ring of the yellow.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import p5b_sig_C as C1   # noqa: E402  (the T3-T5 jaguar: the fan keeps clear of it)


def grid_cells(rows, ax0, ay0):
    return {(ax0 + i, ay0 + j): c for j, r in enumerate(rows) for i, c in enumerate(r) if c not in '. '}


def jaguar_cells():
    """the jaguar's cells by facing (D U R L), as p5b_sig_C.py places them"""
    d = grid_cells(C1.JAG_D, -2, -2)
    u = grid_cells(C1.JAG_U, -2, -2)
    r = grid_cells(C1.JAG_R, -2, -2)
    return {'D': d, 'U': u, 'R': r, 'L': {(15 - x, y): c for (x, y), c in r.items()}}


def fan(cx, cy, rx, ry, r0, ring, n, spread, side=0.0, serr=0.16, outer='01234', inner='56789'):
    """a sunburst of n feathers about (cx, cy) on an ellipse rx x ry (the fan spreads wider than tall): angles from -spread
    to +spread degrees off the vertical (side tilts it for a profile), from r0 (in ellipse units: 1 the rim) out; inside
    `ring` the inner colour (the turquoise, Dye), outside it the tips (the yellow, Plume); each feather pointed at its tip
    (serr: how far its edges fall short of the rim), lit on its upper-left edge, its quill's shade between it and the next"""
    import math
    out = {}
    step = 2 * spread / (n - 1)
    for y in range(int(cy - ry) - 2, int(cy + 2)):
        for x in range(int(cx - rx) - 2, int(cx + rx) + 3):
            dx, dy = (x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry
            r = math.hypot(dx, dy)
            a = math.degrees(math.atan2(dx, -dy)) - side
            if abs(a) > spread + step * 0.45:
                continue
            k = (a + spread) / step
            frac = k - math.floor(k + 0.5)
            rmax = 1.0 - serr * (2 * abs(frac)) ** 1.5
            if r < r0 or r > rmax:
                continue
            sep = abs(frac) > 0.40
            rim = r > rmax - 0.9 / max(rx, ry)
            right = a > spread * 0.45
            st = 3 if frac < -0.05 else 2
            if sep: st = 1
            if right and st > 1: st -= 1
            if r <= ring:
                out[(x, y)] = inner[st]
            else:
                if rim: st = max(0, st - 1) if not sep else 0
                out[(x, y)] = outer[st]
    return out


def clear_of(cells, mask):
    return {k: v for k, v in cells.items() if k not in mask}


def sun5():
    jag = jaguar_cells()
    D = lambda lift=0: fan(7.5, 1.0 - lift, 12.5, 8.0, 0.25, 0.62, 9, 84, serr=0.22)
    R = lambda lift=0: fan(5.0, 1.0 - lift, 10.5, 8.0, 0.25, 0.62, 8, 80, side=-22, serr=0.22)
    out_d, out_u, out_r = clear_of(D(), jag['D']), clear_of(D(), jag['U']), clear_of(R(), jag['R'])
    out_l = clear_of(mirror(R()), jag['L'])
    return facings(out_d, out_u, out_r, l=out_l, breeze=(tips_dip(out_d), tips_dip(out_u)))


# the eagle (T6, masked's T6 view replaced): turquoise feathers (Dye) in scallops, the crown's tuft, the eagle's eyes (a yellow
# ring, Plume, round an ink pupil) either side, its yellow hooked beak over the brow (the man's face in the open beak), the helm's
# metal band at the beak's root as the jaguar's, the feathered cheeks to the jaw and the lower mandible under the chin. Grids 20
# wide from head x -2 (the jaguar's frame), rows from head y -3
EAGLE_D = [
    '......55555555......',
    '....558899888755....',
    '...58899888888775...',
    '..5899866886688775..',
    '.589988888888887775.',
    '.58700488888840075..',
    '.5870K3788887K30775.',
    '.5878004024320047875',
    '.586877024432078675.',
    '.58787702332077875..',
    '.587vwxyy02yxxwwv75.',
    '.587.z....00...z.785',
    '.587..............75',
    '.5865............685',
    '.587..............75',
    '.5865............675',
    '..5875..........575.',
    '...5300000000000035.',
    '....0233332222110...',
    '.....00000000000....',
]
EAGLE_U = [
    '......55555555......',
    '....558899888755....',
    '...58899888888775...',
    '..5899866886688775..',
    '.589888888888887775.',
    '.588888888888877775.',
    '.588666886668866775.',
    '.588888888888877775.',
    '.588888888888777775.',
    '.587866688666876675.',
    '.587888888887777775.',
    '..5878888887777775..',
    '..5876668866687675..',
    '...58788888777775...',
    '....557877777755....',
    '......55555555......',
]
EAGLE_R = [
    '....5555555.........',
    '...588998885........',
    '..58899888885.......',
    '.5899866886875......',
    '.58998888888875.....',
    '.5888888888004 75...',
    '.58866688880K34000..',
    '.588888888874443210.',
    '.5878888877443321100',
    '.587666887vwxyy2100.',
    '.58788877.z...0100..',
    '.5878887..........0.',
    '.587668.............',
    '.58787..............',
    '.5866...............',
    '.5875...........000.',
    '.587............0230',
    '..55...........0000.',
]


def eagle():
    d = grid_cells([r.replace(' ', '8') for r in EAGLE_D], -2, -3)
    u = grid_cells(EAGLE_U, -2, -3)
    r = grid_cells([r.replace(' ', '8') for r in EAGLE_R], -2, -3)
    return facings(d, u, r)


def sun6():
    """the full fan behind the eagle: taller (T6's 10 rows), a third ring at its root (the yellow, Plume)"""
    eg = {'D': grid_cells([r.replace(' ', '8') for r in EAGLE_D], -2, -3), 'U': grid_cells(EAGLE_U, -2, -3),
          'R': grid_cells([r.replace(' ', '8') for r in EAGLE_R], -2, -3)}
    eg['L'] = {(15 - x, y): c for (x, y), c in eg['R'].items()}
    def full(cx, cy, rx, ry, n, spread, side=0.0, lift=0):
        outer = fan(cx, cy - lift, rx, ry, 0.30, 0.74, n, spread, side=side, serr=0.2)
        root = fan(cx, cy - lift, rx * 0.62, ry * 0.62, 0.30, 0.66, n, spread, side=side, serr=0.0, outer='01234', inner='01234')
        out = dict(outer)
        out.update(root)   # the root ring of the yellow over the fan's inner turquoise
        return out
    D = lambda lift=0: full(7.5, 1.0, 13.5, 10.0, 11, 88, lift=lift)
    R = full(5.0, 1.0, 11.5, 10.0, 9, 80, side=-22)
    d, u = clear_of(D(), eg['D']), clear_of(D(), eg['U'])
    return facings(d, u, clear_of(R, eg['R']), l=clear_of(mirror(R), eg['L']), breeze=(tips_dip(d), tips_dip(u)))


# ================================================================================================ SYLVAN
# The lord of the wood (STYLE_P5B 5 Sylvan: "a lord of the wood in gilt-veined silver and a long cloak"; the owner's T5-T6 call:
# a leaf-crested open helm, leaf plates, moss and silver; FLUX sylvan_helmets_1-2). T5: an open silver helm close on the head
# its brow band gilt (Metal 2: the gilt people's T5 fittings), two leaf wings a side swept up and back from the temples (Dye:
# the leaf green, its midrib the forest, Dye2), and on winged_legend the tall leaf crest rising from the brow and curving back
# over the crown (Plume: the leaf green; its midrib gilt), its point stirring. T6: the crest a spray of three leaves, a third
# wing a side, a gem at the brow (G: the living finish).
def leaf(x0, y0, length, width, angle, lit=True, rib='A', body='789', tip=None):
    """a pointed leaf from (x0, y0) along `angle` (degrees, 0 up, + clockwise), `length` px, `width` at its widest; its
    midrib in `rib`, its body in body's three steps (dark, base, light), the light on its upper-left edge"""
    import math
    out = {}
    ca, sa = math.cos(math.radians(angle)), math.sin(math.radians(angle))
    for y in range(int(y0 - length - 2), int(y0 + length + 2)):
        for x in range(int(x0 - length - 2), int(x0 + length + 2)):
            dx, dy = x + 0.5 - (x0 + 0.5), y + 0.5 - (y0 + 0.5)
            t = dx * sa - dy * ca            # along the leaf
            s = dx * ca + dy * sa            # across it (+ right of the axis)
            if t < -0.2 or t > length:
                continue
            half = width / 2 * math.sin(math.pi * min(1.0, max(0.0, (t + 0.4) / (length + 0.4)))) ** 0.8
            if abs(s) > half + 0.25:
                continue
            if abs(s) < 0.5 and 0.5 < t < length - 1.0:
                out[(x, y)] = rib
            elif abs(s) > half - 0.6:
                out[(x, y)] = body[0]
            else:
                up = (s < 0) == (math.cos(math.radians(angle)) > 0) if lit else False
                out[(x, y)] = body[2] if up else body[1]
    if tip:
        out[(int(round(x0 + sa * (length - 0.5))), int(round(y0 - ca * (length - 0.5))))] = tip
    return out


SYL5_D = canvas([
    '............................',  # -10
    '............................',  # -9
    '............................',  # -8
    '............................',  # -7
    '............................',  # -6
    '............................',  # -5
    '............................',  # -4
    '............................',  # -3
    '............................',  # -2
    '..........vvvvvvvv..........',  # -1  the silver bowl
    '........vvzyyyyxxxvv........',  # 0
    '.......vzyyyyyyxxxxwv.......',  # 1
    '......vzyyyyyyyxxxxxwv......',  # 2
    '......vyyyyyyyyxxxxxwv......',  # 3
    '.....vzyyyyyyyyxxxxxxwv.....',  # 4
    '.....vyyyyyyyyyxxxxxxwv.....',  # 5
    '.....vvPPPPPPOOOOOONNvv.....',  # 6  the brow band, gilt
    '......v............v........',  # 7
])
SYL5_DR = regions([
    '............................',  # -10
    '............................', '............................', '............................',
    '............................', '............................', '............................',
    '............................', '............................', '............................',  # -1
    '............................',  # 0
    '.........PPP................',  # 1
    '........PPPP................',  # 2
    '........PPP.................',  # 3
    '............................',  # 4
    '............................',  # 5
    '......RRRRRRRRRRRRRRRR......',  # 6
    '............................',  # 7
])
SYL5_U = canvas([
    '............................',  # -10
    '............................', '............................', '............................',
    '............................', '............................', '............................',
    '............................', '............................',
    '..........vvvvvvvv..........',  # -1
    '........vvyyyyyxxxvv........',  # 0
    '.......vyyyyyyyxxxxwv.......',  # 1
    '......vyyyyyyyyxxxxxwv......',  # 2
    '......vyyyyyyyyxxxxxwv......',  # 3
    '.....vyyyyyyyyyxxxxxxwv.....',  # 4
    '.....vyyyyyyyyyxxxxxxwv.....',  # 5
    '.....vvPPPPPPOOOOOONNvv.....',  # 6
    '......vvvvvvvvvvvvvvvv......',  # 7
])
SYL5_R = canvas([
    '............................',  # -10
    '............................', '............................', '............................',
    '............................', '............................', '............................',
    '............................', '............................',
    '.........vvvvvvvv...........',  # -1
    '.......vvzyyyyyxxvv.........',  # 0
    '......vzyyyyyyyxxxxv........',  # 1
    '.....vzyyyyyyyyxxxxwv.......',  # 2
    '.....vyyyyyyyyyxxxxxwv......',  # 3
    '....vzyyyyyyyyyxxxxxwv......',  # 4
    '....vyyyyyyyyyyxxxxxxwv.....',  # 5
    '....vvPPPPPPPOOOOOONNvv.....',  # 6
    '.....vv..............v......',  # 7
])
SYL5_RR = regions([
    '............................',  # -10
    '............................', '............................', '............................',
    '............................', '............................', '............................',
    '............................', '............................', '............................',  # -1
    '............................',  # 0
    '........PPP.................',  # 1
    '.......PPPP.................',  # 2
    '.......PPP..................',  # 3
    '............................',  # 4
    '............................',  # 5
    '.....RRRRRRRRRRRRRRRR.......',  # 6
    '............................',  # 7
])


def syl_wings(facing, big):
    """the leaf wings swept back from the temples (Dye: the leaf green, the forest midrib), inside the flare columns: facing D
    and U two a side rising outward, facing R two sweeping back past the nape; big (T6): a third, lower leaf, the others grown"""
    w = {}
    if facing in 'DU':
        if big:
            w.update(leaf(1, 6, 5, 2.8, -62, rib='A'))
            w.update(leaf(14, 6, 5, 2.8, 62, rib='A'))
        w.update(leaf(1, 4, 7, 3.8, -42, rib='A'))
        w.update(leaf(14, 4, 7, 3.8, 42, rib='A'))
        w.update(leaf(2, 2, 6, 3.2, -22, rib='A'))
        w.update(leaf(13, 2, 6, 3.2, 22, rib='A'))
    else:
        if big:
            w.update(leaf(2, 6, 5, 2.8, -80, rib='A'))
        w.update(leaf(3, 4, 7, 3.8, -58, rib='A'))
        w.update(leaf(4, 2, 6, 3.2, -32, rib='A'))
    return clip_cols(w)


def syl_crest(facing, big, lift=0):
    """the leaf crest from the brow (Plume: the leaf green; its midrib the gilt, Metal 2): facing D and U a tall blade, facing
    R the blade curving back over the crown; big (T6): two shorter leaves either side of it"""
    c = {}
    if facing in 'DU':
        if big:
            c.update(leaf(5, 0 - lift, 7, 4.4, -34, rib='O', body='123'))
            c.update(leaf(10, 0 - lift, 7, 4.4, 34, rib='O', body='123'))
            c.update(leaf(7.5, 0 - lift, 9, 5.6, 0, rib='O', body='123'))
        else:
            c.update(leaf(7.5, 0 - lift, 7, 5.8, 0, rib='O', body='123'))
    else:
        if big:
            c.update(leaf(7, 0 - lift, 7, 4.4, -64, rib='O', body='123'))
            c.update(leaf(11, -1 - lift, 6, 4.0, 6, rib='O', body='123'))
            c.update(leaf(9, -1 - lift, 8, 5.6, -30, rib='O', body='123'))
        else:
            c.update(leaf(9, 0 - lift, 7, 5.8, -30, rib='O', body='123'))
    return c


def sylvan(big):
    gem = {}
    if big:   # the gem at the brow, set in the band (G: the glow at T5-T6, breathing)
        gem = {(7, 6): 'P', (8, 6): 'O'}
    reg_gem = {(7, 6): 'G', (8, 6): 'G'} if big else {}
    d = merged(syl_wings('D', big), SYL5_D, gem)
    u = merged(syl_wings('U', big), SYL5_U)
    r = merged(syl_wings('R', big), SYL5_R)
    head = facings(d, u, r, merged(SYL5_DR, reg_gem), None, SYL5_RR)
    cd, cu = syl_crest('D', big), syl_crest('U', big)
    crest = facings(cd, cu, syl_crest('R', big), breeze=(sway(cd, -10, -6, 1), sway(cu, -10, -6, -1)))
    return head, crest


# ================================================================================================ STARSPIRE
# The celestial lord (STYLE_P5B 5 Starspire: "a celestial lord whose spires and visor slit glow"; the owner's T5-T6 call: gothic
# arched, fluted, celestial indigo, silver and white-gold, never spiky-evil; the German fluted harness of its From list). T5:
# the gothic helm over the indigo hood (SigTier::coif): a bowl rising to a pointed arch (the ogive), fluted in ribs (lit and
# shaded columns), a white-gold brow band (Metal 2: the alloy's polished trim) and its gem (G); on crown_legend the finial: a
# four-point star in star white (Plume) at the arch's point, twinkling (its breeze view the star turned). T6: the arch taller
# and the star aureole behind the head: an arc of star white with seven stars on it (Plume), the stars' hearts gems.
STAR5_D = canvas([
    '............................',  # -10
    '............................',  # -9
    '............................',  # -8
    '............................',  # -7
    '.............vv.............',  # -6
    '............vzwv............',  # -5  the arch's point
    '............vzxv............',  # -4
    '...........vzyxwv...........',  # -3
    '..........vzyyxxwv..........',  # -2
    '.........vzywyxwxwv.........',  # -1  the flutes begin
    '........vzywyyxwxxwv........',  # 0
    '.......vzywyyyxwxxwwv.......',  # 1
    '.......vywyyyyxxwxxwv.......',  # 2
    '......vzywyyyyxxwxxwwv......',  # 3
    '......vywyyyyyxxxwxxwv......',  # 4
    '.....vvPPPPPPPOOOOOOOOvv....',  # 5  the white-gold band
    '.....vzyv..........vxwv.....',  # 6  the cheek plates' rims
    '......vyv..........vxv......',  # 7
    '......vv............vv......',  # 8
])
STAR5_DR = regions([
    '............................',  # -10
    '............................', '............................', '............................',
    '............................', '............................', '............................',
    '............................', '............................', '............................',  # -1
    '............................',  # 0
    '............................',  # 1
    '............................',  # 2
    '............................',  # 3
    '............................',  # 4
    '.............GG.............',  # 5  the gem (no trim run: the thread's band, a_star3's colours, B8)
    '............................',  # 6
])
STAR5_U = canvas([
    '............................',  # -10
    '............................', '............................', '............................',
    '.............vv.............',  # -6
    '............vywv............',  # -5
    '............vyxv............',  # -4
    '...........vyyxwv...........',  # -3
    '..........vyyyxxwv..........',  # -2
    '.........vyywyxwxwv.........',  # -1
    '........vyywyyxwxxwv........',  # 0
    '.......vyywyyyxwxxwwv.......',  # 1
    '.......vywyyyyxxwxxwv.......',  # 2
    '......vyywyyyyxxwxxwwv......',  # 3
    '......vywyyyyyxxxwxxwv......',  # 4
    '.....vvPPPPPPPOOOOOOOOvv....',  # 5
    '......vyyyyyyyxxxxxxwwv.....',  # 6  the neck guard
    '.......vvvvvvvvvvvvvvv......',  # 7
])
STAR5_R = canvas([
    '............................',  # -10
    '............................', '............................', '............................',
    '.............vv.............',  # -6
    '............vzwv............',  # -5
    '...........vzyxv............',  # -4
    '..........vzyyxwv...........',  # -3
    '.........vzywyxxwv..........',  # -2
    '........vzywyyxwxwv.........',  # -1
    '.......vzywyyyxxwxwv........',  # 0
    '......vzywyyyyxxwxxwv.......',  # 1
    '.....vzywyyyyyxxwxxwv.......',  # 2
    '....vzywyyyyyyxxxwxxwv......',  # 3
    '....vywyyyyyyyxxxwxxwv......',  # 4
    '...vvPPPPPPPPPOOOOOOOvv.....',  # 5
    '...vyyyyyyv.........vxv.....',  # 6  the neck guard; the cheek plate
    '....vvvvvv..........vv......',  # 7
])
STAR5_RR = regions([
    '............................',  # -10
    '............................', '............................', '............................',
    '............................', '............................', '............................',
    '............................', '............................', '............................',  # -1
    '............................', '............................', '............................',
    '............................', '............................',  # 4
    '............................',  # 5
    '............................',  # 6
])


def star(cx, cy, arm, turned=False):
    """a star in star white (Plume), lit to the top left: arm 1 a twinkle (its four points; turned: on the diagonals), arm 2 a
    diamond (a filled rhombus, its heart the highlight: never a cross; turned: the twinkle's long points through it)"""
    if arm == 1:
        pts = ((1, 1), (-1, -1), (1, -1), (-1, 1)) if turned else ((1, 0), (-1, 0), (0, 1), (0, -1))
        out = {(cx, cy): '4'}
        for dx, dy in pts:
            out[(cx + dx, cy + dy)] = '3' if dx < 0 or dy < 0 else '2'
        return out
    out = {}
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            d = abs(dx) + abs(dy)
            if d > 2:
                continue
            if d == 0:
                out[(cx + dx, cy + dy)] = '4'
            elif d == 1:
                out[(cx + dx, cy + dy)] = '3' if dx + dy < 0 else '2'
            else:
                out[(cx + dx, cy + dy)] = '2'
    if turned:
        for dx, dy in ((-2, -2), (2, -2), (-2, 2), (2, 2)):
            out[(cx + dx, cy + dy)] = '3' if dx < 0 and dy < 0 else '2'
    return out


def aureole(cx, cy, rx, ry, n, turned=False):
    """the star aureole: n stars on an elliptic arch from one shoulder over the head to the other (Plume: star white; the
    middle one the finial's place), a point of light midway between each two; behind the head"""
    import math
    out = {}
    for k in range(2 * n - 1):
        a = math.radians(-90 + 180 * k / (2 * n - 2))
        x, y = int(round(cx + rx * math.sin(a) - 0.5)), int(round(cy - ry * math.cos(a) - 0.5))
        if k % 2:
            out[(x, y)] = '2'
        elif k != n - 1:
            out.update(star(x, y, 1, turned=turned and (k // 2) % 2 == 0))
    return out


def starspire(big):
    tall = {}
    if big:   # T6: the arch's point a row taller
        tall = {(7, -7): 'v', (8, -7): 'v', (7, -6): 'z', (8, -6): 'w'}
    head = facings(merged(STAR5_D, tall), merged(STAR5_U, tall), merged(STAR5_R, tall), STAR5_DR, None, STAR5_RR)
    if big:   # the finial star on the taller point (T6: 10 rows), the aureole behind
        d = merged(aureole(8.0, 2.0, 11.5, 11.0, 7), star(7, -7, 2))
        db = merged(aureole(8.0, 2.0, 11.5, 11.0, 7, turned=True), star(7, -7, 2, turned=True))
        r = merged(aureole(5.0, 2.0, 9.0, 11.0, 5), star(7, -7, 2))
    else:     # T5: the finial star on the arch's point (8 rows)
        d, db, r = star(7, -5, 2), star(7, -5, 2, turned=True), star(7, -5, 2)
    fin = facings(d, d, r, breeze=(db, db))
    return head, fin


# ------------------------------------------------------------------------------------------------ the plan
def plan():
    """[(map path, tag, desc, views, cond, replace)] in the order they are written (each map's blocks in this order)"""
    out = []
    mor = os.path.join(MAPS, 'headwear', 'helm', 'morion.pxm')
    morl = os.path.join(MAPS, 'headwear', 'helm', 'morion_legend.pxm')
    h5, p5 = river5()
    out.append((mor, 'river-t5', 'the River T5 guild captain\'s etched morion: the tall comb, the boat brim swept up and gilt-edged',
                h5, 'sighelm-river,helm.t5,!helm.t6', True))
    out.append((morl, 'river-t5-plume', 'the River T5 plume: linen over lapis, curling back from a gilt socket behind the comb (stirs)',
                p5, 'sighelm-river,helm.t5,!helm.t6', True))
    h6, p6 = river6()
    out.append((mor, 'river-t6', 'the River T6 morion grown: the comb a row taller, gilt, a gem on the brow over the brim',
                h6, 'sighelm-river,helm.t6', True))
    out.append((morl, 'river-t6-plume', 'the River T6 grand plume: the linen plume grown over a lapis one rising behind it (stirs)',
                p6, 'sighelm-river,helm.t6', True))
    win = os.path.join(MAPS, 'headwear', 'helm', 'winged.pxm')
    winl = os.path.join(MAPS, 'headwear', 'helm', 'winged_legend.pxm')
    for t, big, cond in ((5, False, 'sighelm-sylvan,helm.t5,!helm.t6'), (6, True, 'sighelm-sylvan,helm.t6')):
        h, c = sylvan(big)
        out.append((win, f'sylvan-t{t}', f'the Sylvan T{t} open leaf helm: the silver bowl, its gilt brow band, the leaf wings swept back'
                    + (', a gem at the brow' if big else ''), h, cond, True))
        out.append((winl, f'sylvan-t{t}-crest', f'the Sylvan T{t} leaf crest' + (': a spray of three leaves' if big else ': a tall leaf')
                    + ' from the brow, its midrib gilt (stirs)', c, cond, True))
    crn = os.path.join(MAPS, 'headwear', 'helm', 'crown.pxm')
    crnl = os.path.join(MAPS, 'headwear', 'helm', 'crown_legend.pxm')
    for t, big, cond in ((5, False, 'sighelm-starspire,helm.t5,!helm.t6'), (6, True, 'sighelm-starspire,helm.t6')):
        h, f = starspire(big)
        out.append((crn, f'starspire-t{t}', f'the Starspire T{t} gothic helm: the fluted ogive, the white-gold band and its gem'
                    + (', the point a row taller' if big else ''), h, cond, True))
        out.append((crnl, f'starspire-t{t}-star', 'the Starspire T6 finial star and the star aureole behind the head (twinkles)' if big
                    else 'the Starspire T5 finial star on the point of the arch (twinkles)', f, cond, True))
    mskl = os.path.join(MAPS, 'headwear', 'helm', 'masked_legend.pxm')
    out.append((mskl, 'suntemple-t5-fan', 'the Sun Temple T5 sunburst of quetzal feathers behind the jaguar (stirs)',
                sun5(), 'sighelm-suntemple,helm.t5,!helm.t6', True))
    out.append((mskl, 'suntemple-t6-fan', 'the Sun Temple T6 full fan behind the eagle: taller, a ring of the yellow at its root (stirs)',
                sun6(), 'sighelm-suntemple,helm.t6', True))
    msk = os.path.join(MAPS, 'headwear', 'helm', 'masked.pxm')
    out.append((msk, 'suntemple-t6-eagle', 'the Sun Temple T6 eagle\'s head: turquoise feathers, its eyes, the yellow hooked beak over the brow',
                eagle(), 'sighelm-suntemple,helm.t6', True))
    return out


# ------------------------------------------------------------------------------------------------ the feather.3 pauldron
# (the judge's envelope red: `pauldron feather.3 -> .2`, the Sun Temple T3 mantle fell back to the small size) a NEW part pair,
# armour/shared/pauldron_feather3_{l,r}.pxm: the large feather mantle's T4-fitted drawing (pauldron_feather4, ARMOUR-X fitcap's
# `armour.t4` views, 3 px past the shoulder line) taken in to T3's 2 px facing D (20 px across with its pair), its new outer
# edge outlined; the other facings the T4-fitted views. No drawn view changes: the T4 part keeps every cell
F3_SRC = os.path.join(MAPS, 'armour', 'shared', 'pauldron_feather4_{}.pxm')
F3_DST = os.path.join(MAPS, 'armour', 'shared', 'pauldron_feather3_{}.pxm')


def _pxm_views(path):
    """[(header line, rows, regions)] of a map's views, in file order"""
    out, cur, mode = [], None, None
    for s in open(path, encoding='utf-8').read().split('\n'):
        if s.startswith('view '):
            cur = [s, [], []]
            out.append(cur)
            mode = 'rows'
        elif s == 'regions' and cur is not None:
            mode = 'reg'
        elif s.startswith('|') and cur is not None:
            (cur[1] if mode == 'rows' else cur[2]).append(s[1:])
        elif cur is not None and not s.startswith('|'):
            mode = None if s.strip() == '' or s.startswith('#') else mode
    return out


def _d_views(text):
    out, cur = {}, None
    for s in text.split('\n'):
        if s.startswith('view '):
            cur = None
            if s.split()[1] == 'D':
                cur = out[s.split(' origin')[0]] = [s, [], []]
                mode = 0
        elif s == 'regions' and cur is not None:
            mode = 1
        elif s.startswith('|') and cur is not None:
            cur[1 + mode].append(s[1:])
    return out


def feather3(side):
    src = F3_SRC.format(side)
    r_d = _d_views(feather3('r')) if side == 'l' else {}
    views = _pxm_views(src)
    fit = {}
    for h, rows, reg in views:
        if ' when armour.t4,!armour.t5 replace' in h:
            fit[h.split(' origin')[0]] = (h.replace(' when armour.t4,!armour.t5 replace', ''), rows, reg)
    lines = [f'# armour/shared/pauldron_feather3_{side}: the medium FEATHER pauldron on the {"right" if side == "r" else "left"} shoulder (feather.3;',
             f'# P5b production 2, area C: tools/doll/p5b_sig_C2.py, NEW). The large feather mantle (pauldron_feather4_{side}) at its T4',
             '# fitted reach taken in to T3\'s: 2 px past the shoulder line facing D (20 px across with its pair, STYLE_P2 29.3), its new',
             '# outer edge outlined. Rows of pointed feather tips in the people\'s colour; rides the lean and the bob and lifts a pixel on',
             '# wind-ups (`up`). Finish regions: P the clumps\' lit faces. Shared by the man and the woman (D10). Generated: re-run the',
             '# script, never hand-edit. Format: rpg/art/doll/doll.h.']
    for s in open(src, encoding='utf-8').read().split('\n'):
        if s.startswith('view '):
            break
        if s.startswith('#'):
            continue
        lines.append(s.replace('feather.4', 'feather.3'))
    while lines and lines[-1] == '':
        lines.pop()
    lines.append('')
    for h, rows, reg in views:
        if ' when ' in h:
            continue
        key = h.split(' origin')[0]
        if key in fit:
            h, rows, reg = fit[key]
        rows, reg = list(rows), list(reg)
        p = h.split()
        ox = int(p[p.index('origin') + 1])
        if p[1] == 'D' and side == 'l':   # the left part facing D: the right part's taken-in view mirrored, re-lit (top-left)
            rh, rrows, rreg = r_d[h.split(' origin')[0]]
            rp = rh.split()
            rox, w = int(rp[rp.index('origin') + 1]), len(rrows[0])
            relit = {'6': '8', '8': '6', '9': '7'}
            rows = [''.join(relit.get(c, c) for c in r[::-1]) for r in rrows]
            reg = [r[::-1] for r in rreg] if rreg else rreg
            p[p.index('origin') + 1] = str(w - 1 - rox)
            h = ' '.join(p)
        elif p[1] == 'D':
            keep = (lambda x: x >= ox - 3) if side == 'r' else (lambda x: x <= ox + 3)
            rows = [''.join(c if keep(x) else '.' for x, c in enumerate(r)) for r in rows]
            reg = [''.join(c if keep(x) else '.' for x, c in enumerate(r)) for r in reg]
            edge = ox - 3 if side == 'r' else ox + 3
            rows = [r[:edge] + ('5' if r[edge] != '.' else '.') + r[edge + 1:] for r in rows]
            reg = [r[:edge] + '.' + r[edge + 1:] for r in reg] if reg else reg
            # the columns cleared off the grid's own edge are dropped (the origin moves with them)
            lo = min((x for r in rows for x, c in enumerate(r) if c != '.'), default=0)
            hi = max((x for r in rows for x, c in enumerate(r) if c != '.'), default=0)
            rows = [r[lo:hi + 1] for r in rows]
            reg = [r[lo:hi + 1] for r in reg] if reg else reg
            p[p.index('origin') + 1] = str(ox - lo)
            h = ' '.join(p)
        lines.append(h)
        lines += ['|' + r for r in rows]
        if reg and any(c != '.' for r in reg for c in r):
            lines.append('regions')
            lines += ['|' + r for r in reg]
        lines.append('')
    return '\n'.join(lines)


def write_new():
    for side in 'lr':
        dst = F3_DST.format(side)
        open(dst, 'w', encoding='utf-8', newline='\n').write(feather3(side))
        print(f'{os.path.relpath(dst, ROOT)}: new part')


def verify_new():
    bad = 0
    for side in 'lr':
        dst = F3_DST.format(side)
        rel = os.path.relpath(dst, ROOT).replace(os.sep, '/')
        ok = os.path.exists(dst) and open(dst, encoding='utf-8').read() == feather3(side)
        print(('ok   ' if ok else 'FAIL ') + f'{rel}: a new part, this script\'s own output')
        bad += 0 if ok else 1
    return bad


# ------------------------------------------------------------------------------------------------ the pxm plumbing
FREE = [chr(c) for c in range(0xC0, 0x180) if chr(c) not in '×÷']   # Latin-1 and Latin Extended-A letters: never a map's


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


def map_blocks():
    """{path: lines}: per map one legend block (every (slot, step) this script's views there use, each its own character),
    then the view blocks in plan order"""
    by = {}
    for path, tag, desc, views, cond, replace in plan():
        by.setdefault(path, []).append((tag, desc, views, cond, replace))
    out = {}
    for path, blocks in by.items():
        used = map_chars(path)
        pool = [c for c in FREE if c not in used]
        need = sorted({c for _, _, views, _, _ in blocks for v in views for r in v[2] for c in r if c not in '.-'},
                      key=lambda c: (ALPHA[c][0], ALPHA[c][1]) if c in ALPHA else ('~', 0))
        assign = {}
        for c in need:
            if c not in ALPHA:
                raise SystemExit(f'{os.path.basename(path)}: {c!r} is not in the authoring alphabet')
            assign[c] = pool.pop(0)
        lines = [f'# {TAG} begin legend: the characters of this script\'s views in this map']
        lines += [f'legend {assign[c]} {ALPHA[c][0]} {ALPHA[c][1]}' for c in need]
        lines.append(f'# {TAG} end legend')
        for tag, desc, views, cond, replace in blocks:
            lines.append(f'# {TAG} begin {tag}: {desc}')
            for f, k, g, reg, org in views:
                lines.append(f'view {f} key {k} origin {org[0]} {org[1]} when {cond}{" replace" if replace else ""}')
                lines += ['|' + ''.join(assign.get(c, c) for c in r) for r in g]
                if reg and any(c != '.' for r in reg for c in r):
                    lines.append('regions')
                    lines += ['|' + r for r in reg]
            lines.append(f'# {TAG} end {tag}')
        out[path] = lines
    return out


def write():
    for path, add in map_blocks().items():
        src = strip_blocks(open(path, encoding='utf-8').read().split('\n'))
        # before the first view and before any other script's people blocks (the T1-T4 blocks: their replace views would win)
        i = next(n for n, s in enumerate(src) if s.startswith('view ') or s.startswith('# (P5b SIG'))
        new = src[:i] + add + src[i:]
        open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(new))
        print(f'{os.path.relpath(path, ROOT)}: +{len(add)} lines')


def verify():
    bad = 0
    for path in sorted({p for p, *_ in plan()}):
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
    tot = 0
    for path, tag, desc, views, cond, replace in plan():
        planes, b = set(), 0
        for f, k, g, reg, org in views:
            w = max(len(r) for r in g)
            key = tuple(r.ljust(w, '.') for r in g)
            b += 35
            if key not in planes:
                planes.add(key)
                b += rle_len([0 if c == '.' else 1 for r in key for c in r])
        b += 2 + len(cond)
        tot += b
        print(f'{tag:28s} {len(views)} views  ~{b} B')
    print(f'{"total":28s} ~{tot} B')


if __name__ == '__main__':
    if '--verify' in sys.argv:
        sys.exit(1 if verify() + verify_new() else 0)
    if '--cost' in sys.argv:
        cost()
        sys.exit(0)
    if '--print' in sys.argv:
        for path, lines in map_blocks().items():
            print(f'== {os.path.relpath(path, ROOT)}')
            print('\n'.join(lines))
        sys.exit(0)
    write()
    write_new()
