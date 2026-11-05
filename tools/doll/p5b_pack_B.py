#!/usr/bin/env python3
"""tools/doll/p5b_pack_B.py (P5b production, area B: Dune, Steppe, Marsh at T1-T4): the owner pack and the slice's measures
run for the area's peoples against all twelve (docs/STYLE_P5B.md 9.3, the slice judge's S1 and R3a).

Inputs (one build, one pool):
  EMB_SLICE_PEOPLE=<all twelve> EMB_SLICE_TIERS=1234 fingerprint_gallery <dir>/after --slice --seeds 1..3 [--pool P]
  EMB_FIX_RULES=8191 (the same) <dir>/before                                    (P5's look: before | after)
  witness_pack.mjs --keep tools/scripts/p5b_B.txt                                (the in-play rows at NEAR, hidden)

  python tools/doll/p5b_pack_B.py --fig <dir> --out <pack dir> [--play <witness run dir>] [--measure-only]

Writes (nearest-neighbour scaling, never a filter on our pixels; every PNG at most 8000 px a side):
  sheet_<people>.png    references and FLUX concepts (ideas only) | T1-T4 man and woman, D and R at 6x | the kit icons at 6x
  tiers_doll_grey.png   at each tier the area's three peoples beside the slice's three (the hardest neighbours named in
                        STYLE_P5B and the brief: Dune / Steppe / Jade): colour, greyscale and the black silhouette fill, 3x
  before_after.png      P5 (EMB_FIX_RULES=8191) | P5b, each people at T1-T4, man D, 4x
  tiers_near.png        in play at NEAR (D, R), colour | greyscale (when --play)
  captions.txt, measure.txt
measure.txt: S1 (masks, with and without the shield, the head alone) for every pair of an area people and any other of
the twelve, per tier T1-T4, body, seed 1-3 and facing; R3a (each figure's nearest other figure in greyscale among the twelve
peoples' 36 must be its own people's, idle, walk and no shield); S2 (the people's share of each figure, 5-30 %).
"""
import argparse
import glob
import os
import sys

from PIL import Image, ImageDraw

ALL = ['fjordfolk', 'highland', 'heartland', 'imperial', 'dune', 'steppe', 'marsh', 'jade', 'river', 'suntemple', 'sylvan',
       'starspire']
MINE = ['dune', 'steppe', 'marsh']
NAMES = {'dune': 'DUNE', 'steppe': 'STEPPE', 'marsh': 'MARSH'}
TIERS = [1, 2, 3, 4]
SEEDS = [1, 2, 3]
BG = (118, 132, 96)
INK = (236, 228, 206)
DARK = (36, 32, 40)
PAIR = {'dune': (0x18849E, 0xECE2C8), 'steppe': (0x9C4426, 0x3CACD6), 'marsh': (0x707E28, 0xD6CCA8)}   # rpg/core/fingerprint.h
REFS = {   # STYLE_P5B 5's "From:" lists (real armour photographs: thumbnails only)
    'dune': ['dune_18th_century_sikh_kulah_khud_helmet_inside_the_white_tower_a', 'dune_helmet_khula_khud_met_02_5_5_001mar2015',
             'dune_13_mameluke_of_egypt', 'dune_armour_of_the_ottoman_empire_zirah_baktar_or_zirh_gomlek',
             'dune_antique_ottoman_empire_armour_krug', 'dune_an_illustrated_and_illuminated_folio_from_a_manuscript_of_hu'],
    'steppe': ['steppe_closeup_of_mongolian_reenactors_at_khaan_quest_07', 'steppe_bronze_helmet_mongol_empire_19219793614',
               'steppe_a_man_and_woman_in_traditional_mongolian_dress_look_on_as_se', 'steppe_pursuit_by_mongol_warriors_diez_albums',
               'steppe_baba_0084', 'steppe_1281battleofhoms'],
    'marsh': ['marsh_01973_tollundmanden', 'marsh_coconut_fiber_armor_from_the_republic_of_kiribati_micronesia',
              'marsh_ainu_group_aborigines_of_japan_from_the_island_of_hokkaido_o', 'marsh_ancient_celt_with_carnyx_ancient_briton',
              'marsh_armor_kiribati_1878_ethnological_museum_berlin_dsc01261', 'marsh_ainus_in_national_gala_costume_married_women_with_tattooed_m'],
}
CONCEPTS = ['full_levy_man', 'full_levy_woman', 'full_veteran_man', 'full_veteran_woman', 'full_elite_man', 'full_elite_woman',
            'helmets_1', 'helmets_2', 'shields', 'pixel_1', 'pixel_2']
