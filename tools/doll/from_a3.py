#!/usr/bin/env python3
"""tools/doll/from_a3.py: .pxm drafts of art study A3's named parts, a draft clip table, and a reference model of the doll
that proves them (EMBERVALE P2b phase A, VISION_PLAN 15.36 item 1; the format is docs/STYLE_P2.md section 24 and
rpg/art/doll/doll.h "The .pxm format").

    python tools/doll/from_a3.py [--out DIR] [--verify] [--quiet]

Reads the locked reference docs/ref/style_a3 (hero.py, woman.py, swing.py: the maps exactly as A3 composed them) and
writes, under DIR (default rpg/art/doll/drafts/a3):
    maps/a3/hero/*.pxm, maps/a3/woman/*.pxm   one part per file, the views and keys A3 composed its frames from
    maps/legend/person.legend, woman.legend   copies of the legends the drafts use (the live ones: rpg/art/doll/maps/legend)
    clips/a3.clip                             the frames: idle and walk in four facings for both, the sword swing R and D
A draft is a STARTING POINT (STYLE_P2 24 "Drafts"): FOLK-BODY copies what it keeps into rpg/art/doll/maps/ for the slice,
splits it into the doll's parts (head, hair, torso, hang, arms with regions...), re-registers the side walks and hand-checks
every map. The drafts carry `tags draft a3` and roles that the A3 looks resolve to, so the whole pipeline can be proved on
them before a single production map exists:

    node tools/doll/pxc.mjs --root rpg/art/doll/drafts/a3 --bin <scratch>/a3.pool
    rpg_test --doll --parity --pool <scratch>/a3.pool          (48 frames, 0 px differing)

--verify composes every A3 frame from the drafts with the reference model below (the same placement, layer order, erase,
sway and finishing rules the C++ doll implements: rpg/art/doll/doll_compose.cpp) and compares it with A3's own frames
(all 64, as (slot, step) through the legends), then exits non-zero on any difference. Run it after editing this file.
"""
import argparse
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, '..', '..'))
REF = os.path.join(REPO, 'docs', 'ref', 'style_a3')
sys.dont_write_bytecode = True
sys.path.insert(0, REF)

import hero  # noqa: E402  (the finished module: swing.py composes on it, as A3 did)
import woman  # noqa: E402
import swing  # noqa: E402


def load_raw(modname):
    """The module's grids BEFORE its finishing pass (A3's modules overwrite their idles with the finished frames at the
    end): the same source run without the trailing '# the finishing pass' block."""
    src = open(os.path.join(REF, modname + '.py'), encoding='utf-8').read()
    cut = src.index('# the finishing pass')
    ns = {'__name__': modname + '_raw', '__file__': os.path.join(REF, modname + '.py')}
    exec(compile(src[:cut], modname + '_raw', 'exec'), ns)
    return ns


HR = load_raw('hero')     # hero raw
WR = load_raw('woman')    # woman raw

# ---------------------------------------------------------------------------------------------------- legends
# char -> (slot, step). person.legend keeps A3's hero characters for their slots (VISION_PLAN 15.36); woman.legend is
# used on top of it for the woman's chemise (linen), apron (cloth 3), ribbon (accent) and lip tint.
PERSON = {
    'K': ('ink', 0), '#': ('face', 0), '@': ('face', 1), 'M': ('face', 2), 'L': ('face', 3), 'Y': ('face', 4),
    'E': ('eyes', 2), 'G': ('eyes', 3),
    'a': ('hair', 0), 'b': ('hair', 1), 'c': ('hair', 2), 'd': ('hair', 3), 'e': ('hair', 4),
    'q': ('skin', 0), 'r': ('skin', 1), 's': ('skin', 2), 't': ('skin', 3), 'u': ('skin', 4),
    '5': ('cloth1', 0), '6': ('cloth1', 1), '7': ('cloth1', 2), '8': ('cloth1', 3), '9': ('cloth1', 4),
    '1': ('cloth2', 0), '2': ('cloth2', 1), '3': ('cloth2', 2), '4': ('cloth2', 3),
    'm': ('accent', 0), 'n': ('accent', 1), 'o': ('accent', 2), 'p': ('accent', 3),
    'f': ('leather', 0), 'g': ('leather', 1), 'h': ('leather', 2), 'i': ('leather', 3), 'j': ('leather', 4),
    'v': ('metal', 0), 'w': ('metal', 1), 'x': ('metal', 2), 'y': ('metal', 3), 'z': ('metal', 4),
    'N': ('metal2', 1), 'O': ('metal2', 2), 'P': ('metal2', 3),
    'C': ('fx', 0), 'B': ('fx', 1), 'A': ('fx', 2),
}
WOMAN_EXTRA = {
    'v': ('linen', 0), 'w': ('linen', 1), 'x': ('linen', 2), 'y': ('linen', 3),
    'A': ('cloth3', 0), 'B': ('cloth3', 1), 'C': ('cloth3', 2), 'D': ('cloth3', 3),
    'R': ('accent', 1), 'S': ('accent', 3),
}
WOMAN_LEG = dict(PERSON, **WOMAN_EXTRA)
# the finishing pass lights step 0 to step 1 on every slot but these (STYLE_P2 8: Ink only on the sole line; nothing
# outlined inside the face; FX carry no light)
UNLIT = {'ink', 'face', 'eyes', 'fx'}

# ---------------------------------------------------------------------------------------------------- the doll's tables
LAYERS = ['fxBack', 'heldBack', 'back', 'cloakBack', 'hairBack', 'armFar', 'legs', 'shoes', 'hangBack', 'torso',
          'hangFront', 'belt', 'neck', 'armNear', 'head', 'face', 'beard', 'hairFront', 'ears', 'headwear', 'held',
          'shield', 'cloakFront', 'fx']
