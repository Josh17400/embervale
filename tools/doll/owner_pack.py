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
#
# (P2c SLICE-DOLL) python tools/doll/owner_pack.py --p2c [--bdir build] [--out DIR] [--assemble-only] [--keep-work]: the P2c
# style slice's owner pack (STYLE_P2 26), its 11 boards from the game: the engine scripts tools/scripts/p2c_slice.txt,
# _phone, _pack and _fight, rpg_test --doll --parity --dump (the A3 re-cut), rpg_test --doll --limbs on
# tools/doll/slice_looks.txt (every slice clip in four facings, with each frame's ms) and the gates (--doll --gates --parity
# --report, --doll-arms --report). Default DIR %LOCALAPPDATA%/Temp/claude/emb_P2c/owner_pack. The boards:
#   01 the A3 re-cut parity sheet           02 the slice lineup at NEAR (D4), four facings, 11:00 and 22:00
#   03 the fit pairs (mail, plate, leather on both bodies)
#   04 side walk and run: 8x strips (both facings) with the pivot and sole row marked, and 3x game-speed GIFs
#   05 the guard: drawn guard, hit 1 and block (GIF at game speed and at 1/4)
#   06 the plate soldier: carry, drawn stance, thrust and block (GIF, game speed and 1/4)
#   07 the archer: draw and release (GIF, game speed and 1/4)       08 the falls and the lying bodies with gear
#   09 each slice look on rig 1 (v1) beside rig 2                     10 the in-world shots (desktop D3 / D4, phone D5, 22:00)
#   11 the budgets and gates table
# A missing input is drawn as a red MISSING tile and counted; the exit code is 0 only with 11 boards and none missing.
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


# ======================================================================================== (P2c SLICE-DOLL) the P2c pack
P2C_SCRIPTS = ['p2c_slice', 'p2c_slice_phone', 'p2c_slice_pack', 'p2c_slice_fight']
P2C_LOOKS = ['hero', 'guard', 'knight', 'archer', 'knightm', 'archerw']
P2C_MISSING = []


def p2c_capture(bdir, work):
    os.makedirs(work, exist_ok=True)
    exe = os.path.join(REPO, bdir, 'embervale.exe')
    rt = os.path.join(REPO, bdir, 'rpg_test.exe')
    slot = ['bash', os.path.join(REPO, 'tools', 'slot.sh')]
    print('engine scripts')
    for s in P2C_SCRIPTS:
        run(slot + [exe, '--script', os.path.join(REPO, 'tools', 'scripts', s + '.txt')], os.path.join(work, s + '.log'), cwd=work)
    print('parity dump, limbs strips, gates')
    for d in ('parity', 'limbs'):
        shutil.rmtree(os.path.join(work, d), ignore_errors=True)
        os.makedirs(os.path.join(work, d))
    run(slot + [rt, '--doll', '--parity', '--dump', os.path.join(work, 'parity')], os.path.join(work, 'parity.log'))
    run(slot + [rt, '--doll', '--limbs', os.path.join(work, 'limbs'), '--limbs-looks', os.path.join(REPO, 'tools', 'doll', 'slice_looks.txt')],
        os.path.join(work, 'limbs.log'))
    run(slot + [rt, '--doll', '--gates', '--parity', '--report'], os.path.join(work, 'gates.log'))
    run(slot + [rt, '--doll-arms', '--report'], os.path.join(work, 'arms.log'))


def p2c_load(work, name):
    path = os.path.join(work, name)
    if os.path.exists(path):
        return load(path)
    P2C_MISSING.append(name)
    t = Image.new('RGB', (220, 40), (90, 20, 24))
    label(t, 'MISSING: ' + name, 4, 14)
    return t


def scale(img, k):
    return img.resize((img.width * k, img.height * k), Image.NEAREST)


def titled(img, text):
    t = Image.new('RGB', (max(img.width, 8 * len(text) + 8), img.height + 16), BG)
    t.paste(img, (0, 16))
    label(t, text, 2, 2)
    return t


def p2c_lineup(work, row, hour):
    shots = [p2c_load(work, f'p2c_pack_{row}_{hour}_{f}.png') for f in ('down', 'right', 'left', 'up')]
    empty = p2c_load(work, f'p2c_pack_{row}_{hour}_empty.png')
    box = None
    for sh in shots:
        if sh.size == empty.size:
            box = union(box, bbox_of_diff(sh, empty, 2, band=True))
    if box is None:
        return stack(shots, 4)
    box = grow(box, 12, empty.width, empty.height)
    return stack([sh.crop(box) if sh.size == empty.size else sh for sh in shots], 4)


