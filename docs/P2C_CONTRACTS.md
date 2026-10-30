# P2C_CONTRACTS: the shapes the P2c areas share (planner, 2026-10-23)

The plan is docs/PLAN.md "## P2c"; the art direction is docs/STYLE_P2.md 25-27. The doll's format stays rpg/art/doll/doll.h
(FROZEN: areas ADD names, enum values at the end before COUNT, and functions; never rename or change a meaning). Where
this file says "P2c restates", that change is made once, by the code owner (SLICE-DOLL in wave 1, DOLL-C in wave 2),
because no map depends on the old meaning. SLICE-DOLL / DOLL-C append "as built" notes at the end; nothing above is edited.
**The ladder re-plan (2026-10-23)** adds the ladder slice's ownership (1), its freezes (2), vocabulary (3), the ladder's
data (3b), the living finish (6b) and test hooks (7); LADDER-DOLL is the code owner in wave 1b.
**The forge re-plan (2026-10-24)** adds the forge slice's ownership (1), its freezes (2), the forge styles' data and pick
rule (3c) and test hooks (7); FORGE-DOLL is the code owner in wave 1c.
**The forge round 2 (2026-10-24, alloy silhouettes)** restates 3c (the silhouette, the shape table, the details; the
texture picks retired on culture alloys) and adds round 2's ownership (1), freezes (2) and hooks (7); FORGE-DOLL stays the
code owner.
**The envelope (2026-10-27, the owner's 32 px lock)** adds wave 1d's ownership (1) and freezes (2), the tier envelope, the
pauldron sizes, the head key, the ramps' antique and glass rules and the A-defect data (3d), and its hooks (7); FORGE-DOLL
stays the code owner.
**Production (2026-10-28, the envelope signed off)** restates wave 2's ownership pointer (1), adds the production rules of
PLAN D67-D70 to 3d and production's hooks to 7; DOLL-C is the code owner in wave 2.
**The wave-2 fix round (2026-10-29)** adds the round's ownership (1), the rules of PLAN D72-D89 to 3d (the cloak ladder,
T2-T4, kEnvelope's two cells, noKitCloak, the trim band, the relics and the Mythic, the lines), the pauldron layer rule
(4) and the round's hooks (7); DOLL is the code owner.
**The flip (2026-10-30, the owner's go; PLAN D100-D106)** adds the flip's ownership (1). It restates rig 1's freeze as the
opt-in fallback's (2), and it adds the flip's hooks (7). Nothing is deleted. SWITCH is the code owner and the integrator.

## 1. Ownership (one owner per file; disjoint)

| area | files (wave 1, the slice) |
|---|---|
| SLICE-DOLL (engineer) | rpg/art/doll/*.h, *.cpp, gen/**, clips/** (loco, death, actions/*; postures.clip and kin_*.clip untouched); tools/doll/**; rpg/art/art_rig.h/.cpp, art_fx.h/.cpp; rpg/sim/anim.h/.cpp; rpg/view/render_anim.cpp, witness_state.cpp, script_p2c.cpp (new); tools/tests/test_doll*.cpp, doll_motion.h, test_anim.cpp; tools/preview/doll_gallery.cpp; tools/scripts/p2c_slice*.txt; witness/p2c_slice.witness.json; this file's "as built" section; docs/PLAN.md P2c "Slice results" (append) |
| SLICE-ARMOUR (artist) | rpg/art/doll/maps/armour/** |
| SLICE-HELM-SHIELD (artist) | rpg/art/doll/maps/headwear/helm/**, maps/shield/** |
| SLICE-ARMS (artist) | rpg/art/doll/maps/arms/adult/action/**, legs/adult/action/**, held/weapons/**, belt/sheaths/**, back/**, fx/** |

| area | files (wave 1b, the ladder slice) |
|---|---|
| LADDER-DOLL (engineer) | rpg/art/doll/*.h, *.cpp, gen/**, clips/**, maps/legend/** (ADD only); tools/doll/**; rpg/art/art_rig.*, art_fx.*; art_human.h (HumanLook ADD) and art_human.cpp's key() only; rpg/sim/anim.*, gear_look.* (ADD only), looks.cpp (rank tiers only), gear_ladder.* (new); rpg/view/render.cpp (people bake, glow pass), render_anim.cpp, view.h, loot_ui.cpp, witness_state.cpp, script_p2c.cpp; tools/tests/test_doll*.cpp, doll_motion.h, test_anim.cpp, test_folk_sim.cpp; tools/preview/doll_gallery.cpp; CMakeLists.txt (gear_ladder.cpp only); tools/scripts/p2c_slice*, p2c_ladder*; witness/p2c_slice, p2c_ladder; this file's section 8; PLAN "Ladder slice results" |
| LADDER-ARMOUR (artist) | rpg/art/doll/maps/armour/**; maps/cloak/** (ADD only) |
| LADDER-HEAD (artist) | maps/headwear/helm/**, maps/shield/**; maps/beard/** (ADD only: `when capped` R / L views) |
| LADDER-ARMS (artist) | maps/arms/adult/action/**, legs/adult/action/**, held/weapons/**, belt/sheaths/**, back/**, fx/** (but `fx glint` and `fx mote`) |
| LADDER-FINISH (artist) | maps/finish/** (new) |

| area | files (wave 1c, the forge slice) |
|---|---|
| FORGE-DOLL (engineer) | rpg/art/doll/*.h, *.cpp, gen/**, clips/** (ADD only), maps/legend/** (ADD only); tools/doll/**; art_human.h (HumanLook ADD) and art_human.cpp's key() only; rpg/art/art_fx.*; rpg/sim/gear_forge.* (new), gear_ladder.*, gear_look.* (ADD only), looks.cpp (the realm into rankTiers only); rpg/view/render.cpp (the steady T4 glow only), witness_state.cpp, script_p2c.cpp; tools/tests/test_doll*.cpp, test_doll_forge.cpp (new), doll_motion.h; tools/preview/doll_gallery.cpp; CMakeLists.txt (gear_forge.cpp, test_doll_forge.cpp only); tools/scripts/p2c_forge*; witness/p2c_forge; this file's section 8; PLAN "Forge slice results" |
| FORGE-PLATE (artist) | maps/armour/man/**, maps/armour/woman/** |
| FORGE-SHOULDERS (artist) | maps/armour/shared/** |
| FORGE-HELM (artist) | maps/headwear/helm/**, maps/headwear/collar/** (new), maps/shield/** |
| FORGE-MARKS (artist) | maps/armour/tiles/**, maps/finish/**, maps/forge/** (new) |

| area | files (wave 1c round 2, alloy silhouettes; round 1's art areas are closed) |
|---|---|
| FORGE-DOLL (engineer) | as round 1, plus rpg/sim/gear_forge_shapes.inc (generated by `rpg_test --forge-solve`), tools/doll/forge_looks.txt, tools/scripts/p2c_forge*, witness/p2c_forge |
| FORGE-HEAD (artist) | maps/headwear/helm/**, maps/headwear/collar/**, maps/shield/** |
| FORGE-TORSO (artist; the look lead) | maps/armour/man/{plate,mail,scale,lamellar,leather,padded,tabard,sash}.pxm, maps/armour/woman/{plate,mail,leather,padded,tabard,sash}.pxm (and any new woman torso), maps/armour/motif/** (new), maps/armour/tiles/**, maps/forge/**, maps/finish/** |
| FORGE-LIMBS (artist) | maps/armour/{man,woman}/*_hang.pxm, skirt_*.pxm (new), sleeve_*.pxm (new), mail_sleeve_*.pxm, mail_stride.pxm |
| FORGE-MANTLE (artist) | maps/armour/shared/**; maps/cloak/** (ADD only: new files and `when` views); maps/neck/** (ADD only) |

| area | files (wave 1d, the envelope; round 2's areas are closed and their files pass here) |
|---|---|
| FORGE-DOLL (engineer-high) | as round 2: rpg/art/doll/*.h, *.cpp (with doll_envelope.h, new, and the ramps), gen/** (pxc output at integration), clips/** (ADD only), maps/legend/** (ADD only); tools/doll/** (pxc.mjs, envelope_looks.txt, forge_looks.txt, owner_pack.py, alloys/**); art_human.h (HumanLook ADD) and art_human.cpp's key() only; rpg/art/art_fx.*; rpg/sim/gear_forge.*, gear_forge_shapes.inc (generated), gear_ladder.*, gear_look.* (ADD only), looks.cpp (rankTiers only); rpg/view/render.cpp (the glow pass and finish light, the rig-2 cache, the doorway draw order), witness_state.cpp, script_p2c.cpp; tools/tests/test_doll*.cpp, test_forge_ramps.cpp, test_doll_envelope.cpp (new), doll_motion.h; tools/preview/doll_gallery.cpp; CMakeLists.txt (new test files only); tools/scripts/p2c_forge*, p2c_envelope* (new); witness/p2c_forge, p2c_envelope (new); this file's section 8; PLAN "Envelope results" |
| FORGE-HEAD (artist-xhigh) | maps/headwear/helm/**, maps/headwear/collar/**, maps/shield/** |
| FORGE-MANTLE (artist) | maps/armour/shared/**; maps/cloak/** (ADD: new files, views, `when` and `breeze` views; P2b's existing views keep their pixels); maps/neck/** (ADD only) |
| FORGE-TORSO (artist-xhigh; the look lead) | maps/armour/man/{plate,mail,scale,lamellar,leather,padded,tabard,sash}.pxm, maps/armour/woman/{plate,mail,leather,padded,tabard,sash}.pxm (and any new woman torso), maps/armour/motif/**, maps/armour/tiles/** |
| FORGE-LIMBS (artist-xhigh) | maps/armour/{man,woman}/*_hang.pxm, skirt_*.pxm, sleeve_*.pxm, mail_sleeve_*.pxm, mail_stride.pxm |
| FORGE-MATERIAL (artist-xhigh; the A4-A8 lead) | maps/forge/** (with maps/forge/glass/**, new), maps/finish/** |
| ZOOM (engineer) | rpg/view/screen.cpp, screen.h, script_p2a.cpp (ADD only); tools/scripts/p2c_zoom*.txt (new); witness/p2c_zoom.witness.json (new) |

| area | files (wave 1d fix round, before the envelope judge's round 4; PLAN "The envelope fix round", D54-D65; the wave 1d areas above are closed and their files pass here) |
|---|---|
| DOLL (engineer-high) | FORGE-DOLL's files above, with tools/doll/envelope_looks.txt, owner_pack.py, tools/scripts/p2c_envelope* and the capture scripts |
| HEAD (artist-xhigh) | maps/headwear/helm/**, maps/headwear/collar/**, maps/shield/** |
| BODY (artist-xhigh; the round's look lead) | maps/armour/** (shared, man, woman, motif, tiles, the hangs, skirts and sleeves); maps/cloak/** (ADD: new files, views, `when` views; P2b's existing views keep their pixels); maps/neck/** (ADD only) |
| MATERIAL (artist-xhigh; the A4-A7 lead) | maps/forge/**, maps/finish/** |

Nobody else edits these in wave 1d. docs/ref/envelope (the owner-picked 32 px figures) is the planner's and read only.

Wave 2 ownership is in docs/PLAN.md "Production areas". Art areas never build: they compile isolated pools and use the
published exes (section 7). Restated 2026-10-28 at the envelope: each area's files are consolidated there from the ladder,
forge, round-2 and wave-1d restatements (nothing moved between areas); the generated files (gen/doll_pool.cpp,
rpg/sim/gear_forge_shapes.inc, clips/actions/{shield, sword_shield, spear_carry}.clip and death.clip's generated end, the
embedded looks) are regenerated only by the integrator with DOLL-C's generators; every file in no wave-2 list is frozen in
wave 2 (a needed change is a planner ruling).

| area | files (the wave-2 fix round, 2026-10-29; PLAN "The wave-2 fix round", D71-D89; the 14 wave-2 areas are closed and their files pass here; every other file is frozen this round) |
|---|---|
| DOLL (engineer-high) | DOLL-C's wave-2 files but tools/doll/p2c_clips.py, p2c_clips.ledger.json, p2c_clips_out/** and p2c_weight.py; plus rpg/sim/looks.cpp (census::rankSlots only), rpg/sim/game.cpp (the bandit chief's spawn lines only) and maps/held/props/** (ADD only: spoon_child.pxm, bread_child.pxm as WEAPONS drew them) |
| ARMOUR (artist-xhigh) | maps/armour/** (man, woman, shared, tiles, motif); maps/cloak/** (ADD; P2b's views keep their pixels; the envelope's `cloak.t5` and breeze views may be repainted in place); maps/neck/** (ADD only) |
| HELMS (artist) | maps/headwear/helm/** (with helm/crest), maps/headwear/collar/** |
| FINISH (artist) | maps/finish/**, maps/forge/** |
| ACTIONS (artist-xhigh) | rpg/art/doll/clips/** (the generated ones regenerated only by the integrator); tools/doll/p2c_clips.py, p2c_clips.ledger.json, p2c_clips_out/**, p2c_weight.py; maps/arms/adult/action/**, maps/legs/adult/action/**, maps/held/weapons/** |
| SHIELDS (artist) | maps/shield/** |

| area | files (wave 3, the flip, 2026-10-30; PLAN "The flip (wave 3)" and its checklist, D100-D106; the fix-round areas are closed; every file in no list below is frozen, every rpg/art/doll file among them) |
|---|---|
| SWITCH (engineer-high; the integrator; BDIR=build, the only area that builds build/) | rpg/art/art_rig.h; rpg/main.cpp; rpg/view/script_p2a.cpp, script_p2c.cpp, view.h, hud.cpp, witness_state.cpp; tools/run_scripts.sh; tools/tests/main.cpp, test_anim.cpp, test_rig.cpp, test_arms.cpp; tools/art_hash.cpp; tools/doll/hash_looks.txt (new); tools/preview/city_gallery.cpp; tests/fixtures/golden_village_art.txt, golden_village_art_rig1.txt (new); .github/workflows/web.yml; tools/scripts/*.txt (the restatements; p2c_flip_look*, p2c_flip_save*: new); witness/*.witness.json; witness/refs/**; tools/witness_web/firstlaunch.mjs (new); PLAN "Production results and the flip" (append) |
| DOLL (engineer-high; BDIR build-proto) | tools/tests/test_doll_ladder.cpp; this file's section 8 (append) |
| WITNESS (scaffolder; BDIR build-witness) | engine/witness.h; engine/witness_emscripten.js (checked, unchanged); witness.play.json; docs/PLAYTEST.md |

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

**The ladder (2026-10-23)** keeps all of this and adds: wearGear's, gearIcon's, census::dress's and makeLook's existing
outputs are frozen until the flip (the ladder writes only its new fields; art_hash, village-art and `--arms` prove it);
HumanLook only gains fields that v1 ignores and key() mixes at rig 2 only. ADD-only exceptions: maps/cloak (`breeze` keys,
`when` views), maps/beard (`when capped` views facing R / L). Restated: A3's arc is re-cut (the owner), and B1 compares
the figure only (the doll composes without fx; A3's Fx colours are left out of the reference).

**The forge (2026-10-24)** keeps all of this and adds: rpg/culture/** is read only (styles come from the alloys as they
are); every look whose forge fields are 0 keeps its pixels (`--freeze-check`; the ladder looks' frames change only by
their styles); today's maps keep every view, and areas ADD views, `when` overlays, marks and region cells (FORGE-HELM adds
`mark crown` to every helm view, FORGE-PLATE adds `when` overlays to the armour maps); the pauldron values plate.N, fur.N
and scale.N keep their meaning; the ladder's tiers, dials and finish table (3b) are unchanged and lay over the style;
wearGear's, census::dress's and gearIcon's existing outputs stay frozen (the forge writes only its new field).

**The forge round 2 (2026-10-24)** keeps all of this and adds: the arm, leg, head, hair and beard maps are untouched
(sleeves are their own parts; boots change only by region slots); maps/cloak and maps/neck are ADD only; the six approved
hero styles keep their pixels (HOMESPUN, TANNER'S, FOUNDRY, WROUGHT, ARMOURER'S but its flutes redrawn at 2 px, PARADE),
and only FURNACE, KNAPPED, STARFALL and the Mythic's face change; the ladder's tiers, dials and finish table are unchanged
(a styled alloy's over-layer takes the cape dial's and the T5 surcoat's place: D38); tabard.pxm's z moves from 3 to 4
(no composed pixel changes: nothing else sits at torso z 3-4).

**The envelope (2026-10-27)** keeps all of this and adds: the bodies (every adult and kid body set), the head box (16 x 14),
the anchors, B1's frames, B2's heights and every clip number are frozen for good (D42: no figure is scaled alone); no map
file, view or size is deleted or emptied and no view changes its meaning (D49): areas ADD sizes 5-6, `when <slot>.t5` /
`.t6`, `cloak.t5`, `breeze` and `variant-<n>` views (the fix round's `<mass>`-keyed helm variants are re-keyed with their
pixels unchanged); an A fix repaints its own view in place and names it in its report; P2b's cloak and neck views keep their
pixels (`--freeze-check` 0 for every untiered look); loot, rarity, pacing and lookTier's table are unchanged (the owner's
answers are pending); rig 1 is frozen; rpg/culture is read only.

**The flip (2026-10-30)** keeps all of this, and rig 1 is now frozen behind the opt-in rather than until the flip (PLAN
D101). Its record holds:
- `art_hash --rig=1` TOTAL 317eef6efe4d8229;
- `city_gallery --rig=1 --village-art-check tests/fixtures/golden_village_art_rig1.txt` 65 / 0;
- `--anim` A1 identical;
- `--arms --seeds 1..3 --rig=1` ALL OK.

Every map, clip and generated file is frozen, and the pool stays 2,570,937 B (c8df3de9a5fc8245; b9e0108's 2,570,727 B plus
210 B of empty tips views, D100). The doll's frames are frozen: the owner-signed tools/doll/freeze/p2c_freeze.txt (drawn at
rig 2, D107) stays at 0 changed at the default rig. save_v23.bin and the town, world, endless
and village goldens never change. Nothing on the retirement list (PLAN "The flip (wave 3)") is deleted, moved or emptied.

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
| fx | arc (A3; re-cut 2026-10-23) heavy chop smash glint thrust swish punch twang (drawn from maps by art_fx: section 6); ladder: `spark` (the block's impact), and `glint` (the edge spark) and `mote`, the living finish's sprites, drawn by the view and never baked (6b) |
| overlay (ladder) | the finish tiles `finish.wear finish.etch finish.filigree finish.rune` (`tags tile`, `on-region W P N`: 3b) |

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

(The forge re-plan's roles, flags, tile modes and Anchor::Crown are in 3c.)

## 3b. The gear ladder in data (re-plan 2026-10-23; STYLE_P2 27, PLAN "The gear ladder")

- **HumanLook ADD** (art_human.h; derived, never saved; v1 ignores them; key() mixes them only at rig 2 and only when
  set): `tier[8]` (0 none, 1-6; slots body helm shield weapon gloves boots cloak back: `weapon` is what is in hand,
  `back` the bow or staff on the back), `rare` (bits by slot: the Rare mark), `metalKind` (0 legacy; 1 leather 2 bronze
  3 iron 4 steel 5 gilded 6 jade 7 obsidian 8 ember 9 starmetal 10 alloy (armourTint) 11 mythic), `bladeKind` (the
  same codes for the piece in hand; 0 = metalKind), `gemColor` (0: the glow colour, else the metal's default).
- **gear_ladder.h/.cpp** (rpg/sim, new; pure): `lookTier(const Item&)` (PLAN's table), `ladderKit(people, tier, body,
  wpn, rarity, seed)` (real Items, the kit table below, for `cult::Atlas::make(people, seed)`), `applyLadderField` (the
  looks macros). **wearGear** fills tier, rare, metalKind, bladeKind and gemColor for every worn slot and changes no
  existing output. **census::dress** sets each role's tiers at the end of the role (PLAN's table; no draw).
- **The kit** (ladderKit; for Heartland the ladder slice's forms are Item::form picks):

| tier | band (ilvl), rarity | body | helm | shield | weapon | gloves, boots | cloak |
|---|---|---|---|---|---|---|---|
| 1 | 1 (3), Common | soft (the people's soft form) | none | round, wood (heartland) | band-1 bronze (Heartland: the OLD BLADE) | none, leather boots | none |
| 2 | 1 (6), Rare | leather | the soft helm (Heartland: the leather cap) | round, painted | iron, band 2 (ilvl 10) Common | leather, leather | none |
| 3 | 3 (20), Common | steel, metal form | steel, metal form | the people's shield | steel | leather (band 1 Rare), iron boots (tall: band 2) | a travel cloak if the cape dial is 2 |
| 4 | 4 (28), Common | the first alloy (Heartland: plate) | alloy (Heartland: great helm) | alloy (Heartland: heater) | alloy | metal (gauntlets, greaves) | a travel cloak |
| 5 | 6 (47), Rare | the masterwork alloy | masterwork | masterwork | masterwork | metal (flared, sabatons) | the noble cloak (band 6) |
| 6 | 6 (47), Legendary | as T5 | as T5 | as T5 | as T5 | as T5 | as T5, Legendary |

- **Flags** (flagsOf): `<slot>.t1`..`<slot>.t6` cumulative (`armour.t3` holds at T3-T6; exactly T1 is
  `armour.t1,!armour.t2`) for slots armour helm shield held gloves boots cloak back (tier[] in that order: armour is the
  body, held the weapon in hand); `<slot>.r` (the Rare mark);
  `tier1`..`tier6` (the figure's highest piece, cumulative); `breeze` (a T5+ cloak, or a T5+ helm with a plume or crest
  region); `metal-<kind>`; `mythic` (exists).
- **Regions** (ADD to Region before COUNT; pxc mirrors): **R** trim, **P** panel, **G** gem, **N** seam, **W** wear. A
  part's regions take its own piece's tier (the recipe gains a per-role region table: armour, overlay tabard and
  pauldrons follow the body; helm; shield; held and sheath follow the weapon; cloak):

| region | T1 | T2 | T3 | T4 | T5 | T6 |
|---|---|---|---|---|---|---|
| R | as drawn | as drawn (+ a `when <slot>.t2` fitting) | metal 2 (brass) | metal 2 | metal 2, gold | gold |
| P | as drawn | as drawn | as drawn | finish.etch | finish.filigree | finish.filigree + finish.rune |
| G | as drawn | as drawn | metal 2 | metal 2 | Glow, 2 steps | Glow |
| N | as drawn | as drawn | as drawn | as drawn | as drawn | Glow + finish.rune |
| W | finish.wear | as drawn | as drawn | as drawn | as drawn | as drawn |

  The Rare mark gives R the next tier's column. A finish tile applies only to parts whose piece's tier the table gives
  it. Maps draw R P G N W pixels in the piece's own material, so "as drawn" is the plain piece.
- **Dials** (the resolver's effective dials; the look's fields stay as they are): T1 no pauldron, skirt short, no crest,
  plume or cape; T2 pauldron small, skirt mid, crest small; T3-T4 the look's own; T5-T6 the look's plus `plumes` on a
  helm with a plume or crest region. A look with a tier and no culture dials (0) takes the heartland line: pauldron none
  small medium medium large large, skirt short short mid mid knee knee, and (no bodyForm) its armour form from metalKind:
  leather leather, bronze and iron mail, steel and up plate. Tiers never add a lift the sim gave (the alloy's +1, the
  census's +1 / +2).
- **Ramps** (rampsOf): Metal from armourTint / armourTint2 / sheen (art::metalRampKeys), else from metalKind (iron,
  steel = A3's, gilded, jade, obsidian, ember, starmetal, bronze), else A3's steel (a look with no tier keeps today's
  pixels). T1 compresses metal (step 4 = step 3, but a blade's tip pixel: C6) and leather (one shine); Mythic tempers
  (gear_look's ember). Metal 2
  turns gold at T5-T6. **Slot::Blade** (ADD after Fx): the held part's Metal pixels draw with it, from bladeKind or
  weaponColor (equal to Metal when the metals match). Glow from glowColor, else gemColor, else the metal's default
  (STYLE_P2 27.3). Leather parts from Cloth1 / Cloth2 by 18 luma (C14), the trouser lifts' way.
- **Colours**: B8 counts 48 + up to 3 Glow steps (T5-T6). Over the cap the resolver merges, in order: the field into the
  accent, the blade into the metal, the plume into the accent, the device to 2 steps.
- **Looks macros** (`gear::applyLadderField`, called by every looks reader before doll::applyLookField): `hero=1` /
  `heroine=1` (A3's face, hair and colours; the scarf through T3), `kit=<people>:<tier>[:<seed>]`, `wpn=<family>`,
  `rarity=<common..mythic>`, `npc=<people>:<role>[:<rank>][:<seed>]` (census::dress), `tier=<n>` and `tierBody=` ..
  `tierBack=`. People: fjordfolk highland heartland imperial dune steppe marsh jade river suntemple sylvan starspire.

## 3c. Forge styles in data (re-plan 2026-10-24; restated the same day for round 2, alloy silhouettes; STYLE_P2 28, PLAN "Forge styles")

Round 1 (below, "kept") gave every piece seven picks; round 2 adds the **silhouette** (six axes read at 1x) and a solved
**shape table** for culture alloys, and retires the texture picks on them. Nothing here is saved: every field is derived
at rig 2 from what the item already carries, enters key() only when set, and rig 1 never sees it.

### 3c.1 The silhouette (round 2)

- **HumanLook ADD**: `uint32_t forgeShape` (0 = none: round 1's pixels). Packed: helm bits 0-4, skirt 5-7, mass 8-10,
  layer 11-13, arm 14-15, motif 16-19, bit 31 set. `forgeGuise` (round 1, i3) is retired: left 0 and no longer resolved
  (the selftest's round-1 look sets it to prove C16o and C18 catch the old bodies). Compare packed values, never memcmp a
  struct (WSL clang).
- **The value lists** (doll_forge.h; 1-based; APPEND ONLY: flags and maps key on the words):
  - helm: cone kettle warhat great mask crested onion veil horned winged sallet snout kabuto morion plumed crown | hood cap
    furnace crystal (hero-only) | ladder (the ladder's own T5-T6 helm)
  - skirt: none tassets strips hauberk split bell
  - mass: none round broad tall drop mantle
  - layer: none tabard sash halfcape cape scarf
  - arm: close bare wide heavy
  - motif: plain disc straps bands chevron keel bib rays | beast crescent (depth)
- **gear_forge.h ADD** (pure, integer, no RNG stream):

```
struct Shape { uint8_t helm = 0, skirt = 0, mass = 0, layer = 0, arm = 0, motif = 0; };
uint32_t packShape(const Shape&);   Shape unpackShape(uint32_t);
uint32_t shapeKey(const Shape&);    // helm | skirt << 8 | mass << 16 | layer << 24: the uniqueness key (arm, motif aside)
int keyAxesApart(const Shape& a, const Shape& b);                 // of helm, skirt, mass, layer (0..4)
int forgeSchool6(const cult::Culture& c);   // posmod(ci, 6) + 6 posmod(cj, 6); kit cultures (Atlas::make):
                                            // ((seed - 1) * 12 + archetype) mod 36
Shape heroShape(int base, const cult::Culture* people);  // STYLE_P2 28.4's table; outside the Heartland kit the people's
                                                          // own helm (and form's hang) for TANNER'S..ARMOURER'S
Shape alloyShape(const cult::Culture& c, int alloyIndex); // the table's key + the details' arm and motif
Shape shapeOf(const Item& it, const cult::Culture* maker); // Mythic -> MYTHIC's; Mat::Alloy -> alloyShape; else heroShape
std::string describeShape(const Shape&);   // "kabuto split drop sash | wide bib"
```

- **Which slot sets what**: the helm piece sets `helm`; the body piece sets skirt, mass, layer, arm and motif (D31: each
  piece its own style). fillLadder fills forgeShape from the worn items; rankTiers (D29) from the realm's alloy or the
  rank's metal.
- **The shape table** (`rpg/sim/gear_forge_shapes.inc`: `kShapeTable[12 peoples][36 schools][3 ranks]` of packed keys,
  `kShapePoolHash`; GENERATED, never hand-edited) is written by `rpg_test --forge-solve [--pool P] --out
  rpg/sim/gear_forge_shapes.inc`, deterministic. Inputs: `kShapeVocab` in doll_forge.h (per people and axis, a weight per
  value, typical first; per rank a flavour weight; the ★ signatures: STYLE_P2 28.5 exactly), each people's material keys
  (heroShape for every base on its kit), and the **measured 1x outlines**: every helm family composed on the man's head
  and every skirt x mass x layer composed on each people's T4 man kit, idle D, idle R and the walk's passing frame R,
  ComposeOpts::grey + noHeld (below), split at the neck anchor into head and body masks. Rules:
  1. **Menu**: every key inside its people's menus and its rank's flavour (the masterwork's CAPE only: D38).
  2. **Culture**: a (people, school)'s three keys have three different helm families, two or more of skirt, mass and layer
     different, and a measured body OD of 0.08 or more pairwise.
  3. **Unique**: keys of different schools are never equal (any people). Every 6 x 6 window of culture cells holds 36
     different schools, so no two alloys in it share a key; kit cultures at seeds 1..3 take 36 different schools, so the
     board set never does either. Same-school keys may repeat across peoples (they never meet in a window).
  4. **Materials**: every key 2 or more axes and a body OD of 0.06 or more off each material key of its people.
  5. **No drawn twins** (soft, in order): no two keys of different schools with body OD under 0.05 AND head OD under
     0.10, first among the board set's 36 kit cultures (seeds 1..3: C18a holds it hard) and between schools adjacent on
     the 6 x 6 torus, then for all; then the spread (every menu value used; no
     helm family on more than 25 % of a people's 108 keys, no body outline on more than 4 %); then the vocabulary weights
     and the rank flavour (the ★ on most masterworks).
  Rules 1-4 are hard: a solve that breaks one fails (and prints the people and school). `rpg_test --forge-shapes
  [--people p] [--seeds a..b] [--check]` prints the table or re-checks rules 1-4 at run time (C16d, C16u). g1 ships a first
  solve from design estimates (an OD matrix in doll_forge.h); each integration re-solves on the delivered pool.
- **Isolated families** take their first archetype's row; **hidden peoples** their archetype's row, the hidden word in the
  details; **dialects** share their family's cell and so its shapes; a culture with no cell (a test culture) takes the kit
  rule.
- **The details** (alloyDetails, gear_forge.cpp; round 1's scoring machinery re-pointed): arm, motif, the shoulder shape
  inside the mass (kMassShapes: round cop lobed roll; broad slab vented plate; tall winged flared crystal shard flame; drop
  layered scale flap leaf; mantle fur), the crest or plume on the helm family (only where the family's map has a socket),
  the collar and the over-layer's pattern. Order: the name's root word (`kRootDetails`: STYLE_P2 28.5's table; the root is
  the name's prefix before the metal word when it is one of the 48 English roots), then the hidden word (masterwork),
  then the scores of round 1 (metal word, sheen, rank, reagents, ornament) seeded by the name; inside a culture the walk
  prefers a different motif and a different arm for each alloy (soft). The sheen filters the motif (banded BANDS;
  iridescent BIB, BANDS or CHEVRON; glowing a seam-rich motif). ForgeStyle's shoulder, crest and collar carry the details;
  **cut, edge, limb and fit are 0 on every culture alloy** (retired).
- **Flags** (flagsOf): `helmfam-<v>`, `skirt-<v>`, `mass-<v>`, `layer-<v>`, `arm-<v>`, `motif-<v>`; `alloyface` (the shield
  tinted, D39); `glowseams` (FURNACE at any tier; a glowing sheen from T4: D28, D40).
- **Parts** (ADD; pxc mirrors every word; art areas ADD files and role lines, never change a base view a 0-forge look uses):
  - **helm families**: `role helm fam.<v>`. New maps maps/headwear/helm/{warhat onion veil horned winged sallet snout
    kabuto morion plumed crown furnace crystal}.pxm; role lines ADDed on nasal.pxm (`fam.cone`), kettle.pxm (`fam.kettle`),
    greathelm.pxm (`fam.great`), masked.pxm (`fam.mask`), crested.pxm (`fam.crested`); horned, plumed, veil and winged also
    serve the people's undrawn forms (`form5`, `form6`, `form7`, `form10`), so the ladder's kits stop falling back to the
    spangenhelm and C1 closes those values. Layer headwear, anchor head, keys hatpose (rest, lie); `mark crown` on every
    view, `mark socket` where a plume or crest sits; closed families draw the visor band. Need: forgeShape.helm set ->
    `helm fam.<v>` (no fallback: `--doll-coverage --scope forge` names a missing family; never a hole); else today's
    form / band need. The guise's headwear heads (coif, turban, jingasa, veil in Metal) are retired.
  - **skirts**: `role overlay skirt.<form>`: new maps/armour/{man,woman}/skirt_{strips,split,bell}.pxm and the woman's
    skirt_hauberk.pxm; plate_hang.pxm ADDs `skirt.tassets` (both bodies) and the man's mail_hang.pxm `skirt.hauberk`.
    Anchor hip, layer hangFront (hangBack views where a panel passes behind the legs), z 10, keys hang (rest wide flare
    idle lie drawn; sit floor kneel ride by the key fallback). When forgeShape.skirt is set the resolver drops the body
    armour's own hang parts (its hangFront and hangBack layers) and needs the skirt; `none` drops the hang and needs
    nothing. The hangs' T5 surcoat views do not apply under a styled skirt (D38).
  - **sleeves**: `role overlay sleeve.<bulk>` (wide, heavy): maps/armour/{man,woman}/sleeve_{wide,heavy}_{r,l}.pxm, `side
    R|L`, anchor shoulderR / L, layer armNear (z 2, over the arm and a mail sleeve), keys armR / armL: every key of the
    loco arms, the drawn keys and the sword's action keys (the spear's and bow's as depth). The key fallback is NOT used
    (a clipped sleeve loses its bulk): a key with no view draws the close arm and `--scope forge` lists it. `bare` is
    regions F and C to Skin (C to Linen when the body is cloth), no part.
  - **motif**: `role overlay motif.<v>`: maps/armour/motif/{man,woman}/<v>.pxm, anchor neck, layer torso z 3 (over the
    armour torso at z 2; tabard.pxm and the sash move to z 4: nothing else sits at torso z 3-4), keys torso: the rest view
    per facing serves every torso key (the key fallback, clipped to the torso's opaque pixels), with own views where that
    misreads (run, runl, lie). Absolute cells (slot and step) and R P G N regions.
  - **over-layers**: `tabard` -> today's `overlay tabard` (+ the woman's new armour/woman/tabard.pxm); `sash` -> `role overlay
    sash` (maps/armour/{man,woman}/sash.pxm, torso z 4, keys torso); `halfcape` -> `role cloak halfcape`
    (maps/cloak/halfcape.pxm, halfcape_front.pxm; keys hang and cloakfront); `cape` -> `cloak noble` (masterworks); `scarf`
    -> `neck scarf` (its views over armour ADDed as `when` views where a gorget hides it). Colours: tabard, sash and scarf in
    Accent (the people's colour; a soldier's kingdom colour), capes in Cloak; one over-layer a figure (B8 holds at 48).
    On a styled alloy body the layer replaces the ladder's cape dial and T5 surcoat (D38).
- **Compose** (doll_compose):
  - **Pauldrons over capes** (D41): on a look with a cloak or the T5 surcoat a pauldron part draws on layer head z -4 (the
    cape's front is head z -5); the side views keep the near pauldron 2 px or more past the arm's line (art).
  - **Iridescent bands**: on an iridescent alloy the skirt's and motif's metal pixels alternate Metal and Metal 2 per band
    between N seams (per 2-3 drawn rows where none is marked); Metal 2 the hue-turned ramp (60 degrees or more).
  - **Glow seams**: with `glowseams`, the N pixels of motif, skirt, vented shoulders and the furnace grille draw in Glow:
    steady 0.35 at T4, the breath from T5 (6b).
  - **Shield and boots** (D39): `alloyface` mixes the shield's Field ramp 30 % toward the body metal's step 2 (its rim and
    boss are already Metal); the boots' K S B regions draw in Metal from T4 when the boots item is of the alloy.
  - **ComposeOpts::noHeld** (NEW): no held weapon, shield, sheath or back item (the outline masks of C16o and C18).
    ComposeOpts::grey as round 1.
- **Macros** (gear::applyLadderSpec): `shape=helm:<v>+skirt:<v>+mass:<v>+layer:<v>+arm:<v>+motif:<v>` (forces values; `none`
  clears), `alloyShape=<people>:<seed>:<n>` (that culture's alloy n: key and details on the body and helm), `forgeShape=0x..`
  (describeLook's output).

### 3c.2 Kept from round 1 (the hero styles and the details' storage)

- **HumanLook::forge** `uint64_t forge[TIER_SLOTS]`, a byte per axis: 0 base, 1 shoulder, 2 crest, 3 cut, 4 edge, 5 collar,
  6 limb, 7 fittings; 0 = no style. Base: 0 generated (an alloy), 1 homespun 2 tanner 3 foundry 4 wrought 5 armourer
  6 parade 7 knapped 8 furnace 9 starfall 10 mythic. Value lists (append only): shoulder plate fur scale roll flap cop slab
  flared lobed shard vented crystal flame layered winged leaf; crest ridge comb knife brow crown shards chimney spire fin
  finial spike horns wings flame; cut smooth fluted keeled banded faceted muscled ribbed vented embossed splinted crackled
  plain plackart round pointed square laced staggered channel diamond brick lacedfront stitched studded tooled; edge plain
  rolled scalloped dagged strips serrated spiked cusped pointed stepped fringed flame; collar rolled stand torc riveted
  gorget flared angular furnace high ember ruff coif; limb plain banded splinted scaled laced studded ribbed fluted faceted
  crackled; fittings rivets studs bosses stars scored knot inlay lacing.
- `ForgeStyle`, `packForge` / `unpackForge`, `baseStyle(metalKind, form)`, `forgeOf(item, maker)`, `styleDistance`,
  `forgeSchool` (3 x 3, the details' school), `forgeParity`, `forgeColumn`, `forgeValueName`, `alloyFamilyOf`,
  `describeForge`, `wordHeroBase`: as built (section 8). `alloyStyle` now returns the details (cut, edge, limb, fit 0).
- **Hero styles**: STYLE_P2 28.4's picks; `styleKitOf` gives each its helm and shield (round 2: FURNACE the furnace helm
  and the round shield, STARFALL the crystal helm; heroShape gives their silhouettes, STARFALL's skirt SPLIT).
- **Flags** `forge-<base>`, `shoulder-`, `cut-`, `edge-`, `collar-`, `limb-`, `fit-` (body), `crest-` (helm), `boss-` `rim-`
  (shield), `glimb-` `blimb-`, `form-<armour>`, `blademetal-<kind>`; on culture alloys only `forge-alloy`, `shoulder-`,
  `crest-` and `collar-` are set.
- **Parts and modes**: `pauldron <shape>.<size>` (nearest size, then the family's, then none); `overlay collar.<v>` (layer
  neck, rigid, a lie view); `overlay crest.<v>` on **Anchor::Crown** (the helm view's `mark crown`, else the head box's top
  centre); the cut tiles (`tags tile` + `on-slot`), edge stamps (`tags tile edge` + `on-edge`), limb tiles and fittings
  (hero styles only from round 2); the `when` key fallback (a forge-flag view drawn for key rest serves every key of its
  facing with none, clipped to that key view's opaque pixels); shield `when boss-` / `rim-` views; the Blade slot for every
  held weapon (D30); ComposeOpts::grey (kGreyRamp on every material slot; skin, hair, eyes, face, ink kept).
- **Macros** `mat=`, `forge=<base>`, `style=<axis>:<value>[+...]`, `alloyStyle=<people>:<seed>:<n>`, `alloy=N`,
  `forgeBody=0x..`..`forgeBack=`.

## 3d. The tier envelope in data (re-plan 2026-10-27; STYLE_P2 29, PLAN "The envelope")

- **doll_envelope.h** (rpg/art/doll, new; FORGE-DOLL; pure constexpr, integer, no RNG; read at rig 2 only):

```
struct TierEnvelope {
  uint8_t shoulders;             // px across facing D: 16 + the pauldrons' reach past the shoulder line, both sides (0: no cap)
  uint8_t rows, ornRows;         // head: every helm-layer pixel / a drawn crest or plume (plume slot), rows above the crown
  uint8_t side, brim, flare;     // columns past the 16 px head box: the shell / a drawn brim / horns, wings, flares, brims, tails
  uint8_t pauldron;              // the resolver's size clamp: 0 none, 2 small, 3 medium, 4 large, 5 great, 6 legend (0 row 0: the dial)
  uint8_t cloak;                 // 0 none, 1 the people's cape, 2 a cape or the travel cloak, 3 the long cloak
  uint8_t cloakSideD, cloakBack; // px past each leg facing D (cap); columns past the body's box behind facing R / L (W8)
  uint8_t skirtFlare, stir;      // extra px past the hips; the cloak's breeze swing in px
  uint8_t movedMin, movedMax;    // drawn motion px a figure-frame (C13; looks with a cloak or a plume)
  uint16_t area;                 // x100 of the line's T1 (C12f); 0: no ladder
  uint16_t loco, run, drawn, action, death, posture;   // B10 KB; 0 = measured on e6 (T5-T6 action, death, posture)
};
constexpr TierEnvelope kEnvelope[7] = {
  // shld rows orn side brim flare paul cloak sideD back sflare stir mvMin mvMax area loco run drawn action death posture
  {  0,  4,  6,  1,  1,  3,  0,  3,  0,  2,  0,  0,  0,  0,   0, 128,  64, 256, 512, 52, 64 },  // 0 untiered: today's rules
  { 16,  2,  2,  0,  1,  3,  0,  0,  0,  2,  0,  0,  0,  0, 100, 128,  64, 256, 512, 52, 64 },  // T1
  { 18,  3,  6,  0,  1,  3,  2,  0,  0,  2,  0,  0,  0,  0, 110, 128,  64, 256, 512, 52, 64 },  // T2
  { 20,  4,  6,  1,  1,  3,  3,  1,  0,  2,  0,  0,  0,  0, 120, 128,  64, 256, 512, 52, 64 },  // T3
  { 22,  6,  6,  1,  1,  3,  4,  2,  0,  2,  0,  0,  0,  0, 135, 160,  80, 320, 512, 52, 64 },  // T4
  { 26,  8,  8,  2,  2,  3,  5,  3,  2,  8,  1,  1,  8, 32, 155, 200, 100, 400,   0,  0,  0 },  // T5
  { 28, 10, 10,  2,  2,  3,  6,  3,  2,  8,  1,  2, 16, 48, 170, 200, 100, 400,   0,  0,  0 },  // T6
};
constexpr int kAreaStep = 5;                       // each tier's area >= the tier below's + 5 (x100 of T1)
constexpr uint32_t kPoolBudget = 2621440;          // 2.5 MiB (D46); pxc.mjs prints it
constexpr int kMeasuredCap[3] = {716, 72, 89};     // T5-T6 ACTION, DEATH, POSTURE KB: e6 measured x 1.1, never above these
const TierEnvelope& envelopeOf(int tier);          // 0..6
std::string describeEnvelope();                    // rpg_test --envelope: the table as PLAN "The envelope" prints it
```

  FORGE-DOLL writes e6's measured ACTION, DEATH and POSTURE (x 1.1, capped) into the T5-T6 rows at the first integration
  and records them in section 8; a 0 left there makes the gate use the cap.
- **Which tier reads which column**: shoulders, pauldron, skirtFlare and area by TIER_BODY; rows, ornRows, side, brim and flare
  by TIER_HELM; cloak, cloakSideD, cloakBack and stir by TIER_CLOAK; B10 and moved by the figure's highest tier (the
  `tier1..tier6` flags); a look with no tier reads row 0. The woman reads the same rows; her crown is F-30.
- **Measures** (C12f, C3b, W8; shared helpers in doll_outline.cpp): area = opaque pixels of the composed frame with
  ComposeOpts::noHeld on the walk's frame 0 facing D and the idle facing R; shoulders = 16 + the reach of the pauldron parts'
  pixels past the shoulder anchors' 16 px line facing D (16 with none); head = helm-layer pixels relative to the head anchor
  (rows from the crown row; columns from the head box); T1's outline = the T1 frame's mask against the same body's bare
  (shirt) mask dilated by 1 px, the hood's rows inside the hair's 2-row rise exempt; cloak = cloak-layer pixels past each leg's
  outer column facing D and past the body's box facing R / L.
- **The resolver** (doll_resolve): pauldron size = min(the dial's size, kEnvelope[tierBody].pauldron) at T1-T4; at T5-T6 a body
  with shoulder plates wears size 5 / 6 (replacing alloys i3's "always the large"), falling back 6 to 5 to 4 where a shape
  lacks the size (each fallback in `DollRecipe::forgeMissing` and `--doll-coverage --scope envelope`; never a hole); crest
  regions by the helm's tier (T1 none, T2 crest1-2, T3 and up the dial); T5-T6 cloaks need the noble cloak's `when cloak.t5`
  views (the long length) and `breeze` views; T5-T6 bodies use the hangs' and skirts' `when armour.t5` flare views where
  drawn (the base view where not, listed).
- **Pauldron sizes** (APPEND): `pauldron <shape>.5` (great) and `.6` (legend): maps/armour/shared/pauldron_<shape>5_{l,r}.pxm
  and ..6.. (FORGE-MANTLE); pxc mirrors the size words; sizes 2-4 keep their meaning.
- **The head key** (D51): `gear::Shape` ADDs `variant` (0 = the family's base drawing, 1..7); `HumanLook::forgeShape` bits 20-22
  hold it (bit 31 still marks a set value; compare packed values, never memcmp). `uint32_t headKey(const Shape&, int crest)` =
  helm | variant << 5 | crest << 8. Flag `variant-<n>` on the helm piece; a family's variant views key on
  `helmfam-<v>,variant-<n>` (`replace`), never on a body axis; a family declares its count (`role helm fam.<v> variants <n>`)
  and pxc checks each variant has its D U R L and lie views. The pick (alloyDetails): the owner's 37 from `kBoardHeads`
  (gear_forge.cpp: name, family as shipped at df2a155 (pinned in the solve), variant, crest; PAPIRON warhat kasa, JAONBRASS
  warhat gat, CRANESILVER warhat jingasa); every other alloy from `kHelmVariantPref[people][family]` (doll_forge.h), then the
  name's hash, a culture's later alloy preferring a variant its earlier ones lack. The solve keeps C16k (hard on the 37 and
  inside every culture) and passes the stability test.
- **The ramps** (doll_ramps.cpp, doll_alloy.cpp): D52's antique rule in metalCharacter for a warm metal (alloyFamilyMetal 0-1,
  or a warm body) under the dark sheen: step 2 luma 95-125 at a brass or bronze hue, step 1 at 45 or less, step 3 at 175 or
  more, the glint warm white; metalRead, metalRead2 and C15 still hold. D53's glass for KNAPPED (metalKind 7) and GLASS alloys
  (alloyFamilyMetal 5): the compose glass pass lays step 3 along the 1-2 px inside the step-0 outline at the lower right of
  each convex metal piece (a connected metal region of 12 px or more) and FORGE-MATERIAL's speculars (maps/forge/glass/*.pxm,
  `tags stamp`) at the top-left of facets of 4 px or more; never inside W7's face box + 1 px or on a helm's face plate.
  Starmetal (metalKind 9) takes the iridescent bands (Metal 2 turned 60 degrees or more, a band per 2-3 rows between N seams).
- **The Mythic's light off the face**: the view's finish light (render.cpp: the rim, the crown, the tongues, the embers;
  section 6b) and the compose speck rules skip W7's face box + 1 px; `art::finish::litFrame(look, frame, t)` (the view's own
  code, deterministic) gives C19's A5 row the frame at the breath's peak.
- **C19's terms** (`rpg_test --doll-adefects`): gear pixels = the frame's pixels outside the Skin, Hair, Eyes and Face slots and
  step 0; metal = Metal + Metal 2; lit share = steps 3-4 of the metal; the rows and selftests as PLAN's A table and C19.
  - **The torso** (D56, the round-3 scope the judge accepted): every torso term (A3's brass share, A4's glass rim, A6's
    bands, A7's glass tell) counts the Armour parts and the `armour/motif/` parts together on the frame composed from
    `noHeldRecipe` (no Held, Shield, Sheath or Fx part); helm and shoulder terms read their own parts on the full frame.
    Thresholds as PLAN's A table.
  - **Round 4 adds** (each with a selftest that a bad frame trips; idle D and idle R unless named; PLAN D58-D62):
    A3: the torso's Metal 50 % or more of its pixels and its cloth slots (Cloth1-3, Linen, Accent) 30 % or less. A4: the
    torso rim as one run of 3 px or more (a run = 4-connected step-3 rim pixels); no step 3-4 run of 2 px or more on a helm
    face plate or inside W7's face box + 1 px facing R or L. A5 (on gear pixels): lit (luma 170 or more) + Glow pixels 1.15
    or more of its T6's (e6 for e6m and fm_myth); their mean luma 0.90 or more of its T6's; warm light (Glow pixels, plus pixels
    at hue 20-60 degrees, chroma 30 or more and luma 170 or more: the gold and the fire) 15 % or more of the gear pixels;
    the helm's luma p90 - p10 100 or more. A6: on the torso facing D, 3 or more alternating runs of
    rows whose metal is mostly Metal or mostly Metal 2, each 2 rows or more and 4 px wide or more; step 1 20 % or more of
    the metal; Metal and Metal 2 steps 2 and 3 at CIE chroma 28 or more (the hueChroma helper). A7: glass (the metal of a
    GLASS alloy, step 0 excluded) 55 % or more at steps 1-2 and 25 % or less at step 3; the rim one run of 3 px or more
    on the helm, the torso and each shoulder; Metal step 3's luma 80 or more over step 2's; no 4-connected step 3-4 region
    of the glass of 12 px or more (a pale patch).
- **The T2 kit** (D55; the hero lines' TANNER'S at T2): the helm is the leather cap's T2 drawing (a `variant-<n>` of the
  family or a T2 view key, as HEAD and DOLL agree), its helm pixels (idle D + idle R) under T3's / 1.05 on the same line
  (C12f's existing head step; DOLL prints the budget in `--doll-envelope --report`); the pauldron shape `capelet` (APPEND:
  `pauldron capelet.2`, maps/armour/shared/pauldron_capelet2_{l,r}.pxm, size 2 only, counted by the shoulders measure); the
  neck value `cowl` (APPEND to the neck vocabulary: maps/neck/{man,woman}/cowl.pxm). Until drawn, each falls back to
  today's leather cap, flap2 and no neck piece, listed by `--doll-coverage --scope envelope`.
- **The T3 guard** (D57; C12f, `--doll-envelope`): eg3's area 1.20 or more of e1's and 1.10 or more of eg0's (C12f's
  measure); its shoulders 20; its cape 1 px or more past each arm's outer column on 6 rows or more above the legs' top
  facing D, and 1 column or more behind the body's box facing R. The watch's T3 kit: its kettle hat with the finial
  crest, the cop shape at size 3, the people's cape with its T3 views (`when cloak.t3`, ADD), the round shield where
  the kit has one. A selftest trips each term.
- **Kit tags** (D64): `kit=<people>:<tier>[m]:<seed>`; `m` with tier 6 = `rarity=mythic`; any other suffix or tier is an
  error naming the tag (the looks loader, `ladderkit` and `p2cline` share the parser).
- **Grey boards** (D63): doll_gallery's `:grey` and owner_pack.py's greyscale write luma (0.299 R + 0.587 G + 0.114 B) on every
  pixel, every slot included; a check (DOLL names it in section 8 and wires it into the gallery's board run) fails on any
  grey-board pixel whose R, G and B differ.
- **Captures** (D65): a script that runs `skill clearing` shoots only after two frames 0.5 s apart are identical (3 s at
  most); phone and owner-pack captures re-shot so.
- **The shape table's metal rule** (A1-A3, gear_forge's solve; hard): a bronze or brass everyday alloy (rank 0) never pairs
  mass NONE with layer TABARD, nor a fur MANTLE with layer TABARD or HALFCAPE; the solve re-runs with rules 1-4 hard,
  C16o, C16u and C18 at 0 and the 37's helm families pinned.
- **Production (wave 2; PLAN D67-D70)**:
  - **C12f at scale** (D67): the ladder lines are source L's kits (`ladderKit(people, tier, body, wpn, -1, seed)` for the 12
    peoples x man and woman x seeds 1..3: 72 lines), the hero lines and e6m; the area, head-step, head-OD and T2-head terms
    as on the hero lines, each line against its own T1; the caps on every look of C1's sources (A-E, F, L). A per-people
    ruling (PLAN) names its lines and the term it waives, with the measured numbers; the gate prints every ruled line.
  - **T6's legend view** (D68): a helm family's `when helm.t6` view (ADD; `replace` over the base view at TIER_HELM 6), inside
    T6's 10 rows and 2 columns; on a people's line it must stand 0.10 head OD or more off the same family's T5 view and its
    helm pixels 5 % or more over them (C12f's head terms, unchanged).
  - **T2 and T3 shoulders** (D69): at TIER_BODY 2 the resolver asks the people's pauldron family at size 2 when its dial is 1
    or more, else `pauldron capelet.2` (its hem by the people's ornament: `variant-<n>` views or edge cells, the base view
    the fallback, listed); at TIER_BODY 3 the family at size 3, else the people's cape at TIER_CLOAK 3 (`when cloak.t3`); a
    T2 helm may take a `when helm.t2` view (ADD) that the resolver prefers at TIER_HELM 2. Each fallback is listed by
    `--doll-coverage --scope envelope`, never a hole.
  - **The T5-T6 budgets** (D70): DOLL-C writes e6's measured ACTION, DEATH and POSTURE (x 1.1, never under the T1-T3 row,
    never over kMeasuredCap) into kEnvelope's T5-T6 rows at the first wave-2 integration that has e6's actions, and records
    them in section 8.
- **The wave-2 fix round (2026-10-29; PLAN D72-D89; DOLL is the code owner)**:
  - **kEnvelope**: row 0 cloakSideD 0 to 2 (P2b's long cloak as drawn); T4 cloakBack 2 to 4 (W8 by tier: T1-T3 2, T4 4,
    T5-T6 8). Nothing else moves; T5-T6 ACTION, DEATH and POSTURE are re-measured on e6 after its long cloak lands.
  - **The cloak ladder** (D73): a worn cloak item shows on every body; a culture alloy's over-layer replaces only the
    ladder's cape dial. CAPE = the tier cloak (noble) in the people's colour darkened; HALFCAPE = the tier cloak with its
    left-shoulder drape (cloak halfcape's T4-T6 views carry the whole cloak); TABARD, SASH and SCARF stay over the armour.
    The kits: T3 the people's cape (cape dial 0 or 1) or the travel cloak (cape dial 2), T4 the travel cloak, T5-T6 the
    noble cloak (ladderKit's rows). Views: `when cloak.t4` (ADD) on travel, cape, fur, noble and halfcape; the long cloak's
    `cloak.t5` views at the cap. One Cloak-slot piece a figure (B8 as before; the colour merges in their order).
  - **T2-T4** (D74, D75): at TIER_BODY 2 on a people's line the neck value `cowl` replaces the scarf, its edge by the people's
    ornament flag (the capelet's hem rule), the base cowl the fallback, listed; `when helm.t2` views preferred at TIER_HELM 2
    and `when armour.t2` pauldron views at size 2; at TIER_BODY 3 the people's plates (size 3 where its dial gives
    shoulders) AND its T3 cloak; at TIER_BODY 4 size 4 for every mass but NONE (3c.1's "T4 3" restated).
  - **Helm views by the resolved tier** (D80): the helm piece's tier after kitVar and the D57 watch picks the view; a view
    drawn for a higher tier is never used (D49); the missing view is listed (`--doll-coverage --scope envelope`).
  - **ComposeOpts::noKitCloak** (D77): the kit's tier cloak item left out; the over-layer drawn as the shape solve measures
    it. Used by C16o, C16k, C18 and `--forge-solve` (frozen cells and the owner's 37 still pinned).
  - **The trim tile** (D78): role `overlay finish.trim`, `tags tile band`, `on-region R` (the band mode, NEW): on every part
    whose piece's tier is T4-T6, each 4-connected run of R pixels is widened inward from the part's outline to the tier's
    width (T4 1 px, T5-T6 2), painted from the tile's cells (slot Metal 2 and its steps; the tile's view per tier `when
    <slot>.t4` / `.t5` / `.t6`); it only adds pixels (a run already at width keeps every pixel) and never covers a face
    (W7's box + 1), a G or N pixel or another part. C12b counts the tier's own trim (R recoloured or added, T3 up).
  - **Compose guards** (D82): the gripL shield's pixels survive every other part's `-` cells (as held pixels survive the
    helm erase). `form-<armour>` is set on every armoured body (D80).
  - **The relics and the Mythic** (D83-D85): styleKitOf / needsOf give PARADE `helm fam.onion` and STARFALL `helm
    fam.crystal` at every tier (their `helm.t5` / `.t6` views); a Mythic culture-alloy piece keeps metalKind 10, its
    alloyStyle and alloyShape, and the slot's `mythic` flag adds the ember (N and R in Glow as FURNACE's, `overlay
    collar.ember`, `overlay crest.flame` on Anchor::Crown); MYTHIC (style 10) only on Mythic bronze, iron, steel and relics.
    Starmetal's crystal shoulders take their `when metal-starmetal` views first. The relic invariant: at one tier, opaque px
    of the front walk's frame 0 gilded < obsidian < emberforged < starmetal, and the crystal the largest mass of its size.
  - **Lines** (D86, D89): census::rankSlots keeps a two-alloy realm's king in his warriors' alloy's over-layer with no full
    cape; the bandit chief's look calls `gear::rankTiers` after its outfit lines (T2 at home, T3 where liftRank lifts);
    lookTier: an alloy draws T4 or more, a masterwork T5 only from band 4; `wpn=` takes every family name (unarmed and
    crossbow included) and an unknown value is an error naming the tag.
- **The wave-2 fix round 4 (2026-10-29; PLAN D90-D98; DOLL is the code owner)**:
  - **Flags (ADD)**: `herokit` (HumanLook::forgeKit set: the hero's and heroine's road), so a fitted view can leave the
    signed-off lines out (D91); `king` (Role::King, set by census::rankSlots) for the king's cape views (D96).
  - **C3d's hidden-grip proof** (D90): facing U, the far arm (the right fist facing L, the left facing R) and turned frames
    only; h1 the grip (or a carry's hand mark) on an opaque non-weapon figure pixel (its outline included), h2 every
    8-connected group of weapon pixels touching a non-weapon figure pixel; no clearance round the grip is asked. Facing D
    and on the near arm a missing H pixel fails as before. Messages name the term.
  - **C12f's T1 bare body** (D91): t1Figure keeps `shield/arm` (the limb) and drops the shield; the head caps skip
    headwear/cloth/** and kin/*/headwear/** on untiered looks (rows and columns), never maps/headwear/helm/**.
  - **The relic invariant** (D92): frontArea counts the opaque pixels of the front walk's frame 0 (noHeld) whose owner is no
    head-anchored part (head, face, ears, hair, beard, helm, crest, headwear).
  - **The trim band's tuning** (D93): per slot, after the marked runs widen, the band extends along the piece's edges (hem,
    neck, arm openings, pauldron lower rim) to the slot's floor + 2 points, or narrows its own rows (then at T3 repaints
    the shortest R runs in the piece's metal) to the ceiling - 2 points; marked runs and drawn R at T4-T6 are kept. The
    level is chosen once per look and slot (the armour slots and the cloak, never the weapon in hand) on the idle D and R
    and laid on every frame of the look.
  - **The king** (D96): rankSlots gives a T5 king `cloak noble` at his tier as CAPE, his over-layer cleared; the cloak ramp
    of a king is his realm's colour darkened without D88's lift, the Heartland king's pinned to e25ea41's five steps.
  - **One kit, one look** (D97): ladderKit makes the shield item for every `wpn=` (the Rng draws stay in step) and carries
    it only for one-handed weapons; fillLadder sets trimColor from it either way. wearGear is unchanged (rig 1).

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

Round 2's parts (helm families, skirts, sleeves, chest motifs, the sash, the half-cape; the pauldron's layer over a cape)
are in 3c.1. **A near-side or facing-D pauldron view never takes `layer headwear`** (PLAN D87): it rides armNear / armFar
as this table says, so the head and face draw over a near pauldron's top rows in every side frame; D41's compose lift
(head z -4, over the cloak's front) is its only raise. Far-side views are not touched by this rule.

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
Atlas budgets per look: LOCO 128, RUN 64, DRAWN 256 (NEW), ACTION 512, DEATH 52 (polish i1: was 48; the corpse's legs now lie with it), POSTURE 64 KB. Effects are drawn from
their maps by art_fx at the frame's pivot plus the clip's fx offset, not baked into each look's atlas. The doll composes
them too, for parity, previews and the gallery (`ComposeOpts::withFx`), and A3's arc is pixel-identical on screen.
(The ladder re-plan: A3's arc is re-cut, so "pixel-identical" now means the re-cut arc on screen as in the compose.)

## 6b. The living finish (T5-T6; rig 2 only; STYLE_P2 27.7)

- **The glow plane**: compose also returns, per RigSheet, the Glow-slot pixels of each frame after the finishing pass,
  cropped to their box and packed (`RigSheet::glow`: an atlas plus a rect and offset per frame; empty when the look has
  none). The view bakes it beside the atlas (at most 12 looks cached) and draws it after the lighting, additively, with
  light a(t) = lo + (hi - lo)(0.5 - 0.5 cos 2pi(t / P + phase)): T6 P 2.2 s, 0.10-1.00; a T5 gem P 3.3 s, 0.10-0.60 (i3);
  phase = the actor's anim phase (anim.cpp's per-actor number); while idle the peak locks to the breathe frame. Depth,
  the rune crawl: three sub-planes by (x + y) / 2 mod 3, a third of a cycle apart.
- **The edge sweep** (polish i1; was the spark): a white head with a 6 px tail in the glow's light runs the frame's
  RigAnchors grip (else the weapon fist) to tip in 0.40 s every 2.75 s (each actor offset), on the drawn piece's own
  pixels only (the frame's silhouette mask), and ends in a 5 px star at the tip; the star flashes on the strike's impact
  frame; a T6 piece in hand with a tip.
- **The halo** (polish i1): each glow pixel's light spills 0.42 onto its 4 non-glow neighbours and 0.16 at 2 px: the
  runes and gems read as a band of light, not dots.
- **The aura** (polish i1): T6, a 1 px ring of the glow's light (half-way to white) outside the silhouette round the helm
  and shoulders (the upper 45 % of the figure, fading down), 0.40 x the breath; the Mythic, flame tongues 1-4.5 px off
  every upper edge (a smooth noise per column, the tip pixel lit by the height's fraction so a tongue grows and sinks,
  white-yellow, amber, red; 0.58 at the root) over a 1 px fire ring down to 62 % of the figure (0.24). Built per frame from
  the frame's silhouette (the view keeps a mask per frame of a T6 look's atlas).
  **Since polish i3 the light is ON the figure** (the panel: "a blurry pale-green selection halo"): no off-silhouette ring.
  T6: a rim one pixel inside the dark outline, lit from the upper left (right side 60 %, no underside), the crown to 62 % of
  the figure fading 60 % down it, the glow's hue a quarter toward white, kRim 0.75 x the breath. The Mythic: a fire rim
  (#FF9C30) up both flanks from 30 % to 86 % (below the face) at kRimM 0.85 on a flame noise, and its drawn flame crown
  (the mask's fire-coloured pixels, value 2, in the upper 30 %; horizontal gold bands skipped) lit at kCrown 0.42 x a
  flicker; the separate flame tongues off the crown stay.
- **Motes and embers** (polish i1): view particles in WORLD space (one set per actor; a walker leaves them behind; seeded
  per actor and birth tick, deterministic under Witness): T6 motes born every 0.45 s on a glow pixel of the current frame,
  4 alive, rising 10 px over 1.2 s with a sway and a soft 4-neighbour glow; the Mythic's embers every 0.07 s, 12 alive,
  0.8 s, licking 13 px up, white-yellow -> amber -> red with a soft glow round the hot ones; while the Mythic strikes every
  other ember is born at the blade's tip (a trail of fire along the swing).
- **The stir**: the idle's breathe frame and (polish i1) the walk's passing frames (1 and 4 of 6, where the hair swings)
  draw the `breeze` views of every rest-key part of a look flagged `breeze` (the T5-T6 cloak's hem, the crest's tail); the
  run's cloak `flare` has a `when cloak.t5` view. No new frame; the frozen loco walk and run lines unchanged.
- **Budgets** (C13, polish i1): sprite-bright animated pixels (light over 0.45, or a glow pixel: the glow plane, the
  sweep, the flame roots, the particles' cores, the rim at its peak) 128 a figure-frame at most (polish i3: was 112; the Mythic measures 119 with its fire rim and lit crown; was 32: the blind panel saw "no animation at
  game scale"); soft light (halo, aura, flame tips) 320 px at most, each 0.45 or less; a T6 figure shows 40 px of soft light
  or more at the breath's peak; still lights (glow, halo, aura) change 0.25 or less per 1/60 s (the sweep and the
  particles move by design); a glow plane 25 % or less of its atlas; 8 T6 figures' light 0.3 ms or less. Nothing draws at
  rig 1.

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
- **The ladder's hooks** (wave 1b): tools/doll/ladder_looks.txt (the macros of 3b) and `rpg_test --expand-looks F` (each
  line's resulting fields); `rpg_test --doll-ladder [--looks F] [--finish] [--report] [--selftest]` (C12, C13, C14;
  `--doll --gates` adds the three lines); `--doll-coverage --scope ladder [--seeds]` (source L); doll_gallery boards
  `ladder` (a look's kit T1-T6, four facings, 1x and 3x, grass and dark ground) and `finish:<tag>` (one living-finish cycle
  as 12 frames and a 3x GIF); script `ladderkit <people> <tier> [wpn]` (the hero's worn items, test only) and `expect doll
  tier <slot>=<n>`; Witness `player.sprite.doll.tier`, `entities[].doll.tier`, `player.finish` {glow, spark, motes}. The
  artists' roots: `%TEMP%\claude\emb_P2c\ladder\<area>\doll\`; LADDER-DOLL publishes s5 (fields, flags, regions,
  macros) within its first working block, and the artists use s4 for the open items until then.
- **The forge's hooks** (wave 1c): tools/doll/forge_looks.txt (3c's macros); `rpg_test --doll-forge [--looks F]
  [--worlds a..b] [--report] [--selftest]` (C16, C17, C15f; `--doll --gates` adds the C16 and C17 lines);
  `--doll-coverage --scope forge` (source F); doll_gallery boards `forge` (the hero styles on one figure: idle D and R and
  the walk facing R; 1x, 3x, 8x), `forgegrey` (the same in the neutral ramp), `silhouette:forge`,
  `alloystyles:<people>:<seed>` (a culture's alloys on its own kit beside their metal word's hero style, colour and grey),
  `metalread` (every family x sheen on its people's T4 kit, the tells marked) and `families` (the 96 families); script
  `forgekit <people> <tier> <mat|alloyN> [wpn]` (the hero's worn items, test only) and `expect doll forge <slot>=<style>`;
  Witness `player.sprite.doll.forge` {body, helm, shield} and `entities[].doll.forge`. The artists' roots:
  `%TEMP%\claude\emb_P2c\forge\<area>\doll\`; FORGE-DOLL publishes exe f1 (fields, flags, parts, Anchor::Crown, tile
  modes, macros, the forge board) within its first working block, and the artists preview with `style=` and `forge=` from
  f1 on.
- **The forge round 2's hooks** (alloy silhouettes): `rpg_test --forge-solve [--pool P] --out
  rpg/sim/gear_forge_shapes.inc` (3c.1) and `--forge-shapes [--people p] [--seeds a..b] [--check]`; `rpg_test --doll-forge
  [--worlds a..b] [--report] [--selftest]` adds C16d (restated) and C16u; `--doll-forge --silhouettes [--worlds a..b]
  [--report]` is C18 (FORGE_SIL_OUT=<dir> writes every mask and the nearest pairs as 1x and 4x PNGs); `--doll --gates` adds
  C16o, C16m, C16n and C17's round-2 terms. doll_gallery boards: `alloys1x[:<people>]` (the board set: idle D and R and the
  walk R at 1x, 3x and 8x; `:grey`, `:black`), `silhouette:alloys` (black silhouettes, the round-1 twin groups ringed),
  `twins` (each round-1 twin group, every member now), `world:<w>` (a world window's alloys as 1x black silhouettes, the
  nearest pairs ringed), `shapes:<axis>` (every value of an axis on one figure: the artists' sheet), `shapes:<people>` (a
  people's whole menu). Script `forgekit` takes `shape=` and `alloyShape=`; `expect doll shape <field>=<value>`; Witness
  `player.sprite.doll.shape` and `entities[].doll.shape`. Artists' roots `%TEMP%\claude\emb_P2c\forge2\<area>\doll\`;
  FORGE-DOLL publishes exe g1 (`rpg_test_g1.exe`, `doll_gallery_g1.exe` in `%TEMP%\claude\emb_P2c\bin`, LATEST.txt) with
  forgeShape, the flags, roles and compose rules of 3c.1, a first table, the macros and the boards within its first
  working block; the artists preview with `shape=` from g1 on.
- **The envelope's hooks** (wave 1d): tools/doll/envelope_looks.txt (e1-e6, e6m, e1w-e6w, eg0, eg3, ecapt, eking, a_uha,
  a_jaon, a_enkho, a_thaer, a_thorn, a_ilsil; the forge lines fm_obsid, f5_obsid, fm_myth, fm_star, f5_star); `rpg_test
  --envelope` (prints kEnvelope); `rpg_test --doll-envelope [--looks F] [--report] [--selftest]` (C12f, C3b and W8 by tier,
  B10 by tier, C13's drawn motion; the lines also in `--doll --gates`); `rpg_test --doll-adefects [--looks F] [--report]
  [--selftest]` (C19; also in `--doll --gates`); `--doll-forge` adds C16k; `--doll-coverage --scope envelope` (sizes 5-6,
  tier views, `cloak.t5` and breeze views, variants). doll_gallery boards: `envelope:<line>` (hero, heroine, guard: T1-T6 on
  one baseline at 1x, 3x and 8x; `:grey`, `:black`; the caps as faint guides at 8x and the area ratio under each),
  `heads:owner37` (the 37 heads, greyscale, 1x and 4x, with their head keys), `adefects` (A1-A8 before and after; the before
  from `--pool-before P`), `stir:<tag>` (a T5-T6 look's breeze frames and a 3x GIF). Script: `ladderkit` as before; `expect
  doll envelope <field>=<value>` (tier, shoulders, rows, area); Witness `player.sprite.doll.envelope` {tier, shoulders, rows,
  area}. Artists' roots `%TEMP%\claude\emb_P2c\env\<area>\doll\`; FORGE-DOLL publishes exe e1 (`rpg_test_e1.exe`,
  `doll_gallery_e1.exe` in `%TEMP%\claude\emb_P2c\bin`, LATEST.txt) in its first working block; until then the artists
  preview with g4 and the `tier=` and `pauldron=` macros. ZOOM: `EMB_DEVICE=phone|tablet|desktop` (read only for the
  first-launch default, when no settings file exists) and the script command `firstlaunch <device>` (re-runs the first-launch
  default as that device, ignoring the settings file), with the existing `expect zoom` and `expect screen want|world`.
- **Production's hooks** (wave 2, 2026-10-28; DOLL-C publishes them as exe w1 in its first working block, w0 being the
  e25ea41 build at hour 0): tools/doll/arms_looks.txt (the matrices M8-M12 at the envelope, PLAN's DOLL-C brief; embedded
  as the gates' matrix by embed_looks.py); `rpg_test --doll-envelope --scope ladder [--seeds a..b] [--report]` (C12f over
  source L's lines, the per-people table) and `--doll-envelope --scope all [--seeds a..b]` (the caps on every look of C1's
  sources); `rpg_test --doll-ladder --finish --scope ladder [--seeds a..b] [--report]` (C12-C13 over source L); every
  command takes `--pool P`; doll_gallery board `envelope:people:<people>` (both bodies' T1-T6 on one baseline at 1x, 3x and
  8x, `:grey`, the ratios and head steps printed). Exes: `%TEMP%\claude\emb_P2c\bin\rpg_test_w<N>.exe` and
  `doll_gallery_w<N>.exe` (DOLL-C, BDIR build-proto) and `rpg_test_int<N>.exe` and `doll_gallery_int<N>.exe` (the
  integrator, after each integration); LATEST.txt names the newest. Artists' roots `%TEMP%\claude\emb_P2c\wave2\areas\<KEY>\doll\`
  and reports `%TEMP%\claude\emb_P2c\wave2\areas\<KEY>\report.txt`. The keys board: `%TEMP%\claude\emb_P2c\wave2\keys\<KEY>.txt`,
  one file per area that names keys (channel, key, facings, family, the frames that use it), written only by its area. The
  freeze manifest: `%TEMP%\claude\emb_P2c\wave2\freeze_e25ea41.txt` (`--doll-wear --freeze-write` at hour 0 by the
  integrator; `--freeze-check` at every integration).
- **The fix round's hooks** (2026-10-29; DOLL publishes them as exe w2f1, `rpg_test_w2f1.exe` and `doll_gallery_w2f1.exe`,
  in its first block; the integrator then `_int5` on): `rpg_test --doll-envelope --scope ladder [--seeds a..b] --ledger`
  (per line and tier: the area, D and R, and each piece's add, the frame without the piece against with it, for the helm,
  pauldrons, cloak, neck piece, skirt and hangs, over-layer and torso; the T2 head budget); `--doll-envelope --scope all`
  under the restated caps; `--doll-ladder` adds the relic invariant; `--doll-wear --freeze-check F --report` attributes
  every changed frame to a cause (a map file, a resolver rule, a clip) and is reported, not gated, until the owner signs
  (PLAN D72); `--doll-coverage --scope all --gates` prints the `reserve` list (a part tagged `tags reserve` is kept and not
  dead; only the planner adds to the list: motif plain and straps, and the pauldron sizes no source asks); doll_gallery's
  `envelope:people:<people>` prints the ledger under each figure, and the bow and crossbow boards read "draw, loose".
  Roots `%TEMP%\claude\emb_P2c\wave2f\areas\<KEY>\doll\`, reports `...\wave2f\areas\<KEY>\report.txt`, the keys board
  `...\wave2f\keys\<KEY>.txt`.
- **D99's reads** (fix round 4, judge round 3; test_doll_ladder.cpp, DOLL):
  - Colours (amended after judge round c12_r1): the tier counts its whole figure's colours; the tier below counts
    coloursInSight(): each pixel's colour unless the tier's kit at that pixel is a different piece (pieceOf: the part's
    tier slot, -1 for a kit part on none, or not kit). The kit (isKit) is gearItem (armour, helm, shield, pauldron, held,
    sheath, cloak, overlay), a part on a tier slot, or a `neck` or `headwear` part; the rest is the person and the
    clothes. 80 % of the tier below, 75 % on a styled body. C12d's 48-colour check still counts the whole figure (B8).
  - The styled body: one test (`alloyshape`, or a forge style on the body, HumanLook::forge, at T4 or above) drives both of its
    forms, colours 75 % and step-4 3 or more.
  - Trim: the body slot's ceiling facing R and L is max(band, 30 %). trimBand and every rendered pixel are unchanged.
  - `--report` prints each still-failing colour view's colours by slot, person and clothes (the tier below's in sight
    against the tier's).
  - The freeze is hard against `%TEMP%\claude\emb_P2c\wave2\freeze_final.txt` (`--doll-wear --freeze-check F`, 0 changed).
  - The planner's measurement and boards are in `%TEMP%\claude\emb_P2c\wave2\fix2\d99\`.
- **The flip's hooks** (wave 3, 2026-10-30; PLAN D100-D106; SWITCH unless named):
  - The rig: `art::kDefaultRig` (2) and `art::RigScope` (save, set, restore). rpg_test, art_hash and city_gallery read
    `EMB_RIG=1|2` as the game does, and also take `--rig=1|2` (rpg_test strips it before dispatch; when both are given,
    the flag wins). The Witness reset's
    `rig` defaults to EMB_WPLAY_RIG, else 2. Script `rig 1`, Witness `rig: 1` and web `?rig=1` open the v1 fallback, and
    the game prints one line when they do.
  - art_hash: at rig 2, `doll` hashes every atlas humanRig bakes for each line of tools/doll/hash_looks.txt (LOCO, RUN,
    DRAWN, ACTION, DEATH and its POSTURE set, in file order). It sits in TOTAL in place of `humans`. `--dump DIR` writes
    `<category>/<name>.png`, and `-v` works as before.
  - city_gallery: `--village-art-dump DIR`. The default fixture is golden_village_art.txt at rig 2 and
    golden_village_art_rig1.txt at rig 1.
  - test_arms: kOldKeys (rig 1) and kOldKeys2 (rig 2). `--print` prints the current rig's keys.
  - C2's counter: View::v1DrawsRun2 and v1DrawsRun1 sum, over the run, the people frames drawn from v1 sheets, split by the
    rig they were drawn at. A `--script` run ends with `v1draws rig2 <n> rig1 <n>`. tools/run_scripts.sh prints `v1draws
    rig2: <n> scripts with v1 frames` and the rig-1 counts per script, and it fails any script with rig-1 frames and no
    `rig 1` line. Witness reports `screen.v1DrawsRun`.
  - Script `loadsave <file>` (test only): loads through Game::deserialize, as CONTINUE does. It fails the script on a
    refused file and never writes.
  - tools/witness_web/firstlaunch.mjs: Chromium, the "iPhone 15 Pro landscape" device, a fresh context, and site-witness on
    127.0.0.1:8090 without `?witness`. It starts New Game and takes a shot. `--query k=v` adds URL params.
  - The C12 cover floor (DOLL, test_doll_ladder.cpp; PLAN D106). For each kit part of the upper tier, take its pixels that
    lie over the tier below (D99's covers). The floor reads a cover of 12 px or more with 4 or more colours below it. The
    part needs 3 or more colours of its own over all its visible pixels (2 under D106's fallback). `--report` prints the
    covers and the five lowest. The synthetic selftests go in selftests(), and the real-frame one in gateC12's hero line.
  - Witness 0.7.11 (WITNESS): engine/witness.h has `witness_drawing()`. witness.play.json's `play.lazyDraw` is on only
    after doctor prints "✔ lazy drawing safe".

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

### LADDER-DOLL, ladder iteration 1 (2026-10-23)
- **HumanLook ADD** (art_human.h): tier[8] (TIER_BODY..TIER_BACK), rare, metalKind, bladeKind, gemColor as 3b, plus
  `mythic` (bits by slot: a Mythic piece) and `motif` (the filigree's motif by the body's maker people: 0 oak 1 clouds
  2 laurel 3 knot). key() mixes them at rig 2 only and only when set; dollCovers' cache key mixes them at every rig.
- **gear_ladder.h/.cpp** (rpg/sim): lookTier, metalKindOf, fillLadder (wearGear calls it last; existing outputs unchanged),
  rankTiers (census::dress calls it last: bandits/pirates T1, soldiers and the village watch T2, capital watch, heralds,
  envoys, warlords T3, captains, royal guard, jarls T4, kings T5), ladderKit (Atlas::make(people, seed) with a test id
  0x1ADD...; Heartland forms by Item::form; the woman's T4+ helm is the open crested helm; a MANTLE name gives way to the
  kit's travel / noble cloak), applyLadderSpec + describeLook.
- **The macros' hook**: the looks-field parser (applyLookField / applyLookSpec) moved from doll_resolve.cpp to doll_walk.cpp
  (rpg_doll, below rpg_sim, so save_test links); `doll::setLookSpecHook` (doll.h ADD) takes gear::applyLadderSpec, which
  gear_ladder.cpp installs at static init and every reader calls (installLadderMacros). Order: hero/heroine, kit or npc
  (with wpn, rarity), the plain fields as written, then tier / tierBody..tierBack / rare / metalKind / motif. `npc=` sets
  Game::makeLook's fixed role base (outfit, helmet, shield, weapon, royal plate) before census::dress.
- **Regions** R P G N W = Region 12..16 (pxc letters), **Slot::Blade** after Fx (pxc `blade`; maps never name it: a library
  held part's Metal pixels draw in it when DollRecipe::blade, i.e. its ramp differs). DollRecipe ADD: fin[8][5], finTiles[8],
  pieceTier[8], blade, motif; `doll::partTierSlot(part)`; FIN_TILE_* incl. GEM (T5+) and GLYPH (T6).
- **The compositor**: finish regions by each part's tier slot (the outline ring keeps its slot); region tiles: masks 32-bit,
  stacked last-listed first, LADDER-FINISH's `tags fit` and `tags stamp` placements, ink cells move the step by k - 2,
  stamps and Glow cells absolute (see LATEST s6). With a finish.rune part N stays as drawn (the tile writes the dashes).
- **The glow plane**: RigSheet::glow (art_rig.h ADD: atlas, rects parallel to frames with the crop's offset, counts), built
  in compose from each cut's Glow-slot pixels after the finishing pass. **art::finish** (art_fx.h): pulseOf, light,
  actorPhase (stride 0.2754 of a cycle: ids up to 3 apart are 0.17+ apart), sparkAt / sparkFrame, motes, enabled().
- **Gates**: the gates' matrix (test_doll_arms.cpp kSliceLooks) is tools/doll/ladder_looks.txt, written by
  tools/doll/embed_looks.py (`--check`). B8 adds the matrix with 3 Glow colours allowed on T5-T6; B10 prints D19 beside
  the base for T5-T6; the pool gate is 1.75 MB. test_doll_ladder.cpp: --expand-looks, --doll-ladder (C12 C13 C14 and
  selftests; also in --doll --gates), --doll-coverage --scope ladder [--peoples all] (source L). C14's lit edge is the upper
  quartile of the first lit pixel per row / column from the top-left, against #242A1E (luma 40), wanting 40 over it or A3's
  own hero's edge in that facing when lower (A3's hero facing R is 67: the plan's number fails A3 itself; planner to rule).
  C13's phase line reads neighbouring ids (id, id+1..3): uniform phases cannot keep 95 % of all pairs 0.15 apart.
- **Not built in iteration 1**: the view's glow pass (render.cpp / render_anim.cpp), the spark and motes on screen, Witness
  `player.finish`, script `ladderkit` / `expect doll tier`, p2c_ladder scripts, witness/p2c_ladder, owner_pack --ladder,
  the clip open items (spear hurt hold, 3-frame blocks and thrust, RUN_CARRY, lying placement), the Mythic aura on doll
  frames, the rune crawl, the regional tier lift, F9.

### LADDER-DOLL, ladder iteration 2 (2026-10-23)
- **The top of the ladder** (the blind panel: T4 "pink clay", T5 = T6, the Mythic "a carrot", the great helm "a bucket",
  the glow invisible in play):
  - rampsOf: an alloy's Metal ramp is **metalized** (value kept, its hue a tint over A3's steel: chroma caps 12-16, the
    highlight L 92+), so a people's alloy reads as a cast of steel, never paint; a heartland high metal (metalKind 5-9:
    gilded, jade, obsidian, ember, starmetal) draws as itself, not gear_look's band tint (D20; iron and steel keep the tint);
    the Mythic row (11) is ember-TEMPERED steel (#14080E #6A2A36 #8E3632 #CC644A #FFC890), the plume pale gold, the cloak
    smoke-black; the Glow ramp is **light** (glowLight: chroma 48+, L 54-62 at rest so the breath has room, and a warm glow
    beside gold trim turns to the steel's cold blue-white, the Mythic excepted); a T6 shield's Device takes the Glow
    ramp (drawn, no pulse); Leather step 1 lifted to 82 luma (C14's lit edge on soft kits).
  - ladderKit: the Heartland's T6 is **STARMETAL** (band-7 steel, ilvl 55: its own pale blue-white metal is the step up
    from T5's masterwork alloy); the Heartland helm from T4 is the **open crested helm on both bodies** (the great helm
    stays in the library and is one word to bring back); the hero's crest and plume in his tabard colour.
- **The glow plane in tiles** (art_rig.h GlowPlane ADD tiles / first / tileCount / each()): each frame's crop is cut into
  8x8 cells trimmed to their pixels, equal tiles packed once (lt6w's plane 51.2 KB -> inside 25 %). rects[i] stays the
  frame's box (x, y unused); readers walk pixels with GlowPlane::each.
- **The view's living finish** (render.cpp / render_anim.cpp / view.h): RigEntry/RigTex carry the plane (glowTex baked as
  its light key); View::queueLivingFinish queues, per rig-2 T5-T6 body frame drawn (not the dead): every glow pixel as a
  1x1 additive light in the glow colour half-way to white, breathing on art::finish::light with the actor's phase **and
  the rune crawl** (band (x + y) / 2 mod 3, a third of a cycle apart); T6: the edge spark (a 1 px core + 1-2 px arms, 3
  frames) grip (else the weapon fist) to tip every 3.3 s, and up to 3 motes; drawn in drawWeather's tail after the
  lighting (a shimmer by day 0.6, a glow at night 1.0). compose: a T6 piece in hand whose held view marks no tip gets its
  farthest cell from the grip origin as the tip (and the grip anchor as the grip), T6 looks only.
- **Scripts**: `ladderkit <tag|spec>` (the ladder looks lt1..lt6 lt6m lt6w lt1w, the flavour row, the census ranks; or a
  spec through the macros), `p2cline ladder|flavour|ranks`, `expect doll tier <n>`; tools/scripts/p2c_ladder.txt (each
  tier on the doll, the T6 breath by day and night every 0.1 s, the edge spark every 0.06 s, the three rows by day and
  night): 0 failures.
- **Gates made to agree with the eye** (each looked at 8x first): C12 b's trim ceilings carry the maps' hems (T3 3-14 %,
  T4 6-20 %, T5-T6 10-24 %), helms and shields take a rim's ceiling (36 %), a weapon or cloak with no R drawn passes,
  colours may fall to 80 % of the tier below (leather to mail), step-4 by 1; C12 c: a figure may shrink 5 % a step and lt5
  over lt1 is 115 % facing D, 105 % facing R; W8: a cloak's back may trail 2 columns (selftest moved to 3); C4 measures
  armour with no cloak on either side. The looks gain **lt1w** (the heroine at T1: the woman's padded coverage); fjord3's
  hair #B87A3E (C14 against mail).
- **C14, the archer** (the panel: "over-corrected: the outfit peach, reads as bare skin"): rampsOf parts the Leather from
  the Skin too (18 luma at step 2); the ladder archer's skin #8A583C (was #B98060: a tan jerkin on tan skin), so his
  jerkin keeps its value against the dark ground and his hair.

### LADDER-DOLL, ladder iteration 3 (2026-10-23; the i2 blind panel: the top of the ladder)
- **The living finish reads at 3x** (all five panelists: "no animation I could see", "about 11 sprite px change").
  `art::finish::frameLights` is the one light model (render.cpp and doll_gallery's boards both call it): each glow pixel's
  light is the figure's breath times (0.7 + 0.3 x the rune crawl), so the whole piece brightens and dims while the light
  runs along the seams (i2's crawl alone averaged the figure to a constant); the **halo** (kHalo 0.32 of that light, the
  glow's colour) spills onto the 4 non-glow neighbours (light, not sprite pixels: C13's pixel budget is unchanged and
  still measured on the plane); the **gleam** (T6): a slanted 3 px band of the figure's own colours added back at 0.8
  sweeps the whole figure in 0.5 s every 3.3 s, half a cycle after the spark (the Mythic's in its fire); by day the
  finish draws at 0.85-1.0 of its light (was 0.6-1.0). **Sign-off wanted (owner or planner):** C13's pulse numbers move
  to T6 0.10-1.00 (was 0.30-0.85) and T5 0.10-0.60 (was 0.15-0.45); the gate checks the new numbers, the pixel light's
  step (<= 0.25 per 1/60 s) and the gleam's 3.3 s / 0.5 s.
- **The Mythic's flame**: `art::finish::embers` (6 alive at most, born every 0.12 s on the upper glow pixels, licking
  10 px up over 0.7 s with a sway, white-yellow -> amber -> red, 2 px tall while hot) replaces the motes on a Mythic look;
  C13 counts them in the frame budget (glow + 6 + the spark <= 32) and checks count, life and rise.
- **Mythic metal** (kMetalKind[11]) is BLACKENED steel (#3C3846 / #625E70 / #9E98AA, a white-hot highlight); its glow
  rests hot amber (L 68); the hero's Mythic crest a flame (#F06A1E), his cloak a black crimson (#480A14). Gold trim as T5.
- **The dye climbs with the cloak's tier** (looks macros, the hero / heroine): T4 the arms' colour at 55 % (travel), T5 a
  crimson from the arms (0.74 / 0.5 / 0.62 of R G B), T6 royal blue #23387A, the Mythic black crimson: the back views of
  T4, T5, T6 and the Mythic now differ (the panel: "the same brown cloak three times").
- **A T6 piece wears T5's dials at least** (ladderLook): pauldron 4 where one is worn, crest 2 (so the plume) where a
  crest is worn. The heartland's band-7 STARMETAL is steel, so gear_look gave it the people's plain dials, under the
  masterwork alloy's: T6 had lost T5's plume and large pauldrons. A lift at T6 only, rig 2 only.
- **metalize** keeps less of an alloy's colour (chroma caps 7 8 9 7 3, shares 0.25-0.34): the heartland T4 alloy no
  longer "mauve putty", the T5 masterwork no longer "beige cloth"; both read as steel with a cast.
- **Drawn forms** (`drawnForm`, needsOf): a culture's helm or shield form the library has not drawn yet wears the nearest
  drawn one (helms: kettle -> nasal, great helm / plumed -> crested, horned / conical / winged / masked -> spangen ->
  nasal; shields: crescent -> round, oval / leaf -> kite). Dune, Marsh, Highland and Steppe T3 kits are no longer
  bare-headed. Still undrawn (art areas): helm form2 kettle, form7 conical-aventail (and horned, plumed, winged).
- **The stir** (compose): an idle's breathe frame (frame 1) of a look flagged `breeze` draws every rest-key part's
  `breeze` view (the T5-T6 cloaks' hem, the crest's tail), so the art LADDER-ARMOUR / LADDER-HEAD drew now moves.
- **Lying**: the corpse's dropped weapon or shield (screen items) lies 3 px up, under the body (was a line 2-3 px below it).
- **The archer** (the panel: "garish speckled orange"): skin #CA8E66, hair #8A5A30 (the leather pick a dark woodsman
  brown, the hair parted from it for C14).

### LADDER-DOLL, polish iteration 1 (2026-10-23; the blind panel's polish list P2, P5 grip, P6, P7)
- **HumanLook ADD** (ladder fields; key() at rig 2 only): `floor` (bits by slot: an empty slot drawn as the T1 floor) and
  `kitVar` (a ranked fighter's own kit, 0 none, 1-255). Looks macros take `floor=` and `kitVar=`.
- **The T1 floor** (gear::fillLadder): nobody who dresses from an inventory goes bare at rig 2: an empty body slot is T1 with
  the floor bit (the resolver draws T1's padded jerkin in the wearer's shirt colour), empty feet T1 leather boots. The
  player starts in T1 gear; a worn piece keeps its own tier. Rig 1 and the look's own fields untouched.
- **Each fighter's kit** (gear::rankTiers(L, tier, person = true); census::dress calls it as before): kitVar from the person's
  face (no draw). The resolver (ladderLook) varies helm, shield, gloves and boots a tier either way (never over body + 1),
  the shoulder and skirt dials a step, a third of T1-T3 bare-handed, a sergeant in six (T2-T4) with a travel cloak in the
  livery, one in eight of the village watch (T1-T2) bare-headed. The body keeps the rank. The looks macros' `tier=` passes
  person = false (the slice looks keep their authored kit).
- **Coverage, not holes**: pauldronOf takes the nearest drawn size of the family (the T2 cap asked for "plate.2", never
  drawn); the tabard is needed only where the library draws one for the body (the watchwoman fell back to v1 in a row of
  rig-2 guards). Still v1 in game: census looks whose armour form has no woman's map (scale 336, brigandine 257, lamellar
  241 in `--doll-coverage --scope all`): production art.
- **Corpses** (compose): a turned frame's screen items are the views that NAME `anchor pivot` (drops, the shield's lie); a
  part anchored at the pivot by default (the legs, the sword) turns with the body. The legs' `lie` stood upright out of
  every corpse. DEATH budget 52 KB (was 48: the legs now lie with the body; worst civil tool look 48.3).
- **The living finish** (art::finish, 6b restated): figureLights (glow + 2 px halo, the T6 aura, the Mythic's flame tongues,
  the edge sweep and its star) and world-space Particles (motes, embers; the Mythic's strike sheds embers off the tip) are
  the one model of the game (render.cpp: masks per frame of a T6 atlas, finishParts_ per actor) and of doll_gallery.
  doll_gallery `motion:<tag>[:<facing>[:night]]` plays a look at game speed (idle 3.3 s, walk 40 px/s, two blows, walk),
  30 frames a second, the board and an 80 x 72 follow crop at 3x. The walk's passing frames draw the `breeze` views.
- **C13 restated** (test_doll_ladder.cpp): the sweep's interval and run, particles' counts, rise and the walker's trail
  (8 px or more), per look and frame the bright and soft budgets, the still lights' step and the T6 soft-light floor, the
  cost of 8 figures (measured 0.043 ms).
- tools/scripts/p2c_polish.txt: the hero as he starts (T1 floor, `expect doll gear armour=padded`), four census lineups,
  the T6 and Mythic walking by day and night (0 failures).

### LADDER-DOLL, polish iteration 2 (2026-10-23; the blind panel on polish i1)
- **The T6 aura stands off the silhouette** (the panel: "the pale green glow outline softens the dark A3 outline", "vanishes
  in front of foliage"): one clear pixel, then a 2 px band (kAura x breath at 2 px, half at 3 px) of warm white-gold (the
  glow's colour 30 %, #FFF0C8 70 %) round the helm and shoulders (reach 0.45). The outline keeps its edge.
- **The Mythic's fire licks in separate tongues** off the crown only (the upper 30 % of the figure), 0-4.8 px on a smooth
  noise with gaps; i1's fire ring down the sides is gone (kFlame2 kept, not drawn): "an orange blob at 3x". On a strike
  frame the Mythic's blade flashes white-hot along grip to tip (0.45-0.85) so it parts from the burning harness.
- **The pieces breathe as one**: pixelLight's crawl share 0.18 (was 0.3): "busy, many separate glints" at night.
- C13 numbers unchanged (112 bright, 320 soft at 0.45 or less, 0.25 step); measured lt6 soft 157-307, lt6m bright 65.
- **Each fighter's kit, more of it** (ladderLook, kitVar, rig 2): a bonnet man (headwear cap, no helm, rank 2+) wears the
  people's kettle hat one in three (helmForm 2, iron; drawnForm finds the part), the rest dye their caps unevenly; the coat a
  step either way (padded -> leather 1/3, leather -> padded 1/4, mail at T2-T3 -> leather 1/4; the body tier stays the
  rank's); the shield's face dark livery 1/4 or a pale field 1/4. `p2cline npc <people> <role> [rank] [n] [mixed]`
  stages census fighters as the game makes them (the npc= macro: the role's base with its weapon); P2b's `lineup` dresses
  a bare look, which is why the i1 rows carried no weapons.
- **Corpse feet** (for the legs' owner or the integrator; legs_lie_feet.patch): legs/adult/loco/std.pxm `lie` foot 6 px wide
  in line with the shin (was 8: the turn stood its 3 px toe block up out of every corpse), tips.pxm `lie` origin 1 1 to match.

### LADDER-DOLL, alloys iteration 1 (2026-10-23; the owner: "does this fit with all our ores")
- **Alloy metal families** (doll.h `alloyFamilyCount/Name`, `alloyFamilyOf`, `alloyRamp`, `metalTest`; doll_ramps.cpp
  kAlloyFam): 16 families (rose copper, red copper, amber bronze, brass, electrum, olive bronze, green bronze, verdigris,
  cerulean, cobalt, ice silver, violet, plum, pearl rose, dark bronze, bottle green) replace i3's metalize() steel cast.
  Each ramp: L 8-16 core, a step 1 lit to luma 82 or more (the C14 edge), a step 4 glint of L 92-99. The sheen
  (cult::Sheen + 1) shapes it: matte has a soft top, bright a white glint, dark/glowing a blackened body with a cold rim,
  iridescent turns its hue across the ramp, pale is whitened, burnished turns its lights gold. Rig 2 only.
- **HumanLook::alloyFam** (new ladder field, 1-based, 0 = nearest to armourTint): mixed into key() at rig 2 (lad << 56).
  `gear::alloyFamilies(culture)` sets it in fillLadder for the armour's alloy. Each people has a family table per
  alloy rank, with every column distinct across the 12 peoples, so neighbouring peoples' same-rank alloys never match. If
  the table's family would sit within dE 20 of an earlier alloy, the nearest free family is used instead.
  `gear::alloyKeys(culture, i)` returns the icon's keys (gearIcon).
- Looks macros: `alloy=N` (kit metal pieces in alloy N) and `alloyFam=`. Census NPCs (looks.cpp; no culture in
  rankTiers) take the nearest family to their tint.
- **C15** (in --doll --gates and --doll-ladder): a, every family under every sheen passes metalTest; b, a people's alloys
  differ by dE76 >= 18 on the mid step (12 peoples x seeds 1..64; measured min 20.3); c, same-rank alloys of neighbouring
  different peoples (worlds 1..6, a 6x6 block of cells) differ by >= 12 (min 12.3); d, icon/doll hue agreement within 45
  degrees for alloys still on their nearest family.
- Finish (art_fx.h, A5): kRim 0.75 -> 0.95, the T6 rim reaching 76 % of the figure (was 62 %) with a fade of 0.5 (was 0.6),
  sweep run 0.40 -> 0.50 s, tail 6 -> 8 px, gleam 0.5 -> 0.65 s, 3 -> 4 px, 0.8 -> 0.9.
- Resolver: an undrawn helm band (4, 6, 7 from real heartland loot) falls back 4/6/7 -> 5 -> 3 -> 2.
- Metal kinds: starmetal is bluer and bronze browner (each had read as steel or as gilded beside the other).

### LADDER-DOLL, alloys iteration 2 (2026-10-23; the blind panel: warm alloys "one gold-orange figure", pinks "skin")
- **Families** (doll_alloy.cpp kAlloyFam, 16): red copper, amber bronze, brass, electrum, olive bronze, green bronze,
  bottle green, verdigris, blued steel, cobalt, white silver, violet, garnet, gunmetal, black bronze, slate iron (rose
  copper, pearl rose, plum, ice silver, cerulean and dark bronze retired). A fourth value row (L 6/31/40/60/93) for the
  black bronze and the garnet; matte's top 0.62 -> 0.8 of the way (it read as leather); a red's lights turn 18/30 deg
  toward gold at 55 % chroma; steps 2-3 kept 22 dE76 off A3's skin (#E9A077) by chroma and a hue turn away from it.
- **gear::alloyFamilies**: the table is [4][12]: everyday bronze/brass (Alloy::baseTier Bronze) -> a warm family,
  everyday iron -> a cool one, the warriors' and masterwork metals never a bronze. Solved offline: every column parts by
  dE 13 between peoples that meet in the atlas, a people's three by 21. Heartland: amber bronze / blued steel (the hero's
  T4), brass (the hero's T5: seed 7 has two alloys), electrum.
- **T5+ trim**: a body metal within dE 26 of the gold trim's mid step takes the silver trim (kTrim[2]).
- **Band metals**: bronze, iron (darker and warmer than steel), obsidian (black glass, near-white glint), emberforged
  (redder, a yellow-hot glint) retuned in kMetalKind.
- **C15e** (new): name agreement (everyday bronze warm, iron cool, no later bronze), every family x sheen 20 dE off the
  skin on steps 2-3, the 7 band metals pass metalTest and part pairwise by dE 14.
- **Finish (A5)**: the gleam every 2.8 s (was 3.3), 0.8 s across (was 0.65), 6 px wide (was 4); T6 motes 6 alive (was 4),
  born every 0.32 s (was 0.45), 1.5 s life (was 1.2), rising 12 px (was 10). C13 checks the constants.

### LADDER-DOLL, alloys iteration 3 (2026-10-23; the blind panel: names on the wrong metal, nine golds, lavender cloth)
- **Families by name** (doll_alloy.cpp kAlloyFam, 96 = 8 slots x 12 peoples, `alloyFamilyAt(slot, people)`,
  `alloyFamilyMetal(fam)`): slot from the alloy's rank and the metal word of its name (`gear::alloyMetalWord`: 0 BRONZE
  1 BRASS 2 IRON 3 STEEL 4 SILVER 5 GLASS): rank 0 bronze/brass/iron -> slots 0/1/2, rank 1 steel/silver -> 3/4, rank 2
  steel/silver/glass -> 5/6/7. `gear::alloyFamilies` takes the people's own family in the slot; a pair still within dE
  20 borrows the next people's family of the same slot (the name holds). The table was solved offline (tools/doll/alloys:
  sim.py replicates famRamp, `python opt.py 20` solves, port.py writes the table) against the real sheens: a people's three metals >= 22.5 dE on the mid step, two peoples'
  same-slot metals >= 13.5, cross-word same-rank >= 9.5, no step 2-3 pink, brown or skin, every sheen metalTest.
- **Ramp model**: value rows L {5 28 38 66 94}, {7 30 44 72 96}, {9 33 52 80 98}, {13 40 62 87 99}, {18 48 71 92 100}
  (v -2..2; v 2 the silvers), chroma per step 0.7 1.1 1.2 0.55 0.12; matte keeps a glint (0.85 of the way); burnished
  turns only a warm metal's lights to gold.
- **T5+ trim**: a culture alloy whose mid step has chroma >= 9 or luma < 120 takes its own ramp one step up as Metal2;
  a pale neutral keeps the gold (silver and gold).
- **Band metals**: iron's lit step 0x949092 (a dull plane under steel's near-white), gilded lemon 0xC8A020 body.
- **Heartland T5 kit**: gilded steel (Epic, ilvl 30) on the metal pieces; the noble cloak at ilvl 47 and the heater from
  the masterwork alloy (Rare, ilvl 47) keep their band-6 shapes. Resolver: a T5-T6 body's pauldron dial 2-3 lifts to 4.
- **C15c**: same rank and same metal word >= 12 (min 13.2); same rank, different metals >= 8. **C15e**: the family's
  metal equals the name's metal word (replaces i2's warm/cool rule). C15 min dE within a people 23.0.
- **Finish (A5)**: gleam every 2.4 s (was 2.8), 0.9 s across (was 0.8), 7 px wide (was 6).

### FORGE-DOLL, forge iteration 1 (2026-10-24; exe f1: the forge styles' data, parts and tile modes)
- **f1 exes**: build-p2c-forge/rpg_test.exe and doll_gallery.exe. Preview: `node tools/doll/pxc.mjs --root <dir> --no-cpp --bin
  X.pool`, then `doll_gallery <out> --pool X.pool forge[:t5|:woman|:alloys|:metalread|:ladder|:<tag,...>]` (the board reads
  tools/doll/forge_looks.txt; put `--looks` BEFORE `--tag` on the other boards). `rpg_test --expand-looks
  tools/doll/forge_looks.txt` prints each look's picks and its forge flags; `rpg_test --forge-picks [--people p] [--seeds A..B]`
  lists every alloy's seven picks; `rpg_test --forge-mech --pool X.pool` checks the mechanisms on a pool.
- **Data**: HumanLook::forge[TIER_SLOTS] (u64, a byte per axis); the value lists, the hero styles' table and the cut-on-family
  mapping in rpg/art/doll/doll_forge.h (header only, read by the sim and the doll); the pick rule in rpg/sim/gear_forge.cpp.
  fillLadder fills body, helm, shield, gloves, boots from each item (forgeOf), the T1 floor HOMESPUN / TANNER'S; rankTiers
  from the rank's metal, (D29) a realm's T4 officers its best alloy below the masterwork and its T5 king the masterwork
  (style and metal family; ladder fields only).
- **The cut on the family**: the resolver sets the body's cut flag on the family the armour draws (`form-<armour>` flag):
  a hero style takes its STYLE_P2 28.4 column; a generated cut maps by character (round, vertical, banded, angular). On mail
  every plate cut but smooth and banded is `cut-plackart`.
- **School (deviation, recorded)**: edge, collar, limb and fittings: each value has a fixed class A/B (gear_forge.cpp
  schoolClass; every people's menu holds both on edge, collar and fittings); the cell's checkerboard parity plus the rank
  picks the class and the best candidate of that class leads; these axes take no seed jitter. The 3 x 3 `s mod n` rule left
  down neighbours on one pick and let two peoples' menus collide (C16c failed 2-6 of 1008); with the classes C16c is 0 of 5092
  over worlds 1..30. Kit cultures (ids 0x1ADD...) and Atlas::make ones use seed % 9 and (seed / 9) & 1.
- **Flags**: `forge-<base>` (alloy homespun tanner foundry wrought armourer parade knapped furnace starfall mythic),
  `shoulder- cut- edge- collar- limb- fit-` (body), `crest-` (helm), `boss-<fit>` `rim-<edge>` (shield), `glimb-` `blimb-`,
  `form-<armour>` (with a body style), `blademetal-<kind>` (every armed look).
- **Parts**: `role pauldron <shape>.<size>` (the shape's nearest drawn size, then the family's, then none);
  `role overlay collar.<v>` (layer neck), `role overlay crest.<v>` with `anchor crown` (`layer headwear`, `z 1`); the crown is
  the helm view's `mark crown`, else the head box's top centre, moving with the head. Each is drawn only where the library
  has it (never a hole); `--doll-coverage --scope forge` names what is not drawn yet.
- **Cut tiles**: `tags tile` + `on-slot <slot>`; cells `+` `-` `=` `~` (step +1, -1, -2, 0) and R P G N (in the grid or a
  regions block); laid over the slot's lit pixels (outline, R, G, W untouched) of the body's armour and pauldron views, the
  grid on the view's origin; not over a `when` view keyed on a forge flag (the drawn overlay is the style). A legend cell
  writes its own slot and step.
- **Edge stamps**: `tags tile edge` + `on-edge hem|rim`; the view's origin row lies on each column's lowest opaque row
  (hem: the armour hang's base views when the frame draws one, else the torso's; rim: the pauldrons); cells below add up to
  2 rows in the hem's slot at the cell's step (Ink stays ink), `-` notches; base views only.
- **Fittings and limbs**: `role overlay fit.<v>` (`tags tile fit` on P, the `<prefix><w>x<h>` views; or `tags tile` +
  `on-region N`) on the body's pieces only; `role overlay limb.<v>` (`on-region ...`) narrowed to the regions of the slots
  that picked v (body U F T, gloves H C X, boots B S K); a body limb tile replaces the material's sleeve tile.
- **Key fallback**: a `when` view keyed on a forge flag (cut- forge- shoulder- edge- collar- limb- fit- crest- boss- rim-
  glimb- blimb- form-) drawn only for key rest serves every key of its facing with no view of that condition, clipped to
  that key view's opaque pixels (checked: 56 of 56 frames on fm_steel).
- **Blade (D30)**: every held weapon part (`role held` sword axe bow staff dagger mace greatsword spear stick, any form)
  paints its metal in Slot::Blade (the ramp of bladeKind; equal to Metal when they match). Props keep the armour's metal.
- **ComposeOpts::grey**: kGreyRamp on every material slot (doll::greyRamps); skin, hair, eyes, face and ink kept.
- **Macros**: `mat=` (cloth padded, leather leather, the metals in the kit's form; clears the alloy tint and sheen; the
  blade keeps its metal), `forge=<base>`, `style=<axis>:<value>[+...]` (`none` clears), `alloyStyle=<people>:<seed>:<n>`,
  `forgeBody=0x..`..`forgeBack=` (describeLook's output).

### FORGE-DOLL, forge iteration 2 (2026-10-24; the i1 blind panel: "the same armour recoloured", "Cloth wears the plate helm")
- **The style's own helm and shield (deviation, recorded; the owner's request outranks 3c's "form unchanged")**: doll_resolve
  `styleKitOf` draws a hero style's helm and shield shape from the library over a people's generic forms (helm nasal kettle
  great-helm spangen plumed crested; shield round kite heater tower oval buckler): HOMESPUN the cloth hood (Headwear `hood`,
  no crest, the gambeson's own Cloth3 ramp), TANNER'S the band-1 leather cap + buckler, FOUNDRY crested + round, WROUGHT kettle
  + round, ARMOURER'S nasal + tower, PARADE crested + oval, KNAPPED masked + round, FURNACE great helm + tower, STARFALL
  spangen + oval. The universal materials (TANNER'S..ARMOURER'S) only with `HumanLook::forgeKit` (ADD; the Heartland's kit:
  fillLadder by the helm's (else body's) maker, rankTiers by the realm, `mat=` always); another people's leather, bronze, iron
  and steel keep its helm, shield and plume (C12e). Signature shapes (horned, conical, masked, winged; crescent, leaf, none)
  are never replaced; at T5-T6 PARADE and STARFALL and always the MYTHIC keep the ladder's own helm.
- **A culture alloy's helm by its crest** (generic-helm peoples): comb crown fin -> crested; knife ridge -> nasal; brow finial
  -> kettle; spike spire shards -> spangen; chimney -> great helm; horns wings flame -> the people's. **Its shield** by
  `HumanLook::alloyRank` (ADD: 1 everyday, 2 warriors', 3 masterwork; fillLadder from the item's alloy index, rankTiers D29):
  the people's own or one of its two neighbours (round: oval buckler; kite: heater oval; heater: kite round; tower: oval kite;
  oval: round tower; buckler: round oval). **The masterwork's shoulders a size larger** (pauldronOf).
- **One crest per helm**: a forge crest takes the helm's own `_crest` / `_plume` part up to T4 where the style kit decides the
  helm (forgeKit, an alloy, or a band metal); on another people's universal metal the plume stays and the style's crest (part
  and need) goes. No crest at T2 (STYLE_P2 27.4: the T2 cap plain).
- **Pick rule (gear_forge)**: a culture's alloys part on the shoulder AND the crest (plus i1's distance 7 and two silhouette
  axes); the metal word's hero is parted on the crest; strictness steps down 2 -> 1 -> 0 in the bounded search when a
  people's menu is too short (Dune's three crests with PARADE's crown among them); the crest leads by the cell's parity and
  the shoulder by its column (`forgeColumn`: ci & 1), values swapped, scores kept, so every orthogonal or diagonal neighbour
  parts on one of the two.
- **The glint pass (doll_compose `glintPass`, styled bodies from T2)**: to T4 the three step-4 clusters nearest the top-left
  light keep their glint on the body's armour and pauldrons, the rest drop to step 3; a figure with none takes one on its
  upper-left lit plane (step 3, else 2); the sheen: matte drops step-4 runs to 1 px, bright makes the top glint a 2 px run;
  a glowing alloy at T4-T5 lights its seams where the form has few N pixels (an inner step-1 pixel between lighter metal, then
  any inner step-1, then every other pixel of the widest step-2 row; 6-10). `DollRecipe::sheen` (ADD) carries the sheen.
- **Ramps (rampsOf)**: a pale alloy's step 2 whitened to 150 (chroma falls), a dark one's under 90 with a glint 196+, parted
  from skin and hair by going darker; steps 2-3 of every metal 20 dE off the look's skin and A3's (chroma together); the lit
  plane 0.57 x step 2's chroma; a styled metal's step 1 lifted toward 80 luma under its caps (45 % of step 3, step 2 - 12,
  step 3 - 61), step 3 or step 2 rising a little where they cap it; a styled figure's leather and cloths 16 dE off metal steps
  2-3 and the shirt 18 luma off the hair (C14's mover, partner sets stepping down when none clears all); the cloak or the
  accent off the metal; ComposeOpts::grey also greys Slot::Head (the hood).
- **Gates**: C17's glint count 1-3 to T4, 1 or more at T5-T6 (C12b's rising finish); C14's lit edge for a dark sheen is its
  own step 1; B10 allows a forge crest the crested RUN 68 KB and a styled body's LOCO and RUN 3 % (PLAN constants "forge:
  atlases"). `rpg_test --doll-forge-drawn [--report]` runs the drawn forge gates alone (FORGE_RAMPS=1 or =<looks file> prints
  the looks' ramp lumas). tools/doll/forge_looks.txt adds a_fj2 (Fjordfolk FROSTSTEEL: the coif collar, B11's coverage).

### INTEGRATE, forge iteration 2 (2026-10-24)
- **Merged**: FORGE-HELM's headwear/helm, headwear/collar and shield maps and FORGE-MARKS' maps/forge/** and finish/{etch,
  filigree,glyph} from their isolated pools (cut/lambanded and cut/mailbanded removed); FORGE-PLATE's six man and woman
  mail/plate/plate_hang maps and FORGE-SHOULDERS' 56 armour/shared maps were already in the tree; FORGE-DOLL's code as is.
- **C3**: the tower's bottom-left corner is filled in its R guard and L rest views (a 1 px pocket between the corner, the
  mail and its hang on lt3, whose ARMOURER'S kit now bears the tower); the Mythic heater's D and U rest views sit a column
  further in (origin 7 8 and 5 7: the RUN box + 1 on lt6m). C3c's grip: facing up, a grip whose 3 x 3 is all cloak drawn
  over the shield with a shield pixel within 3 px counts as attached (the T5 surcoat and the noble cloak hang over the far
  arm; by eye the shield runs on under the cloak to the hand). `EMB_C3_DEBUG=1` prints the grip, hole and run-box surrounds.
- **C14**: the shield's field parts from the leather too where it can (HOMESPUN's straps sat 2 luma off it).
- **C17**: the glint pass runs on any styled look (a soft body under a styled helm) and, where the body's own metal holds
  no lit pair (the Highland and Marsh kits' mail) or too little plate for six glow seams (the Sylvan kit), the bright
  sheen's 2 px run and the glowing seam line fall back to any metal of the figure (helm, shield); a styled body with under
  12 px of metal still takes its one glint.

### FORGE-DOLL, forge iteration 3 (2026-10-24; the i2 blind panel: "about 20 of the 37 alloys wear the same kettle helm on the same torso", "the 6 Jade alloys look like one armour", "Emberforged reads as painted clay")
- **The guise** (`art::doll::forge::Guise`, doll_forge.h; `gear::alloyGuise(c, i)`, gear_forge.cpp; `HumanLook::forgeGuise`
  uint16, derived, never saved, key() at rig 2 only; macro key `forgeGuise=`; `--expand-looks` prints it): a culture alloy's
  silhouette beyond the seven picks: head (HelmForm + 1 of a drawn helm: 1 nasal 2 kettle 3 great 4 spangen 8 mask 9 crested;
  or a metal head cut from the headwear library: 11 coif = hood, 12 turban helm = turban, 13 jingasa = conical, 14 aventail
  = veil), skirt 2-4, cloak 0/2/3 (the masterwork), shoulders 2-4. Packed `helm | skirt << 4 | cloak << 7 | pd << 9 | 1 << 15`.
- **The head rule**: each people's menu of six heads (kVocab; its most typical first, the mail hoods late); the everyday alloy
  starts on one of the first two (the family's first alloy name picks), the crest's own helm next where the people makes it,
  then the menu rotated by the family's name; a culture's alloys never share a head SHAPE (the open domes nasal / kettle /
  spangen / crested are one shape, coif and aventail one) and never the people's own helm (as the doll draws it: an undrawn
  form's nearest); the everyday alloy not even its shape (C16c: the universal materials wear the people's helm there). The
  Heartland: its materials wear their hero helms, so an alloy keeps off its metal word's hero helm by the code only.
- **The doll** (doll_resolve): styleKitOf takes the guise's head for an alloy helm over every people's form (their horned,
  conical, winged forms were never drawn and all fell back to the spangenhelm); a hood head draws the crested
  helm instead at body T5-T6 (C4: the king's armour swap read 23 %). A headwear head sets `DollRecipe::guiseHead`; the compositor draws
  that part's cloth, head, linen and leather pixels in Metal; rampsOf copies Metal into Head. The guise's skirt and shoulders
  only LIFT the dials (C12c); the masterwork's cloak at T4+ in the tabard colour at 70 %; the Dune and every kit with no
  shoulders take the guise's size. The glint pass and C17 count a guise head as body armour.
- **Hero heads**: PARADE wears the gilt turban helm (12) below T5 (C16a bronze vs gilt 0.29 -> pass); ARMOURER'S in the
  aventail was tried and dropped (C3e, C4, C12c); ARMOURER'S on the Heartland's mail wears the hauberk to the knee and its
  guards a size up (C12a +6.2 %).
- **Fixes**: the matte glint pass ran left to right and split a 3 px run into two glints (C17 4-5 glints); emberforged's
  ramp a forge-dark red with a yellow straw lit plane (C17 holds its chroma under 60 % of the body's; an orange went pink).
- **Gates**: C16g (data) a culture's alloys never share a head shape; no shape on over 40 % of the alloys (seeds 1..64:
  dome 24 %, great 18 %, mask 18 %, hood 11 %, turban 16 %, jingasa 11 %); selftest: an unset guise has no head. C3e counts
  a guise head (Headwear) as the helm on the lying frames. `doll_gallery specs:<label>=<spec>;...` previews any specs.

### FORGE-DOLL, forge round 2, iteration 1 (2026-10-24; the owner: "every alloy or material armor should be different in style"; exes g1, g2)
- **Data** (contracts 3c.1 as built): `HumanLook::forgeShape` uint32 (helm bits 0-4, skirt 5-7, mass 8-10, layer 11-13, arm
  14-15 stored as arm - 1 so an unset arm reads as CLOSE, motif 16-19, bit 31), key() at rig 2 only when set; the value lists,
  `packShapeAxes`, `kShapeVocab` (STYLE_P2 28.5 exactly, '*' the signatures), the rank flavour, `massShapes`, `helmClosed`,
  `helmSocket`, the design estimates (`estBodyOD`, `estHeadOD`) in doll_forge.h; `gear::Shape`, `packShape`, `shapeKey`,
  `keyAxesApart`, `forgeSchool6`, `heroShape`, `tableKey`, `alloyShape`, `shapeOf`, `describeShape`, `alloyDetails`,
  `alloyRoot`, the solver (`ShapeModel`, `solveShapes`, `checkShapeTable`, `materialKeys`, `shapeTableInc`), `forgeBoardSet`,
  `forgeTwinGroups` in gear_forge; `gear::kitCulture` and `gear::alloyKitLook` (a culture's alloy on its people's kit) in
  gear_ladder. `forgeGuise` is retired: fillLadder and rankTiers leave it 0; the resolver still draws it when a look sets it
  (the selftests' round-1 bodies).
- **Kit schools**: ((seed - 1) * 12 + archetype + 6 * ((seed - 1) / 3)) mod 36: seeds 1..3 are the contract's formula; the
  term moves seed 5 off seed 2 (the owner's board holds Jade seeds 2 AND 5: KISTEEL and DRAGONSTEEL would have shared a key).
  The board set is seeds 1..3 x 12 peoples plus Jade seed 5 (102 alloys; the owner's 37 among them). The plan's "Jade seed 3
  JAONBRASS DRAGONSTEEL" are Jade seed 5's.
- **Shoulder values APPENDED**: pelt (17), feather (18); MANTLE draws fur, pelt, feather (the SunTemple's feathers first).
- **The details**: root word (kRootDetails, STYLE_P2 28.5's table; the root is the name's prefix before the metal word), the
  hidden word (masterworks), then round 1's boosts; the crest only on a socketed family and only from that family's crests
  (CRESTED comb knife fin ridge, KABUTO horns fin crown, CONE spike finial knife ridge horns wings, ...); the motif filtered by
  the sheen; a culture's alloys prefer another motif and arm. `alloyStyle` returns base 0 + shoulder, crest, collar (cut, edge,
  limb, fittings 0); before a table exists (bootstrap) round 1's pick stands. alloyPick and alloyGuise keep round 1's rule.
- **The shape table**: rpg/sim/gear_forge_shapes.inc, written by `rpg_test --forge-solve [--pool P] [--estimate]`. Measured on
  the pool by default: every helm family on the Heartland seed-1 man (head OD), every skirt x mass x layer on each people's
  seed-1 T4 man in its warriors' alloy (the people's first helm family, close arms, no motif, the mass in its first shape), the
  materials by `mat=` on the same kit; a value the pool does not draw yet keeps the estimate for every pair it is in. Hard:
  rules 1-4 (a failing solve prints the people and school). Soft, as penalties: board-set twins on the REAL board figures
  (80), twins with adjacent schools (10) and with any cell sharing a window of worlds 1..6 (6); exact board keys never repeat
  across peoples; the spread (no helm family on more than 20 % or body outline on more than 4 % of the board set and of each
  window; per people helm 25 %, body 4 %; every menu value used); then the weights, the rank flavour, the signature on
  masterworks. Board cells first, then the peoples with the fewest candidates (the Heartland), five passes. The installed
  table: measured on pool 8927bce4710d3d52 (the repo maps of 16:20, FORGE-HEAD's families and the first skirts and motifs).
- **The resolver**: `ShapeUse` (an alloy body: base 0, a body tier, an armour; an alloy helm: base 0 and a family 1..16; off
  under a guise). Helm `helm fam.<v>` (FURNACE `fam.furnace`, STARFALL below T5 `fam.crystal` where styleKitOf gives the hero
  a helm; FURNACE's shield the round one); skirt `overlay skirt.<v>` drops the armour's hangFront/hangBack parts (`none` drops
  them and needs nothing; a hang part that itself serves the asked skirt stays); sleeves `overlay sleeve.wide|heavy`
  (FURNACE heavy); bare arms F and C to skin (C linen on padded); motif `overlay motif.<v>` (PLAIN needs no part); layer
  tabard `overlay tabard` (the guard's tabard gives way), sash `overlay sash`, halfcape `cloak halfcape`, cape the noble
  cloak, scarf `neck scarf` (ladderLook clears the ladder's cape dial; capes in the people's colour at 70 %); mass NONE no
  pauldron, else the dial lifted (T4 3, T5+ 4). A value the pool does not draw keeps today's part and goes in
  `DollRecipe::forgeMissing` (`--scope forge` and `--expand-looks` list it). `DollRecipe::shape` holds the applied values;
  flags `helmfam-` `skirt-` `mass-` `layer-` `arm-` `motif-` (applied), `alloyshape`, `alloyface`, `glowseams`. FURNACE's N
  regions glow at T1-T5 on every piece made of it (D40); alloy boots K S B in metal from T4; alloyface mixes the Field ramp 30 %.
- **The compositor**: D41 pauldrons (near, armNear) on head z -4 when the look has a cloak or a T5+ body, never on the DEATH
  part (it closed pockets against the helm, C3a), and a lifted pauldron's erase cells cut nothing; iridescent bands (skirt and
  motif metal alternate Metal / Metal 2 per band between N seam rows, every 3 rows where none is marked); the motif's rest view
  serves torso keys it lacks, clipped to the torso armour's view; ComposeOpts::noHeld (no held, shield, sheath or fx parts).
- **The outlines**: doll_outline.cpp `outlineOf` (LOCO, grey + noHeld, idle D, idle R, walk R frame 1; split at the head box's
  bottom row), `outlineDistance` (XOR / union per part, averaged, and the smallest frame), `outlinePixels`.
- **Ramps**: alloyRamp ends in `metalRound2` (the skin box first: the body darker, a pale body and the lit plane paler, then the
  hue toward the gold; the lit plane re-capped at 56 % of the body's chroma; warm lit planes turned 18 degrees toward the gold
  on the quantised value; greens' lit plane 48 % of the body's chroma); rampsOf runs it on an alloy's Metal before and after
  its partings, then parts the body from the hair and skin (darker first; a pale body lighter first, the lit plane climbing
  with it). Bands: obsidian 0B0910 1A1624 302B44 868596 F2F6FF; ember 1C1412 3A2622 5E3A30 887868 F2D6B0, glow FF7A24; bronze
  body 7C4018 / 9A561C, lit E8D4B6, glint FFF4D8. A styled Accent parts from the skin too.
- **Gates**: --doll --gates: C16b/C16c retired; C16o (culture pairs body 0.08 / frame 0.05 / head 0.10, seeds 1..8,
  FORGE_SEEDS=n; materials on seeds 1..3; the four hero pairs), C16m (luma range 40 in the torso box blurred 2 x 2; 10 apart),
  C16n (a gear pixel above or below all of its 3-4 same-slot plane neighbours, step 0 lines and step 4 aside, 3 % or less),
  C17 visor (closed families: 2 rows of 5+ step 0-1 helm pixels on head rows 5-12), C17's round-2 terms (metalRead2: warm,
  green, skin box; KNAPPED median 50 / lit hue 200-290; glowseams 8 glow px at T4; iridescent 20-80 % Metal 2 on the body's
  armour and two hues 40 degrees apart); selftests: round-1 Highland FAIL C16o, a checker FAILs C16n, a closed helm with its
  band lifted FAILs the visor, a one-hue iridescent FAILs C17. C14's edge held to step 1 for KNAPPED and FURNACE (as the dark
  sheen); C12b's cloak ceiling 30 % (D41); C13 allows glowseams below T5. --doll-forge: C16d (culture rule, materials, the
  table's rules with its own model), C16u (board set and worlds' windows at cells 0..5 and 3..8), the round-2 list as rows;
  C16b/c/g retired. --doll-forge --silhouettes: C18a/b/c, FORGE_SIL_OUT. --forge-shapes [--check], --forge-solve.
  --doll-coverage --scope forge adds the board set and every forgeMissing value and sleeve key.
- **Measured at the end of iteration 1** (repo pool 8927bce4710d3d52, 1,883,531 B: OVER the 1,835,008 budget):
  --doll-forge --worlds 1..6 0 failures; --forge-shapes --check 0; C18a 19, C18b 1, C18c 0; C16o 5, C16m 23, C16n 9; C17
  58 glow seams + 16 iridescent (N marks and bands not drawn yet) ; C3 11, C12 6, C13 4, B10 8, B11 limbs 48, B11 wear 270
  (part coverage of the new motif / skirt maps: no forge look judges them yet); C14 1 (g_jd3b skin vs accent). Rig 1
  unchanged (art_hash 317eef6efe4d8229, village-art 65 / 0, --anim 0, --arms ALL OK); full rpg_test and save_test ALL OK;
  WSL clang 18 --doll --gates and --doll-forge match MSVC line for line.

### FORGE-DOLL, forge round 2, iteration 2 (2026-10-24; the i1 blind panel: "about 6 of the 37 alloys unique by outline", "Emberforged a clay stove", "brasses read as leather", "Mythic fire-sparkle soup", "T1 rash")
- **Shape table**: the solver spreads the helm families over the owner's 37 (a second alloy in one family costs 2.5, a third
  10, and two that share a family stand apart on all three body axes) and over the board set (8 of about 108 at most); the
  CROWN leans to the masterwork (-1.5 at ranks 0-1: "a crown on a mid-tier hauberk muddles the ladder"). The measured
  rounds warm-start from the best table so far from round 3 (ShapeModel::start), so they converge instead of starting over.
  Owner board: no family on more than 3 of its alloys (i1: winged 5, veil 4, warhat 3).
- **Details**: a culture's later alloy never repeats an earlier one's crest (the next crest its family's socket takes; the
  root still sets shoulder, motif, arm and collar): "spike-top cones: Froststeel, Thorniron, Thornsteel".
- **Ramps**: KNAPPED 0A0810 181424 2A2440 82808F F2F6FF (the lit facet cold violet-grey, its chroma 60 % of the body's: C17); FURNACE 120C0E 261A1C 40292A
  706462 FFD8A8 (black iron); bronze 24140A 5A3E10 8C6A1E E8D8A8 FFF6DC (olive-gold, off the leather). Alloy round-2 read: a
  warm body in the skin box turns 10 degrees toward the gold before it darkens; a warm alloy's shadow turns 15 degrees toward
  the olive (hue 45-92 only: coppers keep their red) and its lit plane climbs toward white to 35 luma over the body (never
  into the skin box); the dark core (step 1 at 44.5 % of step 3's luma) is restored after every move. (A green body's
  chroma cap was tried and dropped: it closed C15f's green steels and C15c's teal / emerald.)
- **Compose**: flag `furnace` (FURNACE body, T1-T5): the dark seams and vents (a dark pixel under a brighter lip, or between
  two lit pixels) glow as whole crack lines, body first (18 px) then helm, shield and limbs, 26 in all. Iridescent: every
  iridescent piece (skirt, motif, sleeve, body armour, pauldrons, helm) alternates Metal / Metal 2 in 2-row bands over steps
  0-4. The Mythic: a lone glow pixel (no glow neighbour) goes dark. Wear stamps and a quilt's accent ties (forge cut on Cloth3)
  draw in the garment's own shade, never a second hue.
- **Gates**: C16m's "10 apart" is the means apart OR the mean per-pixel difference over the shared torso box (PLAN C16 m
  "differ by 10 luma or more (mean) there"). C12b: a culture alloy's styled body (`alloyshape`) holds 3 glints or more and
  75 % of the tier below's colours (STYLE_P2 28.8's one to three glints). B11 wear judges the forge coverage looks g_cov1w-3w
  (no shield on the sleeve looks: the shield hides the far arm); g_cov3w adds the woman's strips skirt.
- C14: a styled alloy whose accent cannot clear the hair and the shirt together keeps the shirt (mr_1's sash).

### FORGE-DOLL, forge round 2, iteration 3 (2026-10-24; the i2 blind panel: "helm families repeat across the 37: wide hats, prongs, buckets", "Uhabrass, Jaonbrass flat brown leather", "Anibrass cream, Sandsteel plaster", "Amiorsteel a pink and blue clown", "Emberforged a cold stove", "Mythic 1 px glitter", "the board's CT ED LI FI columns blank on every alloy")
- **Shape table**: `helmAlike` (gear_forge.cpp): WARHAT KABUTO KETTLE (a wide brim), HORNED WINGED (two prongs), CONE VEIL ONION
  (a pointed dome), GREAT SNOUT MASK (a closed bucket); on the owner's 37 an alike pair (or any two under head OD 0.16) counts
  0.6 of a repeat in the quadratic spread term, and two alike keys that share a body axis cost 7 (14 for one family), two
  shared 10 more (20). A per-culture alike term (6 a pair) was tried: it moved the 37 into six plumes and five or six crowns
  and its table (C18a 2 on the gate) was dropped. Installed table: re-solved on the integrated repo pool (pxc hash
  ac4b6df4b1c03bbc, 1,816,582 B: the art areas' round-3 maps of 20:23-20:54), 0 hard, C18a 0, board-set twins 7, adjacent 2;
  owner's 37 by look-alike group: prongs 6, brim 6, point 6, crested 4, bucket 4, plumed 4, crown 3, morion 2, sallet 2 (no
  family on more than 4); of its 68 alike pairs 58 differ on all three body axes and 10 on two.
- **Ramps** (round2Read, now `round2Read(r, sheen)`; `metalRound2(r, sheen)` takes the look's sheen): a dark warm body (Lab
  hue 55-110, chroma 12+, L under 38; not a dark or glowing sheen) climbs 60 % of the way to L 44 and turns up to 12 degrees
  toward hue 88 (brass, never a hide; coppers and red bronzes under 55 keep their depth: C15f); a cream, bone or plaster body
  (hue 40-120, chroma 8-34, L over 66) sinks to L 64 unless that lands in the skin box; every metal's lit plane at least
  30 L over its body and its glint at L 93 or more. A green chroma cap stays dropped (C15f, C15c). Iridescent Metal 2: the
  110-degree turn goes whichever way lands farther from the pinks (hue 15), steps 1-3 at chroma 16-22. FURNACE 120C0E
  2A1614 4A2622 786A5E FFD8A8 (a red-black char; the lit plane 49 % of the body's chroma, hue 69: C17's warm rule).
- **C14 parting**: the hair/skin parting after the round-2 read also parts from Cloth2 and never takes a move that breaks
  metalRead2; the trousers' cascade tries {Cloth1 Hair Metal} darker and {Leather Hair Metal} lighter before dropping the
  metal (a_fj2).
- **Compose**: FURNACE heat halo: a step 1-2 metal pixel under (then beside) a glowing crack takes Glow step 1, 16 at most.
  Mythic: after the speck rule, a lone metal / Metal 2 / glow pixel whose four neighbours share one other non-outline code
  takes it, and a metal or glow pixel with no figure neighbour is cleared.
- **Boards**: forgeSheet's legend is HELM SKIRT MASS LAYER ARM MOTIF (forgeShape as applied) + SHAPE (the shoulder's shape)
  + CREST; the header grew one row (top 14 + 10 x 9).
- **Looks**: g_cov4w on Heartland alloy 1 (alloy 2's hauberk under the tabard showed no glint facing R); g_cov5w (woman,
  Jade alloy 1, skirt:bell) judges armour/woman/skirt_bell(_back) in B11 wear. `--doll-coverage --scope forge` skips the
  "plain" edge and limb values (they draw nothing, like motif PLAIN).
- **Measured** (build-p2c-forge, repo pool ac4b6df4b1c03bbc, 1,816,582 B; B1 16/16, B10 0): --doll --gates --parity 0 failures (selftests ALL CAUGHT);
  --doll-forge --worlds 1..6 0; --silhouettes 0 (C18a 0 of 56100, C18b 0, C18c 0); --forge-shapes --check 0; coverage forge
  and ladder 0 missing; --motion 0 (62 looks); full rpg_test ALL OK; save_test ALL OK; art_hash 317eef6efe4d8229;
  village-art 65 / 0; --anim 0; --arms ALL OK; tools/scripts/p2c_forge.txt 0 failures and witness p2c_forge, p2c_ladder,
  p2c_slice ok (the Jade s5 masterwork, UNSHISILVER on the new table, expects crest=crown, not spike); WSL clang 18
  --doll --gates and --doll-forge 0, as MSVC; owner pack --forge2: 31 files, every image 8000 px or less.

### FORGE-DOLL, envelope iteration 1 (2026-10-27; exe e1: the envelope's data, gates and code, wave 1d first block)

- **doll_envelope.h** (new): `kEnvelope[7]`, `kAreaStep`, `kPoolBudget` (2,621,440), `kMeasuredCap`, `kHeadOdStep` 0.10,
  `kHeadAreaStepPct` 5, `envelopeOf`, `envelopeAtlasKB`, `describeEnvelope` (`rpg_test --envelope`). The T5-T6 ACTION, DEATH
  and POSTURE stay 0 (the cap 716 / 72 / 89): e6 today measures ACTION 123.6, DEATH 43.7 KB, which x 1.1 would sit UNDER the
  T1-T3 row (512 / 52); they are measured again on e6 at first light (its envelope art) and never set below the T1-T3 row.
- **The resolver**: pauldron size = min(dial (+1 masterwork), kEnvelope.pauldron) at T1-T4; T5-T6 ask for size 5 / 6 with the
  fallback 6 -> 5 -> 4 (nothing drawn at 5-6 yet, so T5-T6 still wear 4; `DollRecipe::envelopeMissing`, listed by
  `--doll-coverage --scope envelope`, NOT forgeMissing so forge coverage keeps its scope). Sizes are nominal: a drawing
  measured wider than its tier's shoulders (pauldronAcross: the facing-D rest views past the head box's 16 columns) is listed
  and C12f fails the look; it is never swapped for a smaller size (that hid C3a holes at T4). Today: flared3 22 px (T3 cap 20),
  slab4 24 px (T4 cap 22) -> FORGE-MANTLE redraws to the sizes' reach (2 / 3 px).
- **The head key** (D51): `gear::Shape::variant`, forgeShape bits 20-22 (`forge::shapeVariant`, `withShapeVariant`,
  `headKeyOf`, `helmVariantCount`, `legacyHelmVariant`), `gear::headKey`, `alloyHeadKey`, `kBoardHeads` (name, family, variant,
  crest, the whole df2a155 key). The resolver flags `variant-<n>` (a culture alloy's from forgeShape; every other look the
  variant its mass drew, so no approved pixel moves). The maps are still mass-keyed: **tools/doll/rekey_variants.mjs**
  (`--root DIR --write`) re-keys `when helmfam-<v>,<mass terms>` to `when helmfam-<v>,variant-<n>` (helm and crest maps, 105
  views, pixels unchanged; a duplicate keeps its mass terms so pxc's one-view rule holds). FORGE-HEAD runs it on its root; until
  then the hat swap and the board's partings exist only in the data. New drawings asked: horned 1 (STAGBRONZE), crown 2
  (JAGUARIRON), crown 3 (BRIARSTEEL), morion 1 (VENSTEEL). Variant numbers: doll_forge.h's comment.
- **The board's heads** (kBoardHeads): PAPIRON warhat 1 (the great kasa), JAONBRASS warhat 2 (the gat), CRANESILVER warhat 0;
  SLAURIRON great 2 (sugarloaf) vs GREBRASS great 1; UHABRASS winged 1 (eagle) vs THAERGLASS winged 3; SUNBRONZE crest spike vs
  ILSILVER finial; DRAGONSTEEL kabuto 1 (its crescent, kept) vs KISTEEL 0; the crown trio 1 / 2 / 3 and UNSHISILVER 0. Pairwise
  distinct (C16k data 0).
- **The solve**: the 37's whole df2a155 keys pinned (only their helm when the whole key breaks a hard rule), rule 5 (the metal
  rule: a bronze or brass rank-0 alloy never NONE+TABARD nor MANTLE+TABARD/HALFCAPE; `warmEverydayCell` over seeds 1..64) and
  rule 6 (the 37's helm families) in `--forge-shapes --check`. Re-solved on the repo pool (--keep): 53 of 1,296 keys moved, none
  of the 37 (diff: only SUNBRONZE's crest), 0 hard, C18a 0.
- **Ramps** (D52): `antiqueRamp` (doll_alloy.cpp) on a warm metal under the dark sheen, last in rampsOf's metal block; C17's dark
  term reads D52 on bronze and brass; a styled alloy's tabard re-parts from the skin when the brass took its room.
- **Compose** (A8): glintPass's lone-lit rule (a metal step-3 pixel with no lit 4-neighbour takes step 2) and the new
  `glintCapPass` (every frame of a styled figure, soft ones too: step-4 metal in 3 clusters, a horizontal 2 px run first, then the
  body's, the larger first; then the lone rule again).
- **render.cpp**: a rig-2 figure whose feet stand in a DoorH tile sorts before the door (the jambs over its pauldrons). The
  overhang over an interior partition's wall face needs a per-blit clip (engine/pix: not FORGE-DOLL's file): asked.
- **Gates**: `--doll-envelope` (C12f envelope, C12f ladder lines, C3b by tier, C13 drawn motion = a frame against its breeze
  views via compose, B10 by tier), `--doll-adefects` (C19 A1-A8), `--doll-heads` and C16k in `--doll-forge` (data, a head each,
  heads apart at 1x, stability), all with selftests; the envelope and C19 lines in `--doll --gates`. B10 by tier in the matrix
  (envelopeAtlasKB + the allowances), W8 by the cloak's tier, C8 2.5 MiB, pxc prints the real budget. `measureEnvelope`
  (doll_outline.cpp) for `expect doll envelope <tier|shoulders|rows|area>=<n>` and Witness `player.sprite.doll.envelope`.
- **Boards** (doll_gallery): `envelope:<hero|heroine|guard|adefects>[:grey|:black]`, `heads:owner37`. Scripts:
  tools/scripts/p2c_envelope.txt, witness/p2c_envelope.witness.json. Looks: tools/doll/envelope_looks.txt (+ a_papir, a_crane
  for A2's distances), embedded by embed_looks.py.
- **Not yet** (next block): `role helm fam.<v> variants <n>` in pxc, the 8x caps guides and the stir GIF boards, the
  `adefects` before/after board (`--pool-before`), owner_pack --envelope, the glass and iridescent and Mythic-light passes
  (waiting on FORGE-MATERIAL's spec), the skirt-flare term of C12f.

### DOLL, envelope fix round, r1 (2026-10-28; D54-D65; exe r4a then the integrated r4 build in build-p2c-env4)

- **T2 kit** (D55; `doll_resolve` `t2Kit(L)`: forgeKit, body T2, base TANNER): `pauldronOf` asks `capelet.2` first (else today's
  shoulders, listed `pauldron capelet.2 (T2 kit, D55) -> today's shoulders`); `needsOf` puts `neck cowl` in the scarf's place
  once drawn for the body (else the scarf, listed); the helm is a `role helm softcap` part where drawn, else the leather cap
  (HEAD drew its T2 views as `when helm.t2,!helm.t3,!bow,forge-tanner replace` ahead of the old T2 views); flags `softcap`, `t2kit`.
- **The watch at T3** (D57; `watchT3(L)`: Outfit::Guard, body T3, no forge kit): `ladderLook` sets the kettle (helmForm 2) at helm
  T3, pauldron 3, the round shield (shieldForm 1) at shield T3 where he bears one, and the cape worn at cloak T3; `pauldronOf`
  asks the cop at medium first. eg3 is `npc=heartland:guard:0:7,tier=3` (r3's `pauldron=4` retired).
- **A3** (D58; trace): facing D the torso box (armour + motif) was already all Metal (50 px); the blue and tan came from (a) the
  lamellar's `when sheen-dark,alloyshape replace` view existing for key `rest` only, so the idle (key `idle`), run and swing R
  views show the old open front with the tunic (Cloth1 #28728C, its Leather belt) under it (BODY's map), (b) the scarf
  (layer-scarf, Accent = trimColor #40679B: cloth, allowed), (c) the BIB's Accent cells. Code: the flag `brassbody` (a brass or
  bronze everyday alloy: metalKind 10, alloyRank 0-1, alloyFamilyMetal 0-1, armoured) makes the compositor paint the torso
  armour's and the chest motif's cloth and Accent cells in Metal (leather straps keep their leather, forge fix i1).
- **C12f guard** (D57): `guardRule` (eg3 >= 1.20 of e1 and 1.10 of eg0 by C12f's measure, shoulders >= 20, the cape outside
  every other pixel of its row on >= 6 rows each side above the legs facing D, >= 1 column behind facing R); constants
  kGuardOverHeroT1 120, kGuardOverOwnTier 110, kGuardShoulders 20, kGuardCapeRowsD 6, kGuardCapeBackR 1 (doll_envelope.h);
  printed `C12f guard eg3: ...`; the T2 head budget printed per line (`T2 head budget: N px now, M or less`). Selftests 10.
- **C19 round 4** (D56, D58-D62): FORGE-MATERIAL's reference rows (`r4Obsidian`, `r4Mythic`, `r4Star`, `r4Glass`, `rimRun`,
  `faceBox`, `starBands`, `litStat`) merged; A3's torso box term (`torsoMix`: Metal >= 50 %, Cloth1-3 + Linen + Accent <= 30 %,
  idle D and R, noHeld); 13 new selftests, each a bad frame (C19 selftests 19). D56's scope is the `torsoPx` comment.
- **D63**: doll_gallery `greyRows` (luma, every pixel, labels included, after everything is drawn) on every grey sheet
  (shapeGrid's SM_GREY, the forge sheets' grey band) and `greyCheck` (prints `ok / FAIL: grey board <name>`; the run exits 1 on
  any coloured pixel). owner_pack.py `--envelope` writes greys with `env_grey` and fails on any grey file with R != G != B.
- **D64**: `applyLadderSpec` parses `kit=<people>:<tier>[m]:<seed>` strictly (`6m` = rarity mythic; `5m`, `6x`, `m`, `9`, or
  `6m` with another rarity: an error naming the tag). `--doll-envelope` line `D64 kit tags` checks the two spellings' describeLook.
- **D65**: script command `settle [max s] [gap s]` (script_p2c.cpp: the world drawn with the hero hidden every gap, until two
  draws are identical, max at most; logs; never fails) after every `skill clearing` in p2c_envelope.txt and the new
  p2c_envelope_phone.txt; p2c_envelope_seam.txt is the repro without `clearing`. Finding: the straight-edged block beside the
  hero at seed 7 (-140, -432) is in the game itself (no clearing: at 0.8 s, after settle and at 6 s), so it is a terrain bug for
  the owner, not a capture artefact; a living scene is never still, so `settle` shoots at its 3 s cap.
- **Tools**: `rpg_test --doll-recipe <tag>[,..] [--map] [--looks f] [--pool f]` (needs, parts, flags, fallbacks, measures; --map the
  idle D / R slot-and-step grid with the part under each pixel); `owner_pack.py --envelope`; doll_gallery's growth strip labels
  T2 with its head pixels and budget (`E2 H160/229`).

### DOLL-C, wave 2 round 1, first block (2026-10-28; exes w0 at e25ea41, w1 with the production hooks)

- **Exes**: `%TEMP%\claude\emb_P2c\bin\rpg_test_w0.exe` / `doll_gallery_w0.exe` (e25ea41 exactly) and `rpg_test_w1.exe` /
  `doll_gallery_w1.exe` (this block); LATEST.txt names the newest.
- **`rpg_test --doll-envelope --scope ladder [--seeds a..b] [--report] [--selftest] [--pool P]`**: C12f over source L,
  gear::ladderKit for the 12 peoples x man and woman x seeds (`kit=<people>:<t>:<s>,palette=look` on A3's hero / heroine, the
  planner's 2026-10-28 measure, reproduced exactly at w0), each line against its own T1 with C12f's measure (noHeld, walk D
  frame 0 + idle R): the area ratios and the 0.05 step, the Mythic (`kit=<people>:6m:<s>`) at its T6 or more, the head 5 %
  over the tier below for T2-T3 (= T2's head <= T3's / 1.05) to T5-T6, consecutive heads 0.10 head OD apart. Prints each
  missing line (all lines with --report), the per-people table (median ratio [lines at the ratio], the Mythic, the head
  steps, lines ok) and the counts the planner's baseline uses. A failing term under a ruling of
  `art::doll::kEnvelopeRulings` (doll_envelope.h: people, body, the term's prefix, the PLAN reference; none recorded yet) is
  printed as ruled, never as a pass. Selftest: 8 synthetic lines (C12f source L selftests).
- **`--doll-envelope --scope all [--seeds a..b]`**: the caps (shoulders, head rows / ornament rows / shell, brim and flare
  columns, the cloak's side facing D and back facing R / L, T1 inside the bare outline + 1) on every look of C1's sources:
  the census (test_doll.cpp `dollCensusLooks`: A world and residents, B cultures x roles x sexes x ages, D the creator,
  E recruited residents, G the creator in worn gear, K roles, T towns; rig-2-covered looks only), F (the gates' matrix with
  the forge looks) and L (source L with the envelope looks). The skirt's flare is measured (hang parts past the bare body's
  row facing D) and reported per tier, not gated: kEnvelope's skirtFlare is "extra px past the hips" over a base the
  contracts do not fix yet (the planner names it).
- **`rpg_test --doll-ladder --finish --scope ladder [--seeds a..b] [--peoples all] [--selftest]`**: C12 per kit line (its six
  tiers read as lt1..lt6), C13 (the living finish) and C14 on every look of source L (the sword; `--peoples all` every
  weapon family).
- **tools/doll/arms_looks.txt** (written by tools/doll/arms_looks.py, embedded by embed_looks.py as test_doll_arms.cpp
  `kArmsLooks`): M8 192 looks, M9 360, M10 128, M11 62, M12 60 (802). `rpg_test --doll-arms --matrix arms` (or `--looks
  tools/doll/arms_looks.txt`) runs C3-C6 on it; `--doll --gates` keeps the gates' matrix (ladder + forge looks).
- **doll_gallery board `envelope:people:<people>|all[:grey|:black]`**: man and woman x seeds 1..3 a row, T1-T6 and the
  Mythic on one baseline (idle D, idle R, walk R) at 1x, saved 3x and 8x; the label the area / T1, the sub-label the helm
  pixels and the step over the tier below (T2: its pixels / T3's budget).
- **Resolver (D69, the data levers)**: `envelopeLifts(L)` (a tiered fighter's armour at T2-T3). T2: a body whose dial gives
  no shoulders wears `pauldron capelet.2` (listed in envelopeMissing while undrawn; a culture alloy's mass NONE keeps none).
  T3: a small dial (2) is lifted to medium (3); a dial that gives none wears the people's cape at TIER_CLOAK 3. The D57 watch
  keeps its kit. Not taken (tried, measured, reverted): T4 large shoulders (B8 knight fall orphan, C16m dune SUNSTEEL motif
  hidden; T4 at the ratio 21 -> 25 only) and the cape in place of the T3 travel cloak (C12e lt3 vs imp3 outline 7 %: the
  travel cloak is what sets Imperial apart; its `when cloak.t3` view inside T3's caps is ARMOUR-X's lever).
- **Resolver (contracts 3's vocabulary)**: `DollRecipe::vocabMissing` (doll.h ADD). Every sheath, back item, library weapon and
  effect is asked for; one the pool does not draw yet wears its fallback (sheath, back item, effect: none; a fighter's
  greatsword: the sword of its form) and is listed (`--doll-coverage --scope envelope`). HumanLook::backItem is resolved:
  bit 1 the quiver and `bowback` on one who does not wield the bow, bit 2 `staffback` on one who does not wield the staff.

### DOLL-C, wave 2 round 2, fix round (2026-10-28; the judge's w2 r1 items)
- **The child keeps no ladder dress** (doll_resolve.cpp): the T1 floor's padded jerkin never applies to a child (age 1), and a
  finish tile (finish.wear ... finish.glyph) is asked for a child only where the pool draws it for the child body. A look
  whose floor bits or T1 tiers outlived an age change (slicelook's child) wears its tunic: "armour=padded (child)" and
  "overlay=finish.wear (child): no part" cannot happen.
- **The youth's Dance, Cheer and Pray** (clips/kin_youth.clip, ADD): the sim gives a youth Dance (hopscotch, life_game
  playTag; the festival night), Cheer (the festival toast from SitDrink) and Pray (posesOf); none had a youth clip, so the
  view drew the youth from a v1 sheet at rig 2 (the judge's `expect menu v1 0` failures). Dance: the turning step, a hop on
  the beat; Cheer: the mug on wave2; Pray: the hands met on `hold`, the head bowed a row.
- **C1 counts postures drawn on v1** (`--doll-coverage`, test_doll.cpp): every (body, posture) the census gives a look that
  the doll does not cover is printed ("posture on v1 in N looks: <body> posture <n> (<first missing>)") and fails `--gates`.
  posesOf gives a youth SitDrink, SitFloorDrink, Drink and Cheer too.
- **B7's seen counts on their own scope** (`--doll-variety`): the 18-of-24 hair styles, 9 beards, head types and garments
  are counted over seeds 1..20 plus the seeds asked (VISION_PLAN 15.36 B7 defines them on 1..20); the per-settlement
  head-recipe checks run on the seeds asked. The planner confirms or rules otherwise (PLAN).
- **C11 in `--doll-variety`**: every settlement's watch within 160 tiles (12 men as spawnHuman makes them) and each kingdom's
  squad of 8 soldiers: no head recipe (type, nose, brows, beard, hair colour) on more than 2.
- **B11 wear's COVER figures** (test_doll_wear.cpp): every armour form plain on both bodies (cover_form<n>_<body>), each
  carried weapon's sheath or back item (cover_wpn<n>_<body>, cover_back_<body>) and M8's build-0 T4 looks (arms_m8_*), so a
  part a production area adds is judged on its side frames the day it lands.
- **p2c_ladder's shields** are the forge styles' (signed off 2026-10-24, unchanged at e25ea41): form7 at T1-T2, form4 at T3,
  form3 at T4 and T6, form2 at T5.

### DOLL-C, wave 2 round 3 (fix round), 2026-10-28
- **C3b by tier at every tier** (test_doll_arms.cpp helmCheck): T1-T6 read kEnvelope from the head anchor (rows / ornRows
  over the crown; columns past the 16 px head box: side, a part named `brim` max(side, brim), horns / wings / flares /
  brims / tails and crest parts `flare`); untiered (TIER_HELM 0) keeps today's F-35 / F-37 and head box + 1. Selftests add
  a T4 shell 2 columns out (caught), 1 column out (clean), a T2 shell 1 column out (caught), a T3 shell 5 rows up (caught;
  clean at T4), a T4 plume 7 rows up (caught; 6 clean), a T3 horn 3 columns out (clean).
- **Coverage exit codes**: `--doll-coverage --scope forge` and `--scope ladder`, and `--doll-variety`, return 1 whenever
  they print FAIL (before: only with --gates).
- **sword_shield.clip** (p2c_clips.py): the woman's drawn walk and run facing R and L hold the sword through the library
  angle view (`weapon=45,front` / `135,back`) instead of `held=gmove`: the same 45-degree guard with the hilt forward of her
  face (the T4-T6 `gmove` tier views put the rear quillon on her chin: W7). The man's lines are unchanged. The generator
  does not reproduce the integrated block lines (sw_d0 / a3d0 ... with fx=spark) or spear_carry.clip's sp_carry idle:
  those were edited in the generated files by their areas; the file in the tree is the integrated one plus this change.
- **What-if knob** (doll_envelope.h envelopeWhatIf, read by ladderLook; 0 everywhere but `rpg_test --doll-envelope --scope
  ladder --what-if t4large,t3nocape,t4cloak,t2cap`): numbers for the planner's C12f ruling, never a gate result.

### DOLL-C, wave 2 round 4 (fix round), 2026-10-28
- **The freeze's shape cells are pinned** (root cause of slice_g_cov5w / forge_g_cov5w, 72 frames each: every integration
  re-solves the shape table, and nothing held the cells the signed-off looks read; int2's re-solve moved JADE school 7 rank
  0 from 02020105 to 02040105, so g_cov5w's shoulders went cop.4 to shard.4). `rpg_test --forge-frozen MANIFEST --from-inc
  INC` expands every look the freeze manifest holds with gear::tableKey recorded (gear::setTableKeyRecorder, ADD) and writes
  tools/doll/shape_frozen.txt ("<people> <school> <rank> 0x<key>": 37 cells at e25ea41's keys). `--forge-solve` reads it
  by default (`--frozen FILE`, `--no-frozen`) into gear::ShapeModel::frozen (ADD): a cell all of whose ranks are frozen keeps
  its keys whole and is never re-solved; a partly frozen cell solves its other ranks around the frozen key. The frozen pins
  outrank the owner's-37 pins and the hero-cell pin. `--forge-shapes --check` adds rule 7 (every frozen cell holds its key):
  the int3 table fails it (1: jade school 7 rank 0), the r4 table passes. The integrator's `--forge-solve --keep` now writes
  the frozen cells back; r4's solve on pool 4BEAC6C091674704: 0 hard, C18a 0 twins, 2 cells moved against int3's table.
  A new freeze file needs `--forge-frozen` run against it (the old cells stay pinned until then).
- **D69 is a people's line's** (doll_resolve envelopeLifts): the Guard uniform (the watch and the soldiers, a rank's issued
  kit) is no people's line, so the T2 short mantle and the T3 plates/cape never reach it (slice_guard's 136 frames went
  back to the signed-off figure: pauldron none, area 770, not capelet.2 at 818). Source L's 72 kit lines never wear Guard.
- Still changed against freeze_e25ea41 by code (e25 pool, r4 exe): slice_fjord3 136 (D69's T3 plates: flared.2 to
  flared.3, area 1026 to 1044; a people's kit line, kit=fjordfolk:3:5) and slice_nking 124 (FOLK-SIM-C's rankSlots: the
  king's over-layer forced to the cape at T5, halfcape to noble cloak). Both are intended changes to signed-off figures:
  the owner's sign-off and a new freeze, or the change taken back.

### DOLL, the wave-2 fix round, r1 (2026-10-29; PLAN D72-D89; exe w2f1 = rpg_test_w2f1.exe, doll_gallery_w2f1.exe, BDIR build-proto)
- **kEnvelope** (doll_envelope.h): row 0 cloakSideD 2; T4 cloakBack 4. Nothing else moved.
- **The cloak ladder** (D73; doll_resolve ladderLook, gear_ladder): an alloy's over-layer clears only the cape dial
  (E.cape); a worn cloak item stays behind TABARD, SASH, SCARF and NONE; CAPE = the noble cloak (cloak 3) in the people's
  colour darkened; HALFCAPE = `cloak halfcape` alone (one Cloak-slot piece); either takes TIER_CLOAK = the body's tier at
  least. T3 on a people's line: plates (size 2 -> 3) AND the cape where no cloak item is worn (an alloy's layer stands in).
  ladderKit: the T3 cape on every people's line (dial 0 included), and the kit's cloak at TIER_CLOAK = the kit tier from T3
  (the T4 travel cloak reads `cloak.t4`). `DollRecipe::kitCloak` marks a culture alloy whose worn cloak D73 kept.
- **T2 / T4** (D74, D75): `t2Line` (a people's line at TIER_BODY 2) wears `neck cowl` in the scarf's place; flags
  `people-<name>` (HumanLook::makerPeople, ADD: the body's maker people, gear::fillLadder) and `hem-plaid` (Highland) key the
  cowl's people variants; a cowl with no view for the look's people is listed ("neck cowl people-<p> ... -> the base cowl").
  T4: size 4 for every mass but NONE (the alloy mass lift and `peoplesLine` dials 2-3).
- **ComposeOpts::noKitCloak** (D77): drops the Cloak parts of a `kitCloak` recipe and restores D41's lift as it was
  (`pauldronTopAlloy`); `outlineOf(r, true)` in C16o, C18, --forge-solve (test_doll_shapes) and C16k (test_doll_envelope),
  and the gallery's alloy outlines.
- **The trim band** (D78; doll_compose trimBand): a tile part tagged `band` (`tags tile band`, `on-region R`, role
  `overlay finish.trim`) is asked by every look with a T4-T6 piece where drawn. On each such piece's views every R run is
  widened inward (4-connected distance from the view's outline) from its innermost pixels to W = 1 (T4) / 2 (T5-T6),
  never into G, N, a step-0 pixel or a clear cell; existing R pixels keep theirs (only adds). Painted from the tile view
  whose condition names the piece's tier (`<slot>.tN` or `tierN`, N == tier, else the nearest lower, else the base rest
  view): row = depth from the outer edge (0 outer); a tile with 2 x W rows gives the bottom- and right-facing runs the lower
  rows (the top-left lit); columns run along x + y on the part's grid. Cell semantics as region tiles (ink moves the step,
  Glow writes it). C12b counts R pixels in Metal 2 from T3; T1-T2 reported, not gated.
- **Compose guard** (D82): pixels a gripL `Role::Shield` part wrote survive every other part's `-` cells.
- **Helm by the resolved tier** (D80): the C12f caps (`--scope all`), the envelope lines and C3b read
  `DollRecipe::pieceTier` (after kitVar, the watch, D69 / D73), never the look's field; cloth headwear (headwear/cloth/**)
  out of the column caps; T1 measured without its cloak against the same look with every gear part off (t1Figure).
- **The climb's rulings** (D76): `rulingFor` honours only a ratio term ("T<n> area <q> of T1 (<aim>)") 0.05 short or less;
  steps, heads and the Mythic never (4 new selftests). `--doll-envelope --scope ladder --ledger`: under each line, per tier
  the area, the ratio, the step and each piece's add (helm, pauldrons, cloak, neck, skirt, layer, torso: the frame without the
  piece against with it), and T2's head budget. doll_gallery `envelope:people:<p>` prints C / S / H / N adds under each figure.
- **The relics and the Mythic** (D83-D85): PARADE wears `helm fam.onion`, STARFALL `helm fam.crystal`, a Mythic relic helm
  (HumanLook::relicHelm, ADD: gear::fillLadder from a Mythic steel helm of band 4-7) its metal's family; at T5-T6 only once
  that family draws a `helm.t<N>` view that does not ask `alloyshape` (else today's helm, listed "helm fam.<f> T<N> view
  (D83) -> today's helm"). metalKindOf: a Mythic alloy keeps 10 (MYTHIC = 11 for bronze, iron, steel and relics only);
  `mythicAlloy` pieces take R and N in Glow, `overlay collar.ember` in the collar's place and `overlay crest.flame` in the
  crest's (listed when not drawn). C19 A5's warm share and helm luma range read `forge-mythic` looks only. NEW gate line
  `relic invariant` in `--doll-ladder`: hero and heroine at T5 and T6 (kit=heartland, mat=gilded / obsidian / ember /
  starmetal) front walk px strictly climbing, and crystal.N the largest base-view mass of each size N (its `when
  metal-starmetal replace` view where drawn).
- **C3** (D81, D82): the probe names a held part by its library value wherever its map lists it (the spear is checked; A3's
  held/weapons/sword stays out, contracts 2's exception); the probe falls back to handR / handL for the grip as the
  compositor places it; C3d asks the off fist only on DRAWN and ACTION frames; NEW: no weapon pixel more than 2 rows below
  the sole row on an upright frame; a hidden gripping hand (facing U, or the far arm) still fails, tagged "[hidden hand ...;
  ruling proposed]". C3c facing U: attached when the shield touches the cloak part that covers the grip.
- **C1** (D79): `--doll-coverage --scope all` adds sources F, L (seeds 1..3 + the envelope looks) and M8-M12; `reserve`
  parts (map tag `reserve`, motif plain / straps, pauldron sizes no source asks) are listed, never dead; the strike maps art_fx
  draws (`fx heavy chop smash stab swish punch`) are listed apart (contracts 4); a child carries (emigration: Carry v0).
  maps/held/props: WEAPONS' spoon_child.pxm and bread_child.pxm, unchanged.
- **Ramps** (D88; doll_ramps rampsOf, FIX_COLOUR): partSafe moves (cloth1 off skin, leather off hair and cloth1, accent
  off leather, cloak off leather) that never close a pair already 18 apart; T1-T2 leather and shirt climb together to a
  step-1 of 80 where no pair closes; a culture alloy's step 1 re-lifted to 80 after the round-2 read inside the caps (45 %
  of step 3, 12 under step 2, 61 under step 3), its glint toward 245 where it held step 3 down; every cloak's step 3 at 86
  and step 1 at 82 where step 2 allows. C14: GLASS alloys held to their own step 1.
- **Lines** (D86, D89): the bandit chief `gear::retierLook` T2 (T3 where the place's danger band is 4+) in game.cpp (plus
  `#include "rpg/sim/gear_look.h"`); census::rankSlots: a T5 king below the masterwork (alloyRank < 3) keeps his alloy's
  layer and his cloak's tier is cleared, which the resolver reads as "no full cape" (L.cloak, v1's, untouched); the forced
  LY_CAPE is gone. lookTier: a culture alloy T4 or more, a masterwork T5 only from band 4. `wpn=` takes sword axe mace
  dagger greatsword spear bow crossbow staff unarmed (crossbow: the bow item in BowKind::Crossbow form; unarmed: nothing in
  hand, no shield); any other value is a bad-look error naming it. The actions board prints "DRAW, LOOSE" for bow looks.
- **The freeze, reported** (D72): `--doll-wear --freeze-check F --report [--freeze-old-pool P] [--pool Q]` prints per look
  "freeze cause <tag> N frames; rule D73 ... n; maps / clips since the manifest's pool n; wave 2's code before this round n"
  and the look's maps whose drawing changed. The rules are `doll::fixRules()` bits (doll_envelope.h FixRule: FIX_CLOAK,
  FIX_T2, FIX_T4, FIX_TRIM, FIX_SHIELD, FIX_RELIC, FIX_LINES, FIX_COLOUR), all on everywhere but this report.

### DOLL, the wave-2 fix round, r2 (2026-10-29; the judge's fix-round-1 items; BDIR build-proto, embedded pool = fr1)

- **noKitCloak, restated (D77; item 2, the shape table).** `DollRecipe::layerCloakT3`: a culture alloy whose over-layer IS its
  cloak (CAPE, HALFCAPE) composes, under `ComposeOpts::noKitCloak`, with the `cloak.t4`..`cloak.t6` flags off: its cloak at its
  T3 view, the footprint the table was solved on. The tier ladder's T4-T6 cloak views (D73) are tier adds every line shares, so
  ARMOUR's officer's cloak (`when cloak.t4,!cloak.t5`) no longer moves C16d / C16o / C18 / `--forge-solve`. Material styles keep
  their worn cloak in the measure (leaving it out measured 199 rule failures: the table was solved with it). The table was
  re-solved warm (`--forge-solve --keep`, frozen cells pinned): rpg/sim/gear_forge_shapes.inc, 63 lines changed.
- **The Mythic by construction (D84).** `HumanLook::relicBody` (derived by gear::fillLadder, never saved; key() at rig 2 when
  set): a Mythic relic body keeps its metal's outline under MYTHIC's finish (STARFALL's split skirt, FURNACE's heavy arms). A
  Mythic culture-alloy helm keeps its own crest with the flame crown seated over it.
- **C14 (D88).** The parting adds the metal off the hair (`partSafe(Metal, {Hair})`): steps 2-4 move, step 1 (the lit edge) is
  kept while 8 under step 2; the glint capped at 250 luma and kept 4 over the lit plane; C17's plate read and dE terms held.
- **C12f T1 column (D80).** `t1Outside` leaves out `headwear/cloth/**` (as the head columns do) and a helm whose resolved tier is
  2 or more (it has its own head caps). Selftest: a T4 body measured as T1 with its helm left out still fails (C12f 11/11).
- **Diagnostics.** `EMB_FIX_RULES=<mask>` starts `doll::fixRules()` with that mask (unset everywhere else).

### DOLL, the wave-2 fix round, r3 (2026-10-29; the judge's fix-round-2 items; BDIR build-proto, embedded pool = the tree's r2 gen)
- **Compose, the shield under an erased overpaint (D82; SHIELDS' r2 proposal, adopted).** The gripL shield's last pixel under
  each canvas cell is kept (code, region, item). A part that paints over the shield and is then cleared by a later part's `-`
  cell shows the shield again, never a hole through it. With `FIX_SHIELD` off, nothing changes.
- **C1 reserve (D79 "by design").** `forge/cut/splinted`, `forge/edge/fringed`, `forge/edge/spiked` and `forge/fit/knot` are listed
  `reserve`. Forge round 2 retired the cut, edge, limb and fittings of every culture alloy (3c.1, gear_forge.cpp alloyStyle), and no
  hero style's column names these values. `forge/cut/brick`, `forge/cut/diamond` and `back/staff` can be reached in a world, so
  source F reaches them: tools/doll/forge_looks.txt g_cov6 (FOUNDRY on a padded body), g_cov7 (PARADE on a padded body) and
  g_cov8 (a T1 sword fighter with his staff on his back). These looks are embedded and gated like every F look.
- **Ramps (D88), after the parting pass.** A pair the first mover cannot part tries its other side (leather off the accent, the
  shirt off the leather, the leather off the cloak). Where no whole-ramp move parts the metal from the hair, step 2 moves alone,
  between step 1 + 12 and step 3 - 30. On a T1-T2 figure, the leather parts with step 1 at 80 luma or more where such a move exists
  (a finer 0.005 search), else as before. The Heartland's own kit at T1-T3 is left alone (D72). The second lift (mk 10) now raises
  the lit plane when the dark-core rule caps step 1, and the body when step 2 caps it (the first lift's rules: 15 under the glint,
  18 off the skin and hair, 22 under the lit plane, round-2 read kept). The Mythic's black steel takes its step 1 up to 82 luma,
  8 under step 2 at most.
- **Diagnostics (tests only).** `--doll-recipe T --regions`: idle D and R cropped, with the R/P/G/N/W regions and each part's R
  and trim count. `--doll-recipe T --orphans`: the lone pixels in every DEATH frame and the part that drew them.
  `ENV_DEBUG_TAG=<tag>` (`--doll-envelope --scope all`): one look's parts, its T1 pixels outside the outline, and its part
  extents from the head anchor. `B8_DEBUG=1`: where each orphan is.

### DOLL, the wave-2 fix round 4, r1 (2026-10-29; PLAN D90-D98; exe w2f4 = rpg_test_w2f4.exe, doll_gallery_w2f4.exe, BDIR build-proto, embedded pool = fr3's gen)
- **HumanLook ADD (D96).** `HumanLook::king` (1: Role::King; census::rankSlots sets it; derived, never saved; key() at rig 2 only
  when set). The looks macros read and print `king=`. Resolver flags ADD: `herokit` (HumanLook::forgeKit: the Heartland kit's road,
  the hero lines among it) and `king`.
- **The king (D96).** census::rankSlots: a T5 king's cloak tier is his body's (the long cloak, `cloak noble`, his tier's views) and his
  alloy's over-layer is NONE (HumanLook::forgeShape bits 11-13; every other bit kept): one Cloak-slot piece, no shoulder drape. The
  resolver dyes his cape his cloak colour (else his tabard's) darkened to luma 91 where lighter; doll_ramps passes it by D88's lift
  and pins the Heartland king's (forgeKit or maker Heartland) five steps to e25ea41's: #220813 #631B27 #A53C3C #ED7A5D #FFB684 (read
  from rpg_test_w0's nking frames). C14 reads a king's lit edge against his cape's own step 3. `--doll-ladder` prints and gates
  "D96 king cape" on nking: columns left|right of the axis facing D and U within 1, steps 2-3 within dE 6 of e25ea41's, 0 Cloak
  pixels at step 4, 0 lone Cloak pixels within 2 px of a boot, fall + lying Cloak pixels 90 % or more of e25ea41's 2,406.
- **One kit, one look (D97).** gear::ladderKit makes the kit's shield item whenever its people bears one (every `wpn=`; the Rng draws
  stay in step) and carries it (`hasShield`) only with a one-handed weapon (`LadderKit::shieldMade`). The kit macros and
  alloyKitLook take the trim from it when it is not carried (gear::shieldTrimOf, wearGear's own rule; wearGear unchanged).
  `--doll-envelope` adds "D97 one kit one look": 12 peoples x man and woman x T1-T6 x seeds 1..3, the ten weapons equal in every
  field but the hands' (432 lines; 408 differed before).
- **C3d's hidden-grip proof (D90).** Facing U, the far arm and turned frames: a gripping arm with no H pixel passes only on h1 (the grip
  on an opaque non-weapon figure pixel; an unowned outline pixel counts as the body's where the weaponless frame has it) and h2
  (every 8-connected group of weapon pixels touches a non-weapon figure pixel); the rest of C3d then applies (the tip mark, the off
  fist on the haft line). Messages name h1 / h2; "[ruling proposed]" is gone. `--report` prints the C3d residual grouped by arm
  part, key, facing and failing term, with its looks and up to 6 frames. Selftests: 8 new (C3 ALL CAUGHT).
- **C12f T1 and the head caps (D91).** t1Figure's bare body keeps `shield/arm` (the limb; its outline counts); the head caps skip
  headwear/cloth/** and kin/*/headwear/** on untiered looks (rows and columns), never headwear/helm/**. `--doll-envelope --scope
  all` prints "T1 residual (D91)" by collar (with herokit marked) and, with `--report`, every look: tag, collar, body, build, hair
  and the pixels outside. Selftests: a shield bearer's bare body has its shield arm; a collar 2 px past it fails; a row-0 helm 6
  rows over the crown fails; a helm is never cloth (C12f 15/15).
- **The relic invariant on the body (D92).** `--doll-ladder` measures the front walk's frame 0 without held, shield, sheath and fx
  parts, counting the pixels no head-anchored part draws (head, face, ears, hair, beard, helm, crest, plume, headwear; an unowned
  outline pixel beside a counted one). Each line's four numbers and steps are always printed, with the whole figure's for reference.
- **The trim band by rule (D93; FIX_TRIM4).** resolve ends with tuneTrim: per armour slot (body, helm, shield, gloves, boots, cloak;
  never the weapon or the back), C12b's own measure on the idle D and R (trimShares). A slot outside its band walks a list of
  levels until both facings sit inside it (floor + 2 points under, ceiling - 2 over), else keeps the nearest: DollRecipe::trimLevel
  (the floor: the band continues along the hem, the neck opening, the arm openings, a pauldron's lower rim; each first at 1 px,
  then at the tier's width; runs of 2 px or more; never into G, N, drawn R, step 0 or clear; helms are left to their maps) and
  trimCeil (T4-T6: the band's added rows 2 px to 1, then off; drawn R kept. T3: the shortest drawn R runs, 2 px up to every run,
  laid as drawn material). A view with no region layer takes the floor's edges. The levels are cached by everything composed
  but the ramps; they ride on the recipe, so every frame of a look reads the same rule. Selftests: a T6 plate with no R lands in
  10-24 %, a T3 piece with every edge R lands under 14 %, a gem and a seam are never touched. `EMB_TRIM_DEBUG=1` prints the walk.
- **C14 (fix round 4).** The metal's step-2-only move off the hair searches in 0.5 % steps (FIX_C14). `--doll-recipe` prints the
  cloak, metal, metal 2, leather, accent, hair, skin and both cloth ramps; `EMB_C14_DEBUG=1` prints an unparted metal-hair pair.
- **Freeze attribution.** FixRule bits ADD: FIX_KIT 256 (D97), FIX_KING 512 (D96), FIX_TRIM4 1024 (D93), FIX_C14 2048; FIX_ALL 4095.
