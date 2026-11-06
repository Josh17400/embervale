#!/usr/bin/env python3
"""tools/doll/p5b_sig_fr1.py (P5b production, fix round 1; docs/STYLE_P5B.md 4-5, 9.3): the heads the judge found twinned at
T1-T2, redrawn as ADDED views the slice's way (tools/doll/p5b_sig.py). Every block this script writes is new
`when sighelm-<people>,...` views and their local legend lines, tag `(P5b SIG FR1)`, inserted at the top of a part's views (a
`replace` view wins in file order). No view drawn before P5b changes. The production blocks this round supersedes (listed in
SUPERSEDED: not yet signed by the owner, the judge's twins) are taken out, so their bytes pay for the new ones.

  python tools/doll/p5b_sig_fr1.py            write the blocks into rpg/art/doll/maps
  python tools/doll/p5b_sig_fr1.py --verify   each touched map is HEAD's but for this script's blocks and the superseded ones
  python tools/doll/p5b_sig_fr1.py --print    print the blocks
  python tools/doll/p5b_sig_fr1.py --cost     each block's pool bytes (as pxc.mjs packs them)

Authoring is in head coordinates (x across the 16-column head box, 0 at its left; y down, 0 at the box's top: the capped
hair's top, the brow near 5, the eyes 7, the chin 12): each view's origin is placed so the anchor lands on (0, 0). Facing L is
facing R mirrored (x -> 15 - x) with its lit and shade steps swapped (light from the top left). The alphabet:
  Q R S T U   Head 0-4 (a soft head's cloth: Sig::hood or the accent)     0 1 2 3 4   Plume 0-4 (Sig::crest or the dominant)
  5 6 7 8 9   Dye 0-4 (a helm part's dominant)        A B C D E   Dye2 0-4 (a helm part's accent)
  v w x y z   the helm's metal 0-4                    J N O P W   Metal2 0-4 (the fittings)
  f g h i j   leather 0-4                             K           ink        q r s t u  skin 0-4
  '-' erases what is below (the base's own cells and the hair under them)
"""
import os
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
MAPS = os.path.join(ROOT, 'rpg', 'art', 'doll', 'maps')
TAG = '(P5b SIG FR1)'

ALPHA = {}
for _i, _c in enumerate('QRSTU'): ALPHA[_c] = ('head', _i)
for _i, _c in enumerate('01234'): ALPHA[_c] = ('plume', _i)
for _i, _c in enumerate('56789'): ALPHA[_c] = ('dye', _i)
for _i, _c in enumerate('ABCDE'): ALPHA[_c] = ('dye2', _i)
for _i, _c in enumerate('vwxyz'): ALPHA[_c] = ('metal', _i)
for _i, _c in enumerate('JNOPW'): ALPHA[_c] = ('metal2', _i)
for _i, _c in enumerate('fghij'): ALPHA[_c] = ('leather', _i)
for _i, _c in enumerate('qrstu'): ALPHA[_c] = ('skin', _i)
for _i, _c in enumerate('abcde'): ALPHA[_c] = ('linen', _i)
ALPHA['K'] = ('ink', 0)
# lit <-> shade when a drawing turns to face the other way (steps 1 and 3; the top light stays)
RELIT = {}
for _ramp in ('QRSTU', '01234', '56789', 'ABCDE', 'vwxyz', 'JNOPW', 'fghij', 'qrstu', 'abcde'):
    RELIT[_ramp[1]], RELIT[_ramp[3]] = _ramp[3], _ramp[1]
FREE = '!$%&*+,/:;<=>?[]^_`{}~()' + 'klmnopabcdeGHIJKLMNOPVWXYZ' + '0123456789'


def P(*parts):
    return os.path.join(MAPS, *parts)


# ------------------------------------------------------------------------------------------------ authoring
def cells(rows, y0, x0):
    """{(y, x): ch} of rows drawn with their top-left at head (x0, y0); '.' left out"""
    return {(y0 + dy, x0 + dx): c for dy, r in enumerate(rows) for dx, c in enumerate(r) if c != '.'}


def mirror(cs, relight=True):
    return {(y, 15 - x): (RELIT.get(c, c) if relight else c) for (y, x), c in cs.items()}


