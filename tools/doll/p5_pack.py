#!/usr/bin/env python3
# tools/doll/p5_pack.py: (P5 PACK, docs/FIX_BATCH_P5.md 5.6, 10 play/) the owner pack's in-play half from two runs of
# tools/scripts/p5_fingerprint.txt (witness_pack --keep): "before" (the same build with EMB_FIX_RULES=4095, or 80215d7's)
# and "after". For every shot both runs hold, it writes into OUT:
#   <shot>.png        BEFORE | AFTER side by side, whole frames, labelled (<= 8000 px a side)
#   <shot>_zoom.png   BEFORE | AFTER of the region the two differ in (the figures), at a whole-number scale 4x-8x
#                     (nearest neighbour), so the pixel art reads at a glance; "no change" frames get none
#   grey/<name>.png   a greyscale copy of each of the above (the marks and the two-tone bands must still part the peoples)
#   captions.txt      one line per image: "<file>  <caption>"; the PENDING header (section 10: nothing ships as final
#                     before the owner has seen it)
# Nothing is drawn by this tool: crops, whole-number scales, labels and greyscale only. Exit 1 when a run is missing or
# no shot pairs up; prints one PASS/FAIL line per check.
#   python tools/doll/p5_pack.py BEFORE_DIR AFTER_DIR OUT_DIR
# BEFORE_DIR / AFTER_DIR: a witness_pack --out dir (its runs/p5_fingerprint/ is used) or the PNG folder itself.
import os
import re
import sys

from PIL import Image, ImageChops, ImageDraw

MAX_SIDE = 8000
BG, INK, DIM = (24, 22, 26), (240, 230, 200), (150, 140, 120)
LAB = 30   # the label band's height

# the caption of a shot, by its name (p5_fp_<what>_<zoom>.png); first match wins
CAPTIONS = [
    (r'start_watch', "seed 7's start town (a River town): its watch, four of them, River teal and white on every piece"),
    (r'dune_watch', "a Dune town's watch: indigo and saffron bands, the arabesque on the chest"),
    (r'jade_watch', "a Jade town's watch: vermilion and black lacquer, the cloud scrolls"),
    (r'highland_watch', "a Highland town's watch: tartan green and red, the plaid square"),
    (r'suntemple_watch', "a Sun Temple town's watch: magenta and teal, the sun rays"),
    (r'comp', "a companion given three peoples' pieces: a Fjordfolk mail shirt, a Steppe leather cap, Marsh leather boots"),
    (r'hero_dune', "the hero in a Dune T1 leather coat (the padded coat): indigo and saffron laces, the material still brown leather"),
    (r'hero_highland', "the hero in a Highland T1 leather coat: tartan green and red laces"),
    (r'hero_jade', "the hero in a Jade T1 leather coat: vermilion and black laces"),
    (r'shelf_start', "the start town's smith (River): the shelf's armour icons carry the River dye"),
    (r'shelf_dune', "a Dune smith's shelf: the armour icons in indigo and saffron"),
    (r'shelf_highland', "a Highland smith's shelf: the armour icons in green and red"),
    (r'equip', "the EQUIP page: a Dune coat, a Highland cap, Jade gloves and boots; the list's coats of three peoples"),
    (r'night_lamp', "night under a Jade town lamp (23:00): the hero's three peoples' bands still read in lamp light"),
]
ZOOMS = {'near': 'NEAR', 'close': 'CLOSE'}


def caption(name):
    base = name[:-4] if name.endswith('.png') else name
    zoom = next((v for k, v in ZOOMS.items() if base.endswith('_' + k)), '')
    for pat, text in CAPTIONS:
        if re.search(pat, base):
            return text + (f' ({zoom})' if zoom else '')
    return base


def shots_dir(d):
    sub = os.path.join(d, 'runs', 'p5_fingerprint')
    return sub if os.path.isdir(sub) else d


def labelled(pair, labels, scale=1):
    """two same-size images side by side, each under a label band"""
    a, b = pair
    w, h = a.size
    pad = 8
    out = Image.new('RGB', (2 * w + 3 * pad, h + LAB + 2 * pad), BG)
    d = ImageDraw.Draw(out)
    for i, (im, lab) in enumerate(zip((a, b), labels)):
        x = pad + i * (w + pad)
        d.text((x + 4, pad + 8), lab + (f'  {scale}x' if scale > 1 else ''), fill=INK)
        out.paste(im, (x, pad + LAB))
    return out