FIG_BOX = (10, 12, 54, 62)   # the figure window inside the 64x64 gallery canvas (the pivot at 32, 60)


def load(path):
    """a gallery figure: black (0,0,0) is clear (savePng writes the clear canvas black)"""
    im = Image.open(path).convert('RGBA')
    px = im.load()
    for y in range(im.height):
        for x in range(im.width):
            if px[x, y][:3] == (0, 0, 0):
                px[x, y] = (0, 0, 0, 0)
    return im


def grey(im):
    g = im.convert('LA').convert('RGBA')
    g.putalpha(im.split()[3])
    return g


def black(im):
    a = im.split()[3]
    out = Image.new('RGBA', im.size, (24, 20, 28, 255))
    out.putalpha(a)
    return out


def up(im, k):
    return im.resize((im.width * k, im.height * k), Image.NEAREST)


def text(d, xy, s, fill=INK):
    d.text(xy, s, fill=fill)


def figpath(d, p, t, b, f, suffix='', seed=1):
    return os.path.join(d, 'slice_%s_T%d_%s_s%d_%s%s.png' % (p, t, b, seed, f, suffix))


def fig(d, p, t, b, f, suffix='', seed=1):
    path = figpath(d, p, t, b, f, suffix, seed)
    return load(path).crop(FIG_BOX) if os.path.exists(path) else None


def thumb(path, h):
    im = Image.open(path).convert('RGB')
    w = max(1, int(im.width * h / im.height))
    return im.resize((w, h), Image.LANCZOS)   # (a photograph or concept thumbnail: filtered; our pixels never are)