FACINGS = 'DURL'
# base anchors, pivot-relative (the draft clip writes these; FOLK-BODY's clips/loco.clip owns the production numbers)
ANCHORS = {
    'man': {f: {'pivot': (0, 0), 'head': (-8, -33), 'neck': (0, -19), 'hip': (0, -10),
                'shoulderR': (-7, -18) if f == 'D' else (6, -18) if f == 'U' else (-1, -18),
                'shoulderL': (6, -18) if f == 'D' else (-7, -18) if f == 'U' else (-1, -18)} for f in FACINGS},
}
ANCHORS['woman'] = {f: dict(a, head=(-8, -32)) for f, a in ANCHORS['man'].items()}


def delta(anchor, T):
    """The frame's move of an anchor (rpg/art/doll/doll.h "clip frames"): bob moves every anchor but the pivot; lean
    moves the neck, head and shoulders; hip= and head= add their own offsets."""
    if anchor == 'pivot':
        return 0, 0
    dx, dy = 0, T.get('bob', 0)
    if anchor in ('neck', 'head', 'shoulderR', 'shoulderL'):
        dx += T.get('lean', 0)
    if anchor == 'hip':
        hx, hy = T.get('hip', (0, 0))
        dx, dy = dx + hx, dy + hy
    if anchor == 'head':
        hx, hy = T.get('head', (0, 0))
        dx, dy = dx + hx, dy + hy
    return dx, dy


# ---------------------------------------------------------------------------------------------------- drafts
class View:
    def __init__(self, rows, origin, layer=None, anchor=None):
        self.rows, self.origin, self.layer, self.anchor = rows, origin, layer, anchor
        self.marks = {}


class Part:
    def __init__(self, name, layer, anchor, channel, legend, bodies, roles, side=None, sway=0, z=0):
        self.name, self.layer, self.anchor, self.channel = name, layer, anchor, channel
        self.legend, self.bodies, self.roles, self.side, self.sway, self.z = legend, bodies, roles, side, sway, z
        self.views = {}          # (facing, key) -> View
        self.order = 0


PARTS = {}
CLIPS = []                       # (body, clip, facing, part, family, variant, [frame dicts])


def part(name, layer, anchor, channel, legend, bodies, roles, **kw):
    if name not in PARTS:
        p = Part(name, layer, anchor, channel, legend, bodies, roles, **kw)
        p.order = len(PARTS)
        PARTS[name] = p
    return PARTS[name]


def trim(rows):
    """The grid cut to its box of non-'.' cells (erase marks count): (rows, x0, y0); None when empty."""
    ys = [y for y, r in enumerate(rows) if r.strip('.')]
    if not ys:
        return None
    xs = [x for r in rows for x, c in enumerate(r) if c != '.']
    x0, x1, y0, y1 = min(xs), max(xs), ys[0], ys[-1]
    return [r[x0:x1 + 1].ljust(x1 - x0 + 1, '.') for r in rows[y0:y1 + 1]], x0, y0


def norm(rows):
    rows = [r.replace(' ', '.') for r in rows]
    w = max(len(r) for r in rows)
    return [r.ljust(w, '.') for r in rows]


class Frame:
    """One composed frame: a canvas (w, h) with the pivot at (px, py), the transforms, and the placed grids."""
    def __init__(self, body, w, h, px, py, **T):
        self.body, self.w, self.h, self.px, self.py, self.T = body, w, h, px, py, T
        self.keys = {}           # channel -> key ('-' = nothing)
        self.uses = []           # (part, facing, key)

    def put(self, p, facing, key, rows, ox, oy, layer=None, anchor=None):
        """Place a grid at canvas (ox, oy) as part p's (facing, key) view: the view's origin is where its anchor lands."""
        t = trim(norm(rows))
        if t is None:
            return
        grid, x0, y0 = t
        a = anchor or p.anchor
        bx, by = ANCHORS[self.body][facing][a]
        dx, dy = delta(a, self.T)
        ax, ay = self.px + bx + dx, self.py + by + dy
        origin = (ax - (ox + x0), ay - (oy + y0))
        v = p.views.get((facing, key))
        if v is None:
            p.views[(facing, key)] = View(grid, origin, layer, anchor)
        elif v.rows != grid or v.origin != origin or v.layer != layer or v.anchor != anchor:
            raise SystemExit(f'from_a3: {p.name} {facing}/{key} is used with two different grids or origins '
                             f'({v.origin} vs {origin}): give one a key of its own')
        if p.channel != 'rest':
            prev = self.keys.get(p.channel)
            if prev not in (None, key):
                raise SystemExit(f'from_a3: channel {p.channel} asked for {prev} and {key} in one frame')
            self.keys[p.channel] = key
        self.uses.append((p, facing, key))


def diff_overlay(base, new):
    """An overlay that turns base into new: new's char where they differ, '-' where new is transparent."""
    h = max(len(base), len(new))
    base = norm(base + ['.'] * (h - len(base)))
    new = norm(new + ['.'] * (h - len(new)))
    w = max(len(base[0]), len(new[0]))
    out = []
    for y in range(h):
        b, n = base[y].ljust(w, '.'), new[y].ljust(w, '.')
        out.append(''.join(('-' if nc == '.' else nc) if nc != bc else '.' for bc, nc in zip(b, n)))
    return out


def cols(rows, x0, x1):
    return [''.join(ch if x0 <= x <= x1 else '.' for x, ch in enumerate(r)) for r in rows]


