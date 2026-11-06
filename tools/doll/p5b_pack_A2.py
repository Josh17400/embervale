#!/usr/bin/env python3
"""tools/doll/p5b_pack_A2.py (P5b production round 2, area A; the owner's rulings of 2026-11-06): the area's T5-T6 S1 measure and
its owner pack. Area A, round 2: the Highland and the Heartland at T5-T6 (the elite tiers), the slice peoples' T6 heads (the
Fjordfolk jarl, the Jade kuwagata, the Imperial legate), the Fjordfolk T4 spectacle helm's mail in courses.

Inputs (one build):
  fingerprint_gallery <all12> --slice --peoples <all twelve> --tiers 1,2,3,4,5,6 --seeds 1..3      the figures and icons
  fingerprint_gallery <figs> --figs tools/doll/p5b_A2_figs.txt                                     the Heartland's elite (its
                                                                                                    kingdoms' masterwork alloys)
  python tools/doll/p5b_pack_A2.py --all12 <dir> [--figs <dir>] [--out <pack dir>] [--ref <armour ref>]

S1 here: each area people at its new tiers against every other of the twelve at that tier, man and woman, facings D and R,
seeds 1-3, with the slice's thresholds (the figure's opaque mask XOR >= max(12, smaller/25); without the shield >= max(8,
smaller/40); the head >= max(6, smaller head/5) in one facing and 3 in the other). Writes measure.txt; with --out the owner
pack: sheet_<people>.png (references and concepts | T1-T6 man and woman D at 4x | T5-T6 D and R at 6x | the helm icons T1-T6,
colour and grey), and captions.txt.
"""
import argparse
import os
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import p5b_pack as P  # noqa: E402  (the slice's loaders and painters)
import p5b_pack_A as PA  # noqa: E402  (area A's names, pairs and references)

ALL12 = PA.ALL12
AREA = [('highland', [5, 6]), ('heartland', [5, 6]), ('fjordfolk', [6]), ('jade', [6]), ('imperial', [6])]
PEOPLE = ['highland', 'heartland', 'fjordfolk', 'jade', 'imperial']
CAPTION = {
    'highland': 'T5 the chief\'s kettle: a gilt finial and brim, the T3-T4 fur band, two eagle feathers behind a gilt clan brooch; '
                'T6 three feathers fanned from the clan badge on the crown\'s front, the brim wider and turned down. The body: '
                'the great fur mantle, the plaid half cape, the kilted strips, bare arms, the studded targe (gilt at T5)',
    'heartland': 'T5-T6 of the KIT are the hero\'s road, unchanged (the gilt onion, the starmetal crystal: D83, a relic metal keeps '
                 'its helm). The Heartland\'s elite on its kingdoms\' other T5-T6 helms (a masterwork alloy, steel): the great '
                 'helm under the knight\'s heraldry: a red and white torse, the red mantling with its white lining falling to '
                 'dagged ends, the red plume; T6 a crown on the torse, the plume taller, the mantling longer',
    'fjordfolk': 'T4 the spectacle helm (yours) with its mail in courses (the checker read as noise) and the fur mantle in place of '
                 'the spotted pelt; T6 the jarl\'s horns grown past T5\'s and curling in, a boar-crested ridge, a garnet',
    'jade': 'T6 the T5 kuwagata (yours) grown in the T6 kabuto legend view, the crest crown between them',
    'imperial': 'T6 the T5 Attic crest (yours) grown into a broad crimson fan, a laurel wreath in the legion\'s gold at the temples',
}


