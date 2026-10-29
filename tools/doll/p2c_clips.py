#!/usr/bin/env python3
# tools/doll/p2c_clips.py: the P2c style slice's clip lines (SLICE-DOLL; docs/P2C_CONTRACTS.md 5, STYLE_P2 25.8), written
# from the shipped clips so every number the motion method froze (plants, bob, lean, hip, legs, hangs, hair) is kept and only
# the arms, the weapon and the shield change. It rewrites the files it owns whole:
#   clips/actions/shield.clip   a shield bearer's LOCO idle / walk / hurt and RUN (`when shield`): the off arm still on the
#                               shield (armL=- sharm=<k> shield=rest); the ride posture's reins arm with the shield
#   clips/actions/sword_shield.clip  the sword with a shield (`family sword ... when shield`): the drawn idle, walk and run
#                               facing D / U (DRAWN; R / L are sword.clip's `when shield,sword`), hit 1 (A3's swing, the
#                               recover in the drawn guard) and block (ACTION), the falls and lying (DEATH: shield=lie)
#   clips/actions/spear_carry.clip  the spear carried upright (`family spear when spear`), the signed-off side carry
#   clips/actions/staff_carry.clip  the staff carried upright in the right fist (`family staff when staff`, PLAN D81)
#   clips/death.clip (its generated end section)   the spear's and the bow's falls and lying (`family spear|bow when
#                               spear|bow`: held=drop; the archer's quiver sheath=lie)
#   clips/actions/spear.clip and bow.clip are SLICE-ARMS's lines (integrated 2026-10-23, `when spear|bow`); this tool no
#   longer writes them (--families is refused so the integrated files are never overwritten by the old placeholders), and
#   since the wave-2 fix round it no longer re-steps spear.clip either (its steps are authored in the file).
# (wave-2 fix round, ACTIONS r1) a fixed point: run on the integrated tree (or a fresh copy of its clips) it rewrites nothing
# and exits 0; every hand edit the ledger used to keep (the spear's side carry, DOLL-C's three-beat block) lives here now.
# Run it after a change here or to the shipped lines it copies, then `node tools/doll/pxc.mjs`:
#   python tools/doll/p2c_clips.py [--root DIR]      (default rpg/art/doll; an artist's isolated root works the same)
# The keys it names are the art areas' (docs/P2C_CONTRACTS.md 4): arms sp* (spear, both fists), bw* (bow), the shield arm's
# sharm keys (rest walk run hurt guard block fling lie) and the shield's (rest guard block lie); the weapon's angle views
# (a000 ... a337) through `weapon=<deg>,front|back`, the bow's string states through held=<key> (rest nock draw full release),
# held=drop on the ground. A key a map lacks shows as dollMissing / C3a, never as a silent hole.
import argparse
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
FACINGS = 'DURL'
DRAWN_SHIELD_FACINGS = 'DU'   # (fix round r1) the sword-and-shield drawn lines facing R / L are sword.clip's `when shield,sword`
RECOVER_IN_GUARD = False      # (fix round r1) the shield-bearer's hit 1 recover in the drawn guard: proposed, see sword_shield()


def parse(path):
    """clip blocks: [{'body', 'head': the clip line's words, 'name', 'facing', 'attrs': dict, 'frames': [ordered (k, v) lists]}]"""
    out, body, cur = [], None, None
    for raw in open(path, encoding='utf-8').read().splitlines():
        t = raw.strip()
        if not t or t.startswith('#'):
            continue
        w = t.split()
        if w[0] == 'body':
            body, cur = w[1], None
        elif w[0] == 'clip':
            attrs = {}
            i = 3
            while i + 1 < len(w):
                attrs[w[i]] = w[i + 1]
                i += 2
            cur = {'body': body, 'name': w[1], 'facing': w[2], 'attrs': attrs, 'frames': []}
            out.append(cur)
        elif w[0] == 'f' and cur is not None:
            kv = []
            for tok in w[1:]:
                k, _, v = tok.partition('=')
                kv.append([k, v])
            cur['frames'].append(kv)
    return out


def find(clips, body, name, facing, **attrs):
    for c in clips:
        if c['body'] == body and c['name'] == name and c['facing'] == facing and all(c['attrs'].get(k) == v for k, v in attrs.items()) and \
                all(k in attrs for k in c['attrs']):
            return c
    raise KeyError(f'{body} {name} {facing} {attrs}')


def setk(kv, key, val):
    """set (or add, or with None remove) one key of a frame, keeping the others' order"""
    for p in kv:
        if p[0] == key:
            if val is None:
                kv.remove(p)
            else:
                p[1] = val
            return kv
    if val is not None:
        kv.append([key, val])
    return kv


def fline(kv):
    return 'f ' + ' '.join(f'{k}={v}' for k, v in kv)


def copyf(kv):
    return [list(p) for p in kv]


def block(head, frames):
    return [head] + [fline(f) for f in frames]


