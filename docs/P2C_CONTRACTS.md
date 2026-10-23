# P2C_CONTRACTS: the shapes the P2c areas share (planner, 2026-10-23)

The plan is docs/PLAN.md "## P2c"; the art direction is docs/STYLE_P2.md 25-26. The doll's format stays rpg/art/doll/doll.h
(FROZEN: areas ADD names, enum values at the end before COUNT, and functions; never rename or change a meaning). Where
this file says "P2c restates", that change is made once, by the code owner (SLICE-DOLL in wave 1, DOLL-C in wave 2),
because no map depends on the old meaning. SLICE-DOLL / DOLL-C append "as built" notes at the end; nothing above is edited.

## 1. Ownership (one owner per file; disjoint)

| area | files (wave 1, the slice) |
|---|---|
| SLICE-DOLL (engineer) | rpg/art/doll/*.h, *.cpp, gen/**, clips/** (loco, death, actions/*; postures.clip and kin_*.clip untouched); tools/doll/**; rpg/art/art_rig.h/.cpp, art_fx.h/.cpp; rpg/sim/anim.h/.cpp; rpg/view/render_anim.cpp, witness_state.cpp, script_p2c.cpp (new); tools/tests/test_doll*.cpp, doll_motion.h, test_anim.cpp; tools/preview/doll_gallery.cpp; tools/scripts/p2c_slice*.txt; witness/p2c_slice.witness.json; this file's "as built" section; docs/PLAN.md P2c "Slice results" (append) |
| SLICE-ARMOUR (artist) | rpg/art/doll/maps/armour/** |
| SLICE-HELM-SHIELD (artist) | rpg/art/doll/maps/headwear/helm/**, maps/shield/** |
| SLICE-ARMS (artist) | rpg/art/doll/maps/arms/adult/action/**, legs/adult/action/**, held/weapons/**, belt/sheaths/**, back/**, fx/** |

Wave 2 ownership is in docs/PLAN.md "Production areas". Art areas never build: they compile isolated pools and use the
published exes (section 7).

## 2. Frozen in P2c (a needed change is a re-plan)

Every idle view (B1); every existing D / U walk and run view; the adult walk and run legs and arms (legs/adult/loco std,
far, tips; arms/adult/loco) and every number of loco.clip's existing walk and run lines (plants, bob, lean, hip, holds,
arm keys); maps/head, hair, beard, wear, cloak, neck, headwear/cloth, held/civil, held/props, kin, legs and arms pose
maps; postures.clip and kin clips (wave 1); the freeze manifest's frames for every look without gear
(`--freeze-check` 0 changed; frames that move from ACTION to DRAWN are read there); rig 1 (rpg/art/art_*.cpp but
art_rig and art_fx, rpg/sim but anim.*): art_hash TOTAL 317eef6efe4d8229, `city_gallery --village-art-check` 65 / 0,
`rpg_test --anim` 0 failures until the flip. The P2b mail, the nasal helm, A3's scabbard and A3's arc keep their
existing views; areas ADD views. A3's sword is the one exception: it is re-cut into the arm (arms/adult/action/onehand)
and the blade (held/weapons/sword), and every composed frame keeps its pixels (B1 16 / 16 at 0 px, the freeze check 0).

## 3. The resolver's P2c vocabulary (`role <category> <value>`, doll_resolve.cpp)

| role | values (P2c ADDs; a map may declare several) |
|---|---|
| armour | padded leather mail scale lamellar brigandine plate leaf (exists). Fix: the herald (Outfit::Guard, no helm, no bodyForm) is padded |
| overlay | tabard: emitted for every Outfit::Guard look (watch, soldiers, heralds); its colour is the accent (tabardColor). Material tiles (section 4): mail scale lamellar quilt studs rivets leaf lames stitch |
| helm | form1..form10 (cult::HelmForm + 1: nasal kettle greathelm spangen horned plumed aventail masked crested winged); band1..band7 when no form (leathercap nasal greathelm winged crested horned crown) |
| shield | `arm` (every shield bearer: the shield arm); shape0 (legacy round, `shield` set alone); shape1..4 (round heater kite tower); form1..8 (round kite heater tower crescent oval buckler leaf). **form9 = none: no need** (v1's hasShield rule) |
| pauldron | P2c restates: `<family>.<size>`, family plate (plate brigandine leaf), fur (padded leather), scale (mail scale lamellar); size 2 small, 3 medium, 4 large from the dial. **Dial 1 = none: no need** |
| held | fighters (not `civil`): `<weapon>.<form>`: sword.{straight leaf falchion scimitar khopesh wavy broad curved glaive}, greatsword.{same}, axe.bearded, mace.flanged, dagger.straight, spear.{spear glaive halberd}, staff.plain, bow.{classic self recurve composite crossbow longbow}, stick.crook. Civil tools keep P2b's bare names (`held bow` is the hunter's carried bow). A3's sword map serves `sword.straight` (and `sword` until the resolver switches) |
| sheath | sword dagger axe mace (exist); greatsword (across the back) quiver (every bow fighter) bowback (the bow out of combat) staffback shieldback (postures) |
| fx | arc (A3) heavy chop smash glint thrust swish punch twang (drawn from maps by art_fx: section 6) |

**Flags** (`when`): band1..band7 bronze alloy; sheen-{matte bright dark banded iridescent glowing pale burnished};
plumes furtrim gilding horsetail studs etching tassels scallops rivets filigree; skirt1..skirt4; crest1..crest4;
pauldron2..pauldron4; shield onehand twohand; the family name (unarmed dagger sword greatsword axe mace spear staff bow);
blade-<form> pole-<form> bow-<form>; glint-weapon glint-body glint-helm glint-shield glint-cloak; mythic. Existing flags
(armed capped walk run posture...) keep their meaning; every helm sets `capped`.

**Regions** (regionsOf; a region maps to a slot and optionally a tile): gloves band g: H leather (1) or metal (2+), X metal
(3+). Boots band b: B leather (1-2) or metal sabaton (3+), S tall leather (2) or metal greave (3+), K metal cop (3+).
Sleeves by form: mail U F + mail tile; scale and lamellar U + their tiles; plate U F metal (+ lames tile); leather F
leather (bracer) + stitch tile; padded U F cloth1 + quilt tile; brigandine U F cloth1 + rivets tile; leaf U metal + leaf
tile. The slice decides by eye whether the mail tile replaces P2b's drawn mail sleeves (decision D3).

## 4. Parts: anchors, layers, channels, keys

| part | role | anchor | layer | channel: keys |
|---|---|---|---|---|
| armour torso (per body) | armour | neck | torso z 2 | torso: rest idle run runl drawn swing lie (+ any key a clip names) |
| armour skirt / tassets (per body) | armour | hip | hangFront / hangBack | hang: rest wide flare sit floor kneel ride lie idle |
| tabard (per body) | overlay tabard | neck | torso z 3 | torso (as the armour) |
| material tile | overlay <tile> | the region's part | (recolour) | `tags tile`, NEW `on-region <U F C H X T K S B>`: repeated over that region's composed pixels on every part that marks it; a cell names a slot and / or a step delta (+1 / -1) |
| pauldron (shared, _r / _l) | pauldron | shoulderR / L | armNear / armFar by side and facing, over its arm | pauld: rest up lie |
| helm (shared, head box) | helm | head | headwear | rest (rigid); hatpose: lie; crest / plume as `when crestN` / `plumes` regions; `-` masks the hair |
| shield arm | shield arm | shoulderL | armNear / armFar by facing | sharm: the keys every shield-bearer clip names (rest walk run guard block bash hurt fling lie, plus action keys); marks `gripL`; regions U F C H X |
| shield (per shape) | shield | gripL | per view: shield (front) or back / armFar | shield: rest guard block bash lie back ride |
| weapon (library) | held <weapon>.<form> | grip (main fist) | held / heldBack by the frame | held: a000 a022 a045 ... a337 (degrees counter-clockwise from screen-right; a090 points up) + carry drop and named views (A3's: a3r0 ...); drawn once (D) for every facing unless a facing needs its own (`tags anyfacing`, NEW); origin = the grip point; `mark tip` |
| bow (library) | held bow.<form> | gripL (the bow hand) | held / heldBack | held: per facing x string state (rest nock draw full release) + carry drop; the nocked arrow is drawn with the right hand |
| sheath / back item | sheath <value> | hip (belt) / neck (back) | belt / back by view | sheath: rest kick lag drawn sit stride lie |
| effect | fx <family> | pivot (art_fx draws it) | fx / fxBack | fx: a1 a2 (strike, follow) per orientation |

**Arms carry the grips.** Every arm view that holds something has `mark grip` (the main fist's centre) or `mark gripL`.
**The hand over the grip** (NEW in the compositor): after a grip-anchored item is placed, the gripping arm's H pixels are
drawn again over it, for the main hand, for the off hand on a two-handed weapon, and for the bow hand. **Action arm keys**
are free names prefixed by family (sw sp gs ax mc dg st bw un, sh for the shield arm, cs cast, rl roll). Any key that is
new on an existing channel is agreed between the clip owner and the owner of every part on that channel; W1 / C3a
catches a missing view.

## 5. Clips (the code owner writes them; artists propose lines in their isolated roots)

- `weapon=<deg>,front|back` (in the format, now used): picks the held part's `a<deg>` view (deg rounded to 22.5) and
  draws it on held (front) or heldBack (back), placed by the frame's grip mark. `held=<key>` names a non-angle view
  (carry, drop, a3r1...) and wins.
- **Clip conditions** (NEW): `clip <name> <facing> ... when <flags>`. Among clips of the same name, facing, family and
  variant, the first whose condition holds wins over the unconditioned one. A shield bearer's sword clips name
  `armL=- sharm=<key> shield=<key>`; A3's hero keeps his own lines.
- **part drawn** (NEW, RigPart::Drawn appended): the drawn idle and walk (variant 1) and the drawn run (walk variant 2),
  moved from `part action`. ACTION keeps windup, strike, follow (3 variants), block, roll, cast, draw and release.
- The roll's tuck and over frames use `turn=east|west at=dx,dy` (as the topple does), so every garment and armour rolls
  without roll views.
- Death keeps per-family clips (`family <f>`, first, so they win): held=drop, shield=lie, sheath=lie or rest, hatpose=lie.

## 6. The pick, the atlases, the effects

AnimPick.part gains `Drawn` (the drawn idle, walk and run): the slice. Block shows while `Actor::blockT > 0`
(production, FOLK-SIM-C: set by the sword's parry and a shield bearer's parry for `kBlockShow = 0.15 s`, visual only;
in the slice Block is reached by `pose block` and the dump). The bow's Draw / Release are unchanged.
Atlas budgets per look: LOCO 128, RUN 64, DRAWN 256 (NEW), ACTION 512, DEATH 48, POSTURE 64 KB. Effects are drawn from
their maps by art_fx at the frame's pivot plus the clip's fx offset, not baked into each look's atlas. The doll composes
them too, for parity, previews and the gallery (`ComposeOpts::withFx`), and A3's arc is pixel-identical on screen.

## 7. Test hooks

- **Looks file** (`--limbs-looks`, the gates' matrices): `tag who spec [gaits]`, who man|woman, spec `k=v,...` with no
  spaces. NEW fields: helmForm bodyForm shieldForm bladeForm polearmForm bowForm pauldron skirt crest plumeColor
  armsOrnament armourTint armourTint2 sheen glow glowColor shieldField shieldDevice trimColor weaponColor (existing:
  body outfit armorStyle helmStyle helmet shield shieldStyle gloves boots backItem cape weapon civil ... palette=look).
  Today there are three field lists: test_doll.cpp's applyField (the `--limbs` dump, the narrowest: on 2026-10-23 it
  rejected armorStyle, helmet, shield, gloves, boots, head and beardStyle), test_doll_motion.cpp's, and doll_gallery's
  `--look`. SLICE-DOLL unifies them into one list, so every tool reads every line of the slice looks file.
  NEW gaits: didle hit1 hit2 hit3 block draw cast roll hurt fall lying (dumped in D U R L); walk run dwalk drun stay
  (B11 measures R L; strips show all four).
- `rpg_test --doll --limbs OUT --limbs-looks F [--pool P]`: per look and gait, the frames in four facings (finished, 1x
  and 4x strips) and each part's layer. NEW `rpg_test --doll-arms [--looks F] [--report] [--selftest]`: C3 to C6 alone.
- Gate lines: `--doll --gates` adds `C3 gear`, `C4 armour change`, `C5 families`, `C6 weapons`, and B10 DRAWN.
  `--doll-coverage --scope all --gates` is C1 (sources A to E). `--doll-variety` adds C11 (fighters' faces).
- `doll_gallery --look` takes the new fields. NEW boards: `kit` (a look in every armour form, four facings), `actions`
  (every DRAWN / ACTION clip frame by frame), `grip:<part>:<D|U|R|L>:<clip>:<i>` (12x with grip and tip marks),
  `silhouette` (strike frames filled flat, by family).
- Script (script_p2c.cpp): `slicegear <field=value ...>` (the hero's gear fields, test only), `p2cline slice|fit|clear |
  face <0-3>` (the slice rows before the hero), `expect doll gear <role>=<value>`. Existing ones serve too: pose, camhold,
  lineup, armskit, kit6, gear6, shotpair, hideplayer, ui, zoom.
- Witness state: `player.sprite.doll.gear` {armour, helm, shield, pauldron, held, sheath, back}, `entities[].doll.gear`,
  `player.anim.part` (loco run drawn action death). witness/p2c_slice.witness.json: the slice rows covered at rig 2, no
  v1 draw among them, every slice action frozen by `pose` and covered.
- **Artists' loop** (they never build): root `%TEMP%\claude\emb_P2c\slice\<area>\doll\` (copies of
  rpg/art/doll/maps and clips; edit only your files there; your clip edits are proposals).
  1. Compile: `node tools/doll/pxc.mjs --root <root>\doll --no-cpp --bin <root>\x.pool`.
  2. Dump: `%TEMP%\claude\emb_P2c\bin\rpg_test_s<N>.exe --doll --pool <root>\x.pool --limbs <out> --limbs-looks <looks>`
     (until s1 is published: build-proto/rpg_test.exe for loco gaits).
  3. Gallery: `doll_gallery_s<N>.exe <out> --pool <root>\x.pool --look ... actions`.
  4. Measure: `rpg_test_s<N>.exe --doll --motion --pool ... --limbs-looks ... --report`, then `--doll-arms`.
  5. Eye: game speed at 3x on grass, both facings; hip to boot at 8x; impact frames at 12x.
  6. Deliver: copy your files into the tree, re-copy a fresh root, compile and measure again, and write
     `<root>\report.txt` (what changed, measures, strips, proposed clip lines and region slots). Never write
     gen/doll_pool.cpp, never edit another area's file, and validate proposed clip lines with pxc first.
  `LATEST.txt` beside the exes names the newest; SLICE-DOLL publishes s1 within its first working block.

## 8. As built (SLICE-DOLL / DOLL-C append here)

### SLICE-DOLL, iteration 1 (2026-10-23)

- **Looks fields**: one parser, `art::doll::applyLookField` / `applyLookSpec` (doll.h): every HumanLook field by its
  member name (armourTint / armorTint both read). test_doll, test_doll_motion, test_doll_wear, test_doll_arms, doll_gallery
  and script_p2c all use it. The slice looks file is tools/doll/slice_looks.txt (embedded in test_doll_arms.cpp too).
- **RigPart::Drawn** = 4 in the enum; **5 in the pool** (`part drawn`; posture keeps 4): `pool::poolPart(RigPart)`.
  anim.cpp picks Drawn for the drawn idle (Idle v1), walk (Walk v1) and run (Walk v2); render_anim falls back to RUN, then
  LOCO while DRAWN is not baked; a frozen `pose idle|walk <fam> 1|2` draws DRAWN. HeroSprite::part 5 = drawn (4 stays the
  posture atlas, home_view's). The freeze tool files DRAWN's frames under the manifest's old "action" lines (ACTION no longer
  holds Idle/Walk), so the manifest needs no rewrite. B10 adds DRAWN 256 KB.
- **Pool v3**: a clip record ends with `u16 when` (string id, 0 none); v1/v2 pools still load. `pool::clipsOf(..., const
  DollRecipe*)`: per tier (the body's modifier set, then family, then generic) a conditioned clip whose condition holds wins
  its (name, facing, variant) slot; with no recipe conditioned clips are skipped. `pool::condHolds` moved there.
- **Resolver** (doll_resolve.cpp): helmBandOf (a legacy helmet's band by outfit), shieldOf (form9 none), pauldronOf
  (`<family>.<size>`, dial < 2 none), sleeveTileOf; `held` is `<weapon>.<form>` only while a pool part serves it, else
  P2b's bare name (so no look covered today loses coverage); bow fighters need sheath quiver + bowback + fx twang, spear
  fighters fx thrust, greatsword fighters sheath greatsword. Flags as contracts 3; the family flag is the CIVIL-aware one
  (a civil trade's flag is `unarmed`), which is how the spear/bow falls (`when spear|bow`) spare the civil hunter. The
  sleeve tile need and mail's U/F recolour apply only when a `role overlay <tile>` part exists (D3 by eye).
- **Region tiles**: pxc directive `on-region <U F C H X T K S B>` (stored as the tag `on-region:<letters>`) with
  `tags tile`. A tile cell's slot recolours (slot ink = keep the pixel's slot); its step: 2 keep, 1 one down, 3 one up, 0
  and 4 set (min 1 / max 4 for the moves); outline pixels never change. Grid aligned like garment tiles (Item::tx/ty).
- **Library compose**: `weapon=<deg>,front|back` picks `a%03d` of round(deg / 22.5) x 22.5 (a022 a067 a112 ...) on a
  part whose channel is `held`, laid on held / heldBack unless the view names a layer; a named `held=<key>` wins the view
  (the layer still follows `weapon`). `tags anyfacing`: a facing with no view uses D's. The hand over the grip: after a
  library weapon (role held value with a '.') is painted, the H-region pixels of the arm item that marked `grip` are
  painted again (the `gripL` arm's too with the flag twohand); a library weapon on gripL (the bow) gets the gripL arm's.
- **The bow on the back** rides its own channel (`keys bowback`): rest in every LOCO/RUN frame; the bow's DRAWN, ACTION and
  DEATH frames name `bowback=-`. The quiver stays on the sheath channel.
- **Probes**: `doll::frameInfo` (the parts, layers and keys of one frame) and `doll::probeFrame` (per pixel the item that
  wrote it and its region, the items' roles, the grips, the tip) serve the strips and the C gates.
- **Clip lines**: tools/doll/p2c_clips.py writes actions/shield.clip, actions/sword_shield.clip and the spear/bow falls at
  the end of death.clip from the shipped lines (`--families` also writes the spear.clip / bow.clip placeholders that the
  arms area's proposals replace). Not done in iteration 1: effects drawn by art_fx from maps (the arc, thrust and twang are
  still fx parts baked into ACTION; ComposeOpts::withFx not added).

### INTEGRATE, slice iteration 1 (2026-10-23)

- **Clips committed**: SLICE-ARMS's spear.clip / bow.clip (drawn idle/walk/run moved to `part drawn`, every clip `when
  spear|bow`, no `expr` facing U) and its re-cut sword swing section (armR=sw_<k> held=a3<k>; B1 16/16 0 px). p2c_clips.py
  no longer writes the placeholders (`--families` refused); it adds actions/spear_carry.clip (`family spear when spear`: the
  carrying arm still in the walk and run, so the upright spear rides the bob, not the pump: W5 / B12). `crest=up` beside
  every `tuft=up` and on every frame lifted 2+ rows (bob + head) in loco, death and the sword/spear/bow lines (C3b F-37).
  The guard's topple facing R keeps sharm=rest (the far shield stays on the hand). bow_self's `rest` views are `carry` (the
  LOCO frames leave the bow on the back; C6's carry). The quiver and the slung bow share the sheath channel (no `bowback`).
- **Resolver**: mail's U / F are metal with or without the mail tile (the shield arm and A3's re-cut sword arm showed cloth1).
  dollMissing honours `tags anyfacing` and checks the held angle view `weapon=<deg>` names.
- **Gates**: C3c lets the raised shield (guard, block) facing R sit on layer neck (behind the near arm: SLICE-HELM-SHIELD's
  drawing); C3d skips a dropped weapon and finds the arm in the bare probe by part (the bow facing L was misread); C4 measures
  the armour with nothing in hand; W1 excuses a loco arm whose key an arms/adult/action arm on the same channel draws, and
  honours anyfacing; B11 COVER takes the slice looks and ignores conditioned clips' keys.

### SLICE-DOLL, slice iteration 2 (2026-10-23)
- Turned frames: a view whose anchor resolves to `pivot` (the spear's and the bow's `L drop`) is screen-space there: composed
  alone in the left view and laid UNDER the turned body at the body's pivot after the turn (mirrored for head east), so it
  lies level beside the corpse; the pivot is found from the body alone (a spear no longer lifts the corpse 20 px). Probe
  and frameInfo still see the unturned frame. To lay a part flat on the ground (the kite's `lie`), give that view
  `anchor pivot`.
- `at=dx,dy` on an upright frame: the figure drawn dx right / dy down of its pivot (RigAnchors marks, head and hip move
  with it; probeFrame's px/py too). tools/doll/p2c_weight.py writes the steps (spear thrust, the guard's swing and block);
  p2c_clips.py runs it after writing sword_shield.clip. The hero's sword.clip is never touched (B1).
- The spear's drawn walk and run moved from actions/spear.clip into the generated actions/spear_carry.clip (`part drawn`,
  the upright carry); the levelled guard stays on the drawn idle, thrust and block.
- Spear falls keep `held` (the carry) on recoil and buckle; the bow rides the back until it drops.
- Witness: `state.player.anim.part` is the frozen pose's part (DRAWN for a posed idle / walk variant 1-2); p2c_slice
  expects it (sprite.part is only set by a drawn frame, and the harness draws only on shots).
- C3c: facing right with the far shield arm drawing nothing, the touch test is skipped (the hand is behind the kite).
- rpg_test --doll-coverage --report prints each world look drawn on v1 and its first missing value.
- (i3) A grip with no `grip` mark falls back to the hand mark AND takes the arm that marked it as the gripping arm, so the
  fist's H pixels are drawn over a library weapon carried at rest (doll_compose.cpp).
- (i3) Facing D / U the spear's LOCO idle, walk and hurt, the drawn walk and run, and the fall's buckle name `held=carry`
  (the Held layer at the fist: the whole haft, no leaf floating above the helm); side views and the RUN part keep the
  `rest` view (at the fist the haft crossed the helm and face facing R / L; the 39 px carry put knight RUN at 64.2 KB, so
  p2c_clips.py `RUN_CARRY` waits for a 38 px `carry ... when run replace` view). Spear hurt lines step 1 px back (`at=`).
- (i3) The shield bearer's hurt facing R / L holds the shield at the chest (sharm=guard shield=guard, at=-1 / +1): facing R
  the rest view sat wholly behind the body. The guard's block cocks the sword back (the windup's arm and blade, sw_<f>0 /
  a3<f>0) while the shield squares up; p2c_weight.py steps every block 2 px back and the spear block crouches (bob 2).
