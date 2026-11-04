# Fix batch P5: every people's fingerprint on its armour (worn and in the bag)

Planner, 2026-11-03. Base **80215d7** (main == solid; P4 and its fix round shipped; baseline root CURRENT **p4f1**), branch
`fix-p5-int` in the integration worktree. Read with `README.md` (the code map), `docs/ARCHITECTURE.md`, `docs/SOLID_PLAN.md`
section 5 (rules for new code), `docs/STYLE_P2.md` 27.3 (the finish), 28.5 (the peoples' vocabularies) and 29 (the
envelope), `docs/PLAN.md` "The culture generator feeds every tier" (the people table), D116-D117 and the owner rulings of
2026-11-03, `docs/P2C_CONTRACTS.md` 3b-3d, and `docs/FIX_BATCH_P4.md` (the format this batch follows).

**The owner (2026-11-03, after D117):** "I would like every culture's finger print to be on the armor whether that's
color or style, something that makes it unique to the culture." P4 made the worn leather gambeson read as leather (D116,
"it looks good") and every body-armour icon read by its material (D117). That took the maker people's cloth off both: every
people's gambeson is now the same brown, and a people's bronze, brigandine and leather icons are near twins of another's.

**In one paragraph.** Every armour piece a people makes carries one signature of that people, the same on the doll and on
the icon, while the material still reads first: the people's **dye** (a thread and binding colour, with a second colour
for the peoples whose band is two-tone) on the piece's rims, hems, laces and seams, and the people's **mark** (its motif:
the Fjordfolk knot, the Highland plaid, the Heartland oak, the Imperial laurel, the Dune arabesque, the Steppe horses, the
Marsh reeds, the Jade clouds, the River waves, the Sun Temple rays, the Sylvan leaf veins, the Starspire stars) stitched on
the chest at T1-T2, engraved at T3-T4 and gilded at T5-T6, the same climb the ladder already makes from thread to gold.
Gloves and boots carry the dye on their cuff; cloaks on their hem. The fingerprint is data (one table, `rpg/core/fingerprint.h`)
that the doll's resolver and the icon painter both read, keyed by the piece's maker people, so worn and icon always agree
and a new piece inherits it: every armour map already draws the finish regions it lands on (R rims, N seams, P panels).
Nothing in an outline moves (the silhouette gates stay as they are), rig 1 and the classic maker-less pieces are pixel for
pixel what they were, and one switch (`EMB_FIX_RULES` without the new `FIX_FP` bit) draws the pre-P5 look from the same
build, so every baseline move can be proved to be the fingerprint's and the owner pack's "before" comes from the same exe.

**How it should feel.** A player who buys the start smith's 48-gold leather coat in a Dune town sees indigo and saffron
laces down its front and a little arabesque stitched on the chest; the same coat from a Highland smith is laced in tartan
green and red with a plaid square; both are plainly the same brown leather. In a Jade city the watch's harness is edged
in vermilion and black, the royal guard's lamellar is laced vermilion, the king's gilt is cloud scrolls. A companion in a
Fjordfolk mail shirt, a Steppe cap and Marsh boots shows three peoples at once, each piece its own, and their icons in the
bag say the same thing in 16 pixels. In greyscale the marks and the two-tone bands still tell the peoples apart.

Absolute rules for every area (from the batch brief; they bind the areas and the integrator):
1. No visible window: `embervale.exe` only with `--witness` (game scripts through `tools/gates/witness_pack.mjs`, Witness
   tests through `witness test`, Witness calls). A true non-harness run uses `SDL_VIDEODRIVER=offscreen`. rpg_test,
   art_hash, save_test, city_gallery, audio_preview and the `*_gallery` exes are CLI and fine. Never `tools/run_scripts.sh`.
2. Heavy processes only through `tools/slot.sh` (cap 2 while the owner plays a game, else 5: wait). Never kill an
   Embervale.exe you did not start; never touch `%APPDATA%\Josh17400\Embervale` or the Desktop. Scratch output under
   `C:\Users\joshu\AppData\Local\Temp\claude\C--Users-joshu\1979ea42-238a-4969-bda8-d76ab6bf2de5\scratchpad\p5\<area>`;
   delete PNGs when done (the owner pack, section 10, and the audit, `...\scratchpad\p5-audit`, are kept until the owner
   has seen them).
3. Throwaway clones and worktrees link `build/_deps/sdl3-src` to the main checkout by a junction: before deleting one run
   `cmd //c rmdir <clone>\build\_deps\sdl3-src`, then check `C:\Users\joshu\Documents\embervale\build\_deps\sdl3-src\CMakeLists.txt`
   still exists. Never `rm -rf` through a junction.
4. Behaviour changes only where your area owns them; every other gate stays identical. A baseline your change moves is
   declared in `tools/gates/baseline/moves_p5/<AREA>.txt` (section 8) with its reason; you never re-record a baseline
   yourself and never loosen a gate. No area changes the save format (looks are derived, never saved; if one finds it
   must: SAVE_VER bump, migration, fixture test, old saves load, and tell the integrator first). No area adds an RNG draw
   to the census or the loot rolls (the sim digest must stay identical).
5. Owner rules: the rags-to-riches ladder (T1 simple and not bulky, better tiers more detailed, late gear animated); every
   alloy and material its own armour shape and its own helmet (tested in greyscale); the material reads first (D116, D117);
   companions equip better gifts, "if they die they drop everything", kit cap hero level + 4, 240 s pile, rejoin keeps
   person and level; never delete drawn art or rig-1 art. **This batch is additive: no outline, mask, fit or build row of
   any drawn view changes**; marks are new tile views, re-cuts touch region planes only. Every art change goes into the
   owner pack as pending, never silently shipped as final.
6. NO compression; the doll pool stays under its budget (`2,621,440` B; P5's cap is section 3.5).
7. SOLID section 5: new code in its lane and layer, no new layer violation (`tools/gates/check_layers.sh`), no function over
   300 lines (new code under about 150), tables over switches, test-only code only in `rpg/view/scripts/` and `tools/tests/`.
8. Never end a turn while a build or gate still runs: run them in the foreground (chain chunks) and report when finished.

---

## 1. The items at a glance

Every item was checked at 80215d7 with the main checkout's build (`C:\Users\joshu\Documents\embervale\build`, built
2026-11-03 08:22 from 80215d7's tree) by a scratch audit tool linked against that build's libraries
(`...\scratchpad\p5-audit\p5_audit.cpp`, built by `build.bat` beside it; section 2), the code, and the sheets it wrote.

| id | item | real at 80215d7? | evidence (short) | area | owner pack |
|---|---|---|---|---|---|
| P5-1 | the same piece from two peoples is hard to tell apart | **yes** (594 of 2,736 worn pairs, 539 of 2,736 icon pairs are twins, seed 1) | worn: the T1 gambeson 66/78 pairs twins, the jerkin 48, leather gloves 78/78, leather boots 78/78, iron boots 46, the forced T3 forms (scale, lamellar, plate, leaf) 12-18; icons: gambeson and jerkin 46/78, gloves and boots 39-66, mail/scale/plate/leaf 25 (section 2.2) | CORE, DOLL, MARKS, ICONS | yes |
| P5-2 | the T5-T6 filigree wears the people's motif for 4 peoples of 12 | **yes** | `finish/filigree.pxm` draws 11 motifs (oak clouds laurel knot arab horse reed wave ray vein star; FINISH wave 2 r1: "DOLL-C maps the peoples"), but `doll_resolve.cpp` `kMotif[4] = {oak, clouds, laurel, knot}` and `gear_ladder.cpp` fillLadder sets `motif` 0-3 only: Dune, Steppe, Marsh, River, Sun Temple, Sylvan and Starspire wear the oak; Highland shares the Fjordfolk knot | DOLL (wiring), MARKS (Highland's own) | yes |
| P5-3 | census people carry no maker on the doll | **yes** | `HumanLook::makerPeople` and `motif` are set only by `gear::fillLadder` (worn items); `census::dress` (looks.cpp) never sets them, so the watch, soldiers, bandits and kings could not show a people's fingerprint (every king's filigree is the oak) | CORE | yes (guards) |
| P5-4 | icons: gloves, boots and cloaks carry almost nothing of their maker; the shared body forms are twins | **yes** | `art_items.cpp` itemIconLook paints gloves, boots and cloaks by the classic `paintIcon` (the metal ramp; a cloak by its tint): only a gilding people's gilt rim and a culture's iron / steel finish show the maker; body icons of one form and material differ only by that finish or the rim | ICONS | yes |
| P5-5 | the material must still read first | constraint | D116's worn leather coat (hue 20-40, sat >= 0.30), D117's `icons_gallery --check` (leather / bronze / iron / steel >= 30 apart, leather browns hue 15-45 sat >= 0.25); the simulation of this plan keeps them (section 2.5) | all | measured |
| P5-6 | no gate holds the fingerprint | **yes** | `--arms` checks only the steel-Rare and alloy-Legendary kits (>= 40 px apart); `icons_gallery --check` checks materials, not makers; nothing checks gloves, boots, T1-T2, worn-vs-icon agreement | GATES | - |