def p2c_gaits(work, tag):
    path = os.path.join(work, 'limbs', tag + '_gaits.json')
    if not os.path.exists(path):
        P2C_MISSING.append('limbs/' + tag + '_gaits.json')
        return {}
    return {(g['gait'], g['facing']): g for g in json.load(open(path))['gaits']}


def p2c_frames(work, tag, gait, facing):
    """The frames of a gait strip (limbs/<tag>_<gait>_<F>.png, 1x) cut into cells, with each frame's ms."""
    G = p2c_gaits(work, tag).get((gait, facing))
    strip = os.path.join(work, 'limbs', f'{tag}_{gait}_{facing}.png')
    if not G or not G['frames'] or not os.path.exists(strip):
        P2C_MISSING.append(f'limbs/{tag}_{gait}_{facing}.png')
        return [], []
    im = load(strip)
    n = len(G['frames'])
    cw = (im.width - 2) // n
    cells = [im.crop((1 + i * cw, 1, 1 + (i + 1) * cw, im.height - 1)) for i in range(n)]
    ms = [max(40, int(f.get('ms') or 0) or 100) for f in G['frames']]
    return cells, ms


def p2c_gif(path, tracks, k=3, slow=1):
    """An animated GIF: each track (label, cells, ms) side by side, looped on the longest track's clock."""
    tracks = [t for t in tracks if t[1]]
    if not tracks:
        P2C_MISSING.append(os.path.basename(path))
        Image.new('RGB', (64, 16), (90, 20, 24)).save(path)
        return
    total = max(sum(t[2]) for t in tracks)
    step = 20
    frames = []
    w = sum(t[1][0].width * k + 8 for t in tracks)
    h = max(t[1][0].height * k for t in tracks) + 14
    for at in range(0, total, step):
        canvas = Image.new('RGB', (w, h), BG)
        x = 0
        for name, cells, ms in tracks:
            t, i = at % sum(ms), 0
            while t >= ms[i]:
                t -= ms[i]
                i += 1
            canvas.paste(scale(cells[i], k), (x, 14))
            label(canvas, name, x + 2, 1)
            x += cells[0].width * k + 8
        frames.append(canvas)
    frames[0].save(path, save_all=True, append_images=frames[1:], duration=step * slow, loop=0)


def p2c_strip_board(work, tag, gaits, k=4):
    rows = []
    for g in gaits:
        for f in 'DURL':
            cells, _ = p2c_frames(work, tag, g, f)
            if cells:
                rows.append(titled(scale(stack(cells, 2, vertical=False), k), f'{tag} {g} {f}'))
    return stack(rows, 6) if rows else p2c_load(work, f'limbs/{tag}_{gaits[0]}_D.png')


