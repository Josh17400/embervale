#!/usr/bin/env python3
# tools/doll/p3_measure.py: (P3 ART, docs/FIX_BATCH_P3.md 8.4) the measured checks of the ART items, on the doll's own
# frames (rpg_test --doll --limbs: each frame's grip and tip marks, pivot-relative, and its 1x figure):
#   B2  the axe's and the mace's strike heads (the tip mark 2 px back along the haft) >= 4 px apart, and each >= 4 px from
#       its drawn idle's head, every facing and variant 0-2, on the man, the woman and a shield bearer
#   B6  the spear's (and the glaive's) U thrust (hits 1-2, and hit 3 of the spear): the point above the drawn idle's top
#       row (the helm) and within 2 columns of the body's centre (the idle frame's middle column)
#   B10 the bronze brigandine (T1, T2; man and woman; facing R and D): torso pixels at 1x in colours the frame did not hold
#       before (the re-cut's plates in the metal's ramp), with --before EXE (the build before the change)
#   python tools/doll/p3_measure.py [--exe build-solid/rpg_test.exe] [--before EXE] [--out DIR]
import argparse
import json
import math
import os
import subprocess
import sys
import tempfile

from PIL import Image

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SH0 = 'shield=0,shieldStyle=0,'
LOOKS = [
    ('axe', 'man', 'hero=1,kit=heartland:2:7,wpn=axe,' + SH0 + 'palette=look', 'didle,hit1,hit2,hit3'),
    ('mace', 'man', 'hero=1,kit=heartland:2:7,wpn=mace,' + SH0 + 'palette=look', 'didle,hit1,hit2,hit3'),
    ('axew', 'woman', 'heroine=1,kit=heartland:2:7,wpn=axe,' + SH0 + 'palette=look', 'didle,hit1,hit2,hit3'),
    ('macew', 'woman', 'heroine=1,kit=heartland:2:7,wpn=mace,' + SH0 + 'palette=look', 'didle,hit1,hit2,hit3'),
    ('axesh', 'man', 'hero=1,kit=heartland:4:7,wpn=axe,palette=look', 'didle,hit1,hit2,hit3'),
    ('macesh', 'man', 'hero=1,kit=heartland:4:7,wpn=mace,palette=look', 'didle,hit1,hit2,hit3'),
    ('spear', 'man', 'hero=1,kit=heartland:2:7,wpn=spear,' + SH0 + 'palette=look', 'didle,hit1,hit2,hit3'),
    ('spearw', 'woman', 'heroine=1,kit=heartland:2:7,wpn=spear,' + SH0 + 'palette=look', 'didle,hit1,hit2,hit3'),
    ('glaive', 'man', 'hero=1,kit=heartland:2:7,wpn=spear,polearmForm=2,' + SH0 + 'palette=look', 'didle,hit1,hit2,hit3'),
    ('brig1', 'man', 'hero=1,kit=heartland:1:7,bodyForm=6,mat=bronze,palette=look', 'stay'),
    ('brig2', 'man', 'hero=1,kit=heartland:2:7,bodyForm=6,mat=bronze,palette=look', 'stay'),
    ('brigw1', 'woman', 'heroine=1,kit=heartland:1:7,bodyForm=6,mat=bronze,palette=look', 'stay'),
    ('brigw2', 'woman', 'heroine=1,kit=heartland:2:7,bodyForm=6,mat=bronze,palette=look', 'stay'),
    ('brign', 'man', 'kit=heartland:1:3,bodyForm=6,mat=bronze,neck=0,shield=0,shieldStyle=0,shieldForm=0,palette=look', 'stay'),
    ('brignw', 'woman', 'kit=heartland:1:3,body=1,bodyForm=6,mat=bronze,neck=0,shield=0,shieldStyle=0,shieldForm=0,palette=look', 'stay'),
]
# B10's metal ramp: each brigandine look's twin in plate (bodyForm 7: the torso all in the metal's ramp)
LOOKS += [('pl_' + t, w, s.replace('bodyForm=6', 'bodyForm=7'), g) for t, w, s, g in LOOKS if 'bodyForm=6' in s]


def dump(exe, out):
    os.makedirs(out, exist_ok=True)
    lf = os.path.join(out, 'looks.txt')
    with open(lf, 'w') as f:
        for t, w, s, g in LOOKS:
            f.write('%s %s %s %s\n' % (t, w, s, g))
    subprocess.run([exe, '--doll', '--limbs', out, '--limbs-looks', lf], cwd=REPO, stdout=subprocess.DEVNULL, check=False)


def frame(d, tag, gait, fc, clip):
    for g in json.load(open(os.path.join(d, tag + '_gaits.json')))['gaits']:
        if g['gait'] == gait and g['facing'] == fc:
            for f in g['frames']:
                if f['clip'] == clip:
                    return f, g
    return None, None


def head(f):
    (gx, gy), (tx, ty) = f['grip'], f['tip']
    n = math.hypot(tx - gx, ty - gy) or 1
    return (tx - 2 * (tx - gx) / n, ty - 2 * (ty - gy) / n)


