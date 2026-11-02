# Fix batch P4: the first hour from the testers' seat (the first armour, the quest pointer, the gift said aloud, steady conversations, text for testers, small notes, gate notes)

Planner, 2026-11-02. Base **25d47da** (main == solid; P3 and the creator hotfix shipped; baseline root CURRENT **p3f2**),
branch `fix-p4-int` in the integration worktree. Read with `README.md` (the code map), `docs/ARCHITECTURE.md`,
`docs/SOLID_PLAN.md` section 5 (rules for new code), `docs/ARMOUR_SPAWN_RARITY.md`, `docs/VISION_PLAN.md` 7.1-7.5 and
15.1/15.9, and `docs/FIX_BATCH_P3.md` (the format this batch follows). Owner: "yeah que those up for next".

**In one paragraph.** The Witness W7 testers (Sonnet, seed 7) found the first hour harder to read than to play. A newcomer
cannot see what beasts dropped, what a shop sells or what a menu holds, so "nothing to sell, no armour" is what they
report; the first body armour costs more than a newcomer can earn before a quest; the pointer stays on the main quest after
they accept the innkeeper's job; the innkeeper's gift goes into the pack silently; and a conversation's options move
between visits, so a number picks the wrong one. This batch makes the first purchase a real step (a beginner's leather
coat a newcomer can afford after a few hunts), makes a just-accepted job the one the pointer follows, says a gift where it
is given, keeps every speaker's options in one order, puts the shop, the menu, the ground loot and the gifts into the
Witness text, shows danger inside dungeons, and fixes the gate notes. **How it should feel:** a newcomer gets the old
blade from the innkeeper and sees "+ OLD BLADE" on the panel as he says it; takes the wolf job and the goal line, the map
marker and `goto Quest` all point at the den; skins a few beasts, sells the hides and buys a brown leather coat that
looks like leather, not like the shirt he started in; talks to the smith again and finds WARES where it was; and every
gate either passes or names a real change.

Absolute rules for every area (from the batch brief; they bind the areas and the integrator):
1. No visible window: `embervale.exe` only with `--witness` (game scripts through `tools/gates/witness_pack.mjs`, Witness
   tests through `witness test`, Witness calls). A true non-harness run uses `SDL_VIDEODRIVER=offscreen`. rpg_test,
   art_hash, save_test, city_gallery and audio_preview are CLI and fine. Never `tools/run_scripts.sh`.
2. Heavy processes only through `tools/slot.sh` (cap 2 while the owner plays a game, else 5: wait). Never kill an
   Embervale.exe you did not start; never touch `%APPDATA%\Josh17400\Embervale` or the Desktop. Scratch output under
   `C:\Users\joshu\AppData\Local\Temp\claude\C--Users-joshu\1979ea42-238a-4969-bda8-d76ab6bf2de5\scratchpad\p4\<area>`;
   delete PNGs when done (the owner pack, section 15, is kept until the owner has seen it).
3. Throwaway clones and worktrees link `build/_deps/sdl3-src` to the main checkout by a junction: before deleting one run
   `cmd //c rmdir <clone>\build\_deps\sdl3-src`, then check `C:\Users\joshu\Documents\embervale\build\_deps\sdl3-src\CMakeLists.txt`
   still exists. Never `rm -rf` through a junction.
4. Behaviour changes only where your area owns them; every other gate stays identical. A baseline your change moves is
   declared in `tools/gates/baseline/moves_p4/<AREA>.txt` (section 13) with its reason; you never re-record a baseline
   yourself and never loosen a gate. No area in this plan changes the save format (if one finds it must: SAVE_VER bump,
   migration, fixture test, old saves load, and tell the integrator first).
5. Owner rules: the rags-to-riches ladder (beginner gear "okay/mid", starting armour simple and not bulky); every alloy and
   material its own armour shape and its own helmet; companions equip better gifts, "if they die they drop everything",
   kit cap hero level + 4, 240 s pile, rejoin keeps person and level; never delete drawn art or rig-1 art. An art change
   the owner might find "off" goes into the owner pack as pending, never silently shipped as final. The mace stays as it
   is (D113).
6. SOLID section 5: new code in its lane and layer, no new layer violation (`tools/gates/check_layers.sh`), no function over
   300 lines (new code under about 150), tables over switches, test-only code only in `rpg/view/scripts/` and
   `tools/tests/`. `rpg/view/hud.cpp` (2,214 lines) and `rpg/view/render.cpp` (3,842) are over the 2,000-line guide: edit
   existing lines there and put any new function in a smaller neighbour (`dialogue_ui.cpp`, `loot_ui.cpp`, `screens_ui.cpp`).
7. Never end a turn while a build or gate still runs: run them in the foreground (chain chunks) and report when finished.

---

## 1. The items at a glance

Every item was checked at 25d47da with the main checkout's build (`C:\Users\joshu\Documents\embervale\build`, built at
08:59 from 25d47da; the integration worktree holds the same tree). Evidence came from hidden Witness instances (three
throwaway `witness test` files with probes, `scratchpad\p4plan\wt\tests\p4a|p4e|p4f.witness.json`, config
`scratchpad\p4plan\wt\witness.json`: launch = that build with `--witness`), rpg_test, the code, and the W7 reports
(`scratchpad\p3m\wave\witness15\witness\playtests\t5*-t7*` at 94d7bec and `scratchpad\judge-w4-h\...\t5a|t5b`).