def s1(fdir):
    lines, ok_all, worst, n = [], True, {}, 0
    seeds = sorted({int(f.split('_s')[-1].split('_')[0]) for f in os.listdir(fdir) if f.startswith('slice_highland_T5_m_s') and f.endswith('_D.png')})
    for p, tiers in AREA:
        for t in tiers:
            for seed in seeds:
                for b in 'mw':
                    for q in ALL12:
                        if q == p:
                            continue
                        n += 1
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
                            heads.append((len(ha ^ hb), max(6, min(len(ha), len(hb)) / 5)))
                            good &= x1 >= n1 and x2 >= n2
                            parts.append('%s fig %d/%.0f ns %d/%.0f head %d/%.0f' % (f, x1, n1, x2, n2, heads[-1][0], heads[-1][1]))
                        good &= (heads[0][0] >= heads[0][1] and heads[1][0] >= 3) or (heads[1][0] >= heads[1][1] and heads[0][0] >= 3)
                        ok_all &= good
                        margin = min(heads[0][0] - heads[0][1], heads[1][0] - heads[1][1])
                        if (p, t) not in worst or margin < worst[(p, t)][0]:
                            worst[(p, t)] = (margin, q, b, seed)
                        if not good:
                            lines.append('  MISS s%d T%d %s %-9s/%-9s %s' % (seed, t, 'man  ' if b == 'm' else 'woman', p, q, '; '.join(parts)))
    out = ['S1 silhouette at the new tiers: each area people against the other eleven (seeds %s, man and woman, D and R)' % seeds]
    for (p, t), (m, q, b, seed) in sorted(worst.items()):
        out.append('  %-9s T%d: closest head %s (%s s%d): head XOR margin %+.0f px over the need' % (p, t, q, b, seed, m))
    out += lines
    out.append('S1: %s (%d pairs)' % ('PASS' if ok_all else 'FAIL', n))
    return ok_all, out


def figs_row(figs, names, k):
    ims = []
    for nm in names:
        path = os.path.join(figs, nm + '.png')
        if os.path.exists(path):
            ims.append((nm, P.up(P.load(path).crop((10, 4, 54, 62)), k)))
    return ims


