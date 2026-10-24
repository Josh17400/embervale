# P2C_CONTRACTS: the shapes the P2c areas share (planner, 2026-10-23)

The plan is docs/PLAN.md "## P2c"; the art direction is docs/STYLE_P2.md 25-27. The doll's format stays rpg/art/doll/doll.h
(FROZEN: areas ADD names, enum values at the end before COUNT, and functions; never rename or change a meaning). Where
this file says "P2c restates", that change is made once, by the code owner (SLICE-DOLL in wave 1, DOLL-C in wave 2),
because no map depends on the old meaning. SLICE-DOLL / DOLL-C append "as built" notes at the end; nothing above is edited.
**The ladder re-plan (2026-10-23)** adds the ladder slice's ownership (1), its freezes (2), vocabulary (3), the ladder's
data (3b), the living finish (6b) and test hooks (7); LADDER-DOLL is the code owner in wave 1b.

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

**The ladder (2026-10-23)** keeps all of this and adds: wearGear's, gearIcon's, census::dress's and makeLook's existing
outputs are frozen until the flip (the ladder writes only its new fields; art_hash, village-art and `--arms` prove it);
HumanLook only gains fields that v1 ignores and key() mixes at rig 2 only. ADD-only exceptions: maps/cloak (`breeze` keys,
`when` views), maps/beard (`when capped` views facing R / L). Restated: A3's arc is re-cut (the owner), and B1 compares
the figure only (the doll composes without fx; A3's Fx colours are left out of the reference).

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