| id | item | real at 25d47da? | evidence (short) | area | owner pack |
|---|---|---|---|---|---|
| 1 | early armour out of reach | yes | start 30 g; seed 7 smith: LEATHER GAMBESON 80, BRONZE BRIGANDINE 80; beasts' goods sell for ~4 g a kill; 3 of 12 start inns have no job and 4 of 12 send a level-1 hero after a bandit chief or into a dungeon (section 2) | ECONOMY (+ WITNESS text) | yes |
| 1b | "animals drop nothing sellable" | **no** | 4 boars and 4 wolves killed beside the hero (probe D): RAW HIDE x5, VENISON, WOLF PELT x2 picked up within 1.6 s (toasts "+ RAW HIDE" ...); the testers cannot see ground items or toasts in text | WITNESS | - |
| 2 | the pointer stays on the main quest after a job is accepted | yes | probe A: after the blade the pin falls to THE DRAGON'S SHADOW; accepting THE WOLVES OF OSLARS keeps it there (`tracked: true` on the main quest; toast "NEW QUEST: ..."); `acceptQuest` keepPin rule, game_quests.cpp:46-52 | TALK | yes |
| 3 | no "+ OLD BLADE" when the innkeeper gives it | partly | the blade IS given (equipped, pack 1 -> 3 items); its toast is made but waits hidden while the dialogue is open (render.cpp:2333-2340, hud.cpp:412) and shows after it closes, under the NEW QUEST banner, over the innkeeper's name label | VIEW (+ WITNESS text) | yes |
| 4 | options change order between visits | yes | probe B: smith GAOLD with bread `WHAT CAN YOU OFFER? / SHARE YOUR BREAD / HOW ARE YOU KEEPING? / FAREWELL.`, without `LET ME SEE YOUR WARES. / HOW ARE YOU KEEPING? / WORK THE FORGE / ...`; grouping by count (game_dialogue.cpp:562-587) | TALK | yes |
| 5 | shop and menu not in the Witness text | yes | `state` keys in the shop: mode,day,...,digest (no shop); `state.menu` is the bare tab index (5); witness.json `play.menu` returns only "shop"/"menu"; no ground items as entities, no toasts in `play.notice` | WITNESS | - |
| 6a | the ruin warden kills a level-2 hero in ~12 s | yes (danger unseen) | warden = golem at ruin level + 1, hp x1.3 (foes_game.cpp:729-760): 353 hp = 160 x 1.7 x 1.3, a level-8 warden in a level-7 ruin, ~40 a blow vs ~110 hp; the danger skull is never shown indoors (hud.cpp:237) | VIEW (+ WITNESS) | yes |
| 6b | menu tab keys awkward | yes | Q/E (PageUp/Down) switch tabs everywhere; A/D switch tabs on list pages but move inside EQUIP/PARTY/SKILLS/SPELLS (input.cpp:73-96); no page shows the keys (EQUIP shot) | VIEW | yes |
| 6c | selling a worn item needs a second Enter | by design | shop_ui.cpp:20 arms a worn piece; the hint reads "YOU WEAR IT - PRESS ENTER AGAIN TO SELL" (:182); testers just cannot read it | WITNESS (`shop.deal`) | - |
| 6d | guards' arms unreadable at NORMAL | partly, not cheap | seed 7 Oslars guard (`held sword.straight, sheath sword`): helm, tabard and shield read at NORMAL and CLOSE, the sword is sheathed and does not show | - (decision 4) | - |
| 6e | leather gambeson reads as a cream tunic; every body icon the same white vest | yes | gear_look.cpp:337 paints a Padded/Brigandine top in the maker's cloth; seed 7 shop: BRIGANDINE and GAMBESON icons both grey-white vests (pixels 240,235,225 / 161,156,158), gloves and boots brown | ART | yes (art) |
| 6f | the dead companion stands over its pile | yes, cosmetic | probe G: the pile (3 pickups) lies on the first frame of the death while the body is still upright in its fall | VIEW | - |
| 6g | the mace is the least distinct swing | not changed | owner accepted it (D113) | - | - |
| 7a | wave_gates needs WITNESS_HOME from a clone | yes | wave_gates.sh:229, witness_pack.mjs:86, witness_smoke.sh:32 default to `$REPO/../witness` | GATES | - |
| 7b | rebase_baseline prints "No such file" | yes | rebase_baseline.sh:169 `tr ... < "$R/gates/doll/$n.rc" 2>/dev/null`: the input redirect fails before stderr is redirected (reproduced) | GATES | - |
| 7c | load-sensitive lines | yes | logs: `FAIL: prepinterior: nothing made ahead for INN` (seeds 1,5,9,10,17; test_window.cpp:613 3 s wall cap); `towns: per-phase gate ... budget 8 ms`; `B10 atlas and speed ... 0.454 ms at p95 (the plan's 0.385)`; `damaged saves: 35 loaded ... 13 refused` vs `41 ... 7`; `sim-digest FAIL: 532/560 ... 1 of 20 seeds differ` | GATES | - |

Dropped, with evidence: 1b (false: beasts drop sellable goods; the gap is the testers' text, fixed in 5), 6c (a deliberate
safety prompt that exists on screen; its words go into the state), 6d (a visible watch is an art and behaviour batch:
decision 4), 6g (owner ruling D113).

---

## 2. Item 1: the early armour economy

### 2.1 What is true at 25d47da (after P3's B9 shelf)

- **Start:** 30 gold (game.cpp:203), two BREAD (value 8, sells for 3). The opening gives the OLD BLADE (value 12) and a
  MINOR HEALTH POTION, 20 XP, no gold.
- **Prices:** a piece is worth `valueBase x statScale(il)^1.5 x (1 + 0.5 rarity) + 12 x affixes x statScale` (gear.cpp:1026);
  body armour's base is 80 (gear.cpp:846), so a Common coat costs 80 / 91 / 102 at item level 1 / 2 / 3 and 113 at 4 (Vant).
  Selling pays 2/5 of value (gear.cpp:304). P3 B9 gives every smith a light coat (always leather) and a metal coat (bronze
  in band 1) at the shelf's item levels D-2..D+2 (game_shop.cpp:59, 104-105). Seed 7's smith GAOLD (shot C1): BRONZE
  BRIGANDINE 80, LEATHER GAMBESON 80, LEATHER GLOVES 20, LEATHER BOOTS 23, BRONZE HELM 32, BRONZE HEATER 45. The testers'
  "80 here, 113 in the next town" is still true.
- **Beasts** (game_items.cpp:228-231, 276-279; checked in play, probe D): wolf 60 % WOLF PELT (sells 4) + 50 % RAW HIDE (3);
  boar 60 % VENISON (5) + 50 % RAW HIDE; spider 50 % SPIDER SILK (5); goblin 2-9 gold. About **4 gold a beast kill**,
  picked up by the magnet when the hero stands near (40 px after 0.5 s). Beasts drop no gold.
- **Kill rate:** the metrics bot (`rpg_test --metrics --seeds 1..6 --metric-secs 900`) killed 24 / 16 / 13 / 12 / 5 / 0
  in 15 minutes: mean 12, so a hunter makes about 48 gold of goods in 15 minutes: 30 + 48 = 78, one short of a coat.
- **The first job** (probe E, the start inn's ANY WORK GOING? on day 0, seeds 1-12): wolves 118 / 101 / 121 g (seeds 1, 7,
  12: the beginner's job); a bandit chief or a dungeon heirloom 290 / 229 / 203 / 235 g (2, 5, 6, 9: far beyond a level-1
  hero, "a few hours' walk"); a supply run half a day away 63 / 47 g (3, 11); no job at all (4, 8, 10).
- **Design:** VISION 15.9 "slow rags to riches: the player must not get rich fast or get great gear fast"; PLAN.md's
  game-feel table "gold for a first gear upgrade: after 1 to 2 quests"; VISION 15.1 the first weapon within 5 minutes;
  the owner: beginner gear "okay/mid", starting armour simple. ARMOUR_SPAWN_RARITY.md section 4: band 1 is leather or
  bronze at T1.

So a newcomer who hunts is one coat short at 15 minutes; on 4 of 12 seeds the first job is far beyond a level-1 hero, on 2
it is a long supply run that pays 47-63, and on 3 there is none. The quest path is fine where a beginner job exists.

### 2.2 The levers, and the pick

| lever | what it would do | verdict |
|---|---|---|
| pelts and hides worth more | raise WOLF PELT / RAW HIDE values | **no**: the drops exist and are taken (probe D); RAW HIDE is a craft good whose price the census sets (craft.cpp:31, game_shop.cpp's census pass); the testers' "nothing" is text, fixed by WITNESS; with a 48-gold coat five kills' goods are enough |
| the first quest pays | coin from the opening, or a guaranteed beginner job at the inn | **not now**: the opening is the blade (VISION 15.1); a guaranteed beginner hunt at the start inn is the right fix for seeds 2/4/5/6/8/9/10, but it is a JOBS-pool change (saved, sim digest) beyond this list: decision 1b |
| price of the T1 pieces | leather costs less than the band's metal | **yes (L1)**: every shelf prices a leather piece at 3/5 of its value (the smith's own metal costs what it does): leather 48, bronze 80 at item level 1 |
| a cheap T1 piece on the shelf | a beginner's coat always on a band-1 smith's shelf | **yes (L2)**: in band 1 the B9 light coat is always Common at the shelf's lowest item level (with its own Rng, so no other row changes) |