# ------------------------------------------------------------------------------------------------- the shield
def shield_loco(loco):
    L = ['# GENERATED by tools/doll/p2c_clips.py from clips/loco.clip and clips/postures.clip (edit the generator, not this file).',
         '# A shield bearer (`when shield`) at rest, walking, hurt and running: the shipped lines with the off arm on the shield',
         '# (armL=- sharm=<key> shield=rest): the arm does not swing while it carries the shield (STYLE_P2 25.5); the run hugs it',
         '# to the body (25.9).']
    for body in ('man', 'woman'):
        L += ['', f'body {body}']
        for name, sh in (('idle', 'rest'), ('walk', 'walk'), ('hurt', 'hurt'), ('run', 'run')):
            for fc in FACINGS:
                src = find(loco, body, name, fc)
                fr = [setk(setk(copyf(f), 'armL', '-'), 'sharm', sh) for f in src['frames']]   # (the shield at rest: its rest view)
                if name == 'hurt' and fc in 'RL':
                    # (SLICE-DOLL i3, the blind panel: the shield vanished in the standing hurt) facing right the rest view
                    # sits wholly behind the body, so a blow taken out of the guard (shield at the chest) lost the shield for
                    # the flinch. The flinch keeps it up at the chest (sharm=guard shield=guard) and is knocked a pixel back
                    for f in fr:
                        setk(f, 'sharm', 'guard'); setk(f, 'shield', 'guard')
                        setk(f, 'at', '-1,0' if fc == 'R' else '1,0')
                L += block(f'clip {name} {fc} when shield', fr)
    return L


def shield_ride(postures):
    """(wave-2 fix round, SHIELDS' proposal, ACTIONS r1) a shield bearer astride: the off arm carries the shield on the reins
    (armL=- sharm=ride shield=rest; shield/arm's `ride` keys, the rest drawing on the reins grip). Until the posture clips
    honour `when` (DOLL: pool::postureClips passes the recipe to clipsOf, as every other part does) these lines are inert and
    the generic ride draws the reach arm and the shield arm's rest view together."""
    L = ['', '# The ride (posture ride `when shield`): the shield on the reins arm.']
    for body in ('man', 'woman'):
        L += ['', f'body {body}']
        for fc in FACINGS:
            src = find(postures, body, 'idle', fc, posture='ride')
            fr = [setk(setk(setk(copyf(f), 'armL', '-'), 'sharm', 'ride'), 'shield', 'rest') for f in src['frames']]
            L += block(f'clip idle {fc} posture ride when shield', fr)
    return L