def sheet(p, fdir, ref, out, figs=None):
    k4, k6 = 4, 6
    box = (10, 4, 54, 62)
    fw4, fh4 = (box[2] - box[0]) * k4, (box[3] - box[1]) * k4
    fw6, fh6 = (box[2] - box[0]) * k6, (box[3] - box[1]) * k6
    th = 130
    refs = [os.path.join(ref, p, r + '.jpg') for r in (P.REFS.get(p) or PA.P.REFS.get(p, []))]
    refs = [r for r in refs if os.path.exists(r)]
    gen = [os.path.join(ref, 'generated', p, '%s_%s.png' % (p, c)) for c in P.CONCEPTS]
    gen = [g for g in gen if os.path.exists(g)]
    left_w = 3 * 200 + 30
    mid_w = 6 * 2 * (fw4 + 6) + 20
    right_w = 4 * (fw6 + 8) + 20
    elite = figs_row(figs, ['hh5a1_D', 'hh5a1_R', 'hh6a1_D', 'hh6a1_R', 'hh5s2_D', 'hh6s3_D'], k6) if (p == 'heartland' and figs) else []
    W = left_w + mid_w + right_w
    H = max(80 + 2 * (fh4 + 40) + 3 * (16 * 5 + 40) + 40, 80 + 2 * (fh6 + 30) + (fh6 + 60 if elite else 0) + 40,
            80 + ((len(refs) + 2) // 3 + (len(gen) + 2) // 3) * (th + 6) + 60)
    s = Image.new('RGB', (W, H), P.BG)
    d = ImageDraw.Draw(s)
    P.text(d, (14, 8), '%s  P5b production round 2, area A: reference photographs and FLUX concepts (AI, ideas only: never traced) | '
                       'our doll T1-T6, man and woman, D at 4x | T5-T6 D and R at 6x | the helm icons' % P.NAMES[p])
    P.text(d, (14, 22), CAPTION[p][:250])
    if len(CAPTION[p]) > 250:
        P.text(d, (14, 34), CAPTION[p][250:500])
    dom, acc = P.PAIR[p]
    for i, c in enumerate((dom, acc)):
        d.rectangle([W - 120 + i * 40, 8, W - 120 + i * 40 + 30, 24], fill=((c >> 16) & 255, (c >> 8) & 255, c & 255), outline=P.DARK)
    y = 54
    P.text(d, (14, y), 'REFERENCE (real armour)'); y += 14
    for i, r in enumerate(refs):
        t = P.thumb(r, th)
        s.paste(t.crop((0, 0, min(t.width, 195), th)), (14 + (i % 3) * 200, y + (i // 3) * (th + 6)))
    y += ((len(refs) + 2) // 3) * (th + 6) + 8
    P.text(d, (14, y), 'FLUX CONCEPTS (ideas only)'); y += 14
    for i, g in enumerate(gen):
        t = P.thumb(g, th)
        cw = min(t.width, 195)
        s.paste(t.crop(((t.width - cw) // 2, 0, (t.width - cw) // 2 + cw, th)), (14 + (i % 3) * 200, y + (i // 3) * (th + 6)))
    # the middle: T1-T6, man and woman, facing D at 4x (colour, then greyscale)
    x0 = left_w
    for row, mode in enumerate(('colour', 'grey')):
        yy = 60 + row * (fh4 + 40)
        P.text(d, (x0, yy - 12), 'T1-T6, man | woman, D at 4x (%s)' % mode)
        for t in range(1, 7):
            for bi, b in enumerate('mw'):
                path = os.path.join(fdir, 'slice_%s_T%d_%s_s1_D.png' % (p, t, b))
                if not os.path.exists(path):
                    continue
                im = P.load(path).crop(box)
                if mode == 'grey':
                    im = P.grey(im)
                xx = x0 + ((t - 1) * 2 + bi) * (fw4 + 6)
                if bi == 0:
                    P.text(d, (xx + 2, yy + fh4 + 2), 'T%d' % t)
                s.paste(P.up(im, k4), (xx, yy), P.up(im, k4))
    # the helm icons T1-T6 (colour, grey) under the figures
    yy = 60 + 2 * (fh4 + 40) + 10
    P.text(d, (x0, yy - 12), 'the helm, body and shield icons T1-T6 at 5x (colour; grey under)')
    for pi, piece in enumerate(['helm', 'body', 'shield']):
        for t in range(1, 7):
            path = os.path.join(fdir, 'slice_icon_%s_T%d_%s_s1.png' % (p, t, piece))
            if not os.path.exists(path):
                continue
            ic = P.load(path)
            xx, y2 = x0 + (t - 1) * (16 * 5 + 30), yy + pi * (16 * 5 + 40)
            d.rectangle([xx - 2, y2 - 2, xx + 16 * 5 + 1, y2 + 16 * 5 + 1], fill=P.DARK)
            s.paste(P.up(ic, 5), (xx, y2), P.up(ic, 5))
            g = P.up(P.grey(ic), 2)
            s.paste(g, (xx + 16 * 5 + 2, y2 + 16 * 5 - 32), g)
    # the right: T5 and T6, D and R, man (seed 1) and woman (seed 2) at 6x
    x1 = left_w + mid_w
    for ti, t in enumerate((5, 6)):
        yy = 60 + ti * (fh6 + 30)
        P.text(d, (x1, yy - 12), 'T%d  man D, man R, woman D, woman R (6x)' % t)
        for ci, (b, f, seed) in enumerate([('m', 'D', 1), ('m', 'R', 1), ('w', 'D', 2), ('w', 'R', 2)]):
            path = os.path.join(fdir, 'slice_%s_T%d_%s_s%d_%s.png' % (p, t, b, seed, f))
            if not os.path.exists(path):
                continue
            im = P.up(P.load(path).crop(box), k6)
            xx = x1 + ci * (fw6 + 8)
            d.rectangle([xx - 2, yy - 2, xx + fw6 + 1, yy + fh6 + 1], outline=(96, 108, 78))
            s.paste(im, (xx, yy), im)
    if elite:
        yy = 60 + 2 * (fh6 + 30) + 14
        P.text(d, (x1, yy - 12), 'THE HEARTLAND ELITE on its kingdoms\' other T5-T6 helms (masterwork alloys): T5 D R, T6 D R, T5 woman s2, T6 s3')
        for ci, (nm, im) in enumerate(elite[:4]):
            xx = x1 + ci * (fw6 + 8)
            s.paste(im, (xx, yy), im)
    path = os.path.join(out, 'sheet_%s.png' % p)
    s.save(path)
    return path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--all12', required=True)
    ap.add_argument('--figs')
    ap.add_argument('--out')
    ap.add_argument('--ref', default=r'C:\Users\joshu\Documents\embervale-ref\armour')
    a = ap.parse_args()
    P.NAMES.update({'highland': 'HIGHLAND', 'heartland': 'HEARTLAND'})
    P.PAIR.update({'highland': (0x185030, 0xD89A28), 'heartland': (0xC02030, 0xEAE6D8)})
    P.REFS.update(PA.P.REFS)
    ok, lines = s1(a.all12)
    print('\n'.join(lines[:12] + lines[-1:]))
    if a.out:
        os.makedirs(a.out, exist_ok=True)
        open(os.path.join(a.out, 'measure.txt'), 'w').write('\n'.join(lines) + '\n')
        for p in PEOPLE:
            print('wrote', sheet(p, a.all12, a.ref, a.out, a.figs))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
