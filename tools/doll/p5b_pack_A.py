#!/usr/bin/env python3
"""tools/doll/p5b_pack_A.py (P5b production, area A, docs/STYLE_P5B.md 9.3 restated for production): the area's slice gates
and its owner pack. Area A: the Highland and the Heartland at T1-T4, the Fjordfolk T4 fix (the spectacle helm) and the slice
peoples' planned T2 heads (the Imperial's Montefortino tuft; the Jade's great kasa was over T2's head envelope).

Inputs (one build):
  fingerprint_gallery <all12> --slice --peoples <all twelve> --tiers 1,2,3,4 --seeds 1..3      the S1 figures
  EMB_FIX_RULES=8191 fingerprint_gallery <before> --slice --peoples <area> --tiers 1,2,3,4      P5's look (before | after)
  fingerprint_gallery <figs> --figs <file>                                                     extra looks (the chapel, heads)

  python tools/doll/p5b_pack_A.py --all12 <dir> [--before <dir>] [--figs <dir>] [--out <pack dir>] [--ref <armour ref>]
                                  [--env <rpg_test --doll-envelope --scope ladder --report output>]

S1 here: each area people against every other of the twelve at its tier (T1-T4), man and woman, facings D and R, seeds 1-3,
with the slice's thresholds (the figure's opaque mask XOR >= max(12, smaller/25); without the shield >= max(8, smaller/40);
the head >= max(6, smaller head/5) in one facing and 3 in the other). Writes measure.txt; with --out the owner pack:
sheet_<people>.png (references and concepts | T1-T4 man and woman D and R at 6x | the icons at 6x), fjordfolk_heads.png (the
T3 / T4 / T5 head at 8x, before and after the owner's fix), and captions.txt.
"""
import argparse
import os
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import p5b_pack as P  # noqa: E402  (the slice's pack: its loaders and sheet painter, driven with this area's peoples)

ALL12 = ['fjordfolk', 'highland', 'heartland', 'imperial', 'dune', 'steppe', 'marsh', 'jade', 'river', 'suntemple', 'sylvan',
         'starspire']
# the area's (people, tiers) under S1: the two whole peoples and the slice peoples' heads this area drew
AREA = [('highland', [1, 2, 3, 4]), ('heartland', [1, 2, 3, 4]), ('fjordfolk', [4]), ('imperial', [2])]
TIERS = [1, 2, 3, 4]
P.NAMES.update({'highland': 'HIGHLAND', 'heartland': 'HEARTLAND'})
P.PAIR.update({'highland': (0x185030, 0xD89A28), 'heartland': (0xC02030, 0xEAE6D8)})   # rpg/core/fingerprint.h
P.REFS.update({
    'highland': ['highland_gallowglass_d_rer', 'highland_three_irish_kerns_albert_durer_1521', 'highland_charles_edward_stuart_s_targe',
                 'highland_davidson_tartan_modern_tiling', 'highland_42nd_black_watch_officer_by_edward_dayes_c_1791_93',
                 'highland_medieval_warriors_at_edinburgh_castle_geograph_org_uk_424728'],
    'heartland': ['heartland_chapel_de_fer_kettle_helmet_or_war_hat_snowshill_manor_nt_13', 'heartland_elbl_g_kapalin_helmet_xv_c',
                  'heartland_1350_topfhelm_des_hans_rieter_zu_kornburg_anagoria', 'heartland_bataille_de_courtrai_1302'],
})


