#!/usr/bin/env python3
# tools/doll/p5_pack.py: (P5 PACK, docs/FIX_BATCH_P5.md 5.6, 10 play/) the owner pack's in-play half from two runs of
# tools/scripts/p5_fingerprint.txt (witness_pack --keep): "before" (the same build with EMB_FIX_RULES=4095, or 80215d7's)
# and "after". For every shot both runs hold, it writes into OUT:
#   <shot>.png        BEFORE | AFTER side by side, whole frames, labelled (<= 8000 px a side)
#   <shot>_zoom.png   BEFORE | AFTER of the region the two differ in (the figures), at a whole-number scale 4x-8x
#                     (nearest neighbour), so the pixel art reads at a glance; "no change" frames get none
#   grey/<name>.png   a greyscale copy of each of the above (the marks and the two-tone bands must still part the peoples)
#   captions.txt      one line per image: "<file>  <caption>"; the PENDING header (section 10: nothing ships as final
#                     before the owner has seen it)
# Nothing is drawn by this tool: crops, whole-number scales, labels and greyscale only. Exit 1 when a run is missing or
# no shot pairs up; prints one PASS/FAIL line per check.
#   python tools/doll/p5_pack.py BEFORE_DIR AFTER_DIR OUT_DIR
# BEFORE_DIR / AFTER_DIR: a witness_pack --out dir (its runs/p5_fingerprint/ is used) or the PNG folder itself.
#
# (P5 fix r2) the pack's sheet half, every sheet from one build (the judged HEAD):
#   python tools/doll/p5_pack.py --sheets BDIR PACK_DIR [--seeds A..B] [--assemble-only]
# runs (through tools/slot.sh) BDIR/fingerprint_gallery.exe after and --before (FIX_FP off) for seed 1 (the worn matrices,
# kits, census, swatches, report) and BDIR/icons_gallery.exe for the seeds (default 1..3: the icon matrices) into
# PACK_DIR/work, then writes each sheet with a _grey copy and a caption (<area>/captions.txt):
#   MARKS/matrices  worn_<group>_<n>_seed1 BEFORE above AFTER (the gallery's sheets pasted unaltered)
#   MARKS/kits      kits_T1-T6_man_woman_seed1 BEFORE | AFTER; four peoples' T1-T6 (man) at 4x
#   MARKS/census    census_guard0-2_soldier_bandit_captain_king_DR_seed1 BEFORE | AFTER
#   MARKS           decision2 (the T1 leather coat column, every people, 6x), decision3 (Highland's T5-T6, 8x)
#   ICONS, matrices icons_fp_seed<N> (icons_gallery's BEFORE above AFTER) and its 6x groups; matrices/icons_before_after
#   DOLL            doll_fp_guard (the watch rank 1, D and R), doll_fp_t1 / t2 / t4 (every people's kit, man), 6x
#   GATES, root     swatches.png (+ _grey); GATES/report_seed1.txt; build.sha (git HEAD, "+dirty" when the tree is)
# --assemble-only reuses PACK_DIR/work. Exit 1 when a gallery run fails or a sheet is missing.
import os
import re
import subprocess
import sys

from PIL import Image, ImageChops, ImageDraw

MAX_SIDE = 8000
BG, INK, DIM = (24, 22, 26), (240, 230, 200), (150, 140, 120)
LAB = 30   # the label band's height

