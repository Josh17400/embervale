#!/usr/bin/env python3
"""Write tools/doll/arms_looks.txt: the production matrices M8-M12 at the envelope (docs/PLAN.md "## P2c" DOLL-C brief;
docs/P2C_CONTRACTS.md 7 "Production's hooks"). DOLL-C, wave 2.

  python tools/doll/arms_looks.py            # write the file
  python tools/doll/arms_looks.py --check    # exit 1 when the file is stale

The looks go through the one looks-field parser (art::doll::applyLookSpec) with the ladder macros (kit=<people>:<tier>:<seed>
is gear::ladderKit's real items worn through wearGear; the fields after it override). tools/doll/embed_looks.py embeds the file
in tools/tests/test_doll_arms.cpp (kArmsLooks): `rpg_test --doll-arms --matrix arms` runs C3-C6 on it, as does
`--doll-arms --looks tools/doll/arms_looks.txt`.
  M8   the 8 armour forms x both bodies x 3 builds at T1, T3, T4 and T6 (Heartland's kit, the form forced)
  M9   every alloy helm family at T4, T5 and T6 x both bodies x two hair styles a body (the people rotates), the hair sweep
       (all 8 styles, both bodies) under a tight, a flared and a tall helm at T6, and every legacy helm form (cult::HelmForm)
       at T2, T3 and T4 x both bodies
  M10  every shield form (cult::ShieldForm 1-8) x the one-hand families (sword axe mace dagger) x both bodies at T3 and T5
  M11  every weapon and form (9 blades on the sword and the greatsword, the axe, the mace, the dagger, 3 polearms, 6 bows,
       the staff) x both bodies at T3: sheathed or carried in the walk and run, drawn in the drawn gaits and the strike
  M12  the heaviest kits at T4 and T6: plate, each shoulder mass (size 4 at T4, the legend size at T6), a tower shield with
       a sword, a spear, a greatsword, the cape (T4) or the long cloak (T6), a T4 crest at F-37 and a T6 helm at F-41; both
       bodies
"""
import pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / 'tools/doll/arms_looks.txt'
PEOPLES = ['fjordfolk', 'highland', 'imperial', 'dune', 'steppe', 'marsh', 'jade', 'river', 'suntemple', 'sylvan', 'starspire']
FORMS = ['padded', 'leather', 'mail', 'scale', 'lamellar', 'brigandine', 'plate', 'leaf']      # cult::BodyArm, bodyForm = i + 1
HELMS = ['cone', 'kettle', 'warhat', 'great', 'mask', 'crested', 'onion', 'veil', 'horned', 'winged', 'sallet', 'snout', 'kabuto',
         'morion', 'plumed', 'crown', 'hood', 'cap', 'furnace', 'crystal', 'ladder']           # doll_forge.h kHelm
LEGACY = ['nasal', 'kettle', 'greathelm', 'spangen', 'horned', 'plumed', 'conical', 'masked', 'crested', 'winged']   # cult::HelmForm
SHIELDS = ['round', 'kite', 'heater', 'tower', 'crescent', 'oval', 'buckler', 'leaf']           # cult::ShieldForm, shieldForm = i + 1
BLADES = ['straight', 'leaf', 'falchion', 'scimitar', 'khopesh', 'wavy', 'broad', 'curved', 'glaive']
POLES = ['spear', 'glaive', 'halberd']
BOWS = ['classic', 'self', 'recurve', 'composite', 'crossbow', 'longbow']
MASSES = ['round', 'broad', 'tall', 'drop', 'mantle']
HAIR_M = (1, 4)    # Short, Mohawk
HAIR_W = (2, 6)    # Long, Bun
FULL = 'stay,walk,run,didle,hit1,hurt,fall,lying'
ARMED = 'stay,walk,run,dwalk,drun,didle,hit1,fall,lying'
BODY = ['man', 'woman']