def view(f, cs, key='rest'):
    """a view from head-coordinate cells: its grid spans them, its origin puts the anchor on (0, 0)"""
    ys = [y for y, _ in cs]
    xs = [x for _, x in cs]
    y0, x0 = min(ys), min(xs)
    g = [['.'] * (max(xs) - x0 + 1) for _ in range(max(ys) - y0 + 1)]
    for (y, x), c in cs.items():
        g[y - y0][x - x0] = c
    return (f, key, [''.join(r) for r in g], (-x0, -y0))


def four(d, r, u=None):
    """the four facings from D and R cells (U: the back, default D's; L: R mirrored)"""
    return [view('D', d), view('U', u if u is not None else d), view('R', r), view('L', mirror(r))]


def draw(*layers):
    out = {}
    for rows, y0, x0 in layers:
        out.update(cells(rows, y0, x0))
    return out


# ------------------------------------------------------------------------------------------------ the heads
# ---- River T1: the chaperon (STYLE_P5B 4 River T1 "rolled roundel, hanging tail"). Over the headscarf (the guild's linen in
# Head, Sig::hood): the fat lapis roll (the bourrelet, Plume) bulges a pixel past the head each side; the cornette, a long
# lapis tail, hangs from the roll's left end to the jaw; both of the kerchief's falls are taken out (the face and the hair
# show under the roll: the chaperon is a hat, never a hood). Facing R the liripipe hangs down the back to the shoulder blades,
# narrow, in lapis. T1's two rows over the crown and its area hold (C12f).
def chaperon():
    roll_d = (['134433322222111.', '0233222221111100'], 4, 0)       # the roll across the brow (C's chaperon, kept)
    roll_l, roll_r = (['1', '0'], 4, -1), (['2', '1'], 4, 16)
    puff = (['TTUUT'], -1, 8)
    d = draw((['....', '----', '---.', '--..', '.-..'], 5, 0), (['....', '----', '.---', '..--', '..-.'], 5, 12), roll_d,
             roll_l, roll_r, (['21', '32', '21', '21', '10'], 6, -1))
    r = draw((['--', '--', '--', '--', '--', '--', '--', '-.'], 7, -2), (['---', '---', '--.', '-..'], 7, 0),
             (['1344433332222110', '02333222221111100'], 4, -1),
             (['12', '21', '21', '10', '21', '10', '21', '10', '0.'], 6, -3))
    u = draw((['0344333322221110', '0233322222111100'], 4, 0))
    return four(d, r, u)


# ---- Sun Temple T1: the headband (STYLE_P5B 4 "headband, one upright feather"). Over the circlet: the turquoise band (Head:
# Sig::hood) two rows deep standing a pixel off the hair each side, a yellow sun disc at the brow; the feather (Plume: the
# yellow) tall and upright at the left temple, its tip two rows over the hair, a red-orange second vane (the plume's dark
# steps; its tip at the hair's top: T1's ornament two rows), jade ear spools, the band's knot at the nape and its two tails
# (facing R and U), so the head reads in greyscale and as a black fill with or without the shield (the judge's R3a: a bare
# head)
def headband():
    band_d = (['QUTTTTTTTSSSSSSRRQ', 'QSSSSSSRRRRRRRRRRQ', '.QRRQQQQQQQQQQRRQ.'], 3, -1)
    feather = (['.3.', '343', '242', '131', '.1.', '.0.'], 0, -2)
    disc = (['3443', '.33.'], 4, 6)
    d = draw(band_d, disc, feather, (['RT', 'QS', '.3'], 8, -1), (['TR', 'SQ', '3.'], 8, 15))
    r = draw((['QUTTTTTTTSSSSSRRQ', 'QSSSSSRRRRRRRRRRQ', '.QRRQQQQQQQQQQRQ.'], 3, -1), (['Q', 'R', 'Q'], 4, -2), (['RT', 'QS', '.3'], 8, 5),
             (['QR', 'RQ', 'Q.', 'RQ', 'Q.', 'QR', '.Q'], 6, -2),                   # the knot's two tails down the nape
             (['.3.', '343', '242', '131', '.1.', '.0.'], 0, 13), (['34', '.3'], 4, 15))
    u = draw((['QTTTTTSSSSSSSRRQ', 'QSSSSRRRRRRRRRRQ'], 4, 0), (['RQ', 'QR'], 6, 6),
             (['Q.', 'RQ', 'Q.', 'QR', '.Q'], 8, 5), (['.Q', 'QR', '.Q', 'RQ', 'Q.'], 8, 9),
             (['.3.', '343', '242', '131', '.1.', '.0.'], 0, 15))
    return four(d, r, u)