# the caption of a shot, by its name (p5_fp_<what>_<zoom>.png); first match wins
CAPTIONS = [
    (r'start_watch', "seed 7's start town (a River town): its watch, four of them, River teal and white on every piece"),
    (r'dune_watch', "a Dune town's watch: indigo and saffron bands, the arabesque on the chest"),
    (r'jade_watch', "a Jade town's watch: vermilion and black lacquer, the cloud scrolls"),
    (r'highland_watch', "a Highland town's watch: tartan green and red, the plaid square"),
    (r'suntemple_watch', "a Sun Temple town's watch: magenta and teal, the sun rays"),
    (r'comp', "a companion given three peoples' pieces: a Fjordfolk mail shirt, a Steppe leather cap, Marsh leather boots"),
    (r'hero_dune', "the hero in a Dune T1 leather coat (the padded coat): indigo and saffron laces, the material still brown leather"),
    (r'hero_highland', "the hero in a Highland T1 leather coat: tartan green and red laces"),
    (r'hero_jade', "the hero in a Jade T1 leather coat: vermilion and black laces"),
    (r'shelf_start', "the start town's smith (River): the shelf's armour icons carry the River dye"),
    (r'shelf_dune', "a Dune smith's shelf: the armour icons in indigo and saffron"),
    (r'shelf_highland', "a Highland smith's shelf: the armour icons in green and red"),
    (r'equip', "the EQUIP page: a Dune coat, a Highland cap, Jade gloves and boots; the list's coats of three peoples"),
    (r'steppe_soldier', "a Steppe realm's soldiers: sky blue and horse-hair red on every piece, the running horse"),
    (r'steppe_bandit', "Steppe bandits (the local people's): sky blue and red, worn and plain"),
    (r'imperial_soldier', "an Imperial realm's soldiers: purple and gold thread, the laurel"),
    (r'night_lamp', "night under a Jade town lamp (23:00): the hero's three peoples' bands still read in lamp light"),
]
ZOOMS = {'near': 'NEAR', 'close': 'CLOSE'}


def caption(name):
    base = name[:-4] if name.endswith('.png') else name
    zoom = next((v for k, v in ZOOMS.items() if base.endswith('_' + k)), '')
    for pat, text in CAPTIONS:
        if re.search(pat, base):
            return text + (f' ({zoom})' if zoom else '')
    return base


def shots_dir(d):
    sub = os.path.join(d, 'runs', 'p5_fingerprint')
    return sub if os.path.isdir(sub) else d


def labelled(pair, labels, scale=1):
    """two same-size images side by side, each under a label band"""
    a, b = pair
    w, h = a.size
    pad = 8
    out = Image.new('RGB', (2 * w + 3 * pad, h + LAB + 2 * pad), BG)
    d = ImageDraw.Draw(out)
    for i, (im, lab) in enumerate(zip((a, b), labels)):
        x = pad + i * (w + pad)
        d.text((x + 4, pad + 8), lab + (f'  {scale}x' if scale > 1 else ''), fill=INK)
        out.paste(im, (x, pad + LAB))
    return out


def diff_box(a, b):
    """the bounding box of the pixels the two frames differ in (None: identical)"""
    return ImageChops.difference(a, b).convert('L').point(lambda v: 255 if v > 0 else 0).getbbox()


