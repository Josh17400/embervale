#!/usr/bin/env python3
"""tools/doll/p5b_pack_B2.py (P5b production 2, area B: the Dune, Steppe and Marsh elite at T5-T6): the owner's sheets.

Inputs (one build or one pool):
  fingerprint_gallery <dir> --slice --peoples <all twelve> --tiers 1,2,3,4,5,6 --seeds 1..3 [--pool P]

  python tools/doll/p5b_pack_B2.py --fig <dir> --out <pack dir> [--ref <embervale-ref/armour>]

Writes (nearest-neighbour scaling, never a filter on our pixels; every PNG at most 8000 px a side):
  sheet_<people>.png   references and FLUX concepts (ideas only) | T1-T6 man and woman facing D at 4x (the ladder) | T5-T6 man and
                       woman, D and R at 6x | the helm, body, shield and cloak icons T1-T6 at 4x (greyscale beside)
  elite_all12.png      T5 and T6, man D and R, all twelve peoples at 3x: colour, greyscale, the black silhouette fill
"""
import argparse
import os

from PIL import Image, ImageDraw

ALL = ['dune', 'steppe', 'marsh', 'fjordfolk', 'highland', 'heartland', 'imperial', 'jade', 'river', 'suntemple', 'sylvan',
       'starspire']
MINE = ['dune', 'steppe', 'marsh']
NAMES = {'dune': 'DUNE', 'steppe': 'STEPPE', 'marsh': 'MARSH'}
PAIR = {'dune': (0x18849E, 0xECE2C8), 'steppe': (0x9C3400, 0x3CACD6), 'marsh': (0x707E28, 0xD6CCA8)}   # rpg/core/fingerprint.h
ELITE = {'dune': 'the amir (T5): the gilt-spiked turban helm, its falcon plume, the mail aventail; the sultan (T6): the gilt crescent '
                 'on the onion dome, the turban girt in gilt, the gilt-hemmed aventail',
         'steppe': 'the khan (T5): the tall gilt-ribbed cone in its rust horsetail, the fur roll at brow and neck, the lamellar laced '
                   'in sky silk; the khan of khans (T6): the taller spire, the great fur crown and collar',
         'marsh': 'the bog king (T5): the bulrush crown over the widest reed brim, its grass at the ends, the grass collar strung '
                  'with bone; crowned in bone (T6): the antlers, the bone brow, the skull at the collar'}
REFS = {
    'dune': ['dune_18th_century_sikh_kulah_khud_helmet_inside_the_white_tower_a', 'dune_helmet_khula_khud_met_02_5_5_001mar2015',
             'dune_13_mameluke_of_egypt', 'dune_armour_of_the_ottoman_empire_zirah_baktar_or_zirh_gomlek'],
    'steppe': ['steppe_closeup_of_mongolian_reenactors_at_khaan_quest_07', 'steppe_bronze_helmet_mongol_empire_19219793614',
               'steppe_a_man_and_woman_in_traditional_mongolian_dress_look_on_as_se', 'steppe_pursuit_by_mongol_warriors_diez_albums'],
    'marsh': ['marsh_01973_tollundmanden', 'marsh_coconut_fiber_armor_from_the_republic_of_kiribati_micronesia',
              'marsh_ainu_group_aborigines_of_japan_from_the_island_of_hokkaido_o', 'marsh_ancient_celt_with_carnyx_ancient_briton'],
}
CONCEPTS = ['full_elite_man', 'full_elite_woman', 'helmets_1', 'helmets_2', 'full_veteran_man', 'pixel_2']
BG, INK, DARK = (118, 132, 96), (236, 228, 206), (36, 32, 40)
BOX = (10, 6, 54, 62)   # the figure window inside the 64x64 gallery canvas (T6 crests to the canvas row 8)


def load(path):
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
    out = Image.new('RGBA', im.size, (24, 20, 28, 255))
    out.putalpha(im.split()[3])
    return out


def up(im, k):
    return im.resize((im.width * k, im.height * k), Image.NEAREST)


def fig(d, p, t, b, f, seed=1):
    path = os.path.join(d, 'slice_%s_T%d_%s_s%d_%s.png' % (p, t, b, seed, f))
    return load(path).crop(BOX) if os.path.exists(path) else None


def thumb(path, h):
    im = Image.open(path).convert('RGB')
    return im.resize((max(1, int(im.width * h / im.height)), h), Image.LANCZOS)   # (a photograph or a concept: never our pixels)