# ------------------------------------------------------------------------------------------------ the sheets
def people_sheet(p, fdir, ref, out):
    k = 6
    fw, fh = (FIG_BOX[2] - FIG_BOX[0]) * k, (FIG_BOX[3] - FIG_BOX[1]) * k
    cols = [('m', 'D'), ('m', 'R'), ('w', 'D'), ('w', 'R')]
    th = 150
    refs = [os.path.join(ref, p, r + '.jpg') for r in REFS[p]]
    refs = [r for r in refs if os.path.exists(r)]
    gen = [os.path.join(ref, 'generated', p, '%s_%s.png' % (p, c)) for c in CONCEPTS]
    gen = [g for g in gen if os.path.exists(g)]
    left_w = 3 * 230 + 40
    W = left_w + len(cols) * (fw + 12) + 40 + 6 * (16 * k + 10) + 30
    H = 70 + len(TIERS) * (fh + 34) + 40
    Hl = 70 + ((len(refs) + 2) // 3 + (len(gen) + 2) // 3) * (th + 6) + 80
    sheet = Image.new('RGB', (W, max(H, Hl)), BG)
    d = ImageDraw.Draw(sheet)
    text(d, (14, 10), '%s  P5b (area B): reference photographs and FLUX concepts (AI, ideas only: never traced) | our doll T1-T4, man and '
                      'woman, D and R at 6x | the kit icons at 6x (greyscale under each)' % NAMES[p])
    dom, acc = PAIR[p]
    for i, c in enumerate((dom, acc)):
        d.rectangle([14 + i * 40, 26, 14 + i * 40 + 30, 44], fill=((c >> 16) & 255, (c >> 8) & 255, c & 255), outline=DARK)
    text(d, (100, 30), 'dominant #%06X  accent #%06X' % (dom, acc))
    y = 60
    text(d, (14, y), 'REFERENCE (real armour)')
    y += 14
    for i, r in enumerate(refs):
        t = thumb(r, th)
        t = t.crop((0, 0, min(t.width, 225), th))
        sheet.paste(t, (14 + (i % 3) * 230, y + (i // 3) * (th + 6)))
    y += ((len(refs) + 2) // 3) * (th + 6) + 10
    text(d, (14, y), 'FLUX CONCEPTS (ideas only)')
    y += 14
    for i, g in enumerate(gen):
        t = thumb(g, th)
        cw = min(t.width, 225)
        t = t.crop(((t.width - cw) // 2, 0, (t.width - cw) // 2 + cw, th))
        sheet.paste(t, (14 + (i % 3) * 230, y + (i // 3) * (th + 6)))
    x0 = left_w
    for ci, (b, f) in enumerate(cols):
        text(d, (x0 + ci * (fw + 12) + 4, 52), ('MAN ' if b == 'm' else 'WOMAN ') + f)
    for ti, t in enumerate(TIERS):
        yy = 70 + ti * (fh + 34)
        text(d, (x0 - 34, yy + fh // 2), 'T%d' % t)
        for ci, (b, f) in enumerate(cols):
            im = fig(fdir, p, t, b, f)
            if im is None:
                continue
            xx = x0 + ci * (fw + 12)
            d.rectangle([xx - 2, yy - 2, xx + fw + 1, yy + fh + 1], outline=(96, 108, 78))
            sheet.paste(up(im, k), (xx, yy), up(im, k))
    xi = x0 + len(cols) * (fw + 12) + 30
    for pi, piece in enumerate(['helm', 'body', 'shield', 'gloves', 'boots', 'cloak']):
        text(d, (xi + pi * (16 * k + 10), 52), piece.upper())
    for ti, t in enumerate(TIERS):
        yy = 70 + ti * (fh + 34) + fh // 2 - 8 * k
        for pi, piece in enumerate(['helm', 'body', 'shield', 'gloves', 'boots', 'cloak']):
            path = os.path.join(fdir, 'slice_icon_%s_T%d_%s_s1.png' % (p, t, piece))
            if not os.path.exists(path):
                continue
            ic = up(load(path), k)
            xx = xi + pi * (16 * k + 10)
            d.rectangle([xx - 2, yy - 2, xx + 16 * k + 1, yy + 16 * k + 1], fill=DARK)
            sheet.paste(ic, (xx, yy), ic)
            g = up(grey(load(path)), 2)
            sheet.paste(g, (xx + 16 * k - 32, yy + 16 * k + 4), g)
    sheet.save(os.path.join(out, 'sheet_%s.png' % p))
    return sheet.size


def tiers_doll(fdir, out):
    """per tier the area's three peoples beside the slice's (Jade, Imperial, Fjordfolk): colour, greyscale, black fill, 3x"""
    k = 3
    fw, fh = (FIG_BOX[2] - FIG_BOX[0]) * k, (FIG_BOX[3] - FIG_BOX[1]) * k
    modes = [('colour', lambda im: im), ('greyscale', grey), ('silhouette', black)]
    peoples = MINE + ['jade', 'imperial', 'fjordfolk']
    per = [(p, b, f) for p in peoples for b in 'mw' for f in 'DR']
    W = 120 + len(per) * (fw + 6)
    H = 40 + len(TIERS) * len(modes) * (fh + 8) + 20
    sheet = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(sheet)
    text(d, (10, 8), 'P5b area B from the doll at 3x: each tier Dune, Steppe, Marsh beside the slice peoples (man D R, woman D R), in '
                     'colour, greyscale and the black silhouette fill')
    for i, (p, b, f) in enumerate(per):
        if b == 'm' and f == 'D':
            text(d, (120 + i * (fw + 6), 24), p.upper())
    for ti, t in enumerate(TIERS):
        for mi, (mn, fn) in enumerate(modes):
            yy = 40 + (ti * len(modes) + mi) * (fh + 8)
            text(d, (8, yy + fh // 2), 'T%d %s' % (t, mn))
            for i, (p, b, f) in enumerate(per):
                im = fig(fdir, p, t, b, f)
                if im is None:
                    continue
                im = up(fn(im), k)
                sheet.paste(im, (120 + i * (fw + 6), yy), im)
    sheet.save(os.path.join(out, 'tiers_doll_grey.png'))
    return sheet.size


def before_after(bdir, adir, out):
    k = 4
    fw, fh = (FIG_BOX[2] - FIG_BOX[0]) * k, (FIG_BOX[3] - FIG_BOX[1]) * k
    W = 140 + 2 * len(TIERS) * (fw + 8) + 40
    H = 40 + len(MINE) * (fh + 14)
    sheet = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(sheet)
    text(d, (10, 8), 'BEFORE (P5, EMB_FIX_RULES=8191) | AFTER (P5b): T1-T4, man facing D, 4x')
    for pi, p in enumerate(MINE):
        yy = 34 + pi * (fh + 14)
        text(d, (8, yy + fh // 2), NAMES[p])
        for side, dirp in enumerate((bdir, adir)):
            for ti, t in enumerate(TIERS):
                im = fig(dirp, p, t, 'm', 'D')
                if im is None:
                    continue
                xx = 140 + side * (len(TIERS) * (fw + 8) + 40) + ti * (fw + 8)
                sheet.paste(up(im, k), (xx, yy), up(im, k))
    d.line([140 + len(TIERS) * (fw + 8) + 16, 30, 140 + len(TIERS) * (fw + 8) + 16, H - 6], fill=INK, width=2)
    sheet.save(os.path.join(out, 'before_after.png'))
    return sheet.size


def play_sheets(play, out):
    """the in-play rows at NEAR (tools/scripts/p5b_B.txt): per tier the D and R shots, colour | greyscale; close shots"""
    made = []
    shots = {os.path.basename(f): f for f in glob.glob(os.path.join(play, '**', 'p5b_B_*.png'), recursive=True)}
    ROW6, ROW3 = (380, 30, 1060, 230), (470, 20, 850, 220)
    rows = []
    for t in TIERS:
        for f in 'dr':
            n = 'p5b_B_t%d_%s.png' % (t, f)
            if n in shots:
                im = Image.open(shots[n]).convert('RGB').crop(ROW6)
                rows.append((t, f, im.resize((im.width * 2, im.height * 2), Image.NEAREST)))
    if rows:
        w = max(r[2].width for r in rows)
        h = sum(r[2].height + 30 for r in rows)
        sheet = Image.new('RGB', (w * 2 + 10, h), DARK)
        d = ImageDraw.Draw(sheet)
        y = 0
        for t, f, im in rows:
            text(d, (6, y + 6), 'T%d facing %s, in play at NEAR (hidden Witness): DUNE man, woman | STEPPE man, woman | MARSH man, woman  '
                                '(colour | greyscale)' % (t, f.upper()))
            sheet.paste(im, (0, y + 24))
            sheet.paste(im.convert('L').convert('RGB'), (im.width + 10, y + 24))
            y += im.height + 30
        sheet.save(os.path.join(out, 'tiers_near.png'))
        made.append(('tiers_near.png', sheet.size))
    for n in sorted(shots):
        if n.startswith('p5b_B_close'):
            im = Image.open(shots[n]).convert('RGB').crop(ROW3)
            im = im.resize((im.width * 3, im.height * 3), Image.NEAREST)
            im.save(os.path.join(out, n.replace('p5b_B_', '')))
            made.append((n.replace('p5b_B_', ''), im.size))
    return made


# ------------------------------------------------------------------------------------------------ the measures
def mask_of(path):
    im = Image.open(path).convert('RGB')
    px = im.load()
    return {(x, y) for y in range(im.height) for x in range(im.width) if px[x, y] != (0, 0, 0)}


def grey_vec(path):
    im = Image.open(path).convert('RGB')
    return [-60 if c == (0, 0, 0) else 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2] for c in im.getdata()]


def s1(fdir, L):
    """the slice's S1 for every pair (an area people, any other of the twelve)"""
    L.append('S1 silhouette (opaque masks, XOR px; need >= max(12, smaller/25) with the shield, >= max(8, smaller/40) without, the head')
    L.append('   >= max(6, smaller head/5) in one facing and >= 3 in the other); each area people against the other eleven, T1-T4,')
    L.append('   man and woman, seeds 1-3, facings D and R. Lines: the tightest pair per people and tier, then every miss')
    cache = {}

    def m(p, t, b, f, suf, s):
        k = (p, t, b, f, suf, s)
        if k not in cache:
            path = figpath(fdir, p, t, b, f, suf, s)
            cache[k] = mask_of(path) if os.path.exists(path) else None
        return cache[k]
    ok_all, misses, n = True, [], 0
    for p in MINE:
        for t in TIERS:
            tight = None
            for q in ALL:
                if q == p:
                    continue
                for b in 'mw':
                    for s in SEEDS:
                        parts, good, heads, worst = [], True, [], 9e9
                        for f in 'DR':
                            A, B = m(p, t, b, f, '', s), m(q, t, b, f, '', s)
                            An, Bn = m(p, t, b, f, '_ns', s), m(q, t, b, f, '_ns', s)
                            Ah, Bh = m(p, t, b, f, '_head', s), m(q, t, b, f, '_head', s)
                            if None in (A, B, An, Bn, Ah, Bh):
                                good = False
                                parts.append('%s missing' % f)
                                continue
                            x1, n1 = len(A ^ B), max(12, min(len(A), len(B)) / 25)
                            x2, n2 = len(An ^ Bn), max(8, min(len(An), len(Bn)) / 40)
                            x3 = len(Ah ^ Bh)
                            heads.append((x3, max(6, min(len(Ah), len(Bh)) / 5)))
                            good &= x1 >= n1 and x2 >= n2
                            worst = min(worst, x1 / n1, x2 / n2)
                            parts.append('%s fig %d/%.0f noshield %d/%.0f head %d/%.0f' % (f, x1, n1, x2, n2, x3, heads[-1][1]))
                        if len(heads) == 2:
                            hd = (heads[0][0] >= heads[0][1] and heads[1][0] >= 3) or (heads[1][0] >= heads[1][1] and heads[0][0] >= 3)
                            good &= hd
                            worst = min(worst, max(heads[0][0] / heads[0][1], heads[1][0] / heads[1][1]))
                        n += 1
                        line = '  T%d %s s%d %-9s/%-9s %s' % (t, 'man  ' if b == 'm' else 'woman', s, p, q, '; '.join(parts))
                        if not good:
                            ok_all = False
                            misses.append(line + ' | MISS')
                        if tight is None or worst < tight[0]:
                            tight = (worst, line)
            if tight:
                L.append('%s | tightest (margin x%.2f)' % (tight[1], tight[0]))
    L.append('S1: %d pairs, %d miss: %s' % (n, len(misses), 'PASS' if ok_all else 'FAIL'))
    L.extend(misses[:60])
    return ok_all


def r3a(fdir, L):
    """each area figure's nearest other figure (greyscale L1, canvas aligned, clear = -60) among the twelve peoples' 36 at its
    tier, body, facing and variant must be its own people's (another seed)"""
    bad, lines, worst = 0, 0, []
    for t in TIERS:
        for b in 'mw':
            for f in 'DR':
                for var in ['', '_walk', '_ns']:
                    figs = {}
                    for p in ALL:
                        for s in SEEDS:
                            path = figpath(fdir, p, t, b, f, var, s)
                            if os.path.exists(path):
                                figs[(p, s)] = grey_vec(path)
                    for k, g in figs.items():
                        if k[0] not in MINE:
                            continue
                        dist = lambda h: sum(abs(x - y) for x, y in zip(g, h))
                        own = min((dist(h) for k2, h in figs.items() if k2 != k and k2[0] == k[0]), default=9e9)
                        oth = min(((dist(h), k2[0]) for k2, h in figs.items() if k2[0] != k[0]), default=(9e9, ''))
                        lines += 1
                        if oth[0] <= own:
                            bad += 1
                        worst.append((oth[0] / max(own, 1), t, b, f, var or '_idle', k, oth[1]))
    worst.sort()
    L.append('R3a nearest-neighbour people read (the slice judge\'s, over all twelve peoples): %d of %d area figures nearest to '
             'another people' % (bad, lines))
    for mg, t, b, f, var, k, q in worst[:12]:
        L.append('   T%d %s %s %-6s %-7s s%d  nearest other %-9s other/own %.2f%s' % (t, b, f, var, k[0], k[1], q, mg,
                                                                                 '  MISS' if mg <= 1 else ''))
    return bad == 0


def s2(fdir, L):
    L.append('S2 the people colour on the figure (slice_shares.txt, idle D, seed 1): the dye slots and the signed cloth, steps 1-4;')
    L.append('   want 5-30 % of the figure')
    sh = os.path.join(fdir, 'slice_shares.txt')
    ok = True
    if os.path.exists(sh):
        for ln in open(sh, encoding='utf-8'):
            q = ln.split()
            if len(q) < 10 or not q[1].endswith('_s1') or not any(q[1].startswith('slice_%s_' % p) for p in MINE):
                continue
            pct = float(q[-1].rstrip('%'))
            good = 5 <= pct <= 30
            ok &= good
            L.append('  %-28s area %4s  dye %3s  cloth %3s  %5.1f %%  %s' % (q[1], q[3], q[5], q[7], pct, 'ok' if good else 'OUT'))
    return ok


def n2(fdir, L):
    """the slice judge's N2 (S5's greyscale read): per piece and tier, each area people's icon against every other people's
    (seeds 1-3): grey-distinct px (mask XOR + shared px whose luma differs by >= 24) >= 24 for helm, body, shield and cloak"""
    def ld(f):
        im = Image.open(f).convert('RGB')
        px = im.load()
        return {(x, y): px[x, y] for y in range(im.height) for x in range(im.width) if px[x, y] != (0, 0, 0)}
    lu = lambda c: 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]
    bad = 0
    L.append('N2 icons in greyscale (the slice judge: each area people against the other eleven; want >= 24 grey-distinct px for')
    L.append('   helm, body, shield and cloak; gloves and boots reported): the worst pair per people, piece and tier')
    for p in MINE:
        for t in TIERS:
            for pc in ('helm', 'body', 'shield', 'gloves', 'boots', 'cloak'):
                worst = None
                for s in SEEDS:
                    fa = os.path.join(fdir, 'slice_icon_%s_T%d_%s_s%d.png' % (p, t, pc, s))
                    if not os.path.exists(fa):
                        continue
                    A = ld(fa)
                    for q in ALL:
                        fb = os.path.join(fdir, 'slice_icon_%s_T%d_%s_s%d.png' % (q, t, pc, s))
                        if q == p or not os.path.exists(fb):
                            continue
                        B = ld(fb)
                        gd = len(set(A) ^ set(B)) + sum(1 for k in set(A) & set(B) if abs(lu(A[k]) - lu(B[k])) >= 24)
                        if worst is None or gd < worst[0]:
                            worst = (gd, q, s)
                if worst:
                    gated = pc in ('helm', 'body', 'shield', 'cloak')
                    miss = gated and worst[0] < 24
                    bad += miss
                    L.append('  T%d %-9s %-6s worst %-9s s%d: %3d grey-distinct px  %s' % (t, p, pc, worst[1], worst[2], worst[0],
                                                                                    'FAIL' if miss else 'ok' if gated else '(reported)'))
    L.append('N2: %d FAIL' % bad)
    return bad == 0


def measure(fdir, out, extra):
    L = ['P5b area B measures (STYLE_P5B 9.3; the slice judge r3), from fingerprint_gallery --slice over the twelve peoples',
         '(EMB_SLICE_PEOPLE all twelve, EMB_SLICE_TIERS 1234, seeds 1-3; idle frame 0 aligned on the pivot)', '']
    a = s1(fdir, L)
    L.append('')
    b = r3a(fdir, L)
    L.append('')
    c = s2(fdir, L)
    L.append('')
    d = n2(fdir, L)
    for e in extra:
        if e and os.path.exists(e):
            L.append('')
            L.extend(open(e, encoding='utf-8').read().rstrip().split('\n'))
    open(os.path.join(out, 'measure.txt'), 'w', encoding='utf-8').write('\n'.join(L) + '\n')
    return a, b, c, d


CAPTIONS = {
    'sheet_dune.png': 'DUNE (vertical spike, flowing kaftan; teal and cream). T1 the cream turban wound low with its teal brow '
                      'wraps and brass brooch, a leather coat with the teal sash and the split kaftan hem; T2 the turban over the '
                      'spiked cap (its steel crown over the turban), lobed shoulders, wide sleeves, the teal scarf; T3 the kulah: '
                      'the needle spike over a steel bowl, a teal turban band wound round it, the mail aventail to the shoulders; '
                      'T4 the kulah grown (its aventail longer) in the alloy. The small round dhal in teal with its cream device.',
    'sheet_steppe.png': 'STEPPE (a tall point, horizontal lamellar rows; rust and sky). T1 the felt hat: a sky-blue crown on a fur '
                        'brim with fur ear flaps, the long split riding coat; T2 the peaked felt cap with its rust knot, the fur '
                        'mantle at the neck; T3 the pointed helm under the rust horsetail, its cheek and neck guards in courses '
                        'of laced plates, lamellar flaps at the shoulders, the rust scarf; T4 the same grown in the alloy, wide '
                        'sleeves. The rust round shield.',
    'sheet_marsh.png': 'MARSH (ragged, drooping; moss and bone). T1 the bone hide hood under a moss grass collar cut into hanging '
                       'bone-tipped strands, a fringe over the brow, bare arms; T2 the rawhide kettle with a bone crest and two '
                       'moss feathers, hide flaps, the fringed strip skirt; T3 the tasselled reed war hat (drooping brim, moss '
                       'wrap, tassels), the reed half cape; T4 the wide war hat drooping, its moss wrap and a reed tuft, bone '
                       'tassels on its cords. The long oval shield in moss.',
    'tiers_doll_grey.png': 'Each tier T1-T4 from the doll at 3x: Dune, Steppe and Marsh beside the slice peoples (Jade, Imperial, '
                           'Fjordfolk), man D R and woman D R, in colour, greyscale and the black silhouette fill.',
    'before_after.png': 'Before (P5, EMB_FIX_RULES=8191: the people as a thin dye band on generic heads) beside after (P5b: the '
                        'people in the head, the shoulders, the hem and the cloth). The same kits, man facing D, 4x.',
    'tiers_near.png': 'In play at NEAR (hidden Witness, tools/scripts/p5b_B.txt): each tier a row of six (Dune man, woman; Steppe '
                      'man, woman; Marsh man, woman), facing D then R; colour beside greyscale.',
}


def captions(out, names):
    L = ['P5b production, area B (docs/STYLE_P5B.md 4-5): Dune, Steppe and Marsh at T1-T4 (T5-T6 wear the people axes and',
         'colours on their own heads until the budget ruling). The reference photographs and the FLUX concepts are thumbnails for',
         'direction only: every doll pixel is our own (hand-placed part views ADDED by tools/doll/p5b_sig_B.py and existing parts',
         'through the doll pipeline). measure.txt holds S1, R3a, S2, the ladder and the pool.', '']
    for n in names:
        L.append('%s: %s' % (n, CAPTIONS.get(n, '')))
    open(os.path.join(out, 'captions.txt'), 'w', encoding='utf-8').write('\n'.join(L) + '\n')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--fig', required=True)
    ap.add_argument('--play', default='')
    ap.add_argument('--out', required=True)
    ap.add_argument('--ref', default=r'C:/Users/joshu/Documents/embervale-ref/armour')
    ap.add_argument('--extra', action='append', default=[], help='text files appended to measure.txt (envelope, pool)')
    ap.add_argument('--measure-only', action='store_true')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    after, before = os.path.join(a.fig, 'after'), os.path.join(a.fig, 'before')
    if not os.path.isdir(after):
        after = a.fig
    made = []
    if not a.measure_only:
        for p in MINE:
            made.append(('sheet_%s.png' % p, people_sheet(p, after, a.ref, a.out)))
        made.append(('tiers_doll_grey.png', tiers_doll(after, a.out)))
        if os.path.isdir(before):
            made.append(('before_after.png', before_after(before, after, a.out)))
        if a.play:
            made += play_sheets(a.play, a.out)
        captions(a.out, [n for n, _ in made])
    ok = measure(after, a.out, a.extra)
    for n, s in made:
        assert s[0] <= 8000 and s[1] <= 8000, n
        print('%-22s %dx%d' % (n, s[0], s[1]))
    print('measure.txt S1 %s, R3a %s, S2 %s, N2 %s' % tuple('PASS' if x else 'FAIL' for x in ok))


if __name__ == '__main__':
    main()
