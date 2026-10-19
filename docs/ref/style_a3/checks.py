"""checks.py: the craft gates run over every frame (orphans, height, width, palette use).

python checks.py  -> a table per frame: height sole-to-crown, silhouette width, orphan pixels, colours used.
"""
import sys

sys.dont_write_bytecode = True


def bbox(rows):
    ys = [y for y, r in enumerate(rows) if r.strip('.')]
    xs = [x for r in rows for x, c in enumerate(r) if c != '.']
    return min(xs), min(ys), max(xs), max(ys)


def orphans(rows, ignore=''):
    """A pixel whose colour no 8-neighbour shares AND whose 4-neighbours are all transparent or of a different
    material family (a lone speck). Reports only truly isolated specks: all 8 neighbours transparent."""
    h, w = len(rows), len(rows[0])
    out = []
    for y in range(h):
        for x in range(w):
            c = rows[y][x]
            if c == '.' or c in ignore:
                continue
            nb = [rows[y + dy][x + dx] if 0 <= x + dx < w and 0 <= y + dy < h else '.'
                  for dy in (-1, 0, 1) for dx in (-1, 0, 1) if (dx, dy) != (0, 0)]
            if all(n == '.' for n in nb):
                out.append((x, y, c))
    return out


def lone_colour(rows, ignore='.#@EzMYKA BCD'):
    """Single pixels whose colour appears in none of the 8 neighbours (noise inside clusters). Face / FX chars are
    deliberately single and are ignored."""
    h, w = len(rows), len(rows[0])
    out = []
    for y in range(h):
        for x in range(w):
            c = rows[y][x]
            if c in ignore:
                continue
            nb = [rows[y + dy][x + dx] if 0 <= x + dx < w and 0 <= y + dy < h else '.'
                  for dy in (-1, 0, 1) for dx in (-1, 0, 1) if (dx, dy) != (0, 0)]
            if c not in nb:
                out.append((x, y, c))
    return out


def report(name, rows, body_only=None):
    x0, y0, x1, y1 = bbox(body_only or rows)
    o = orphans(rows)
    l = lone_colour(rows)
    cols = sorted(set(''.join(rows)) - {'.'})
    return f'{name:14s} h={y1 - y0 + 1:2d} (rows {y0}-{y1}) w={x1 - x0 + 1:2d} orphans={o} lone={len(l)} colours={len(cols)}'


if __name__ == '__main__':
    import importlib
    for modname in sys.argv[1:] or ['hero', 'woman']:
        m = importlib.import_module(modname)
        for name in m.FRAMES:
            print(modname, report(name, m.FRAMES[name]))