def minus(rows, over, oy=0):
    """rows with '.' wherever `over` (placed oy rows down) is opaque."""
    out = [list(r) for r in rows]
    for y, r in enumerate(over):
        for x, c in enumerate(r):
            if c not in '.-' and 0 <= y + oy < len(out) and x < len(out[y + oy]):
                out[y + oy][x] = '.'
    return [''.join(r) for r in out]


FRAMES = {}    # 'hero_WALK_D3' -> (Frame, reference rows, legend)

# ---------------------------------------------------------------------------------------------------- the hero
HROLES_HEAD = [('head', 'oval'), ('hair', 'tousled'), ('nose', 'button'), ('brow', 'straight'), ('ears', 'human')]
hb = ('man',)
h_head = part('a3/hero/head', 'head', 'head', 'rest', 'person', hb, HROLES_HEAD)
h_torso = part('a3/hero/torso', 'torso', 'neck', 'torso', 'person', hb, [('garment', 'tunic'), ('neck', 'scarf')])
h_legs = part('a3/hero/legs', 'legs', 'pivot', 'legs', 'person', hb, [('legs', 'std')])
h_sheath = part('a3/hero/sheath', 'belt', 'hip', 'sheath', 'person', hb, [('sheath', 'sword')])
h_hilt = part('a3/hero/hilt', 'belt', 'hip', 'hilt', 'person', hb, [('sheath', 'sword')], z=1)
h_armR = part('a3/hero/arm_r', 'armNear', 'shoulderR', 'armR', 'person', hb, [('arms', 'std')], side='R')
h_tuft = part('a3/hero/tuft', 'hairFront', 'head', 'tuft', 'person', hb, [('hair', 'tousled')])
h_tail = part('a3/hero/tail', 'neck', 'neck', 'neck', 'person', hb, [('neck', 'scarf')])
h_expr = part('a3/hero/expr', 'face', 'head', 'expr', 'person', hb, [('expr', 'fierce')])
h_swing = part('a3/hero/swing', 'held', 'pivot', 'held', 'person', hb, [('held', 'sword')])
h_arc = part('a3/hero/arc', 'fx', 'pivot', 'fx', 'person', hb, [('fx', 'arc')])

W, H, PX, PY = 24, 36, 12, 34      # A3's figure canvas and its pivot


def hframe(name, ref, **T):
    f = Frame('man', W, H, PX, PY, **T)
    FRAMES['hero_' + name] = (f, ref, PERSON)
    return f


def tuft_rows(bob, x0):
    """hero._tuft as an overlay on the composed frame (canvas rows): the tip stays behind as the head drops (bob 1),
    flattens into the crown as it rises (bob -1)."""
    if bob == 1:
        g = ['.' * 24, '.' * 24]
        g[1] = '.' * (x0 + 1) + 'a' + '.' * (22 - x0)
        return g
    if bob == -1:
        return ['.' * x0 + '--' + '.' * (22 - x0), '.' * (x0 + 1) + 'a' + '.' * (22 - x0)]
    return None


BOBS = [0, 1, -1, 0, 1, -1]
# --- down: idle and walk (hero.py _walk_d + _tuft)
d_head, d_torso = HR['D_UPPER'][:15], HR['D_UPPER'][15:26]
d_legs_idle = minus(HR['IDLE_D'][26:36], HR['D_SCAB'])
f = hframe('IDLE_D', hero.FRAMES['IDLE_D'])
f.put(h_head, 'D', 'rest', d_head, 0, 0)
f.put(h_torso, 'D', 'rest', d_torso, 0, 15)
f.put(h_legs, 'D', 'idle', d_legs_idle, 0, 26)
f.put(h_sheath, 'D', 'rest', HR['D_SCAB'], 0, 26)
WD = [('A', 0, 'back'), ('A', 1, 'back'), ('B', -1, None), ('C', 0, 'fwd'), ('C', 1, 'fwd'), ('D', -1, None)]
for i, (lg, bob, arm) in enumerate(WD):
    f = hframe(f'WALK_D{i}', hero.FRAMES[f'WALK_D{i}'], bob=bob)
    f.put(h_legs, 'D', 'walk' + lg, HR['D_LEGS'][lg], 0, 25)
    f.put(h_head, 'D', 'rest', d_head, 0, bob)
    f.put(h_torso, 'D', 'rest', d_torso, 0, 15 + bob)
    f.put(h_sheath, 'D', 'rest', HR['D_SCAB'], 0, 26 + bob)
    if arm == 'fwd':
        f.put(h_armR, 'D', 'fwd', HR['ARM_FWD'], 4, 21 + bob)
    elif arm == 'back':
        f.put(h_armR, 'D', 'back', HR['ARM_BACK'], 4, 20 + bob)
    t = tuft_rows(bob, 13)
    if t:
        f.put(h_tuft, 'D', 'down' if bob == 1 else 'up', t, 0, 0)