# ---- Highland T1-T2: the bonnet's ribbon tails (the Scots bonnet's two black silk tails at the back of its band; Plume's dark
# steps, the tartan's own green) hanging to the nape, and its toorie a row fuller: the bonnet parts from the Jade jingasa and
# the Dune turban (the judge's J1, 0.21 and 0.25) facing R and from behind
def bonnet_ribbons():
    d = draw((['1..0'], 7, 0), (['--', '--', '--', '-.'], 3, 14))
    r = draw((['10', '01', '10'], 7, 0), (['.-', '--', '--', '-.'], 3, 13))
    u = draw((['10.01', '01.10', '10.0.', '0..1.', '...0.'], 7, 5), (['--', '--', '--', '.-'], 3, 0))
    return four(d, r, u)


# ---- Fjordfolk T2: the spangen cap (STYLE_P5B 4 Fjordfolk T2) grown from the T1 cap toward the T3 spangenhelm: its cone's
# crown steeper, a dark hide brow strap set with five iron rivet heads (Plume: the people's iron crest colour; the strap the
# leather's own, so the cap stays a brown, F4), and a quilted leather neck curtain behind, its edges to the jaw facing D (the
# T3 aventail's ancestor). Inside the head box and the figure (C12f: no T2 shell past the box, the T2 area under T3's)
def fjord_cap2():
    d = draw((['g3ggg3ggg3ggg3gg', 'ffgffffgffffgfff'], 4, 0), (['g', 'h', 'g', 'f'], 9, 0), (['h', 'g', 'f', 'f'], 9, 15),
             (['-........-'], 1, 3))
    r = draw((['g3ggg3ggg3ggg3gg', 'ffgffffgffffgfff'], 4, 0),
             (['ghhg', 'ghih', 'ghhg', 'gihg', 'ghhg', 'ghg.', 'fg..'], 6, 0))
    u = draw((['g3ggg3ggg3ggg3gg', 'ffgffffgffffgfff'], 4, 0),
             (['.ghhhhhhhhhhhhg.', 'ghihhihhihhihhhg', 'ghhhhhhhhhhhhhhg', 'fggggggggggggggf'], 6, 0))
    return four(d, r, u)


# ---- Imperial T2: the Montefortino (STYLE_P5B 4 Imperial T2) grown from the T1 knob cap: the hinged cheek guards over the
# cheeks (Metal2: the fittings' brass), a neck guard flaring behind; the crimson tuft is area A's overlay (kept)
def imperial_cap2():
    d = draw((['J', 'J'], 4, 0), (['J', 'J'], 4, 15), (['JP', 'PO', 'PO', 'PO', 'JN'], 6, 0), (['NJ', 'ON', 'ON', 'ON', 'NJ'], 6, 14))
    r = draw((['PJ', 'OO', 'ON', 'ON', 'NJ'], 6, 10), (['PPJ', 'J..'], 5, 0))
    u = draw((['JPPPPPPPPPPPPPPJ', '.JJJJJJJJJJJJJJ.'], 5, 0))
    return four(d, r, u)


# ---- Marsh T2: the rawhide cap (STYLE_P5B 4 Marsh T2 "bone crest, two feathers"): the bone crest (Dye2: the accent) rising
# two rows over the crown as a notched ridge, two moss feathers (Plume) two pixels wide raked up and out from the right
# temple, and the T1 cap's ragged moss fringe at the rim (over the brow, and strands to the jaw beside the face)
def marsh_cap2():
    crest_d = (['------------', '---......---', '-...BDCB...-', '-....CB....-'], -1, 2)
    fe_d = (['....3', '...3.', '..2..', '.2...', '1....'], -4, 12)
    fringe_d = (['2.1........1.2', '1.0........0.1'], 6, 1)
    d = draw(crest_d, fe_d, fringe_d)
    r = draw((['-------------', '----.....----', '..BCBDBCBDB..'], -1, 1), (['3....', '.3...', '..2..', '...2.', '....1'], -4, 0),
             (['2.1', '1.0'], 6, 0), (['2.1', '1.0'], 6, 12))
    u = draw(crest_d, (['3....', '.3...', '..2..', '...2.', '....1'], -4, 0),
             (['2.1..2.1..2.1..2', '1.0..1.0..1.0..1'], 6, 0))
    return four(d, r, u)