**L1 + L2:** a band-1 smith always sells a plain leather coat, Common, item level max(1, D - 2), at 3/5 of its value:
**48 gold** at the start (80 x 3/5). The bronze coat stays 80-102, so the ladder reads in the price too (the testers' "the
brigandine is only +1 armour for the same 80"). Rules kept: shelves still by the settlement's band (VISION 7.5 rule 4),
nothing above Rare, no new random draw in the shared shelf stream, sell 2/5 untouched.

### 2.3 Numbers that must fit together

- 30 start gold + 18 = the 48-gold coat. 18 gold = 5 beast kills at ~4 g (wolf 3.9, boar 4.5, spider 2.5) = about 6
  minutes at the bot's 0.8 kills a minute, plus a walk to the smith to sell: **first armour at about 7-9 minutes** for a
  hunter, about 5-8 for a newcomer who takes the wolves (101-121 g) where that job exists. Inside the owner's 10-15.
- No faucet: the coat bought at 48 sells back for 19 (2/5); a found leather coat (value 80) still sells for 32 and joins
  the shelf at 80. The village purse (300) covers a few sales.
- Slow riches kept: the next step, the bronze coat (80-102) or the 113-gold coats of the next town, still needs a quest or
  two (PLAN game-feel target); iron at band 2 (level 9, about 2.5 h) is untouched.
- Leather exists only in band 1 (gear.cpp:554-562 `matFor`), so L1 touches band-1 shelves only.

### 2.4 How it is measured

`rpg_test --first-armour --seeds A..B [--report] [--gates]` (new, `tools/tests/test_first_armour.cpp`), on the real game
(`newEndlessGame`, the start village's own smith and innkeeper, `LootOps::shopStock`, `Game::hasOffer`/the offer text,
beasts spawned at the start's level and killed through `Game::kill`, values through `gear::sellPrice`):
- **FA1** the start village has a smith whose day-0 shelf holds a Common leather body piece at <= 48 gold (20/20 seeds);
- **FA2** the expected goods per beast kill at the start (the start land's hunt targets, 200 kills each, sold) >= 3.0 gold;
- **FA3** the hunter model: ceil((cheapest coat - 30) / goods per kill) <= 9 kills (12 minutes at 0.8 a minute, the
  15-minute bot mean measured at 25d47da) on 20/20 seeds;
- **FA4** no faucet and no side effects: the beginner's coat sells back for <= 40 % of its price; every shelf row other
  than the light coat keeps 25d47da's name, item level and rarity, and only leather gear pieces change price (3/5; the
  LEATHER crafting material and every non-leather piece keep theirs).
- report per seed: smith, light coat (name, price), metal coat (name, price), the inn's first job (kind, gold, or none),
  goods per kill, kills and minutes to the coat.

---

## 3. Item 2: the quest pointer

**Cause.** `Game::acceptQuest` (game_quests.cpp:46-52): "a new job never takes the pin from an open quest the player is
following" (the 2026-10 playtest rule, after "accepting a quest silently replaces the tracked quest"). At the start the
opening's completion (giveFirstWeapon, game_quests.cpp:452-457) finds no other job open and moves the pin to THE DRAGON'S
SHADOW; the wolves accepted a moment later then keep the main quest pinned and only toast "NEW QUEST: ...". The HUD goal,
the map marker, `goto goal` and the Witness `Quest` things (tracked first, witness_state.cpp:328) all follow that pin.
P3 did not touch quest tracking (no commit to the quest or dialogue files since b3463ce).

**Fix (TALK).** A just-accepted quest becomes the tracked one, loudly: the toast and banner say "QUEST STARTED: <title>",
the goal line and marker move to it, and the journal's TRACK still switches back (input.cpp:98). A street errand keeps its
own rule (takes the pin, hands it back when done). The story engine's own pins are untouched. The old rule's complaint
("silently") is met by the banner and the goal line changing in front of the player. WITNESS updates the play brief, which
today tells testers the opposite.

---

## 4. Item 3: the innkeeper's blade

**Cause.** `giveFirstWeapon` (game_quests.cpp:439) adds the blade and the potion with `announce`, so the view makes "+ OLD
BLADE" and "+ MINOR HEALTH POTION" toasts (loot_ui.cpp:360) the moment the innkeeper speaks. Toasts are not drawn while a
dialogue is open (hud.cpp:412) and wait unaged (render.cpp:2333), so the player reads "TAKE MY FATHER'S OLD BLADE" with no
sign of it, then, after FAREWELL, finds the toasts under the NEW QUEST banner on top of the innkeeper's name label (shot
A2). The Witness text never shows toasts at all.

**Fix (VIEW).** A gift given in a conversation is shown in the dialogue panel as it is said: a strip under the speaker's
name, "+ OLD BLADE  + MINOR HEALTH POTION", each name in its rarity colour (any piece added while the dialogue is open:
the opening, a reward's item, a story's gift). Such toasts are marked (`Toast::dlg`) and are not shown again in the column
after the panel closes. The panel's layout reserves the strip in `dlgLay` so taps stay on their rows. WITNESS reports them
as `state.dialogue.gifts` and lists fresh toasts in the play notice.

---

## 5. Item 4: dialogue order

**Cause.** `Game::talkTo` (game_dialogue.cpp) adds options in the order its hooks run, then section 9 groups the services
under WHAT CAN YOU OFFER? (and the inn's house business under WHAT ELSE DO YOU DO HERE?, the kindnesses under HOW ARE YOU
KEEPING?) only when the first page would hold more than `speech::FIRST_PAGE` (6) rows (game_dialogue.cpp:562-587). Any
passing option tips it: SHARE YOUR BREAD (bread in the pack and a hungry speaker, life_game.cpp:395), ANY WORK GOING? (a
job on offer), COLLECT BOUNTY, a story beat. Probe B: GAOLD is grouped with bread and flat without, so "1)" is WHAT CAN
YOU OFFER? on one visit and LET ME SEE YOUR WARES. on the next; probe E: the innkeeper's WHAT CAN YOU OFFER? is option 2 or
3 depending on whether there is a job. A tester's `choose 1` then took a smith's job by accident.

**Fix (TALK): one order, by kind, never by count.**
1. Every first-page option has a kind from one table (rows by action range or value): **standing** (the speaker's trade,
   offered on every visit: WARES, the room, rumours, blessing, lessons, the forge and the craft, the home trades, the sea
   services, HOW ARE YOU KEEPING? and the group headers) or **passing** (this visit only: a reward to collect, the main
   quest, a story scene or pitch, a job on offer, a parcel or escort, news, the beasts' word, a need or mood line, a
   companion's offer).
2. The first page lists the standing options in the table's order, then the passing ones in the table's order, then
   FAREWELL. A speaker's headers are decided by the kinds it offers (its standing services), so the same speaker shows the
   same headers on every visit; kindnesses (SHARE YOUR <FOOD>, SPARE A COIN, HIRE A DAY'S HELP) always sit under HOW ARE
   YOU KEEPING? when the speaker has it. A passing option never regroups the page.
3. A group's page lists its options in the table's order, SOMETHING ELSE. and FAREWELL last. Answers (an offer's I'LL DO
   IT. / NOT RIGHT NOW., a story scene's choices, a companion's voice choices) keep their own order.
4. The keyboard cursor starts on the first passing quest option (COLLECT, the main quest, a story scene) when there is one,
   so Enter still collects at once; the index of every standing option stays put.
5. The first page stays within `FIRST_PAGE` rows on 95 % or more of the `--dialogue` sample (the M7b lint); when a
   visit's passing options overflow, the panel pages behind MORE (already built, hud_layout.h `dlgLay`).

Side effect, declared by TALK: the test bots that pick option 0 (seed_run.cpp:359, test_sim_digest.cpp:191/236,
metrics.cpp:264) now pick the first standing option, so S2.seeds bot lines and the sim digest move where a bot talks
(section 13). `test_defence.cpp` asserts "the first option collects" (its bounty case): it becomes "COLLECT is on the first
page, the cursor starts on it, and choosing it pays".

---

## 6. Item 5: shop and menu text for testers

**Cause.** The Witness snapshot (witness_state.cpp) has no shop, the menu is the bare tab index (`state.menu`, :195), the
ground loot is not listed as things (entities are actors, doors, quest places and exits only), and the toasts are in
`state.toasts` but witness.json's play config maps neither a notice nor items nor screen text (its `menu` expression only
says "shop"/"menu"). So the play observation shows none of it, `pickup` has nothing to walk to, and the reports count
"pickups 0".

**Fix (WITNESS).** The snapshot gains (all read-only; `state.menu` stays the int that determinism.witness.json and
touch_snapshot.witness.json read):
- `state.shop` (null when no shop is open): `{ title, side: "buy"|"sell", cursor, gold, purse, deal, stock: [{ i, name,
  price, canAfford, rarity, kind }], pack: [{ i, name, price, worn }] }` (40 rows each at most). `deal` is the line the
  panel shows ("BUY FOR 48 GOLD", "NOT ENOUGH GOLD", "YOU WEAR IT - PRESS ENTER AGAIN TO SELL", "HE HAS NO GOLD LEFT").
- `state.menuPage` (null when closed): `{ tab, index, cursor, rows, hint }`: the tab's name (VIEW's `menuTabName`), the
  cursor, the rows on show (ITEMS: the pack with WORN marks; QUESTS: the journal rows and the tracked one; EQUIP: each slot
  and what it wears, the slot and candidate under the cursor), at most 24, and the page's key hint (VIEW's `menuHint`).