# --- up (A's back view on the front legs; the right arm swings, the left hand keeps the hilt)
u_head, u_torso = HR['U_UPPER'][:15], HR['U_UPPER'][15:26]
f = hframe('IDLE_U', hero.FRAMES['IDLE_U'])
f.put(h_head, 'U', 'rest', u_head, 0, 0)
f.put(h_torso, 'U', 'rest', u_torso, 0, 15)
f.put(h_legs, 'U', 'idle', minus(HR['IDLE_U'][26:36], HR['U_SCAB']), 0, 26)
f.put(h_sheath, 'U', 'rest', HR['U_SCAB'], 0, 26)
WU = [('A', 0, 'fwd'), ('A', 1, 'fwd'), ('B', -1, None), ('C', 0, 'back'), ('C', 1, 'back'), ('D', -1, None)]
for i, (lg, bob, arm) in enumerate(WU):
    f = hframe(f'WALK_U{i}', hero.FRAMES[f'WALK_U{i}'], bob=bob)
    f.put(h_legs, 'U', 'walk' + lg, HR['D_LEGS'][lg], 0, 25)
    f.put(h_head, 'U', 'rest', u_head, 0, bob)
    f.put(h_torso, 'U', 'rest', u_torso, 0, 15 + bob)
    f.put(h_sheath, 'U', 'rest', HR['U_SCAB'], 0, 26 + bob)
    if arm:
        f.put(h_armR, 'U', arm, HR['U_ARM'][arm], 16, 21 + bob)
    t = tuft_rows(bob, 9)
    if t:
        f.put(h_tuft, 'U', 'down' if bob == 1 else 'up', t, 0, 0)
# --- right (the scabbard behind the legs, the hilt at the hip, the near arm swinging, the scarf's tail flying)
r_head = HR['IDLE_R'][:15]
f = hframe('IDLE_R', hero.FRAMES['IDLE_R'])
f.put(h_sheath, 'R', 'rest', HR['R_SCAB'], 0, 26, layer='back')
f.put(h_legs, 'R', 'idle', minus(HR['IDLE_R'][26:36], HR['R_SCAB']), 0, 26)
f.put(h_head, 'R', 'rest', r_head, 0, 0)
f.put(h_torso, 'R', 'idle', HR['IDLE_R'][15:26], 0, 15)
f.keys['hilt'] = '-'
WR_ = [('A', 0, 'B', 'flow'), ('A', 1, 'B', 'flow2'), ('B', -1, 'N', 'flow'), ('C', 0, 'F', 'flow'), ('C', 1, 'F', 'flow2'),
       ('D', -1, 'N', 'flow')]
for i, (lg, bob, arm, tail) in enumerate(WR_):
    f = hframe(f'WALK_R{i}', hero.FRAMES[f'WALK_R{i}'], bob=bob)
    f.put(h_sheath, 'R', 'rest', HR['R_SCAB'], 0, 26 + bob, layer='back')
    f.put(h_legs, 'R', 'walk' + lg, HR['R_LEGS'][lg], 0, 25)
    f.put(h_head, 'R', 'rest', r_head, 0, bob)
    f.put(h_torso, 'R', 'rest', HR['R_TORSO'], 0, 15 + bob)
    f.put(h_hilt, 'R', 'rest', HR['R_HILT'], 16, 19 + bob)
    f.put(h_armR, 'R', arm, HR['R_ARM'][arm], 0, 17 + bob)
    f.put(h_tail, 'R', tail, HR['TAIL_R'][tail], 0, 15 + bob - (1 if tail == 'flow2' else 0))
# --- left (A's left view: the scabbard in front of the legs, lagging; the far hand peeking past the hip)
l_head, l_torso = HR['L_UPPER'][:15], HR['L_UPPER'][15:26]
h_armRfar = h_armR
f = hframe('IDLE_L', hero.FRAMES['IDLE_L'])
f.put(h_legs, 'L', 'idle', minus(HR['IDLE_L'][26:36], HR['L_SCAB']), 0, 26)
f.put(h_head, 'L', 'rest', l_head, 0, 0)
f.put(h_torso, 'L', 'rest', l_torso, 0, 15)
f.put(h_sheath, 'L', 'rest', HR['L_SCAB'], 0, 26)
WL = [('A', 0, 'flow', False, 0), ('A', 1, 'flow2', False, 1), ('B', -1, 'flow', False, 0), ('C', 0, 'flow', True, 0),
      ('C', 1, 'flow2', True, 1), ('D', -1, 'flow', False, 0)]
for i, (lg, bob, tail, far, lag) in enumerate(WL):
    f = hframe(f'WALK_L{i}', hero.FRAMES[f'WALK_L{i}'], bob=bob)
    f.put(h_legs, 'L', 'walk' + lg, HR['L_LEGS'][lg], 0, 25)
    f.put(h_head, 'L', 'rest', l_head, 0, bob)
    f.put(h_torso, 'L', 'rest', l_torso, 0, 15 + bob)
    f.put(h_sheath, 'L', 'lag' if lag else 'rest', HR['L_SCAB'], lag, 26 + bob)
    if far:
        f.put(h_armR, 'L', 'back', HR['FAR_HAND_L'], 16, 22 + bob, layer='armFar')
    f.put(h_tail, 'L', tail, HR['TAIL_L'][tail], 0, 15 + bob - (1 if tail == 'flow2' else 0))

# --- the sword swing, facing right (swing.py key_r: 44 x 44, the body box at (10, 8): the pivot at (22, 42))
SWP = (22, 42)
fierce_r = diff_overlay(r_head, swing.HEAD_R_FIERCE)
SR = [('A', -1, 1, None, swing.R1, None), ('L', 1, 1, swing.R2_ARC, hero.OV2F, 'fierce'),
      ('L', 1, 2, swing.R3_ARC, hero.OV3F, 'fierce'), ('A', 0, 0, None, hero.OV4, None)]