# ---- Jade T2: the great kasa (STYLE_P5B 4 Jade T2), drawn low to T2's three rows: a broad shallow cone of plaited straw (the
# T2 helm's leather) over the whole head box and three pixels past it each side, radial plait lines, the vermilion binding on
# its rim (Dye: the dominant), a lacquer knob at the apex and the vermilion chin cord down both cheeks. Twice the jingasa's
# breadth at its rim and flat where the jingasa peaks; the kabuto (T3) follows
def kasa():
    d = draw((['--------------------', '--------------------', '--------------------'], -3, -2),
             (['-----.....------', '-..............-'], 0, 0),          # the hair round the cone's crown taken in
             (['.......0.........'], -1, -1),
             (['.....jiihhg......'], 0, 0),
             (['..jjiihhhhgghg...'], 1, -1),
             (['.jjiijhhhhghhgg..'], 2, -2),
             (['34444333332222111100'], 3, -2), (['.ffggggggggggggff.'], 4, -1),
             (['0', '1', '1', '0'], 5, 1), (['1', '1', '0', '0'], 5, 14))
    d = {k: v for k, v in d.items()}
    r = draw((['--------------------', '--------------------', '--------------------'], -3, -2),
             (['------......------', '--..............--'], 0, -1),
             (['.......0..........'], -1, 0),
             (['......jiihhg......'], 0, 0),
             (['...jjiihhhhgghg...'], 1, -1),
             (['..jjiijhhhhghhgg..'], 2, -2),
             (['334444333332222111100'], 3, -3), (['.fggggggggggggggff.'], 4, -2),
             (['0', '1', '1', '0'], 5, 11))
    return four(d, r)


# ---- Marsh T1: the bog cap (the Tollund cap, STYLE_P5B 5 Marsh "From"): the hide kerchief (Head: the accent, bone), its
# falls drawn down to the jaw, a moss fringe (Plume) in two ragged rows over the brow, and facing R and U the knot's tails a
# ragged moss hank at the nape. T1's two rows over the crown and its slim area hold (C12f): no point, no strands outside.
def bogcap():
    d = draw((['2.3.2.1', '1.2.1.0'], 6, 4),
             (['.QT.', '..QT'], 9, 0), (['.SQ.', 'SQ..'], 9, 12))
    r = draw((['32121', '21.1.'], 6, 9), (['3.2', '2.1', 'T.T'], 11, -1),
             )
    u = draw((['2.1.2', '1.0.1', 'T.T.S'], 9, 6))
    return four(d, r, u)


# ---- Fjordfolk T3 (the slice's aventail, judge fix r1 item 1, C3b): the mail aventail hangs from the spangenhelm's rim, so long
# hair never shows over it. In each column where the aventail is the helm's only pixel (behind the dome facing R, L and U), the
# hair above the mail is taken in ('-': the hair's own cells; the dome and the aventail as drawn). Read from the maps: the
# spangen's base views and the slice's aventail overlay share the base's origin
def read_views(path):
    """{(facing, key, when): (rows, origin or None)}"""
    out, cur, mode = {}, None, None
    for line in open(path, encoding='utf-8'):
        s = line.rstrip('\n')
        if s.startswith('view '):
            p = s.split()
            w = s.split(' when ', 1)[1] if ' when ' in s else ''
            k = p[p.index('key') + 1] if 'key' in p else 'rest'
            org = (int(p[p.index('origin') + 1]), int(p[p.index('origin') + 2])) if 'origin' in p else None
            cur = (p[1], k, w)
            out[cur] = ([], org)
            mode = 'g'
        elif s.startswith('regions') or s.startswith('build') or s.startswith('mark') or s.startswith('#'):
            mode = 'x' if not s.startswith('mark') else mode
        elif s.startswith('|') and cur is not None and mode == 'g':
            out[cur][0].append(s[1:])
    return out