- the ground loot within 20 tiles as things: `{ id: 49200000 + n, type: "Item"|"Gold", kind: "item", item: true, name,
  pos, dist, hostile: false, talk: false }`, nearest first, at most 12.
- `state.dialogue.gifts`: the names given in this conversation (VIEW's `Toast::dlg`).
- `state.location.level` (the dungeon's level inside one, else the zone's) and `state.location.danger` (level - hero).
- `state.screenText`: the open shop's or menu page's lines in reading order (Witness prints it on its own line).
- witness.json and witness.play.json `play`: `item: "e.item === true"`, a `notice` that adds toasts younger than 2 s to
  `state.notice`, `menu` naming the shop's or page's title, `textCap` raised for menu and screen text, and the brief
  rewritten (the pinned new quest, Q/E for tabs, the shop rows and their prices, items on the ground and `pickup`, the
  worn-piece confirm, the danger level).

---

## 7. Item 6: the smaller notes

**6a The ruin warden (VIEW, WITNESS state).** Real as stated. One ruin in three keeps a stone warden one level above the
ruin, with 1.3x health and a 70 px aggro (foes_game.cpp:727-760); the Tomb of Kee's (353 hp) is a level-8 golem in a
level-7 ruin a few minutes from Oslars (danger by distance, VISION 7.1). Its ~40-damage slam (19 x 1.99 x the level-gap
1.06) kills a ~110-hp level-2 hero in three blows. That is the danger-by-place design (VISION 7.1/7.5: the world never
scales to the player), but the game never says so indoors: `dangerSkull` returns -1 when inside (hud.cpp:237). **Fix:** the
skull shows inside caves, ruins and shrines (not buildings) for the dungeon's level against the hero's, and entering a
dungeon 5+ levels above the hero shows a banner ("DANGEROUS GROUND" / "<NAME>: LEVEL 7 - YOU ARE LEVEL 2"). No rebalance.

**6b Menu keys (VIEW).** Q/E switch tabs on every page; A/D also switch tabs on the list pages but move within EQUIP,
PARTY, SKILLS and SPELLS, and no page says so. **Fix:** a key hint on every menu page (keyboard only; touch keeps its
buttons): "Q/E TABS" plus the page's own keys (EQUIP: "W/S SLOT  A/D CHOOSE  ENTER WEAR / TAKE OFF  [ ] TURN"). Key
behaviour unchanged (scripts and players rely on it).

**6c Selling a worn piece (dropped).** By design: the first Enter arms it and the panel says "YOU WEAR IT - PRESS ENTER
AGAIN TO SELL" (shop_ui.cpp:20, :182). WITNESS's `shop.deal` carries those words to testers.

**6d Guards at NORMAL zoom (dropped, decision 4).** Shots F1/F2 (seed 7 Oslars guard, NORMAL and CLOSE): the helm, purple
tabard and shield read; the sword is sheathed at the hip and does not show at either zoom. Town guards walk sheathed
(P2b sheathe). Making them hold their arms at posts is an art and behaviour change for its own batch.

**6e The leather gambeson and the body icons (ART).** Real. A Padded coat is painted in the maker culture's cloth whatever
its material (gear_look.cpp:333-337, an M6 rule so the worn coat matched its icon), so a LEATHER GAMBESON is a cream tunic
next to the hero's own T1 floor jerkin; and the body-armour icon reads grey-white for both leather and bronze (shot C1).
**Fix:** a leather body piece is drawn in leather (the leather ramp, a laced or stitched front), slim and simple at T1
(owner rule), clearly not the floor jerkin and not the bronze brigandine (D114 stays); every body-armour icon in its own
material (leather, bronze, iron, steel, the alloys), no two the same white vest. Pending the owner (D116+).

**6f The companion's pile (VIEW).** Real, cosmetic: the pile appears on the death's first frame while the body is still
upright. **Fix:** a fresh drop (`Pickup::t` < 0.3 s) is drawn tossed out of the body on a short arc to its spot (every
drop, beasts too); the sim and the 240 s pile are unchanged.

**6g The mace (dropped).** Owner ruling D113.

---

## 8. Item 7: gate notes

- **7a WITNESS_HOME from a clone.** wave_gates.sh:229, witness_smoke.sh:32, witness_pack.mjs:86 and perf_frame.mjs:26 fall
  back to `$REPO/../witness`, which is missing for a clone under the scratchpad. **Fix:** lib.sh resolves Witness once
  (WITNESS_HOME, else `$REPO/../witness`, else `<origin>/../witness` when the clone's origin is a local checkout, else
  `$USERPROFILE/Documents/witness`), exports it, prints which, and step/wave gates stop before the first gate with
  "Witness not found: looked in ...; set WITNESS_HOME" when none exists.
- **7b rebase_baseline.sh:169.** `2>/dev/null` after the input redirect cannot silence it: test for the `.rc` file first.
- **7c Load-sensitive lines (never loosen the idle limits).** A list `tools/gates/alone_rerun.txt` (`<gate> <regex>  #
  why`) names the five lines; when a listed line fails or differs in a loaded run, the harness reruns just that command
  alone (`slot_alone`, every slot held and the CPU quiet) with today's limits, and the gate passes only if the alone rerun
  matches the record; its line says "(alone rerun: <regex> failed under load; first log kept)". Today W1/S4 rerun failing CI
  commands and doll_gates reruns unstable timing lines; P4 adds S2.seeds (the failing seeds only:
  `rpg_test --seeds N..N`), S3.save, S10.digest / W6.digest_clang (the differing seeds: `--sim-digest --seeds N..N
  --check`) and W4.doll's B10 line. The lines: `prepinterior: nothing made ahead` (a 3 s wall cap on a worker thread),
  `towns: per-phase gate` (8 ms), `B10 atlas and speed` (0.385 ms p95), `damaged saves:` / `damaged life blocks:`
  (save_test), and the digest's differing seeds.
- **The digest flake is a determinism question first.** A digest that differs only under load means a wall clock still
  reaches the sim (the hotfix saw seed 3 from t90; 532/560 in later runs). GATES gets a 3-hour box to find it (5 loaded
  runs of `--sim-digest --seeds 3..3` beside 4 heavy jobs, `--dump` to name the field) and fix it in test code; if the
  cause is in a sim file no P4 area owns, the integrator assigns that file to GATES alone. If not found in the box, the
  alone rerun covers it and the finding is written down. The same goes for save_test's damaged-save counts (P3's C2 did not
  hold under the P3 judges' load).

---

## 9. Areas

Six areas, one owner per file. A file not listed is touched by nobody. Each area owns its moves file
`tools/gates/baseline/moves_p4/<AREA>.txt` (section 13) and its scratch dir `...\scratchpad\p4\<area>`; build dir
`build-p4-<area>` (`tools/slot.sh cmd //c "set BDIR=build-p4-<area>&& tools\\build.bat" < /dev/null`). Worktrees (the
workflow may remap): ECONOMY emb-solid-11, TALK emb-solid-12, VIEW emb-solid-13, ART emb-solid-14, WITNESS emb-solid-15,
GATES emb-solid-16; branches `fix-p4-<AREA>` from 25d47da. A Witness test on an area build: `tools/slot.sh node
../witness/bin/witness.js test witness/<file> --repeat 2 --timeout 120 --cmd "<abs build dir>/embervale.exe --witness"`
(`--cmd` needs no witness.json; or `tools/gates/witness_smoke.sh all15 --bdir build-p4-<area> --out <scratch>/w3` for
every file, which writes its own config copy).

### 9.1 ECONOMY (engineer): the first armour within reach

Files: `rpg/sim/game_shop.cpp`, `tools/tests/test_first_armour.cpp` (new), `rpg/view/scripts/economy_p4.cpp` (new: `expect
shopprice <words> <op> <n>`), `tools/scripts/p4_economy.txt` (new), `witness/p4_newcomer.witness.json` (new),
`docs/ARMOUR_SPAWN_RARITY.md` and `docs/VISION_PLAN.md` (one line each recording L1/L2 under 15.9), moves_p4/ECONOMY.txt.

Brief: section 2. L1 (leather gear pieces at 3/5 on every shelf, applied in the pass where the background price factor
is, game_shop.cpp:199-201; never the LEATHER crafting material) and L2 (a band-1
smith's B9 light coat is Common at the shelf's lowest item level, made with its own Rng after the shared stream has drawn
what it draws today, so every other row is unchanged), and the `--first-armour` measure (2.4).
- core: the 48-gold beginner's coat at every start; FA1-FA4 green on 20/20 seeds; no other shelf row changes;
- depth: the report names the seeds whose start inn has no job a level-1 newcomer can do (data for decision 1b);
- delight: the coat's card in the shop says its maker ("OSLARS WORK"), as every piece does.

Done (in the worktree):
- build rc 0, no new warning; `tools/gates/check_layers.sh` PASS;
- `build-p4-econ/rpg_test.exe --first-armour --seeds 1..20 --report --gates`: FA1-FA4 PASS, the table printed;
- rc 0 and output as the p3f2 record or declared: `--loot`, `--loot-sources --seeds 1..3`, `--loot-audit`,
  `--ladder-pace --worlds 1..20 --report --gates`, `--power-curve`, `--gear --seeds 1..3`, `--seeds 1..20` (identical: the
  bots never buy), `--sim-digest --check tools/gates/baseline/p3f2/probes/sim_digest_msvc.txt` (560/560: prices are not
  saved);
- `node tools/gates/witness_pack.mjs --bdir build-p4-econ --out <scratch>/wp --keep tools/scripts/p4_economy.txt` PASS
  (seed 7: the smith's leather coat 48, the bronze coat 80; the hero buys the coat with 48 gold and wears it);
- `witness/p4_newcomer.witness.json` passes twice (`--repeat 2`): seed 7, a hero with 30 gold kills the start's beasts
  (staged with `skill foe ... level 1` and `killnear`), takes the drops, sells them at the smith and buys the coat;
- view pack: only shop shots listed in moves_p4/ECONOMY.txt differ (`witness_pack --quick --check
  tools/gates/baseline/p3f2/probes/view_quick/hashes.txt`).

### 9.2 TALK (engineer-high): the pin and one order of options

Files: `rpg/sim/game_quests.cpp`, `rpg/sim/game_dialogue.cpp`, `rpg/sim/talk.h`, `rpg/sim/dlg_order.cpp` (new, if the
order table is its own file), `tools/tests/test_dialogue.cpp`, `tools/tests/test_quests.cpp`, `tools/tests/test_defence.cpp`,
`rpg/view/scripts/talk_p4.cpp` (new: `expect optindex <words> <n>`), `tools/scripts/p4_talk.txt` (new),
`tools/scripts/m7b_talk.txt` and `tools/scripts/m7b_talk_phone.txt` (only lines that name the old order),
`witness/p4_talk.witness.json` (new), moves_p4/TALK.txt.

Brief: sections 3 and 5. The pin rule in `acceptQuest` (errands unchanged); the option table and the first page, groups and
cursor rules of section 5; the checks below; the bots' moves measured and declared seed by seed.
- core: an accepted quest is tracked; the same speaker's shared options keep their index; FAREWELL last; nothing becomes
  unreachable;
- depth: `--dialogue` gains the order check over every keyed speaker of the start village and the nearest town;
- delight: the cursor waits on COLLECT when a reward is due.

Done:
- build rc 0; check_layers PASS; size_report no new item;
- `rpg_test --dialogue --seeds 1..3` rc 0 with its new order check named in the output (3 visits per speaker: as is; with
  bread, a job on offer and a reward due staged; without): every option present on two visits has the same first-page
  index, the headers are the same, FAREWELL is last, every option is reachable (`dlgReach`), and the first page holds at
  most 6 rows on 95 % or more of the sampled talks (the rest page behind MORE);
- `rpg_test --quests --seeds 1..3` rc 0 with the pin cases (an accepted job takes the pin; an errand takes it and hands it
  back; TRACK switches it);
- rc 0: `--content-p1b --seeds 1..3 --gates`, `--jobs --seeds 1..3`, `--quest-mix --seeds 1..20 --gates --p1b --gates-p1b
  --report --ports`, `--story --seeds 1..3`, `--story-p1 --seeds 1..3`, `--companion --seeds 1..3`, `--mainquest --seeds 1..3`,
  `--home --seeds 1..3`, `--gear --seeds 1..3`, `--lord --seeds 1..3`, `--seeds 1..20` (20 passed);
- `rpg_test --sim-digest --check tools/gates/baseline/p3f2/probes/sim_digest_msvc.txt`: every differing seed listed in
  moves_p4/TALK.txt with its first differing checkpoint and the dialogue choice that changed (found with `--dump`);
- `witness/p4_talk.witness.json` passes twice: seed 7, the wolves accepted, `state.goal.text` starts "THE WOLVES OF OSLARS"
  and the first `Quest` thing is that quest; GAOLD's options with and without bread keep each shared option's index;
- `witness_pack ... --keep tools/scripts/p4_talk.txt` PASS (owner pack shots: the HUD after accepting; GAOLD twice);
- `witness_pack --all --check .../p3f2/probes/view_all/hashes.txt`: only shots listed in moves_p4/TALK.txt differ.

### 9.3 VIEW (engineer): the gift strip, danger indoors, menu hints, the toss

Files: `rpg/view/fx_state.h`, `rpg/view/hud_layout.h`, `rpg/view/dialogue_ui.cpp`, `rpg/view/hud.cpp`, `rpg/view/render.cpp`,
`rpg/view/loot_ui.cpp`, `rpg/view/scripts/view_p4.cpp` (new: `expect dlggift`, `expect skull`, `expect banner`,
`expect menuhint`, `expect pickuplift`), `tools/scripts/p4_view.txt` (new), moves_p4/VIEW.txt.

Brief: sections 4 and 7 (6a, 6b, 6f). **First, the contract commit (wave 1, within the hour):** `Toast::dlg` (fx_state.h:
true for a toast made while the dialogue is open), `const char* menuTabName(int tab)` (the labels hud.cpp's kTabs holds,
moved into hud_layout.h) and `const char* menuHint(int tab, bool touch)` (hud_layout.h), bodies included; and the test
command `p4dungeon ruin|cave [above N]` (view_p4.cpp: enter the nearest ruin or cave whose level is at least the hero's +
N, through `Game::debugEnterSite`), which WITNESS's state test uses too.
- core: "+ OLD BLADE" on the panel while the innkeeper says it, not repeated after; the skull inside dungeons and the
  banner; a key hint on every menu page; fresh drops tossed out of the body;
- depth: the gift strip for every in-conversation gift (a reward's item, a story's gift), names in rarity colours;
- delight: the banner names the place, its level and yours.

Done:
- build rc 0; check_layers PASS; `bash tools/gates/size_report.sh --check tools/gates/baseline/p3f2/checks/size_report.txt
  --renames tools/gates/size_renames.txt`: no new item (hud.cpp and render.cpp not grown by new functions);
- `witness_pack ... --keep tools/scripts/p4_view.txt` PASS: seed 7 inn, `expect dlggift OLD BLADE` with the dialogue open
  and no "+ OLD BLADE" in the toast column after FAREWELL; after `p4dungeon ruin above 5` `expect skull 2` (or 3) and
  `expect banner DANGEROUS*`; EQUIP and ITEMS `expect menuhint Q/E*`; a fresh drop drawn above its spot at 0.1 s
  (`expect pickuplift`);
- view pack (`--quick` and `--all` against p3f2): only shots listed in moves_p4/VIEW.txt differ (dialogue panels with a
  gift, menu pages with the hint, dungeon interiors with the skull);
- `tools/gates/witness_smoke.sh all15 --bdir build-p4-view --out <scratch>/w3`: every Witness file passes (the menu and
  dialogue state the tests read unchanged).

### 9.4 WITNESS (engineer): what testers can read

Files: `rpg/view/witness_state.cpp`, `rpg/view/view_probe.h`, `rpg/view/paperdoll.cpp` (a read-only accessor for the EQUIP
cursor; no drawing change), `witness.json`, `witness.play.json`, `tools/scripts/p2c_playtests.txt` (the W7 goals),
`docs/PLAYTEST.md`, `witness/p4_state.witness.json` (new), moves_p4/WITNESS.txt.

Brief: section 6, plus the state side of 1b, 3 and 6a. Then the W7 goals for P4 (12 testers, calls allowed): keep T5 x3
(the leather step now without a gold call) and T7 x3; replace T4 (swings, owner-accepted) and T6 (guards, decision 4) with
**T8 newcomer x3** (seeds 7, 4, 2: no gold calls; get the blade, take a job if one is offered, hunt, sell, buy the first
body armour; report the game minute of the purchase, whether the goal line followed the accepted job, and whether the gift
was seen) and **T9 talker x3** (seeds 7, 1, 10: talk to the smith and the innkeeper twice with and without bread and say
whether the options moved; read a shop and a menu page from text; sell a worn piece; enter the nearest ruin and say what
the screen warns).
- core: the shop, the menu page, the ground loot, the gifts and the danger in the snapshot; witness.json/witness.play.json
  read them;
- depth: `state.screenText` reads like the screen, top to bottom;
- delight: the brief tells a tester how to read every new field in one paragraph.

Done:
- build rc 0; check_layers PASS;
- `witness/p4_state.witness.json` passes twice: seed 7 smith's shop: `state.shop.stock.length >= 10`, every row has name,
  price, canAfford; the leather coat's price equals the panel's (after ECONOMY merges: 48); SELL side lists the pack with
  `worn`; one Enter on a worn piece makes `state.shop.deal` "YOU WEAR IT - PRESS ENTER AGAIN TO SELL"; EQUIP open:
  `state.menuPage.tab == 'EQUIP'` and its hint holds "Q/E"; six boars killed 4 tiles away (`skill foe boar 4 0 unaware`,
  `killnear 6`; out of the 40 px magnet): at least two `item: true` things named RAW HIDE or VENISON, gone once the hero
  walks over them; the innkeeper's first talk: `state.dialogue.gifts` holds "OLD BLADE"; after `p4dungeon ruin above 5`
  `state.location.danger` >= 5; `state.menu` is still the int;
- `tools/gates/witness_smoke.sh all15 --bdir build-p4-witness --out <scratch>/w3`: every file passes;
- `node ../witness/bin/witness.js play check --config witness.play.json` shows items, menu and notice present;
- `node tools/gates/witness_pack/goals_check.mjs --bdir build-p4-witness tools/scripts/p2c_playtests.txt` PASS;
- one smoke tester on T8 (seed 7) through `witness play batch --goals <scratch>/t8.json --allow-calls --parallel 1` (on the
  merged head after ECONOMY, TALK and VIEW): its report shows shop rows, ground items and "+ OLD BLADE" in text;
- no view-pack PNG differs (the snapshot only).

### 9.5 ART (artist): leather that reads as leather, icons by material

Files: `rpg/sim/gear_look.cpp`, `rpg/art/art_items.cpp`, `rpg/art/art_items.h`, `rpg/art/doll/doll_ramps.cpp`,
`rpg/art/doll/doll_resolve.cpp`, `rpg/art/doll/maps/**` (ADD only; nothing drawn is deleted), `rpg/art/doll/gen/**`
(regenerated by `node tools/doll/pxc.mjs` only), `tools/doll/**` (looks lists; `freeze/p2c_freeze.txt` re-signed for the
changed looks only; `freeze/p2c_freeze_rig1.txt` only for the same shared doll frames, D115's reading), `tools/preview/icons_gallery.cpp`
(new, optional: a sheet of body icons per material), `tests/fixtures/golden_village_art.txt` (only if leather-coat
villagers change), `docs/PLAN.md` (D116+: one decision per art change), `tools/scripts/p4_art.txt` (new), moves_p4/ART.txt.

Brief: 6e. Follow docs/STYLE_P2.md and the envelope (D42-D96); T1 stays simple and slim; the brigandine (D114) and the
floor jerkin (hotfix) keep their looks; rig 1 untouched. Every change goes into the owner pack as before/after, pending.
- core: a T1 LEATHER GAMBESON / JERKIN in leather browns with a leather detail, clearly unlike the floor jerkin and the
  bronze brigandine, man and woman, all four facings; every body-armour icon its material's colour and mark;
- depth: the leather helm (CAP), gloves and boots icons and worn pieces agree with the coat;
- delight: the leather coat's laces or stitching catch the light one pixel brighter than the hide.

Done:
- `node tools/doll/pxc.mjs` then `--check`: up to date, pool under its budget (2,621,440 B);
- rc 0 or as the record: `rpg_test --doll --gates`, `--doll-arms --matrix arms`, `--doll-ladder`, `--doll-envelope`,
  `--doll-coverage --scope all --seeds 1..20 --gates`, `--arms --seeds 1..3`;
- `rpg_test --doll-wear --freeze-check tools/doll/freeze/p2c_freeze.txt` 0 changed after a re-sign whose diff holds only
  leather Padded/Jerkin looks (listed in moves_p4/ART.txt); rig 1: `art_hash --rig=1` and `city_gallery --rig=1
  --village-art-check tests/fixtures/golden_village_art_rig1.txt` identical;
- `tools/gates/doll_gates.sh --bdir build-p4-art --out <scratch>/doll`: every line as the record except moves_p4/ART.txt's;
- measured: in a 1x shot the leather coat's torso mean colour lies in the leather ramp (hue 20-40 deg, saturation >= 0.30)
  and differs from the floor jerkin's (shirt colour) and the brigandine's; the body icons of leather, bronze, iron and
  steel differ pairwise in mean colour by >= 30 (RGB distance);
- `witness_pack ... --keep tools/scripts/p4_art.txt` PASS; the owner pack sheets written (section 15).

### 9.6 GATES (engineer-high): the gate notes

Files: `tools/gates/lib.sh`, `tools/gates/wave_gates.sh`, `tools/gates/step_gates.sh`, `tools/gates/doll_gates.sh`,
`tools/gates/rebase_baseline.sh`, `tools/gates/witness_smoke.sh`, `tools/gates/perf_pack.sh`, `tools/gates/witness_pack.mjs`,
`tools/gates/perf_frame.mjs`, `tools/gates/alone_rerun.txt` (new), `tools/gates/baseline/README.md`,
`tools/gates/baseline/moves_p4/GATES.txt`, `tools/tests/test_sim_digest.cpp`, `tools/save_test.cpp`,
`tools/tests/test_window.cpp` and `tools/tests/test_towns.cpp` (only if a cause found lives there), `README.md`,
`docs/ARCHITECTURE.md`. Not `tools/gates/witness_pack/` (goals_check is untouched) and not the other areas' moves files.

Brief: section 8 (7a, 7b, 7c, the digest and save_test hunt), then, merging last, the README "Where is ..." rows and the
ARCHITECTURE notes for the other areas' new things (the first-armour measure, the option table, the Witness fields, the
alone-rerun list), sent to GATES by each area as one line in its moves file's header.
- core: a gate run from a clone finds Witness; no stray "No such file"; a red line means a real change, measured alone;
- depth: the digest flake's cause found (or written down with the 5 loaded runs);
- delight: the wave summary names, per alone rerun, how loaded the first try was.

Done:
- from a throwaway clone (SDL by junction, rule 3) with WITNESS_HOME unset: `tools/gates/step_gates.sh --bdir <b> --out
  <o> --only S9` runs and its log names the Witness it found; with HOME pointing at an empty dir and no ../witness:
  `wave_gates.sh` exits 2 before any gate with "Witness not found: looked in ...";
- `tools/gates/rebase_baseline.sh --selftest` PASS, and a negative test with a missing doll `.rc` prints no "No such file";
- `GATES_SELFTEST=alone_rerun bash tools/gates/lib.sh`: a listed line that fails under fake load passes only after an alone
  rerun that matches; a listed line that fails alone FAILs; an unlisted line is never rerun;
- loaded A/A: two `tools/gates/step_gates.sh --only S2,S3,S10.digest` runs beside 3 other heavy jobs both PASS (their alone
  reruns named), and the idle limits in the tests are byte-identical to 25d47da (`git diff 25d47da -- tools/tests` shows no
  changed limit);
- the digest: 5 loaded runs of `rpg_test --sim-digest --seeds 3..3 --check tools/gates/baseline/p3f2/probes/sim_digest_msvc.txt`
  beside 4 heavy jobs identical, or the finding in moves_p4/GATES.txt; same for `save_test`'s damaged-save lines;
- `tools/gates/step_gates.sh --bdir build-p4-gates --out <scratch>/step` on GATES' branch: every line as the record.

---

## 10. Contracts

- **VIEW's contract commit (wave 1, before WITNESS's state for gifts and menus):** `struct Toast { ...; bool dlg = false; }`
  in fx_state.h; `const char* menuTabName(int tab)` and `const char* menuHint(int tab, bool touch)` in hud_layout.h, with the
  words of 6b; the test command `p4dungeon ruin|cave [above N]` in rpg/view/scripts/view_p4.cpp. WITNESS includes
  hud_layout.h and fx_state.h read-only and calls `p4dungeon` from its test.
- **WITNESS's shapes** are section 6's; they are additive and `state.menu` stays an int.
- **ECONOMY's numbers** (the coat at 48 in band 1 at the start) are read by WITNESS's p4_state test and TALK/ART owner shots,
  never by their code.
- **Moves files:** `tools/gates/baseline/moves_p4/<AREA>.txt`, the P3 grammar (`<gate> <item glob>[,...] <id> <reason>`,
  `rebase_baseline.sh --help`); each area measures against the p3f2 record on its own branch before it hands over; ids are
  `P4-1`, `P4-2`, `P4-3`, `P4-4`, `P4-5`, `P4-6a/b/e/f`, `P4-7`.
- **The digest:** only TALK may move `S10.digest` / `W6.digest_clang`; every other area shows 560/560 on its branch.

## 11. Waves and integration

- **Wave 1 (all six at once):** VIEW's contract commit first (about an hour); everybody else starts immediately (WITNESS
  writes the shop, menu rows, ground loot and danger parts, then rebases onto the contract for gifts and the tab names).
  No other barrier.
