#!/usr/bin/env python3
# tools/doll/owner_pack.py: the P2b owner pack (STYLE_P2 19 item 6), regenerated from the game. Every file comes from the
# game itself: engine scripts (tools/scripts/p2b_slice_pack.txt and the slice look / scene scripts), the doll's A3
# parity dump (rpg_test --doll --parity --dump) and frame-exact Witness captures at 60 Hz (camhold, 1 art px per shot px)
# for the walk, run and swing GIFs. Nothing is drawn by this tool; it only crops, scales by whole numbers and lays out.
#
#   python tools/doll/owner_pack.py [--bdir build] [--out DIR] [--assemble-only] [--keep-work]
#
# Run from the repo root in Git Bash (Python 3 with Pillow and numpy; node for Witness, ../witness beside the repo).
# Every heavy step goes through tools/slot.sh. Witness writes its shots into witness/shots (it has no output option):
# this tool names them op_* and moves them into DIR/work/motion. The 25 files land in DIR:
#   01 the parity sheet: every A3 frame the doll composes beside A3's own (rpg_test --doll --parity --dump), 0 px
#   02-05 the variety, armour, elders-and-children and villager-swap rows at NEAR (D4), four facings (down right left up)
#   06 walk (44 px/s), 07 run (74 px/s), 08 swing (hit 1): the hero and the woman, Witness at 60 Hz, game timing
#      (every second game frame shown for 33.3 ms: GIF delays alternate 30/40/30 ms), 3x, the ground held (camhold)
#   09 the hero's fall and lying (down and right, both variants): fall frames 0-2 and the lying body, D4
#   10 Sit on real seats: the variety row seated by `seatall` in the start village's inn, D4
#   11 A3's scene_phone_zoom.png beside the same spot in the engine (phone D5, 2x)
#   12-25 the desktop (1440x810, D3 / D4) and phone (2556x1179, D5) shots, 11:00 and 22:00
import argparse
import json
import os
import shutil
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
BG = (24, 22, 30)
LABEL = (200, 196, 210)
FONT = ImageFont.load_default()

# ---- capture
SCRIPTS = ['p2b_slice_pack', 'p2b_slice_look', 'p2b_slice_look_phone', 'p2b_slice_scene', 'p2b_slice_scene_phone']
KEYS = {'east': 'd', 'west': 'a', 'south': 's', 'north': 'w'}
HERO = [["hair", "short", "copper"], ["slicelook", "a3"]]
WOMAN = [["body", "woman"], ["hair", "braids", "wheat"], ["slicelook", "a3woman", "outfit=1", "weapon=0"]]
WALK_F0, WALK_N = 60, 120     # 2 s of the sneak walk (44 px/s), from frame 60
RUN_F0, RUN_N = 56, 72        # 1.2 s of the run (74 px/s): the hero stays on the 480x270 canvas facing north
SWING_F0, SWING_N, SWING_AT = 70, 54, 1.30   # hit 1 from a stand: 0.13 s of idle, the swing, the recover