def aventail_hair():
    path = P('headwear', 'helm', 'spangen.pxm')
    src = head_text(path).replace('\r\n', '\n')
    tmp = os.path.join(os.environ.get('TEMP', '/tmp'), 'p5b_fr1_spangen.pxm')
    open(tmp, 'w', encoding='utf-8').write(src)
    vs = read_views(tmp)
    out = []
    for f in ('U', 'R', 'L'):
        base, org = vs[(f, 'rest', '')]
        av, _ = vs[(f, 'rest', 'sighelm-fjordfolk,helm.t3,!helm.t4')]
        ox, oy = org
        cs = {}
        w = max(max(len(r) for r in base), max(len(r) for r in av))
        for c in range(w):
            if any(c < len(r) and r[c] not in '.-' for r in base):
                continue   # the dome's own column: its rim is the top, the base's '-' rows clear the hair over it
            top = next((y for y, r in enumerate(av) if c < len(r) and r[c] not in '.-'), None)
            if top is None:
                continue
            for y in range(0, top):
                cs[(y - oy, c - ox)] = '-'
        if cs:
            out.append(view(f, cs))
    return out


# ------------------------------------------------------------------------------------------------ the plan
SUPERSEDED = [   # (map, the block's begin line prefix, its end line prefix)
    (P('headwear', 'cloth', 'headscarf.pxm'), '# (P5b SIG C) begin river-chaperon', '# (P5b SIG C) end river-chaperon'),
    (P('headwear', 'cloth', 'headscarf.pxm'), '# (P5b SIG B) begin marsh-bogcap', '# (P5b SIG B) end marsh-bogcap'),
    (P('headwear', 'cloth', 'circlet.pxm'), '# (P5b SIG C) begin suntemple-band', '# (P5b SIG C) end suntemple-band'),
    (P('headwear', 'helm', 'nasal.pxm'), '# (P5b SIG B) begin marsh-cap', '# (P5b SIG B) end marsh-cap'),
]


def plan():
    """[(map, tag, desc, views, cond, replace)]"""
    hsc = P('headwear', 'cloth', 'headscarf.pxm')
    nas = P('headwear', 'helm', 'nasal.pxm')
    return [
        (hsc, 'river-chaperon', 'the River T1 chaperon: the fat lapis roll, the cornette to the shoulder, the patte', chaperon(),
         'sighelm-river,!helm.t2', False),
        (hsc, 'marsh-bogcap', 'the Marsh T1 bog cap: drawn to a point, tied under the chin, fringed in moss', bogcap(),
         'sighelm-marsh', False),
        (P('headwear', 'cloth', 'circlet.pxm'), 'suntemple-band', 'the Sun Temple T1 headband: the band off the hair, the sun disc, '
         'the tall feather, the knot\'s tails', headband(), 'sighelm-suntemple,!helm.t2', False),
        (P('headwear', 'cloth', 'cap.pxm'), 'highland-ribbons', 'the Highland T1-T2 bonnet: its ribbon tails and a fuller toorie',
         bonnet_ribbons(), 'sighelm-highland,!helm.t3,!helm.t4', False),
        (P('headwear', 'helm', 'spangen.pxm'), 'fjordfolk-aventail-hair', 'the Fjordfolk T3 aventail: no hair over the mail '
         '(C3b; the slice\'s aventail as drawn)', aventail_hair(), 'sighelm-fjordfolk,!helm.t4,helm.t3', False),
        (P('headwear', 'helm', 'spangen.pxm'), 'fjordfolk-cap2', 'the Fjordfolk T2 spangen cap: the fleece roll, the nasal, the '
         'leather neck curtain', fjord_cap2(), 'sighelm-fjordfolk,helm.t2,!helm.t3', False),
        (nas, 'imperial-cap2', 'the Imperial T2 Montefortino: the hinged cheek guards, the neck guard', imperial_cap2(),
         'sighelm-imperial,!helm.t3,helm.t2', False),
        (nas, 'marsh-cap2', 'the Marsh T2 rawhide cap: the bone crest ridge, two moss feathers, the moss fringe', marsh_cap2(),
         'sighelm-marsh,helm.t2,!helm.t3', False),
        (P('headwear', 'cloth', 'conical.pxm'), 'jade-kasa', 'the Jade T2 great kasa: a broad shallow cone of plaited straw, '
         'the vermilion binding and chin cord', kasa(), 'sighelm-jade,helm.t2,!helm.t3', True),
    ]


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