for k, (lg, dx, dy, back, front, head) in enumerate(SR):
    f = Frame('man', 44, 44, SWP[0], SWP[1], bob=dy, lean=dx)
    FRAMES[f'hero_SWING_R{k}'] = (f, swing.FRAMES[f'SWING_R{k}'], PERSON)
    if back:
        f.put(h_arc, 'R', f'a{k}', back, 0, 0, layer='fxBack')
    f.put(h_sheath, 'R', 'rest', HR['R_SCAB'], 10, 8 + 26 + dy, layer='back')
    f.put(h_legs, 'R', 'walk' + lg if lg != 'L' else 'lunge', HR['R_LEGS'][lg], 10, 8 + 25)
    f.put(h_head, 'R', 'rest', r_head, 10 + dx, 8 + dy)
    f.put(h_torso, 'R', 'rest', HR['R_TORSO'], 10 + dx, 8 + 15 + dy)
    if head:
        f.put(h_expr, 'R', 'fierce', fierce_r, 10 + dx, 8 + dy)
    f.put(h_swing, 'R', f'r{k}', front, 0, 0)
    f.keys.setdefault('hilt', '-')
    f.keys.setdefault('neck', '-')
    f.keys.setdefault('armR', '-')
# --- the sword swing, facing down (swing.py key_d: 48 x 48, the body at (12, 8): the pivot at (24, 42))
SWD = (24, 42)
d_head_lit = swing.D_UPPER_S[:15]
lit_d = diff_overlay(d_head, d_head_lit)
fierce_d = diff_overlay(d_head, swing.HEAD_D_FIERCE)
SD = [(-1, 1, swing.D1, None, None), (1, 1, swing.D2, swing.D2_ARC, 'fierce'), (2, 1, swing.D3, swing.D3_ARC, 'fierce'),
      (0, 0, swing.D4, None, None)]
for k, (dx, dy, arm, fx, head) in enumerate(SD):
    f = Frame('man', 48, 48, SWD[0], SWD[1], bob=dy, lean=dx, hip=(dx, 0))
    FRAMES[f'hero_SWING_D{k}'] = (f, swing.FRAMES[f'SWING_D{k}'], PERSON)
    f.put(h_legs, 'D', 'drawn', swing.D_LEGS_S, 12, 8 + 26)
    f.put(h_sheath, 'D', 'drawn', swing.D_SCAB_S, 12 + dx, 8 + 26 + dy)
    f.put(h_head, 'D', 'rest', d_head, 12 + dx, 8 + dy)
    f.put(h_torso, 'D', 'drawn', swing.D_UPPER_S[15:26], 12 + dx, 8 + 15 + dy)
    f.put(h_expr, 'D', 'fierce' if head else 'lit', fierce_d if head else lit_d, 12 + dx, 8 + dy)
    f.put(h_swing, 'D', f'd{k}', arm, 0, 0)
    if fx:
        f.put(h_arc, 'D', f'a{k}', fx, 0, 0)

# ---------------------------------------------------------------------------------------------------- the woman
WROLES_HEAD = [('head', 'oval'), ('hair', 'braid'), ('nose', 'button'), ('brow', 'arched'), ('ears', 'human')]
wb = ('woman',)
w_head = part('a3/woman/head', 'head', 'head', 'rest', 'woman', wb, WROLES_HEAD)
w_torso = part('a3/woman/torso', 'torso', 'neck', 'torso', 'woman', wb, [('garment', 'dress'), ('overlay', 'apron')])
w_hang = part('a3/woman/hang', 'hangFront', 'hip', 'hang', 'woman', wb, [('garment', 'dress')], sway=5)
w_shoes = part('a3/woman/shoes', 'belt', 'pivot', 'legs', 'woman', wb, [('legs', 'tips')], z=-1)
w_armR = part('a3/woman/arm_r', 'armNear', 'shoulderR', 'armR', 'woman', wb, [('arms', 'std')], side='R')
w_armL = part('a3/woman/arm_l', 'armNear', 'shoulderL', 'armL', 'woman', wb, [('arms', 'std')], side='L')
w_braid = part('a3/woman/braid', 'hairFront', 'head', 'hair', 'woman', wb, [('hair', 'braid')])


def wframe(name, ref, **T):
    f = Frame('woman', W, H, PX, PY, **T)
    FRAMES['woman_' + name] = (f, ref, WOMAN_LEG)
    return f


HEAD_ROWS, TORSO_ROWS = 17, 29      # the woman's split: head rows 0-16, torso 17-28, the hang (skirt) 29-33


def wbody(f, facing, body, bob, sway, torso_key='rest', hang_key='rest'):
    f.put(w_head, facing, 'rest', body[:HEAD_ROWS], 0, bob)
    f.put(w_torso, facing, torso_key, body[HEAD_ROWS:TORSO_ROWS], 0, HEAD_ROWS + bob)
    f.put(w_hang, facing, hang_key, body[TORSO_ROWS:], 0, TORSO_ROWS + bob)
    if sway:
        f.T['sway'] = sway


def shifted(rows, dx):
    return [('.' * dx + r[:len(r) - dx]) if dx > 0 else (r[-dx:] + '.' * (-dx)) if dx < 0 else r for r in rows]


# --- idles (A's own maps: the head, the torso with its arms, the skirt with both shoes and the soles)
for fc in 'DURL':
    idle = WR['IDLE_' + fc]
    f = wframe('IDLE_' + fc, woman.FRAMES['IDLE_' + fc])
    f.put(w_head, fc, 'rest', idle[:HEAD_ROWS], 0, 0)
    f.put(w_torso, fc, 'idle', idle[HEAD_ROWS:TORSO_ROWS], 0, HEAD_ROWS)
    f.put(w_hang, fc, 'idle', idle[TORSO_ROWS:], 0, TORSO_ROWS)
    f.keys['legs'] = '-'
