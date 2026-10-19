"""build.py: every A3 deliverable (take A's proportions, A2's rendering) from the hand-placed maps.

  sheet.png         4x: idles in 4 facings, walks in 4 facings (6 frames), the swing right and down (4 keys), ramps
  anim_walks.gif    4x, game timing: hero at 74 px/s (the 28 px cycle: 70/50/70 ms per contact/down/pass), the woman at
                    a town stroll of ~18 px/s (folk speed 40 x 0.45-0.5: 270/220/270 ms); one loop = 4 hero cycles
  anim_swing.gif    4x, game timing (SkillOps sword pace 1.0: windup 70 ms, strike 110, recover 130 split in two)
  anim_swing_slow.gif  the same at 1/4 speed, for reading the poses
  scene.png         desktop capture, D=3 (480 x 270 world px), cast composited at 1x then scaled 3x
  scene_phone.png   phone capture, D=5
  scene_zoom.png    2x crop of the desktop scene around the figures; scene_phone_zoom.png likewise
  before_after.png  A's hero and woman next to A2's at 8x
"""
import sys
sys.dont_write_bytecode = True
from PIL import Image, ImageDraw, ImageFont
import numpy as np
import hero, woman, swing
import pal as P
from pixlib import render, upscale, grid

FONT_B = 'C:/Windows/Fonts/consolab.ttf'
FONT = 'C:/Windows/Fonts/consola.ttf'

# ------------------------------------------------------------------ palette swaps (same maps, other ramps)
SWAP_TRAVELLER = dict(P.HERO, **{    # black hair, deeper skin, rust tunic, mustard scarf
    'a': '#140C18', 'b': '#2C2232', 'c': '#45384C', 'd': '#67566E', 'e': '#94829C',
    'q': '#46202A', 'r': '#8A4C3A', 's': '#B97856', 't': '#D69A72', 'u': '#EEC096',
    '5': '#26100E', '6': '#54261A', '7': '#844024', '8': '#B26634', '9': '#DA9658',
    'm': '#3A2808', 'n': '#7A5814', 'o': '#B88E1E', 'p': '#E8C44A', 'E': '#4A3020', 'G': '#8A6040',
})
SWAP_GREEN = dict(P.HERO, **{        # ash-blond lad in a moss tunic, blue kerchief
    'a': '#3A2A1A', 'b': '#7A5E36', 'c': '#B08E58', 'd': '#D8BC84', 'e': '#F4E2B0',
    '5': '#18240E', '6': '#2E4418', '7': '#4A6A24', '8': '#6E9234', '9': '#A8C860',
    'm': '#141C3A', 'n': '#24366A', 'o': '#3A5AA4', 'p': '#6E92D8', 'E': '#3A6A3A', 'G': '#88C088',
})
SWAP_MIRA = dict(P.WOMAN, **{        # raven hair, deep skin, blue dress
    'a': '#150E1B', 'b': '#2F2336', 'c': '#4B394B', 'd': '#6F566B', 'e': '#9A829C',
    'q': '#3E1C20', 'r': '#7C4432', 's': '#A86A4A', 't': '#C98C64', 'u': '#E2AE84',
    '5': '#101830', '6': '#1E3062', '7': '#2E4E94', '8': '#4A74C2', '9': '#86A8E2',
    'Y': '#C8705C', 'L': '#B85456', 'M': '#6A2A2C', 'E': '#5A3420', 'G': '#A0704A', 'R': '#C83E38', 'S': '#F47650',
})
SWAP_HEDDA = dict(P.WOMAN, **{       # auburn hair, ochre dress, white apron
    'a': '#2E0E10', 'b': '#6A1E1A', 'c': '#9E3422', 'd': '#CC5A30', 'e': '#F08E50',
    '5': '#2E1A08', '6': '#6A4012', '7': '#A06A1E', '8': '#CC9A34', '9': '#F0CC6A',
    'A': '#4A4248', 'B': '#A8A2AC', 'C': '#DAD6DC', 'D': '#F6F4F0', 'E': '#3C70C4', 'G': '#8CB8EC',
})

# ------------------------------------------------------------------ the renderer-style contact shadow
SHADOW = grid("""
...xxxxxxxx...
.xxXXXXXXXXxx.
xxXXXXXXXXXXxx
.xxXXXXXXXXxx.
...xxxxxxxx...
""", 'shadow')
SH_MUL = {'x': 0.80, 'X': 0.66}