def drop_superseded(path, lines):
    for p, b, e in SUPERSEDED:
        if os.path.normcase(p) != os.path.normcase(path):
            continue
        i = next((n for n, s in enumerate(lines) if s.startswith(b)), None)
        if i is None:
            continue
        j = next(n for n in range(i, len(lines)) if lines[n].startswith(e))
        lines = lines[:i] + lines[j + 1:]
    return lines


def used_chars(lines):
    used = set()
    for s in lines:
        if s.startswith('|'):
            used.update(s[1:])
        elif s.startswith('legend '):
            used.add(s.split()[1])
    for lg in ('person', 'woman'):
        for s in open(os.path.join(MAPS, 'legend', lg + '.legend'), encoding='utf-8'):
            if s.startswith('legend '):
                used.add(s.split()[1])
    return used


def block(tag, desc, views, cond, replace, used):
    need = sorted({c for v in views for r in v[2] for c in r if c not in '.-'})
    pool = [c for c in FREE if c not in used]
    assign = {}
    for c in need:
        if c not in ALPHA:
            raise SystemExit(f'{tag}: {c!r} is not in the authoring alphabet')
        assign[c] = pool.pop(0)
    lines = [f'# {TAG} begin {tag}: {desc}']
    lines += [f'legend {assign[c]} {ALPHA[c][0]} {ALPHA[c][1]}' for c in need]
    for f, k, g, org in views:
        lines.append(f'view {f} key {k} origin {org[0]} {org[1]} when {cond}{" replace" if replace else ""}')
        lines += ['|' + ''.join(assign.get(c, c) for c in r) for r in g]
    lines.append(f'# {TAG} end {tag}')
    return lines, set(assign.values())


def build(path, text=None):
    """the map's new lines: the file's without this script's blocks and the superseded ones, this round's blocks on top"""
    text = open(path, encoding='utf-8').read() if text is None else text
    src = drop_superseded(path, own_strip(text.replace('\r\n', '\n').split('\n')))
    used = used_chars(src)
    add = []
    for p, tag, desc, views, cond, rep in plan():
        if os.path.normcase(p) != os.path.normcase(path):
            continue
        lines, chars = block(tag, desc, views, cond, rep, used)
        used |= chars
        add += lines
    i = next(n for n, s in enumerate(src) if s.startswith('view ') or s.startswith('# (P5b SIG'))
    return src[:i] + add + src[i:], add


def touched():
    out = []
    for p in [p for p, *_ in plan()] + [p for p, _, _ in SUPERSEDED]:
        if p not in out:
            out.append(p)
    return out


def head_text(path):
    rel = os.path.relpath(path, ROOT).replace('\\', '/')
    return subprocess.run(['git', '-C', ROOT, 'show', f'HEAD:{rel}'], capture_output=True, text=True, encoding='utf-8').stdout


def write():
    for path in touched():
        new, add = build(path, head_text(path))
        open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(new))
        print(f'{os.path.relpath(path, ROOT)}: +{len(add)} lines (this round)')


def verify():
    bad = 0
    for path in touched():
        rel = os.path.relpath(path, ROOT).replace('\\', '/')
        old = drop_superseded(path, own_strip(head_text(path).replace('\r\n', '\n').split('\n')))
        now = drop_superseded(path, own_strip(open(path, encoding='utf-8').read().replace('\r\n', '\n').split('\n')))
        if old != now:
            print(f'FAIL {rel}: outside this round\'s blocks and the superseded ones the map differs from HEAD')
            bad += 1
        else:
            print(f'ok   {rel}: only {TAG} blocks added (superseded production blocks out)')
    return bad


def rle_len(cs):
    n, i = 0, 0
    while i < len(cs):
        if cs[i] == 0:
            k = 0
            while i < len(cs) and cs[i] == 0 and k < 128:
                k += 1
                i += 1
            n += 1
        else:
            j = i
            while j < len(cs) and j - i < 128 and cs[j] != 0:
                j += 1
            n += 1 + (j - i)
            i = j
    return n


def cost():
    tot = 0
    for p, tag, desc, views, cond, rep in plan():
        planes, b = set(), 0
        for f, k, g, org in views:
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
        sys.exit(1 if verify() else 0)
    if '--cost' in sys.argv:
        cost()
        sys.exit(0)
    if '--print' in sys.argv:
        for path in touched():
            print('\n'.join(build(path, head_text(path))[1]))
        sys.exit(0)
    write()