def zoom_pair(a, b, box):
    """the differing region (padded, cut to fit) of both, at a whole-number scale 4x-8x"""
    x0, y0, x1, y1 = box
    padpx = 12
    x0, y0 = max(0, x0 - padpx), max(0, y0 - padpx)
    x1, y1 = min(a.width, x1 + padpx), min(a.height, y1 + padpx)
    # the two crops side by side must fit MAX_SIDE at 4x: at most (MAX_SIDE - margins) / 8 px wide a crop, centred
    maxw, maxh = (MAX_SIDE - 64) // 8, (MAX_SIDE - 64) // 4
    if x1 - x0 > maxw:
        c = (x0 + x1) // 2
        x0, x1 = c - maxw // 2, c - maxw // 2 + maxw
    if y1 - y0 > maxh:
        c = (y0 + y1) // 2
        y0, y1 = c - maxh // 2, c - maxh // 2 + maxh
    w, h = x1 - x0, y1 - y0
    scale = max(4, min(8, 1600 // max(w, h, 1)))
    while scale > 4 and (2 * w * scale + 24 > MAX_SIDE or h * scale + LAB + 16 > MAX_SIDE):
        scale -= 1
    up = lambda im: im.crop((x0, y0, x1, y1)).resize((w * scale, h * scale), Image.NEAREST)
    return (up(a), up(b)), scale, (x0, y0, x1, y1)


# ------------------------------------------------------------------------------------------- (fix r2) the sheets
GAP, LAB2 = 24, 26
B_LAB, A_LAB = "BEFORE (pre-P5, the same build, FIX_FP off)", "AFTER (P5: the peoples' fingerprint)"
PENDING = "PENDING THE OWNER'S REVIEW (docs/FIX_BATCH_P5.md 10, 13): nothing ships as final before the owner has seen it."


def pair_sheet(a, b, labels=(B_LAB, A_LAB), vertical=None):
    """BEFORE | AFTER (or BEFORE above AFTER when side by side would pass MAX_SIDE), each pasted unaltered under a label"""
    a, b = a.convert('RGB'), b.convert('RGB')
    if vertical is None:
        vertical = a.width + b.width + GAP > MAX_SIDE
    if vertical:
        w, h = max(a.width, b.width), a.height + b.height + 2 * LAB2 + GAP
        pos = [(0, LAB2), (0, a.height + 2 * LAB2 + GAP)]
    else:
        w, h = a.width + b.width + GAP, max(a.height, b.height) + LAB2
        pos = [(0, LAB2), (a.width + GAP, LAB2)]
    out = Image.new('RGB', (w, h), BG)
    d = ImageDraw.Draw(out)
    for im, lab, (x, y) in zip((a, b), labels, pos):
        d.text((x + 4, y - LAB2 + 8), lab, fill=INK)
        out.paste(im, (x, y))
    return out


def grey(im):
    return im.convert('L').convert('RGB')


def up(im, k):
    return im.resize((im.width * k, im.height * k), Image.NEAREST)


def hcat(*ims, gap=0):
    out = Image.new('RGB', (sum(i.width for i in ims) + gap * (len(ims) - 1), max(i.height for i in ims)), BG)
    x = 0
    for i in ims:
        out.paste(i, (x, 0))
        x += i.width + gap
    return out


def run(cmd, log):
    print('  $ ' + ' '.join(cmd))
    with open(log, 'w', encoding='utf-8') as f:
        rc = subprocess.call(cmd, stdout=f, stderr=subprocess.STDOUT)
    print(f'{"PASS" if rc == 0 else "FAIL"} p5_pack {os.path.basename(log)} rc {rc}')
    return rc == 0


# the gallery's figure and worn sheets (fingerprint_gallery.cpp): cells 40 x 52, the label column 44, the top 10, saved at
# 3x; the icon matrix (icons_gallery.cpp fpSheets): cells 20, label 44, top 12, each half 278 rows, saved at 3x
FIG_W, FIG_H, FIG_LAB, FIG_TOP = 40, 52, 44, 10
ICON_CELL, ICON_LAB, ICON_HALF = 20, 44, 278
# the icon matrix's column groups (kFpCols order): name, first column, count, caption
ICON_GROUPS = [
    ('t1t2_bodies', 0, 6, "the first hours' coats (leather gambeson, jerkin, own leather coat, bronze, bronze brigandine, iron)"),
    ('t3t5_bodies', 6, 9, "steel in the people's form and mail, scale, lamellar, brigandine, plate, leaf, alloy T4, masterwork T5"),
    ('helms_shields', 15, 10, 'the caps and helms T1-T4, the shields T1-T4'),
    ('gloves_boots', 25, 9, 'the gloves and boots T1-T4'),
    ('cloaks', 34, 2, 'the cloaks T3 and T5'),
]


def fig_block(im, col, ncol, row=0, nrow=None, k=1):
    """columns [col, col + ncol) of a figure sheet (rows [row, row + nrow) or all) with its people labels, at k x of 1x"""
    y0 = 3 * (FIG_TOP + FIG_H * row) if nrow else 0
    y1 = 3 * (FIG_TOP + FIG_H * (row + nrow)) if nrow else im.height
    lab = im.crop((0, y0, 3 * FIG_LAB, y1))
    c = im.crop((3 * (FIG_LAB + FIG_W * col), y0, 3 * (FIG_LAB + FIG_W * (col + ncol)), y1))
    out = hcat(lab, c)
    return up(out.resize((out.width // 3, out.height // 3), Image.NEAREST), k)


def icon_group(im, half, c0, n):
    lab = im.crop((0, 3 * half * ICON_HALF, 3 * ICON_LAB, 3 * (half + 1) * ICON_HALF))
    c = im.crop((3 * (ICON_LAB + ICON_CELL * c0), 3 * half * ICON_HALF, 3 * (ICON_LAB + ICON_CELL * (c0 + n)), 3 * (half + 1) * ICON_HALF))
    return up(hcat(lab, c), 2)


def sheets(argv):
    a = argv[2:]
    if len(a) < 2:
        print('usage: python tools/doll/p5_pack.py --sheets BDIR PACK_DIR [--seeds A..B] [--assemble-only]')
        return 2
    bdir, pack = a[0], a[1]
    seeds = a[a.index('--seeds') + 1] if '--seeds' in a else '1..3'
    s0, s1 = (int(v) for v in seeds.split('..'))
    work = os.path.join(pack, 'work')
    wa, wb, wi = os.path.join(work, 'after'), os.path.join(work, 'before'), os.path.join(work, 'icons')
    ok = True
    if '--assemble-only' not in a:
        for d in (wa, wb, wi):
            os.makedirs(d, exist_ok=True)
        slot = ['bash', 'tools/slot.sh']
        ok &= run(slot + [os.path.join(bdir, 'fingerprint_gallery.exe'), wa, '--seeds', '1..1', '--grey'], os.path.join(work, 'gallery_after.log'))
        ok &= run(slot + [os.path.join(bdir, 'fingerprint_gallery.exe'), wb, '--seeds', '1..1', '--grey', '--before'], os.path.join(work, 'gallery_before.log'))
        ok &= run(slot + [os.path.join(bdir, 'icons_gallery.exe'), wi, '--seeds', seeds], os.path.join(work, 'icons_gallery.log'))
    caps = {}

    def save(area, name, im, cap):
        nonlocal ok
        d = os.path.join(pack, area) if area else pack
        os.makedirs(d, exist_ok=True)
        if max(im.size) > MAX_SIDE:
            print(f'FAIL p5_pack {area}/{name}: {im.size} over {MAX_SIDE} px')
            ok = False
        im.save(os.path.join(d, name))
        top = area.split('/')[0]
        rel = os.path.relpath(os.path.join(d, name), os.path.join(pack, top) if top else pack).replace(os.sep, '/')
        caps.setdefault(top, []).append(f'{rel}: {cap}')

    def both(area, stem, before, after, cap, suffix='_before_after', gstem=None, **kw):
        save(area, f'{stem}{suffix}.png', pair_sheet(before, after, **kw), cap)
        save(area, f'{gstem or stem + "_grey"}{suffix}.png', pair_sheet(grey(before), grey(after), **kw), cap + ' (greyscale)')

    def load(d, n):
        nonlocal ok
        p = os.path.join(d, n)
        if not os.path.exists(p):
            print(f'FAIL p5_pack missing {p}')
            ok = False
            return None
        return Image.open(p).convert('RGB')

    # the worn matrices (BEFORE above AFTER), the kits and the census (BEFORE | AFTER): the gallery's sheets pasted unaltered
    worn = sorted(f for f in os.listdir(wa) if f.startswith('worn_') and f.endswith('_seed1.png')) if os.path.isdir(wa) else []
    if not worn:
        print('FAIL p5_pack: no worn matrices in ' + wa)
        ok = False
    for f in worn:
        grp = f.split('_')[1]
        A, B = load(wa, f), load(wb, f.replace('_seed1.png', '_before_seed1.png'))
        if A and B:
            both('MARKS/matrices', f[:-4], B, A, f'the {grp} pieces, the 13 makers (rows) x the audit columns, man D / R, woman D / R, '
                 'before above after (red frame: twin, amber: colour-only)', vertical=True)
    ka, kb = load(wa, 'kits_T1-T6_man_woman_seed1.png'), load(wb, 'kits_T1-T6_man_woman_before_seed1.png')
    if ka and kb:
        both('MARKS/kits', 'kits_T1-T6_man_woman_seed1', kb, ka, "every people's T1-T6 ladder kit, man then woman, before | after")
        both('MARKS/kits', 'closeup_fjord_highland_heartland_imperial_T1-T6_man_4x', fig_block(kb, 0, 6, 0, 4, 4), fig_block(ka, 0, 6, 0, 4, 4),
             'Fjordfolk, Highland, Heartland, Imperial T1-T6 (man) at 4x, before | after', suffix='')
        plaid = lambda im: hcat(fig_block(im, 4, 2, 1, 1, 8), fig_block(im, 10, 2, 1, 1, 8), gap=GAP)   # Highland: man T5-T6, woman T5-T6
        both('MARKS', 'decision3_highland_plaid_filigree_8x', plaid(kb), plaid(ka),
             "decision 3 (D120): Highland's T5-T6 (man, woman) before (the Fjordfolk knotwork it shared) | after (its own plaid lattice), 8x", suffix='')
        for t in (1, 2, 4):
            both('DOLL', f'doll_fp_t{t}', fig_block(kb, t - 1, 1, k=6), fig_block(ka, t - 1, 1, k=6),
                 f"each people's own T{t} kit (man), before | after at 6x: the dye band on rims, hems, laces and cuffs" +
                 ('; the rims stay brass, the dye in the seams, the mark engraved' if t == 4 else ''), suffix='')
    ca, cb = load(wa, 'census_guard0-2_soldier_bandit_captain_king_DR_seed1.png'), load(wb, 'census_guard0-2_soldier_bandit_captain_king_DR_before_seed1.png')
    if ca and cb:
        both('MARKS/census', 'census_guard0-2_soldier_bandit_captain_king_DR_seed1', cb, ca,
             'the watch ranks 0-2, a soldier, a bandit, a captain and the king of every people, D and R, before | after')
        both('DOLL', 'doll_fp_guard', fig_block(cb, 2, 2, k=6), fig_block(ca, 2, 2, k=6),
             "the town watch (rank 1, D and R) of the 12 peoples at 6x: the arms culture's dye on every worn piece", suffix='')
    # decision 2: the T1 leather coat ("body leather leather T1": body sheet 1, column 9), man D / R and woman D, 6x
    ba, bb = load(wa, 'worn_body_1_seed1.png'), load(wb, 'worn_body_1_before_seed1.png')
    if ba and bb:
        coat = lambda im: up(hcat(im.crop((0, 0, 3 * FIG_LAB, im.height)),
                                  im.crop((3 * (FIG_LAB + 9 * 4 * FIG_W), 0, 3 * (FIG_LAB + 9 * 4 * FIG_W + 3 * FIG_W), im.height))), 2)
        both('MARKS', 'decision2_leather_coat_laces_T1', coat(bb), coat(ba), 'decision 2 (D121): the T1 leather coat of every people, '
             "before (red frames: twins) | after (laces, hem and mark in the maker's dye), man D / R, woman D, 6x",
             suffix='_before_after_6x', gstem='decision2_leather_coat_laces_T1')
    # the icons: icons_gallery's own BEFORE above AFTER matrices, and their 6x groups (before left, after right)
    for sd in range(s0, s1 + 1):
        im, img = load(wi, f'icons_fp_seed{sd}.png'), load(wi, f'icons_fp_seed{sd}_grey.png')
        if not (im and img):
            continue
        for area, nm in (('ICONS', f'icons_fp_seed{sd}'), ('matrices', f'icons_before_after_seed{sd}')):
            save(area, nm + '.png', im, f"seed {sd}: every armour icon (36 pieces) x no maker + the 12 peoples, BEFORE (FIX_FP off) above AFTER, each people's dye at the right; red = twin, amber = colour-only")
            save(area, nm + '_grey.png', img, f'seed {sd}: the same in greyscale')
        for g, c0, n, what in ICON_GROUPS:
            for src, gs in ((im, ''), (img, '_grey')):
                save('ICONS', f'{g}_seed{sd}_before_after{gs}.png', pair_sheet(icon_group(src, 0, c0, n), icon_group(src, 1, c0, n), ('BEFORE', 'AFTER')),
                     f'seed {sd}, 6x: {what}, before left, after right' + (' (greyscale)' if gs else ''))
    for f in ('icons_body.png', 'icons_body_1x.png'):
        im = load(wi, f)
        if im:
            save('ICONS', f, im, 'the D117 sheet (Atlas cultures), AFTER: the leather, bronze, iron and steel coats still read by material')
    # the swatches (GATES) at the pack's root and in GATES/; the gallery's report
    for f in ('swatches.png', 'swatches_grey.png'):
        im = load(wa, f)
        if im:
            cap = 'the twelve dyes with their bands and the twelve motifs at 1x, 3x and as icons' + (' (greyscale)' if 'grey' in f else '')
            save('', f, im, cap)
            save('GATES', f, im, cap)
    rep = os.path.join(wa, 'report_seed1.txt')
    if os.path.exists(rep):
        os.makedirs(os.path.join(pack, 'GATES'), exist_ok=True)
        with open(rep, encoding='utf-8') as f, open(os.path.join(pack, 'GATES', 'report_seed1.txt'), 'w', encoding='utf-8', newline='\n') as g:
            g.write(f.read())
        caps.setdefault('GATES', []).append("report_seed1.txt: fingerprint_gallery's report (every column's pairs, twins, colour-only pairs, dye shares)")
    for area, lines in caps.items():
        d = os.path.join(pack, area) if area else pack
        with open(os.path.join(d, 'captions.txt' if area else 'captions_root.txt'), 'w', encoding='utf-8', newline='\n') as f:
            f.write(PENDING + ' BEFORE = FIX_FP off on the same build (the pre-P5 look); AFTER = the build of build.sha.\n' + '\n'.join(lines) + '\n')
    sha = subprocess.run(['git', 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip()
    dirty = subprocess.run(['git', 'status', '--porcelain', '--untracked-files=no'], capture_output=True, text=True).stdout.strip()
    with open(os.path.join(pack, 'build.sha'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(sha + (' +dirty' if dirty else '') + '\n')
    n = sum(len(v) for v in caps.values())
    print(f'{"PASS" if ok else "FAIL"} p5_pack --sheets wrote {n} files into {pack} (build.sha {sha[:7]}{" +dirty" if dirty else ""})')
    return 0 if ok else 1


def main(argv):
    if len(argv) != 4:
        print('usage: python tools/doll/p5_pack.py BEFORE_DIR AFTER_DIR OUT_DIR')
        return 2
    bdir, adir, out = shots_dir(argv[1]), shots_dir(argv[2]), argv[3]
    ok = True
    for lab, d in (('before', bdir), ('after', adir)):
        good = os.path.isdir(d) and any(f.endswith('.png') for f in os.listdir(d))
        print(f'{"PASS" if good else "FAIL"} p5_pack {lab}: {d}')
        ok &= good
    if not ok:
        return 1
    names = sorted(f for f in os.listdir(adir) if f.endswith('.png') and os.path.exists(os.path.join(bdir, f)))
    only = sorted(set(f for f in os.listdir(adir) if f.endswith('.png')) ^ set(f for f in os.listdir(bdir) if f.endswith('.png')))
    print(f'{"PASS" if names else "FAIL"} p5_pack pairs: {len(names)} shots in both' + (f'; in one run only: {" ".join(only)}' if only else ''))
    if not names:
        return 1
    os.makedirs(os.path.join(out, 'grey'), exist_ok=True)
    caps = ['# PENDING THE OWNER\'S REVIEW (docs/FIX_BATCH_P5.md 10 play/): BEFORE = the pre-P5 look (EMB_FIX_RULES=4095), '
            'AFTER = the fingerprint. *_zoom.png: the region that changed, 4x-8x. grey/: the same in greyscale.']
    changed = 0
    for n in names:
        a = Image.open(os.path.join(bdir, n)).convert('RGB')
        b = Image.open(os.path.join(adir, n)).convert('RGB')
        if a.size != b.size:
            print(f'FAIL p5_pack {n}: sizes differ {a.size} {b.size}')
            ok = False
            continue
        base = n[:-4]
        cap = caption(n)
        box = diff_box(a, b)
        sheet = labelled((a, b), ('BEFORE (pre-P5)', 'AFTER (P5 fingerprint)' + ('' if box else ': no change')))
        files = [(base + '.png', sheet, cap + ('' if box else ' [no change]'))]
        if box:
            changed += 1
            pair, scale, crop = zoom_pair(a, b, box)
            files.append((base + '_zoom.png', labelled(pair, ('BEFORE', 'AFTER'), scale), f'{cap}, the changed region {crop} at {scale}x'))
        for fn, im, c in files:
            if max(im.size) > MAX_SIDE:
                print(f'FAIL p5_pack {fn}: {im.size} over {MAX_SIDE} px')
                ok = False
            im.save(os.path.join(out, fn))
            im.convert('L').convert('RGB').save(os.path.join(out, 'grey', fn))
            caps.append(f'{fn}  {c}')
            caps.append(f'grey/{fn}  {c} (greyscale)')
    with open(os.path.join(out, 'captions.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(caps) + '\n')
    print(f'{"PASS" if ok else "FAIL"} p5_pack wrote {len(caps) - 1} images ({changed} of {len(names)} shots changed) + captions.txt into {out}')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(sheets(sys.argv) if sys.argv[1:2] == ['--sheets'] else main(sys.argv))