- **Integration** (merges in `emb-solid-int` on `fix-p4-int`; disjoint files, so a conflict is a plan error): 1. ECONOMY,
  2. TALK, 3. ART, 4. VIEW, 5. WITNESS (its state test reads the coat price, the pin and the gift strip), 6. GATES (last:
  README rows, and the alone-rerun list governs the regression). After each merge, that area's own Done lines on the merged
  head (fix rounds verify only their fixes). Then the one full regression (section 14).

## 12. What it should look and feel like (for the areas and the judge)

A newcomer at seed 7, keyboard: three minutes in, the innkeeper's panel shows "+ OLD BLADE + MINOR HEALTH POTION" under his
name; "ANY WORK GOING?" is where it was on the next talk; THE WOLVES OF OSLARS is on the goal line and the map; the wolves'
pelts and hides toast as they are picked up; at the smith the leather coat costs 48, the row under the 80-gold bronze
brigandine; worn, it is brown leather, not the cream jerkin; inside the Tomb of Kee a red skull and "DANGEROUS GROUND"
come up before the warden does. A Witness tester reads every one of those as text.

## 13. Baselines: expected moves

Each area writes moves_p4/<AREA>.txt; the integrator runs `GATES_MOVES_DIR=tools/gates/baseline/moves_p4
tools/gates/rebase_baseline.sh` from p3f2 to **p4**. Anything else moving is a bug.