def p2c_hero_crop(img, size=140):
    w, h = img.size
    return img.crop((w // 2 - size, h // 2 - size, w // 2 + size, h // 2 + size // 2))


def p2c_table(work):
    lines = []
    for log in ('gates.log', 'arms.log'):
        path = os.path.join(work, log)
        if not os.path.exists(path):
            P2C_MISSING.append(log)
            continue
        for ln in open(path, encoding='utf-8', errors='replace').read().splitlines():
            if ln.startswith(('ok ', 'FAIL:', '  B10 slice', '  C4 ', '  C5 ', '  C6 ', 'doll:', 'doll-arms:')):
                lines.append(ln[:150])
    img = Image.new('RGB', (1000, 14 * max(1, len(lines)) + 24), BG)
    label(img, 'P2c SLICE: BUDGETS AND GATES (rpg_test --doll --gates --parity --report; --doll-arms --report)', 4, 4)
    for i, ln in enumerate(lines):
        ImageDraw.Draw(img).text((4, 20 + i * 14), ln, fill=(232, 96, 60) if ln.startswith('FAIL') else LABEL, font=FONT)
    return img


def p2c_assemble(work, out):
    os.makedirs(out, exist_ok=True)
    made = []

    def put(name, img):
        img.save(os.path.join(out, name))
        made.append(name)

    put('01_recut_parity_sheet.png', parity_sheet(work) if os.path.isdir(os.path.join(work, 'parity')) and os.listdir(os.path.join(work, 'parity'))
        else p2c_load(work, 'parity/hero_IDLE_D.png'))
    put('02_slice_lineup_D4.png', stack([titled(p2c_lineup(work, 'slice', 11), '11:00'), titled(p2c_lineup(work, 'slice', 22), '22:00')], 8, vertical=False))
    put('03_fit_pairs_D4.png', stack([titled(p2c_lineup(work, 'fit', 11), '11:00'), titled(p2c_lineup(work, 'fit', 22), '22:00')], 8, vertical=False))
    strips = [p2c_strip_board(work, t, ['walk', 'run'], 8) for t in ('guard', 'knight', 'archer', 'knightm', 'archerw')]
    put('04_side_walk_run_8x.png', stack(strips, 10))
    for tag in ('guard', 'knight', 'archer', 'knightm', 'archerw'):
        tracks = []
        for g in ('walk', 'run'):
            for f in 'RL':
                c, ms = p2c_frames(work, tag, g, f)
                tracks.append((f'{tag} {g} {f}', c, ms))
        p2c_gif(os.path.join(out, f'04_{tag}_walk_run_3x.gif'), tracks)
    for num, tag, gaits in (('05', 'guard', ['didle', 'hit1', 'block']), ('06', 'knight', ['stay', 'didle', 'hit1', 'block']), ('07', 'archer', ['didle', 'draw'])):
        board = p2c_strip_board(work, tag, gaits, 4)
        put(f'{num}_{tag}_actions.png', board)
        tracks = []
        for g in gaits:
            for f in 'RLDU':
                c, ms = p2c_frames(work, tag, g, f)
                tracks.append((f'{g} {f}', c, ms))
        p2c_gif(os.path.join(out, f'{num}_{tag}_actions_game_speed.gif'), tracks, 3, 1)
        p2c_gif(os.path.join(out, f'{num}_{tag}_actions_quarter_speed.gif'), tracks, 3, 4)
    put('08_falls_and_lying.png', stack([p2c_strip_board(work, t, ['fall', 'lying'], 4) for t in P2C_LOOKS], 10))
    pairs = []
    for t in P2C_LOOKS:
        r2 = p2c_load(work, f'p2c_slice_{t}_idle_down.png')
        r1 = p2c_load(work, f'p2c_slice_{t}_rig1.png')
        pairs.append(titled(stack([p2c_hero_crop(r1), p2c_hero_crop(r2)], 4, vertical=False), f'{t}: rig 1 (v1) | rig 2 (doll)'))
    put('09_rig1_beside_rig2.png', stack(pairs, 8))
    world = [titled(p2c_load(work, n), n) for n in ('p2c_fight_guard_d3.png', 'p2c_fight_guard_d4.png', 'p2c_fight_guard_night.png',
                                                    'p2c_fight_knight_run_east.png', 'p2c_fight_archer_draw.png', 'p2c_slice_phone_guard_near.png')]
    put('10_in_world.png', stack(world, 8))
    put('11_budgets_and_gates.png', p2c_table(work))
    return made


def p2c_main(a):
    out = os.path.abspath(a.out if a.out_set else os.path.join(os.environ.get('LOCALAPPDATA', '/tmp'), 'Temp', 'claude', 'emb_P2c', 'owner_pack'))
    work = os.path.join(out, 'work')
    if not a.assemble_only:
        p2c_capture(a.bdir, work)
    made = p2c_assemble(work, out)
    if not a.keep_work:
        shutil.rmtree(work, ignore_errors=True)
    miss = sorted(set(P2C_MISSING))
    print(f'owner pack (P2c): {len(made)} boards in {out}; {len(miss)} missing input(s)' + (': ' + ', '.join(miss[:12]) if miss else ''))
    return 0 if len(made) == 11 and not miss else 1


# ---- (P2c forge, FORGE-DOLL) python tools/doll/owner_pack.py --forge [--bdir build] [--pool FILE] [--out DIR]
#      [--assemble-only] [--keep-work]: the forge slice's owner pack (STYLE_P2 28.10), every image 8000 px or less on its long
#      side. Sources: doll_gallery's forge boards (forge, forgegrey, silhouette:forge, forge:t5 / alloys / woman / ladder,
#      metalread, families, alloystyles:<people>:<seed>), rpg_test --doll --limbs on tools/doll/forge_looks.txt (the walk
#      frames and GIFs), the gates (--doll --gates --parity --report, --doll-forge --report, --doll-coverage --scope forge) and
#      the in-world script tools/scripts/p2c_forge.txt (embervale.exe, when built). Default DIR
#      %LOCALAPPDATA%/Temp/claude/emb_P2c/forge/owner_pack. The boards:
#   01 the materials lineup (colour, dark ground, greyscale; the walk facing R at 3x)   02 the silhouettes
#   03 the T5 row          04 the alloys (each culture's alloys beside their metal word's style)
#   05 the ladder (and its walk GIF)      06 the metal read (the six names; every sheen's ramp)   07 the colour spread
#   08 the open items (T2 to T3, the T5 face, the Mythic crown, the blade) at 8x     09 in the world    10 the gates table
FORGE_MAX = 8000
FORGE_BOARDS = ['forge', 'forgegrey', 'silhouette:forge', 'forge:t5', 'forge:alloys', 'forge:woman', 'forge:ladder', 'metalread', 'families',
                'alloystyles:highland:1', 'alloystyles:jade:5', 'alloystyles:starspire:1', 'forge:lt2,lt3,f5_gilt,fm_myth']
FORGE_WORLD = ['p2c_forge_hero_d4_11.png', 'p2c_forge_hero_d4_22.png', 'p2c_forge_phone_d5_11.png', 'p2c_forge_captains_d4.png']


def forge_capture(bdir, work, pool):
    for d in ('', 'boards', 'limbs'):
        os.makedirs(os.path.join(work, d), exist_ok=True)
    slot = ['bash', os.path.join(REPO, 'tools', 'slot.sh')]
    gal, rt = os.path.join(REPO, bdir, 'doll_gallery.exe'), os.path.join(REPO, bdir, 'rpg_test.exe')
    pl = ['--pool', pool] if pool else []
    run(slot + [gal, os.path.join(work, 'boards')] + pl + FORGE_BOARDS, os.path.join(work, 'gallery.log'), cwd=REPO)
    run(slot + [rt, '--doll', '--limbs', os.path.join(work, 'limbs'), '--limbs-looks', os.path.join(REPO, 'tools', 'doll', 'forge_looks.txt')] + pl,
        os.path.join(work, 'limbs.log'), cwd=REPO)
    run(slot + [rt, '--doll', '--gates', '--parity', '--report'] + pl, os.path.join(work, 'gates.log'), cwd=REPO)
    run(slot + [rt, '--doll-forge', '--report', '--selftest'], os.path.join(work, 'forge.log'), cwd=REPO)
    run(slot + [rt, '--doll-coverage', '--scope', 'forge'] + pl, os.path.join(work, 'coverage.log'), cwd=REPO)
    exe = os.path.join(REPO, bdir, 'embervale.exe')
    if os.path.exists(exe):
        for sc in ('p2c_forge', 'p2c_forge_phone'):
            run(slot + [exe, '--script', os.path.join(REPO, 'tools', 'scripts', sc + '.txt')], os.path.join(work, sc + '.log'), cwd=work)


def forge_fit(img):
    """The largest whole-number shrink that brings the long side to FORGE_MAX or less (boards come at 3x: shrink by 3 to 1x)."""
    k = 1
    while max(img.width, img.height) / k > FORGE_MAX:
        k += 1
    return img if k == 1 else img.resize((img.width // k, img.height // k), Image.NEAREST)


def forge_grey(img):
    return img.convert('L').convert('RGB')


def forge_text(lines, w=1400):
    img = Image.new('RGB', (w, 14 * len(lines) + 8), BG)
    d = ImageDraw.Draw(img)
    for i, ln in enumerate(lines):
        d.text((4, 4 + 14 * i), ln, fill=(230, 120, 110) if ln.startswith('FAIL') else LABEL, font=FONT)
    return img


def forge_assemble(work, out):
    os.makedirs(out, exist_ok=True)
    made = []

    def put(name, img):
        img = forge_fit(img)
        assert max(img.width, img.height) <= FORGE_MAX
        img.save(os.path.join(out, name))
        made.append(name)

    B = lambda n: p2c_load(work, os.path.join('boards', n))
    fm = ['fm_home', 'fm_tan', 'fm_bronze', 'fm_iron', 'fm_steel', 'fm_gilt', 'fm_obsid', 'fm_ember', 'fm_star', 'fm_myth']
    walks = []
    for t in fm:
        c, ms = p2c_frames(work, t, 'walk', 'R')
        if c:
            row = stack([scale(x, 3) for x in c], 2, vertical=False)
            walks.append(titled(stack([row, forge_grey(row)], 2, vertical=False), t + ' walk R, colour | greyscale (3x)'))
    put('01_materials_lineup.png', stack([B('doll_forge_3x.png')] + walks, 8))
    put('01b_materials_lineup_1x.png', B('doll_forge_1x.png'))
    put('02_silhouettes.png', B('doll_silhouette_forge_3x.png'))
    put('03_t5_row.png', B('doll_forge_t5_3x.png'))
    put('04_alloys.png', stack([B('doll_forge_alloys_3x.png'), B('doll_alloystyles_highland_1_3x.png')], 8))
    put('04b_alloys_jade_starspire.png', stack([B('doll_alloystyles_jade_5_3x.png'), B('doll_alloystyles_starspire_1_3x.png')], 8))
    put('04c_woman.png', B('doll_forge_woman_3x.png'))
    put('05_ladder.png', B('doll_forge_ladder_3x.png'))
    tracks = []
    for t in ['lt1', 'lt2', 'lt3', 'lt4', 'lt5', 'lt6', 'lt6m']:
        c, ms = p2c_frames(work, t, 'walk', 'R')
        tracks.append((t, c, ms))
    p2c_gif(os.path.join(out, '05b_ladder_walk_3x.gif'), tracks, 3, 1)
    made.append('05b_ladder_walk_3x.gif')
    put('06_metal_read.png', B('doll_forge_metalread_3x.png'))
    put('07_colour_spread.png', B('doll_families_3x.png'))
    put('08_open_items.png', B('doll_forge_lt2,lt3,f5_gilt,fm_myth_8x.png'))
    world = [titled(p2c_load(work, n), n) for n in FORGE_WORLD]
    put('09_in_world.png', stack(world, 8))
    lines = []
    for lg in ('gates.log', 'forge.log', 'coverage.log'):
        p = os.path.join(work, lg)
        if os.path.exists(p):
            lines += [lg + ':'] + [ln.rstrip() for ln in open(p, encoding='utf-8', errors='replace') if ln.startswith(('ok', 'FAIL', '  pool', 'doll', '  C16 C17'))]
        else:
            P2C_MISSING.append(lg)
    put('10_gates.png', forge_text(lines))
    return made


def forge_main(a):
    out = os.path.abspath(a.out if a.out_set else os.path.join(os.environ.get('LOCALAPPDATA', '/tmp'), 'Temp', 'claude', 'emb_P2c', 'forge', 'owner_pack'))
    work = os.path.join(out, 'work')
    if not a.assemble_only:
        forge_capture(a.bdir, work, a.pool)
    made = forge_assemble(work, out)
    if not a.keep_work:
        shutil.rmtree(work, ignore_errors=True)
    miss = sorted(set(P2C_MISSING))
    print(f'owner pack (forge): {len(made)} files in {out}; {len(miss)} missing input(s)' + (': ' + ', '.join(miss[:12]) if miss else ''))
    return 0 if not miss else 1


def main():
    ap = argparse.ArgumentParser(description='the P2b owner pack from the game (STYLE_P2 19 item 6)')
    ap.add_argument('--forge', action='store_true', help='the P2c forge slice pack (STYLE_P2 28.10)')
    ap.add_argument('--pool', default='', help='(--forge) an isolated pool for the boards and gates')
    ap.add_argument('--bdir', default='build', help='the build directory (embervale.exe and rpg_test.exe), relative to the repo')
    ap.add_argument('--out', default=os.path.join(os.environ.get('LOCALAPPDATA', '/tmp'), 'Temp', 'claude', 'emb_P2b', 'owner_pack'))
    ap.add_argument('--assemble-only', action='store_true', help='reuse DIR/work (no game runs)')
    ap.add_argument('--keep-work', action='store_true', help='keep DIR/work (the raw shots and captures)')
    ap.add_argument('--p2c', action='store_true', help='the P2c style slice pack (STYLE_P2 26): 11 boards')
    a = ap.parse_args()
    a.out_set = any(x == '--out' or x.startswith('--out=') for x in sys.argv[1:])
    if a.p2c:
        return p2c_main(a)
    if a.forge:
        return forge_main(a)
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
