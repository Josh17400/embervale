#!/usr/bin/env python3
"""tools/doll/p5b_pack.py (P5b, docs/STYLE_P5B.md 9.2 and 9.3): the style slice's owner pack and its measures.

Inputs (made first, both from one build):
  fingerprint_gallery <dir>/after --slice                       the slice with P5b on (the game's rules)
  EMB_FIX_RULES=8191 fingerprint_gallery <dir>/before --slice   the same with FIX_SIG off (P5's look: before | after)
  witness_pack.mjs --keep tools/scripts/p5b_slice.txt           the in-play rows at NEAR (hidden --witness instances)

  python tools/doll/p5b_pack.py --fig <dir> --play <witness run dir> --out <pack dir> [--ref <embervale-ref/armour>]

Writes (PNGs at most 8000 px a side; nearest-neighbour scaling, never a filter on the pixels):
  sheet_<people>.png          reference and FLUX concept thumbnails (ideas only, never traced) | T1/T3/T5 man and woman worn
                              at 6x, facings D and R | the kit icons at 6x
  tiers_near.png              the three peoples side by side at each tier, in play at NEAR (D and R), colour over greyscale
  tiers_doll_grey.png         the same tiers from the doll at 3x: colour, greyscale and the black silhouette fill
  before_after.png            P5 (EMB_FIX_RULES=8191) | P5b, each people at T1/T3/T5, man D
  ladder.png                  each people's T1-T6 (the gallery kit sheet) for the climb
  close_<n>.png               in-play close shots
  captions.txt, measure.txt   what each sheet shows; S1 (masks), S2 (shares), S6 (areas) measured on the 1x figures
"""
import argparse
import glob
import os
import sys

from PIL import Image, ImageDraw

PEOPLE = ['jade', 'imperial', 'fjordfolk']
NAMES = {'jade': 'JADE', 'imperial': 'IMPERIAL', 'fjordfolk': 'FJORDFOLK'}
TIERS = [1, 3, 5]
BG = (118, 132, 96)
INK = (236, 228, 206)
DARK = (36, 32, 40)
PAIR = {'jade': (0xE8641C, 0x1C1A1E), 'imperial': (0x7C082A, 0xE2B840), 'fjordfolk': (0x4878C8, 0xE8E2CE)}   # rpg/core/fingerprint.h
REFS = {   # STYLE_P5B 5's "From:" lists (real armour photographs: thumbnails only)
    'jade': ['jade_3_jingasa', 'jade_ashigaru_jingasa', 'jade_akaito_odoshi_yoroi_replica', 'jade_akaito_odoshi_yoroi_replica_kabuto',
             'jade_helmet_hoshi_kabuto_met_dt306020', 'jade_antique_samurai_o_yoroi_1'],
    'imperial': ['imperial_bronze_legionary_helmet_1st_half_of_the_1st_century_ad_xante', 'imperial_legiii',
                 'imperial_legio_xxi_rapax_sechsel_uten_2011_lindenhof_2011_04_10_15_58', 'imperial_cricau_festival_2013_lorica_squamata_1',
                 'imperial_2008_08_archeon_testudo', 'imperial_praetorian_cohiii'],
    'fjordfolk': ['fjordfolk_0920_spangen_helmet_from_5th_6th_c_replica', 'fjordfolk_0921_spangen_helmet_from_groningen_holland_5th_6th_c_replica',
                  'fjordfolk_leidang_kriger_norr_n_ringbrynje_hjelm_nesebeskytter_middela', 'fjordfolk_leidang_kriger_norr_n_ringbrynje_hjelm_sverd_middelalder_nor',
                  'fjordfolk_2004_sutton_hoo_01', 'fjordfolk_02019_1011_schlachten_der_rus_war_ger_und_saqaliba_wikingerl'],
}
CONCEPTS = ['full_levy_man', 'full_levy_woman', 'full_veteran_man', 'full_veteran_woman', 'full_elite_man', 'full_elite_woman',
            'helmets_1', 'shields', 'pixel_1']


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


FIG_BOX = (10, 12, 54, 62)   # the figure window inside the 64x64 gallery canvas (the pivot at 32, 60)


def fig(dirp, p, t, b, f, suffix='', seed=1):
    path = os.path.join(dirp, 'slice_%s_T%d_%s_s%d_%s%s.png' % (p, t, b, seed, f, suffix))
    return load(path).crop(FIG_BOX) if os.path.exists(path) else None


