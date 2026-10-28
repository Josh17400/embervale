# Armour spawning and alloys with rarity (approved 2026-10-28; revision 6)

**Approved by the owner on 2026-10-28.** The owner answered the twelve questions in section 11 with "Defaults is good",
so every recommended default stands, including option (b) for question 6 and "grow it" for question 5. The owner amended
question 11 so the hero can give a companion better gear: "we probably will need to allow for players to give their
companions better armor than what they start with though, so maybe you can't take their defaults but you can give them
better armor and they will equip what's better". Section 6 carries the amended design. The rulings are recorded in
docs/VISION_PLAN.md 15.37, and in each section a ruling changes (7.2, 7.3, 7.5, 9, 15.6, 15.9, 15.11, 15.19, 15.26,
15.27, 15.28 and 15.36's P2c). Question 12 decides the timing. The look-only parts (Batch A) ship inside P2c. The rules
(Batch B) are their own update after P2c is DONE, with their own full regression: they are planned rules, not work for
P2c's wave 2. "Revision 4" below lists what the answers changed in this document. The same day the owner settled the two
companion points the plan had read for itself ("Made once, and if they die they drop everything"), and "Revision 5" lists
what that changed. The owner then answered the four companion points revision 5 left to confirm, and reversed one
reading: a companion who leaves and comes back is the same companion, not a new join. "Revision 6" lists what that
changed.

This design was written read-only on p2c-slice (df2a155), dated 2026-10-25. The owner asked how armour should spawn and
how the advanced alloys should work with rarity. Revision 3 answers a rules-compliance review against
docs/VISION_PLAN.md, docs/PLAN.md and the owner's rulings of 2026-10-25. Revision 2 answered three earlier reviews (fit
with the code, the player's eye, the economy). Section 12 logs what was taken and what was not. Every review item was
checked against the repo before it was applied. Claims about today's game cite file:line at df2a155. P2c's wave 2 is
editing some of the cited files, so check a line before relying on it. Numbers marked *model* come from scripts that copy
gear.cpp's rules (`loot_model.py`, `luck_gate.py`, `luck_hours.py` and revision 5's `kit_value.py` in
`C:\Users\joshu\AppData\Local\Temp\claude\C--Users-joshu\1979ea42-238a-4969-bda8-d76ab6bf2de5\scratchpad\`, outside the
repo). They are not playtest results. Once P2c's `rpg_test --ladder-pace` exists, it replaces them as the measure. The
owner summary (%TEMP%\claude\emb_P2c\loot\OWNER_SUMMARY.md, outside the repo) is the plain-language version.

**In one line:** where you are decides the metal, luck decides the finish, old places hold the old makers' metals, and
every metal keeps its own helmet.

**How it should feel.** Starting armour is simple and slim. Every step up is more detailed, more alive and bulkier. Each
land has its metal: leather and bronze at home, iron in the next kingdoms, steel by the first cities. From band 4, each
people's own alloys appear in order, each with its own outline and its own helmet: first the everyday metal, then the
warriors' metal, then the masterwork at the frontier. Luck never swaps the metal. It shows as finish. A Rare piece gets
trim. An Epic is a tier bulkier and finer. A Legendary lives, with lit runes, motes and the edge spark. A Mythic burns,
and the ember and the flame crown burn on top of its own alloy's helmet and outline. Ruins, shrines and the hoards of
world bosses hold the old makers' metals: gilded parade steel, black obsidian glass, furnace iron and crystal starmetal.
They are the same in every land, and they form a ladder of their own that ends on the biggest shoulders in the game. A
hero who braves a harder place finds the next land's metal early. Because no roll swaps the metal, a new land's shop
window is always new.

## Revision 6: the owner's answers on companions (2026-10-28)

Revision 5 left four companion points for the owner to confirm: the three in docs/VISION_PLAN.md 15.37 (items 4-6) and
its reading of a companion who comes back (section 6, rule 1). VISION's list now holds all four as items 4-7. The owner
answered them the same day, and the fourth reverses the plan's reading:
- **The kit is capped like a find (ruled).** Its item level is capped at the hero's level + 4, and its rarity rolls with
  the hero as the finder. The owner: "that works for me".
- **The pile keeps the ground loot's rules (ruled).** It lies 240 s and is gone at a map change or a load, and a death off
  the hero's map (a companion at home carried off in a raid whose rescue fails) leaves no pile. The owner: "yep thats
  good".
- **Hire-and-let-die pays once per sellsword slot (ruled, accepted).** The owner: "thats fine if someone wants to farm
  that way they can but there are defintly better avenues to make money".
- **A companion who comes back is the same companion (ruled; it reverses revision 5's reading).** The owner: "well if
  they re join they should stay at the level they left, and shouldnt get treated as a new join since they arent new."
  Revision 5 read a return as a new join: the record started again, bond included, and the kit was made again from its
  seed. Now a companion who parts while living and later comes back keeps their level, the kit they left in (it left
  with them and is not made again) and their record: bond, approval, memories and history. Gifts still come back to the
  hero at the parting.

What it changes:
- **Today a parting forgets them** (loop 6, section 6). Dismissal, an unpaid sellsword's leaving, a personal quest's
  farewell and a sellsword's angry leave remove the whole record (partyDismiss, party_game.cpp:914-922). The next join
  builds a new one with the kit made again (party_recruit.cpp:359-375). Only a census companion who leaves in anger is
  kept, as State::Left, until the apology (party_game.cpp:257-260, 320).
- **Rule 8 keeps them** (section 6). Their record waits whole in a new saved roll in the party block, `Party::parted`,
  found again by their npc id or, for a sellsword, by his inn and slot. The census can't hold it: it is rebuilt from
  seeds and saves only a few flags per resident (life.h:119-120, 368-369). They leave and come back in their own kit,
  and PartyOps::kit never runs on a return.
- **Their level** tracks the hero's, one below (VISION 9; party.cpp:216). It is never stored, so it can't fall while they
  are away. What they grow themselves, bond and the skills it opens at 20, 50 and 80, comes back with the record. The
  plan reads "stay at the level they left" as "nothing they earned resets" (VISION 15.37 item 8 asks the owner).
- **Whether they come** keeps today's rules, and only who comes changes (item 10 asks about a raid captive who may have
  no reason left to come).
- **An angry leave keeps today's way back:** the apology, refused the first time and taken on a later day, lifts approval
  to at least -30 (party_talk.cpp:622-656). There is no bond bar, and bond is untouched. A sellsword who leaves in anger
  now keeps his record, so he asks the same apology at his inn table (item 9).
- **Sellswords.** Every living parting, an angry one included, sets his hire mark to that day, so the week's wait holds.
  Rehired after it, he brings back his record and kit, and he is paid a day's wage up front, as any hire is.
- **Three quiet loops close:** a kit made again at a higher cap after the hero levels, approval laundered by a dismissal,
  and a finished personal quest offered again (party_talk.cpp:200 reads MF_QUEST_DONE, which a new record drops).
- The party block's own version steps to 5 for the roll and still reads 4, so save_v23 loads. SAVE_VER 23 holds
  (section 8).
- Sections 3, 5, 6, 8, 9, 11 and 12 follow it.

## Revision 5: the owner's follow-up on companions (2026-10-28)

Revision 4 left two companion points as the plan's own readings (docs/VISION_PLAN.md 15.37). The owner settled both:
"Made once, and if they die they drop everything".
- **Made once (ruled).** A companion's kit is made once, when they join. It is never remade as they level, and the
  hero's gifts are the only way up.
- **A death drops everything (ruled).** This replaces revision 4's death rule, under which gifts came back to the hero's
  pack and the grave kept one keepsake. When a companion dies, everything they carry (their kit, worn and stowed, the
  hero's gifts, and their belongings, such as the potions and food they were given) drops where they fell, as a pile the
  hero can pick up. The pile is the game's own ground loot, the Pickup that a body's loot already uses
  (game_rpg.cpp:217-221; game.h:245-254), and it keeps that loot's rules. It flies to the hero within 40 px and is taken
  at 9 px (game.cpp:1710-1732), and it lies for 240 s (game.cpp:1734). It is never saved, so changing map or loading
  clears it (game.cpp:83, 1946). Each kit piece drops without IF_KIT, as ordinary gear that sells and breaks down.
  Revision 6 confirms these rules ("yep thats good").
- **Partings while they live are unchanged.** Dismissed, unpaid or leaving in anger, a companion hands every gift back
  into the hero's pack, and their kit leaves with them. (Revision 6: the kit comes back with them, with their whole
  record, if they return.)
- **The grave holds no keepsake.** Everything they had lies where they fell, so nothing is laid with the grave to take.
  The grave itself stays, because it carries the remembrance (VISION 15.6.2): their people's stone with their weapon
  laid on it (a look, party_game.cpp:218; party_graves.cpp:166), the epitaph, flowers and words, their kin's grief
  (party_game.cpp:229) and the party page's roll of the fallen. None of these reads the keepsake. Only the grave's
  "TAKE SOMETHING" choice (party_talk.cpp:443) and the roll's "A KEEPSAKE TAKEN" mark (paperdoll.cpp:708) do, and both
  stay quiet on a grave laid with nothing. IF_KEEPSAKE leaves Batch B. A grave laid before Batch B keeps what it holds
  and its one keepsake, as today.
- **The kit is capped like a find.** A dropped kit is ordinary gear now, so its item level is capped at the hero's
  level + 4 and its rarity rolls with the hero as the finder. Today a kit takes the place's danger and rolls rarity at
  that level (party_recruit.cpp:290, 374, 404; party_game.cpp:95). Left so, a level-10 hero who recruits in a D-30 town
  and lets the companion die would pick up item-level-30 gear rolled past question 6's gate. Revision 6 rules the cap
  ("that works for me").
- **The farming guards hold.** A sellsword's inn slot retires for good when he dies (MERC_FALLEN, party_game.cpp:240),
  the inn never stands him again (party_recruit.cpp:125), and the mark is saved (game_rpg.cpp:2659-2660, 2824). A census
  companion dies once. So each kit can drop once. The owner accepted the one-time find in revision 6.
- Sections 3, 5, 6, 8, 9, 11 and 12 follow it.

## Revision 4: the owner's answers (2026-10-28)

| # | question | the owner's answer | ships in |
|---|---|---|---|
| 1 | alloys in order | yes | Batch B (the rules update) |
| 2 | old makers' metals as relics of old places | yes | Batch B |
| 3 | rarity is finish | yes | Batch A (inside P2c) |
| 4 | every metal keeps its helmet | yes to both | Batch A |
| 5 | bulk on the old makers' ladder | grow it | Batch A |
| 6 | luck and the level gate | (b): luck lifts to Legendary only from level 17 and to Mythic only from 27; campaign finales aside | Batch B |
| 7 | shops | yes | Batch B |
| 8 | crafted looks | yes | Batch A |
| 9 | people in far lands | yes | Batch A |
| 10 | war spoils | yes | Batch B |
| 11 | companions | yes, amended: the hero may give a companion better gear (section 6) | Batch B |
| 12 | timing | yes | - |

What revision 4 changes:
- **Section 6, companions:** the owner's amendment, ruled as follows. The kit stays theirs. A given piece is worn only if
  it is better, by the compare arrow's own rule read at the companion's level, and a tie keeps their own. A declined piece
  never leaves the hero's pack. A worn gift stows their own piece with them. TAKE BACK returns a gift and puts their own
  piece back on. At every parting, gifts come back to the hero. (Revision 4 also sent gifts back at death and kept one
  keepsake at the grave. Revision 5 replaces that: a death drops everything where they fell.) The swap leak
  (partyEquip -> partyUnequip -> addItem) is closed by one checked door. Sections 3, 5, 8 and 9 follow it.
- **Section 7, under the P2c envelope.** The envelope was signed off on 2026-10-28, after this proposal was written
  (docs/PLAN.md D42-D70), and it sets the how:
  - Question 4's relic helms replace the envelope's hero-road helms at T5 and T6 (the plumed great helm and the winged
    helm, and D61's "part by the helm family"). D66 left those waiting on this question. D44's measures still bind, met
    by the onion and crystal helms' own tier views, as D68 does for the peoples' lines.
  - Question 5's growth stays inside kEnvelope's tier caps (D43) and ADDs sizes and views (D49). It does not redraw the
    drawn crystal set in place.
  - D68's open point closes: a Legendary keeps its metal's helm family.
- **Section 8:**
  - Batch A ships inside P2c. docs/PLAN.md places it in P2c's waves; PLAN's wave-2 line "Not in wave 2: the loot
    proposal's twelve questions" predates the answers.
  - The bandit chiefs' line is in game.cpp, which no wave-2 area owns, so the P2c planner assigns it.
  - The doc batch's VISION_PLAN.md part is written (2026-10-28). Its PLAN.md, STYLE_P2.md and P2C_CONTRACTS.md parts wait
    for the P2c planner, who owns those files. D27 is already closed by the envelope (D46, D48).
- **Section 11:** each question carries its answer.

## 0. What changed in revision 3

The review's items:
1. **Mythic alloys keep their alloy (high).** Today any Mythic metal piece becomes the MYTHIC style (gear_ladder.cpp:60;
   gear_forge.cpp:553; the MYTHIC row, gear_forge.cpp:738). A Mythic culture-alloy piece therefore loses its alloy's
   helmet, outline and colour. The owner ruled that every alloy always gets its own helmet. Now a Mythic alloy piece keeps
   them, and the ember tempering and the flame crown sit on top as finish. The MYTHIC style stays for bronze, iron, steel
   and the relics. Found while checking: gilded and starmetal swap their own helmets for the ladder's at T5-T6
   (doll_resolve.cpp:264-265, 277). Band-7 starmetal is never below T5, so its crystal helm is never seen in play.
   Question 4 covers both.
2. **The luck gate is now an owner question, not a plan fix.** The code lets luck beat the level gate by a whole step
   (gear.cpp:151-161). Hoards, bosses and named foes give Epic from level 5, Legendary from 12 and Mythic from 20.
   PLAN.md:1949 says level 20. VISION 15.27.4 says luck beats the gate "a little, never a lot", and a campaign's last
   reward is the main early path (VISION_PLAN.md:3077). Revision 2 proposed rewriting PLAN to match the code. That is
   withdrawn. Question 6 offers three options and recommends one.
3. **Bulk on the relic ladder.** The owner ruled that better armour gets bulkier. The relic ladder stalls (gilded and
   obsidian are both T4 with medium shoulders) and ends on slim starmetal, lighter than emberforged. STARFALL's drawing is
   kept. Question 5 proposes the largest shoulder mass for it and a size step for obsidian, or slim starmetal as an
   accepted exception.
4. **Batch B lands after P2c is DONE**, as its own batch with its own full regression, not "right after the flip".
5. **Exemptions from the shop cap.** PLAN.md:1948 lists masterwork smiths and the hidden kingdoms as sources of T5. A
   capital smith who trusts the hero, and every hidden-kingdom smith, are exempt from the warriors'-alloy cap (question 7).
6. **The masterwork look rule changes a signed-off table.** "A masterwork draws T5 only from band 4" changes PLAN.md:1933
   ("T5 at least") and old saves' looks. Question 8 says so plainly.
7. **Two-alloy kings follow D38**: crest, gold, gem and the T5 finish on the over-layer's hem; no surcoat and no full cape
   (PLAN.md:2555-2558).
8. **Bandit chiefs** get only the ladder's tier field in Batch A. Their outfit stays as it is (PLAN.md:2180-2181).
9. **Gilded's tier is stated three ways** (PLAN.md:2082, STYLE_P2.md:1265, VISION_PLAN.md:1045). The doc batch makes it
   one statement (section 8).

The gaps it named are now covered:
- the companion kit still leaks through partyEquip (party_game.cpp:871, 888) and through a fallen companion's keepsake
  (party_game.cpp:213, 741): section 6;
- the full source audit gates: section 9;
- a real DropSource for war spoils: section 5;
- the art cost against the 2.5 MB budget: section 7;
- rarity colours on the paper doll screen: section 2;
- hidden kingdoms and the focus vendor: section 6.

Also, the owner's rulings of 2026-10-25 are folded in (section 7). Batch A now sits in P2c's own wave-2 areas. And
`--ladder-pace` is P2c's tool (FOLK-SIM-C, PLAN.md:3295), which Batch B extends.

## 1. Today, and what's wrong

Every piece comes from one maker (gear.cpp:1028-1050). Its metal follows its item level's band and its maker people
(gear.cpp:552-572): leather or bronze in band 1, iron in band 2, steel in band 3, then the maker's alloy. Rarity is a
separate roll (gear.cpp:207-220) and never picks the metal. The look tier reads both (gear_ladder.cpp:36-56):
- the band sets the base tier;
- Rare adds the next tier's trim;
- Epic adds one tier, capped at T5;
- Legendary is T6, and Mythic is T6 with the flame;
- a masterwork alloy is at least T5.

There are eleven problems.
1. **The four heartland metals have no real source.**
   - GILDED, OBSIDIAN, EMBERFORGED and STARMETAL appear only on band-4+ pieces made without culture data (gear.cpp:566,
     590, 956; gear_ladder.cpp:65-69).
   - Every tile has a culture (culture_map.cpp:93-103; world_culture.cpp:19-22). So bodies, chests, shops and quests
     always have a maker (game_rpg.cpp:199-203, 349, 2095, 966).
   - In play these metals come from four places only: saga loot and the story loot hook (story.cpp:163-167, 626-633;
     story_game.cpp:56-58); companion kits' weapons and shields, which get an id but no data (party_game.cpp:102;
     gear.cpp:1052-1055); and the dragon's hoard (game_rpg.cpp:289-295).
   - A player cannot seek out any of them, so four hand-drawn hero styles are almost never worn: PARADE, KNAPPED, FURNACE
     and STARFALL (STYLE_P2.md:1489-1508).
2. **Alloys stop getting stronger.** An alloy's strength is 1.12 + 0.04 x tierStep (gear.cpp:957-959), while the band
   step climbs to 1.28 (gear.cpp:838-843). A two-alloy people whose warriors' alloy rolled tierStep 1 stays at 1.16
   through band 7, 10 % under its band (*model*).
3. **Some alloys never drop.** matFor takes the best alloy whose 3 + tierStep fits the band (gear.cpp:568-570). The
   warriors' alloy rolls tierStep 1 half the time (priors.cpp:963), takes band 4 too, and the everyday alloy never drops.
   About 41 % of ordinary peoples have only two alloys (priors.cpp:939).
4. **A crafted look can promise more than the piece gives.** Crafted gear takes the bench's danger as its item level
   (craft.cpp:357-359). A masterwork alloy draws T5 at any band (gear_ladder.cpp:53-54). Any other alloy forged below band
   4 draws T1-T3, tiers alloy styles were not made for (STYLE_P2.md:1420).
5. **Rarity stops showing at the top.** In bands 6-7, Common, Uncommon and Epic all draw T5 (gear_ladder.cpp:40-47).
   Rare's mark is the next tier's trim, which at T5 lands on the same gold slot (doll_resolve.cpp:867-869), so an Epic
   there draws plainer than a Rare. In band 1, Rare and Epic both draw T2.
6. **People don't match their land.** Soldiers, guards and captains drop nothing (game_rpg.cpp:190; war_game.cpp:281).
   "Finer soldiers in far lands" is planned but not built (PLAN.md:1966). Bandit chiefs draw T1 (looks.cpp:822;
   game.cpp:1864-1866 swaps only the outfit), though the plan says T2 (PLAN.md:1960).
7. **The UI doesn't explain metals.** The HUD line names the band's best alloy, not the piece's own metal (hud.cpp:416).
   Every piece with no maker reads "HEARTLAND WORK" (loot_paint.h:501).
8. **A Mythic alloy loses its alloy.** metalKindOf returns 11 for any Mythic metal piece (gear_ladder.cpp:60). forgeOf
   then returns the MYTHIC base (gear_forge.cpp:553), shapeOf its row (the ladder's helm, tassets, tall shoulders and a
   cape: gear_forge.cpp:738), and fillLadder skips the alloy's colour family (gear_ladder.cpp:167). A Mythic DRAGONSTEEL
   piece looks like a Mythic iron one. This breaks the owner's ruling that every alloy always gets its own helmet.
9. **Two relic metals lose their helmets at the top.** At T5-T6, PARADE and STARFALL wear the ladder's crested helm and
   winged crown (doll_resolve.cpp:264-265, 275-277; STYLE_P2.md:1489-1491, 1507). Band-7 STARMETAL is at least T5
   (gear_ladder.cpp:40), so its crystal helm never shows on a found piece. A gilded Epic and a starmetal piece share one
   helm.
10. **Bulk stalls, then shrinks, on the relic ladder.** Heartland pieces take pauldrons none, small, medium, medium,
    large, large by tier (STYLE_P2.md:1266-1267). Gilded (band 4) and obsidian (band 5) are both T4, with round and tall
    shoulders of the same medium size. Emberforged (band 6, T5) is the heaviest: broad vented shoulders and heavy arms.
    Starmetal (band 7, T5) is "slim and tall" (gear_forge.cpp:734-737; STYLE_P2.md:1504-1508). The last step reads
    lighter than the one before.
11. **Luck beats the gate by a whole step.** The allowed top is the gate's top plus one (gear.cpp:156-161), so with luck
    Legendary comes from level 12 and Mythic from 20, not 20 and 30. *Model:* in a harder place, a hoard or boss piece is
    Legendary 2.8-4 % of the time at levels 12-19. An explorer finds about 0.9 Legendaries before level 20 and 0.5 Mythics
    before level 30, outside campaign finales.

## 2. The rule

| | decided by | what you see |
|---|---|---|
| **Metal** | the item level, from the place's danger and your level (gear.cpp:226-241), and the maker | colour, outline, helmet, base strength |
| **Rarity** | luck: the source, the foe's rank and your level gate (gear.cpp:85-220) | name colour, affixes, power, finish |
| **Look tier** | both, as today, plus the marks at the top (change 4) | how fine and how bulky it is |

**Every metal keeps its own helmet.**
- A culture alloy wears its own helm family at every tier and rarity, Mythic included.
- A relic wears its own metal's helm at every tier (question 4).
- Leather, bronze, iron and steel wear their hero styles' helms or their people's own (gear_ladder.cpp:193-199).
- The Mythic's flame crown sits on the helm. It never replaces an alloy's helm.

**The metal ladder.** The hours are a *model*: the level at which ordinary finds reach each band, mapped onto VISION
1.4's table (VISION_PLAN.md:110-117).

| band | item level | ordinary finds from | a people's piece | an old maker's piece (relic) |
|---|---|---|---|---|
| 1 | 1-8 | the start | leather or bronze | none |
| 2 | 9-16 | level 9, about 2.5 h | iron | none |
| 3 | 17-24 | level 17, about 9 h | steel | none |
| 4 | 25-32 | level 25, about 19 h (PLAN.md:1947 says about 28 h) | its **everyday** alloy | GILDED |
| 5 | 33-42 | level 33, about 36 h | its **warriors'** alloy | OBSIDIAN |
| 6 | 43-52 | level 43, about 68 h | its **masterwork** (two-alloy peoples: the warriors') | EMBERFORGED |
| 7 | 53-60 | level 47-49, from bosses and named foes in D 55+ lands, about 87-95 h | the same | STARMETAL |

Braving harder places pays. A find's level runs up to 4 past the lower of your level and the danger (gear.cpp:230).
Hoards, caches, bosses and named foes add 1 to 3 more (gear.cpp:231-238). So a level-21 hero in a D-30 ruin finds band-4
pieces (*model*).

**Six changes** (sections 7 and 8 say where they go and how they are checked):
1. **Alloys in order** (problem 3).
   - The everyday alloy comes at band 4, the warriors' at band 5 and the masterwork at bands 6-7, whatever the hidden
     tierStep says. A two-alloy people keeps its warriors' alloy from band 5 on.
   - Crafting caps follow the rank: everyday band 4, warriors' band 5, masterwork band 6. Today they follow tierStep
     (gear.cpp:574-582).
2. **Strength follows the band** (problem 2).
   - From band 4 an alloy is never weaker than its band's step: max(1.12 + 0.04 x tierStep, the band step).
   - No alloy gets weaker than today. Only new pieces change, because an item's power is saved (items.cpp:293).
3. **Old makers' metals** (problem 1).
   - From band 4, part of the metal gear from old places is made with no maker. Today's maker-less band rule then names
     and draws it (gear.cpp:566, 590, 956; gear_ladder.cpp:65-69): GILDED at band 4, rising to STARMETAL at band 7.
   - It carries no culture id, so it wears the Heartland hero styles' helm, shapes and motif, not the local people's
     (gear_ladder.cpp:193-199, 215-221).
   - It needs no new field, no metal lift and no new random draw: the decision hashes draws the roll has already made.
     Section 3 gives the shares.
4. **Rarity marks at the top** (problem 5; look only).
   - A Rare piece at T5 adds the glyph panel, which today is a T6 tile (doll_resolve.cpp:877).
   - An Epic held at the T5 cap adds the glyph and an unlit gold rune inlay. A band-1 Epic adds the Rare trim.
   - Light and motion stay T6's: runes lit and breathing, motes and the edge spark. The flame stays Mythic's
     (STYLE_P2.md:1296-1312).
   - The artist picks the exact marks under one rule: each step reads finer at 1x than the step below, with no new light
     and no motion.
5. **Helmets and bulk** (problems 8-10; look only; questions 4 and 5). A Mythic alloy keeps its alloy. Relics keep their
   own helmets at every tier. Relic bulk climbs at every band. Section 7 gives the details.
6. **Explain the metal** (problem 7).
   - The item card's maker row names the rank, for example "HIGHLAND WORK: GWESTEEL, WARRIORS' METAL (2 OF 3)".
   - A relic reads "OLD MAKERS' WORK: OBSIDIAN (2 OF 4)" (loot_paint.h:500-505).
   - The HUD line shows the piece's own metal (matWord) instead of the band's best (hud.cpp:416).
   - "Heartland" leaves the UI, because a heartland-born hero's own people have generated alloys of their own
     (priors.cpp:1178).

**What rarity adds** (gear.cpp:58-69; gear_ladder.cpp:36-56; doll_resolve.cpp:859-879)

| rarity | power; affixes | on the hero |
|---|---|---|
| Common, Uncommon | x1.00 / x1.27; 0 / 1 | the band's tier (T1 is patched) |
| Rare | x1.50; 2 | the next tier's trim (band 1: a clean T2); at T5, plus the glyph (new) |
| Epic | x1.60; 3 | one tier finer and bulkier, in the same metal; band 1: T2 plus trim (new); at the T5 cap, plus the glyph and the gold inlay (new) |
| Legendary | x1.64; 3 and a power | T6, in its own metal's style and helmet: lit, breathing runes, motes and the edge spark |
| Mythic | x1.68; 4 and a Mythic power | a culture alloy: its own helmet, outline and colour, with ember-lit seams and rims, the ember collar, the flame crown on its helmet and the flame (new); bronze, iron, steel and relics: the MYTHIC style and the flame |

**Three cues, one channel each.**
- The metal, with its colour, outline and helmet, says where a piece was made.
- The name colour, the frame and the light on the ground say how lucky the find was (items.cpp:112-117;
  loot_ui.cpp:250-276).
- Light says the tier: a faintly breathing gem is T5; lit runes, motes and sparks are T6; the flame is Mythic
  (STYLE_P2.md:1199-1208, 1296-1309).

Rarity never tints the drawn figure's metal, and the flame is the only rarity mark on it. This is about the figure only.
Every rarity colour stays on the paper doll screen, on the names, slot frames and icon borders, as VISION 15.27.1 requires
(VISION_PLAN.md:3062; paperdoll.cpp:147, 171). The shop, chest and forge screens keep theirs too.

**The invariant.** In every band, Common looks the same as Uncommon, and each step from Uncommon to Mythic looks finer
than the one below. `--doll-ladder` and `--ladder-pace` check it (section 9).

## 3. Where armour comes from

"Relics" counts only metal kinds (weapons, and armour other than cloaks), and only from band 4.

| source | item level | rarity | maker | relic share |
|---|---|---|---|---|
| a person's body (bandits, raiders) | the danger and your level; named foes +3, bosses +2 (gear.cpp:226-241) | body odds; a named foe gives Rare or better (game_rpg.cpp:207-213) | their own people (game_rpg.cpp:198-203) | none |
| an undead body | as a body | a loot roll 15 % of the time (game_rpg.cpp:234) | the land's | half inside a ruin or shrine; none elsewhere |
| a dungeon boss | +2 | a boss roll, a step of luck (game_rpg.cpp:211, 273) | the land's | half inside a ruin or shrine |
| a plain chest | as a body | +1 luck (gear.cpp:166) | the land's or the dungeon's (game_rpg.cpp:349) | none |
| a chest or hoard in a cave or bandit camp | +1 for a hoard | a hoard gets +6 luck and a step; its first piece is a boss roll (game_rpg.cpp:359-360, 423-426) | the dungeon's | none |
| a cache or hoard in a ruin or shrine | +1 | +5 or +6 luck and a step (game_rpg.cpp:423-424) | the dungeon's | **half** |
| an outdoor cache at a wonder, ruin or shrine | +1 | +5 luck and a step (game_rpg.cpp:432) | the land's | **half** (a wayside cache: none) |
| a dragon lair's hoard | +1 | as a hoard | none | **all** |
| a world boss | +2 | Epic or better (game_rpg.cpp:210, 285) | none | **all** |
| a shop shelf | the settlement's danger, +/- 2 (game_rpg.cpp:2103) | Rare at most; Epic in a capital where you are Honoured (game_rpg.cpp:2097-2111) | the settlement's, up to its warriors' alloy; hidden-kingdom smiths uncapped (new) | none |
| a smith's commission | the settlement's danger (craft_game.cpp:193, 200) | Uncommon to Rare (craft_game.cpp:197-198) | the settlement's, with the same cap; a trusting capital smith and hidden-kingdom smiths uncapped (new) | none |
| your forge | the bench's danger, capped by the metal (craft.cpp:357-359) | a skill roll, Epic at most (craft.cpp:325-335) | the recipe's people | none |
| a quest | the quest's danger (game_rpg.cpp:961-965) | half the time an item, Uncommon to Epic (game_rpg.cpp:960, 975-976) | the giver's people | none |
| a saga or campaign | the story's danger | Uncommon or better; a campaign finale gives a Legendary (story.cpp:108-131) | the hook's people (new; today none: story.cpp:163-167) | none |
| a companion's kit | made once, at their first join (ruled in revision 5), and kept through every parting and return (revision 6): the place's danger, as today (party_game.cpp:91; party_recruit.cpp:290, 374, 404), capped at your level + 4 like any find (new; ruled in revision 6) | as a find with you as the finder, so question 6's gate reads you (new, ruled in revision 6; today rolled at the kit's own level, party_game.cpp:95-97) | their own people (new; today an id with no data) | none; the kit stays theirs while they live and never reaches your pack as kit (section 6) |
| a fallen companion's pile (revision 5) | as each piece was made | as made | as made | none: everything they carried (their kit, worn and stowed, your gifts and their belongings) lies where they fell as ground loot (game_rpg.cpp:217-221), each kit piece without IF_KIT; the grave holds nothing to take (section 6) |
| war spoils (new) | a body roll: D the siege's level, your level the finder's, so the +4 cap holds | body odds, gated by your level; a captain an elite's luck and Uncommon or better | the soldier's realm's people | none |
| a hidden land's mage (the focus vendor) | your level + 2 (magic_learn.cpp:441) | Rare | the tradition's people | none: a focus, not armour; untouched |

**How often** (*model*). This is an ordinary body find, gear only, at a danger equal to your level. The odds are
percentages in the order Common / Uncommon / Rare / Epic / Legendary / Mythic.

| your level (VISION 1.4 hours) | band | odds |
|---|---|---|
| 6 (1 h) | 1 | 76 / 22 / 2 / 0 / 0 / 0 |
| 10 (3 h) | 2 | 69.5 / 25 / 5.5 / 0 / 0 / 0 |
| 14 (6 h) | 2 | 65 / 25 / 9 / 1.2 / 0 / 0 |
| 18 (10 h) | 3 | 61 / 25 / 11 / 3 / 0 / 0 |
| 22 (15 h) | 3 | 59 / 25 / 11 / 4.8 / 0.4 / 0 |
| 26 (21 h) | 4 | 58 / 25 / 11 / 5.5 / 0.8 / 0 |
| 30 (28 h) | 4 | 57 / 25 / 11 / 5.5 / 1.2 / 0.1 |
| 35 (42 h) | 5 | 57 / 25 / 11 / 5.5 / 1.7 / 0.2 |
| 40 (55 h) | 5 | 56 / 25 / 11 / 5.5 / 2.2 / 0.3 |
| 45-50 (78-100 h) | 6 | 55 / 25 / 11 / 5.5 / 2.5 / 0.5-0.7 |

- **Luck and the level gate** (question 6).
  - Today luck beats the gate by one whole step (gear.cpp:151-161). With luck, Epic comes from level 5, Legendary from
    12 and Mythic from 20, from hoards, caches, bosses and named foes, best in a harder place.
  - Ordinary finds wait for the floors: Epic at 12, Legendary at 20, Mythic at 30 (gear.cpp:85-89).
  - A campaign finale is ungated: always a Legendary, with a Mythic chance from level 20 (gear.cpp:120-122, 181-184). The
    dragon's two named Legendaries are a set piece (game_rpg.cpp:286-296).
  - *Model*, per piece from the best sources in a harder place (D = your level + 3):

| rule | Legendary | Mythic | before level 20 / 30 for an explorer |
|---|---|---|---|
| today: one step | from 12: hoard or boss 2.8-4.1 %, ruin cache 2.1-3.3 %, named foe 0.5-1.4 % | from 20: hoard or boss 0.7-1.2 % | about 0.9 Legendaries / 0.5 Mythics |
| recommended: luck's lift reaches Epic at any level, but Legendary and Mythic only within 3 levels of their floors | from 17: hoard or boss 3.7 % | from 27: hoard or boss 1.1 % | about 0.4 / 0.2 |
| strict floors | from 20 | from 30 | 0 / 0 |

  - The explorer is one dungeon an hour (a boss and a two-piece hoard), half a ruin cache, a fifth of a world boss's
    drops and 0.3 named foes, outside campaign finales (`luck_hours.py`).
  - PLAN.md:1949 ("level 20 at the earliest") changes only to the owner's answer.
- From level 20, hoards and caches roll Rare about 16 % of the time and Epic about 8 % (*model*).
- **Armour per event** (*model*). Gear is 36 % of a body or plain-chest roll, and armour is 33 % of gear
  (gear.cpp:1062-1085). Bigger sources always give gear (gear.cpp:1060-1061).
  - A person gives armour 4.2 % of the time, an elite 12 %, a boss 33 % and an undead 1.8 %.
  - A plain chest holds 0.18 pieces, a dungeon chest 0.31, a hoard or ruin cache 0.66 and an outdoor cache 0.5.
  - A world boss gives 0.66, plus weapons. A quest gives 0.3.
  - A smith restocks 5 armour pieces every 2 days (game_rpg.cpp:2091, 2155, 2158).
- **Slots.**
  - Ordinary armour: body 30 %, helm 16, shield 14, gloves 13, boots 13, cloak 14 (items.cpp:206-214).
  - A relic reads the same draw through its own table: body 40, helm 25, shield 15, gloves 10, boots 10, and never a
    cloak, because cloaks are cloth (gear.cpp:554). Relics favour the slots the doll shows best (section 4).
- **Relic yield** (*model*).
  - The assumed explorer's hour at band 4+ (`--ladder-pace` prints and measures it): 40 people killed; 15 undead, 5 of
    them in ruins; 6 plain chests; one dungeon (a ruin or shrine one time in three) with 3 chests, a hoard and a boss;
    half an outdoor old-place cache; a tenth of a world boss; 2 quests.
  - That hour gives about 6 armour drops, 0.7 of them relics: about 1 armour drop in 8, or 1 metal armour drop in 7. It
    also gives about 0.9 relic weapons. With one shop purchase an hour, relics are about 1 in 10 of all armour acquired.
  - Over band 4 (about 17 hours), that is roughly a dozen relic armour pieces, about five of them body pieces, and as
    many weapons.
  - A relic is exactly as strong as a floored alloy of its band, so choosing one is taste, not power. Tune the old places'
    "half" after `--ladder-pace` measures it.

## 4. The matrix: band x rarity gives the metal and look

| band | Common, Uncommon | Rare | Epic | Legendary | Mythic |
|---|---|---|---|---|---|
| 1 | leather or bronze, T1 | same, T2 | same, T2 + trim (new) | same, T6 | bronze: MYTHIC style, T6 + flame; leather: its own style, T6 + flame |
| 2 | iron, T2 | T2 + trim | T3 | T6 | MYTHIC style |
| 3 | steel, T3 | T3 + trim | T4 | T6 | MYTHIC style |
| 4 | everyday alloy or GILDED, T4 | T4 + gold trim | T5 | T6 | alloy: its own look + ember + flame crown (new); GILDED: MYTHIC style |
| 5 | warriors' alloy or OBSIDIAN, T4 (obsidian's shoulders a size larger, new) | T4 + gold trim | T5 | T6 | as band 4 |
| 6 | masterwork (two-alloy peoples: warriors') or EMBERFORGED, T5 | T5 + glyph (new) | T5 + glyph + inlay (new) | T6 | as band 4 |
| 7 | the same, or STARMETAL (the largest shoulders, new), T5 | T5 + glyph | T5 + glyph + inlay | T6 | as band 4 |

- **The metal never changes along a row.** Only the finish does, and the helmet stays the metal's own.
- **Legendary** keeps its metal's style and helmet with T6's finish (PLAN.md:2533). **Mythic**: section 7.
- **The Rare trim.** At T4 it turns from brass to gold (doll_resolve.cpp:867-869). At T5 it needs the new glyph to show.
- **What shows on the hero.**
  - The body piece sets the whole armour's colour and the body's outline; the helm sets the head's outline
    (gear_ladder.cpp:159-164, 204-212; PLAN.md:2490-2491, D15).
  - A shield, gloves or boots show their own small shapes in the body's colour (PLAN.md:2531, D31).
  - A weapon shows its own metal (PLAN.md:2529, D30). Every piece shows its metal in its icon (gear_look.cpp:428-442).

**Examples.** These use Highland seed 1 from the boards: THORNIRON is the everyday alloy, GWESTEEL the warriors' and
THORNSTEEL the masterwork (PLAN.md:2963, 3117).
- **New hero** (level 1-8, at home).
  - A bandit drops a leather jerkin (T1, patched). The village smith sells a bronze kettle hat.
  - A chest at level 6 gives a Rare leather jerkin (T2, clean).
  - A lucky hoard at level 7 gives an Epic bronze piece, T2 with brass trim.
- **Middle game** (level 18-24, in steel lands).
  - The city smith sells steel mail (T3).
  - A dungeon boss drops a purple STEEL SCALE: Epic, T4, etched, with broader shoulders and a crest.
  - At level 22, the hero braves a D-30 Highland ruin. Its hoard gives a THORNIRON BRIGANDINE (band 4, T4), a land early,
    and a GILDED helm made by the old makers, in its own onion shape.
- **Late game** (level 30-50).
  - A band-6 Highland city still sells GWESTEEL (T5). THORNSTEEL comes from drops, the hero's forge, or a commission from
    a Highland capital smith who trusts the hero.
  - A ruin cache gives OBSIDIAN PLATE (T4, black glass, large shard shoulders).
  - Band-6 Highland drops are THORNSTEEL (T5). It is the only Highland alloy with a full cape (PLAN.md:2555-2558, D38). A
    Rare one shows the glyph, an Epic one the gold inlay, and a Legendary one lights its runes.
  - A Mythic THORNSTEEL breastplate keeps THORNSTEEL's helm, cape and outline. Its seams and rims glow ember, and the
    flame crown rises from its helm.
  - A world boss gives EMBERFORGED gauntlets in their furnace shape, drawn in the body's colour.
  - STARMETAL waits in D 55+ lands for heroes of level 47 or more: the crystal helm and the biggest crystal shoulders.

## 5. People: what they wear and what they drop

A far land is a place of band 4 or more, meaning danger 25 or more.

| who | at home (bands 1-3) | in far lands | drops |
|---|---|---|---|
| bandits, pirates | T1, the local poor kit, never an alloy (looks.cpp:626-631, 822) | T2 | gold, and a loot roll 35 % of the time, as today (game_rpg.cpp:223-227) |
| bandit chiefs | T2 (new, as PLAN.md:1960 plans): the ladder's tier field only, in the plate outfit game.cpp:1866 already gives them | T3 | a boss roll, as today (game_rpg.cpp:224-225) |
| village watch, soldiers | T2, their people's own metal in livery (looks.cpp:432-438, 510) | T3, still no alloy | nothing; foes at a siege you joined give spoils (below) |
| capital watch, heralds, envoys, warlords | T3 | T3 | nothing |
| captains, royal guard, jarls | T4, the realm's warriors' alloy (PLAN.md:2526-2528, D29; gear_ladder.cpp:265-277) | the same | an enemy captain at a siege you joined gives spoils |
| kings | T5, the masterwork. A two-alloy realm's king wears the warriors' alloy at T5: crest, gold trim, gem, and the T5 finish on his over-layer's hem; no surcoat and no full cape (D38: PLAN.md:2555-2558; gear_ladder.cpp:267) | the same | nothing |

Near home, the officers wear a metal the hero will only find at band 5, which works as a quiet promise. In a far land the
soldiers stay out of alloys, because alloys start at T4 (STYLE_P2.md:1420; gear_ladder.cpp:265). Their officers wear the
warriors' alloy and their king the masterwork, while the land's drops come in its everyday alloy.

**War spoils** (new)
- **Who.** Only foes at a siege where you hold a War quest on the other side. Their deaths already reach Game::warKill
  (war_game.cpp:892-903).
- **How many.**
  - A soldier gives a piece 10 % of the time, and a captain 50 % of the time.
  - The cap is 3 per siege. The count lives in two free bits of that War quest's saved flags, bits 18-19 (game.h:342,
    351-352). So a reload, a walk away or a fresh wave cannot reset it. Soldiers refill every 7 s (war_game.cpp:375),
    camps despawn past 70 tiles (war_game.cpp:340), and the defenders' captain returns every 240 s (war_game.cpp:417).
- **The roll.** A spoil is a body roll through rollDropC. Its DropSource has src Body, D the siege's level, playerLevel
  the hero's level, and culture the soldier's realm's people. A captain adds an elite's luck (rank 1) and atLeast
  Uncommon. So the item level stays within 4 of the lower of the hero's level and the siege's (gear.cpp:230), and the
  rarity gate reads the hero (gear.cpp:151-161). A low-level hero at a high-level siege gets nothing he could not get
  from a bandit there.
- **Where the numbers live.** Today spawnMan sets a.level = 10 for everyone and no culture (war_game.cpp:278-296). It will
  set Actor::dropD to the level it receives (the zone's level; captains +1) and Actor::culture to the realm's people
  (game.h:168-181). warKill reads both.
- **What stays the same.**
  - Joining through a service order without a War quest gives no spoils, so there is one saved counter.
  - The War quest's own reward stays: half the time an item, with a step of luck for war (game_rpg.cpp:960, 978).
- **Audited.** `--loot-sources` gains a WAR SPOILS row of its own, as START WEAPON has (section 9).

**Anti-farming**
- **No loot from friends.** Friendly guards, townsfolk and soldiers drop nothing, and killing them is a crime
  (game_rpg.cpp:190).
- **No loot from summons, raised thralls or decoys** (new).
  - Today, when foes kill one of these, it rolls loot. Summons and thralls roll the undead's loot; a decoy rolls a
    person's gold and 35 % loot roll.
  - The cause: they are not NPCs (magic_game.cpp:496, 2063, 2597), and Game::kill calls dropLoot (game.cpp:1481;
    game_rpg.cpp:190, 223-234).
  - The fix: skip dropLoot for magicTag MT_SUMMON and MT_DECOY (magic.h:293-294). A plausible faucet, not run.
- **Undead relics only in old places.** Undead count as relic sources only inside ruins and shrines, never in dens, which
  refill after 3 days (game.cpp:2326).
- **Companion kits and commissions.** A companion's kit never enters the hero's pack while they live. What the hero
  gives stays the hero's and comes back at a parting. At a death everything they carried drops where they fell, the kit
  as ordinary gear (revision 5), and each kit can drop only once: a dead sellsword's inn slot retires for good
  (MERC_FALLEN, party_game.cpp:240; party_recruit.cpp:125), and a census companion dies once. The kit's item level is
  capped at the hero's level + 4 and its rarity rolls with the hero as the finder, so the pile holds nothing a find there
  could not. What is left is a one-time find per companion, which the owner accepted in revision 6 ("thats fine if
  someone wants to farm that way they can but there are defintly better avenues to make money"). A warrior sellsword's
  Common kit sells for about 2.5 times his first day's wage (*model*: 176 gold against a 70-gold hire at a D-10 inn),
  and a healer's for less than his wage. A census companion's kit costs no gold, but the friend is gone for good and
  their kin grieve. A companion who parts and comes back brings the same kit, never a second one (revision 6). A
  sellsword who parts waits a week, and a commission is one piece per restock (section 6).
- **Kept as today.** Easy land pays poorly (gear.cpp:188, 230). Each chest is looted once (game_rpg.cpp:307-308). Shelves
  restock every 2 days, with a capped purse (game_rpg.cpp:2091; gear.cpp:294-297; gear.h:274-275).

## 6. Sources that need care

**Companions.**
- **Today.**
  - A kit is made by makeGear with the people's id but no data (party_game.cpp:102; gear.cpp:1052-1055). Its weapons and
    shields get heartland metal words, the doll draws the people's forms, and the armour stays iron or leather forever
    (party_game.cpp:118-121).
- **Proposal.** Build kits with makeGearC and the companion's own culture, so a kit is their people's make and climbs the
  same metal ladder. Warriors wear iron up to band 2, then their band's metal.
- **Six loops to close first**, because this change makes kits richer:
  1. A dismissed sellsword's hire mark is erased (party_game.cpp:919), so the 7-day wait never applies
     (party_recruit.cpp:125). A rehire runs the same kit again from the same seed (party_game.cpp:90;
     party_recruit.cpp:290).
  2. TAKE BACK on the paper doll (paperdoll.cpp:962) calls partyUnequip, which puts the piece in the hero's pack
     (party_game.cpp:880-888). From there it sells (game_rpg.cpp:2303) and breaks down to teach the people's patterns
     (craft_game.cpp:100-105, 243-253).
  3. **The swap.** Giving a companion a piece for a slot they already fill calls partyUnequip first (party_game.cpp:871),
     so the kit piece lands in the hero's pack anyway (party_game.cpp:888). Refusing only at the TAKE BACK button would
     leave this open.
  4. **The grave.** A fallen companion's whole pack becomes their belongings (party_game.cpp:213), and the keepsake is its
     first piece (party_game.cpp:739-744). That is usually the kit's weapon.
  5. **Gifts lost at parting** (found in revision 4). partyDismiss removes the member and their whole pack
     (party_game.cpp:914-922), so gear the hero gave leaves with them. At death the gifts become belongings, and the hero
     can take back only one piece, as the keepsake.
  6. **A parting forgets them** (found in revision 6). partyDismiss removes the whole record, not only the pack
     (party_game.cpp:914-922): bond and the skills it opened, approval, memories, days and kills, the MF flags, the VOICE
     fields (the personal quest's spec, stage and talk bits), the horse, the fixed look and the id. An unpaid sellsword
     (party_game.cpp:352), a personal quest's farewell (story.cpp:815) and a sellsword's angry leave (party_game.cpp:320)
     go the same way. Asked again, they are built anew (party_recruit.cpp:359-375; a sellsword through stranger(),
     party_recruit.cpp:250-272), and their kit is made again (party_recruit.cpp:290, 374). That drops an unhappy
     companion's low approval, and MF_QUEST_DONE, so a finished personal quest can be offered again
     (party_talk.cpp:200). Only a census companion who leaves in anger stays a member, as State::Left with the record
     whole, until the apology (party_game.cpp:257-260, 785, 823).
- **The owner's amendment (2026-10-28):** "we probably will need to allow for players to give their companions better
  armor than what they start with though, so maybe you can't take their defaults but you can give them better armor and
  they will equip what's better." Ruled:
  1. **Their kit.** It is made once, when they join (ruled in revision 5: "Made once"), in their own people's make, so it
     climbs the metal ladder the way any find does: a companion who joins in band-4 lands, with a hero near that band,
     arrives in their people's everyday alloy. Every piece carries IF_KIT.
     - It is never remade as they level, because better gear is the hero's to give.
     - Its item level is the place's danger, as today, capped at the hero's level + 4 like any find. Its rarity rolls
       with the hero as the finder: DropSource playerLevel is the hero's level, not the kit's own. A death drops the kit
       as ordinary gear (5), so it may hold nothing a find there could not. At the join this costs the companion
       nothing, because level sync already uses a piece at no more than their level + 4 (effLevel, gear.cpp:36), which
       is the hero's + 3. Ruled in revision 6 ("that works for me").
     - While they live, no kit piece can reach the hero's pack, whether by TAKE BACK, a swap or a parting, and none can
       be sold or broken down. TAKE BACK on a kit piece is greyed, with the companion's line ("THAT'S MINE. I CAME WITH
       IT.").
     - A companion who parts while living and later comes back is the same companion (ruled in revision 6: "they arent
       new"). They come back in the kit they left in, which is never made again, with their level and their record
       (rule 8). Revision 5 read a return as a new join, with the kit made again from its seed; the owner reversed it.
  2. **Giving.** The hero offers any piece the companion can wear: armour, a weapon, a ring or an amulet.
     - They wear it only if it is better than what they wear in that slot. "Better" is measured: gear::powerScore at their
       level (party::levelFor, the level their gear is synced to in PartyOps::body) more than 3 % over the worn piece's.
     - This is the compare arrow's own rule (lootp::verdict, loot_paint.h), moved into gear.h so the sim and the arrow
       share it.
     - An empty slot always takes the piece. A piece within 3 % is a tie, and they keep their own.
     - A worse piece is declined in their own voice (a gear line in banter.cpp, for example "MINE SERVES ME BETTER."), and
       nothing leaves the hero's pack.
     - The companion page's arrow is read at their level too, so a green arrow is a promise they will take it.
  3. **A worn gift.** Their own piece for that slot is stowed with them: it stays in their pack, unworn, with IF_KIT kept.
     It is never moved to the hero. A gift displaced by a better gift goes back to the hero.
  4. **Taking it back.** A gift stays the hero's. TAKE BACK returns it, and the companion puts their stowed piece for that
     slot back on, or leaves the slot empty if they had none. Only what the hero gave can come back, which closes
     strip-and-sell.
  5. **Parting and death.**
     - At every parting (dismissed, unpaid, or leaving in anger), the companion hands every gift back into the hero's pack
       at that moment, and puts their own stowed piece back on in each slot a gift held, as TAKE BACK does, so they
       leave in their own kit. An alert names how many gifts came back. The pack has no cap on gear (Game::addItem).
     - Their kit, and the potions and food they were given, leave with them and come back with them (rule 8).
     - A companion sent home is still a member, and the hero can take gifts back from the party page.
     - **At death they drop everything** (ruled in revision 5: "if they die they drop everything"). Everything they
       carry (their kit, worn and stowed, the hero's gifts, and their belongings) drops on the ground where they fell, as
       the game's own ground loot: one Pickup per item (game.h:245-254), laid as a body's loot is (game_rpg.cpp:217-221).
       It keeps that loot's rules. It flies to the hero within 40 px and is taken at 9 px (game.cpp:1710-1732), a potion
       waits while the belt is full (game.cpp:1715-1722), and it lies for 240 s (game.cpp:1734). It is never saved, so it
       is gone once the hero changes map (clearNonPlayer, game.cpp:1946) or loads (game.cpp:83). Each kit piece drops
       without IF_KIT, as ordinary gear that sells and breaks down. Nothing goes into the hero's pack at the death itself.
       The owner confirmed these rules, and the raid at home below, in revision 6 ("yep thats good").
     - Where they fell is the body that went down. That is their party body when they bleed out or are struck down
       (party_game.cpp:398, 543), or, for a death in their census life on the hero's map, their census body, which drops
       nothing of its own (game_rpg.cpp:190). A downed companion carried through a door dies on the hero's map too
       (party_move.cpp:276-285). Only a death off the hero's map has no ground: a companion at home carried off in a raid
       (life_raids.cpp:31-38) whose rescue then fails (quests.cpp:1501-1502). What they carried is lost with them.
  6. **The grave** holds no keepsake. Everything they had lies where they fell, so `belongings` stays empty and the grave
     offers nothing to take (the choice already hides itself, party_talk.cpp:443). The grave keeps the rest of the
     remembrance: their people's stone with their weapon laid on it (a look, party_game.cpp:218), the epitaph, flowers
     and words, their kin's grief (party_game.cpp:229) and the party page's roll of the fallen. A sellsword's inn slot
     still retires for good when he dies (party_game.cpp:240; the inn skips it, party_recruit.cpp:125), and a census
     companion dies once, so no kit can be farmed by letting its owner die again. The one-time find per companion that
     is left was accepted by the owner in revision 6.
  7. **Sellswords.** Every living parting sets his hire mark to that day: dismissal and an unpaid sellsword's leaving,
     instead of erasing it (party_game.cpp:919), and his angry leave, which today leaves the mark at his hire day, so his
     slot can stand him again at once. So the week's wait applies (party_recruit.cpp:124-125). A rehire brings back the
     same man, with his record and the kit he left with (rule 8), for a day's wage up front, as any hire. Nothing is made
     again, so nothing doubles.
  8. **Leaving and coming back** (ruled in revision 6: "well if they re join they should stay at the level they left, and
     shouldnt get treated as a new join since they arent new.").
     - **Who.** Every companion who parts while living and can be met again: a census companion who is dismissed or sent
       off by their personal quest's end, and a sellsword who is dismissed, unpaid or leaves in anger. A census companion
       who leaves in anger already stays a member, as State::Left with the record whole (loop 6), and goes on so. Those
       with nowhere to be met again (the animal, a saga's ally, a stranger rescued with no census home) go as today,
       with nothing kept (party_game.cpp:318-320).
     - **Where they wait.** Their record waits whole in `Party::parted`, a new roll in the party block beside the fallen.
       It is kept in a member's saved form (writeMember and readMember, party.cpp:308-383), so the runtime fields come
       back fresh, as after a load. The census can't hold it: it is rebuilt from seeds and saves only a few flags per
       resident (life.h:119-120, 368-369). A census companion is found again by their npc id, as partyOffer matches
       members (party_recruit.cpp:217); a sellsword by his inn and slot, as the inn does (party_recruit.cpp:123-124).
       Their body lives on as today: a census companion in their own life at home (party_game.cpp:918), and a sellsword
       at his inn table once his week is out, wearing the kit he left with.
     - **What comes back.** The same id, so a personal quest's story finds them again (story.cpp:547-551); their bond and
       the skills it opened; approval, memories, flags (MF_LEFT_ONCE, MF_QUEST_DONE), days, kills and the VOICE fields;
       the horse the hero bought them; their fixed look; and their kit, worn as they left, with the potions and food
       they were given. PartyOps::kit does not run. Health is full and leftDay is cleared. lastDay is set to that day,
       so bond does not grow for the days away (daily, party_game.cpp:329-342). The alert reads "JOINS YOU AGAIN."
     - **Their level** is never stored. It tracks the hero's, one below (VISION 9, VISION_PLAN.md:1342; party::levelFor,
       party.cpp:216), and their page shows it (paperdoll.cpp:766). It can't fall while they are away, so they come back
       at the hero's level less one, never below the level they left at. Their kit keeps the item levels it was made at
       ("Made once"), and the hero's gifts carry them on. The plan reads "stay at the level they left" as "nothing they
       earned resets". A level frozen while they are away would need a stored level and a catch-up rule, and would bring
       them back weaker than a stranger hired that day, so the owner confirms the reading (VISION 15.37 item 8).
     - **Whether they come** is unchanged. A census companion still needs what any recruit needs: alive, not a key
       person, of age, off duty, room in the party and in the records, and a reason to come (befriended, helped, or a
       regular at the inn: party_recruit.cpp:183-245). A sellsword needs his week, room and his wage. Only who comes
       changes: themselves. (Joining never befriends a census person: life_game.cpp:4921 is the only call. So a raid
       captive who joined as Rescued may have no reason left, though their farewell says "YOU KNOW WHERE TO FIND ME",
       party_talk.cpp:961-962. The plan keeps the rule: VISION 15.37 item 10.)
     - **Angry leaves.** A census companion who left in anger comes back as today, by the apology: refused the first
       time and taken on a later day (party_talk.cpp:622-656), which lifts approval to at least -30 (:647). No bond bar
       applies, and bond is untouched. A sellsword who left in anger now keeps his record, approval included. Rehired as
       he is, he would walk out at the next deed (party_game.cpp:312-321). So his table says ANGRY with the existing line
       ("WITH YOU? AFTER WHAT YOU DID? YOU OWE ME AN APOLOGY FIRST.", party_talk.cpp:497) until the same apology is made
       there, and then he takes his wage (VISION 15.37 item 9).
     - **Death while away.** A census companion who dies in their own life after parting is a census death, as today:
       their kin grieve, nothing drops (a census body drops nothing, game_rpg.cpp:190) and no grave is laid. Their record
       leaves the roll. A parted sellsword stands at his table as any sellsword for hire does today, and what befalls
       that actor is unchanged.
     - **Old saves.** Anyone dismissed before Batch B has no record, so they join anew, once. A companion travelling in
       an old save has no IF_KIT on their gear, so at their first parting all of it comes back to the hero as gifts (the
       accepted rule below), and their record keeps no kit. If they come back holding no kit piece, which only such a
       companion can, their kit is made then, once, as a first join's is.
     - **The roll's size.** MAX_PARTED is 32. A record is at most about 1.2 KB: about 0.4 KB of identity, standing,
       history and look; up to four kit pieces and a few kinds of potion and food at about 75 bytes an item
       (items.cpp:291-298); and up to 24 memories at 14 bytes (party.cpp:324). A full roll adds at most about 38 KB to a
       save. MAX_RECORDS (8) still counts only members, so parting still frees a place, as the RECORDS refusal asks
       (party_talk.cpp:490). Past 32 partings, the friend parted longest ago is forgotten and would join anew.
- **The fix in code** (Batch B). One item flag uses a free bit of the flags byte, which is already saved (items.h:111;
  items.cpp:297, 309): IF_KIT (16). Revision 4's IF_KEEPSAKE (32) is dropped in revision 5, since no grave keeps one.
  - **PartyOps::kit** (party_game.cpp:88-122) caps the item level at the hero's level + 4 (:91) and rolls rarity with
    DropSource playerLevel the hero's level (:95). Every recruit path keeps passing the level it passes today.
  - **partyEquip** (party_game.cpp:858-878):
    - It reads the verdict first. A decline returns false with the companion's line, and nothing moves.
    - On accept it no longer calls partyUnequip, which is the leak at :871.
    - A worn IF_KIT piece is stowed in place: it stays in the companion's pack, and only `eq` moves.
    - A worn gift goes back through the door below.
  - **One door** from a companion to the hero's pack: one helper that refuses any IF_KIT piece. Every move goes through it:
    - TAKE BACK (partyUnequip, called from paperdoll.cpp's party page), which then re-wears the stowed kit piece of that
      slot;
    - the gift-for-gift swap;
    - every parting (partyDismiss, and the angry leave in PartyOps::leave), which also re-wears the stowed kit piece of
      each slot a gift held.
  - **A death** (PartyOps::dieForGood) moves nothing into the hero's pack. In place of `f.belongings = m.inv`
    (party_game.cpp:213), and after the grave record reads their look (:218), it lays their whole pack on the ground.
    That is one Pickup per item at the body that went down, set round it by a hash of each item's seed so there is no
    new random draw, each with IF_KIT cleared. With no body on the hero's map it lays nothing.
  - **The grave** (partyGraveAct, act 2) is unchanged. It acts only on a grave from an old save that still holds
    belongings.
  - Selling (Game::sell, game_rpg.cpp:2303) and breaking down (CraftOps::studyable, craft_game.cpp:100-105, 226-253)
    refuse IF_KIT, which is the second lock.
  - **Partings and returns** (rule 8, revision 6):
    - `Party::parted` and MAX_PARTED (party.h), written after the fallen roll as the members are, each record
      length-prefixed. Party::VERSION steps to 5, and deserialize still reads 4 with an empty roll. Today it refuses any
      version but its own (party.cpp:431), and a block its owner can't read refuses the whole save (game_rpg.cpp:2826),
      so without this save_v23 would stop loading.
    - partyDismiss, and a sellsword's angry leave (party_game.cpp:320), hand the gifts back through the door and re-wear
      the stowed kit, set a sellsword's hire mark to that day, and then move the record into the roll instead of
      dropping it.
    - partyOffer and partyRecruit (party_recruit.cpp:183-388) look in the roll after the members. A found record moves
      back into the party in place of a new Member, with no PartyOps::kit call unless it holds no IF_KIT piece (old
      saves). lastDay, leftDay and health are set as rule 8 says.
    - The angry sellsword's ANGRY offer and the apology at his table run in party_talk.cpp, through the census
      companion's own steps (party_talk.cpp:622-656).
    - The inn stands a parted sellsword in his record's look and kit (PartyOps::mapChanged, party_recruit.cpp:105-156).
    - A parted census companion's record leaves the roll when the census shows them dead (RF_DEAD), beside
      PartyOps::censusDeaths (party_game.cpp:266-273).
  - **Old saves.** A kit made before Batch B has no IF_KIT, so it counts as a gift, once. This is accepted: in an old save
    a gift can't be told from a kit, and locking every worn piece would trap the hero's own gifts. A grave laid before
    Batch B keeps its belongings, and its one keepsake works as today (party_game.cpp:739-744). A companion dismissed
    before Batch B has no kept record and joins anew, once; one who parts after it holding no kit piece gets their kit
    at their return (rule 8).

**Crafting.**
- Crafted gear keeps the bench's danger as its level, capped by the metal's rank (craft.cpp:357-359).
- An alloy piece draws at least T4, the tier alloy art is made for (STYLE_P2.md:1195, 1420).
- A masterwork draws T5 only from band 4 (today: at any band, gear_ladder.cpp:53-54). So a masterwork forged at a D-10
  bench draws T4. It never draws as patched T1, and never as a T5 it cannot back up.
- **This changes a signed-off table.** PLAN.md:1933 says a culture's masterwork alloy is T5 at least. Old saves change
  too: a masterwork forged in bands 1-3 drops from T5 to T4 on the hero, and other alloys forged there rise to T4.
  Question 8 asks.

**Shops and commissions.**
- Shelves and commissions stop at the warriors' alloy (game_rpg.cpp:2102-2113; craft_game.cpp:181-200), with two
  exemptions, because PLAN.md:1948 names "masterwork smiths ... the hidden kingdoms" as T5 sources:
  - a capital's smith of the alloy's own people, once he trusts the hero with his patterns (trust 60: craft.h:171). His
    commission may be the masterwork from band 6, as drops are;
  - every smith of a hidden people (Culture::isolated: culture.h:221). Their shelves and commissions follow the band rule,
    so they sell their strange masterwork from band 6.
- Otherwise the masterwork is earned, from band-6 drops or your own forge. A mainland masterwork's secret comes only from
  ruin lore or theft (priors.cpp:982-983). A hidden people's can also be learned by apprenticeship (culture.cpp:413).
- Today a commission reseeds from the hero's gold after paying (craft_game.cpp:185), so moving one coin rerolls it. Seed
  it from the smith and the restock period (day / 2) instead.

**Hidden kingdoms and the focus vendor.**
- Hidden peoples always have three alloys (priors.cpp:939). Their masterwork is the strange metal: VEIL, WHORL, TIDE,
  PEARL, SHARD or WRACK, smelted with a reagent found only on their land (culture.cpp:381-417).
- They lie far out at sea, where danger is high, because it grows with distance from home (region.cpp:660-682).
  `--ladder-pace` reports which bands their drops reach.
- Their officers wear the warriors' alloy and their king the strange masterwork (gear_ladder.cpp:265-277). That is the
  player's first sight of it.
- Their smiths are exempt from the shop cap (above).
- Their trials and ruins are old places, so relics drop there too. The strange masterwork is never a relic, because it
  carries its people's id.
- **The focus vendor.** A hidden land's mage sells its tradition's focus, sold nowhere else: a Rare staff at the hero's
  level + 2 (magic_learn.cpp:347-352, 436-446). It is not armour, and this proposal does not touch it. The plan's
  wording differs ("Legendaries are never sold, except by the hidden-kingdom focus vendor": VISION_PLAN.md:1122;
  PLAN.md:1937). The doc batch notes it.

**Saga loot.** storyLoot passes no culture (story.cpp:163-167). Give it the hook's people, as rewardRoll already does
(story.cpp:703-711).

**Building chests.**
- Interiors entered by a door have no site (game.cpp:2484), so their chests have no maker (game_rpg.cpp:349). In band-4+
  towns they would roll heartland metals. Give them the town's culture, through the building's site.
- Also check: openChest and chestSource read zoneLevel with interior coordinates there (game_rpg.cpp:311, 398). Found
  while reading; not run.

## 7. Looks under the owner's rulings (2026-10-25)

The rulings: no compression, with the art budget raised to about 2.5 MB; every alloy always gets its own helmet;
starting armour is simple and slim, and better armour gets more detailed, more dynamic and bulkier; keep the existing
drawn armour. The character size is still open: 32 px with bulk, or 40 px for everyone (emb_P2c/size/CHARACTER_SIZE.md).

**Helmets** (question 4).
- **Mythic alloys keep their alloy.** In the doll's path a Mythic culture-alloy piece is drawn as its alloy: metal 10,
  its colour family, alloyStyle and alloyShape (helm family, skirt, shoulder mass, over-layer and details). The slot's
  Mythic bit, which is already set (HumanLook::mythic, gear_ladder.cpp:146), adds the ember on top:
  - its seams and rims lit ember in the glow slot, the way FURNACE's seams glow (D40);
  - the ember collar;
  - the flame crown seated on the alloy's own helm, with no flame within 2 rows of the eye row (PLAN.md:2108);
  - the flame aura, as today.
- Weapons keep today's white-hot Mythic blade. The item icon keeps today's rule until the icons pass (D22;
  gear_look.cpp:434), so the change lives in fillLadder, forgeOf and shapeOf, not in metalKindOf.
- **MYTHIC stays the style** of Mythic bronze, iron, steel and relics. D32 ("MYTHIC is its own style, on any metal",
  PLAN.md:2533) is restated for culture alloys only.
- **Relics keep their own helmets at T5-T6.** PARADE keeps its onion helm and STARFALL its crystal helm, with the T5-T6
  finish on them (a gilt rim and brow gem; a glowing slit or brow rune).
  - The ladder's crested helm and winged crown stay in use: the Mythic's crown, people's looks, and boards with no forge
    style. Nothing drawn is dropped.
  - The hero's road boards would show the onion helm at T5 and the crystal helm at T6.
- If the owner adopts the size study's "a helm for every tier" (CHARACTER_SIZE 7.2), nothing here conflicts.
- **Under the envelope (revision 4).** The envelope (docs/PLAN.md "Helmets", D44, D61, D66, D68) gave the hero's road a
  plumed great helm or armet at T5 and a winged helm at T6. It also parted f5_star from e6 by helm family, and left
  "T5 starmetal beside the plain T6" waiting on questions 4 and 5. The owner's yes decides it:
  - gilded at T5-T6 wears PARADE's onion helm, and starmetal STARFALL's crystal helm, each in its own tier views;
  - D44's measures still bind: consecutive tiers 0.10 head OD or more apart, and from T3 each tier's head area 5 % or more
    over the tier below. The family's tier views meet them, grown from its own crown as D68 does on the peoples' lines;
  - A5's black, gold and fire Mythic now covers the MYTHIC style only (bronze, iron, steel, relics). A Mythic culture
    alloy keeps its own colours under the ember, with A5's face rule and C19's Mythic terms;
  - the plumed great helm and the winged helm stay drawn (D49) for every look that still uses them;
  - D68's open point closes: a Legendary keeps its metal's helm family.
  The P2c planner writes this into docs/PLAN.md.

**Bulk** (question 5).
- The look already grows with tier. The hero's front walk is 463 px at T1, 539 at T3 and 674 at T5, but 649 at T6,
  smaller than T5 (CHARACTER_SIZE 2). The size study owns that fix.
- **The relic ladder (recommended).**
  - Obsidian's shard shoulders are drawn one size larger than its tier gives (large from T4), so band 5 is bulkier than
    band 4.
  - Emberforged keeps its broad vented shoulders and heavy arms.
  - Starmetal's crystal shoulders are redrawn in place as the largest mass in the game, taller and wider than
    emberforged's. STARFALL's crystal helm, slim ribbed waist, high collar and long split skirt stay. It reads as a
    crystal champion, broad above and slim below.
  - Bulk then climbs at every relic band. The alternative is to accept slim starmetal as the one exception.
  - **Under the envelope (revision 4; the owner chose "grow it").** The envelope sizes shoulders by tier (sizes 2-6) inside
    kEnvelope's caps (D43), and growth only ADDs (D49). So:
    - obsidian draws one shoulder size above gilded at the same tier;
    - starmetal's crystal shoulders are the largest shoulder mass at every tier they draw, from larger crystal sizes
      ADDed beside the drawn ones, never by repainting the crystal set in place;
    - each relic band reads bulkier than the band below (opaque px, front walk f0), all inside the tier's caps;
    - if a cap blocks a step, it goes to the planner with its numbers (D67's path), never a quiet cap change.
- Culture alloys are unchanged here. Their bulk comes from the shape table by rank and from the tier's dials, and only the
  masterwork wears a full cape (D38).

**The art budget.**
- Today's pool is 1,383,460 bytes: 697 parts and 17,076 views (`node tools/doll/pxc.mjs --check --report` at df2a155,
  which writes nothing). That is about 47 bytes of records per view and 0.9 bytes per drawn pixel.
- The pool gate is 1.75 MiB (test_doll.cpp:791). The owner's ruling raises it to about 2.5 MB with no compression, which
  closes D27 (PLAN.md:2520-2523).
- The size study estimates the pool after production at about 2.0-2.1 MB at 32 px with bulk (about 0.4 MB spare), and
  about 2.5 MB at 40 px (none spare).
- This proposal reuses first:

| item | cost |
|---|---|
| the T5 Rare glyph | 0: finish.glyph (50 views, 670 px) used at T5 too |
| the band-1 Epic trim | 0: the Rare trim's rule |
| the Epic gold inlay at the cap | 0 as finish.rune recoloured gold and unlit; about 1-3 KB if the artist draws its own tile |
| the Epic bit in HumanLook | no pool bytes and no extra atlas: a look's key changes, as with any gear swap |
| Mythic on alloys | 0 for the ember seams, rims and collar (existing slots and parts: collar/ember, crest/flame); about 1 KB for each helm family that needs its own crown seat, under 10 KB in all |
| relic helms at T5-T6 | 0: the helms exist; their T5-T6 finish regions are marked where missing |
| starmetal's grown shoulders | about 2 KB (a shoulder part is about 1 KB a side: 12 views, about 400 px) |
| relics, war spoils, every rule | 0 |
| **total** | **about 0-15 KB, under 1 % of 2.5 MB at either size** |

**Size.** Nothing in this proposal depends on the size choice.
- The rules (Batch B) touch no art.
- The look rules state relations, not pixels: a rarer find reads finer at 1x, each relic band is bulkier than the one
  below, and every metal keeps its own helmet. These hold at 32 px or 40 px.
- At 40 px, the few new parts are drawn at the new size with everything else. At 40 px the budget has no slack, and the
  reuse-first choices above keep this proposal's cost near zero.

## 8. What changes, what stays

**The doc batch, right after the owner's answers and before Batch A.** Docs only (PLAN.md, STYLE_P2.md, P2C_CONTRACTS.md,
VISION_PLAN.md), written to the answers. *Status, 2026-10-28:*
- The VISION_PLAN.md part is written (15.37 and the sections it lists).
- The PLAN.md, STYLE_P2.md and P2C_CONTRACTS.md parts wait for the P2c planner, who owns those files during wave 2. They
  include the envelope items in section 7: the hero road's T5-T6 helms, D61, D66 item 2, A5's scope, D68's note and
  D32.
- D27 is already closed by D46 and D48.
- PLAN.md's cited lines have moved since df2a155:
  - "T5 at least" is now near PLAN.md:1962;
  - the T5 and T6 "from" cells are near 1977-1978.

The doc batch's items:
- the ladder table's masterwork line (PLAN.md:1933), if question 8 is yes;
- the T5 and T6 "from" cells (PLAN.md:1948-1949), to the answers to questions 6 and 7. PLAN.md:1949 is matched to the code
  only if the owner picks that option;
- D32 restated for culture alloys, and STYLE_P2 28.4's PARADE and STARFALL helm lines (STYLE_P2.md:1489-1491, 1507, 1525,
  1528), for question 4;
- STARFALL's and KNAPPED's shoulder lines, if question 5 is yes;
- gilded's tier as one statement: band 4, T4 as found and T5 as an Epic; the Heartland hero's road boards show a gilded
  Epic at T5. Today PLAN.md:2082 ("PARADE (T5), STARFALL (T6)"), STYLE_P2.md:1265 ("the Heartland's T5 is gilded") and
  VISION_PLAN.md:1045 (bands named Iron, Steel, Gilded, Jade...: gilded at band 3, from before M6's materials,
  gear.cpp:519, 539-550) disagree with PLAN.md:2039 and gear_ladder.cpp:67 (gilded at band 4);
- the two-alloy king's regalia (D38), wherever the king's T5 surcoat is named;
- D27 closed: no compression, a pool gate of about 2.5 MB (test_doll.cpp:791; pxc.mjs:1050 still prints "1.0 MB
  budget");
- the focus vendor's wording (VISION_PLAN.md:1122; PLAN.md:1937) against the code's Rare focus.

**Batch A, inside P2c's production (wave 2).** These are look-only changes to ladder fields, with no rule, no item
field, no save change and no draw, which is what P2c requires (PLAN.md:1906-1916, 2171-2188). wearGear's, census::dress's
and gearIcon's outputs stay unchanged. The `epic` bit is a look field like rare and mythic, keyed at rig 2 only. Batch A
joins the P2c areas that already own these files (PLAN.md:3283-3330), so no file gets a second owner. P2c's own
integration regression covers it. *Revision 4:*
- Approved for P2c (question 12). docs/PLAN.md places it in P2c's waves. Its wave-2 line "Not in wave 2: the loot
  proposal's twelve questions" predates the answers.
- Wave 2's consolidated owners match the table below, except the bandit chiefs' line in game.cpp, which no wave-2 area
  owns. The P2c planner either grants FOLK-SIM-C that one call or has FOLK-SIM-C derive a boss bandit's tier in its own
  files.
- FOLK-SIM-C's wave-2 brief already carries the far-land lift for bandits, soldiers and the watch. The chiefs and the
  two-alloy king are what question 9 adds.
- The art costs about 0-15 KB of pool, inside D70's allowances.

| change | where | P2c area |
|---|---|---|
| Mythic alloys keep their alloy, with the ember on top (Q4) | gear_ladder.cpp fillLadder 159-178; gear_forge.cpp forgeOf 553, shapeOf 965-972; doll_resolve.cpp | DOLL-C; HELMS (crown seats); FINISH (ember tiles, if any) |
| relics keep their own helmets at T5-T6 (Q4) | doll_resolve.cpp styleKitOf 264-285, needsOf 344-351 | DOLL-C; HELMS (T5-T6 views and finish regions of the onion and crystal helms) |
| relic bulk (Q5) | doll_resolve.cpp effective dials 818-855 (ARMOURER'S size-up at 840-843 is the precedent); the crystal shoulders in maps/armour/shared/** | DOLL-C; ARMOUR-X |
| the marks: band-1 Epic trim, the T5 Rare glyph, the Epic glyph and gold inlay at the cap; HumanLook gains an `epic` bit per slot beside rare and mythic (rig-2 key) | gear_ladder.{h,cpp} lookTier 36-56, fillLadder 137-147; art_human.h (ADD) and key(); doll_resolve.cpp ladderOf 859-879 | DOLL-C; FINISH |
| alloys at least T4; masterwork T5 only from band 4 (Q8) | gear_ladder.cpp:53-54 | DOLL-C |
| far-land people one tier finer (Q9) | looks.cpp:816-831; census::Rank gains `far` (its twin declaration, gear_ladder.cpp:17-21, changes in the same commit) | FOLK-SIM-C (DOLL-C for the twin) |
| bandit chiefs T2: one rankTiers call after the existing outfit lines, writing only the ladder's fields | game.cpp:1864-1866 | FOLK-SIM-C |
| two-alloy kings (D38) | no code expected: rankTiers already gives them the warriors' alloy at T5 (gear_ladder.cpp:265-277), and D38 drops the T5 surcoat on an alloy body; checked on the boards | DOLL-C |
| the invariant | `rpg_test --doll-ladder` | DOLL-C |

**Batch B, the rules batch, after P2c is DONE.** P2c forbids rule changes (PLAN.md:2178-2179). Batch B is its own
batch: its own areas, a verifier gate, one full regression at its integration (the owner's rule: once per update; fix
rounds verify only their fixes), then judge rounds (at most 4). Section 9 lists its gates.

| change | where |
|---|---|
| alloys in order; the strength floor; crafting caps by rank | gear.cpp: matFor 552-572, bandWord 539-550, matBandCap 574-582, matMult 950-963 |
| relics: DropSource.relicPct (runtime, never saved: gear.h:37-38), the decision and the relic slot table | gear.h:58-72; gear.cpp:1058-1092; items.cpp:206-214 |
| the luck gate, if question 6 changes it | gear.cpp:151-161 (rarityAllowedTop) |
| the relic shares at each source; no loot from summons and decoys; building chests take the town's maker | game_rpg.cpp: dropLoot 189-300, openChest 302-393, chestSource 395-436 |
| the shelf cap and its exemptions; selling refuses IF_KIT | game_rpg.cpp:2090-2116, 2303 |
| IF_KIT (revision 5: no IF_KEEPSAKE) | items.h:111 |
| saga loot's maker | story.cpp:163-167 (callers 626-633; story_game.cpp:56-58) |
| companions (revisions 4, 5 and 6: the owner's amendment, follow-up and answers, section 6): their people's make; the kit made once, its item level capped at the hero's level + 4 and its rarity rolled with the hero as the finder; IF_KIT on the kit; partyEquip's verdict first (a decline moves nothing), the kit stowed in place, a displaced gift back to the hero; the one door that refuses IF_KIT (TAKE BACK, the swap, every parting); TAKE BACK of a gift re-wears the stowed kit piece; gifts handed back, and the stowed kit re-worn, at dismissal, an unpaid sellsword's leaving and an angry leave; at a death, their whole pack laid on the ground where they fell with IF_KIT cleared, and the grave laid with nothing to take; the hire mark set at every living parting of a sellsword, so the week's wait holds; the parted roll (rule 8): a living parting keeps the record in Party::parted (the party block's version 5, still reading 4), and a return restores it with the kit as it left, never made again (made only for an old save's companion holding none); the angry sellsword's apology at his table; the inn standing a parted sellsword in his own kit; a parted record dropped at its census death | party_game.cpp:88-122, 191-273, 858-922; party.{h,cpp} (the roll, MAX_PARTED, the block version); party_recruit.cpp:105-156, 183-388 (the inn, the offer, the joins); party_talk.cpp (the apology at a sellsword's table) |
| the "better" rule shared by the sim and the compare arrow (powerScore more than 3 % over the worn piece's, at a given level) | gear.h / gear.cpp (moved from lootp::verdict) |
| the companions' gear lines: the decline, the kit refusal, the hand-back at parting, putting their own piece back on; and their cry on coming back (revision 6) | banter.cpp (a gear line beside cryLine) |
| commissions: the cap and its exemptions, the seed; breaking down refuses IF_KIT | craft_game.cpp:100-105, 181-260 |
| war spoils: dropD and culture in spawnMan; the roll and the counter in warKill | war_game.cpp:278-299, 892-930; game.h:351-352 |
| the card's metal row; the HUD's metal word; on the party page, TAKE BACK greyed on a kit piece, the decline shown as the companion's line, and the compare arrow read at the companion's level (rarity colours untouched); a delight: the party page lists the friends you parted from and where they wait (their home, or their inn once the week is out) | loot_paint.h:500-506; hud.cpp:416; paperdoll.cpp (the party page: partyPageKey and the GEAR list) |
| optional: the everyday alloy's lore line "HARDER THAN TRADE BRONZE, CHEAPER THAN STEEL" becomes "HARDER THAN STEEL, AND CHEAPER TO POUR" (the same pick, so no culture changes) | priors.cpp:928-929 |
| `--ladder-pace` gains the relic, metal-rank and spoils rows; `--loot-sources` gains WAR SPOILS, RELIC, kit and fallen-companion rows | tools/tests/test_ladder_pace.cpp (P2c's), test_loot_sources.cpp |

**What stays.**
- The rarity odds, floors and luck (gear.cpp:85-220), unless question 6 changes the gate.
- Item levels (gear.cpp:226-241). Affixes, powers and attunement.
- The alloy generator (priors.cpp:933-1010) and every forge style's drawing.
- The item layout (items.cpp:289-321) and SAVE_VER 23 (game_rpg.cpp:2404). A bump would refuse every existing save
  (game_rpg.cpp:2357-2360). IF_KIT and the spoils counter use only free bits in bytes that are already saved. The
  parted roll steps only the party block's own version, to 5, which still reads 4 (section 6, rule 8).
- Loot rolls make no new random draw. The commission seed and the spoils roll are the only new streams.

**Old saves.**
- Every piece keeps its stored metal, name and power (items.cpp:289-321).
- Looks that change in Batch A:
  - alloys forged below band 4 draw T4, so a masterwork forged there drops from T5 to T4;
  - Mythic alloy pieces regain their alloy's helmet, outline and colour, and leave the all-black MYTHIC style;
  - gilded and starmetal pieces at T5-T6 wear their own helmets; obsidian's shoulders grow a size, and starmetal's
    become the largest;
  - Epic pieces in bands 1, 6 and 7, and Rare pieces at T5, gain their marks;
  - people in far lands step up a tier, and bandit chiefs draw T2.
- In Batch B, the HUD shows each piece's own metal. A kit made before the change has no IF_KIT flag, so it counts as a
  gift once: it can be taken back, and it is handed back at a parting. A grave laid before the change keeps its
  belongings and its one keepsake, as today. A companion dismissed before the change has no kept record and joins anew,
  once. The party block of save_v23 (version 4) loads with an empty parted roll.

**Risks.**
- **Power.**
  - New pieces gain up to 3 %: band-5 warriors' alloys of tierStep 1, and band-7 masterworks.
  - Two-alloy peoples of tierStep 1 gain 7-10 % in bands 6-7 (*model*).
  - Same-band trash must still take 3-5 hits (tools/tests/test_power.cpp:2-6, 288), and the best allowed rarity must
    still take at least 2.5 (gear.cpp:53-56). `--power-curve` and `--metrics` gate the batch.
- **Goldens.** The `--loot` goldens change: new band-4+ pieces get new names and strength. The random stream does not
  change, except for the commission seed and the spoils roll.
- **The Mythic crown on closed helms.** The flame crown must seat on all 16 alloy helm families, closed ones included,
  and keep the face rule (PLAN.md:2108, 2122). HELMS draws a seat where the crest socket is missing, and C14's face line
  checks it.
- **The hero's road changes at T5-T6** if relics keep their helmets. It is a signed-off board, so it is the owner's
  call (question 4).
- **Big shoulders at doors.** Top-tier shoulders overhang one-tile door jambs (CHARACTER_SIZE 4 (d) and 7.5).
  Starmetal's grown shoulders join that list, and the size study's draw-order fix covers them.
- **Relic looks.** A relic wears the Heartland hero styles even in a Jade ruin. This is intended, because relics are the
  old makers' work. It needs the piece to carry no culture id, or the doll would cut it in the land's forms
  (gear_ladder.cpp:193-221; gear_look.cpp:108-121).
- **Relic share.** It depends on how often players explore old places. `--ladder-pace` measures it on a stated mix, and
  the "half" is the knob.
- **The everyday alloy may read as a step down.** Its name ends in BRONZE, BRASS or IRON (priors.cpp:944-956), and the doll
  draws it in that metal's colours (gear_ladder.cpp:101-117), so after steel it may look like a step down. Its own T4
  outline and helmet, the compare arrows, the card's rank row and the lore line carry it, and the T4-T7 playtests watch
  it.
- **Two-alloy peoples have no masterwork.** From band 5 their warriors' alloy carries on. Relics and the top-tier marks
  give their late game its steps.
- **A friend comes back in old kit.** A companion who returns after the hero has gained many levels wears the kit they
  left in, at its old item levels ("Made once"), while their body tracks the hero. The hero's gifts carry them, as
  ruled. The playtests watch whether a returning friend feels under-geared.
- **Hue clashes.** Some relic metals share a hue with a rarity colour: GILDED's gold with Legendary, OBSIDIAN's violet
  with Epic, and EMBERFORGED's ember with Mythic (items.cpp:113-114; STYLE_P2.md:1239-1243). One channel per meaning keeps
  rarity on the name, the frame and the ground light. A doll check keeps FURNACE's steady seam heat apart from the Mythic
  flame (PLAN.md:2563-2564, D40), and now also a Mythic alloy's ember seams.

## 9. How it is checked

**`rpg_test --ladder-pace [--worlds a..b] [--report]`.** P2c builds it on today's rules (FOLK-SIM-C, PLAN.md:3295), and
that run is Batch B's baseline. Batch B adds its rows.
- **What it rolls.**
  - It walks each VISION 1.4 stage: levels 5, 10, 15, 20, 25, 30, 40 and 50, each at the middle of its danger band
    (VISION_PLAN.md:110-117).
  - At each stage it makes 1,000 rolls per source from section 3, through the game's own makers: rollDropC with each
    caller's DropSource, shopStock, and the commission, kit, forge and spoils paths.
  - It runs over 20 worlds, so two- and three-alloy peoples both appear.
- **What it reports.** Per stage and source: band, metal (a people's rank or a relic), look tier and mark, and rarity; the
  material multiplier per band and alloy rank; the assumed hour, with its weights printed, giving the relic share of
  drops and of everything acquired with one shop buy an hour; the first level at which each rarity and each metal
  appears.
- **What fails.**
  - A rarer piece draws plainer than a commoner one. Common matching Uncommon is allowed.
  - An alloy is weaker than its band's step, or a people's alloy never drops at its band.
  - A shelf or commission offers a masterwork outside the two exemptions.
  - A relic appears below band 4, outside an old place, or carrying a culture id.
  - An alloy draws below T4, or a masterwork draws T5 below band 4.
  - A Mythic alloy piece is drawn without its alloy's helm family.
  - If question 5 is yes: a relic band is not bulkier (opaque px, front walk f0) than the band below.
  - Relics fall under 8 % or over 20 % of metal armour drops in the assumed hour.
  - Legendary or Mythic appears below the level question 6 settles, outside a campaign finale or the dragon's hoard.

**The full audits** (VISION 15.27.2 and 15.27.5: VISION_PLAN.md:3067, 3088). All stay green:
- `--loot-sources`: every source in real games passes lootValid, with new rows for WAR SPOILS and RELIC, a kit row, and
  a row for a fallen companion's pile (revision 5: no IF_KIT on it, and no kit piece over the hero's level + 4 at the
  join).
  It checks that an IF_KIT piece never reaches the hero's pack. A source the game uses but the audit never exercised
  fails (test_loot_sources.cpp:1-15);
- `--loot` (the gate per level band, power curves, determinism) and `--power-curve`;
- `--metrics`, the balance sim; `--foes` (elites, named uniques, world bosses); `--arms` (culture kits and looks);
- `--companion` (revisions 4, 5 and 6) covers:
  - TAKE BACK refused on a kit piece;
  - a worse or equal gift declined, with both packs unchanged;
  - a better gift worn, with the kit stowed;
  - TAKE BACK of a gift, which re-wears the kit piece;
  - a gift-for-gift swap returning the first gift;
  - dismissal, an unpaid sellsword and an angry leave each returning exactly the gifts (item seeds compared);
  - a death dropping exactly what they carried, kit worn and stowed, gifts and belongings, as ground loot within a tile
    of where they fell (item seeds and counts compared), with no IF_KIT on any dropped piece, nothing added to the
    hero's pack, and a grave with nothing to take;
  - the kit's item level at most the hero's level + 4, on every recruit path;
  - the hire mark set to the day on every living parting of a sellsword (dismissed, unpaid or angry), his slot empty
    for 7 days from it and standing on the 7th; MERC_FALLEN after his death (his slot never stands again);
  - leaving and coming back (revision 6, rule 8), for a census companion dismissed and asked back at home and for a
    sellsword dismissed, unpaid or angry and rehired after his week, with the hero 10 levels higher in between:
    - level: their page reads the hero's level less one, never below the level they left at, and every kit piece
      keeps the item level it had;
    - kit seeds: the same kit item seeds, worn in the same slots, with IF_KIT, and no kit made again;
    - bond: the same bond and unlocked skills, with the same id, approval, memories, flags, days, kills and VOICE
      fields, and no bond grown for the days away;
    - the gifts in the hero's pack exactly once, from the parting, and nothing added at the return;
  - an angry census companion's record unchanged by the leave and by the apology on a later day, apart from approval
    lifted to at least -30; an angry sellsword answering ANGRY at his table until his apology;
  - the parted roll through a save and load; save_v23's party block (version 4) loading with an empty roll; damaged
    party blocks refused as today; a parted census companion's death removing their record, with no pile and no grave;
    the 33rd parting forgetting the friend parted longest ago; an old save's companion who parts with no kit piece
    getting their kit at the return;
  - a seeded fuzz of give, take back, swap, dismiss, leave, rehire, rejoin and death over seeds 1..20 that never puts an
    IF_KIT item in the hero's pack or on the ground, and never makes a kit twice: each kit seed lives once, across the
    hero's pack, the members, the parted roll and the ground.
- `--wards`, the sieges, with the spoils counter;
- `--doll-ladder` with the invariant, and `--doll-coverage` source L for the far-land tiers, Mythic alloys and relic
  helmets;
- save_test (save_v23 still loads, its party block with an empty parted roll; the new bits round-trip), `rpg_test
  --seeds 1..20`, the script sweep and Witness. Witness plays one return: dismiss a companion, ask them back at their
  home, and read their page (the same bond, skills and kit).

**Areas (approved 2026-10-28; revision 6 splits PARTY-WAR).** Each set of files belongs to one area.
- **Batch A** ships inside P2c (section 8): DOLL-C, FOLK-SIM-C, HELMS, ARMOUR-X and FINISH, each on files it already
  owns. The one exception is the bandit chiefs' line in game.cpp, which the P2c planner assigns.
- **Batch B**, the armour rules update after P2c is DONE (docs/VISION_PLAN.md 15.37):
  - LOOT (engineer-high): gear.{h,cpp}, items.{h,cpp}, game_rpg.cpp, story.cpp, story_game.cpp. It publishes the item
    flags, the DropSource fields and the shared "better" rule first.
  - PARTY (engineer-high): party_game.cpp, party.{h,cpp}, party_recruit.cpp, party_talk.cpp, banter.cpp (the
    companions' gear lines and their cry on coming back only). Revision 6 split it from PARTY-WAR, because leaving and
    coming back adds the party block, the recruit paths and the apology at a sellsword's table to the companion work.
  - WAR-CRAFT (engineer): craft_game.cpp, war_game.cpp, game.h (the spoils counter's bits).
  - UI (engineer): loot_paint.h, hud.cpp, paperdoll.cpp.
  - TEST (engineer): test_loot_sources.cpp, test_loot.cpp, test_party.cpp, test_wards.cpp, whichever test file holds
    P2c's `--ladder-pace` once P2c is DONE, and two hooks in script_party.cpp for the script sweep and Witness:
    `party snap <name>` and `expect party same <name>` (id, bond, approval, memories, and the kit's seeds by slot).
  - LOOT, PARTY, WAR-CRAFT, UI and TEST run in parallel after LOOT's contract block. Then come the verifier gate, the
    one full regression, and a judge on the audits above.

## 10. Other ways, and the pick

| model | how alloys meet rarity | good | bad |
|---|---|---|---|
| **A. Where sets the metal, luck sets the finish, old places hold the relics** (recommended) | the band picks the metal and its alloys come in order; rarity adds finish, with marks at the top; relics come from old places | keeps VISION's bands as materials (7.2, 15.11); every alloy and every hero style can be found; no land's reveal is spent early; a small rule surface | the late game leans on finish marks and relics; the relic share needs measuring |
| A2. Draft 1: as A, but Epic and up are one metal ahead | Epic, Legendary and Mythic in the next rung's metal | a purple find gets a new outline | spends each land's reveal; arriving in band 5 feels like a downgrade; +15 % early armour; clashes with the shop cap; a relic lift needs a field |
| B. Alloys are a rarity | Rare is the first alloy, Epic the second, Legendary the masterwork, at any band | colour equals metal | breaks bands as materials; alloys at band 1; plain steel for most late finds |
| C. Alloys are only earned | band-4+ drops are relics; a people's alloys come only from its smiths, its army and your forge | the secrets matter | most of the 37+ alloy looks are never worn; slow |
| D. Fixes only | alloys in order and strength by band | the smallest change | the relic styles stay unreachable; Epic adds a tier but nothing at the T5 cap, and a Rare there shows nothing |

**Recommendation: A.** Ship the look-only parts in P2c's production, and the rules as their own batch after P2c is DONE,
measured by `--ladder-pace` and the full audits. Alloy traits (VISION 1.4: "culture gear sets with their own move
quirks") and leathers above band 1 (D23) stay for later.

## 11. Owner questions (answered 2026-10-28: "Defaults is good")

1. **Alloys in order.** Should each people's alloys come in a fixed order: everyday at band 4, warriors' at band 5,
   masterwork at bands 6-7, with two-alloy peoples keeping their warriors' alloy? Should alloys also never be weaker than
   their band? Today a hidden roll keeps about half the peoples' everyday alloy out of drops. Recommended: yes.
   **Answered 2026-10-28: yes** (the rules update after P2c).
2. **Old makers' metals.** Should gilded, obsidian, emberforged and starmetal become relics of old places, drawn in their
   own hand-drawn styles and named "old makers' work"? From band 4 they would be about half the metal gear in ruin and
   shrine hoards and caches, in wonders' caches and on undead in ruins, and all of a world boss's or a dragon lair's
   hoard. Recommended: yes, tuned to about one armour drop in eight.
   **Answered 2026-10-28: yes** (the rules update after P2c).
3. **Rarity is finish.** Should rarity always show as finish in the piece's own metal, never as the next land's metal? An
   Epic stays one tier finer and bulkier in its own metal. At the top, a Rare gains a glyph, an Epic at the T5 cap a gold
   rune inlay, and a band-1 Epic trim, so a rarer find never looks plainer. Lit runes, sparks and motes stay Legendary's,
   and the flame Mythic's. Recommended: yes.
   **Answered 2026-10-28: yes** (looks, inside P2c).
4. **Every metal keeps its helmet.** Following the ruling that every alloy always gets its own helmet: should a Mythic
   alloy piece keep its alloy's helmet, outline and colour, with the ember tempering and the flame crown on top? The
   all-black MYTHIC style would stay for bronze, iron, steel and relics, and D32 would be restated for alloys. And should
   gilded and starmetal keep their own helmets at T5-T6? Today they swap to the ladder's crested helm and winged crown, so
   starmetal's crystal helm is never seen in play. The hero's road boards would then show the onion helm at T5 and the
   crystal helm at T6. Recommended: yes to both.
   **Answered 2026-10-28: yes to both** (looks, inside P2c; section 7 says how it meets the envelope).
5. **Bulk on the old makers' ladder.** You ruled that better armour gets bulkier. Today gilded and obsidian are the same
   size, and the ladder ends on slim starmetal, lighter than emberforged. Should obsidian take one shoulder size more than
   gilded, and starmetal's crystal shoulders be redrawn in place as the biggest in the game, keeping its crystal helm,
   slim waist and long split skirt? Or is slim starmetal an accepted exception? Recommended: grow it.
   **Answered 2026-10-28: grow it** (looks, inside P2c, inside the envelope's caps, growth by ADDed sizes: section 7).
6. **Luck and the level gate.** Today hoards, bosses and named foes give a Legendary from level 12 and a Mythic from 20:
   about 0.9 Legendaries before level 20 and 0.5 Mythics before 30 for a dungeon-goer (*model*). VISION 15.27.4 says luck
   beats the gate "a little, never a lot", with a campaign's last reward as the main early path. The options:
   - (a) keep today's rule, and fix PLAN.md:1949 to match;
   - (b) outside campaign finales, luck still lifts a find to Epic at any level, but to Legendary only from level 17 and
     to Mythic only from level 27, within 3 of their floors (about 0.4 and 0.2);
   - (c) strict floors outside campaign finales: Legendary from 20, Mythic from 30.

   Epic's lift from level 5 stays in all three. Recommended: (b).
   **Answered 2026-10-28: (b)**: Legendary from 17 and Mythic from 27 by luck, campaign finales aside (the rules update after P2c).
7. **Shops.** Should shelves and smith commissions stop at a people's warriors' alloy, with two exemptions? A capital smith
   of that people who trusts you with his patterns may forge the masterwork on commission from band 6, and hidden-kingdom
   smiths sell their strange masterwork as the band allows. Otherwise the masterwork comes from band-6 drops or your own
   forge. The hidden land's focus vendor is untouched. Recommended: yes.
   **Answered 2026-10-28: yes** (the rules update after P2c).
8. **Crafting looks.** Should an alloy piece always draw at least T4, and a masterwork T5 only from band 4? This changes the
   signed-off ladder table, which says a masterwork alloy is T5 at least (PLAN.md:1933). It also changes old saves: a
   masterwork forged in bands 1-3 drops from T5 to T4 on the hero, while other alloys forged there rise to T4.
   Recommended: yes. **Answered 2026-10-28: yes** (looks, inside P2c).
9. **People in far lands.** In band-4+ lands, should bandits step up to T2, and soldiers and the village watch to T3,
   still without an alloy? Captains keep the warriors' alloy and kings the masterwork. A two-alloy kingdom's king wears
   the warriors' alloy at T5, with crest, gold, gem and the finish on his cloth over-layer, and no surcoat or cape (D38).
   Bandit chiefs draw T2, as the plan says, in the plate they already wear. Recommended: yes.
   **Answered 2026-10-28: yes** (looks, inside P2c).
10. **War spoils.** At a siege where you hold a War quest, should enemy soldiers (10 %) and captains (50 %, Uncommon or
    better) drop a piece in their realm's make, up to three per siege? They roll like a body at the siege's danger, with
    the item level and rarity capped by your level as for any find. Recommended: yes.
    **Answered 2026-10-28: yes** (the rules update after P2c).
11. **Companions.** Should a companion's kit be their own people's make and climb the metal ladder? The kit stays theirs:
    you cannot take it back, swap it into your pack, sell it or break it down, and a kit piece replaced by your gift waits
    in their pack. A fallen companion's keepsake can be worn but never sold or broken down. A dismissed sellsword waits a
    week before you can rehire them. Recommended: yes.
    **Answered 2026-10-28: yes, amended**: "we probably will need to allow for players to give their companions better armor
    than what they start with though, so maybe you can't take their defaults but you can give them better armor and they
    will equip what's better." The kit stays theirs; a gift is worn only if better, can be taken back, and comes back at
    every parting (section 6; the rules update after P2c). **Followed up the same day:** "Made once, and if they die
    they drop everything." The kit is never remade, and a death drops all they carry where they fell, the kit as
    ordinary gear (revision 5). **And the four points left to confirm, the same day** (revision 6): the kit's cap at the
    hero's level + 4, "that works for me"; the pile's rules, "yep thats good"; hire-and-let-die once per slot, "thats
    fine if someone wants to farm that way they can but there are defintly better avenues to make money"; and a
    companion who comes back, "well if they re join they should stay at the level they left, and shouldnt get treated
    as a new join since they arent new." A companion who parts and comes back keeps their level, kit and record
    (section 6, rule 8).
12. **Timing.** Should the look-only parts ship inside P2c's production: the marks, the helmets, the bulk, the crafting
    looks, and the far-land and chief tiers? The rules would follow as their own batch after P2c is DONE, with their own
    full regression and the loot and balance audits (`--loot-sources`, `--ladder-pace`, `--power-curve`, `--metrics`,
    `--foes`, `--arms`, `--companion`). Recommended: yes.
    **Answered 2026-10-28: yes.** The looks ship inside P2c; the rules are planned as their own update after P2c is DONE, not as
    wave-2 work.

## 12. Review log

The tags give each point's source review: R is revision 3's rules-compliance review; F (fit with the code), P (the
player's eye) and E (the economy) are revision 2's reviews.

**Revision 6 (2026-10-28): the owner's answers on companions**
- **Ruled, in the owner's words** (2026-10-28):
  - the kit's cap at the hero's level + 4, with the hero as the finder: "that works for me";
  - the pile's 240 s, gone at a map change or a load, and nothing dropped by a death off the hero's map: "yep thats
    good";
  - hire-and-let-die paying once per sellsword slot: "thats fine if someone wants to farm that way they can but there
    are defintly better avenues to make money";
  - a companion who comes back: "well if they re join they should stay at the level they left, and shouldnt get
    treated as a new join since they arent new."
- **Reversed:** revision 5's reading that a companion who comes back is a new join (the record started again, the kit
  made again from its seed). Rule 8 (section 6) keeps the record and the kit. Gifts still come back at the parting.
- **Found while ruling it:**
  - Today only a census companion's angry leave keeps the record (State::Left, party_game.cpp:257-260). Dismissal, an
    unpaid sellsword, a personal quest's farewell and a sellsword's angry leave remove it (party_game.cpp:320, 352,
    914-922; story.cpp:815). Loop 6 in section 6.
  - A companion's level is never stored (party.cpp:216; VISION 9), so "the level they left" is kept by keeping bond,
    the skills it opened and the kit. The reading goes to the owner (VISION 15.37 item 8).
  - The apology is the existing way back from an angry leave (party_talk.cpp:622-656). There is no bond bar, so none is
    added. An angry sellsword with his record kept would walk out at the next deed, so he asks the same apology (item 9).
  - A new record drops an unhappy companion's approval and MF_QUEST_DONE, so dismissing and asking again launders the
    one and lets a finished personal quest be offered again (party_talk.cpp:200). Keeping the record closes both.
  - Restored as saved, a record's lastDay would pay bond for every day away (party_game.cpp:329-342), so a return sets
    it to that day.
  - Party::deserialize refuses any version but its own (party.cpp:431), and a refused block refuses the whole save
    (game_rpg.cpp:2826), so the roll's version 5 must still read 4.
  - An angry sellsword's hire mark stays at his hire day today, so his slot can stand him again at once
    (party_recruit.cpp:124-125). Every living parting now sets it.
  - The census can't carry the record (it is rebuilt from seeds and saves a few flags: life.h:119-120, 368-369), so it
    waits in the party block.
  - Joining never befriends a census person (life_game.cpp:4921 is the only call), so a raid captive who joined as
    Rescued may have no reason to come again, though their farewell promises it (party_talk.cpp:961-962). Kept as the
    rule; the owner's call (item 10).
  - An old save's companion hands back all their gear as gifts at their first parting (IF_KIT is new), so a return
    holding no kit piece makes their kit then, once.
- **Changed:** PARTY-WAR splits into PARTY (engineer-high) and WAR-CRAFT (engineer), since rule 8 adds party.{h,cpp},
  party_recruit.cpp and party_talk.cpp to the companion work. TEST gains two script hooks (section 9).

**Revision 5 (2026-10-28): the owner's follow-up on companions**
- **Ruled, in the owner's words:** "Made once, and if they die they drop everything" (2026-10-28). It settles the two
  points revision 4 left as the plan's readings (docs/VISION_PLAN.md 15.37).
- **Made once.** The reading (the kit made when they join, never remade as they level) is now a ruling.
- **They drop everything.** It replaces revision 4's death rule (gifts back to the pack, one keepsake at the grave). A
  death lays all they carry where they fell through the existing ground loot (Pickup, game.h:245-254, laid as dropLoot
  lays a body's, game_rpg.cpp:217-221) under its despawn rules (240 s, game.cpp:1734; never saved and cleared at a map
  change, game.cpp:1946). Kit pieces drop without IF_KIT, as ordinary gear. Partings while they live are unchanged.
- **The keepsake goes; the grave stays.** No other system reads the keepsake. Grief comes from Life::residentDied
  (party_game.cpp:229), the retired inn slot from MERC_FALLEN (party_game.cpp:240), and the stone, the weapon laid on
  it, the epitaph and the flowers from the Fallen record. IF_KEEPSAKE leaves Batch B, and old graves keep their keepsake.
- **Found while ruling it:**
  - Today a kit takes the place's danger as its item level and rolls rarity at that level (party_recruit.cpp:290, 374,
    404; party_game.cpp:95). Once kits drop, that would hand a low-level hero high-level gear past question 6's gate. So
    the kit is capped at the hero's level + 4 and rolls with the hero as the finder. This takes revision 2's E item
    ("gate a kit's level on the hero's level + 4"), declined while kits could not reach the pack.
  - MERC_FALLEN still retires a dead sellsword's slot (party_game.cpp:240; game_internal.h:348), the inn skips it
    (party_recruit.cpp:125), and Game::marks is saved (game_rpg.cpp:2659-2660, 2824). Hire-and-let-die therefore pays
    once per slot: about 2.5 times a warrior's first day's wage (*model*, kit_value.py). Accepted by the owner in
    revision 6.
  - A death off the hero's map (a companion at home carried off in a raid whose rescue fails: life_raids.cpp:31-38,
    quests.cpp:1501-1502) has no ground to drop on, so what they carried is lost. Every other death has a body on the
    hero's map, a carried one included (party_move.cpp:276-285). Confirmed by the owner in revision 6.

**Revision 4 (2026-10-28): the owner's answers** ("Defaults is good", with question 11 amended)
- **The amendment, ruled.** The kit stays theirs, and a gift stays the hero's. A gift is worn only if better, by the
  compare arrow's rule at the companion's level; a tie keeps their own and a worse piece is declined. A worn gift stows
  the kit. TAKE BACK returns a gift and re-wears the kit. Gifts come back at every parting, and also at death until
  revision 5 replaced that. The swap leak is closed by one door that refuses IF_KIT (section 6).
- **Found while ruling it:** today partyDismiss drops the hero's gifts with the member (party_game.cpp:914-922), and at
  death gifts become belongings with one keepsake (loop 5, section 6). The companion page's compare arrow reads at the
  hero's level, not the companion's (paperdoll.cpp, "at their level: the hero's"), so it moves to the companion's
  level.
- **Reconciled with the P2c envelope** (signed off the same day, after this proposal): question 4 settles the hero
  road's T5-T6 helms and D66 item 2; question 5 grows inside kEnvelope's caps by ADDed sizes (D49), not a repaint in
  place; D68's open point closes; A5's black Mythic now covers the MYTHIC style only (section 7).
- **Placement:** Batch A ships inside P2c, where the P2c planner places it in PLAN.md's waves. The bandit chiefs' line in
  game.cpp has no wave-2 owner yet. Batch B is the armour rules update after P2c is DONE (docs/VISION_PLAN.md 15.37).

**Taken in revision 3** (each checked against the repo first)
- **R, high: a Mythic alloy must keep its alloy's helmet and outline.** Confirmed at gear_ladder.cpp:60,
  gear_forge.cpp:553 and 738, and gear_ladder.cpp:167. Now the alloy is kept, with the ember and the flame crown on top;
  MYTHIC stays for plain metals and relics (sections 2, 4, 7; question 4).
- **R: don't rewrite PLAN.md:1949 to the code's one-step bypass.** Confirmed at gear.cpp:156-161 against
  VISION_PLAN.md:3077. The rewrite is withdrawn, and question 6 offers three options with numbers.
- **R: bulk on the relic ladder.** Confirmed at gear_forge.cpp:734-737 and STYLE_P2.md:1266-1267, 1504-1508. STARFALL's
  drawing is kept, and its shoulders grow in place, or the owner accepts the exception (question 5).
- **R: Batch B after P2c is DONE**, with its own regression (section 8).
- **R: the shop cap against PLAN.md:1948.** Two exemptions: a trusting capital smith and hidden-kingdom smiths
  (section 6; question 7).
- **R: the masterwork rule changes PLAN.md:1933 and old saves.** Said plainly in section 6 and question 8.
- **R: two-alloy king's regalia.** Per D38: crest, gold, gem and the finish on the over-layer's hem; no surcoat
  (sections 5 and 8).
- **R: bandit chiefs in Batch A.** The tier field only; the outfit is unchanged (PLAN.md:2180-2181).
- **R: gilded's tier in three docs.** Listed for the doc batch, with the code's band-4 reading (section 8).
- **R gaps.** The swap leak (party_game.cpp:871, 888) and the grave keepsake (party_game.cpp:213, 741) are closed with
  IF_KIT and IF_KEEPSAKE. The full audit list is in section 9. War spoils roll as a body with the hero's level, so the +4
  cap and the gate apply. The art is costed at 0-15 KB against 2.5 MB. Rarity colours stay on the paper doll screen.
  Hidden kingdoms and the focus vendor are covered in section 6.
- **R: the owner's rulings of 2026-10-25** (no compression; a helmet for every alloy; slim to bulky; keep drawn armour)
  are folded in, and the size question is shown not to matter here (section 7).
- **Found while checking:** gilded and starmetal lose their own helmets at T5-T6, so starmetal's crystal helm is never
  seen (doll_resolve.cpp:264-265, 277); `--ladder-pace` is already P2c's (PLAN.md:3295); Batch A now joins P2c's wave-2
  areas instead of new ones; the item icon keeps today's Mythic rule (D22; gear_look.cpp:434).

**Taken in revision 2** (in brief)
- F: a relic lift can't be stored without a field, so relics keep today's band rule. F and E: the rarity gates were
  restated with the model's odds. F and E: Epic was plainer than Rare in bands 6-7, so the top-tier marks and the
  invariant were added. F and P: far-land soldiers step up to T3 with no alloy. F and E: the spoils needed a counter, a
  level and a maker. F: the shelf cap now covers commissions. F: the power change was restated. F: companion kits and the
  dragon's greatsword were described wrongly. F: a relic must carry no culture id. F and E: the undead yield is 1.8 %
  armour. F: Option D's row was corrected. F: the hours follow VISION 1.4.
- P: the next land's reveal was spent early, so there is no lift. P: bands 6-7 got visible steps. P: rarity hues
  collide with relic metals, so each meaning keeps one channel and "glow means legendary" is gone. P: what shows on the
  hero is stated honestly, and relics favour the body and helm. P: the card's rank row and "OLD MAKERS' WORK" explain
  the two ladders. P: saga loot was added. P: alloys got a T4 floor. P: the relic target got its denominator.
- E: the kit duplication was closed first. E: summons and decoys drop nothing. E: the two-alloy power gain is listed
  under risks. E: the commission reroll is seeded per restock.

**Declined or deferred**
- **P: make the warriors' alloy the band-4 metal.** Declined. Every people's everyday alloy would vanish from drops, and
  bands 4 and 5 would share one metal, losing a step of the ladder. The rank row, the lore line and the alloy's own T4
  outline carry the read instead, and the playtests watch it.
- **P: move the relic metals so none sits on its rarity's hue.** Declined. The metals' order and colours are the approved
  hand-drawn styles (STYLE_P2.md:1489-1508; PLAN.md:2500-2502, 2533-2535, 2563-2564). Rarity stays off the figure's metal
  instead.
- **P: a third step for two-alloy peoples, such as a tempered variant.** Deferred. A new alloy needs the generator, whose
  draws feed every later culture field (priors.cpp:939-1008), and a derived variant would need its own solved outline
  (PLAN.md:2545-2549, D36). Relics and the top-tier marks come first. Revisit after the playtests.
- **P: each relic piece keeps its own colour on its own slot.** Deferred. One armour metal is a palette-budget decision
  (PLAN.md:2490-2491, D15; the 48-colour cap, STYLE_P2.md:1278), so it is the owner's call later.
- **E: gate a kit's level on the hero's level + 4.** Declined in revisions 2-4: while kit pieces could not reach the
  hero's pack, their level shaped only the companion's own strength (party_recruit.cpp:290). **Taken in revision 5**,
  because a death now drops the kit as ordinary gear (section 6), and ruled by the owner in revision 6.
- **F: design and gate T3 alloy looks.** Declined. Steel at T3 is what PLAN plans, and it needs no new art.
- **F and E: decide spoils once, on the siege's outcome.** Declined. The War quest already pays an item on completion
  (game_rpg.cpp:960, 978).
- **P: forbid forging a masterwork below band 4.** Declined. It would lock a learned recipe behind travel.
- **R option not taken: drop the keepsake when only kit pieces remain.** Declined in revisions 3-4. Overtaken in
  revision 5: the owner ruled that a death drops everything, so there is no keepsake, and a fallen friend's sword lies
  where they fell with the rest.
