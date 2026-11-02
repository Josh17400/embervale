#!/usr/bin/env python3
# tools/doll/p4_measure.py: (P4 ART, 6e) the leather coat measured in tools/scripts/p4_art.txt's shots (witness_pack --keep):
# the torso's mean colour facing D (a box inside the coat under the collar, over the belt, between the arms: 8 x 8 art px at
# NEAR's 3 screen px a pixel, so the mean is the 1x figure's) for the leather gambeson, the T1 floor jerkin and the bronze
# brigandine, the man and the woman. PASS: the gambeson's mean in the leather ramp (hue 20-40 deg, HSL saturation >= 0.30,
# lightness <= 0.58: the buff quilt is hue 35, saturation 0.31, but lightness 0.62: cream, not hide)
# and >= 30 (RGB distance) from the floor jerkin's and from the brigandine's.
#   python tools/doll/p4_measure.py RUNDIR        (RUNDIR: <out>/runs/p4_art)
import colorsys, math, os, sys
from PIL import Image

BOX = (708, 356, 732, 380)   # the hero at (720, 365) facing D at NEAR: the coat's chest and belly


def mean(path):
    im = Image.open(path).convert('RGB').crop(BOX)
    px = [im.getpixel((x, y)) for y in range(im.height) for x in range(im.width)]
    return tuple(sum(p[i] for p in px) / len(px) for i in range(3))


def hls(c):
    h, l, s = colorsys.rgb_to_hls(*(v / 255 for v in c))
    return h * 360, l, s


def main(run):
    fails = 0
    for who in 'mw':
        m = {k: mean(os.path.join(run, f'p4_art_{who}_{k}_d.png')) for k in ('gambeson', 'floor', 'brig', 'jerkin')}
        h, l, s = hls(m['gambeson'])
        ok = 20 <= h <= 40 and s >= 0.30 and l <= 0.58
        print('%s gambeson mean (%.0f %.0f %.0f) hue %.1f light %.2f sat %.2f: %s' % (who, *m['gambeson'], h, l, s, 'PASS' if ok else 'FAIL'))
        fails += not ok
        for k in ('floor', 'brig', 'jerkin'):
            d = math.dist(m['gambeson'], m[k])
            hk, lk, sk = hls(m[k])
            gate = k != 'jerkin'
            ok = d >= 30 or not gate
            print('%s   vs %-8s mean (%.0f %.0f %.0f) hue %.1f sat %.2f: %.1f apart%s' % (
                who, k, *m[k], hk, sk, d, (': PASS' if ok else ': FAIL') if gate else ' (shown)'))
            fails += not ok
    print('p4_measure: %s' % ('PASS' if not fails else 'FAIL (%d)' % fails))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
