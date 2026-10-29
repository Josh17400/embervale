#!/usr/bin/env python3
# tools/doll/p2c_weight.py: the weight pass over the slice's strike clips (SLICE-DOLL i2, the panel's "the body is frozen,
# only the weapon moves"). It sets each frame's `at=` (the figure's step from its pivot: back on the windup, into the blow
# on the strike and the hit-stop, half back on the pull, home on the guard) and `lean=` (the shoulders behind the weapon)
# in the four facings, on the guard's swing (clips/actions/sword_shield.clip: never the hero's A3 sword, which B1 holds
# pixel-identical; and since the wave-2 fix round never the spear's thrust, whose steps clips/actions/spear.clip authors
# per frame). Idempotent: it rewrites the two keys only.
#   python tools/doll/p2c_weight.py [--root DIR]
import argparse
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
FWD = {'R': (1, 0), 'L': (-1, 0), 'D': (0, 1), 'U': (0, -1)}
# per clip: one (step, lean) per frame, in forward units (lean only in the side views)
# (INTEGRATE i2) the spear's leans are SLICE-ARMS's (lunge lean 4 on the sheared torso, the 60 ms settle): only the step is added
SPEAR = {'windup': [(-2, None)], 'strike': [(3, None), (3, None)], 'follow': [(1, None), (0, None)], 'block': [(-2, None)]}
SPEAR_D = {'R': None, 'L': None, 'D': (1, 1), 'U': (-1, -1)}   # the spear's own line facing down / up (a337 / a157)
# (the sword's arm pieces sit on the pivot, not the shoulders: its leans stay A3's, only the step is added)
# (wave-2 fix round, ACTIONS r1) the block is DOLL-C's three-beat block now: p2c_clips.py writes its steps per frame (1-3-2 px
# back, lean 1), so this table leaves it alone (its old one-step brace re-stepped all three beats to 2 px)
SWORD = {'windup': [(-1, None)], 'strike': [(2, None)], 'follow': [(1, None), (0, None)]}
# the brace: the block on the wide stance (the windup's legs), so it never reads as the drawn idle
EXTRA = {}   # (i2: the block on the stance put the kite over the shin facing right, C3c; the brace is the step back and lean)
# (i3, the panel: the knight's block read as the idle) the brace crouches a pixel lower (bob 2) under the raised haft
EXTRA.update({('spear', 'block', fc): {'bob': '2'} for fc in 'DURL'})


def setkey(line, key, val):
    if re.search(rf'(?<= ){key}=\S+', line):
        line = re.sub(rf'(?<= ){key}=\S+', f'{key}={val}', line) if val is not None else re.sub(rf' {key}=\S+', '', line)
    elif val is not None:
        line = line.rstrip() + f' {key}={val}'
    return line


def weigh(path, table, fam, diag=None):
    out, clip, fc, i = [], None, None, 0
    n = 0
    for line in open(path, encoding='utf-8').read().split('\n'):
        m = re.match(r'clip (\w+) ([DURL]) .*family (\w+)', line)
        if m:
            clip, fc, i = (m.group(1), m.group(2), 0) if m.group(3) == fam else (None, None, 0)
        elif line.startswith('clip '):
            clip = None
        elif line.startswith('f ') and clip in table:
            steps = table[clip]
            st, ln = steps[min(i, len(steps) - 1)]
            i += 1
            d = (diag or {}).get(fc) or FWD[fc]
            if fc in 'DU':
                st = max(-1, min(1, st))   # (facing the camera a step is a row: kept to one)
            at = (d[0] * st, d[1] * st)
            line = setkey(line, 'at', f'{at[0]},{at[1]}' if at != (0, 0) else None)
            if fc in 'RL' and ln is not None:
                line = setkey(line, 'lean', str(ln * FWD[fc][0]) if ln else None)
            for k, v in EXTRA.get((fam, clip, fc), {}).items():
                line = setkey(line, k, v)
            n += 1
        out.append(line)
    open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
    print(f'p2c_weight: {path} ({n} frames)')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=os.path.join(REPO, 'rpg', 'art', 'doll'))
    a = ap.parse_args()
    # (wave-2 fix round, ACTIONS r1) never over spear.clip again: since wave 2 its steps are authored per frame and blow
    # in the file (ACTIONS-2H); SPEAR / SPEAR_D stay only as the slice's record
    weigh(os.path.join(a.root, 'clips', 'actions', 'sword_shield.clip'), SWORD, 'sword')


if __name__ == '__main__':
    main()