def motion_suite():
    """The Witness suite: (panel name, test list). Each panel = a camhold run + its hideplayer twin (the crop masks)."""
    tests, panels = [], []
    base = [["weather", "clear"], ["skill", "kit", "sword", "none"], ["skill", "clearing", 12], ["ui", 0], ["ring", 0]]

    def add(name, look, extra, inp, f0, n):
        panels.append((name, n))
        for hidden in (False, True):
            calls = base + look + extra + ([["hideplayer", 1]] if hidden else [])
            steps = [{"at": 0.2, "call": calls}, {"at": 0.8, "call": [["camhold", 1]]}]
            for i in range(n):
                steps.append({"at": f"{f0 + i}f", "shot": f"op_{name}_{i:03d}{'_nb' if hidden else ''}.png"})
            if not hidden:
                steps[2]["probe"] = ["state.player.anim", "state.player.sprite"]
                steps[-1]["probe"] = ["state.player.anim", "state.player.sprite"]
            tests.append({"name": f"owner pack {name}{' hidden' if hidden else ''}", "seed": 7,
                          "params": {"rig": 2, "hour": 11, "god": True, "at": [-140, -432]}, "input": inp, "steps": steps})

    for who, look in (('hero', HERO), ('woman', WOMAN)):
        for d in ('east', 'west', 'south', 'north'):
            add(f'walk_{who}_{d}', look, [["skill", "sneak", 1]], f"{KEYS[d]}@0.9-{(WALK_F0 + WALK_N + 6) / 60:.2f}", WALK_F0, WALK_N)
    for who, look in (('hero', HERO), ('woman', WOMAN)):
        for d in ('east', 'west', 'south', 'north'):
            add(f'run_{who}_{d}', look, [], f"{KEYS[d]}@0.9-{(RUN_F0 + RUN_N + 6) / 60:.2f}", RUN_F0, RUN_N)
    for d in ('east', 'south', 'west', 'north'):   # face with a tap, then hit 1
        add(f'swing_hero_{d}', HERO, [], f"{KEYS[d]}@0.9-0.94, j@{SWING_AT:.2f}-{SWING_AT + 0.05:.2f}", SWING_F0, SWING_N)
    return tests, panels