# --- down and up: the body bobs and its hem sways, the shoes stay planted, the hands swing, the braid's tip lags
WDW = [(0, -1, 'L', 'up', 'down'), (1, -1, 'L', 'up', 'down'), (-1, 0, 'L', None, None), (0, 1, 'R', 'down', 'up'),
       (1, 1, 'R', 'down', 'up'), (-1, 0, 'R', None, None)]
for fc, body0, shoes, arms_l, arms_r in (('D', WR['_D_BODY'], WR['W_D_SHOES'], WR['W_ARM_L'], WR['W_ARM_R']),
                                         ('U', WR['_U_BODY'], WR['W_U_SHOES'], WR['W_ARM_L'], WR['W_ARM_R'])):
    for i, (bob, sway, sh, la, ra) in enumerate(WDW if fc == 'D' else
                                                [(0, -1, 'L', 'down', 'up'), (1, -1, 'L', 'down', 'up'), (-1, 0, 'L', None, None),
                                                 (0, 1, 'R', 'up', 'down'), (1, 1, 'R', 'up', 'down'), (-1, 0, 'R', None, None)]):
        f = wframe(f'WALK_{fc}{i}', woman.FRAMES[f'WALK_{fc}{i}'], bob=bob)
        wbody(f, fc, body0, bob, sway)
        # A3's arm patches: the viewer's left patch is the body's right arm facing down, its left arm facing up
        lpart, rpart = (w_armR, w_armL) if fc == 'D' else (w_armL, w_armR)
        if la:
            f.put(lpart, fc, la, arms_l[la], 4, 20 + bob)
        if ra:
            f.put(rpart, fc, ra, arms_r[ra], 17, 20 + bob)
        f.put(w_shoes, fc, sh, shoes[sh], 0, 33)
        FRAMES[f'woman_WALK_{fc}{i}'] = (f, woman.FRAMES[f'WALK_{fc}{i}'], WOMAN_LEG)
# --- right and left: the near arm swings, the hem widens on the contact frames, the braid swings on the down frames
for fc, body0, wide, braid, arms, shoes in (('R', WR['_R_BODY'], WR['_R_HEM_WIDE'], WR['_R_BRAID_SWING'], WR['W_R_ARM'], WR['W_R_SHOES']),
                                            ('L', WR['_L_BODY'], WR['_L_HEM_WIDE'], WR['_L_BRAID_SWING'], WR['W_L_ARM'], WR['W_L_SHOES'])):
    wide_body = list(body0)
    for y, r in wide.items():
        wide_body[y] = r
    arm_part = w_armR if fc == 'R' else w_armL
    for i, (bob, wd, sh, arm) in enumerate([(0, True, 'A', 'B'), (1, True, 'A', 'B'), (-1, False, 'B', 'N'),
                                            (0, True, 'C', 'F'), (1, True, 'C', 'F'), (-1, False, 'B', 'N')]):
        f = wframe(f'WALK_{fc}{i}', woman.FRAMES[f'WALK_{fc}{i}'], bob=bob)
        body = wide_body if wd else body0
        wbody(f, fc, body, bob, 0, hang_key='wide' if wd else 'rest')
        if bob == 1:
            swung = list(body)
            for y, r in braid.items():
                swung[y] = r
            # (woman.py swings the braid in the body rows, under the arm: so the overlay sits under the near arm)
            f.put(w_braid, fc, 'swing', diff_overlay(body[:TORSO_ROWS], swung[:TORSO_ROWS]), 0, bob, layer='neck')
        f.put(arm_part, fc, arm, arms[arm], 0, 20 + bob)
        f.put(w_shoes, fc, sh, shoes[sh], 0, 33)
# the braid's tip in the down and up walks (woman.py _braid_d, the up walk's row 18): overlays on the composed frame,
# computed once the frames compose (below)


# ---------------------------------------------------------------------------------------------------- the reference model
def legend_of(p):
    return WOMAN_LEG if p.legend == 'woman' else PERSON


def compose(f, extra=()):
    """The doll's composition of a frame (doll_compose.cpp): every view placed at its anchor + the frame's moves, painted
    in layer order (layer, then z, then the part's order), '-' erasing what is below; then the finishing pass."""
    items = []
    for p, facing, key in list(f.uses) + list(extra):
        v = p.views[(facing, key)]
        a = v.anchor or p.anchor
        bx, by = ANCHORS[f.body][facing][a]
        dx, dy = delta(a, f.T)
        ax, ay = f.px + bx + dx, f.py + by + dy
        layer = v.layer or p.layer
        sw = (min(p.sway, len(v.rows)), f.T.get('sway', 0)) if p.sway else (0, 0)
        items.append((LAYERS.index(layer), p.z, p.order, v.rows, ax - v.origin[0], ay - v.origin[1], legend_of(p), sw))
    items.sort(key=lambda t: t[:3])
    can = [[None] * f.w for _ in range(f.h)]
    for _, _, _, rows, ox, oy, leg, (sn, sdx) in items:
        for y, r in enumerate(rows):
            for x, c in enumerate(r):
                X, Y = ox + x + (sdx if y >= len(rows) - sn else 0), oy + y   # (the sway: the hang's bottom rows moved)
                if c == '.' or not (0 <= X < f.w and 0 <= Y < f.h):
                    continue
                can[Y][X] = None if c == '-' else leg[c]
    return finish(can)


