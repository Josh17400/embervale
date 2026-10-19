"""pixlib: hand-placed pixel maps -> PIL images (art study A2, EMBERVALE 32 px figures).

A sprite is a character grid (one char = one pixel) plus a palette {char: '#rrggbb'}; '.' is transparent.
Frames are layered the way an Aseprite artist works: a hand-drawn head is copied onto a hand-drawn torso, which sits on
hand-drawn legs for that frame. Nothing here draws shapes procedurally; the helpers only paste, erase and offset.
"""
from PIL import Image


def hexrgb(h):
    h = h.lstrip('#')
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4)) + (255,)


def grid(s, name='?', w=None, strict=False):
    """Rows of a hand-drawn block. Short rows are padded with '.' on the right (small parts); strict=True insists on
    equal widths (full-canvas maps, where a short row is a typo)."""
    rows = [r for r in s.strip('\n').split('\n')]
    rows = [r.split('|')[0].rstrip() if '|' in r else r for r in rows]  # allow trailing '| comment'
    width = w or max(len(r) for r in rows)
    for i, r in enumerate(rows):
        if len(r) > width or (strict and len(r) != width):
            raise ValueError(f'{name}: row {i} has width {len(r)} (expected {width}): {r!r}')
    return [r.ljust(width, '.') for r in rows]


def blank(w, h):
    return ['.' * w for _ in range(h)]


def paste(base, part, ox, oy, skip='.'):
    """Copy part's non-transparent chars onto base at (ox, oy). ' ' and '.' in part are see-through."""
    out = [list(r) for r in base]
    for y, r in enumerate(part):
        for x, ch in enumerate(r):
            if ch in (skip, ' '):
                continue
            X, Y = ox + x, oy + y
            if 0 <= Y < len(out) and 0 <= X < len(out[0]):
                out[Y][X] = ch
    return [''.join(r) for r in out]


def erase(base, part, ox, oy, mark='-'):
    """Where part has `mark`, make base transparent (for un-painting under a moved limb)."""
    out = [list(r) for r in base]
    for y, r in enumerate(part):
        for x, ch in enumerate(r):
            if ch == mark:
                X, Y = ox + x, oy + y
                if 0 <= Y < len(out) and 0 <= X < len(out[0]):
                    out[Y][X] = '.'
    return [''.join(r) for r in out]


def layer(w, h, *parts):
    """parts: (rows, ox, oy) tuples; later parts paint over earlier ones; '-' in a part erases."""
    g = blank(w, h)
    for p in parts:
        if p is None:
            continue
        rows, ox, oy = p
        g = erase(g, rows, ox, oy)
        g = paste(g, [r.replace('-', '.') for r in rows], ox, oy)
    return g


def shift(rows, dx, dy):
    h, w = len(rows), len(rows[0])
    return layer(w, h, (rows, dx, dy))


def mirror(rows, swap=None):
    swap = swap or {}
    return [''.join(swap.get(c, c) for c in reversed(r)) for r in rows]


def recolour(rows, mapping):
    return [''.join(mapping.get(c, c) for c in r) for r in rows]


def render(rows, pal):
    h, w = len(rows), len(rows[0])
    im = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    px = im.load()
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            if ch == '.':
                continue
            if ch not in pal:
                raise KeyError(f'char {ch!r} at {x},{y} not in palette')
            c = pal[ch]
            if c is None:
                continue
            px[x, y] = hexrgb(c)
    return im


def upscale(im, k):
    return im.resize((im.width * k, im.height * k), Image.NEAREST)


# selective outlining: an outline pixel whose outward edge faces the light (top-left) takes its material's next lighter
# step, the way a pixel artist recolours the lit side of a line art pass. It recolours existing outline pixels only.
SELOUT_HERO = {'a': 'b', 'q': 'r', '5': '6', 'f': 'g', '1': '2', 'm': 'n', 'v': 'w'}
SELOUT_WOMAN = {'a': 'b', 'q': 'r', '5': '6', 'f': 'g', 'v': 'w', 'A': 'B'}


def selout(rows, mapping, threshold=0.35):
    import math
    h, w = len(rows), len(rows[0])
    L = (-0.7071, -0.7071)
    out = [list(r) for r in rows]
    for y in range(h):
        for x in range(w):
            c = rows[y][x]
            if c not in mapping:
                continue
            nx = ny = 0.0
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    if dx == dy == 0:
                        continue
                    X, Y = x + dx, y + dy
                    if not (0 <= X < w and 0 <= Y < h) or rows[Y][X] == '.':
                        k = 1.0 if dx == 0 or dy == 0 else 0.7
                        nx += dx * k
                        ny += dy * k
            n = math.hypot(nx, ny)
            if n < 1e-6:
                continue
            if (nx * L[0] + ny * L[1]) / n > threshold:
                out[y][x] = mapping[c]
    return [''.join(r) for r in out]