| gate | expected | why (area) |
|---|---|---|
| S1.build | 36 exes (37 if ART adds icons_gallery); warnings <= 366, no new kind | ART |
| S2.seeds | the bot lines of seeds whose bot's option 0 changed; nothing else | TALK |
| S3.save, S3.savever | identical; SAVE_VER 24 | - |
| S3.fixtures | `golden_village_art.txt` only if leather-coat villagers change | ART |
| S4.clang, W1.msvc_ci | 69 commands, 0 fails (the new checks live inside `--dialogue` and `--quests`; `--first-armour` is a judge line) | - |
| S5.rig2 | categories holding leather Padded/Jerkin looks | ART |
| S5.rig1, S7.rig1 | identical | - |
| S6.freeze | 0 changed against ART's re-sign (diff reviewed) | ART |
| S6.freeze1 | identical, or the same looks' shared doll frames (D115) | ART |
| S6.pool | bytes and hash only if ART adds views | ART |
| S7.rig2 | groups with leather-coat wearers | ART |
| S8, W5.firstlaunch | unchanged | - |
| S9.test / S9.shot | pass; `village_start.png` only if a leather coat shows at t = 2 s | ART |
| S10.digest, W6.digest_clang | act/rng/marks/ser/len from each listed seed's first checkpoint where a bot's dialogue choice differs | TALK |
| S10.view, W2.view_all | shops (leather prices, the beginner's coat, icons); dialogue panels (order, the gift strip); the HUD goal after accepting; menu pages (hint); dungeon interiors (skull, banner); leather-coat wearers | ECONOMY, TALK, VIEW, ART |
| S10.audio | identical | - |
| S10.layers | PASS, no new violation | all |
| W2.scripts | 0 FAIL; new p4_*.txt PASS | all |
| W3.witness | 18 -> 21 files (p4_newcomer, p4_talk, p4_state), all pass x2 | ECONOMY, TALK, WITNESS |
| W4.doll | lines touching leather Padded/Jerkin looks; B10 under the alone-rerun rule | ART, GATES |
| W6.perf | within the record's noise + 3 % | - |
| W7.play | the P4 goals (T5, T7, T8, T9); read the reports | WITNESS |

## 14. The full regression (once, at integration)

The owner's rule: one all-systems regression per update; fix rounds rerun only their own lines.

```sh
cd /c/Users/joshu/Documents/emb-solid-int        # on fix-p4-int with all six areas merged
S=/c/Users/joshu/AppData/Local/Temp/claude/C--Users-joshu/1979ea42-238a-4969-bda8-d76ab6bf2de5/scratchpad/p4
export WITNESS_MAX_PROCS=5 WITNESS_MAX_BROWSERS=1   # keep-awake loop running (PC sleep trap)
# 0. a fresh clone and build dir, SDL by junction (rule 3), no network fetch
git clone -q --no-hardlinks . $S/p4clone && git -C $S/p4clone -c advice.detachedHead=false checkout -q "$(git rev-parse HEAD)"
mkdir -p $S/p4clone/build/_deps
cmd //c mklink /J "$(cygpath -w $S/p4clone/build/_deps/sdl3-src)" "C:\Users\joshu\Documents\embervale\build\_deps\sdl3-src"
# 1. ONE wave run against CURRENT (p3f2), W7 with the P4 goals (GATES 7a finds ../witness from the clone's origin)
( cd $S/p4clone && tools/gates/wave_gates.sh --bdir build-p4wave --out $S/p4wave --w7 )
# 2. every difference declared, every declared move seen
GATES_MOVES_DIR=tools/gates/baseline/moves_p4 tools/gates/rebase_baseline.sh --check-moves --run $S/p4wave --bdir $S/p4clone/build-p4wave
# 3. re-record the declared items into tools/gates/baseline/p4, flip CURRENT; commit with REASONS.md
GATES_MOVES_DIR=tools/gates/baseline/moves_p4 tools/gates/rebase_baseline.sh --from p3f2 --to p4 --run $S/p4wave --bdir $S/p4clone/build-p4wave
# 4. confirm the rebase on the moved lines only, with the clone's build (no second full run)
tools/gates/step_gates.sh --bdir $S/p4clone/build-p4wave --out $S/p4recheck --only S2,S3,S5,S6,S7,S10
node tools/gates/witness_pack.mjs --bdir $S/p4clone/build-p4wave --out $S/p4recheck/view --all \
     --check tools/gates/baseline/p4/probes/view_all/hashes.txt
# 5. the P4 judge lines on the clone's build
B=$S/p4clone/build-p4wave
tools/slot.sh $B/rpg_test.exe --first-armour --seeds 1..20 --report --gates
tools/slot.sh $B/rpg_test.exe --dialogue --seeds 1..3
tools/slot.sh $B/rpg_test.exe --quests --seeds 1..3
tools/slot.sh $B/rpg_test.exe --ladder-pace --worlds 1..20 --report --gates
tools/gates/witness_smoke.sh all15 --bdir $B --out $S/p4recheck/w3
# 6. clean up: never rm -rf through the junction
cmd //c rmdir "$(cygpath -w $S/p4clone/build/_deps/sdl3-src)"
test -f /c/Users/joshu/Documents/embervale/build/_deps/sdl3-src/CMakeLists.txt && rm -rf $S/p4clone
```

Time: P3's wave gates took 61-67 min of wall time with 5 slots; W7 adds up to about 4 h (12 testers, 2 at a time); about
2.5x slower while the owner plays (cap 2).

## 15. The owner pack

Before (25d47da build) / after images from hidden Witness runs at the same seed and spot, under
`...\scratchpad\p4-pack\<item>\`, each with a one-line caption:
- **art, pending the owner's review (ART):** the T1 leather coat on the man and the woman, four facings, beside the floor
  jerkin and the bronze brigandine; a bandit and a militiaman in leather; the shop's body icons by material (6e);
- **visible changes:** the seed-7 smith's shelf (80 -> 48, the beginner's coat; ECONOMY); the HUD and map after accepting the
  wolves, and GAOLD's first page with and without bread (TALK); the innkeeper's panel with "+ OLD BLADE", the Tomb of Kee's
  skull and banner, the EQUIP page's hint, a death pile landing (VIEW).

## 16. Decisions for the owner (the plan proceeds on the first reading of each)

1. **The first armour:** leather sells at 3/5 on every shelf and a band-1 smith always has a Common leather coat at the
   shelf's lowest level: 48 gold at the start, about 5 beast kills' goods. Pelts, hides and quest rewards unchanged.
   1b. Not built: a guaranteed beginner hunt at the start inn (3 of 12 start inns have no job and 4 of 12 send a level-1
   newcomer after a bandit chief or into a dungeon): a JOBS change for a later batch if the owner wants it.
2. **The pin:** a just-accepted quest becomes the tracked one (this reverses the 2026-10 rule "a new job never takes the
   pin"); the banner and the goal line say so, and the journal's TRACK switches back.
3. **Dialogue order:** the speaker's trade first in a fixed order, this visit's extras after it, FAREWELL last; COLLECT
   moves from first to just above FAREWELL, with the cursor starting on it.
4. **Guards' weapons in town (6d):** not in P4: a visible watch (arms in hand at posts) would be an art and behaviour batch.
5. **The ruin warden (6a):** danger shown, not rebalanced: the skull inside dungeons and a banner on entry.
6. **W7 for P4:** T4 (swings) and T6 (guards) give way to T8 (newcomer) and T9 (talker); T5 and T7 stay.
7. `--first-armour --gates` is a judge line this batch; adding it to the CI list is a follow-up.

## 17. What the judge checks

From the state a player starts in, look and feel first, measured (eyes for shots, numbers for the rest):
- item 1: `--first-armour --gates` green; in play (seed 7, no gold calls) a hero buys the coat for 48 after hunting; the coat
  reads as brown leather; no other shelf row moved;
- item 2: accepting the wolves moves the goal line, the map marker and `goto Quest` to the den; an errand still hands the pin
  back;
- item 3: "+ OLD BLADE" on the innkeeper's panel while he speaks, not repeated after FAREWELL; `state.dialogue.gifts`;
- item 4: GAOLD and the innkeeper twice (with and without bread, with a job and without): every shared option at the same
  number; `--dialogue` order check named and green;
- item 5: a Witness tester (T8/T9) reads the shop rows with prices and can-afford, the menu page, the ground items and the
  gifts in text; `pickup` works;
- item 6: the skull and banner in the Tomb of Kee; the key hint on every menu page; the leather coat and the icons in the
  owner pack; a death pile lands as the body falls;
- item 7: the gate notes' Done lines; the regression (section 14 steps 1-5) clean; `REASONS.md` lists every moved item.