def finish(can):
    h, w = len(can), len(can[0])
    out = [list(r) for r in can]
    for y in range(h):
        for x in range(w):
            c = can[y][x]
            if c is None or c[1] != 0 or c[0] in UNLIT:
                continue
            nx = ny = 0.0
            for ddy in (-1, 0, 1):
                for ddx in (-1, 0, 1):
                    if ddx == ddy == 0:
                        continue
                    X, Y = x + ddx, y + ddy
                    if not (0 <= X < w and 0 <= Y < h) or can[Y][X] is None:
                        k = 1.0 if ddx == 0 or ddy == 0 else 0.7
                        nx += ddx * k
                        ny += ddy * k
            n = math.hypot(nx, ny)
            if n < 1e-6:
                continue
            if (nx * -0.7071 + ny * -0.7071) / n > 0.35:
                out[y][x] = (c[0], 1)
    return out


def as_codes(rows, leg):
    return [[None if c == '.' else leg[c] for c in r] for r in rows]


def braid_fixups():
    """woman.py's braid tips on the down / up walks are replacements on the frame as composed (rows 22-25 where the frame
    is opaque; the up walk's row 18): the overlay is the difference against the doll's own composition of that frame."""
    for fc in 'DU':
        for i, bob in enumerate(BOBS):
            if bob != 1:
                continue
            f, ref, leg = FRAMES[f'woman_WALK_{fc}{i}']
            raw = compose_raw(f)
            if fc == 'D':
                new = [list(r) for r in raw]
                for k, r in enumerate(WR['BRAID_D_IN']):
                    y = 22 + bob + k
                    new[y] = [(n if o != '.' else o) for o, n in zip(raw[y], r)]
                new = [''.join(r) for r in new]
            else:
                continue    # the up walk's row 18 is in the body grid (below)
            ov = diff_overlay(raw[:30], new[:30])
            f.put(w_braid, fc, f'swing{i}', ov, 0, 0)    # (the two down frames sway apart: a key each)


def compose_raw(f):
    """The frame composed to chars (no finishing), for the braid fix-ups."""
    items = []
    for p, facing, key in f.uses:
        v = p.views[(facing, key)]
        a = v.anchor or p.anchor
        bx, by = ANCHORS[f.body][facing][a]
        dx, dy = delta(a, f.T)
        sw = (min(p.sway, len(v.rows)), f.T.get('sway', 0)) if p.sway else (0, 0)
        items.append((LAYERS.index(v.layer or p.layer), p.z, p.order, v.rows, f.px + bx + dx - v.origin[0],
                      f.py + by + dy - v.origin[1], sw))
    items.sort(key=lambda t: t[:3])
    can = [['.'] * f.w for _ in range(f.h)]
    for _, _, _, rows, ox, oy, (sn, sdx) in items:
        for y, r in enumerate(rows):
            for x, c in enumerate(r):
                X, Y = ox + x + (sdx if y >= len(rows) - sn else 0), oy + y
                if c != '.' and 0 <= X < f.w and 0 <= Y < f.h:
                    can[Y][X] = '.' if c == '-' else c
    return [''.join(r) for r in can]


# the up walk's braid (woman.py: body[18] = U_BRAID_OUT on the down frames) is a body row: an overlay at the head anchor
for i, bob in enumerate(BOBS):
    if bob != 1:
        continue
    f, ref, leg = FRAMES[f'woman_WALK_U{i}']
    body = WR['_U_BODY']
    swung = list(body)
    swung[18] = WR['U_BRAID_OUT']
    f.put(w_braid, 'U', 'swing', diff_overlay(body[:TORSO_ROWS], swung[:TORSO_ROWS]), 0, bob)
braid_fixups()


# ---------------------------------------------------------------------------------------------------- marks
def add_marks():
    """Where the face and hands are (rpg/art/doll/doll.h marks -> RigAnchors): the face's top-left (the brows' row, the
    10 px face's column 0), the hands' 3 x 2 blocks. Drafts carry the front views' marks; FOLK-BODY places the rest."""
    # hero front: brows on row 9, the face from column 7; the free hand at (4, 23), the hilt hand at (17, 23)
    for p, fc, mk, xy in ((h_head, 'D', 'face', (7, 9)), (w_head, 'D', 'face', (7, 10))):
        v = p.views[(fc, 'rest')]
        # grid coords: the head grid was trimmed; canvas (x, y) -> grid (x - x0, y - y0) through the origin
        a = p.anchor
        bx, by = ANCHORS['man' if p is h_head else 'woman'][fc][a]
        ax, ay = PX + bx, PY + by
        v.marks[mk] = (xy[0] - ax + v.origin[0], xy[1] - ay + v.origin[1])
    for fc, hand, xy in (('D', 'handR', (4, 23)), ('D', 'handL', (17, 23)), ('U', 'handL', (4, 23)), ('U', 'handR', (17, 23))):
        v = h_torso.views[(fc, 'rest')]
        bx, by = ANCHORS['man'][fc]['neck']
        v.marks[hand] = (xy[0] - (PX + bx) + v.origin[0], xy[1] - (PY + by) + v.origin[1])


add_marks()


# ---------------------------------------------------------------------------------------------------- verify
def verify(quiet=False):
    bad = 0
    lone_total = 0
    for name, (f, ref, leg) in sorted(FRAMES.items()):
        got = compose(f)
        want = as_codes(ref, leg)
        if len(want) != f.h or len(want[0]) != f.w:
            print(f'{name}: reference is {len(want[0])} x {len(want)}, frame {f.w} x {f.h}')
            bad += 1
            continue
        diff = [(x, y, got[y][x], want[y][x]) for y in range(f.h) for x in range(f.w) if got[y][x] != want[y][x]]
        if diff:
            bad += 1
            print(f'FAIL {name}: {len(diff)} px differ, first {diff[:4]}')
        elif not quiet:
            print(f'ok   {name}')
    print(f'from_a3 verify: {len(FRAMES)} frames, {bad} differing' + ('' if bad else ' (every A3 frame composes from the drafts)'))
    return bad