def sword_shield(loco_sword, sword, death):
    L = ['# GENERATED by tools/doll/p2c_clips.py from clips/actions/sword.clip and clips/death.clip (edit the generator).',
         '# The sword with a shield (`family sword ... when shield`, STYLE_P2 25.8): the drawn guard with the shield at the chest',
         '# (sharm=guard shield=guard), the drawn run with it hugged (sharm=run shield=rest); A3\'s hit 1 with the shield on the',
         '# arm (the off arm tucks it: sharm=guard); block squares the shield to the threat (sharm=block shield=block, 150 ms,',
         '# the sword held back in the guard); the falls fling it and the lying body has it flat beside (shield=lie).']
    for body in ('man', 'woman'):
        L += ['', f'body {body}']
        for name, v in (('idle', '1'), ('walk', '1'), ('run', '2')):
            # (wave-2 fix round, ACTIONS r1) facing D and U only: the drawn idle, walk and run facing R and L are
            # sword.clip's own `when shield,sword` lines (ACTIONS-1H r4, the judge's W7: the library angle a045 / a135, the
            # shield a row lower facing L), which load first and replaced these; the generator no longer writes the dead copies
            for fc in DRAWN_SHIELD_FACINGS:
                src = find(sword, body, name, fc, part='drawn', family='sword', variant=v)
                sh, sv = ('guard', 'guard')   # (SLICE-HELM-SHIELD's lines: the guard kept in the drawn run too)
                fr = [setk(setk(setk(copyf(f), 'armL', '-'), 'sharm', sh), 'shield', sv) for f in src['frames']]
                head = f'clip {name} {fc} part drawn family sword variant {v} when shield'
                L += block(head, fr)
        for name in ('windup', 'strike', 'follow'):
            for fc in FACINGS:
                src = find(sword, body, name, fc, family='sword', variant='0')
                # (SLICE-HELM-SHIELD's lines) the shield tucked through the swing (sharm=shtuck), back in the guard on the recover
                fr = [setk(setk(setk(copyf(f), 'armL', '-'), 'sharm', 'guard' if name == 'follow' and i == len(src['frames']) - 1 else 'shtuck'), 'shield', 'guard')
                      for i, f in enumerate(src['frames'])]
                if name == 'follow' and fc in 'RL' and RECOVER_IN_GUARD:
                    # (wave-2 fix round, ACTIONS r1: PROPOSED, off until the owner signs the freeze, PLAN D72) the recover
                    # lands in the drawn guard as sword.clip's `when shield,sword` idle draws it facing R / L (the library
                    # angle a045 / a135 with the guard arm; facing L the shield a row lower, sharm=walk shield=rest): A3's own
                    # guard view (a3r3 / a3l3) puts the hilt at the heroine's mouth facing R and the shield's rim over it
                    # facing L (w7a hit1 R / L: 10 / 15 face px with D87 in, 4 / 4 with this), and the swing ends a pose
                    # away from the guard it returns to. It changes 1-2 signed-off frames on every sword-and-shield line
                    # (slice lt1, lt1w, lt2, lt5 among them), so it waits for the owner
                    f = fr[-1]
                    setk(f, 'armR', 'guard'); setk(f, 'held', None); setk(f, 'weapon', '45,front' if fc == 'R' else '135,back')
                    if fc == 'L':
                        setk(f, 'sharm', 'walk'); setk(f, 'shield', 'rest')
                L += block(f'clip {name} {fc} family sword variant 0 when shield', fr)
        for fc in FACINGS:   # block (SLICE-HELM-SHIELD's lines, re-cut by DOLL-C in wave 2): the shield squared on the beat
            side = fc in 'RL'
            # (wave 2, DOLL-C's surgical fix, moved here by ACTIONS r1 so a clean run keeps it) three beats in A3's own sword
            # views (the hero's guard key sw_<f>0 / held a3<f>0, so the blade never reads as a second pose): the guard
            # (30 ms, the shield still at the chest), the impact (50 ms, the shield squared, the knees buckled, the spark,
            # braced a step further back) and the hold (70 ms). The steps and lean are written here (p2c_weight leaves the
            # block alone): facing R / L the brace 1-3-2 px back, lean 1 toward the back; facing D / U 1-2-1 rows
            k = {'D': 'd', 'U': 'u', 'R': 'r', 'L': 'l'}[fc]
            back = {'D': (0, -1), 'U': (0, 1), 'R': (-1, 0), 'L': (1, 0)}[fc]
            steps = (1, 3, 2) if side else (1, 2, 1)
            lean = f' lean={-1 if fc == "R" else 1}' if side else ''
            head = f'armL=- armR=sw_{k}0 held=a3{k}0{" hilt=-" if side else ""}'
            sheath = 'rest' if side else 'drawn'
            L += [f'clip block {fc} family sword when shield']
            for i, (legs, sh, bob, fx, ms) in enumerate((('buckle' if side else 'drawn', 'guard', 1, '', 30),
                                                         ('buckle', 'block', 2, ' fx=spark', 50),
                                                         ('buckle', 'block', 2, '', 70))):
                at = f'{back[0] * steps[i]},{back[1] * steps[i]}'
                L += [f'f {head} legs={legs} neck=rest sheath={sheath} torso=rest tuft=- sharm={sh} shield={sh}{lean} bob={bob}{fx} ms={ms} at={at}']
        for name in ('fall', 'lying'):
            for fc in FACINGS:
                for v in ('0', '1'):
                    try:
                        src = find(death, body, name, fc, variant=v, family='sword')
                    except KeyError:
                        continue
                    fr = []
                    for i, f in enumerate(src['frames']):
                        f = setk(copyf(f), 'armL', '-')
                        lie = name == 'lying' or i == len(src['frames']) - 1
                        # (INTEGRATE i1: C3c) facing right the shield is the far side's, behind the body: its fling would
                        # pull the hand off the shield's back, so the arm keeps it at rest on the first topple frame
                        # (wave-2 fix round, ACTIONS r1) SHIELDS' r2-r4 buckle line (frame 1 sharm=guard) was tried and not kept:
                        # C3c is 0 on the falls at int4 without it, and with it the oval reads over 2 shin columns facing D and the
                        # tower shows a 1 px hole facing U (4 new C3 lines on the 24 sword-and-shield looks of arms_looks)
                        setk(f, 'sharm', 'lie' if lie else ('fling' if i == 0 and fc != 'R' else 'rest'))
                        if lie: setk(f, 'shield', 'lie')
                        fr.append(f)
                    L += block(f'clip {name} {fc} variant {v} family sword when shield', fr)
    return L


# ------------------------------------------------------------------------------------------------- the spear
# the angle (degrees counter-clockwise from screen-right) and depth of the spear by facing (STYLE_P2 25.7: behind the body
# when its hand is the far one): carried upright; levelled at the hip in the guard (tip forward); the thrust at full reach
CARRY = {'D': (90, 'front'), 'U': (90, 'back'), 'R': (90, 'front'), 'L': (90, 'back')}
CARRY_RUN = {'D': (90, 'front'), 'U': (90, 'back'), 'R': (67, 'front'), 'L': (112, 'back')}
GUARD = {'D': (247, 'front'), 'U': (67, 'back'), 'R': (0, 'front'), 'L': (180, 'back')}
WIND = {'D': (247, 'front'), 'U': (67, 'back'), 'R': (0, 'front'), 'L': (180, 'back')}
THRUST = {'D': (270, 'front'), 'U': (90, 'back'), 'R': (0, 'front'), 'L': (180, 'back')}
BLOCK = {'D': (157, 'front'), 'U': (22, 'back'), 'R': (45, 'front'), 'L': (135, 'back')}


def wpn(table, fc):
    a, d = table[fc]
    return f'{a},{d}'


