#!/usr/bin/env python3
"""tools/doll/p5b_fix_r2.py (P5b2 fix round 2, the lead; docs/STYLE_P5B.md 2, 4; judge P5b2 r2 items 1, 4, 5, 6): ADDED views
only. Every block this script writes is tagged `# (P5b fix-r2) begin <tag>` .. `end <tag>`: new `when` views (a replace view
at the top of the part's views, where the first replacing view whose condition holds wins; an overlay at the end) and their
local legend lines. No view drawn before this round changes a cell. Run it LAST, after tools/doll/p5b_sig_A2.py, p5b_sig_B2.py,
p5b_sig_C2.py and p5b_fix_r1.py (those rebuild their blocks from their own bases and keep the rest of each map; this script
strips its own blocks and writes them again, so it is idempotent), then `node tools/doll/pxc.mjs`.

  python tools/doll/p5b_fix_r2.py            write the blocks into rpg/art/doll/maps
  python tools/doll/p5b_fix_r2.py --verify   each touched map is the base's (ad67d9c, fix r1) but for this script's blocks
  python tools/doll/p5b_fix_r2.py --print    print the blocks

The blocks:
  steppe-t4-lit   (C19 A3, ENKHOBRASS: the helm's top-left 6 px of steps 3-4) the Steppe T4 signature's spiked bowl (plumed
                  variant 2, the owner's T4) facing D with its highlight carried up the left of the cone: the drawn view lit the
                  bowl's left from the brim up but left the cone's upper left dark, so under the horsetail plume (the head box's
                  top) the helm's top-left quadrant held 4 lit px. Two cells of row 8 step up (x 2 -> 3): the highlight now
                  sweeps from the cone's top down its left flank. Signed Steppe T4 helms only (sighelm-steppe).
  sylvan-leaf5-tip (C19 A7, THAERSTEEL: 2-4 speculars of 2-6 px) the Sylvan T5 signature's great leaf pauldron (leaf.5, the
                  right shoulder, facing D): the last leaf's gilt tip held a lone 1 px step-4 glint (the compositor's glints
                  keep a drawn step-3 trim point); under the signed Sylvan T5 body the tip's outer two cells are lit together, a
                  2 px glint on the leaf's lit (outer) half. (Dimming the tip instead moved the glass pass's glints: four on the
                  winged helm.)
"""
import os
import re
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
MAPS = os.path.join(ROOT, 'rpg', 'art', 'doll', 'maps')
TAG = '(P5b fix-r2)'
BASE = 'ad67d9c'   # fix r1: the maps before this round


def P(*parts):
    return os.path.join(MAPS, *parts)


def read_lines(path):
    return open(path, encoding='utf-8').read().replace('\r\n', '\n').split('\n')


def find_view(lines, head):
    """the body of the view whose header line is exactly `head`: (marks, grid, regions), each a list of lines"""
    for i, s in enumerate(lines):
        if s.rstrip() == head:
            j = i + 1
            marks, grid, regs = [], [], []
            while j < len(lines) and lines[j].startswith('mark '):
                marks.append(lines[j])
                j += 1
            while j < len(lines) and lines[j].startswith('|'):
                grid.append(lines[j])
                j += 1
            if j < len(lines) and lines[j].strip() == 'regions':
                j += 1
                while j < len(lines) and lines[j].startswith('|'):
                    regs.append(lines[j])
                    j += 1
            return marks, grid, regs
    raise SystemExit(f'no view "{head}"')


def set_cells(grid, cells):
    """grid rows ('|'-prefixed) with (row, col, old, new) cells changed; old is checked"""
    rows = [list(r[1:]) for r in grid]
    for y, x, old, new in cells:
        if rows[y][x] != old:
            raise SystemExit(f'cell ({x},{y}) is {rows[y][x]!r}, expected {old!r}')
        rows[y][x] = new
    return ['|' + ''.join(r) for r in rows]


def overlay_of(grid, regs, cells):
    """an overlay grid the base's size: '.' everywhere but the cells (row, col, old, new), each with its base region"""
    rows = [list(r[1:]) for r in grid]
    rr = [list(r[1:]) for r in regs] if regs else None
    out = [['.'] * len(r) for r in rows]
    oreg = [['.'] * len(r) for r in rows]
    for y, x, old, new in cells:
        if rows[y][x] != old:
            raise SystemExit(f'cell ({x},{y}) is {rows[y][x]!r}, expected {old!r}')
        out[y][x] = new
        if rr:
            oreg[y][x] = rr[y][x]
    return ['|' + ''.join(r) for r in out], ['|' + ''.join(r) for r in oreg]