Seen in the audit, **not in this batch** (decision 7): a people's leather cap worn with a metal body draws in the body's
metal (P4's note under D116: the doll has one Metal slot a figure); a T1 bronze helm of the Heartland, Imperial and Sylvan
draws HOMESPUN's cloth hood in the wearer's shirt colour (no bronze at all; `worn_helm_seed1.png`, the bronze helm T1
column); the culture's own leather colour (`ArmsStyle::leather`) is unused by the doll and the icons (decision 6).

---

## 2. The audit at 80215d7

### 2.1 Method

- **Makers:** none (culture 0, the classic heartland look) and the 12 archetypes, each `gear::kitCulture(people, seed)`
  (the ladder kit's cultures: what the gates use), seeds 1, 2 and 3 (a culture's cloth, plume and alloys vary by seed).
- **Pieces** (36 columns): body: the leather gambeson (form padded) and jerkin (form leather) at T1, the people's own soft
  coat at T2 (Rare), the people's bronze coat and the bronze brigandine at T1, iron at T2, steel at T3 in the people's form
  and forced to mail, scale, lamellar, brigandine, plate and leaf, the first alloy at T4 and the masterwork at T5; helm:
  the leather cap T2, bronze T1, iron T2, steel T3, alloy T4; shield: leather T2, bronze T1, iron T2, steel T3, alloy T4;
  gloves: leather T2, iron T2, steel T3, alloy T4; boots: leather T1 and T2, iron and steel T3, alloy T4; cloak: T3 and T5.
  Each made by `gear::makeGearC` as the shops make it (`icons_gallery`'s recipe), forms forced by `Item::form`.
- **Worn:** the one piece through `wearGear` over A3's man and woman in their shirts (the game's hero path), idle frame 0,
  facings D and R, composed by `art::doll::probeFrame`; the piece's own pixels by the probe's owner and region (armour,
  pauldron, tabard, sash and cowl parts and the sleeves' armour slots for the body; helm parts; shield parts; the Hand and
  Flare regions for gloves; Boot, Sole and armoured Shin / Knee for boots; cloak parts).
- **Icons:** `gearIcon` -> `art::itemIconLook` at rig 2 (16 px).
- **Measures, per pair of makers and per view** (four worn views, one icon): `xor`, silhouette pixels in one and not the
  other (aligned on the pivot); `grey`, shared pixels whose luma differs by 12 or more; `distinct`, pixels whose colour lies
  dE 10 or more (CIE76) from every colour of the other's palette. A pair is a **twin** when all three stay within
  `max(2, area / 40)` px in every view, **colour-only** when shape and greyscale are twins but the palettes differ.
- **Sheets** (`...\scratchpad\p5-audit`, every one also `_grey`): `icons_matrix_seed{1,2,3}.png` (rows: makers; columns:
  pieces; red frame = twin, amber = colour-only), `worn_{body,helm,shield,gloves,boots,cloak}_seed{1,2,3}.png` (man D, man R,
  woman D, woman R per piece, 3x), `kits_T1-T6_man_woman_seed*.png` (every people's ladder kit), 
  `census_guard0-2_soldier_bandit_DR_seed*.png` (the watch ranks 0-2, a soldier and a bandit per people, D and R),
  `worn_*_sim_seed*.png` (section 2.5), and `report_seed*.txt` / `report_sim_seed*.txt` (every pair named).

### 2.2 Results

| piece | pairs | worn twins s1/s2/s3 | worn colour-only s1 | icon twins s1/s2/s3 | worn twins under this plan's rules, simulated |
|---|---|---|---|---|---|
| gambeson T1 | 78 | 66/66/66 | 0 | 46/39/46 | 0/0/0 |
| jerkin T1 | 78 | 48/56/48 | 0 | 46/39/46 | 0/0/0 |
| own soft coat T2 | 78 | 6/7/5 | 1 | 13/10/12 | 0/0/0 |
| bronze coat T1 | 78 | 8/7/6 | 0 | 11/10/11 | 0/0/0 |
| bronze brigandine T1 | 78 | 1/5/3 | 6 | 2/5/2 | 0/0/0 |
| iron coat T2 | 78 | 0/0/0 | 0 | 5/3/5 | 0/0/0 |
| steel coat T3 | 78 | 0/1/0 | 0 | 4/4/5 | 0/0/0 |
| mail T3 (forced) | 78 | 3/3/5 | 3 | 25/19/25 | 0/0/0 |
| scale T3 (forced) | 78 | 15/15/15 | 4 | 25/19/25 | 0/0/0 |
| lamellar T3 (forced) | 78 | 15/15/15 | 4 | 4/3/6 | 0/0/0 |
| brigandine T3 (forced) | 78 | 0/4/0 | 1 | 1/3/1 | 0/0/0 |
| plate T3 (forced) | 78 | 12/9/12 | 1 | 25/19/25 | 0/0/0 |
| leaf T3 (forced) | 78 | 18/18/18 | 5 | 25/19/25 | 0/0/0 |
| alloy T4 / masterwork T5 | 66 | 0 | 0 | 0 | 0 |
| leather cap T2 | 78 | 11/11/11 | 0 | 3/2/2 | 0/0/0 |
| bronze helm T1 | 78 | 7/7/7 | 0 | 1/2/1 | 0/0/1 |
| iron helm T2 | 78 | 2/2/2 | 0 | 1/2/1 | 0/0/0 |
| steel helm T3 / alloy helm T4 | 78 / 66 | 0 | 0 | 1/2/1 / 0 | 0 |
| shield T2 | 78 | 1/1/2 | 1 | 0 | 0/0/0 |
| bronze shield T1 | 78 | 9/8/12 | 0 | 0 | 0/0/0 |
| iron / steel shield | 78 | 1-2 | 0 | 0 | 0/0/0 |
| alloy shield T4 | 66 | 0 | 0 | 0 | 0 |
| leather gloves T2 | 78 | **78/78/78** | 0 | 39/34/39 | 0/0/0 |
| iron gloves T2 | 78 | 28/28/28 | 9 | 39/34/39 | 0/0/0 |
| steel gloves T3 | 78 | 30/30/30 | 13 | 39/34/39 | 0/0/0 |
| alloy gloves T4 | 66 | 5/2/3 | 4 | 0 | 0/0/0 |
| leather boots T1 / T2 | 78 | **78/78/78** | 0 | 66 / 39 | 0/0/0 |
| iron boots T3 | 78 | 46/46/46 | 0 | 39/34/39 | 0/0/0 |
| steel boots T3 | 78 | 20/20/20 | 11 | 39/34/39 | 0/0/0 |
| alloy boots T4 | 66 | 5/2/3 | 4 | 0 | 0/0/0 |
| cloak T3 | 78 | 2/2/1 | 10 | 0/3/1 | 0/0/0 |
| cloak T5 | 78 | 0/1/0 | 11 | 1/2/0 | 0/1/0 |
| **all** | 2,736 | **594 / 602 / 595** | 88 | **539 / 475 / 540** | **0 / 1 / 1** |

The whole kits (`kits_T1-T6_man_woman_seed1.png`) say it at a glance: T1 and T2 of all twelve peoples are the same tan
figure with a different shield; from T3 the helm and shield part them; from T4 the alloys do. The census sheet: the watch
of Highland, Marsh and the Sun Temple are three cream hoods over red; the bodies under the tabards are one.

### 2.3 What already differs (keep it)

- **Forms:** each people's helm (`ArmsStyle::helm[0..1]`), body (`body[0..1]`, alloys a finer cut) and shield; steel and
  alloy helms, every shield and every alloy piece are already distinct (0-2 twins above). Kept as they are.
- **Dials:** pauldron, skirt, crest, cape (`ArmsStyle` dials; poor pieces capped, T1-T2 envelopes). Kept.
- **Metals:** a culture's iron and steel take a third of its smiths' finish (`gear_look.cpp` metalBase); alloys their own
  families (`gear::alloyFamilies`, dE 20 apart within a people, 13 across) and forge guises: T4+ never twins. Kept.
- **Cloth and paint:** shields painted in the maker's cloth with its device; padded cloth coats and brigandine seams in the
  maker's cloth (D114, D117); cloaks in the maker's heraldry or cloth; the culture's plume colour on helms. Kept: a kingdom's
  own colours stay where they are; the people's dye is added beside them.
- **Ornaments where drawn:** studs (leather, mail, shields), fur trim (mail, pauldrons), tassels (lamellar hang, mail,
  capelet, kettle), scallops (lamellar hang), rivets (shields), plumes (great helm), gilding (the woman's plate); poor pieces
  keep fur, studs and tassels (`gear_look.cpp` ornamentOf). Kept unchanged (no new ornament rule: decision 6).
- **D74:** the T2 cowl and capelet hems by people (`people-<name>`, `hem-plaid`). Kept.
- **The T5-T6 filigree motif** for the Heartland, Jade, Imperial and Fjordfolk / Highland. Kept and widened (P5-2).

### 2.4 What does not

- **T1-T2 leather** (the first hours of every game: the gambeson, the jerkin, the cap, gloves and boots): nothing of the
  maker shows; D116 rightly took the maker's cloth off the leather coat and nothing replaced it.
- **Gloves and boots** at every material below the alloys: drawn by recolouring the arm and leg libraries' Hand and Boot
  regions; no finish region, no maker field reaches them.
- **The same form and material from two peoples** (the forced T3 forms, bronze coats, the T1 bronze helm and shield):
  only the metal finish's third differs, which is near nothing between grey finishes (Fjordfolk 150,150,156 vs Heartland
  176,178,186).
- **Census people:** no maker field at all (P5-3).
- **Icons:** gloves, boots and cloaks have no culture paint; body icons of one form share every pixel but the finish.

### 2.5 The plan's rules, simulated

The audit tool's `--sim` mode applies section 3.3's rules to the probe's regions with section 3.2's starting table (the
dye band on R at T1-T2, on N at T3-T5, on the gloves' and boots' cuff row at T1-T5 and the cloak's hem; a sparse dye mark
standing in for the motif tile on P at T1-T3). Worn twins fall from 594 / 602 / 595 to **0 / 1 / 1** (the last: a T5 noble
cloak whose gilt border and heraldic field match; the motif badge on the cloak, section 3.3, parts it). Measured on the
same frames: the dye covers at most 10 % of the gambeson and 9 % of the jerkin, whose mean colour over the whole piece
(outline included; man, facing D) stays leather: hue 26-35 and 22-31, sat >= 0.31 and 0.22 (the same measure before P5:
hue 30 and 27, sat 0.40 and 0.30; the cap 0.26 -> 0.23); 13 % of a soft T2 coat, 15 % of boots (a red cuff pulls a boot's
mean hue to 11: boots and gloves are held by their share, not their mean), 9 % of a cloak; but 23-26 % of a steel or scale
shirt (every N row taken), 31-32 % of a T1-T2 helm and 34 % of a T1-T2 shield (their rims are their edge): section 3.5
caps those, so the areas thin them (every other N row, the inner rim row only).

---

## 3. The fingerprint (the art direction; MARKS writes it into `docs/STYLE_P2.md` as section 30)

### 3.1 The feel

A people's fingerprint is **how its makers finish a piece**, not a new piece: the colour of the thread, the binding on the
edge, the lacing, the little mark a maker stitches or engraves. It sits on the edges and seams, so the material keeps the
plane (leather looks like leather, bronze like bronze) and the silhouette keeps the ladder (T1 simple and slim). It climbs
with the ladder: **thread** at T1-T2 (the dye on rims, hems, laces and seams, the mark stitched on the chest), **engraving**
at T3-T4 (the rims turn brass as the ladder already does; the dye moves into the lacing and seams; the mark is cut into
the metal), **gold** at T5-T6 (the mark becomes the people's filigree, the gem and the runes as today). Greyscale test:
the mark and a two-tone band read with the colour removed; the dye alone may not, and that is allowed (the owner: "color
or style"), but every people with a one-colour band has a mark that reads in grey.

### 3.2 The table (the starting values; MARKS owns them in `rpg/core/fingerprint.h` and may tune within section 3.5)

| people | motif (mark and filigree key) | dye | dye2 | band (`run` of dye, then dye2, `period`) | luma dye / dye2 | nearest other dye (dE) | nearest material step (dE) |
|---|---|---|---|---|---|---|---|
| none (culture 0) | (oak: the classic filigree) | - | - | no fingerprint (classic pieces unchanged) | - | - | - |
| Fjordfolk | knot (knotwork) | woad #3C6EBE | bleached wool #E8E2CE | 3:1 / 4 | 104 / 225 | Dune 25.0 | steel step 1 30.4 |
| Highland | **plaid** (new: the tartan sett) | tartan green #1E683A | red #AC2430 | 2:2 / 4 (a check) | 76 / 78 | Marsh 29.6 | leather 42.0 |
| Heartland | oak (oak leaves) | heraldic red #C42828 | white #EAE6D8 | 4:1 / 5 | 86 / 229 | Jade 32.4 | red-brown leather 27.4 |
| Imperial | laurel | purple #641E6E | gold thread #E2B840 | 2:1 / 3 | 60 / 182 | Dune 25.4 | leather 42.8 |
| Dune | arab (arabesque) | indigo #243484 | saffron #ECA61E | 2:2 / 4 | 56 / 171 | Fjordfolk 25.0 | steel 37.1 |
| Steppe | horse (running horses) | sky blue #3CACD6 | horse-hair red #C43028 | 2:1 / 3 | 143 / 91 | River 25.7 | steel step 2 24.4 |
| Marsh | reed (reeds) | moss #687E24 | faded indigo #465078 | 4:1 / 5 | 109 / 81 | Highland 29.6 | bronze 27.5 |
| Jade | clouds (cloud scrolls) | vermilion #F06E1E | black lacquer #1C1A1E | 2:1 / 3 | 139 / 27 | Heartland 32.4 | red-brown leather 28.7 |
| River | wave (waves) | river teal #12868C | white #ECEAE2 | 2:2 / 4 | 100 / 233 | Steppe 25.7 | iron 30.4 |
| Sun Temple | ray (sun rays) | magenta #B41E64 | teal #249688 | 2:2 / 4 | 82 / 114 | Imperial 38.2 | leather 41.1 |
| Sylvan | vein (leaf veins) | leaf green #A0C850 | amber #E8B040 | 4:1 / 5 | 174 / 179 | Marsh 29.8 | brass 33.4 |
| Starspire | star (four-point stars) | violet #9678F0 | star white #DCE4F8 | 3:1 / 4 | 142 / 227 | Fjordfolk 32.4 | steel 55.1 |

Chosen from each people's own palette (priors.cpp kSchemes, PLAN's people table, STYLE_P2 28.5) by a search that maximised
the closest pair: the twelve dyes lie dE 25.0 or more apart (the closest: Fjordfolk's woad and Dune's indigo, whose bands
part them: 3:1 white against 2:2 saffron), every two-tone signature 25.4 or more, every dye 24.4 or more from every step 1-3
of the six leathers, bronze, iron, steel, gilded and brass ramps, so a dye never reads as a material.

- **The fingerprint is per people, fixed across its kingdoms** (decision 1): a kingdom's own cloth, heraldry and plume
  stay on its shields, cloth coats, tabards, cloaks and crests, as today.
- **Isolated (crossed) families** take their first archetype's row; **the hidden peoples** beyond the Mist their
  archetype's row too (their strangeness is their metals, from T4) (decision 4).
- **Highland's own motif** is new (`plaid`: a tartan sett as a mark and a gilt lattice as filigree, STYLE_P2 28.5's
  "SASH (plaid)"), so no two peoples share a motif; Highland's T5-T6 filigree moves from the knotwork to it (decision 3).

### 3.3 Where it shows (the rules; DOLL implements, ICONS mirrors)

Every rule keys on the **piece's own maker** (one people per tier slot, `HumanLook::makers`, section 4.2) and the
**piece's own tier**, so a Jade helm on a Fjordfolk shirt shows Jade on the helm and Fjordfolk on the shirt.

| where | T1-T2 (thread) | T3-T4 (engraving) | T5-T6 (gold) |
|---|---|---|---|
| **R** (rims, hems, a collar's rim, a shield's rim, a tabard's edge, a cloak's border) | the dye band | the ladder's brass (unchanged); a cloak's border stays the dye band through T4 | the ladder's gold (unchanged) |
| **N** (seams, the lacing rows, a lamellar's or scale's lowest course, a brigandine's lower edge; the leather coat's laces) | the dye band | the dye band (thinned to the cap: every other row where a piece has many) | T5: the dye band; T6: the runes (the ladder's, unchanged); a glowing alloy's, FURNACE's and a Mythic alloy's N keep their Glow |
| **P** (the lit chest, the brow's panel, a shield's field panel) | the mark tile (`finish.mark`, FIT, the slot's motif): its thread cells in the dye on leather and cloth, as Ink relief (one step down) on metal | T3: the mark as Ink relief; T4: the mark as Ink relief **instead of** the generic etch where the piece has a maker | the filigree in the slot's people motif (all 12 wired, per slot) |
| **cuffs** (gloves: the top row of the Hand region, of the Flare on a gauntlet; boots: the top row of the boot's worn regions: Boot, plus Shin from boots band 2, Knee from band 3) | the dye band | the dye band | T5: the dye band; T6: none (the ladder's) |
| **a cloak with no R** (the fur mantle, the cape) | its hem: the lowest cloak pixel of each column, the dye band | the same through T4 | the badge on its back (the filigree fit, the slot's motif) |

- **The band** runs along canvas `x + y`: `run` pixels of the dye, then the dye2 to `period` (a solid band when dye2 is 0).
  Every dyed pixel keeps its drawn step (the shading carries), the outline ring (step 0) keeps its slot (as the ladder's
  finish already does).
- **The leather coat's laces** (D116's rawhide crossing, `when leather-coat` views) take the dye band through N cells MARKS
  adds to those views (decision 2); a maker-less coat keeps the rawhide.
- **Census people** wear their arms culture's fingerprint on every slot they wear (the watch, soldiers, captains, heralds,
  envoys, jarls and kings their owner kingdom's people; bandits, pirates and militia the local people's), so a town's watch
  shows whose they are.
- **Not drawn:** weapons (decision 5), the T1 floor (an empty body slot's padded jerkin and empty feet have no maker),
  classic pieces of culture 0, rig 1.

The icons (ICONS, `art_items.cpp`, rig 2 and `FIX_FP` only) say the same in 16 px: a body's collar and hem rows in the dye
band at T1-T2, its seam rows (the form's own: the lamellar laces, the brigandine's seam between courses, the scale and leaf
rows' shadow, the plate's fauld line, the jerkin's front seam, the gambeson's laces) at T3-T5; the people's motif as a 3x3
or 4x4 glyph on the chest (thread at T1-T2, Ink at T3-T4, gold at T5-T6); a helm's brow band; a shield's rim binding (T1-T2)
or inner ring (T3-T5); the gloves' cuff and the boots' top (two rows, the dye band); a cloak's hem and collar. The icon's dye
ramp is `art::doll::rampA3(dye, Material::Cloth)`: the doll's own generator, so the bag and the doll agree by construction.

### 3.4 Don'ts

- Never move an outline, a mask, a fit, a build row or a stretch row: the fingerprint lands only on finish regions, the cuff
  and hem rows and the mark tile's cells inside P. `--doll-forge --silhouettes` (`sil_w*`), the envelope's areas and the
  silhouette boards stay identical.
- Never cover the material's lit plane: no dye on the lit chest outside the mark's own cells, no dye run longer than the
  piece's R or N runs, no dye on step 4 glints.
- Never a checker or dither: a band is runs of 1-4 px along the edge; a mark is a shape (every thread pixel touches another),
  never a lone dot (STYLE_P2 27.3's rule for etching and filigree).
- Never dye a T1 piece beyond its rims, laces and one mark: T1 stays plain and used.
- Never change a kingdom's colours (tabard fields, shield fields, cloaks, plumes): the people's dye goes beside them.
- Never let a mark sit where the T5 gem or the T6 runes sit (they win: G and N at those tiers).

### 3.5 Numbers that must fit together

- **Dye separation** (F1, section 7): primaries pairwise >= dE 25 (the table: 25.0); two-tone signatures >= 25 (25.4);
  each primary >= dE 20 from every step 1-3 of the leather (six ramps), bronze, iron, steel, gilded and brass ramps (24.4);
  dye2 >= dE 20 from its own dye. A tuned table must keep all four.
- **Shares** (F4): the dye's pixels over a worn piece's pixels, the largest of the four idle views: body <= 15 % at T1-T2
  and <= 20 % at T3-T5 (simulated: 10-13 % and 16-26 %: thin the heavy ones), helm <= 25 % (simulated 31-32 %: thin), shield
  <= 30 % (34 %: thin), gloves <= 40 % (31 %), boots <= 20 % (15 %), cloak <= 15 % (9 %). Icons, of the icon's opaque
  pixels: a leather body or cap <= 10 % (D117's mean-colour check is the binding one: a 15 % sky-blue collar on a leather
  coat pulls its saturation to 0.24, under the check's 0.25), every other body, helm, shield and cloak <= 15 %, gloves and
  boots <= 20 %.
- **Material read** (F4): the pixels the fingerprint does not take are the pre-P5 pixels (F6); a worn leather body or cap
  keeps its mean hue (whole piece, man, facing D) in 15-45 and its sat >= its pre-P5 value minus 0.10 (pre-P5 on this
  measure: gambeson 0.40, jerkin 0.30, cap 0.26; simulated 0.31, 0.22, 0.23); boots and gloves by their share only;
  `icons_gallery --check`'s D117 lines unchanged and green (leather / bronze / iron / steel >= 30 apart, leather browns hue
  15-45 sat >= 0.25).
- **Distinctness** (F2, F3): no twin pair (section 2.1's measure) in any audit column for the 13 makers, seeds 1..3, worn
  (four views) and icons. Colour-only pairs are reported, and at T1-T3 at most a quarter of a column's pairs may be colour-only
  (the marks carry the greyscale read).
- **The pool:** 2,589,744 B today, budget 2,621,440 B (31,696 B free). P5 may add **12,288 B at most** (the mark tile
  about 3-6 KB: 12 motifs x about 8 fits x a few px; Highland's plaid filigree about 2-3 KB, like the arabesque's 24 views;
  N cells on views that already carry a region plane cost nothing; no view gains a new region plane). Ceiling: 2,602,032 B,
  so P6 keeps 19 KB.
- **Slots:** the doll's pixel code is `2 + slot x 5 + step` in a byte: 22 slots today (codes to 111). P5 adds 18 (`dye`,
  `dye2`, and one of each per tier slot): 40 slots, codes to 201 < 255.
- **Bake time:** B10's p95 0.385 ms (gates_parity, alone-rerun rule): P5 may cost at most 3 % (one region lookup above a
  pixel for the cuffs, one more FIT tile; the dye routing is the finish lookup that already runs).
- **The sim:** no RNG draw, no saved field, no Game member: S2, S3 and the digest identical.

---

## 4. Contracts (wave 1: three small commits, merged first; section 6)

### 4.1 `rpg/core/fingerprint.h` (new; MARKS owns it; core: every layer may include it, it includes only `<cstdint>`)

```cpp
// rpg/core/fingerprint.h (P5): the peoples' fingerprints on armour, one row per maker people (docs/FIX_BATCH_P5.md 3).
// Data only (MARKS tunes the values within FIX_BATCH_P5 3.5); the doll's resolver and the icon painter read it.
namespace fp {
constexpr int kPeoples = 13;   // 0 none (culture 0: no fingerprint), 1..12 cult::Archetype + 1
struct Row {
  const char* name;     // "fjordfolk" ... (the doll's people-<name>); "none"
  const char* motif;    // the mark's and the filigree's key prefix: "knot" "plaid" "oak" "laurel" "arab" "horse" ...
  uint32_t dye, dye2;   // 0xRRGGBB; dye2 0: a solid band
  uint8_t run, period;  // the band along canvas x + y: run px of dye, then dye2 up to period
};
inline constexpr Row kRows[kPeoples] = { /* section 3.2's table */ };
inline const Row& row(int people) { return kRows[people > 0 && people < kPeoples ? people : 0]; }
}  // namespace fp
```

### 4.2 The look and the icon carry the maker per piece (CORE owns `rpg/core/art_ids.h`, `rpg/art/art_items.h`)

- `HumanLook::makers` (`uint32_t`, 0 none): 4 bits a tier slot (`TIER_BODY` bits 0-3 ... `TIER_BACK` bits 28-31), each the
  maker people of the piece worn there (`cult::Archetype + 1`, 0 none). Helpers `int makerAt(int slot) const` and
  `void setMaker(int slot, int people)`. Derived, never saved; `key()` mixes it at rig 2 only and only when set (art_human.cpp,
  as `makerPeople`); `--look-key`'s list gains it. `makerPeople` and `motif` (D74, the filigree's legacy) stay as they are.
- `art::IconLook::people` (`uint8_t`, 0 none, 1..12) and `art::IconLook::tier` (`uint8_t`, the piece's look tier 1-6, 0
  none); `key()` mixes both. `gearIconKeys` sets them from the maker's archetype and `gear::lookTier(it, maker)`.

### 4.3 The doll (DOLL owns `rpg/art/doll/doll.h`, `doll_envelope.h`, `tools/doll/pxc.mjs`)

- **Slots** appended before `COUNT` (pxc.mjs SLOTS mirrors the list): `Dye`, `Dye2` (maps name them `dye` / `dye2`; the
  compositor moves a part's dye pixels to its tier slot's, as it moves a held part's metal to `Blade`), then `DyeT0..DyeT7`
  and `Dye2T0..Dye2T7` (never named in maps). `rampsOf` fills each tier slot's pair from `fp::row(makerAt(slot))` with
  `rampA3(dye, Material::Cloth)` (a slot with no maker: the Accent ramp, so drafts render). `greyRamps` greys them as
  materials. For a contact with a dye, `partByContact` moves the dye ramp a step, never the material's (F5 compares hue
  families, not exact pixels; F6 needs every non-dye pixel unchanged).
- **`FIX_FP = 4096`** in `FixRule` (`FIX_ALL` becomes 8191): every P5 change on the doll and the icons is behind it, so
  `EMB_FIX_RULES=4095` draws the pre-P5 look from the same build (diagnostics only: never set in the game or a gate), and
  `--doll-wear --freeze-check F --report` attributes a moved frame to it.
- **The mark tile:** a part serving `role overlay finish.mark`, `tags tile overlay fit`, `on-region P`, views keyed
  `<motif><w>x<h>` in `dye`, `dye2` and `ink` cells (MARKS draws them in `maps/finish/mark.pxm`); the resolver asks for it on
  a T1-T4 piece with a maker (`FIN_TILE_MARK`); FIT by the slot's motif; on a metal piece (the slot's own band >= 2:
  `armorStyle`, `helmStyle`, `shieldStyle`, `gloves`, `boots`) its dye cells draw as Ink one step down; at T4 it replaces the
  generic etch on that piece. A motif with no fitting view draws nothing (never a
  hole); `--doll-coverage --scope envelope` lists it.
- **The motif per slot:** `DollRecipe::motif` becomes one key per tier slot (from `fp::row(makerAt(slot)).motif`; a slot
  with no maker keeps today's `L.motif` path), for the mark and the filigree fits.
- **The looks field** `makers=<body>:<helm>:<shield>:<weapon>:<gloves>:<boots>:<cloak>:<back>` (people names, numbers or
  `-`; one name alone sets every worn slot) in `applyLadderLookField` (`doll_look_fields.cpp`), so looks files,
  `doll_gallery --look` and the gates can set it (colons: the spec itself splits on commas).
- **pxc.mjs `--report`** prints `fp-landing: <part> <facing> <key>` for every base view of a part serving `armour`, `helm`,
  `shield` or `cloak` that has neither R nor N cells (a new piece must carry the regions the fingerprint lands on). Once
  MARKS's re-cuts land the list holds only the parts that carry no fingerprint by design: the crests and plumes
  (`headwear/helm/crest/*`, `*_crest`, `*_plume`), the shield's arm strap (`shield/arm`), the mail sleeves
  (`armour/*/mail_sleeve_*`: the shirt's own part carries it), the `cape` and `fur` cloaks (their hem is procedural) and the
  `*_front` cloak parts. Not a craft warning: `pxc --check`'s output (the `pxc_check` record) does not change.
  **Amended in fix round 1 (owner-visible by-design list):** D (the dye seam) lands as R and N do; an empty placeholder view
  (no opaque cell: the `*_legend` helms' 1x1 base views, `mail_stride`, `mail_hang`) is not listed; and no cloak part is
  listed, because the doll's procedural hem now lands on every cloak VIEW with no R (`halfcape`, `noble` and `travel` draw
  their border only in their tier views), through T5. The judge's grep prints nothing.

### 4.4 Who sets `makers` (CORE)

- `gear::fillLadder` (every wearer of items: the hero, companions, the ladder kits): each worn slot's piece's maker
  archetype + 1 (culture 0: 0). `retierLook` keeps it.
- `census::dress` (looks.cpp), after `rankTiers` and `rankSlots`: every tier slot the look wears (`L.tier[s] > 0`; body,
  helm, shield, gloves, boots, cloak) takes the arms culture's archetype + 1: the owner kingdom's for the watch, soldiers,
  captains, heralds, envoys, jarls and kings, the local culture's for bandits, pirates and militia (the roles' `A` / `Al`
  split at looks.cpp:415-416). `liftRank` keeps it. No draw.
- The hero's T1 floor (fillLadder's floor bits) and culture-0 pieces: 0.

### 4.5 The icon fingerprint (ICONS)

`itemIconLook` (rig 2, `doll::fixOn(FIX_FP)`, `people >= 1`, armour kinds: Armor, Helmet, Shield, Gloves, Boots, Cloak) runs
one pass after the piece is painted and before the ornament (section 3.3's icon list). The pass is table-driven by kind
(one row per `Icon`: where the rim, seam, cuff and glyph rows lie), never a switch per people. The 12 glyphs (3x3 and 4x4)
are a table beside it, drawn to match MARKS's mark tiles.

### 4.6 Moves files

`tools/gates/baseline/moves_p5/<AREA>.txt`, the P3/P4 grammar (`<gate> <item glob>[,...] <id> <reason>`; `rebase_baseline.sh
--help`); ids `P5-1` .. `P5-6`. Each area measures against the p4f1 record on its own branch before it hands over. Because
the fingerprint shows only when CORE (makers), DOLL (rules) and MARKS (marks) meet, the view-pack and freeze moves are
measured once on the integrated head (GATES, section 6), each proved by the A/B (`EMB_FIX_RULES=4095` reproduces the p4f1
record).

---

## 5. Areas

Six areas, disjoint files. An area that needs another's file reads it and asks its owner; a conflict at integration is a
plan error.

### 5.1 CORE (engineer): the maker on every worn piece and every icon

Files: `rpg/core/art_ids.h` (HumanLook::makers, helpers; wave-1 contract), `rpg/art/art_items.h` (IconLook::people, tier,
key; wave-1 contract), `rpg/art/art_human.cpp` (HumanLook::key's makers line only), `rpg/sim/gear_look.h`,
`rpg/sim/gear_look.cpp` (gearIconKeys, retierLook), `rpg/sim/gear_ladder.h`, `rpg/sim/gear_ladder.cpp` (fillLadder,
describeLook prints makers), `rpg/sim/looks.cpp` (census::dress, liftRank's keep), `tools/tests/test_look_key.cpp`,
`tools/tests/test_arms.cpp` (only if a check must name the new field), `tools/gates/baseline/moves_p5/CORE.txt`.

Brief: items P5-1 (who carries the maker) and P5-3 (census).
- core: the wave-1 contract commit (section 4.2) within the first hour; `makers` filled for every worn slot of every item
  wearer and every census armour wearer, no draw; the icon's people and tier;
- depth: `describeLook` and `--expand-looks` print `makers=` so every kit and npc line shows its peoples; the item card's
  maker row (`gear::makerLine`) is untouched;
- delight: a companion in three peoples' pieces expands to three makers in `--expand-looks` (the PACK staging uses it).

Done (on fix-p5-CORE, built in `build-p5-core`; nothing visible moves until DOLL reads the field):
- `tools/slot.sh cmd //c "set BDIR=build-p5-core&& tools\\build.bat"` rc 0, no new warning kind;
- `tools/slot.sh build-p5-core/rpg_test.exe --look-key` PASS with `makers` in its list;
- `tools/slot.sh build-p5-core/rpg_test.exe --arms --seeds 1..3` ALL OK;
- `tools/gates/step_gates.sh --bdir build-p5-core --out <scratch>/core/step --only S2,S3,S5,S6,S7,S10.digest`: every line
  as the p4f1 record (makers read by nothing yet: pixels identical; the digest identical: no draw);
- `tools/slot.sh build-p5-core/rpg_test.exe --expand-looks tools/doll/ladder_looks.txt`: every `kit=` line names its makers
  (the Heartland's T2 cap 0: its kit makes it with no maker), every hero line with no kit 0; a scratch looks file of
  `npc=<people>:guard:0..2`, `soldier`, `bandit` for the 12 peoples: every armour slot names the arms culture's people;
- `tools/slot.sh build-p5-core/save_test.exe` ALL OK, `SAVE_VER` 24.

### 5.2 DOLL (engineer-high): the rules on the doll

Files: `rpg/art/doll/doll.h` (slots, recipe fields; wave-1 contract), `rpg/art/doll/doll_envelope.h` (FIX_FP; wave-1
contract), `rpg/art/doll/doll_pool.cpp` (slot names; wave-1), `tools/doll/pxc.mjs` (SLOTS mirror, wave-1; the `fp-landing`
report), `rpg/art/doll/doll_look_fields.cpp` (`makers=`; wave-1), `rpg/art/doll/doll_resolve.cpp`, `rpg/art/doll/doll_ramps.cpp`,
`rpg/art/doll/doll_paint.cpp`, `rpg/art/doll/doll_compose.cpp`, `rpg/art/doll/doll_compose_internal.h`,
`rpg/art/doll/doll_passes.cpp`, `rpg/art/doll/doll_internal.h`, `tools/tests/test_doll.cpp`, `tools/tests/test_doll_ladder.cpp`,
`tools/tests/test_doll_arms.cpp`, `tools/tests/test_doll_envelope.cpp`, `tools/tests/test_doll_forge.cpp` (only where a gate
lists slots or must skip the dye slots; no limit loosened), `tools/gates/baseline/moves_p5/DOLL.txt`.

Brief: items P5-1 (the worn rules, section 3.3) and P5-2 (the motif per slot, all 12 wired). Read `doll_paint.cpp`
`FramePaint::recolour` and `finishPlacements`, `doll_resolve.cpp` `ladderOf` and `flagsOf` first: the finish already maps R P
G N W per tier slot; P5 is new rows in that mapping, a band phase, two procedural edge rows and one more FIT tile.
- core: the wave-1 contract commit (section 4.3); R and N to the slot's dye band per section 3.3, the cuff rows, the cloak
  hem row, the mark tile at T1-T4 (FIN_TILE_MARK, Ink on metal, replacing the etch at T4), the filigree's motif per slot with
  the 12 keys, all behind FIX_FP; the shares inside section 3.5's caps (thin N to every other row where needed);
- depth: `--doll-coverage --scope envelope` lists a motif's missing mark fit (never a hole); `--doll-recipe <tag>` prints
  each slot's maker and motif; C14 contact parting covers the dye slots. C17 (metal reads) and C19 (`adefects`: the torso's
  metal and cloth counts) are material gates: a maker look that fails one carries too much dye there: thin it; never
  re-count the dye as metal or loosen the line;
- delight: the band's phase follows the canvas so a hem's two-tone check runs on unbroken from the coat onto its skirt.

Done (on fix-p5-DOLL with CORE's and the contracts' commits merged in, built in `build-p5-doll`):
- `node tools/doll/pxc.mjs --check` up to date (DOLL adds no maps);
- with `EMB_FIX_RULES=4095`: `tools/slot.sh build-p5-doll/rpg_test.exe --doll-wear --freeze-check tools/doll/freeze/p2c_freeze.txt`
  0 changed, `build-p5-doll/art_hash.exe` equal to the p4f1 record (the pre-P5 look reproduced exactly);
- without: the freeze check's changed frames all belong to looks with makers and `--report` attributes every one to FIX_FP;
  `--rig=1 --doll-wear --freeze-check tools/doll/freeze/p2c_freeze_rig1.txt`, `art_hash --rig=1` and
  `city_gallery --rig=1 --village-art-check tests/fixtures/golden_village_art_rig1.txt` identical;
- `tools/gates/doll_gates.sh --bdir build-p5-doll --out <scratch>/doll/dg`: every line PASS; differences only in lines
  that measure maker looks (listed in moves_p5/DOLL.txt with the numbers); `sil_w1..6`, `forge_shapes`, `heads` and the
  rig-1 lines identical (no silhouette moved); B10 inside the record under the alone-rerun rule;
- `tools/slot.sh build-p5-doll/fingerprint_gallery.exe <scratch>/doll/fp --seeds 1..3` (GATES's early commit): worn twins 0
  at every column (section 2.5 predicts it from the bands, cuffs and hems alone; the colour-only share is MARKS's to bring
  down with the marks), every share under its cap.

### 5.3 MARKS (artist): the table, the marks and the direction

Files: `rpg/core/fingerprint.h` (new; wave-1 contract), `rpg/art/doll/maps/finish/mark.pxm` (new), `rpg/art/doll/maps/finish/filigree.pxm`
(ADD the `plaid` motif's views only), region-plane re-cuts by script, never a pixel, on the armour, helm and cloak maps
whose T1-T5 views lack the N (or P) cells the rules land on: at least `rpg/art/doll/maps/armour/{man,woman}/{lamellar,lamellar_hang,mail_hang}.pxm`
(the lacing rows), the `when leather-coat` views of `armour/{man,woman}/{padded,padded_hang}.pxm` (the laces), and every
`rpg/art/doll/maps/headwear/helm/*.pxm` family with no N today (kettle, horned, kabuto, morion, onion, sallet, veil, warhat,
winged, crown, nasal, plumed, crested and their `_legend` files: the brow band's lower row, the lining under the rim),
`tools/doll/p5_marks.py` (new: the re-cuts, and `--verify`: every changed view keeps its cells, origin, mask and build row,
and only views that already carry a region plane are touched), `tools/doll/p4_leather.py` (only if the laces' N is better
emitted there), `rpg/art/doll/gen/**` (regenerated by `node tools/doll/pxc.mjs` only), `docs/STYLE_P2.md` (section 30, from
this plan's section 3), `docs/PLAN.md` (D118+: one decision per art change, PENDING THE OWNER'S REVIEW),
`tools/gates/baseline/moves_p5/MARKS.txt`.

Brief: items P5-1 (the marks, the table's values, the laces) and P5-2 (Highland's plaid). Style: STYLE_P2 27.3 (finish),
28.5 (the peoples' vocabularies), 29 (the envelope: T1 slim and simple); the filigree's own drawings are the language
(`finish/filigree.pxm`: gold drawn as shapes, every pixel touching another): the mark is the same motif in thread. Measure
the P boxes the T1-T4 views give first (a script), then draw each motif at those sizes.
- core: the wave-1 table commit (section 3.2's values; 30 minutes); 12 motifs x the T1-T4 P box sizes as `finish.mark`
  views (thread cells `dye` / `dye2`, holes `ink`; 2x2 to 6x6), the plaid's filigree views, the N re-cuts; pool under
  section 3.5's ceiling; every audit column 0 twins with at most a quarter colour-only at T1-T3;
- depth: tune the table's values on the sheets (in-game ground: grass, dirt, snow, sand; day and night) inside section
  3.5; a people's mark reads at 1x as a shape, at 3x as stitching;
- delight: the plaid's check meets the Highland's `hem-plaid` capelet in the same sett; the Jade's black-lacquer second
  colour reads as lacquered cord on the vermilion lacing.

Done (on fix-p5-MARKS with the contracts, CORE and DOLL merged in, built in `build-p5-marks`):
- `node tools/doll/pxc.mjs` then `node tools/doll/pxc.mjs --check`: up to date; the pool <= 2,602,032 B; `--strict` adds no
  warning in a P5 map; `node tools/doll/pxc.mjs --report`'s `fp-landing` lines name only section 4.3's by-design parts;
  `python tools/doll/p5_marks.py --verify` PASS (cells unchanged in every re-cut view);
- `tools/slot.sh build-p5-marks/fingerprint_gallery.exe <scratch>/marks/fp --seeds 1..3`: worn twins 0 in every column,
  colour-only <= 25 % at T1-T3, every share under its cap, the leather means inside section 3.5;
- `tools/gates/doll_gates.sh --bdir build-p5-marks --out <scratch>/marks/dg`: PASS, moves only as DOLL's plus the pool line
  and the C14 value counts (listed in moves_p5/MARKS.txt); `--doll-coverage --scope envelope` lists no missing mark fit for
  a T1-T4 view of any body form, helm or shield the kits wear;
- the owner-pack sheets (section 10: matrices, kits, census, before / after, greyscale) written; D118+ and STYLE_P2 30 in
  the branch.

### 5.4 ICONS (artist): the fingerprint in 16 pixels

Files: `rpg/art/art_items.cpp` (the fingerprint pass and the glyph table, rig 2 and FIX_FP only), `tools/preview/icons_gallery.cpp`
(the sheet: every audit column x the 13 makers, colour and greyscale; `--check` adds the fingerprint lines),
`tools/gates/baseline/moves_p5/ICONS.txt`.

Brief: item P5-4 (and P5-1's icon side). Section 3.3's icon list and 4.5. Keep D117's material reads exactly (the leather
browns, iron dark and matte, bronze's warm glint, the gilt iron coat's collar and shoulders); the dye is the doll's ramp
(`art::doll::rampA3`), the glyph the people's motif.
- core: every armour icon of a maker people shows its dye band where section 3.3 says and its glyph on the body; 0 twin
  pairs in every icon column; D117's lines green;
- depth: the 12 glyphs at 3x3 and 4x4 match MARKS's marks (the same shapes at icon scale); the cloak's hem and collar;
- delight: a Rare piece's glyph sits on the lit plate, its thread lit one step above the plate's (as the doll's mark).

Done (on fix-p5-ICONS with the contracts and CORE merged in, built in `build-p5-icons`):
- `tools/slot.sh build-p5-icons/icons_gallery.exe --check` PASS: D117's lines as before, and new: 0 twin pairs in every
  column (section 2.1's measure at 16 px, tolerance max(3, area / 40)), every maker's icon holds >= 3 px of its dye family
  (hue within 15 degrees, chroma >= 20) and <= section 3.5's icon share;
- with `EMB_FIX_RULES=4095`, `build-p5-icons/art_hash.exe` equal to the p4f1 record (`gates/S5.rig2.txt`: its `icons+new`
  line hashes the M6 icon looks, so the pre-P5 icons are reproduced) and, with the variable set, `icons_gallery --check`'s
  D117 lines print what they printed at 80215d7; `build-p5-icons/art_hash.exe --rig=1` equal to the record;
- `tools/gates/step_gates.sh --bdir build-p5-icons --out <scratch>/icons/step --only S5,S10.view`: S5.rig2 `icons+new` and
  TOTAL move (moves_p5/ICONS.txt), the view shots that show armour icons move, nothing else;
- the icon sheets for the owner pack (before / after, colour and greyscale).

### 5.5 GATES (engineer-high): the measure, the gallery, the re-sign, the map

Files: `tools/preview/fingerprint_gallery.cpp` (new; first, section 6), `tools/tests/test_fingerprint.cpp` (new: `rpg_test
--fingerprint`, section 7), `tools/doll/fp_looks.txt` (new: the gate's census and kit lines), `tools/gates/doll_gates.sh`
(the fingerprint line), `tools/doll/freeze/p2c_freeze.txt` and `tools/doll/freeze/p2c_freeze_rig1.txt` (re-signed at
integration only, for maker looks only), `tests/fixtures/golden_village_art.txt` (re-recorded at integration only if the
village's armour wearers moved), `README.md`, `docs/ARCHITECTURE.md`, `tools/gates/baseline/moves_p5/GATES.txt` and the
integration's measured `tools/gates/baseline/moves_p5/VIEW.txt`. Not the other areas' moves files.

Brief: item P5-6, and the integration's measurement. Productise the scratch audit tool (`...\scratchpad\p5-audit\p5_audit.cpp`:
its piece list, its isolation by the probe's owner and region, its measures, its sheets) as `fingerprint_gallery`
(`--seeds A..B`, `--before` = FIX_FP off through `art::doll::fixRules()`, `--grey`, `--matrix|kits|census|icons`), the
columns enumerated from the forms and materials (every `cult::BodyArm`, `HelmForm`, `ShieldForm` with its materials and
tiers: a new form joins the gate by itself). Then the gate (section 7) with a selftest, then at integration the freeze
re-sign and the moves.
- core: the gallery committed early in wave 2 (every area measures with it); `--fingerprint` F1-F8 and `--selftest`; the
  re-sign; the moves;
- depth: the gallery's report names each failing pair with its three measures and the pixel it differs most at;
- delight: the sheets mark a twin red and a colour-only pair amber, as the audit did, and print each people's dye swatch.

Done (on fix-p5-int after every merge, build `build-p5-int`):
- `tools/slot.sh build-p5-int/rpg_test.exe --fingerprint --selftest` PASS (an injected twin, a dye over its cap, a moved
  outline pixel, a census armour slot with no maker each FAIL);
- `tools/slot.sh build-p5-int/rpg_test.exe --fingerprint --seeds 1..3 --report --gates` PASS;
- before the re-sign: `rpg_test --doll-wear --freeze-check tools/doll/freeze/p2c_freeze.txt --report` attributes every
  changed frame to FIX_FP and every changed look has makers; after: 0 changed in both manifests (rig 1's for the same shared
  doll frames, D115's reading); `git diff --stat` of the manifests names only maker looks;
- `tools/gates/doll_gates.sh --bdir build-p5-int --out <scratch>/gates/dg`: every line as the record except the declared;
- README "Where is ..." rows (the fingerprint table, the rules, the gate, the gallery) and ARCHITECTURE notes (the core
  header, the dye slots, FIX_FP).

### 5.6 PACK (engineer): the fingerprint in play

Files: `rpg/view/scripts/view_p5.cpp` (new: `expect makers <slot> <people>` on the hero, a companion or the nearest census
person; `p5stage <people> guard|soldier|bandit [n]` when `lineup` and `gotoculture` are not enough), `tools/scripts/p5_fingerprint.txt`
(new: the shots, only commands 80215d7 already has, so the same file shoots "before" on the old build),
`tools/scripts/p5_makers.txt` (new: the `expect makers` checks, after only), `witness/p5_fingerprint.witness.json` (new,
optional: the makers read as state), `tools/doll/p5_pack.py` (new: side-by-side and greyscale copies, captions),
`tools/gates/baseline/moves_p5/PACK.txt`.

Brief: the owner pack's in-play half (section 10) and a script the judge can rerun. Existing commands do most of it
(`gotoculture <archetype> town`, `lineup <archetype> guard n`, `gear6 <kind> <archetype> mat leather equip`, `kit6`,
`armskit`, `compkit`, `shopat smith`, `zoom`); shots at NEAR and CLOSE.
- core: before and after shots of: four towns' watch (Dune, Jade, Highland, Sun Temple, plus seed 7's start town), a
  companion in three peoples' pieces, the hero in three peoples' T1 leather coats, a smith's shelf with three peoples'
  pieces, the EQUIP page; each in greyscale too. "Before" is the integrated build with `EMB_FIX_RULES=4095` (the same
  frames and stagings, no drift); the script is developed in wave 2 on 80215d7's build (`--bdir C:\Users\joshu\Documents\embervale\build`
  while `git -C C:\Users\joshu\Documents\embervale rev-parse HEAD` is 80215d7; else a throwaway clone at 80215d7, SDL by
  junction), whose shots spot-check the A/B;
- depth: five of the A/B's "before" shots equal 80215d7's pixel for pixel;
- delight: a night shot under a lamp, where the bands still read.

Done:
- `node tools/gates/witness_pack.mjs --bdir build-p5-int --out <scratch>/pack/after --keep tools/scripts/p5_fingerprint.txt`
  and `... --keep tools/scripts/p5_makers.txt` PASS (every `expect makers` line true); `EMB_FIX_RULES=4095 node
  tools/gates/witness_pack.mjs --bdir build-p5-int --out <scratch>/pack/before --keep tools/scripts/p5_fingerprint.txt`
  PASS; five of its shots equal the 80215d7 build's;
- `python tools/doll/p5_pack.py <before> <after> <scratch>/../p5-pack/play` writes the side-by-sides, the greyscale copies
  and `captions.txt`;
- W2: the new script PASS in `witness_pack --all`; W3 (if the witness test is added) passes twice.

---

## 6. Waves and integration

- **Wave 1 (contracts, the first hour; the integrator merges them onto `fix-p5-int` as they land, in this order):**
  1. MARKS: `rpg/core/fingerprint.h` with section 3.2's table;
  2. CORE: `HumanLook::makers` and its helpers, `IconLook::people` / `tier`, the key lines, `--look-key`'s list;
  3. DOLL: the slots (+ pxc SLOTS, slot names, the default dye ramps), `FIX_FP`, the `makers=` looks field.
  Every area rebases on them. Nothing visible moves (fields unread, slots unused).
- **Wave 2 (all six at once; no other barrier):** GATES commits `fingerprint_gallery` first (about two hours; the
  integrator merges it onto `fix-p5-int` at once, every area pulls it to measure). PACK shoots "before" on 80215d7's build
  at once. DOLL's first milestone is the dye routing and the mark tile (so MARKS sees marks on figures); MARKS drafts the
  marks meanwhile in `doll_gallery part:` views. ICONS needs only the table and the IconLook fields.
- **Integration (merges on `fix-p5-int`; disjoint files, so a conflict is a plan error):** 1. CORE, 2. DOLL, 3. MARKS
  (`node tools/doll/pxc.mjs` after the merge), 4. ICONS, 5. PACK, 6. GATES (last: the gate's thresholds checked on the merged
  head, the freeze re-sign, the village fixture if it moved, README / ARCHITECTURE, the measured VIEW moves). After each merge
  that area's own Done lines on the merged head (fix rounds verify only their fixes). Then the one full regression
  (section 9).

---

## 7. The gate: `rpg_test --fingerprint [--seeds A..B] [--report] [--gates] [--selftest]`

GATES, `tools/tests/test_fingerprint.cpp`, over `fp::kRows`, the gallery's columns (every form x material x tier the shops
and kits make) and `tools/doll/fp_looks.txt`; a judge line this batch (decision 8), not yet a CI line.

| line | what | pass |
|---|---|---|
| F1 table | the dyes | primaries >= dE 25 apart; signatures >= 25; primaries >= 20 from every material ramp step 1-3; dye2 >= 20 from its dye; 12 motifs, all different, each drawn in `finish.mark` and `finish.filigree` |
| F2 worn | every column x the 13 makers (78 pairs; 66 for alloys) x seeds | no twin (four views: man and woman, D and R idle frame 0; tolerance max(2, area / 40)); colour-only reported, <= 25 % of a T1-T3 column |
| F3 icons | the same columns as icons | no twin (tolerance max(3, area / 40)) |
| F4 material | shares and means | section 3.5's caps; the worn leather means; `icons_gallery --check`'s D117 lines (run inside) |
| F5 agreement | worn and icon from one row | for every piece x maker: the worn piece holds >= 2 px of its dye family in the D view, the icon >= 3 px; both within 15 degrees of the row's dye (or dye2) hue, chroma >= 20 |
| F6 additive | FIX_FP on vs off | on every look of the columns, the kits and `fp_looks.txt` (idle and walk, four facings): identical opaque masks; every differing pixel is a dye-slot pixel, inside a P region (mark, motif) or an Ink step of a mark cell |
| F7 classic | makers 0 | looks with makers 0 (the A3 hero, the T1 floor, culture-0 pieces) and icons with people 0 pixel-identical on vs off |
| F8 census | the watch, soldiers, bandits, captains, kings of the 12 peoples (`npc=` lines, seeds 1..3) | every worn armour slot carries the arms culture's people; whole figures of two peoples' watch at one rank never twins |

---

## 8. Baselines: expected moves

Each area writes `moves_p5/<AREA>.txt`; the integrator runs `GATES_MOVES_DIR=tools/gates/baseline/moves_p5
tools/gates/rebase_baseline.sh` from p4f1 to **p5**. The proof of every visible move is the A/B: the same build with
`EMB_FIX_RULES=4095` reproduces p4f1's records (section 9 step 2b). Anything else moving is a bug.

| gate | expected | why (area) |
|---|---|---|
| S1.build | 37 -> 38 exes (`fingerprint_gallery`); warnings <= record, no new kind | GATES |
| S2.seeds | identical | - |
| S3.save, S3.savever | identical; SAVE_VER 24 | - |
| S3.fixtures | `golden_village_art.txt` only if the village's armour wearers moved (re-recorded at integration); the rig-1 fixture identical | CORE, DOLL, MARKS |
| S4.clang, W1.msvc_ci | 69 commands, 0 fails (`--fingerprint` is a judge line) | - |
| S5.rig2 | `doll` (hash_looks lines that wear maker pieces), `icons+new` (maker icons), TOTAL; `expect ART_TOTAL_RIG2` moves with them | DOLL, MARKS, ICONS |
| S5.rig1, S7.rig1 | identical | - |
| S6.freeze | 0 changed against GATES's re-sign, whose diff holds only maker looks, every frame attributed to FIX_FP | GATES (for all) |
| S6.freeze1 | the same looks' shared doll frames (D115's reading) | GATES |
| S6.pool | bytes and hash: `finish/mark.pxm`, the plaid filigree, the N re-cuts (<= 2,602,032 B) | MARKS |
| S7.rig2 | the village groups with armour wearers (the watch, militia) | CORE, DOLL, MARKS |
| S8, W5.firstlaunch | unchanged (the hero's T1 floor has no maker) | - |
| S9.test / S9.shot | `village_start.png` only if an armour wearer is in shot at t = 2 s | CORE, DOLL |
| S10.digest, W6.digest_clang | identical (no draw, no saved field) | - |
| S10.view, W2.view_all | every shot with a maker-armour wearer (the watch, soldiers, bandits, captains, kings, companions, the hero in maker gear) or an armour icon (shop, pack, EQUIP, forge, cards, ground loot); new `p5_fingerprint/*` | all (measured once at integration: VIEW.txt) |
| S10.audio | identical | - |
| S10.layers | PASS, no new violation (`rpg/core/fingerprint.h` includes nothing) | all |
| W2.scripts | 0 FAIL; `p5_fingerprint.txt` and `p5_makers.txt` PASS | PACK |
| W3.witness | every test passes twice (a new `p5_fingerprint` test if PACK adds it) | PACK |
| W4.doll | the lines that measure colours or hash frames of maker looks: `arms_matrix`, `ladder_*`, `forge_w*`, `motion_*`, `variety`, `freeze`, `freeze_dressed_rig1` (its CHANGED list follows the rig-2 re-sign), `art_hash`, `village_art`, `pxc_check`, `gates_parity` (pool line, C14 counts; B10 under the alone rule), a new `fingerprint` line; `env_*` and `adefects` only where a colour measure among them moves (C19's torso metal and cloth counts: a dye pixel is neither; named in DOLL.txt: their areas and shoulders cannot move); **the shape lines identical: `sil_w1..6`, `forge_shapes`, `cov_all`, `cov_ladder`, `cov_forge`, `heads`, `rig*`, `anim*`, every `*_rig1`; `cov_envelope` identical once every mark fit the kits need is drawn (it lists the missing ones)** | DOLL, MARKS, GATES |
| W6.perf | within the record's noise + 3 % | - |
| W7.play | not run (an art batch) | - |

---

## 9. The full regression (once, at integration)

The owner's rule: one all-systems regression per update; fix rounds rerun only their own lines.

```sh
cd /c/Users/joshu/Documents/emb-solid-int        # on fix-p5-int with all six areas merged
S=/c/Users/joshu/AppData/Local/Temp/claude/C--Users-joshu/1979ea42-238a-4969-bda8-d76ab6bf2de5/scratchpad/p5
export WITNESS_MAX_PROCS=5 WITNESS_MAX_BROWSERS=1   # keep-awake loop running (PC sleep trap)
# 0. a fresh clone and build dir, SDL by junction (rule 3), no network fetch
git clone -q --no-hardlinks . $S/p5clone && git -C $S/p5clone -c advice.detachedHead=false checkout -q "$(git rev-parse HEAD)"
mkdir -p $S/p5clone/build/_deps
cmd //c mklink /J "$(cygpath -w $S/p5clone/build/_deps/sdl3-src)" "C:\Users\joshu\Documents\embervale\build\_deps\sdl3-src"
# 1. ONE wave run against CURRENT (p4f1)
( cd $S/p5clone && tools/gates/wave_gates.sh --bdir build-p5wave --out $S/p5wave )
B=$S/p5clone/build-p5wave
# 2. every difference declared, every declared move seen
GATES_MOVES_DIR=tools/gates/baseline/moves_p5 tools/gates/rebase_baseline.sh --check-moves --run $S/p5wave --bdir $B
# 2b. the A/B proof: the pre-P5 look from the same build reproduces p4f1 (views, art hash, freeze, village); the clone's
#     freeze manifests and village fixture are P5's, so the old ones come from 80215d7
git show 80215d7:tools/doll/freeze/p2c_freeze.txt > $S/freeze_p4f1.txt
git show 80215d7:tests/fixtures/golden_village_art.txt > $S/village_p4f1.txt
EMB_FIX_RULES=4095 tools/slot.sh $B/rpg_test.exe --doll-wear --freeze-check $S/freeze_p4f1.txt          # 0 changed
EMB_FIX_RULES=4095 tools/slot.sh $B/art_hash.exe > $S/p5ab_art.txt                                       # = p4f1 gates/S5.rig2.txt
EMB_FIX_RULES=4095 tools/slot.sh $B/city_gallery.exe --village-art-check $S/village_p4f1.txt            # 0 changed
EMB_FIX_RULES=4095 node tools/gates/witness_pack.mjs --bdir $B --out $S/p5ab/view --all \
     --check tools/gates/baseline/p4f1/probes/view_all/hashes.txt                                         # every PNG identical
# 3. re-record the declared items into tools/gates/baseline/p5, flip CURRENT; commit with REASONS.md
GATES_MOVES_DIR=tools/gates/baseline/moves_p5 tools/gates/rebase_baseline.sh --from p4f1 --to p5 --run $S/p5wave --bdir $B
# 4. confirm the rebase on the moved lines only, with the clone's build (no second full run)
tools/gates/step_gates.sh --bdir $B --out $S/p5recheck --only S3,S5,S6,S7,S9,S10
node tools/gates/witness_pack.mjs --bdir $B --out $S/p5recheck/view --all \
     --check tools/gates/baseline/p5/probes/view_all/hashes.txt
tools/gates/doll_gates.sh --bdir $B --out $S/p5recheck/doll
# 5. the P5 judge lines on the clone's build
tools/slot.sh $B/rpg_test.exe --fingerprint --seeds 1..3 --report --gates
tools/slot.sh $B/rpg_test.exe --fingerprint --selftest
tools/slot.sh $B/icons_gallery.exe --check
tools/slot.sh $B/rpg_test.exe --arms --seeds 1..3
tools/slot.sh $B/rpg_test.exe --look-key
tools/slot.sh $B/fingerprint_gallery.exe $S/p5judge/after --seeds 1..3
tools/slot.sh $B/fingerprint_gallery.exe $S/p5judge/before --seeds 1..3 --before
node tools/gates/witness_pack.mjs --bdir $B --out $S/p5judge/play --keep tools/scripts/p5_fingerprint.txt
node tools/gates/witness_pack.mjs --bdir $B --out $S/p5judge/makers --keep tools/scripts/p5_makers.txt
node tools/doll/pxc.mjs --report | grep "^fp-landing" | grep -v -E "helm/crest/|_crest |_plume |shield/arm |mail_sleeve_|cloak/(cape|fur) |_front "
#    (prints nothing: only the by-design parts of section 4.3 carry no R or N)
# 6. clean up: never rm -rf through the junction
cmd //c rmdir "$(cygpath -w $S/p5clone/build/_deps/sdl3-src)"
test -f /c/Users/joshu/Documents/embervale/build/_deps/sdl3-src/CMakeLists.txt && rm -rf $S/p5clone
```

`EMB_FIX_RULES` reaches the game: Witness passes its environment to the game it launches (`witness_pack.mjs`'s own note
on WITNESS_WATCH; `EMB_V1TRACE` travels the same way). Time: P4's wave gates took 61-67 min with 5 slots (no W7 here);
step 2b adds a second view pack (about 20 min); about 2.5x while the owner plays (cap 2).

---

## 10. The owner pack (`...\scratchpad\p5-pack\`, every image also in greyscale, each with a one-line caption)

PENDING THE OWNER'S REVIEW: every image below is art the owner may find "off"; nothing ships as final before he has seen it.
- **matrices/** (MARKS with GATES's gallery; ICONS for the icons): per piece group (body, helm, shield, gloves, boots,
  cloak), the 13 makers x the audit's columns, man and woman, D and R, before (`--before`) | after; the icon matrix before |
  after. The section 2.2 table with the after column measured.
- **kits/** (MARKS): every people's T1-T6 ladder kit, man and woman, before | after; one people's T1-T6 at 4x.
- **census/** (MARKS): the watch ranks 0-2, a soldier, a bandit, a captain and the king of every people, D and R, before | after.
- **play/** (PACK): four towns' watch (Dune, Jade, Highland, Sun Temple) and seed 7's start town at NEAR; a companion in
  three peoples' pieces; the hero in three peoples' T1 leather coats side by side; a smith's shelf of three peoples' pieces
  (the icons); the EQUIP page; a night shot.
- **swatches.png** (GATES): the twelve dyes with their bands and the twelve motifs at 1x, 3x and as icons.
- **The decisions** (section 11) each with its image: the laces (2), Highland's plaid (3).

---

## 11. Decisions for the owner (the plan proceeds on the first reading of each)

1. **One fingerprint per people**, fixed across its kingdoms (a Dune town is indigo and saffron wherever it is); a kingdom's
   own colours stay on its shields, tabards, cloth coats, cloaks and plumes. (Other reading: each kingdom's own colour as
   the dye: varied, but neighbouring kingdoms of different peoples could match.)
2. **The leather gambeson's laces take the people's dye** (D116's rawhide crossing kept on maker-less coats only); its
   collar and hem too. (Other reading: the laces stay rawhide; the dye on the collar and hem only: a weaker mark at T1.)
3. **Highland gets its own motif, the plaid**, so no two peoples share one; its T5-T6 filigree moves from the knotwork.
4. **Crossed families and the hidden peoples** wear their first archetype's fingerprint (their strangeness is their metals).
5. **Weapons are not in this batch** (the owner asked for armour; a grip wrap in the dye is a later, small step).
6. **No new ornament rule and no per-people leather tanning** (`ArmsStyle::leather` stays unused): the leather stays the
   wearer's, so the material reads first; the existing ornaments stay as they are.
7. **Seen, not fixed here:** a leather cap worn with a metal body draws in the body's metal (one Metal slot a figure: a
   per-piece metal batch would fix it with the same per-slot mechanism this batch adds for the dye); a T1 bronze helm of the
   Heartland, Imperial and Sylvan draws HOMESPUN's cloth hood in the shirt colour.
8. **`--fingerprint` is a judge line** this batch; adding it to the CI list (web.yml, `--list-ci`) is a follow-up.

---

## 12. What the judge checks

From the state a player starts in, look and feel first, measured (eyes for shots, numbers for the rest):
- **look:** the owner pack's after sheets beside the before (`fingerprint_gallery` both ways from one build): every people's
  T1 coat, cap, gloves and boots carry its band and mark; T3-T5 its lacing; T5-T6 its motif in gold; nothing reads as a new
  material (leather still leather, bronze still bronze) and no outline moved (the greyscale sheets: the marks and two-tone
  bands still part the peoples); the census sheet's watch of Highland, Marsh and the Sun Temple no longer one;
- **in play:** `p5_fingerprint.txt`'s shots (four towns' watch, the companion in three peoples' pieces, the shelf, EQUIP) at
  NEAR, and one hidden Witness run at seed 7 (no gold calls): buy the start smith's leather coat and see its people's laces
  on the hero and on its icon;
- **numbers:** `--fingerprint --seeds 1..3 --gates` (F1-F8) and `--selftest`; `icons_gallery --check`; the doll gates as the
  record but the declared; `sil_w*` identical; the A/B (`EMB_FIX_RULES=4095`) equal to p4f1; the pool under 2,602,032 B;
  `pxc --report`'s `fp-landing` list holding only the by-design parts (a new piece inherits the fingerprint); B10 inside its
  budget; the sim digest identical;
- **the rules:** rig 1 identical; no drawn view's cells changed (`p5_marks.py --verify`, `git diff` of the maps: ADDed views
  and region rows only); no draw in the census (the digest); the ladder intact (T1 slim and plain: the envelope's areas and
  shoulders unchanged).

---

## 13. Fix round 1 (the judge's R1 items; what changed in the rules, all behind FIX_FP, PENDING THE OWNER'S REVIEW)

The plan's 2.5 prediction (bands, cuffs and hems alone clear the twins) failed where a piece's region planes carry nothing
the rules land on, and where another part covers the row a rule picks. Round 1 changed these rules (the owner pack's play
half and the kits sheet show them):

- **Region D lands.** `p5_marks.py`'s probe for DOLL's `D` read a literal backspace, so D122's re-cuts never wrote; they land
  now (lamellar lacing and lowest course, the helms' brow lining), with new D rules: the T1 caps the bronze spangen, plumed
  and masked helms draw (a D-only plane on the brim row: decision 7's hood path for the face helms), the great helm's breath
  band under the slit, the band above the dome's foot on the kettle, horned, crested and winged T3 views (the HOMESPUN and
  TANNER hood's row over its panel), the leather and leaf coats' back collar. The pool is 2,601,938 B (ceiling 2,602,032).
- **Cuffs where they show.** A glove's or boot's cuff covered by a sleeve or a hem shows on its topmost visible row; a
  gauntlet's two rows (one when its share would pass 40 %).
- **The rim's lining and the bare views.** A T3-T5 body, helm or shield view with a brass rim but no seam takes the dye on the
  plate along the rim's inner edge (the rim stays brass: C12's trim); a view with no landing region at all takes the
  procedural hem (never on the trim band's brass); the buckler's boss takes the band at T1-T4; a T1-T2 binding takes its lit
  metal; an interior seam line (step 0 inside the piece) takes the dye at its shadow step.
- **The mark.** No mark cell cuts or dyes a metal glint (C17); at T4 the mark lies over the etch (C17: the etch's lowered
  glint stays lowered), and a chased T4 body (an alloy's forge motif over the torso) keeps its motif and no mark (C16m); on a
  painted field, cloth or leather the mark stays thread at T3-T4 (only metal is cut).
- **Never a lone dot.** N thins in two-row blocks; after the passes every fingerprinted pixel with no changed 4-neighbour
  goes back to the pre-P5 pixel; from T3 the lacing and seams take the dye alone (the two-tone band alternated lace by lace
  into a checker on a lamellar skirt); the band stays two-tone on rims, hems, cuffs and the T1-T2 laces.
- **Highland's plaid.** The mark is an open check; the filigree a gilt lattice over the piece's metal (lines every 3 px, U
  where they cross, the squares clear) on the arabesque's sizes, the knotwork's set stones copied in place (the T5 gem).
- **Icons.** The share counts the outline ring (the eye sees it; ICONS and GATES print the same number); the shield's
  lashing runs 2 px every 8.
- **Not cleared: F5 agreement** (worn D view >= 2 px of the dye family, icon >= 3): 1,290 -> 330 of 4,176 pieces. What is
  left are T3 steel face helms (masked, plumed, spangen) and T4 alloy bodies whose D view shows no dye: their seams lie under
  a belt or skirt, or on lit plate the rules keep metal. Clearing them needs map re-cuts the pool's ceiling no longer holds
  (94 B left) or a per-piece rule the owner should see first; the alloy itself parts the peoples at T4 (F2 0 twins).

## 14. Fix round 2 (the judge's R2 items; behind FIX_FP, PENDING THE OWNER'S REVIEW)

- **River's teal lands** (F5: River held 152 of the 330 short pieces). `doll::dyeRamp` (the doll's dye pairs and the icons'
  `fpRamp`, so the bag and the hero still agree by construction) keeps a dye's shadow and lit steps (1, 3) within 10 degrees
  of its CIELAB hue: the generator's hue shift is a fixed a*b* offset, so River's soft teal (chroma 29) turned 22 and 15
  degrees and read as another blue-green beside the teal shirt. Every other row already lay within 12, so only River's ramp
  moved. A dye group that must part from a contact and would sink its shadow step under chroma 20 (River's teal between dark
  hair and a helm's metal: luma 72, chroma 18, no teal left) climbs instead where a climb clears (its glint capped).
  A piece whose finished frame shows under 2 px of its dye and more of its dye2 (a hand's 2 px cuff on a 2:2 band's white
  pair) turns its band's phase (dye and dye2 trade slots, steps kept).
- **B10.** The per-pixel paint runs in two instantiations: a frame with no fingerprint runs the pre-P5 loop (no fingerprint
  branch, no under plane); LOCO p50 back to 80215d7's.
- **The pack.** `p5_pack.py --sheets` writes every sheet of section 10 from one build (`build.sha`).
- **Still open: F5 194 of 4,176** (river 16, the others 9-29: highland 29, sun temple 26, imperial 19, sylvan 17, steppe
  16, heartland 15, starspire 14, marsh 11, fjordfolk 11, dune 11, jade 9). What is left: the T4 alloy bodies whose kit
  layer covers the torso in the D view (67; 7 more alloy helms, gloves and shields), the T3 masked / plumed / spangen steel helms with no D or N plane on their
  steel view (84), and 36 others (T3 steel helms 15, Heartland's bucklers 12, Sun Temple's iron coats 6, a steel oval shield 3). Clearing them needs the planner's ruling: map
  re-cuts past this plan's 2,602,032 B ceiling (94 B left; the budget is 2,621,440 B) or an F5 exemption for those classes.