def spear(loco, death):
    L = ['# GENERATED by tools/doll/p2c_clips.py from clips/loco.clip and clips/death.clip (edit the generator, not this file).',
         '# The spear (STYLE_P2 25.7-25.8): carried upright in the right hand at rest (LOCO / RUN `when spear`, the hand at mid',
         '# shaft; tilted one step forward in the run), levelled at the hip in the drawn guard with both fists on the haft (arms',
         '# spguard, spguardL: the off fist 6-8 px along the haft), the drawn walk and run on the shipped legs, hit 1 the thrust',
         '# (wind: both fists drawn to the back hip, weight back; strike: full reach, the front foot planted, the streak; follow:',
         '# the shaft pulled back to the guard), block with the haft across the body at 45 degrees; it lands beside the hand when',
         '# the body falls (held=drop).']
    for body in ('man', 'woman'):
        L += ['', f'body {body}']
        # the drawn guard, walk and run (DRAWN)
        for name, v in (('idle', '1'), ('walk', '1'), ('run', '2')):
            for fc in FACINGS:
                src = find(loco, body, name, fc)
                fr = []
                for f in src['frames']:
                    f = copyf(f)
                    setk(f, 'armR', 'sprun' if name == 'run' else 'spguard')
                    setk(f, 'armL', 'sprunL' if name == 'run' else 'spguardL')
                    setk(f, 'hilt', None)
                    setk(f, 'weapon', wpn(CARRY_RUN if name == 'run' else GUARD, fc))
                    fr.append(f)
                L += block(f'clip {name} {fc} part drawn family spear variant {v}', fr)
        # hit 1: the thrust (ACTION)
        for fc in FACINGS:
            side = fc in 'RL'
            stance = 'stance' if side else 'drawn'
            lean = 1 if fc == 'R' else -1 if fc == 'L' else 0
            fierce = '' if fc == 'U' else ' expr=fierce'   # (the face is away facing up)
            L += [f'clip windup {fc} family spear variant 0',
                  f'f armL=spwindL armR=spwind legs={stance} neck=rest sheath=rest torso=rest bob=1 lean={-2 * lean} weapon={wpn(WIND, fc)} ms=70',
                  f'f armL=spwindL armR=spwind2 legs={stance} neck=rest sheath=rest torso=rest bob=1 lean={-2 * lean} weapon={wpn(WIND, fc)} ms=70',
                  f'clip strike {fc} family spear variant 0',
                  f'f armL=spthrustL armR=spthrust legs={"lunge" if side else "drawn"} neck=rest sheath=rest torso=rest{fierce} fx=a1 bob=1 lean={2 * lean} weapon={wpn(THRUST, fc)} ms=110',
                  f'f armL=spthrustL armR=spthrust legs={"lunge" if side else "drawn"} neck=rest sheath=rest torso=rest{fierce} fx=a2 bob=1 lean={2 * lean} weapon={wpn(THRUST, fc)} ms=60',
                  f'clip follow {fc} family spear variant 0',
                  f'f armL=spbackL armR=spback legs={stance} neck=rest sheath=rest torso=rest bob=1 lean={lean} weapon={wpn(GUARD, fc)} ms=70',
                  f'f armL=spguardL armR=spguard legs={stance} neck=rest sheath=rest torso=rest weapon={wpn(GUARD, fc)} ms=60',
                  f'clip block {fc} family spear',
                  f'f armL=spblockL armR=spblock legs={stance} neck=rest sheath=rest torso=rest bob=1 weapon={wpn(BLOCK, fc)} ms=150']
    return L


# (wave-2 fix round 2) the spear fall's first frame (the hit) holds the haft in the flinch's right arm
FALL_HOLD = {'D': ('idle', '90,back'), 'U': ('idle', '90,back'), 'R': ('sp_hurt', None), 'L': ('sp_hurt', None)}


def death_family(death, body, fam):
    L = []
    for name in ('fall', 'lying'):
        for fc in FACINGS:
            for v in ('0', '1'):
                try:
                    src = find(death, body, name, fc, variant=v, family='sword')
                except KeyError:
                    continue
                fr = []
                for i, f in enumerate(src['frames']):
                    f = setk(copyf(f), 'hilt', None)
                    lie = name == 'lying' or i == len(src['frames']) - 1
                    if fam == 'bow':
                        setk(f, 'sheath', 'lie' if lie else 'rest')   # (the quiver and the slung bow share the sheath channel)
                    # (i2) the spear stays in the fist through the recoil and the buckle (its carry view, as in the hurt) and
                    # only leaves it on the topple; the bow rides the back (bowback) until it lands beside the hand
                    # (i3) facing down / up the buckle keeps the carry drawn at the fist (carry_dv: no leaf floating above the helm); the
                    # recoil flings the arms wide, so its spear stays the rest view (at the fist it crossed the face)
                    # (INTEGRATE i3: SLICE-ARMS's fall) the spear stays upright behind the body on the hit (held=fall), tips back
                    # 45 degrees as the body buckles (weapon=135|45,back), then lies (held=drop): the shaft never crosses the face
                    if fam == 'spear' and not lie and i == 0:
                        setk(f, 'held', 'fall')
                        # (wave-2 fix round 2, D81: a fall frame is a carry, its grip read from the arm's grip or hand mark)
                        # the hit keeps the haft in the right fist, the flinch's arm with the off arm flung: the fling arms
                        # mark no hand, so the fall view (grip-anchored, behind the body) stood at the default grip, unheld
                        setk(f, 'armR', FALL_HOLD[fc][0])
                        if FALL_HOLD[fc][1]:   # facing D / U upright at the fist (the fall view's slant stood wide of it)
                            setk(f, 'held', None)
                            setk(f, 'weapon', FALL_HOLD[fc][1])
                    elif fam == 'spear' and not lie:
                        setk(f, 'held', None)
                        setk(f, 'weapon', '135,back' if fc in 'DR' else '45,back')
                        if fc == 'L':   # (fix round 2) the far fist on the haft as it tips back (armR=- held nothing)
                            setk(f, 'armR', 'sp_hurt')
                    else:
                        setk(f, 'held', 'drop' if lie else (None if fam == 'spear' else '-'))
                    fr.append(f)
                # (`when <family>`: a fighter's; a civil trade with the same tool (the hunter's bow) has the flag unarmed and
                # keeps the shipped falls)
                L += block(f'clip {name} {fc} variant {v} family {fam} when {fam}', fr)
    return L