def diff_box(a, b):
    """the bounding box of the pixels the two frames differ in (None: identical)"""
    return ImageChops.difference(a, b).convert('L').point(lambda v: 255 if v > 0 else 0).getbbox()


def zoom_pair(a, b, box):
    """the differing region (padded, cut to fit) of both, at a whole-number scale 4x-8x"""
    x0, y0, x1, y1 = box
    padpx = 12
    x0, y0 = max(0, x0 - padpx), max(0, y0 - padpx)
    x1, y1 = min(a.width, x1 + padpx), min(a.height, y1 + padpx)
    # the two crops side by side must fit MAX_SIDE at 4x: at most (MAX_SIDE - margins) / 8 px wide a crop, centred
    maxw, maxh = (MAX_SIDE - 64) // 8, (MAX_SIDE - 64) // 4
    if x1 - x0 > maxw:
        c = (x0 + x1) // 2
        x0, x1 = c - maxw // 2, c - maxw // 2 + maxw
    if y1 - y0 > maxh:
        c = (y0 + y1) // 2
        y0, y1 = c - maxh // 2, c - maxh // 2 + maxh
    w, h = x1 - x0, y1 - y0
    scale = max(4, min(8, 1600 // max(w, h, 1)))
    while scale > 4 and (2 * w * scale + 24 > MAX_SIDE or h * scale + LAB + 16 > MAX_SIDE):
        scale -= 1
    up = lambda im: im.crop((x0, y0, x1, y1)).resize((w * scale, h * scale), Image.NEAREST)
    return (up(a), up(b)), scale, (x0, y0, x1, y1)


def main(argv):
    if len(argv) != 4:
        print('usage: python tools/doll/p5_pack.py BEFORE_DIR AFTER_DIR OUT_DIR')
        return 2
    bdir, adir, out = shots_dir(argv[1]), shots_dir(argv[2]), argv[3]
    ok = True
    for lab, d in (('before', bdir), ('after', adir)):
        good = os.path.isdir(d) and any(f.endswith('.png') for f in os.listdir(d))
        print(f'{"PASS" if good else "FAIL"} p5_pack {lab}: {d}')
        ok &= good
    if not ok:
        return 1
    names = sorted(f for f in os.listdir(adir) if f.endswith('.png') and os.path.exists(os.path.join(bdir, f)))
    only = sorted(set(f for f in os.listdir(adir) if f.endswith('.png')) ^ set(f for f in os.listdir(bdir) if f.endswith('.png')))
    print(f'{"PASS" if names else "FAIL"} p5_pack pairs: {len(names)} shots in both' + (f'; in one run only: {" ".join(only)}' if only else ''))
    if not names:
        return 1
    os.makedirs(os.path.join(out, 'grey'), exist_ok=True)
    caps = ['# PENDING THE OWNER\'S REVIEW (docs/FIX_BATCH_P5.md 10 play/): BEFORE = the pre-P5 look (EMB_FIX_RULES=4095), '
            'AFTER = the fingerprint. *_zoom.png: the region that changed, 4x-8x. grey/: the same in greyscale.']
    changed = 0
    for n in names:
        a = Image.open(os.path.join(bdir, n)).convert('RGB')
        b = Image.open(os.path.join(adir, n)).convert('RGB')
        if a.size != b.size:
            print(f'FAIL p5_pack {n}: sizes differ {a.size} {b.size}')
            ok = False
            continue
        base = n[:-4]
        cap = caption(n)
        box = diff_box(a, b)
        sheet = labelled((a, b), ('BEFORE (pre-P5)', 'AFTER (P5 fingerprint)' + ('' if box else ': no change')))
        files = [(base + '.png', sheet, cap + ('' if box else ' [no change]'))]
        if box:
            changed += 1
            pair, scale, crop = zoom_pair(a, b, box)
            files.append((base + '_zoom.png', labelled(pair, ('BEFORE', 'AFTER'), scale), f'{cap}, the changed region {crop} at {scale}x'))
        for fn, im, c in files:
            if max(im.size) > MAX_SIDE:
                print(f'FAIL p5_pack {fn}: {im.size} over {MAX_SIDE} px')
                ok = False
            im.save(os.path.join(out, fn))
            im.convert('L').convert('RGB').save(os.path.join(out, 'grey', fn))
            caps.append(f'{fn}  {c}')
            caps.append(f'grey/{fn}  {c} (greyscale)')
    with open(os.path.join(out, 'captions.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(caps) + '\n')
    print(f'{"PASS" if ok else "FAIL"} p5_pack wrote {len(caps) - 1} images ({changed} of {len(names)} shots changed) + captions.txt into {out}')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