def s1(fdir):
    lines, ok_all, worst = [], True, {}
    seeds = sorted({int(n.split('_s')[-1].split('_')[0]) for n in os.listdir(fdir) if n.startswith('slice_highland_T1_m_s') and n.endswith('_D.png')})
    for p, tiers in AREA:
        for t in tiers:
            for seed in seeds:
                for b in 'mw':
                    for q in ALL12:
                        if q == p:
                            continue
                        good, heads, parts = True, [], []
                        for f in 'DR':
                            A, B = P.whole(fdir, p, t, b, f, '', seed), P.whole(fdir, q, t, b, f, '', seed)
                            An, Bn = P.whole(fdir, p, t, b, f, '_ns', seed), P.whole(fdir, q, t, b, f, '_ns', seed)
                            Ah, Bh = P.whole(fdir, p, t, b, f, '_head', seed), P.whole(fdir, q, t, b, f, '_head', seed)
                            ma, mb = P.mask(A), P.mask(B)
                            x1, n1 = len(ma ^ mb), max(12, min(len(ma), len(mb)) / 25)
                            na, nb = P.mask(An), P.mask(Bn)
                            x2, n2 = len(na ^ nb), max(8, min(len(na), len(nb)) / 40)
                            ha, hb = P.mask(Ah), P.mask(Bh)
                            x3 = len(ha ^ hb)
                            heads.append((x3, max(6, min(len(ha), len(hb)) / 5)))
                            good &= x1 >= n1 and x2 >= n2
                            parts.append('%s fig %d/%.0f ns %d/%.0f head %d/%.0f' % (f, x1, n1, x2, n2, x3, heads[-1][1]))
                        good &= (heads[0][0] >= heads[0][1] and heads[1][0] >= 3) or (heads[1][0] >= heads[1][1] and heads[0][0] >= 3)
                        ok_all &= good
                        key = (p, t)
                        margin = min(heads[0][0] - heads[0][1], heads[1][0] - heads[1][1])
                        if key not in worst or margin < worst[key][0]:
                            worst[key] = (margin, q, b, seed)
                        if not good:
                            lines.append('  MISS s%d T%d %s %-9s/%-9s %s' % (seed, t, 'man  ' if b == 'm' else 'woman', p, q, '; '.join(parts)))
    out = ['S1 silhouette: each area people against the other eleven at its tier (seeds %s, man and woman, D and R)' % seeds]
    for (p, t), (m, q, b, seed) in sorted(worst.items()):
        out.append('  %-9s T%d: closest head %s (%s s%d): head XOR margin %+.0f px over the need' % (p, t, q, b, seed, m))
    out += lines
    out.append('S1: %s (%d pairs)' % ('PASS' if ok_all else 'FAIL', sum(len(t) for _, t in AREA) * len(seeds) * 2 * 11))
    return ok_all, out


def retitle(path, title):
    im = Image.open(path).convert('RGB')
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, im.width, 22], fill=P.BG)
    P.text(d, (14, 10), title)
    im.save(path)