# ---------------------------------------------------------------------------------------------------- writing
def legend_text(name, table, base=None):
    lines = [f'# rpg/art/doll/maps/legend/{name}.legend: character -> slot and step (STYLE_P2 6; rpg/art/doll/doll.h)',
             '# written by tools/doll/from_a3.py from A3\'s character legend (docs/ref/style_a3/pal.py)']
    if base:
        lines.append(f'# used on top of {base}.legend: the woman\'s chemise (linen), apron (cloth3), ribbon (accent) and lip')
    for ch, (slot, step) in table.items():
        lines.append(f'legend {ch} {slot} {step}')
    return '\n'.join(lines) + '\n'


def write_part(out, p):
    path = os.path.join(out, 'maps', p.name + '.pxm')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    L = [f'# {p.name}: a draft of A3\'s parts (tools/doll/from_a3.py; docs/ref/style_a3). A starting point: hand-check it.',
         f'part {p.name}', 'tags draft a3', 'use person']
    if p.legend == 'woman':
        L.append('use woman')
    L.append('body ' + ' '.join(p.bodies))
    for cat, val in p.roles:
        L.append(f'role {cat} {val}')
    L.append(f'layer {p.layer}')
    L.append(f'anchor {p.anchor}')
    L.append(f'keys {p.channel}')
    if p.side:
        L.append(f'side {p.side}')
    if p.z:
        L.append(f'z {p.z}')
    if p.sway:
        L.append(f'sway {p.sway}')
    for (fc, key), v in sorted(p.views.items(), key=lambda kv: (FACINGS.index(kv[0][0]), kv[0][1])):
        hdr = f'view {fc} key {key} origin {v.origin[0]} {v.origin[1]}'
        if v.layer:
            hdr += f' layer {v.layer}'
        if v.anchor:
            hdr += f' anchor {v.anchor}'
        L.append('')
        L.append(hdr)
        for mk, (mx, my) in sorted(v.marks.items()):
            L.append(f'mark {mk} {mx} {my}')
        for r in v.rows:
            L.append('|' + r)
    with open(path, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('\n'.join(L) + '\n')


def clip_text():
    """The draft clip table: the frames in the order the doll's RigSheet holds them."""
    L = ['# rpg/art/doll/drafts/a3/clips/a3.clip: A3\'s frames as doll clips (tools/doll/from_a3.py). Format: rpg/art/doll/doll.h.',
         '# Walk frames run contact, down, pass, contact, down, pass; A3\'s 28 px cycle (FOLK-BODY re-registers to 19 / 20 px).']
    for body in ('man', 'woman'):
        L.append('')
        L.append(f'body {body}')
        for fc in FACINGS:
            a = ANCHORS[body][fc]
            L.append(f'anchors {fc} ' + ' '.join(f'{k}={x},{y}' for k, (x, y) in a.items() if k != 'pivot'))
        who = 'hero' if body == 'man' else 'woman'
        for fc in FACINGS:
            L.append(f'clip idle {fc}')
            L.append(frame_line(FRAMES[f'{who}_IDLE_{fc}'][0]))
        for fc in FACINGS:
            L.append(f'clip walk {fc} cycle 28')
            for i in range(6):
                L.append(frame_line(FRAMES[f'{who}_WALK_{fc}{i}'][0]))
        if body == 'man':
            for fc in 'RD':
                for clip, ks in (('windup', [0]), ('strike', [1]), ('follow', [2, 3])):
                    L.append(f'clip {clip} {fc} family sword variant 0')
                    for k in ks:
                        L.append(frame_line(FRAMES[f'hero_SWING_{fc}{k}'][0], ms=(70, 110, 70, 60)[k]))
    return '\n'.join(L) + '\n'


def frame_line(f, ms=None):
    toks = ['f']
    for ch in sorted(f.keys):
        toks.append(f'{ch}={f.keys[ch]}')
    # channels the frame does not use are switched off explicitly where a part on them would otherwise draw its rest key
    for t in ('bob', 'lean', 'sway'):
        if f.T.get(t):
            toks.append(f'{t}={f.T[t]}')
    if f.T.get('hip') and f.T['hip'] != (0, 0):
        toks.append('hip={},{}'.format(*f.T['hip']))
    if ms:
        toks.append(f'ms={ms}')
    return ' '.join(toks)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=os.path.join(REPO, 'rpg', 'art', 'doll', 'drafts', 'a3'))
    ap.add_argument('--verify', action='store_true')
    ap.add_argument('--quiet', action='store_true')
    a = ap.parse_args()
    bad = verify(a.quiet)
    if a.verify:
        sys.exit(1 if bad else 0)
    if bad:
        sys.exit('from_a3: the drafts do not compose to A3; nothing written')
    out = a.out
    os.makedirs(os.path.join(out, 'maps', 'legend'), exist_ok=True)
    os.makedirs(os.path.join(out, 'clips'), exist_ok=True)
    with open(os.path.join(out, 'maps', 'legend', 'person.legend'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.write(legend_text('person', PERSON))
    with open(os.path.join(out, 'maps', 'legend', 'woman.legend'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.write(legend_text('woman', WOMAN_EXTRA, 'person'))
    for p in PARTS.values():
        write_part(out, p)
    with open(os.path.join(out, 'clips', 'a3.clip'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.write(clip_text())
    print(f'from_a3: {len(PARTS)} parts, {sum(len(p.views) for p in PARTS.values())} views, {len(FRAMES)} frames -> {out}')


if __name__ == '__main__':
    main()