def people_sheet(p, fdir, ref, out):
    fw, fh = BOX[2] - BOX[0], BOX[3] - BOX[1]
    th, left = 170, 3 * 240 + 30
    refs = [r for r in (os.path.join(ref, p, x + '.jpg') for x in REFS[p]) if os.path.exists(r)]
    gens = [g for g in (os.path.join(ref, 'generated', p, '%s_%s.png' % (p, c)) for c in CONCEPTS) if os.path.exists(g)]
    lad_w = 6 * 2 * (fw * 4 + 8)
    el_w = 4 * (fw * 6 + 10) * 2
    ic_w = 4 * (16 * 4 + 12) + 4 * 36
    W = left + max(lad_w, el_w, ic_w) + 40
    H = 80 + (fh * 4 + 40) + 2 * (fh * 6 + 40) + 6 * (16 * 4 + 14) + 80
    sheet = Image.new('RGB', (W, max(H, 80 + ((len(refs) + 2) // 3 + (len(gens) + 2) // 3) * (th + 8) + 60)), BG)
    d = ImageDraw.Draw(sheet)
    d.text((14, 10), '%s  P5b production 2 (area B): reference photographs and FLUX concepts (AI, ideas only: never traced) | our doll: '
                     'T1-T6 man and woman facing D at 4x (the ladder) | the elite T5-T6, D and R at 6x | the icons T1-T6 at 4x '
                     '(greyscale beside)' % NAMES[p], fill=INK)
    d.text((14, 26), ELITE[p], fill=INK)
    dom, acc = PAIR[p]
    for i, c in enumerate((dom, acc)):
        d.rectangle([14 + i * 40, 44, 44 + i * 40, 60], fill=((c >> 16) & 255, (c >> 8) & 255, c & 255), outline=DARK)
    d.text((100, 46), 'dominant #%06X  accent #%06X' % (dom, acc), fill=INK)
    y = 76
    d.text((14, y), 'REFERENCE (real armour)', fill=INK)
    y += 14
    for i, r in enumerate(refs):
        t = thumb(r, th)
        sheet.paste(t.crop((0, 0, min(t.width, 235), th)), (14 + (i % 3) * 240, y + (i // 3) * (th + 8)))
    y += ((len(refs) + 2) // 3) * (th + 8) + 10
    d.text((14, y), 'FLUX CONCEPTS (ideas only)', fill=INK)
    y += 14
    for i, g in enumerate(gens):
        t = thumb(g, th)
        cw = min(t.width, 235)
        sheet.paste(t.crop(((t.width - cw) // 2, 0, (t.width - cw) // 2 + cw, th)), (14 + (i % 3) * 240, y + (i // 3) * (th + 8)))
    x0, y0 = left, 76
    d.text((x0, y0), 'THE LADDER T1-T6 (man D | woman D), 4x', fill=INK)
    for t in range(1, 7):
        for bi, b in enumerate('mw'):
            im = fig(fdir, p, t, b, 'D')
            if im is None:
                continue
            xx = x0 + ((t - 1) * 2 + bi) * (fw * 4 + 8)
            if bi == 0:
                d.text((xx + 4, y0 + 16), 'T%d' % t, fill=INK)
            sheet.paste(up(im, 4), (xx, y0 + 30), up(im, 4))
    y0 += fh * 4 + 44
    for t in (5, 6):
        d.text((x0, y0), 'T%d ELITE at 6x: man D, man R, woman D, woman R | the same in greyscale' % t, fill=INK)
        for ci, (b, f) in enumerate([('m', 'D'), ('m', 'R'), ('w', 'D'), ('w', 'R')]):
            im = fig(fdir, p, t, b, f)
            if im is None:
                continue
            xx = x0 + ci * (fw * 6 + 10)
            sheet.paste(up(im, 6), (xx, y0 + 16), up(im, 6))
            g = up(grey(im), 6)
            sheet.paste(g, (xx + 4 * (fw * 6 + 10) + 20, y0 + 16), g)
        y0 += fh * 6 + 30
    d.text((x0, y0), 'ICONS T1-T6: helm, body, shield, cloak (4x) | greyscale (2x)', fill=INK)
    y0 += 16
    for t in range(1, 7):
        d.text((x0, y0 + 26), 'T%d' % t, fill=INK)
        for pi, piece in enumerate(['helm', 'body', 'shield', 'cloak']):
            path = os.path.join(fdir, 'slice_icon_%s_T%d_%s_s1.png' % (p, t, piece))
            if not os.path.exists(path):
                continue
            ic = load(path)
            xx = x0 + 30 + pi * (16 * 4 + 12 + 36)
            d.rectangle([xx - 2, y0 - 2, xx + 16 * 4 + 1, y0 + 16 * 4 + 1], fill=DARK)
            sheet.paste(up(ic, 4), (xx, y0), up(ic, 4))
            sheet.paste(up(grey(ic), 2), (xx + 16 * 4 + 6, y0 + 32), up(grey(ic), 2))
        y0 += 16 * 4 + 12
    sheet.save(os.path.join(out, 'sheet_%s.png' % p))
    return sheet.size


def all12(fdir, out):
    fw, fh = BOX[2] - BOX[0], BOX[3] - BOX[1]
    k = 3
    rows = [(t, b, f, mode) for t in (5, 6) for mode in ('colour', 'grey', 'black') for b, f in (('m', 'D'), ('m', 'R'))]
    W = 110 + len(ALL) * (fw * k + 6)
    H = 30 + len(rows) * (fh * k + 6)
    sheet = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(sheet)
    for i, p in enumerate(ALL):
        d.text((110 + i * (fw * k + 6) + 4, 8), p.upper(), fill=INK)
    for ri, (t, b, f, mode) in enumerate(rows):
        yy = 26 + ri * (fh * k + 6)
        d.text((6, yy + fh * k // 2), 'T%d %s %s' % (t, f, mode), fill=INK)
        for i, p in enumerate(ALL):
            im = fig(fdir, p, t, b, f)
            if im is None:
                continue
            im = {'colour': im, 'grey': grey(im), 'black': black(im)}[mode]
            sheet.paste(up(im, k), (110 + i * (fw * k + 6), yy), up(im, k))
    sheet.save(os.path.join(out, 'elite_all12.png'))
    return sheet.size


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--fig', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--ref', default=r'C:\Users\joshu\Documents\embervale-ref\armour')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    for p in MINE:
        print('sheet_%s.png' % p, people_sheet(p, a.fig, a.ref, a.out))
    print('elite_all12.png', all12(a.fig, a.out))