# ------------------------------------------------------------------------------------------------- the bow
def bow(loco, death):
    L = ['# GENERATED by tools/doll/p2c_clips.py from clips/loco.clip and clips/death.clip (edit the generator, not this file).',
         '# The bow (STYLE_P2 25.8.6): on the back with the quiver at rest (the sheath channel: no LOCO lines of its own); the drawn',
         '# idle low in the left hand with an arrow nocked (armL=bwlow, armR=bwnock, held=nock: the bow on gripL), the drawn walk',
         '# and run on the shipped legs; the draw over the windup: nock (held=nock), chest (held=draw), cheek (held=full: the bow',
         '# arm straight, the elbow up); the release (held=release, the twang, the bow hand recoiling a pixel); it lands beside',
         '# the hand when the body falls (held=drop), the quiver under the body (sheath=lie). The bow on the back rides its own',
         '# channel (`keys bowback`: rest in every LOCO / RUN frame, off (bowback=-) while it is in hand).']
    for body in ('man', 'woman'):
        L += ['', f'body {body}']
        for name, v in (('idle', '1'), ('walk', '1'), ('run', '2')):
            for fc in FACINGS:
                src = find(loco, body, name, fc)
                fr = []
                for f in src['frames']:
                    f = copyf(f)
                    setk(f, 'armL', 'bwrun' if name == 'run' else 'bwlow')
                    setk(f, 'armR', 'bwrunR' if name == 'run' else 'bwnock')
                    setk(f, 'hilt', None)
                    setk(f, 'held', 'rest' if name == 'run' else 'nock')
                    setk(f, 'bowback', '-')   # (the bow is in hand: its back view off; the quiver stays on the sheath channel)
                    fr.append(f)
                L += block(f'clip {name} {fc} part drawn family bow variant {v}', fr)
        for fc in FACINGS:
            side = fc in 'RL'
            stance = 'stance' if side else 'drawn'
            fierce = '' if fc == 'U' else ' expr=fierce'   # (the face is away facing up)
            L += [f'clip draw {fc} family bow',
                  f'f armL=bwaim armR=bwnock legs={stance} neck=rest sheath=rest bowback=- torso=rest held=nock ms=100',
                  f'f armL=bwaim armR=bwdraw legs={stance} neck=rest sheath=rest bowback=- torso=rest held=draw ms=100',
                  f'f armL=bwaim armR=bwfull legs={stance} neck=rest sheath=rest bowback=- torso=rest held=full{fierce} ms=100',
                  f'clip release {fc} family bow',
                  f'f armL=bwrecoil armR=bwrelease legs={stance} neck=rest sheath=rest bowback=- torso=rest held=release fx=a1{fierce} ms=80']
    return L



# ------------------------------------------------------------------------------------------------- the spear carried
# (side views keep the rest view behind the head: drawn at the fist the haft crossed the helm and the face)
CARRY_FACINGS = 'DU'
RUN_CARRY = False


def carry_dv(f, fc):
    """(SLICE-DOLL i3, the blind panel's worst gear item) facing down and up the spear's `rest` view rides the neck layer,
    so the plate pauldron and the wide helm hid the haft from the fist to the leaf and the leaf floated above the helm.
    Facing D / U the carry is drawn at the fist instead (`held=carry`: the Held layer at the grip, the hand redrawn over
    the haft), the whole spear one line from the butt to the tip."""
    return setk(f, 'held', 'carry') if fc in CARRY_FACINGS else f