def run(cmd, log, cwd=None):
    with open(log, 'w', encoding='utf-8') as f:
        r = subprocess.run(cmd, cwd=cwd or REPO, stdout=f, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
    tail = open(log, encoding='utf-8', errors='replace').read().strip().splitlines()[-1:] or ['']
    print(f'  {os.path.basename(log)}: rc={r.returncode} {tail[0][:100]}')
    return r.returncode


def capture(bdir, work):
    os.makedirs(work, exist_ok=True)
    exe = os.path.join(REPO, bdir, 'embervale.exe')
    slot = ['bash', os.path.join(REPO, 'tools', 'slot.sh')]
    print('engine scripts')
    for s in SCRIPTS:
        run(slot + [exe, '--script', os.path.join(REPO, 'tools', 'scripts', s + '.txt')], os.path.join(work, s + '.log'), cwd=work)
    print('parity dump')
    pd = os.path.join(work, 'parity')
    shutil.rmtree(pd, ignore_errors=True)
    os.makedirs(pd)
    run(slot + [os.path.join(REPO, bdir, 'rpg_test.exe'), '--doll', '--parity', '--dump', pd], os.path.join(work, 'parity.log'))
    print('witness captures (60 Hz)')
    tests, _ = motion_suite()
    suite = os.path.join(work, 'owner_pack_motion.witness.json')
    json.dump(tests, open(suite, 'w'), indent=0)
    shots = os.path.join(REPO, 'witness', 'shots')
    for f in os.listdir(shots) if os.path.isdir(shots) else []:
        if f.startswith('op_'):
            os.remove(os.path.join(shots, f))
    run(slot + ['node', os.path.join('..', 'witness', 'bin', 'witness.js'), 'test', suite, '--cmd', bdir + '\\embervale --witness', '--json'],
        os.path.join(work, 'motion.json'))
    md = os.path.join(work, 'motion')
    shutil.rmtree(md, ignore_errors=True)
    os.makedirs(md)
    for f in os.listdir(shots):
        if f.startswith('op_'):
            shutil.move(os.path.join(shots, f), os.path.join(md, f))


# ---- assembly helpers
def load(p):
    return Image.open(p).convert('RGB')


def label(img, text, x=4, y=2):
    ImageDraw.Draw(img).text((x, y), text, fill=LABEL, font=FONT)


def stack(images, gap=4, vertical=True, bg=BG):
    w = max(i.width for i in images) if vertical else sum(i.width for i in images) + gap * (len(images) - 1)
    h = sum(i.height for i in images) + gap * (len(images) - 1) if vertical else max(i.height for i in images)
    out = Image.new(images[0].mode, (w, h), bg if images[0].mode == 'RGB' else 0)
    at = 0
    for i in images:
        out.paste(i, (0, at) if vertical else (at, 0))
        at += (i.height if vertical else i.width) + gap
    return out


def heaviest_run(counts, minCount, gap):
    """The [lo, hi) span of the heaviest group of indices with counts >= minCount, groups split by more than gap."""
    idx = np.nonzero(counts >= minCount)[0]
    if not len(idx):
        return None
    best, lo, mass = None, idx[0], 0
    for k, i in enumerate(idx):
        mass += int(counts[i])
        if k + 1 == len(idx) or idx[k + 1] - i > gap:
            if best is None or mass > best[2]:
                best = (int(lo), int(i) + 1, mass)
            if k + 1 < len(idx):
                lo, mass = idx[k + 1], 0
    return best[0], best[1]


def bbox_of_diff(a, b, minCount=2, band=False):
    """The box of what differs between a shot and its twin. band: the heaviest horizontal band and the heaviest run of
    columns in it (a row of people; a townsperson who wandered elsewhere between the shots is left out)."""
    m = np.any(np.abs(np.asarray(a, np.int16) - np.asarray(b, np.int16)) > 24, axis=2)
    if band:
        r = heaviest_run(m.sum(axis=1), minCount, 6)
        if r is None:
            return None
        c = heaviest_run(m[r[0]:r[1]].sum(axis=0), minCount, 64)
        return None if c is None else [c[0], r[0], c[1], r[1]]
    cols = np.nonzero(m.sum(axis=0) >= minCount)[0]
    rows = np.nonzero(m.sum(axis=1) >= minCount)[0]
    if not len(cols) or not len(rows):
        return None
    return [int(cols.min()), int(rows.min()), int(cols.max()) + 1, int(rows.max()) + 1]


def union(a, b):
    if a is None:
        return b
    if b is None:
        return a
    return [min(a[0], b[0]), min(a[1], b[1]), max(a[2], b[2]), max(a[3], b[3])]


def grow(box, m, w, h):
    return [max(0, box[0] - m), max(0, box[1] - m), min(w, box[2] + m), min(h, box[3] + m)]


def parity_sheet(work):
    pd = os.path.join(work, 'parity')
    names = sorted(f[:-4] for f in os.listdir(pd) if f.endswith('.png'))
    groups = []
    for who in ('hero', 'woman'):
        groups.append([n for n in names if n.startswith(who + '_IDLE')])
        for d in 'RDLU':
            groups.append([n for n in names if n.startswith(f'{who}_SWING_{d}')])
        for d in 'DURL':
            groups.append([n for n in names if n.startswith(f'{who}_WALK_{d}')])
    rows = []
    for g in groups:
        if not g:
            continue
        tiles = []
        for n in g:
            im = load(os.path.join(pd, n + '.png'))
            t = Image.new('RGB', (im.width, im.height + 14), BG)
            t.paste(im, (0, 14))
            label(t, n + '  (engine | A3 | red: px that differ)', 2, 1)
            tiles.append(t)
        rows.append(stack(tiles, 4, vertical=False))
    return stack(rows, 6)


def lineup_sheet(work, row):
    shots = [load(os.path.join(work, f'p2b_pack_{row}_{f}.png')) for f in ('down', 'right', 'left', 'up')]
    empty = load(os.path.join(work, f'p2b_pack_{row}_empty.png'))
    box = None
    for s in shots:
        box = union(box, bbox_of_diff(s, empty, 2, band=True))
    box = grow(box, 12, empty.width, empty.height)
    return stack([s.crop(box) for s in shots], 4)


def fall_sheet(work):
    W, H = 1440, 810   # the desktop window; the camera centres the hero
    box = (W // 2 - 150, H // 2 - 105, W // 2 + 150, H // 2 + 95)
    rows = []
    for d in ('down', 'right'):
        for v in (0, 1):
            names = [f'p2b_pack_fall_{d}_v{v}_f{k}.png' for k in range(3)] + [f'p2b_pack_lying_{d}_v{v}.png']
            tiles = []
            for n in names:
                t = load(os.path.join(work, n)).crop(box)
                label(t, n[9:-4])
                tiles.append(t)
            rows.append(stack(tiles, 4, vertical=False))
    return stack(rows, 4)


def scene_beside(work):
    a3 = load(os.path.join(REPO, 'docs', 'ref', 'style_a3', 'scene_phone_zoom.png'))
    eng = load(os.path.join(work, 'p2b_scene_day_world_phone.png'))
    # the spot A3's zoom frames (the hero facing the woman, the villager behind): 600x460 device px at D5, shown 2x
    crop = eng.crop((1015, 260, 1015 + 600, 260 + 460)).resize((1200, 920), Image.NEAREST)
    label(a3, 'A3 reference (docs/ref/style_a3/scene_phone_zoom.png)', 6, 4)
    label(crop, 'engine, rig 2, phone D5 (p2b_scene_day_world_phone.png), 2x', 6, 4)
    return stack([a3, crop], 12, vertical=False)


def figure_palette(img, mask, keep=200):
    """img in 256 colours, the people's own exact: every colour under mask (the figure: the shot against its hideplayer
    twin) is kept as drawn (up to keep, the commonest first); the ground and the labels share the rest (median cut)."""
    a = np.asarray(img)
    fig = a[np.asarray(mask) > 0]
    cols = np.zeros((0, 3), np.uint8)
    if len(fig):
        u, cnt = np.unique(fig.reshape(-1, 3), axis=0, return_counts=True)
        cols = u[np.argsort(-cnt)][:keep]
    rest = a[np.asarray(mask) == 0].reshape(1, -1, 3)
    nb = 256 - len(cols)
    if rest.size and nb > 0:
        q = Image.fromarray(rest.astype(np.uint8)).quantize(colors=nb, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        bp = np.array(q.getpalette()[:3 * nb], np.uint8).reshape(-1, 3)
        cols = np.concatenate([cols, bp])[:256]
    flat = list(cols.reshape(-1)) + [0] * (768 - cols.size)
    pimg = Image.new('P', (1, 1))
    pimg.putpalette([int(v) for v in flat])
    return img.quantize(palette=pimg, dither=Image.Dither.NONE)


def gif(work, kind, panels, path, scale=3):
    """One GIF of camhold panels (all the same frame count), every second 60 Hz game frame at game timing."""
    md = os.path.join(work, 'motion')
    sel = [(n, c) for n, c in panels if n.startswith(kind + '_')]
    n = sel[0][1]
    crops = []
    for name, cnt in sel:
        box = None
        first = load(os.path.join(md, f'op_{name}_000.png'))
        for i in range(cnt):
            box = union(box, bbox_of_diff(load(os.path.join(md, f'op_{name}_{i:03d}.png')),
                                          load(os.path.join(md, f'op_{name}_{i:03d}_nb.png')), 1))
        crops.append((name, grow(box, 18 if kind == 'swing' else 6, first.width, first.height)))
    def layout(tiles):
        if kind == 'swing':
            return stack(tiles, 6, vertical=False)
        rows = []   # per body: east over west (the long side runs), then south and north beside them
        for k in range(0, len(tiles), 4):
            e, w, s, n4 = tiles[k:k + 4]
            rows.append(stack([stack([e, w], 6), s, n4], 6, vertical=False))
        return stack(rows, 6)

    frames, masks = [], []
    for i in range(0, n, 2):
        tiles, mtiles = [], []
        for name, box in crops:
            shot = load(os.path.join(md, f'op_{name}_{i:03d}.png'))
            twin = load(os.path.join(md, f'op_{name}_{i:03d}_nb.png'))
            m = Image.fromarray((np.any(np.asarray(shot) != np.asarray(twin), axis=2) * 255).astype(np.uint8)).crop(box)
            im = shot.crop(box)
            im = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
            m = m.resize((m.width * scale, m.height * scale), Image.NEAREST)
            t = Image.new('RGB', (im.width, im.height + 14), BG)
            t.paste(im, (0, 14))
            label(t, name.replace('_', ' '), 2, 1)
            tm = Image.new('L', (m.width, m.height + 14), 0)
            tm.paste(m, (0, 14))
            tiles.append(t)
            mtiles.append(tm)
        frames.append(layout(tiles))
        masks.append(layout(mtiles))
    # 33.33 ms a shown frame, in GIF centiseconds without drift: 30, 40, 30, 30, 40, 30 ...
    dur = [int(round((k + 1) * 10 / 3.0)) * 10 - int(round(k * 10 / 3.0)) * 10 for k in range(len(frames))]
    pal = [figure_palette(f, m) for f, m in zip(frames, masks)]
    pal[0].save(path, save_all=True, append_images=pal[1:], duration=dur, loop=0, optimize=False, disposal=1)


COPIES = [
    ('12_desktop_D3_day_lineup.png', 'p2b_look_d3_variety.png'),
    ('13_desktop_D4_day_lineup_world.png', 'p2b_look_d4_variety_world.png'),
    ('14_desktop_D3_2200_lineup.png', 'p2b_look_d3_night.png'),
    ('15_phone_D5_day_lineup.png', 'p2b_look_d3_variety_phone.png'),
    ('16_phone_D5_2200_lineup.png', 'p2b_look_d3_night_phone.png'),
    ('17_desktop_D3_hero.png', 'p2b_look_d3_down.png'),
    ('18_desktop_D4_hero.png', 'p2b_look_d4_down.png'),
    ('19_desktop_D3_elders.png', 'p2b_look_d3_elders.png'),
    ('20_desktop_D4_armour.png', 'p2b_look_d4_armour.png'),
    ('21_desktop_D3_scene_day.png', 'p2b_scene_day.png'),
    ('22_desktop_D4_scene_world.png', 'p2b_scene_day_d4_world.png'),
    ('23_desktop_D3_scene_2200.png', 'p2b_scene_night.png'),
    ('24_phone_D5_scene_day.png', 'p2b_scene_day_phone.png'),
    ('25_phone_D5_scene_2200.png', 'p2b_scene_night_phone.png'),
]


def assemble(work, out):
    os.makedirs(out, exist_ok=True)
    _, panels = motion_suite()
    made = []

    def put(name, img):
        img.save(os.path.join(out, name))
        made.append(name)

    put('01_parity_sheet.png', parity_sheet(work))
    for name, row in (('02_variety_lineup_D4.png', 'variety'), ('03_armour_lineup_D4.png', 'armour'),
                      ('04_elders_children_D4.png', 'elders'), ('05_villager_swaps_D4.png', 'swaps')):
        put(name, lineup_sheet(work, row))
    for name, kind in (('06_walk_44pxs.gif', 'walk'), ('07_run_74pxs.gif', 'run'), ('08_swing_hit1.gif', 'swing')):
        gif(work, kind, panels, os.path.join(out, name))
        made.append(name)
    put('09_fall_and_lying_D4.png', fall_sheet(work))
    put('10_sit_inn_D4.png', load(os.path.join(work, 'p2b_pack_sit_inn_d4_world.png')))
    put('11_A3_scene_beside_engine.png', scene_beside(work))
    for dst, src in COPIES:
        shutil.copyfile(os.path.join(work, src), os.path.join(out, dst))
        made.append(dst)
    return made


def main():
    ap = argparse.ArgumentParser(description='the P2b owner pack from the game (STYLE_P2 19 item 6)')
    ap.add_argument('--bdir', default='build', help='the build directory (embervale.exe and rpg_test.exe), relative to the repo')
    ap.add_argument('--out', default=os.path.join(os.environ.get('LOCALAPPDATA', '/tmp'), 'Temp', 'claude', 'emb_P2b', 'owner_pack'))
    ap.add_argument('--assemble-only', action='store_true', help='reuse DIR/work (no game runs)')
    ap.add_argument('--keep-work', action='store_true', help='keep DIR/work (the raw shots and captures)')
    a = ap.parse_args()
    out = os.path.abspath(a.out)
    work = os.path.join(out, 'work')
    if not a.assemble_only:
        capture(a.bdir, work)
    made = assemble(work, out)
    if not a.keep_work:
        shutil.rmtree(work, ignore_errors=True)
    print(f'owner pack: {len(made)} files in {out}')
    return 0 if len(made) == 25 else 1


if __name__ == '__main__':
    sys.exit(main())