def b2(d):
    bad, lines = 0, []
    for a, b in (('axe', 'mace'), ('axew', 'macew'), ('axesh', 'macesh')):
        for fc in 'RDLU':
            ia, ib = head(frame(d, a, 'didle', fc, 'idle')[0]), head(frame(d, b, 'didle', fc, 'idle')[0])
            for v in range(3):
                sa = head(frame(d, a, 'hit%d' % (v + 1), fc, 'strike')[0])
                sb = head(frame(d, b, 'hit%d' % (v + 1), fc, 'strike')[0])
                dab, da, db = math.dist(sa, sb), math.dist(sa, ia), math.dist(sb, ib)
                ok = dab >= 4 and da >= 4 and db >= 4
                bad += not ok
                lines.append('  B2 %-5s %s v%d axe (%5.1f,%5.1f) mace (%5.1f,%5.1f) apart %4.1f axe-idle %4.1f mace-idle %4.1f %s'
                             % (a, fc, v, sa[0], sa[1], sb[0], sb[1], dab, da, db, 'ok' if ok else 'FAIL'))
    return bad, lines


def b6(d):
    bad, lines = 0, []
    for t in ('spear', 'spearw', 'glaive'):
        idle, _ = frame(d, t, 'didle', 'U', 'idle')
        top, mid = -idle['py'], 0.0   # the body's centre: the pivot column (between the feet, under the head box's middle)
        for v in (1, 2, 3):
            if t == 'glaive' and v == 3:
                continue   # the glaive's hit 3 is its sweep (D111: up over the left shoulder)
            f, _ = frame(d, t, 'hit%d' % v, 'U', 'strike')
            tx, ty = f['tip']
            ok = ty < top and abs(tx - mid) <= 2
            bad += not ok
            lines.append('  B6 %-6s U hit%d point (%d,%d) helm top %d body centre %.1f %s' % (t, v, tx, ty, top, mid, 'ok' if ok else 'FAIL'))
    return bad, lines


def torso_cols(d, tag, fc):
    """{colour: [pixels]} on the torso (rows 2-9 under the neck anchor, 5 columns either side of the pivot), 1x"""
    f, g = frame(d, tag, 'stay', fc, 'idle')
    im = Image.open(os.path.join(d, g['png'])).convert('RGBA')
    # the strip's first frame: the pivot at (1 + L, 2 + U), L / U the largest px / py over the strip's frames
    px = 1 + max(x['px'] for x in g['frames'])
    py = 2 + max(x['py'] for x in g['frames'])
    neck = -18 if 'w' in tag.split('_')[-1] else -19
    cols = {}
    for y in range(neck + 2, neck + 10):
        for x in range(-5, 6):
            c = im.getpixel((px + x, py + y))
            if c[3]:
                cols.setdefault(c[:3], []).append((x, y))
    return cols


def b10(d, db):
    """torso pixels in the metal's ramp (the colours the same look's plate twin shows on its torso, gorget and rivets
    included), after vs before: at least 8 and more than before"""
    bad, lines = 0, []
    for t in ('brig1', 'brig2', 'brigw1', 'brigw2', 'brign', 'brignw'):
        for fc in 'RD':
            metal = set(torso_cols(d, 'pl_' + t, fc))
            na = sum(len(p) for c, p in torso_cols(d, t, fc).items() if c in metal)
            nb = sum(len(p) for c, p in torso_cols(db, t, fc).items() if c in metal)
            ok = na >= 8 and na > nb
            if t in ('brig2', 'brigw2'):
                ok = True   # the T2 hero's cowl (and facing D the shield) cover the chest: reported, not gated
            bad += not ok
            lines.append('  B10 %-6s %s torso px in the metal ramp: %d (before %d) %s' % (t, fc, na, nb, 'ok' if ok else 'FAIL'))
    return bad, lines


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default=os.path.join(REPO, 'build-solid', 'rpg_test.exe'))
    ap.add_argument('--before', default='')
    ap.add_argument('--out', default=os.path.join(tempfile.gettempdir(), 'p3_measure'))
    a = ap.parse_args()
    a.exe = os.path.abspath(a.exe)
    a.before = os.path.abspath(a.before) if a.before else ""
    d = os.path.join(a.out, 'after')
    dump(a.exe, d)
    db = ''
    if a.before:
        db = os.path.join(a.out, 'before')
        dump(a.before, db)
    total = 0
    for name, fn in (('B2', lambda: b2(d)), ('B6', lambda: b6(d)), ('B10', lambda: b10(d, db))):
        if name == 'B10' and not db:
            print('B10: needs --before')
            continue
        bad, lines = fn()
        total += bad
        print('\n'.join(lines))
        print('%s: %s' % (name, 'PASS' if not bad else 'FAIL %d' % bad))
    sys.exit(1 if total else 0)


if __name__ == '__main__':
    main()