def spear_carry(loco):
    """(INTEGRATE i1, SLICE-ARMS's ask + W5) a spear fighter's LOCO walk and RUN (`family spear when spear`): the shipped
    lines with the carrying (right) arm held still, so the upright spear rides the body's bob instead of the arm's swing
    (it lifted clear of the fist at the run's pump peaks: W5). The hold: idle facing down / up, the walk's neutral N facing
    right, the run's neutral pump pN facing right and left, the far arm's own visible key in the walk facing left."""
    L = ['# GENERATED by tools/doll/p2c_clips.py from clips/loco.clip (edit the generator, not this file).',
         '# The spear carried upright (`family spear when spear`): the walk and the run with the right arm still on the haft.']
    for body in ('man', 'woman'):
        L += ['', f'body {body}']
        for name in ('idle', 'walk', 'hurt', 'run'):
            for fc in FACINGS:
                src = find(loco, body, name, fc)
                if name == 'hurt':
                    hold = 'sp_hurt' if fc in 'RL' else None   # (ACTIONS-2H, wave 2) the side flinch keeps the haft in the fist
                elif fc in 'DU':
                    hold = 'idle'
                elif name == 'run' and fc == 'R':
                    hold = 'pN'
                else:
                    # (ACTIONS-2H, wave 2, signed off) the side idle and walk: the fist on the haft. (wave-2 fix round 2,
                    # D81: a carry's grip reads the arm's grip or hand mark) the run facing L too: its far arm's pump pN
                    # draws no fist and marks no hand, so the upright run carry rode the chest with nothing holding it
                    hold = 'sp_carry'
                fr = [setk(copyf(f), 'armR', hold) if hold else copyf(f) for f in src['frames']]
                # (i3, B10) the RUN part keeps the rest view facing down / up: the carry is 39 px (the rest's run view 38),
                # which put the knight's RUN atlas at 64.2 KB of 64. SLICE-ARMS ask: a 38 px `carry ... when run replace`
                # view in held/weapons/spear.pxm; then RUN_CARRY = True
                if (name != 'run' or RUN_CARRY) and name != 'hurt':
                    fr = [carry_dv(f, fc) for f in fr]
                if name == 'hurt':   # (i3, the panel: the hurt barely read) the flinch knocked a pixel back
                    fr = [setk(f, 'at', {'R': '-1,0', 'L': '1,0', 'D': '0,-1', 'U': '0,1'}[fc]) for f in fr]
                fr = [side_carry(f, fc, name) for f in fr]
                if name == 'hurt' and fc == 'D':
                    # (wave-2 fix round 3, ACTIONS; C3d) the flinch facing D raises the spear fist to the shoulder (runF), under a
                    # size-6 pauldron at T6: the shoulders hunch with it (pauld=up), so the fist on the haft reads (10 m12 lines)
                    fr = [setk(f, 'pauld', 'up') for f in fr]
                L += block(f'clip {name} {fc} family spear when spear', fr)
        # (SLICE-DOLL i2, B10 DRAWN) the drawn walk and run carry the spear upright too: the levelled guard (42 px wide in
        # every frame) put the knight's DRAWN atlas at 319 KB of 256; the guard stays on the drawn idle, the thrust and the
        # block. Same lines, `part drawn` (walk variant 1, run variant 2: the sword's drawn slots)
        L += ['', f'# the drawn walk and run ({body}): both fists on the haft (PLAN D81): the port, the charge facing D on the guard']
        for name, var in (('walk', 1), ('run', 2)):
            for fc in FACINGS:
                src = find(loco, body, name, fc)
                fr = [drawn_port(copyf(f), fc, name) for f in src['frames']]
                L += block(f'clip {name} {fc} part drawn family spear variant {var} when spear', fr)
    return L


# (wave-2 fix round 2, ACTIONS; PLAN D81: the off fist is asked on every DRAWN frame) the drawn walk and run hold the spear
# in both fists on the shipped legs: facing R / L the port (the block's high hold, sp_hi: the haft 67.5 degrees forward,
# the head high), where the levelled guard would stand 42 px wide in every frame (B10 DRAWN); facing D the walk at the
# port across the body (the brace, sp_brace a045: the foreshortened guard all but vanishes at 1x on a walking body) and
# the run, the charge, on the guard levelled at the viewer (fD, the drawn idle's arms); facing U the guard (fU). The
# upright one-handed carry they wore since SLICE-DOLL i2 was a carry on a DRAWN frame (C3d: 1,100 lines without, or off,
# the off fist). Per gait and facing: (armR, armL, held view, library angle)
DRAWN_HOLD = {('walk', 'D'): ('sp_brace', 'sp_brace', 'a045', '45,front'), ('run', 'D'): ('sp_guard', 'sp_hit', 'fD', '337.5,front'),
              'U': ('sp_guard', 'sp_guard', 'fU', '157.5,back'),
              'R': ('sp_hi', 'sp_hi', 'a067', '67.5,front'), 'L': ('sp_hi', 'sp_hi', 'a112', '112.5,front')}


def drawn_port(f, fc, name):
    armR, armL, held, w = DRAWN_HOLD.get((name, fc)) or DRAWN_HOLD[fc]
    setk(f, 'armL', armL)
    setk(f, 'armR', armR)
    setk(f, 'hilt', None)
    setk(f, 'held', None)
    setk(f, 'held', held)
    setk(f, 'weapon', w)
    return f


