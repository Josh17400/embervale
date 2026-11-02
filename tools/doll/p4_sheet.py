#!/usr/bin/env python3
# tools/doll/p4_sheet.py: (P4 ART, 6e) the owner pack's sheets from tools/scripts/p4_art.txt's shots (witness_pack --keep):
# the hero cropped at NEAR (the doll's 3 screen px an art px), rows = looks, columns = the four facings, man then woman;
# BEFORE above AFTER when both runs are given. Nothing is drawn by this tool: crops, whole-number scales and labels only.
#   python tools/doll/p4_sheet.py OUT.png RUNDIR [RUNDIR_AFTER] [--rows floor,gambeson,...] [--scale N]
import argparse, os
from PIL import Image, ImageDraw

CX, CY, HW, TOP, BOT = 720, 365, 42, 78, 66   # the hero's centre at NEAR, the crop's half width and its rows above / below
FACES = ['d', 'r', 'l', 'u']


def crop(path, scale):
    im = Image.open(path).convert('RGB').crop((CX - HW, CY - TOP, CX + HW, CY + BOT))
    return im.resize((im.width * scale, im.height * scale), Image.NEAREST)


def sheet(runs, rows, scale, labels):
    w, h = 2 * HW * scale, (TOP + BOT) * scale
    pad, lab = 6, 150
    cols = len(FACES) * 2
    W = lab + cols * (w + pad) + pad
    blocks = []
    for run, name in zip(runs, labels):
        blocks.append((run, name))
    H = sum(len(rows) * (h + pad) + 28 for _ in blocks) + pad
    out = Image.new('RGB', (W, H), (24, 22, 26))
    d = ImageDraw.Draw(out)
    y = pad
    for run, name in blocks:
        d.text((pad, y + 4), name + '   (man: down right left up | woman: down right left up)', fill=(240, 230, 200))
        y += 22
        for r in rows:
            d.text((pad, y + h // 2), r, fill=(230, 220, 200))
            x = lab
            for who in 'mw':
                for f in FACES:
                    p = os.path.join(run, f'p4_art_{who}_{r}_{f}.png')
                    if os.path.exists(p): out.paste(crop(p, scale), (x, y))
                    else: d.rectangle((x, y, x + w, y + h), fill=(160, 20, 20))
                    x += w + pad
            y += h + pad
        y += 6
    return out


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('out'); ap.add_argument('runs', nargs='+')
    ap.add_argument('--rows', default='floor,gambeson,jerkin,heart,brig,set')
    ap.add_argument('--scale', type=int, default=2)
    a = ap.parse_args()
    labels = ['BEFORE (25d47da)', 'AFTER (P4 ART)'][:len(a.runs)] if len(a.runs) > 1 else ['shots']
    sheet(a.runs, a.rows.split(','), a.scale, labels).save(a.out)
    print(a.out)