def sheet(p, fdir, ref, out, extra=None):
    """the slice's people sheet at T1-T4 (its painter, this area's tiers); extra: (label, [figure PNG paths]) appended below"""
    P.TIERS = TIERS
    P.people_sheet(p, fdir, ref, out)
    path = os.path.join(out, 'sheet_%s.png' % p)
    retitle(path, '%s  P5b production, area A: reference (photographs) and FLUX concepts (AI, ideas only: never traced) | our doll '
                  'T1-T4, man and woman, D and R at 6x | the kit icons at 6x (grey under each)' % P.NAMES[p])
    if extra:
        label, figs = extra
        im = Image.open(path).convert('RGB')
        k = 6
        crop = P.FIG_BOX
        fw, fh = (crop[2] - crop[0]) * k, (crop[3] - crop[1]) * k
        x0 = 3 * 230 + 40
        out_im = Image.new('RGB', (im.width, im.height + fh + 50), P.BG)
        out_im.paste(im, (0, 0))
        d = ImageDraw.Draw(out_im)
        y = im.height + 10
        P.text(d, (14, y + fh // 2), label)
        for i, f in enumerate(figs):
            if not os.path.exists(f):
                continue
            g = P.up(P.load(f).crop(crop), k)
            xx = x0 + i * (fw + 12)
            d.rectangle([xx - 2, y - 2, xx + fw + 1, y + fh + 1], outline=(96, 108, 78))
            out_im.paste(g, (xx, y), g)
        out_im.save(path)


def heads_sheet(fdir, figs, out):
    """the slice peoples' heads this area drew: the Fjordfolk T4 spectacle helm, the Imperial T2 Montefortino (and the Jade T2, the
    slice's own soft cap: the great kasa is over T2's 3-row head), man and woman D and R at 6x beside T1-T3 for the climb, icons"""
    k = 6
    crop = P.FIG_BOX
    fw, fh = (crop[2] - crop[0]) * k, (crop[3] - crop[1]) * k
    rows = [('fjordfolk', [3, 4, 5]), ('imperial', [1, 2, 3]), ('jade', [1, 2, 3])]
    W = 140 + 3 * 4 * (fw + 8) + 40 + 3 * (16 * k + 12)
    H = 40 + len(rows) * (fh + 40)
    sh = Image.new('RGB', (W, H), P.BG)
    d = ImageDraw.Draw(sh)
    P.text(d, (10, 8), 'The slice peoples, the heads area A drew: FJORDFOLK T4 the spectacle helm (between the T3 spangenhelm and the T5 horns); '
                       'IMPERIAL T2 the Montefortino tuft (the T1 knob cap grown); JADE T2 the slice cap. 6x, man D R, woman D R')
    for ri, (p, tiers) in enumerate(rows):
        y = 34 + ri * (fh + 40)
        P.text(d, (10, y + fh // 2), P.NAMES.get(p, p.upper()))
        for ti, t in enumerate(tiers):
            for ci, (b, f) in enumerate([('m', 'D'), ('m', 'R'), ('w', 'D'), ('w', 'R')]):
                path = os.path.join(fdir, 'slice_%s_T%d_%s_s1_%s.png' % (p, t, b, f))
                if not os.path.exists(path):
                    path = os.path.join(figs or '', 'slice_%s_T%d_%s_s1_%s.png' % (p, t, b, f))
                if not os.path.exists(path):
                    continue
                g = P.up(P.load(path).crop(crop), k)
                xx = 140 + ti * 4 * (fw + 8) + ci * (fw + 8)
                if ci == 0:
                    P.text(d, (xx, y - 12), 'T%d' % t)
                sh.paste(g, (xx, y), g)
            ip = os.path.join(fdir, 'slice_icon_%s_T%d_helm_s1.png' % (p, t))
            if not os.path.exists(ip) and figs:
                ip = os.path.join(figs, 'slice_icon_%s_T%d_helm_s1.png' % (p, t))
            if os.path.exists(ip):
                ic = P.up(P.load(ip), k)
                xx = 140 + 3 * 4 * (fw + 8) + 20 + ti * (16 * k + 12)
                d.rectangle([xx - 2, y - 2, xx + 16 * k + 1, y + 16 * k + 1], fill=P.DARK)
                sh.paste(ic, (xx, y), ic)
    sh.save(out)


def near_sheet(play, out):
    """the in-play rows at NEAR (hidden Witness, tools/scripts/p5b_A.txt): each row D over R, colour beside greyscale"""
    import glob
    shots = {os.path.basename(f): f for f in glob.glob(os.path.join(play, '**', 'p5b_A_*.png'), recursive=True)}
    names = ['t1', 't2', 't3', 't4', 'chapel_t2']
    crops = []
    for n in names:
        for f in 'dr':
            path = shots.get('p5b_A_%s_%s.png' % (n, f))
            if not path:
                continue
            im = Image.open(path).convert('RGB')
            # the row of six stands at the top of the frame: its box from the frame's own pixels (the shot's top 260 rows)
            crops.append((n, f, im.crop((400, 30, 1060, 230))))
    if not crops:
        return
    cw, chh = crops[0][2].width, crops[0][2].height
    sh = Image.new('RGB', (2 * cw + 30, len(crops) * (chh + 18) + 30), P.BG)
    d = ImageDraw.Draw(sh)
    P.text(d, (10, 8), 'In play at NEAR (hidden Witness, tools/scripts/p5b_A.txt): Highland man woman | Heartland man woman | Fjordfolk man woman; '
                       'last rows: the Heartland chapel, the Imperial T2, the Jade T2. Colour | greyscale')
    for i, (n, f, im) in enumerate(crops):
        y = 26 + i * (chh + 18)
        P.text(d, (10, y), '%s facing %s' % (n.upper(), f.upper()))
        sh.paste(im, (10, y + 12))
        sh.paste(im.convert('L').convert('RGB'), (20 + cw, y + 12))
    sh.save(out)
    for n in ('close_1_fjordfolk_t345', 'close_2_highland_dusk'):
        path = shots.get('p5b_A_%s.png' % n)
        if path:
            Image.open(path).save(os.path.join(os.path.dirname(out), '%s.png' % n))


def fjord_heads(after, before, out):
    """the Fjordfolk T3 / T4 / T5 heads at 8x, man and woman, D and R: before (the slice's T4 cone) and after (the spectacle helm)"""
    k = 8
    crop = (14, 18, 50, 48)
    rows = []
    for label, d in (('BEFORE (the slice: T4 the borrowed spiked cone)', before), ('AFTER (T4 the rounded spectacle helm)', after)):
        ims = []
        for b in 'mw':
            for t in (3, 4, 5):
                for f in 'DR':
                    path = os.path.join(d, 'fj%d%s_%s.png' % (t, b, f))
                    ims.append((('T%d %s %s' % (t, 'man' if b == 'm' else 'woman', f)), P.load(path).crop(crop) if os.path.exists(path) else None))
        rows.append((label, ims))
    cw, ch = (crop[2] - crop[0]) * k, (crop[3] - crop[1]) * k
    W = 12 * (cw + 10) + 20
    H = len(rows) * (ch + 50) + 40
    sh = Image.new('RGBA', (W, H), P.BG + (255,))
    d = ImageDraw.Draw(sh)
    P.text(d, (10, 8), 'FJORDFOLK heads T3 | T4 | T5 at 8x (man, woman; D and R): the owner fix "overly pointy and his eyes are too far apart"')
    y = 30
    for label, ims in rows:
        P.text(d, (10, y), label)
        for i, (lab, im) in enumerate(ims):
            x = 10 + i * (cw + 10)
            P.text(d, (x, y + 14), lab)
            if im:
                sh.alpha_composite(P.up(im, k), (x, y + 28))
        y += ch + 50
    sh.save(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--all12', required=True)
    ap.add_argument('--before')
    ap.add_argument('--figs')
    ap.add_argument('--fjbefore')
    ap.add_argument('--out')
    ap.add_argument('--env')
    ap.add_argument('--play')
    ap.add_argument('--ref', default=r'C:\Users\joshu\Documents\embervale-ref\armour')
    a = ap.parse_args()
    ok, lines = s1(a.all12)
    if a.env and os.path.exists(a.env):
        lines.append('')
        lines.append('S6 the ladder (rpg_test --doll-envelope --scope ladder --seeds 1..3 --report):')
        for ln in open(a.env, encoding='utf-8'):
            if any(k in ln for k in ('| Highland', '| Heartland', '| Fjordfolk', '| Imperial', '| Jade', '| people', 'C12f source L')):
                lines.append('  ' + ln.rstrip())
    dest = a.out or '.'
    os.makedirs(dest, exist_ok=True)
    open(os.path.join(dest, 'measure.txt'), 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
    print('\n'.join(lines[:12]))
    if a.out:
        sheet('highland', a.all12, a.ref, a.out)
        chapel = [os.path.join(a.figs or '', 'ch1%s_%s.png' % (b, f)) for b in 'mw' for f in 'DR']
        sheet('heartland', a.all12, a.ref, a.out, ('T1 chapel: a Heartland-made T1 helm (the hero start road stays bareheaded)', chapel))
        heads_sheet(a.all12, a.figs, os.path.join(a.out, 'sheet_slice_heads.png'))
        if a.figs and a.fjbefore:
            fjord_heads(a.figs, a.fjbefore, os.path.join(a.out, 'fjordfolk_heads.png'))
        if a.play:
            near_sheet(a.play, os.path.join(a.out, 'tiers_near.png'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