# (wave 2, ACTIONS-2H's signed-off side carry, moved here by ACTIONS r1: int4's regeneration had reverted it and put the
# haft over the face, W7 18) the held view and the library angle per gait and facing: the side idle and walk carry the
# spear in the fist sloped back over the shoulder (carryR / carryL, a112 / a067 behind the body); the side run and the
# side flinch the upright run carry (runcarry, a090); the flinch facing D / U holds it at the fist's angle (a112 / a067)
SIDE_CARRY = {('idle', 'R'): ('carryR', '112.5,back'), ('idle', 'L'): ('carryL', '67.5,back'),
              ('walk', 'R'): ('carryR', '112.5,back'), ('walk', 'L'): ('carryL', '67.5,back'),
              ('run', 'R'): ('runcarry', '90,back'), ('run', 'L'): ('carryL', '67.5,back'),
              ('hurt', 'R'): ('runcarry', '90,back'), ('hurt', 'L'): ('runcarry', '90,back'),
              ('hurt', 'D'): ('a112', '112.5,front'), ('hurt', 'U'): ('a067', '67.5,front')}


def side_carry(f, fc, name):
    hv = SIDE_CARRY.get((name, fc))
    if hv:
        setk(f, 'held', None)
        setk(f, 'held', hv[0])
        setk(f, 'weapon', hv[1])
    return f


# ------------------------------------------------------------------------------------------------- the staff carried
# (wave-2 fix round, D81: WEAPONS' r4 proposal, its staff_carry.py folded in by ACTIONS r1) the staff fighter carried
# upright in the right FIST (D5, one-handed like the spear): LOCO idle / walk / hurt, RUN and the fall, with an arm key that
# marks `grip` (facing D / U the loco `guard` arm, the idle arm with the fist a row up; facing R / L the spear's `sp_carry`
# arm) and the staff's carry views (held/weapons/staff.pxm: carry facing D / U, carryR / carryL the upright side carry,
# runcarry the run tilted one step). The fall keeps it in the fist through the recoil and the buckle (the off arm flung)
# and lays it beside the near hand on the topple (held=drop, C3e as before).
def staff_hold(fc):
    return 'guard' if fc in 'DU' else 'sp_carry'


def staff_held(fc, run):
    return {'D': 'carry', 'U': 'carry', 'R': 'runcarry' if run else 'carryR', 'L': 'runcarry' if run else 'carryL'}[fc]


def staff_carry(loco, death):
    L = ['# GENERATED by tools/doll/p2c_clips.py from clips/loco.clip and clips/death.clip (edit the generator, not this file).',
         '# The staff carried upright in the right fist (`family staff when staff`, PLAN D5 / D81; WEAPONS r4): LOCO, RUN and the',
         '# fall. The fist marks `grip` (D / U the loco guard arm, R / L sp_carry); the run tilts it one step (runcarry).']
    for body in ('man', 'woman'):
        L += ['', f'body {body}']
        for name in ('idle', 'walk', 'hurt', 'run'):
            for fc in FACINGS:
                src = find(loco, body, name, fc)
                fr = []
                for f in src['frames']:
                    f = copyf(f)
                    setk(f, 'armR', staff_hold(fc)); setk(f, 'held', staff_held(fc, name == 'run')); setk(f, 'weapon', None)
                    fr.append(f)
                L += block(f'clip {name} {fc} family staff when staff', fr)
        for fc in FACINGS:
            for v in ('0', '1'):
                src = find(death, body, 'fall', fc, variant=v, family='sword')
                fr = []
                n = len(src['frames'])
                for i, f in enumerate(src['frames']):
                    f = setk(copyf(f), 'hilt', None)
                    if i < n - 1:
                        setk(f, 'armR', staff_hold(fc)); setk(f, 'held', staff_held(fc, False)); setk(f, 'weapon', None)
                    else:
                        setk(f, 'held', 'drop')
                    fr.append(f)
                L += block(f'clip fall {fc} variant {v} family staff when staff', fr)
    return L


