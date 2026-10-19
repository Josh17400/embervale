"""look.py: contact sheets for the look-and-adjust loop.

contact(frames, pal, path, k=12): each frame at 1x / 2x / 4x on grass and dirt along the top, then at k x with a
pixel grid and row / column numbers. frames: list of (label, rows) or rows.
"""
from PIL import Image, ImageDraw, ImageFont
from pixlib import render, upscale

GRASS = (140, 142, 80, 255)
DIRT = (123, 95, 66, 255)
FONT = ImageFont.truetype('C:/Windows/Fonts/consola.ttf', 11)


def contact(frames, pal, path, k=12, grid=True, bgs=(GRASS, DIRT)):
    frames = [f if isinstance(f, tuple) else ('', f) for f in frames]
    ims = [(lab, render(r, pal) if isinstance(r, list) else r) for lab, r in frames]
    w = max(im.width for _, im in ims)
    h = max(im.height for _, im in ims)
    top_h = h * 4 + 16
    M = 16
    W = max(len(ims) * (w * k + M) + M, len(bgs) * (w * 7 + 12) * len(ims) + 20)
    H = top_h + h * k + M * 2 + 14
    out = Image.new('RGBA', (W, H), (40, 38, 48, 255))
    d = ImageDraw.Draw(out)
    x = 6
    for bg in bgs:
        for _, im in ims:
            for s in (1, 2, 4):
                t = Image.new('RGBA', (im.width * s, im.height * s), bg)
                t.alpha_composite(upscale(im, s))
                out.alpha_composite(t, (x, 6 + (h - im.height) * s))
                x += im.width * s + 2
            x += 6
        x += 10
    x = M
    for lab, im in ims:
        y0 = top_h + 14
        big = upscale(im, k)
        bg = Image.new('RGBA', big.size, (96, 100, 60, 255))
        bd = ImageDraw.Draw(bg)
        if grid:
            for gx in range(0, big.width + 1, k):
                bd.line([(gx, 0), (gx, big.height)], fill=(106, 110, 68, 255))
            for gy in range(0, big.height + 1, k):
                bd.line([(0, gy), (big.width, gy)], fill=(106, 110, 68, 255))
        bg.alpha_composite(big)
        out.alpha_composite(bg, (x, y0))
        d.text((x, top_h), lab, font=FONT, fill=(230, 220, 200, 255))
        if grid:
            for gx in range(0, im.width, 2):
                d.text((x + gx * k + 2, y0 + big.height + 1), str(gx), font=FONT, fill=(170, 170, 150, 255))
            for gy in range(0, im.height, 2):
                d.text((x - 14, y0 + gy * k + 1), str(gy), font=FONT, fill=(170, 170, 150, 255))
        x += big.width + M
    out = out.crop((0, 0, max(x, 10), H))
    out.save(path)
    return out.size


def strip(frames, pal, path, k=4, bg=GRASS, gap=4):
    ims = [render(r, pal) if isinstance(r, list) else r for r in frames]
    w = sum(im.width for im in ims) * k + gap * (len(ims) + 1)
    h = max(im.height for im in ims) * k + gap * 2
    out = Image.new('RGBA', (w, h), bg)
    x = gap
    for im in ims:
        out.alpha_composite(upscale(im, k), (x, gap))
        x += im.width * k + gap
    out.save(path)
    return out.size


def gif(seqs, pal, path, k=4, bg=GRASS, gap=6, shadow=None):
    """seqs: list of (frames, durations_ms) played side by side; every sequence must have the same total length in ms.
    Each GIF frame is cut at every change in any sequence."""
    from PIL import Image
    ims = [[render(f, pal) if isinstance(f, list) else f for f in fr] for fr, _ in seqs]
    w = sum(max(i.width for i in s) for s in ims) * k + gap * (len(ims) + 1)
    h = max(max(i.height for i in s) for s in ims) * k + gap * 2
    # event times
    cuts = set()
    for _, durs in seqs:
        t = 0
        for d in durs:
            cuts.add(t); t += d
    total = sum(seqs[0][1])
    cuts = sorted(c for c in cuts if c < total) + [total]
    out = []
    durs_out = []
    for a, b in zip(cuts, cuts[1:]):
        fr = Image.new('RGBA', (w, h), bg)
        x = gap
        for s, (_, durs) in zip(ims, seqs):
            t, idx = 0, 0
            for j, d in enumerate(durs):
                if t <= a < t + d:
                    idx = j
                    break
                t += d
            im = s[idx]
            sw = max(i.width for i in s)
            if shadow:
                fr.alpha_composite(shadow(sw, max(i.height for i in s), k), (x, gap))
            fr.alpha_composite(upscale(im, k), (x, gap))
            x += sw * k + gap
        out.append(fr.convert('RGB').convert('P', palette=Image.ADAPTIVE, colors=255))
        durs_out.append(b - a)
    out[0].save(path, save_all=True, append_images=out[1:], duration=durs_out, loop=0, disposal=1)
    return len(out)