def rows():
    out = []
    def add(tag, who, spec, gaits):
        out.append((tag, BODY[who], spec, gaits))
    out.append(('# M8', '', 'the 8 forms x both bodies x 3 builds at T1, T3, T4 and T6', ''))
    for fi, f in enumerate(FORMS):
        for who in (0, 1):
            for build in (0, 1, 2):
                for t in (1, 3, 4, 6):
                    hero = 'heroine=1' if who else 'hero=1'
                    add(f'm8_{f}_{"wm"[who == 0]}b{build}_t{t}', who, f'{hero},kit=heartland:{t}:7,bodyForm={fi + 1},build={build},palette=look', FULL)
    out.append(('# M9', '', 'every alloy helm family at T4-T6 x bodies x hair; the hair sweep; the legacy forms', ''))
    for hi, h in enumerate(HELMS):
        p = PEOPLES[hi % len(PEOPLES)]
        for t in (4, 5, 6):
            for who in (0, 1):
                for hair in (HAIR_W if who else HAIR_M):
                    add(f'm9_{h}_t{t}_{"wm"[who == 0]}h{hair}', who, f'kit={p}:{t}:1,shape=helm:{h},hair={hair},hairVar=0,palette=look', 'stay,walk,run,didle,hit1,fall,lying')
    for h in ('kettle', 'kabuto', 'crown'):
        for who in (0, 1):
            for hair in range(8):
                add(f'm9s_{h}_{"wm"[who == 0]}h{hair}', who, f'kit=steppe:6:1,shape=helm:{h},hair={hair},hairVar=1,palette=look', 'stay,walk,run,didle,fall,lying')
    for li, h in enumerate(LEGACY):
        for t in (2, 3, 4):
            for who in (0, 1):
                hero = 'heroine=1' if who else 'hero=1'
                add(f'm9l_{h}_t{t}_{"wm"[who == 0]}', who, f'{hero},kit=heartland:{t}:7,helmet=1,helmStyle={min(t, 3)},helmForm={li + 1},palette=look', 'stay,walk,run,didle,hit1,fall,lying')
    out.append(('# M10', '', 'every shield form x the one-hand families x both bodies at T3 and T5', ''))
    for si, s in enumerate(SHIELDS):
        for w in ('sword', 'axe', 'mace', 'dagger'):
            for who in (0, 1):
                for t in (3, 5):
                    add(f'm10_{s}_{w}_{"wm"[who == 0]}_t{t}', who, f'kit=imperial:{t}:2,wpn={w},shield=1,shieldStyle={min(t, 3)},shieldForm={si + 1},palette=look', ARMED + ',block')
    out.append(('# M11', '', 'every weapon and form x both bodies at T3: sheathed, carried and drawn', ''))
    for who in (0, 1):
        w = 'wm'[who == 0]
        for bi, b in enumerate(BLADES):
            add(f'm11_sword_{b}_{w}', who, f'kit=dune:3:1,wpn=sword,bladeForm={bi + 1},palette=look', ARMED)
            add(f'm11_great_{b}_{w}', who, f'kit=dune:3:1,wpn=greatsword,bladeForm={bi + 1},palette=look', ARMED)
        for x in ('axe', 'mace', 'dagger', 'staff'):
            add(f'm11_{x}_{w}', who, f'kit=dune:3:1,wpn={x},palette=look', ARMED)
        for pi, p in enumerate(POLES):
            add(f'm11_pole_{p}_{w}', who, f'kit=dune:3:1,wpn=spear,polearmForm={pi + 1},palette=look', ARMED)
        for bi, b in enumerate(BOWS):
            add(f'm11_bow_{b}_{w}', who, f'kit=dune:3:1,wpn=bow,bowForm={bi},palette=look', ARMED)
    out.append(('# M12', '', 'the heaviest kits at T4 and T6: plate, each mass, tower + sword, spear, greatsword, cape / long cloak', ''))
    for m in MASSES:
        for t in (4, 6):
            layer = 'cape' if t == 4 else 'cape'
            for who in (0, 1):
                w = 'wm'[who == 0]
                base = f'kit=fjordfolk:{t}:1,bodyForm=7,shape=mass:{m}+layer:{layer},cloak={2 if t == 4 else 3}'
                crest = ',crest=3' if t == 4 else ''
                add(f'm12_{m}_t{t}_{w}_tower', who, f'{base},wpn=sword,shield=1,shieldStyle=3,shieldForm=4{crest},palette=look', ARMED + ',block')
                add(f'm12_{m}_t{t}_{w}_spear', who, f'{base},wpn=spear{crest},palette=look', ARMED)
                add(f'm12_{m}_t{t}_{w}_great', who, f'{base},wpn=greatsword{crest},palette=look', ARMED)
    return out


def text():
    head = ['# P2c production matrices M8-M12 (DOLL-C, wave 2; written by tools/doll/arms_looks.py, embedded by tools/doll/embed_looks.py).',
            '# tag who spec gaits; macros as tools/doll/ladder_looks.txt (kit=<people>:<tier>:<seed>; fields after the kit override it).']
    lines = head[:]
    for tag, who, spec, gaits in rows():
        if tag.startswith('#'):
            lines.append(f'{tag}: {spec}')
            continue
        lines.append(f'{tag:<24} {who:<6} {spec}  {gaits}')
    return '\n'.join(lines) + '\n'


if __name__ == '__main__':
    t = text()
    if '--check' in sys.argv:
        ok = OUT.exists() and OUT.read_text(encoding='utf-8') == t
        print('arms_looks: ' + ('in step' if ok else 'STALE: run python tools/doll/arms_looks.py'))
        sys.exit(0 if ok else 1)
    OUT.write_text(t, encoding='utf-8', newline='\n')
    n = sum(1 for l in t.splitlines() if l and not l.startswith('#'))
    print(f'arms_looks: {n} looks written to {OUT.relative_to(ROOT)}')