def main():
    ap = argparse.ArgumentParser(description='the P2c slice clip lines (written from the shipped clips)')
    ap.add_argument('--root', default=os.path.join(REPO, 'rpg', 'art', 'doll'))
    ap.add_argument('--force', action='store_true', help='overwrite files edited since the last generation (the ledger guard)')
    ap.add_argument('--families', action='store_true', help='also write the spear and bow placeholders (clips/actions/spear.clip, bow.clip)')
    a = ap.parse_args()
    clips = os.path.join(a.root, 'clips')
    loco = parse(os.path.join(clips, 'loco.clip'))
    death = parse(os.path.join(clips, 'death.clip'))
    sword = parse(os.path.join(clips, 'actions', 'sword.clip'))
    postures = parse(os.path.join(clips, 'postures.clip'))
    outs = {'shield.clip': shield_loco(loco) + shield_ride(postures), 'sword_shield.clip': sword_shield(loco, sword, death), 'spear_carry.clip': spear_carry(loco),
            'staff_carry.clip': staff_carry(loco, death)}
    if a.families:
        raise SystemExit('p2c_clips: --families is retired: clips/actions/spear.clip and bow.clip are the integrated SLICE-ARMS lines')
    # (wave 2 int4, INTEGRATOR) every output is built in memory first and written only through the ledger guard below: a
    # file someone edited after this tool last wrote it (an area's hand-tuned lines, a surgical fix) is never overwritten.
    # Root cause it closes: the integration recipe regenerates these files, and int4's regeneration silently reverted the
    # signed-off spear side carry (sp_carry, weapon=112.5) in spear_carry.clip, DOLL-C's surgical sword_shield.clip fix and
    # ACTIONS-2H's own at= steps in spear.clip (the weight pass rewrites its keys), which put the haft over the face (W7 18).
    import hashlib
    import importlib.util
    import json
    import tempfile
    spec = importlib.util.spec_from_file_location('p2c_weight', os.path.join(HERE, 'p2c_weight.py'))
    wmod = importlib.util.module_from_spec(spec); spec.loader.exec_module(wmod)

    def weighed(text, table, fam, diag=None):
        fd, tp = tempfile.mkstemp(suffix='.clip'); os.close(fd)
        try:
            open(tp, 'w', encoding='utf-8', newline='\n').write(text)
            wmod.weigh(tp, table, fam, diag)
            return open(tp, encoding='utf-8').read()
        finally:
            os.remove(tp)

    def rd(path):
        return open(path, encoding='utf-8').read() if os.path.exists(path) else None

    act = os.path.join(clips, 'actions')
    want = {}
    for name, lines in outs.items():
        want[os.path.join(act, name)] = '\n'.join(lines) + '\n'
    # (i2) the weight pass (the step into the blow) over the guard's swing. (wave-2 fix round, ACTIONS r1) no longer over
    # clips/actions/spear.clip: since wave 2 that file is authored whole (ACTIONS-2H's tools/fams.py and its hand steps per
    # frame and blow, then ACTIONS r1's blow 3 facing D), so its at= / lean= / bob= live in the file, not in a table; the
    # pass's one-step-per-clip table re-stepped every blow (int4: 140 lines, the haft over the face). The ledger forgets it.
    sp = os.path.join(act, 'sword_shield.clip')
    want[sp] = weighed(want[sp], wmod.SWORD, 'sword')
    # the spear's and the bow's falls and lying: a generated section at the end of clips/death.clip
    dp = os.path.join(clips, 'death.clip')
    text = rd(dp)
    mark = '# ---- (P2c, GENERATED by tools/doll/p2c_clips.py: the spear and the bow fall; edit the generator)'
    if mark in text:
        text = text[:text.index(mark)]
    text = text.rstrip('\n') + '\n'
    sec = [mark, "# The weapon lands beside the near hand (held=drop) on the topple and the lying body, the bow archer's quiver under the",
           "# body (sheath=lie); `when <family>`: a fighter's (a civil hunter keeps the shipped falls)."]
    for body in ('man', 'woman'):
        sec += ['', f'body {body}'] + death_family(death, body, 'spear') + death_family(death, body, 'bow')
    want[dp] = text + '\n' + '\n'.join(sec) + '\n'

    # the ledger: the sha256 of what this tool last wrote to each file (keyed by its path under the root). A file is written
    # only when it is unchanged since that write (or already equal); anything else is KEPT, the generator's text goes to
    # tools/doll/p2c_clips_out/ for a diff, and the run exits 2. --force overwrites (only on the owner's or DOLL-C's word).
    default_root = os.path.abspath(a.root) == os.path.abspath(os.path.join(REPO, 'rpg', 'art', 'doll'))
    led_path = os.path.join(HERE if default_root else a.root, 'p2c_clips.ledger.json')
    try:
        ledger = json.load(open(led_path, encoding='utf-8'))
    except (OSError, ValueError):
        ledger = {}
    sha = lambda t: hashlib.sha256(t.encode('utf-8')).hexdigest()
    kept = []
    for path, new in want.items():
        key = os.path.relpath(path, a.root).replace(os.sep, '/')
        cur = rd(path)
        if cur == new:
            ledger[key] = sha(new)
            print(f'p2c_clips: {key} unchanged')
            continue
        if cur is None or a.force or ledger.get(key) == sha(cur):
            open(path, 'w', encoding='utf-8', newline='\n').write(new)
            ledger[key] = sha(new)
            print(f'p2c_clips: {key} written')
            continue
        od = os.path.join(HERE if default_root else a.root, 'p2c_clips_out'); os.makedirs(od, exist_ok=True)
        op = os.path.join(od, key.replace('/', '__'))
        open(op, 'w', encoding='utf-8', newline='\n').write(new)
        nd = sum(1 for x, y in zip(cur.split('\n'), new.split('\n')) if x != y) + abs(cur.count('\n') - new.count('\n'))
        kept.append(key)
        print(f'p2c_clips: KEPT {key}: edited since this tool last wrote it (~{nd} lines differ); the generator text is {op}')
    ledger.pop('clips/actions/spear.clip', None)   # (fix round r1) no longer generated (above)
    json.dump(ledger, open(led_path, 'w', encoding='utf-8', newline='\n'), indent=1, sort_keys=True)
    if kept:
        print(f'p2c_clips: {len(kept)} file(s) kept as edited; move the edits into the generator (or say --force) to regenerate them')
        raise SystemExit(2)


if __name__ == '__main__':
    main()