def whole(dirp, p, t, b, f, suffix='', seed=1):
    """a gallery figure's whole 64x64 canvas (the measures: nothing cropped, as the judge's harness reads them)"""
    return load(os.path.join(dirp, 'slice_%s_T%d_%s_s%d_%s%s.png' % (p, t, b, seed, f, suffix)))


def thumb(path, h):
    im = Image.open(path).convert('RGB')
    w = max(1, int(im.width * h / im.height))
    return im.resize((w, h), Image.LANCZOS)   # (a photograph or concept thumbnail: filtered; our pixels never are)


def people_sheet(p, fdir, ref, out):
    k = 6
    fw, fh = (FIG_BOX[2] - FIG_BOX[0]) * k, (FIG_BOX[3] - FIG_BOX[1]) * k
    cols = [('m', 'D'), ('m', 'R'), ('w', 'D'), ('w', 'R')]
    # left: the references and the concepts
    th = 150
    refs = [os.path.join(ref, p, r + '.jpg') for r in REFS[p]]
    refs = [r for r in refs if os.path.exists(r)]
    gen = [os.path.join(ref, 'generated', p, '%s_%s.png' % (p, c)) for c in CONCEPTS]
    gen = [g for g in gen if os.path.exists(g)]
    left_w = 3 * 230 + 40
    W = left_w + len(cols) * (fw + 12) + 40 + 6 * (16 * k + 10) + 30
    H = 70 + len(TIERS) * (fh + 34) + 40
    sheet = Image.new('RGB', (W, max(H, 70 + ((len(refs) + 2) // 3 + (len(gen) + 2) // 3) * (th + 24) + 80)), BG)
    d = ImageDraw.Draw(sheet)
    text(d, (14, 10), '%s  P5b STYLE SLICE: reference (photographs) and FLUX concepts (AI, ideas only: never traced) | our doll T1 T3 T5, man and woman, D and R at 6x | icons at 6x' % NAMES[p])
    dom, acc = PAIR[p]
    for i, c in enumerate((dom, acc)):
        d.rectangle([14 + i * 40, 26, 14 + i * 40 + 30, 44], fill=((c >> 16) & 255, (c >> 8) & 255, c & 255), outline=DARK)
    text(d, (100, 30), 'dominant #%06X  accent #%06X' % (dom, acc))
    y = 60
    text(d, (14, y), 'REFERENCE (real armour)'); y += 14
    for i, r in enumerate(refs):
        t = thumb(r, th)
        t = t.crop((0, 0, min(t.width, 225), th))
        sheet.paste(t, (14 + (i % 3) * 230, y + (i // 3) * (th + 6)))
    y += ((len(refs) + 2) // 3) * (th + 6) + 10
    text(d, (14, y), 'FLUX CONCEPTS (ideas only)'); y += 14
    for i, g in enumerate(gen):
        t = thumb(g, th)
        t = t.crop(((t.width - min(t.width, 225)) // 2, 0, (t.width - min(t.width, 225)) // 2 + min(t.width, 225), th))
        sheet.paste(t, (14 + (i % 3) * 230, y + (i // 3) * (th + 6)))
    # middle: the worn kit
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
    # right: the icons (16 px at 6x) per tier
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
    """the three peoples side by side per tier from the doll: colour, greyscale, black fill; man and woman, D and R, 3x"""
    k = 3
    fw, fh = (FIG_BOX[2] - FIG_BOX[0]) * k, (FIG_BOX[3] - FIG_BOX[1]) * k
    modes = [('colour', lambda im: im), ('greyscale', grey), ('silhouette', black)]
    per = [(p, b, f) for p in PEOPLE for b in 'mw' for f in 'DR']
    W = 120 + len(per) * (fw + 6)
    H = 40 + len(TIERS) * len(modes) * (fh + 8) + 20
    sheet = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(sheet)
    text(d, (10, 8), 'P5b slice from the doll at 3x: each tier the three peoples (man D R, woman D R), in colour, greyscale and the black silhouette fill')
    for i, (p, b, f) in enumerate(per):
        if b == 'm' and f == 'D':
            text(d, (120 + i * (fw + 6), 24), NAMES[p])
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
    H = 40 + len(PEOPLE) * (fh + 14)
    sheet = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(sheet)
    text(d, (10, 8), 'BEFORE (P5, EMB_FIX_RULES=8191) | AFTER (P5b): T1 T3 T5, man facing D, 4x')
    for pi, p in enumerate(PEOPLE):
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
    """the in-play rows at NEAR: per tier the D and R shots, colour over greyscale; the close shots copied"""
    made = []
    shots = {}
    for f in glob.glob(os.path.join(play, '**', 'p5b_slice_*.png'), recursive=True):
        shots[os.path.basename(f)] = f
    # the rows stand on the start village's clear ground at a fixed place (seed 7, `at -140 -432`): the six figures inside
    # ROW6, the close rows of three inside ROW3 (the 1440 x 810 window at NEAR); scaled by whole numbers, never filtered
    ROW6, ROW3 = (380, 30, 1060, 230), (470, 20, 850, 220)
    rows = []
    for t in TIERS:
        for f in 'dr':
            n = 'p5b_slice_t%d_%s.png' % (t, f)
            if n in shots:
                im = Image.open(shots[n]).convert('RGB').crop(ROW6)
                rows.append((t, f, im.resize((im.width * 2, im.height * 2), Image.NEAREST)))
    if rows:
        w = max(r[2].width for r in rows)
        h = sum(r[2].height for r in rows)
        scale = 1
        while (w * 2) * scale > 8000 or h * scale > 8000:
            scale = 0.5 if scale == 1 else scale / 2
        sheet = Image.new('RGB', (int(w * 2 * scale) + 10, int(h * scale) + 30 * len(rows)), DARK)
        d = ImageDraw.Draw(sheet)
        y = 0
        for t, f, im in rows:
            if scale != 1:
                im = im.resize((int(im.width * scale), int(im.height * scale)), Image.NEAREST)
            text(d, (6, y + 6), 'T%d facing %s, in play at NEAR (hidden Witness): JADE man, woman | IMPERIAL man, woman | FJORDFOLK man, woman  (colour | greyscale)' % (t, f.upper()))
            sheet.paste(im, (0, y + 24))
            sheet.paste(im.convert('L').convert('RGB'), (im.width + 10, y + 24))
            y += im.height + 30
        sheet.save(os.path.join(out, 'tiers_near.png'))
        made.append(('tiers_near.png', sheet.size))
    for n in sorted(shots):
        if n.startswith('p5b_slice_close'):
            im = Image.open(shots[n]).convert('RGB').crop(ROW3)
            im = im.resize((im.width * 3, im.height * 3), Image.NEAREST)
            im.save(os.path.join(out, n.replace('p5b_slice_', '')))
            made.append((n.replace('p5b_slice_', ''), im.size))
    return made


# ------------------------------------------------------------------------------------------------ the measures
def mask(im):
    a = im.split()[3]
    return {(x, y) for y in range(im.height) for x in range(im.width) if a.getpixel((x, y))}


def lab(c):
    def lin(v):
        v /= 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = [lin(v) for v in c[:3]]
    X = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
    Y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    Z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883
    f = lambda t: t ** (1 / 3) if t > 0.008856 else 7.787 * t + 16 / 116
    X, Y, Z = f(X), f(Y), f(Z)
    return (116 * Y - 16, 500 * (X - Y), 200 * (Y - Z))


def measure(fdir, out, budget_line, env=''):
    L = ['P5b style slice measures (STYLE_P5B 9.3), from fingerprint_gallery --slice (idle frame 0, aligned on the pivot)', '']
    L.append('S1 silhouette (opaque masks, XOR px; need >= max(12, smaller/25) with the shield, >= max(8, smaller/40) without, the head')
    L.append('   >= max(6, smaller head/5) in one facing and >= 3 in the other)')
    # (fix r1) every seed the gallery rendered (1-3), the judge's thresholds as written (no floor division), and the no-shield
    # line on the true shield-less renders (_ns: shield=0,shieldForm=9,shieldStyle=0): the same numbers as the judge's ns_measure
    seeds = sorted({int(n.split('_s')[-1].split('_')[0]) for n in os.listdir(fdir) if n.startswith('slice_jade_T1_m_s') and n.endswith('_D.png')})
    ok_all = True
    for seed in seeds or [1]:
        for t in TIERS:
            for b in 'mw':
                for i, p in enumerate(PEOPLE):
                    for q in PEOPLE[i + 1:]:
                        parts = []
                        good = True
                        heads = []
                        for f in 'DR':
                            A, B = whole(fdir, p, t, b, f, '', seed), whole(fdir, q, t, b, f, '', seed)
                            An, Bn = whole(fdir, p, t, b, f, '_ns', seed), whole(fdir, q, t, b, f, '_ns', seed)
                            Ah, Bh = whole(fdir, p, t, b, f, '_head', seed), whole(fdir, q, t, b, f, '_head', seed)
                            ma, mb = mask(A), mask(B)
                            x1 = len(ma ^ mb)
                            n1 = max(12, min(len(ma), len(mb)) / 25)
                            na, nb = mask(An), mask(Bn)
                            x2 = len(na ^ nb)
                            n2 = max(8, min(len(na), len(nb)) / 40)
                            ha, hb = mask(Ah), mask(Bh)
                            x3 = len(ha ^ hb)
                            heads.append((x3, max(6, min(len(ha), len(hb)) / 5)))
                            good &= x1 >= n1 and x2 >= n2
                            parts.append('%s fig %d/%.0f noshield %d/%.0f head %d/%d' % (f, x1, n1, x2, n2, x3, min(len(ha), len(hb))))
                        hd = (heads[0][0] >= heads[0][1] and heads[1][0] >= 3) or (heads[1][0] >= heads[1][1] and heads[0][0] >= 3)
                        good &= hd
                        ok_all &= good
                        L.append('  s%d T%d %s %-9s/%-9s %s | %s' % (seed, t, 'man  ' if b == 'm' else 'woman', p, q, '; '.join(parts), 'ok' if good else 'MISS'))
    L.append('S1: %s' % ('PASS' if ok_all else 'FAIL'))
    L.append('')
    L.append('S2 the people colour on the figure (fingerprint_gallery --slice slice_shares.txt, idle D, seed 1): the dye slots and the')
    L.append('   cloth the signature dresses in the pair (layer Accent, cloak, crest Plume, shield Field, soft or T1 head), steps 1-4;')
    L.append('   want 5-30 % of the figure')
    sh = os.path.join(fdir, 'slice_shares.txt')
    if os.path.exists(sh):
        for ln in open(sh, encoding='utf-8'):
            q = ln.split()
            if len(q) < 10 or not q[1].endswith('_s1'):
                continue
            pct = float(q[-1].rstrip('%'))
            L.append('  %-28s area %4s  dye %3s  cloth %3s  %5.1f %%  %s' % (q[1], q[3], q[5], q[7], pct, 'ok' if 5 <= pct <= 30 else 'OUT'))
    L.append('')
    L.append('S6 the ladder: rpg_test --doll-envelope --scope ladder (C12f over source L: areas T2 >= 1.10, T3 >= 1.20, T4 >= 1.35, T5 >= 1.55,')
    L.append('   T6 >= 1.70 x T1, each tier 0.05 over the one below, heads +5 % and OD >= 0.10 a tier) and --scope all (the caps of every look:')
    L.append('   head rows, shell, cloak) on this build:')
    if env and os.path.exists(env):
        for ln in open(env, encoding='utf-8'):
            if any(k in ln for k in ('| Jade', '| Imperial', '| Fjordfolk', '| people', 'C12f source L', 'C12f caps')):
                L.append('  ' + ln.rstrip())
    L.append('')
    L.append('S3 ' + budget_line)
    open(os.path.join(out, 'measure.txt'), 'w', encoding='utf-8').write('\n'.join(L) + '\n')
    return ok_all


CAPTIONS = {
    'sheet_jade.png': 'JADE (flared, banded; vermilion on black lacquer). Left: real armour (jingasa, akaito-odoshi yoroi and '
                      'kabuto, hoshi kabuto) and the FLUX concepts (ideas only). Middle: our doll, T1 the black-lacquer jingasa with '
                      'a vermilion mon over a laced leather coat and the obi; T3 the kabuto, its shikoro and the lames laced in '
                      'vermilion, sode, the kusazuri bell, wide sleeves; T5 the crescent kabuto with gilt kuwagata, great sode, '
                      'the horo cloak and the jinbaori. Right: the kit icons (the jingasa, the kabuto, the kuwagata; the oval).',
    'sheet_imperial.png': 'IMPERIAL (rectangular, banded, the crest arc; crimson and gold). T1 the bronze knob cap '
                          '(a Montefortino: knob, neck lip, cheek guards) over a leather jerkin and the crimson focale, the plain '
                          'scutum; T3 the crested helm with its transverse crimson crest (a fan across the crown facing D, '
                          'edge-on facing R), banded shoulders, pteruges, the paludamentum over the right shoulder, the crimson '
                          'scutum with its gold rim; T5 the legate: the Attic helm with its gilt brow band and cheek guards under '
                          'the tall crimson crest, great banded shoulders, the long crimson cloak, the scutum on its gilt rim. '
                          'Icons: the knob cap, the transverse crest, the tall crest; the scutum.',
    'sheet_fjordfolk.png': 'FJORDFOLK (round, heavy-shouldered; woad and wool). T1 the round leather cap with its fleece brow, '
                           'bare forearms, the round shield bound in woad; T3 the spangenhelm with nasal, cheek plates and a mail '
                           'aventail, the fur roll at the neck, the split mail skirt, the woad cape, the quartered round shield; '
                           'T5 the jarl: the horned helm, its horns gilt and grown to the flare and over the crown (no comb), the '
                           'great pelt, the long woad cloak, the gilt rim. Icons: the fleece cap, the spangenhelm on its mail, '
                           'the gilt horns; the quartered round.',
    'tiers_doll_grey.png': 'The three peoples side by side at T1, T3 and T5 from the doll at 3x (man D R, woman D R): colour, '
                           'greyscale and the black silhouette fill. Each people is its outline before its colour.',
    'tiers_near.png': 'In play at NEAR (hidden Witness, tools/scripts/p5b_slice.txt): each tier a row of six (Jade man, woman; '
                      'Imperial man, woman; Fjordfolk man, woman), facing D then R; colour beside greyscale.',
    'before_after.png': 'Before (P5, EMB_FIX_RULES=8191: the people as a thin dye band) beside after (P5b: the people in the '
                        'head, the shoulders, the hem and the cloth). The same kits, man facing D, 4x.',
    'ladder.png': 'Each people, the whole ladder T1-T6, man then woman (the gallery kit sheet, 3x): Jade, Imperial, Fjordfolk. '
                  'The slice draws T1, T3 and T5; T2, T4 and T6 wear the people axes and colours on existing heads where the '
                  'planned head is not drawn yet (Fjordfolk T4: the cone with its spike over the aventail).',
    'close_1_t3_women_noon.png': 'Close, in play: the T3 women by noon (Jade, Imperial, Fjordfolk).',
    'close_2_t5_men_dusk.png': 'Close, in play: the T5 men at dusk (Jade, Imperial, Fjordfolk).',
}


def captions(out, names):
    L = ['P5b style slice (docs/STYLE_P5B.md 9): Jade, Imperial and Fjordfolk at T1, T3 and T5. The reference photographs and the',
         'FLUX concepts are thumbnails for direction only: every doll pixel is our own (hand-placed part views, recolours and',
         'existing parts through the doll pipeline). measure.txt holds S1, S2, S3 and S6.', '']
    for n in names:
        L.append('%s: %s' % (n, CAPTIONS.get(n, '')))
    L.append('measure.txt: the slice measures (S1 masks per pair and facing, the people pair share, the ladder areas, the pool).')
    open(os.path.join(out, 'captions.txt'), 'w', encoding='utf-8').write('\n'.join(L) + '\n')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--fig', required=True)
    ap.add_argument('--play', default='')
    ap.add_argument('--out', required=True)
    ap.add_argument('--ref', default=r'C:/Users/joshu/Documents/embervale-ref/armour')
    ap.add_argument('--budget', default='')
    ap.add_argument('--ladder', default='')
    ap.add_argument('--envelope', default='', help='rpg_test --doll-envelope --scope ladder (and --scope all) output')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    after, before = os.path.join(a.fig, 'after'), os.path.join(a.fig, 'before')
    made = []
    for p in PEOPLE:
        made.append(('sheet_%s.png' % p, people_sheet(p, after, a.ref, a.out)))
    made.append(('tiers_doll_grey.png', tiers_doll(after, a.out)))
    if os.path.isdir(before):
        made.append(('before_after.png', before_after(before, after, a.out)))
    if a.play:
        made += play_sheets(a.play, a.out)
    if a.ladder and os.path.exists(a.ladder):
        im = Image.open(a.ladder).convert('RGB')
        rows = [8, 4, 1]   # the gallery's kit sheet rows: jade, imperial, fjordfolk (cell 52 px at 3x, top 10 at 3x)
        lad = Image.new('RGB', (im.width, 156 * 3), BG)
        for i, r in enumerate(rows):
            lad.paste(im.crop((0, 30 + (r - 1) * 156, im.width, 30 + r * 156)), (0, 156 * i))
        lad.save(os.path.join(a.out, 'ladder.png'))
        made.append(('ladder.png', lad.size))
    ok = measure(after, a.out, a.budget, a.envelope)
    captions(a.out, [n for n, _ in made])
    for n, s in made:
        assert s[0] <= 8000 and s[1] <= 8000, n
        print('%-22s %dx%d' % (n, s[0], s[1]))
    print('measure.txt S1 %s' % ('PASS' if ok else 'FAIL'))


if __name__ == '__main__':
    main()