# ------------------------------------------------------------------------------------------------ the blocks
def steppe_t4_lit(base):
    head = 'view D key rest origin 2 4 when helmfam-plumed,variant-2 replace'
    marks, grid, regs = find_view(base, head)
    g = set_cells(grid, [(8, 7, 'x', 'y'), (8, 8, 'x', 'y')])
    return [('view D key rest origin 2 4 when sighelm-steppe,helm.t4,!helm.t5,variant-2 replace', marks, g, regs)]


def sylvan_leaf5_tip(base):
    out = []
    for key, org in (('rest', '6 3'), ('up', '6 4')):
        marks, grid, regs = find_view(base, f'view D key {key} origin {org}')
        g, r = overlay_of(grid, regs, [(12, 4, 'w', 'y')])
        out.append((f'view D key {key} when sigbody-sylvan,armour.t5,!armour.t6', [], g, r))
    return out


def plan():
    """(path, tag, description, builder, place)"""
    return [
        (P('headwear', 'helm', 'plumed.pxm'), 'steppe-t4-lit', 'the Steppe T4 spiked bowl facing D, its highlight carried up the '
         'left of the cone (C19 A3: the brass helm lit top-left)', steppe_t4_lit, 'top'),
        (P('armour', 'shared', 'pauldron_leaf5_r.pxm'), 'sylvan-leaf5-tip', 'the Sylvan T5 great leaf pauldron facing D, the last '
         'leaf\'s gilt tip lit 2 px across (C19 A7: 2-6 px speculars)', sylvan_leaf5_tip, 'end'),
    ]


# ------------------------------------------------------------------------------------------------ the pxm plumbing
def own_strip(lines):
    out, skip = [], False
    for s in lines:
        if s.startswith(f'# {TAG} begin '):
            skip = True
            continue
        if s.startswith(f'# {TAG} end '):
            skip = False
            continue
        if not skip:
            out.append(s)
    return out


def block(tag, desc, views):
    lines = [f'# {TAG} begin {tag}: {desc}']
    for head, marks, grid, regs in views:
        lines.append(head)
        lines += marks
        lines += grid
        if regs and any(c != '.' for r in regs for c in r[1:]):
            lines.append('regions')
            lines += regs
    lines.append(f'# {TAG} end {tag}')
    return lines


def build(path, text):
    src = own_strip(text.replace('\r\n', '\n').split('\n'))
    while src and src[-1] == '':
        src.pop()
    top, end = [], []
    for p, tag, desc, fn, place in plan():
        if os.path.normcase(p) != os.path.normcase(path):
            continue
        lines = block(tag, desc, fn(src))
        (top if place == 'top' else end).extend(lines + [''])
    i = next(n for n, s in enumerate(src) if s.startswith('view ') or s.startswith('# (P5b'))
    return src[:i] + top + src[i:] + ([''] + end[:-1] if end else []) + [''], top + end


def touched():
    out = []
    for p, *_ in plan():
        if p not in out:
            out.append(p)
    return out


def base_text(path):
    rel = os.path.relpath(path, ROOT).replace('\\', '/')
    r = subprocess.run(['git', '-C', ROOT, 'show', f'{BASE}:{rel}'], capture_output=True, text=True, encoding='utf-8')
    if r.returncode:
        raise SystemExit(f'git show {BASE}:{rel} failed')
    return r.stdout


def write():
    for path in touched():
        new, add = build(path, open(path, encoding='utf-8').read())
        open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(new))
        print(f'{os.path.relpath(path, ROOT)}: {len([s for s in add if s])} lines in {TAG} blocks')


def verify():
    bad = 0
    for path in touched():
        rel = os.path.relpath(path, ROOT).replace('\\', '/')
        old = own_strip(base_text(path).replace('\r\n', '\n').split('\n'))
        now = own_strip(open(path, encoding='utf-8').read().replace('\r\n', '\n').split('\n'))
        norm = lambda ls: [s for s in ls if s.strip()]
        if norm(old) != norm(now):
            print(f'FAIL {rel}: outside this script\'s blocks the map differs from {BASE}')
            bad += 1
        else:
            print(f'ok   {rel}: only {TAG} blocks added')
    return bad


if __name__ == '__main__':
    if '--verify' in sys.argv:
        sys.exit(1 if verify() else 0)
    if '--print' in sys.argv:
        for path in touched():
            print('\n'.join(build(path, open(path, encoding='utf-8').read())[1]))
        sys.exit(0)
    write()