def shade(img, cx, cy):
    a = np.array(img).astype(float)
    h, w = len(SHADOW), len(SHADOW[0])
    x0, y0 = cx - w // 2, cy - h // 2
    for y, r in enumerate(SHADOW):
        for x, ch in enumerate(r):
            if ch in SH_MUL:
                X, Y = x0 + x, y0 + y
                if 0 <= X < a.shape[1] and 0 <= Y < a.shape[0]:
                    a[Y, X, :3] *= SH_MUL[ch]
    return Image.fromarray(a.clip(0, 255).astype('uint8'), 'RGBA')


GRASS = (140, 142, 80, 255)
BG = (30, 28, 38, 255)
K = 4


def cell(img, big=False):
    t = Image.new('RGBA', img.size, GRASS)
    if img.width == 44:
        t = shade(t, 22, 42)
    else:
        t = shade(t, 24 if big else 12, 42 if big else 34)
    t.alpha_composite(img)
    return t


# ------------------------------------------------------------------ sheet
def sheet():
    H_ = lambda f: render(f, hero.PAL)
    W_ = lambda f: render(f, woman.PAL)
    S_ = lambda f: render(f, swing.PAL)
    rows = [
        [('HERO  idle: down, up, left, right', [H_(hero.FRAMES[f'IDLE_{d}']) for d in 'DULR']),
         ('WOMAN  idle: down, up, left, right', [W_(woman.FRAMES[f'IDLE_{d}']) for d in 'DULR']),
         ('VILLAGERS: the same maps, other ramps', [render(hero.IDLE_D, SWAP_TRAVELLER), render(hero.IDLE_D, SWAP_GREEN),
                                                    render(woman.IDLE_D, SWAP_MIRA), render(woman.IDLE_D, SWAP_HEDDA)])],
        [('HERO  walk down: contact, down, pass x2', [H_(f) for f in hero.WALK_D]),
         ('HERO  walk up', [H_(f) for f in hero.WALK_U])],
        [('HERO  walk left (painted for top-left light)', [H_(f) for f in hero.WALK_L]),
         ('HERO  walk right', [H_(f) for f in hero.WALK_R])],
        [('WOMAN  walk down', [W_(f) for f in woman.WALK_D]),
         ('WOMAN  walk up', [W_(f) for f in woman.WALK_U])],
        [('WOMAN  walk left (braid on the near side)', [W_(f) for f in woman.WALK_L]),
         ('WOMAN  walk right', [W_(f) for f in woman.WALK_R])],
        [('HERO  sword, facing right: wind-up, strike, follow-through, recover', [S_(f) for f in swing.SWING_R])],
        [('HERO  sword, facing down: wind-up, strike, follow-through, recover', [S_(f) for f in swing.SWING_D])],
    ]
    M, G, GAP, LBL = 8, 4, 14, 10
    placed, labels = [], []
    y = M
    widest = 0
    for row in rows:
        x = M
        rh = 0
        for title, imgs in row:
            labels.append((x, y, title))
            cx = x
            for im in imgs:
                placed.append((cx, y + LBL, cell(im, im.width >= 44)))
                cx += im.width + G
                rh = max(rh, im.height)
            x = cx - G + GAP
        widest = max(widest, x)
        y += LBL + rh + 8
    # the ramps, to the right of the two swing rows
    sw_rows_top = y - 2 * (LBL + 48 + 8)
    px0 = M + 4 * (48 + G) + GAP + 8
    labels.append((px0, sw_rows_top, 'RAMPS  outline > shadow > base > light > highlight'))
    ramp_draw = []
    yy = sw_rows_top + LBL + 2
    for who, pal_, ramps in (('hero', P.HERO, P.RAMPS_HERO), ('woman', P.WOMAN, P.RAMPS_WOMAN)):
        labels.append((px0, yy, who))
        yy += 8
        col = 0
        for name, keys in ramps:
            rx = px0 + (col % 2) * 46
            ry = yy + (col // 2) * 9
            for j, kch in enumerate(keys):
                ramp_draw.append((rx + j * 6, ry, pal_[kch]))
            labels.append((rx + len(keys) * 6 + 1, ry - 1, name, 'small'))
            col += 1
        yy += ((col + 1) // 2) * 9 + 4
    Wd = max(widest, px0 + 100)
    Hd = max(y, yy) + M + 6
    base = Image.new('RGBA', (Wd, Hd), BG)
    for cx, cy, im in placed:
        base.alpha_composite(im, (cx, cy))
    d0 = ImageDraw.Draw(base)
    for rx, ry, hx in ramp_draw:
        d0.rectangle([rx, ry, rx + 4, ry + 6], fill=hx)
    big = upscale(base, K)
    d = ImageDraw.Draw(big)
    f = ImageFont.truetype(FONT_B, 15)
    fs = ImageFont.truetype(FONT, 12)
    for lab in labels:
        x, yy_, t = lab[:3]
        small = len(lab) > 3
        d.text((x * K, yy_ * K + (6 if not small else 4)), t, font=fs if small else f,
               fill=(200, 192, 176, 255) if small else (238, 228, 204, 255))
    d.text((M * K, (Hd - 6) * K), "Art study A3: take A's proportions and silhouettes, A2's rendering (faces, ramps, folds, selective "
           "outlines). 4x nearest on the game grass with the renderer-style contact shadow. Arc FX: A/B/C ramp.",
           font=fs, fill=(150, 144, 160, 255))
    big.save('sheet.png')
    print('sheet', big.size)


# ------------------------------------------------------------------ gifs
def gif_seqs(seqs, path, k=4, total=None, cols=None, labels=None):
    """seqs: list of (frames(images), durations ms). Each sequence loops on its own; total = the GIF's loop length (a
    common multiple of the cycles). Frames are laid out in a grid of `cols`."""
    total = total or sum(seqs[0][1])
    cuts = set([0])
    for _, durs in seqs:
        cyc = sum(durs)
        t = 0
        while t < total:
            for dd in durs:
                cuts.add(t)
                t += dd
    cuts = sorted(c for c in cuts if c < total) + [total]
    cw = max(max(i.width for i in fr) for fr, _ in seqs)
    ch = max(max(i.height for i in fr) for fr, _ in seqs)
    cols = cols or len(seqs)
    nrows = (len(seqs) + cols - 1) // cols
    gap = 4
    LB = 14 if labels else 0
    Wd, Hd = cols * (cw * k + gap) + gap, nrows * (ch * k + gap + LB) + gap
    font = ImageFont.truetype(FONT, 12)
    out, durs_out = [], []
    for a, b in zip(cuts, cuts[1:]):
        fr = Image.new('RGBA', (Wd, Hd), BG)
        dr = ImageDraw.Draw(fr)
        for i, (frames, durs) in enumerate(seqs):
            cyc = sum(durs)
            t = a % cyc
            idx, acc = 0, 0
            for j, dd in enumerate(durs):
                if acc <= t < acc + dd:
                    idx = j
                    break
                acc += dd
            im = frames[idx]
            cx = gap + (i % cols) * (cw * k + gap)
            cy = gap + (i // cols) * (ch * k + gap + LB)
            tile = Image.new('RGBA', (cw, ch), GRASS)
            tile = shade(tile, cw // 2, ch - 2 if ch == 36 else 42)
            tile.alpha_composite(im, ((cw - im.width) // 2, ch - im.height - (2 if im.height == 44 else 0)))
            fr.alpha_composite(upscale(tile, k), (cx, cy + LB))
            if labels:
                dr.text((cx + 2, cy), labels[i], font=font, fill=(230, 220, 200, 255))
        out.append(fr.convert('RGB'))
        durs_out.append(b - a)
    # one shared palette keeps the frames from flickering
    pal_src = Image.new('RGB', (Wd, Hd * len(out[:24])))
    for i, im in enumerate(out[:24]):
        pal_src.paste(im, (0, i * Hd))
    q = pal_src.quantize(colors=255, method=Image.Quantize.MEDIANCUT)
    outp = [im.quantize(palette=q, dither=Image.Dither.NONE) for im in out]
    outp[0].save(path, save_all=True, append_images=outp[1:], duration=durs_out, loop=0, disposal=1, optimize=False)
    print(path, len(outp), 'frames', sum(durs_out), 'ms')


HERO_WALK_MS = [70, 50, 70, 70, 50, 70]          # 380 ms: 28 px at ~74 px/s
WOMAN_WALK_MS = [260, 240, 260, 260, 240, 260]   # 1520 ms: 28 px at ~18 px/s (cuts snapped to the hero's,
                                                 # so no GIF frame is shorter than 50 ms)


def gifs():
    H_ = lambda f: render(f, hero.PAL)
    W_ = lambda f: render(f, woman.PAL)
    seqs, labs = [], []
    for d, name in zip('DULR', ('down', 'up', 'left', 'right')):
        seqs.append(([H_(f) for f in getattr(hero, 'WALK_' + d)], HERO_WALK_MS))
        labs.append('hero ' + name)
    for d, name in zip('DULR', ('down', 'up', 'left', 'right')):
        seqs.append(([W_(f) for f in getattr(woman, 'WALK_' + d)], WOMAN_WALK_MS))
        labs.append('woman ' + name)
    gif_seqs(seqs, 'anim_walks.gif', k=4, total=1520, cols=4, labels=labs)
    S_ = lambda f: render(f, swing.PAL)
    idle_r = Image.new('RGBA', (44, 44)); idle_r.alpha_composite(H_(hero.IDLE_R), (10, 8))
    idle_d = Image.new('RGBA', (48, 48)); idle_d.alpha_composite(H_(hero.IDLE_D), (12, 8))
    ms = [70, 110, 70, 60, 640]                      # windup, strike, follow, recover, then standing
    seqs = [([S_(f) for f in swing.SWING_R] + [idle_r], ms), ([S_(f) for f in swing.SWING_D] + [idle_d], ms)]
    gif_seqs(seqs, 'anim_swing.gif', k=4, total=sum(ms), cols=2, labels=['swing right', 'swing down'])
    ms4 = [280, 440, 280, 240, 760]
    seqs = [([S_(f) for f in swing.SWING_R] + [idle_r], ms4), ([S_(f) for f in swing.SWING_D] + [idle_d], ms4)]
    gif_seqs(seqs, 'anim_swing_slow.gif', k=4, total=sum(ms4), cols=2, labels=['swing right (1/4 speed)',
                                                                               'swing down (1/4 speed)'])


# ------------------------------------------------------------------ scenes
def world_of(path, D, ox, oy):
    a = np.array(Image.open(path).convert('RGBA'))
    return Image.fromarray(np.ascontiguousarray(a[oy::D, ox::D]), 'RGBA'), a


# (palette, frame, feet x, feet y in DESKTOP world px); the frame is a 24 x 36 box with its sole on row 34
CAST = [
    (hero.PAL, hero.IDLE_R, 212, 118),              # the hero, turned to the woman
    (woman.PAL, woman.IDLE_L, 234, 118),            # the woman, facing him
    (SWAP_HEDDA, woman.IDLE_U, 270, 104),           # a villager at the stall, her back to us
    (SWAP_TRAVELLER, hero.WALK_D[0], 322, 112),     # a traveller walking down
    (SWAP_MIRA, woman.WALK_R[0], 250, 205),         # a villager walking right below the well
    (SWAP_GREEN, hero.WALK_L[3], 380, 150),         # a lad walking left past the cart
]


def compose(world, dx, dy):
    w = world.copy()
    for pal_, frame, fx, fy in sorted(CAST, key=lambda c: c[3]):
        fx, fy = fx + dx, fy + dy
        w = shade(w, fx, fy)
        w.alpha_composite(render(frame, pal_), (fx - 12, fy - 34))
    return w


def scene():
    wd, _ = world_of('world_r2.png', 3, 0, 0)
    out = compose(wd, 0, 0)
    upscale(out, 3).save('scene.png')
    sc = Image.open('scene.png')
    sc.crop((540, 210, 1060, 650)).resize((1040, 880), Image.NEAREST).save('scene_zoom.png')
    wp, raw = world_of('phone_r2.png', 5, 3, 4)
    A = np.array(wd.convert('L')).astype(float)
    B = np.array(wp.convert('L')).astype(float)
    patch = A[140:180, 200:250]
    best = None
    for oy in range(0, B.shape[0] - 40):
        for ox in range(0, B.shape[1] - 50):
            s = np.mean(np.abs(B[oy:oy + 40, ox:ox + 50] - patch))
            if best is None or s < best[0]:
                best = (s, ox - 200, oy - 140)
    outp = compose(wp, best[1], best[2])
    dev = Image.fromarray(raw, 'RGBA')
    dev.alpha_composite(upscale(outp, 5), (3, 4))
    dev.save('scene_phone.png')
    cx, cy = (223 + best[1]) * 5 + 3, (100 + best[2]) * 5 + 4
    dev.crop((cx - 300, cy - 230, cx + 300, cy + 230)).resize((1200, 920), Image.NEAREST).save('scene_phone_zoom.png')
    print('scenes ok, phone offset', best[1:])


# ------------------------------------------------------------------ before / after
def load_a():
    import importlib.util

    def load(name, path):
        sys.path.insert(0, 'orig_a')
        spec = importlib.util.spec_from_file_location(name, path)
        m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(m)
        sys.path.pop(0)
        return m
    return load('a_hero', 'orig_a/hero.py'), load('a_woman', 'orig_a/woman.py')


def before_after():
    ah, aw = load_a()
    pairs = []
    for who, am, nm in (('hero', ah, hero), ('woman', aw, woman)):
        for d, name in (('D', 'front'), ('R', 'right'), ('L', 'left')):
            pairs.append((f'A  {who} {name}', render(getattr(am, 'IDLE_' + d), am.PAL)))
            pairs.append((f'A3  {who} {name}', render(getattr(nm, 'IDLE_' + d), nm.PAL)))
    k = 8
    cw, chh = 24 * k, 36 * k
    gap, LB = 10, 22
    cols = 6
    Wd = cols * (cw + gap) + gap
    Hd = 2 * (chh + gap + LB) + gap + 54
    out = Image.new('RGBA', (Wd, Hd), BG)
    d = ImageDraw.Draw(out)
    f = ImageFont.truetype(FONT_B, 16)
    fs = ImageFont.truetype(FONT, 13)
    for i, (lab, im) in enumerate(pairs):
        x = gap + (i % cols) * (cw + gap)
        y = gap + (i // cols) * (chh + gap + LB)
        tile = shade(Image.new('RGBA', (24, 36), GRASS), 12, 34)
        tile.alpha_composite(im, (0, 0))
        out.alpha_composite(upscale(tile, k), (x, y + LB))
        d.text((x, y + 2), lab, font=f, fill=(238, 228, 204, 255) if 'A3' in lab else (170, 164, 178, 255))
        t1 = Image.new('RGBA', (24 * 3 + 6, 36 * 2 + 4), GRASS)
        t1.alpha_composite(im, (2, 2))
        t1.alpha_composite(upscale(im, 2), (28, 2))
        out.alpha_composite(t1, (x + cw - t1.width, y + LB + chh - t1.height))
    d.text((gap, Hd - 46), "Same outline as A, pixel for pixel (see notes.md for the silhouette diff). Inside it: eye whites, "
           "iris + glint, softened brows, a 1 px mouth and a clean chin; A2's hue-shifted ramps; boot shine;", font=fs,
           fill=(200, 192, 176, 255))
    d.text((gap, Hd - 28), "coloured selective outlines lit on the top-left edges; solid hem trims. 8x nearest; 1x and 2x in "
           "each corner.", font=fs, fill=(200, 192, 176, 255))
    out.save('before_after.png')
    print('before_after', out.size)


def silhouettes():
    """Overlay A3's idle frames on A's: the outer silhouette (alpha) must match. Writes work/silhouette_diff.png."""
    ah, aw = load_a()
    lines = []
    tiles = []
    for who, am, nm in (('hero', ah, hero), ('woman', aw, woman)):
        for d in 'DRUL':
            a = getattr(am, 'IDLE_' + d)
            b = getattr(nm, 'IDLE_' + d)
            diff = [(x, y) for y in range(36) for x in range(24) if (a[y][x] == '.') != (b[y][x] == '.')]
            area = sum(1 for y in range(36) for x in range(24) if a[y][x] != '.')
            lines.append(f'{who} IDLE_{d}: {len(diff)} px differ of {area} (A silhouette) {diff}')
            im = Image.new('RGBA', (24, 36), (40, 38, 48, 255))
            px = im.load()
            for y in range(36):
                for x in range(24):
                    ia, ib = a[y][x] != '.', b[y][x] != '.'
                    px[x, y] = (200, 200, 200, 255) if ia and ib else (255, 60, 60, 255) if ia else                         (60, 200, 255, 255) if ib else (40, 38, 48, 255)
            tiles.append(upscale(im, 6))
    out = Image.new('RGBA', (len(tiles) * (24 * 6 + 6), 36 * 6), (20, 20, 26, 255))
    for i, t in enumerate(tiles):
        out.alpha_composite(t, (i * (24 * 6 + 6), 0))
    out.save('work/silhouette_diff.png')
    print(chr(10).join(lines))


if __name__ == '__main__':
    what = sys.argv[1:] or ['sheet', 'gifs', 'scene', 'before_after', 'silhouettes']
    for w in what:
        globals()[w]()
