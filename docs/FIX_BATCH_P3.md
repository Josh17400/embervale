# Fix batch P3: the loot and companion rules, the playtest fixes, the gate tools, the creator under Witness, the mix

Planner, 2026-11-01. Base **b3463ce** (main == solid, the SOLID refactor's last commit), branch `fix-p3-int` in the
integration worktree. Read with `README.md` (the code map), `docs/ARCHITECTURE.md` (layers and seams),
`docs/SOLID_PLAN.md` section 5 (the rules for new code), `docs/ARMOUR_SPAWN_RARITY.md` (revision 6: the rules' design)
and `docs/VISION_PLAN.md` 15.37 (the owner's rulings). The owner said "go ahead" to all of it (scopes A-E).

**In one paragraph.** This is the first batch after SOLID that is allowed to change behaviour. It ships the armour and
companion rules the owner ruled on 2026-10-28 (where you are decides the metal, luck decides the finish, old places hold
the old makers' metals; a companion keeps their own kit, takes better gifts, drops everything where they fall, and comes
back as the same friend), it fixes the twelve things playtests and judges found, it repairs the gate harness so a red
line means a real problem, it lets the creator scripts run headless, and it makes the sea's sounds audible over the
music. **How it should feel:** a sword cut reads as a cut, an axe and a mace each hit like themselves, a bow is drawn
before it is loosed, a bronze coat reads as metal, a smith has a real choice of body armour, a pickup says what you
picked up, a friend you send home comes back as themselves with your gifts handed back first, and every gate either
passes or names a real change with its reason.

Absolute rules for every area (from the batch brief; they bind the areas and the integrator):
1. No visible window: `embervale.exe` only with `--witness` (game scripts through `tools/gates/witness_pack.mjs`, Witness
   tests through `witness test`). A true non-harness run uses `SDL_VIDEODRIVER=offscreen`. rpg_test, art_hash, save_test,
   city_gallery and audio_preview are CLI and fine.
2. Heavy processes only through `tools/slot.sh` (cap 2 while the owner plays a game, else 5). Never kill an Embervale.exe
   you did not start; never touch `%APPDATA%\Josh17400\Embervale` or the Desktop. Temp output under
   `C:\Users\joshu\AppData\Local\Temp\claude\C--Users-joshu\1979ea42-238a-4969-bda8-d76ab6bf2de5\scratchpad\p3\<area>`;
   delete PNGs when done (the owner pack, section 12, is kept until the owner has seen it).
3. Throwaway clones and worktrees link `build/_deps/sdl3-src` to the main checkout by a junction: before deleting one, run
   `cmd //c rmdir <clone>\build\_deps\sdl3-src`, then check `C:\Users\joshu\Documents\embervale\build\_deps\sdl3-src\CMakeLists.txt`
   still exists. Never `rm -rf` through a junction.
4. Behaviour changes only where your area owns them: every other gate stays identical. A baseline your change moves is
   declared in your moves file (section 10) with its reason; you never re-record a baseline yourself and never loosen a
   gate. A save format change bumps SAVE_VER with a migration and a fixture test; old saves load.
5. Owner rules: the rags-to-riches ladder; every alloy and material its own armour shape and its own helmet; starting armour
   simple and not bulky; companions equip better gear given to them, "if they die they drop everything", kit capped at the
   hero's level + 4, a 240 s dropped pile, a friend who rejoins keeps person and level; never delete drawn art or rig-1 art.
   An art change the owner might find "off" goes into the owner pack for review, never silently shipped as final.
6. SOLID section 5: new code in its lane and layer, no new layer violation (`tools/gates/check_layers.sh`), no function
   over 300 lines (new code under about 150), tables over switches, test-only code only in `rpg/view/scripts/` and
   `tools/tests/`. `rpg/view/hud.cpp` (2,214 lines) and `rpg/view/render.cpp` (3,841) are over the 2,000-line guide: edit
   existing lines there and put any new function in a smaller neighbour.

---

## 1. The items at a glance

Every item was checked at b3463ce with the build `emb-solid-int/build-solid-w5wave` (built 10:58 after b3463ce; b3463ce
itself changed docs only). Repro shots came from one hidden Witness run of a throwaway script
(`witness_pack.mjs --bdir build-solid-w5wave --keep p3_repro.txt`, 23 PNGs, read and deleted).

| id | item | real at b3463ce? | evidence (short) | area | owner pack |
|---|---|---|---|---|---|
| A | loot and companion rules (Batch B) | not built at all | no IF_KIT, relicPct, parted roll, spoils, luck gate (b) in the code (2.1) | LOOT, PARTY, VIEW | yes (2.5) |
| A-bug | partyDismiss deletes the hero's gifts | yes | `partyDismiss` -> `PartyOps::removeMember` erases the member and `m.inv` (party_game.cpp:912-920, 58-63) | PARTY | - |
| B1 | sword live strike reads as a bow | yes | live strike shots: the Steel Arc crescent frames a level blade = bow and arrow | ART | yes (art) |
| B2 | axe and mace frozen strikes look idle and alike | yes | `pose strike axe/mace 0 0` facing R: both a low forward hold at the hip, near-identical | ART | yes (art) |
| B3 | no notice when web saves are off | yes | web/index.html:42-53 and save_store.cpp:41-56 only `console.warn/log` | HARNESS | yes |
| B4 | the bow fires with the other weapon in hand | yes | `key k` with sword + bow equipped: the sword stays in hand, the bow on the back | ART | yes (art) |
| B5 | script `key D` does not turn the hero | yes | `p2cline face 0`, `key d`: still facing down; `key` pushes down+up in one frame, `View::input` reads held keys only | HARNESS | - |
| B6 | spear U thrust goes WNW | yes | spear.clip `strike U ... weapon=157.5` (0 = east, 90 = up): WNW; the shot shows the tip west of the head | ART | yes (art) |
| B7 | creator preview is half size | yes (code) | `menuDollScale(..., swing=true)` reserves the T6 envelope (41 px up) and the swing's reach: 2x on a 144 px stage, v1 drew 5x | VIEW | yes |
| B8 | zoom "close" is not close | yes (code) | CLOSE = D0 + 2 within 150-300 rows: 1.4x NORMAL at 1080/1179 px, == NEAR at 540/720 px; world:fit caps K at 4 | VIEW + HARNESS | yes |
| B9 | smiths stock no mail or plate | partly | a smith stocks ONE body armour (game_shop.cpp:98), 60 % leather at ilvl <= 4; "plate near the start" is by design (steel from band 3) | LOOT | yes |
| B10 | bronze brigandine reads as cloth | yes | doll_resolve.cpp:790 paints Upper/Fore in Cloth1 + gear_look.cpp:337 the maker's cloth; shot: a red-brown tunic | ART | yes (art) |
| B11 | loot goes into the pack with no message | partly | a toast DOES show the bare item name (shot: "LEATHER ARMOR", top left); it has no verb, and the Witness state has no toasts | VIEW | yes |
| B12 | "goto Exit" picks a fight with a boss | yes, cause in Witness | Witness autopilot.js `autofight` fights any aggro hostile within reach x 2.2 during a goto; Embervale's snapshot has no `boss` field | VIEW (+ Witness, decision 2) | - |
| C1 | W7 batch without `--allow-calls` | yes | wave_gates.sh:222; 3 batches: 9 of 12 testers blocked; T5/T7 goals name resets testers cannot run | GATES + HARNESS | - |
| C2 | S3.save damaged-save count moves with load | yes | save_test plays with the stream worker and the realm's 2 ms wall-clock slice; "damaged saves: 35/13" and "damaged life blocks" are unmasked | PARTY (owns save_test.cpp) | - |
| C3 | the "alone" reruns are not alone | yes | lib.sh `slot_alone` takes only slots 1..cap: with the gaming cap 2 it runs while slots 3-5 still run; no check for heavy work outside slot.sh | GATES | - |
| C4 | size_report matches by function name | yes | size_report.sh header; renamed functions count as new | GATES | - |
| C5 | `--look-key` not in the CI list | yes | not in web.yml / kCiList (tools/tests/main.cpp:62) | GATES | - |
| C6 | quest-mix `--ports`, nondeterminism | `--ports`: yes; drift: not reproduced | SOLID_PLAN 1.1/3.x lines lack `--ports`; A/A identical (seeds 1..4 x2, 5..9 x4 side by side); the test runs with the stream worker and the 2 ms realm slice (the two drift sources the sim digest removes) | GATES | - |
| D | 9 creator scripts SKIP under Witness | yes | witness_driver.cpp:184 `wantNewGame` -> `startNew` skips the creator; witness_pack.mjs:184 SKIPs `expect mode creator` | HARNESS | - |
| E | audio_preview mix check red | yes | 7 sea sfx fail; quietest ReelClick -34.0 dB (bed -25.9): margin -8.1, phone -5.1 | AUDIO | yes (WAVs) |
| P3 | SOLID judges' small notes | yes | render_sorted.cpp setMarketSnow bracket; doll.h comment file list; mark_tags.h rows carry bare numbers | VIEW / ART / LOOT | - |

Nothing is dropped whole. Dropped parts, with evidence: B9's "plate near the start" (by the owner's ladder, steel is band
3), B11's "no message" (a toast exists), B12's autofight rule (it lives in Witness, a separate repo: decision 2), and C6's
"nondeterminism bug" (not reproduced; it is hardened and documented, not "fixed").

---

## 2. A. The loot and companion rules (Batch B of docs/ARMOUR_SPAWN_RARITY.md)

### 2.1 Audit at b3463ce

**The look-only parts (Batch A) are done** (P2c wave-2 fix rounds), and gated by W4.doll (PASS at b3463ce, wave-5 run):

| look part | done? | where | gate |
|---|---|---|---|
| hero T5 gilded onion helm, T6 starmetal crystal helm (D83) | done | doll_resolve.cpp:394-430, 512-521 (`fam.onion`, `fam.crystal`); gear_slice lt5/lt6 | `--doll-ladder` (ladder_finish lines) |
| Mythic black only for bronze, iron, steel and the old metals; a Mythic alloy keeps its alloy with the ember (D84, D32 restated) | done | gear_ladder.cpp:65-67 `metalKindOf` (FIX_RELIC), 216-237; doll_resolve.cpp:193, 592, 630-651 | `--doll-ladder`, `--doll-coverage` |
| D61 (starmetal bands, crystal helm) and D68 (T6 legend view; its open point closed by D83) | done | doll ramps / helm views (envelope r2, fix round) | W4 env_ladder, forge_* lines |
| starmetal growth by ADDed sizes inside the caps (D85, D92) | done | relic invariant | `--doll-ladder` "relic invariant" |
| bandit chief's ladder tier (D86: T2 home, T3 far) | done | game.cpp:1029, sim_schedule.cpp:18 | `--doll-coverage` |
| crafted looks (alloy T4+, masterwork T5 only from band 4; D86) | done | gear_ladder.cpp:54-60 `lookTier` | `--doll-ladder` |
| the Q3 marks (an `epic` bit, a band-1 Epic's trim, the T5 Rare glyph, the gold inlay) | **NOT done** (D86 deferred them) | HumanLook has no `epic` | out of this batch (decision 6) |

**The rules (Batch B) are not built.** `grep` at b3463ce finds no `IF_KIT`, no `relicPct`, no `Party::parted`/`MAX_PARTED`,
no spoils, no shared "better" rule in gear.h (the compare arrow's `lootp::verdict` is view-only, loot_paint.h:319):
- ruling 1: `matMult` gives an alloy `1.12 + 0.04 x tierStep` whatever the band (gear.cpp:950-963), so a band-6 masterwork
  can be weaker than the band's step; no crafting cap by rank;
- ruling 2: no relic decision; ruling 6: `rarityAllowedTop` still lifts luck one step above the gate (gear.cpp:156-161),
  so Legendary can roll from level 12; ruling 7: shelves and commissions have no alloy cap (game_shop.cpp:45-58,
  craft_game.cpp); ruling 10: no war spoils (war_game.cpp `WarOps::kill`:894); ruling 11: kits made with the place's
  danger (party_game.cpp:89-122), no IF_KIT, `partyEquip` calls `partyUnequip` first (the swap leak, :869), death copies
  the pack to the grave (:214), every parting deletes the record (:912-920, the live gift bug);
- IN extras: `storyLoot` passes no culture (story.cpp:134, story_internal.h:63); door interiors' chests have no maker
  (game_items.cpp:328) and read the zone level with interior coordinates (:377); `dropLoot` has no summon / thrall /
  decoy test (:168-170); the commission reseeds from the hero's gold (craft_game.cpp:187).

Tests at b3463ce (build-solid-w5wave): `--companion --seeds 1..3` 0 failures; `--loot-sources --seeds 1..3` 0; `--loot` 0;
`--power-curve` 0; `--ladder-pace` 3 checks (LP1-LP3) PASS; `--wards --seeds 1..3` failed once on its timing line ("an
overlay apply costs 0.200 ms a settlement (target 0.2)") while another agent's build ran: a load-flaky timing line (C3).

### 2.2 What this batch builds (the gap), by post-SOLID file

The design is docs/ARMOUR_SPAWN_RARITY.md: section 6 (companions, lines 532-722: the six loops, rules 1-8 of the owner's
amendment, "The fix in code" at 678), section 8's Batch B table (lines 912-928), section 9 (how it is checked, lines
987-1072; the `--companion` list 1020-1049), and VISION_PLAN 15.11 / 15.37.
Its file:line citations are at df2a155; at b3463ce they live here:

| design citation (df2a155) | now (b3463ce) |
|---|---|
| game_rpg.cpp dropLoot 189-300, openChest 302-393, chestSource 395-436 | rpg/sim/game_items.cpp `LootOps::dropLoot` 168, `Game::openChest` ~285, `LootOps::chestSource` 374 |
| game_rpg.cpp shelves 2090-2116, sell 2303 | rpg/sim/game_shop.cpp `LootOps::shopStock` 33 (smith 95-110), `Game::sell` |
| game_rpg.cpp SAVE_VER 2404, refusal 2357-2360 | rpg/sim/game_save.cpp `SAVE_VER` 58, `SAVE_VER_MIN` 101, checks 111 / 366 |
| game.cpp pickups 1710-1734, clearNonPlayer 1946, load 83 | rpg/sim/game_combat.cpp `Game::updatePickups` 657 (unchanged: the pile reuses it); game.cpp (unchanged) |
| game.h:351-352 the spoils counter | a saved mark per siege, `(Mk)(32 + n)` in the WARDS range, one row in rpg/sim/mark_tags.h (no Game member: SOLID rule 2) |
| party_game.cpp 88-122, 191-273, 858-922 | same file: `kit` 89, `dieForGood` 192, `leave` 246, `applyDeed` angry 314, `partyEquip` 856, `partyUnequip` 878, `partyDismiss` 912 |
| party.cpp writeMember/readMember 308-383, block 413-460 | same file 308 / 331 / 413-452; `party.h` MAX_RECORDS 181 |
| party_recruit.cpp 105-156, 183-388 | same file: `mapChanged` 105, `partyOffer` 183, `partyRecruit` 275 |
| party_talk.cpp 622-656 (apology), 497 (ANGRY) | same file 496 |
| loot_paint.h 500-506 (maker row), verdict | same file 497-503, `verdict` 319 |
| story.cpp 163-167 | rpg/story/story.cpp 134 + story_internal.h:63 + callers story_effects.cpp:191, story_game.cpp:60 |

**Rules built (core):** rulings 1, 2, 6, 7, 10 and 11 exactly as ruled (VISION 15.37 table; the 12 rules), plus the IN
list: the metal explained on the card ("HIGHLAND WORK: GWESTEEL, WARRIORS' METAL (2 OF 3)", "OLD MAKERS' WORK: OBSIDIAN
(2 OF 4)") and the HUD naming each worn piece's own metal; saga loot and building chests given a maker; no loot from
summons, thralls or decoys; the commission seeded per smith and restock (day / 2). **Companions (rev 4-6, rules 1-8):**
the kit in their people's make, made once at the join, ilvl capped at hero level + 4, rarity rolled with the hero as the
finder, every piece IF_KIT; a gift is worn only if better (gear::powerScore at their level more than 3 % over the worn
piece, the compare arrow's rule moved into gear.h), a tie or worse is declined in their voice and nothing moves; a worn gift
stows their kit piece; TAKE BACK returns only gifts and re-wears the stowed kit; one door from companion to hero that
refuses IF_KIT (TAKE BACK, swap, every parting); every living parting hands every gift back (the live bug) and sets a
sellsword's hire mark to that day; death lays everything as ground Pickups where they fell (IF_KIT cleared, nothing into
the pack, 240 s, gone at a map change or load, no pile off the hero's map), the grave holds nothing to take; the parted
roll keeps the whole record (`Party::parted`, MAX_PARTED 32, oldest forgotten first) and a rejoin restores person, bond,
skills, record and the kit as it left (made again only for an old save's companion who holds none); level reads "nothing
they earned resets"; an angry sellsword asks the apology at his table; the inn stands a parted sellsword in his own kit; a
parted census companion's record leaves at their census death. Selling and breaking down refuse IF_KIT.
**Depth:** a companion declines a piece their vocation never uses. **Delight:** the party page marks the pieces the hero
lent and lists the friends you parted from and where they wait; a parted sellsword waits at his table in the kit he left
with; "JOINS YOU AGAIN." Optional lore line (priors.cpp): **out** (culture data; keeps the culture goldens untouched).

### 2.3 The save (rule 4 of this batch)

- `SAVE_VER` 23 -> **24**, `SAVE_VER_MIN` stays **23** (the M9 precedent, game_save.cpp:99-101): a v23 save loads through
  the migration below. The design said "23 holds" because a bump refused old saves; with MIN 23 nothing is refused, and the
  batch rule asks for the bump (decision 1).
- The party block's own `VERSION` steps to 5 and still reads 4 with an empty parted roll (party.cpp:431 refuses any version
  but its own today, and a refused block refuses the save). Records are length-prefixed members (`writeMember`/`readMember`).
- `IF_KIT = 16` uses a free bit of the item flags byte, already saved (items.h:111). The spoils counter is a saved mark.
- Fixture test: `tests/fixtures/save_v23.bin` is never edited and must still load (party block v4 -> empty roll); a new
  `tests/fixtures/save_v24.bin` (`save_test --make-fixture`) carries a parted companion and IF_KIT pieces and round-trips.
- A pre-batch kit has no IF_KIT: it counts as a gift once (accepted in the design); a grave laid before keeps its
  belongings and keepsake; a companion dismissed before has no record and joins anew once.

### 2.4 Numbers that must fit together

- **Save size:** the soak save is 74,024 B; a full parted roll adds at most 32 x about 1.2 KB = 38.4 KB: 112.4 KB, inside
  the 150 KB budget (`--save-soak`, save_test 2b).
- **Relics:** from band 4 about half the metal gear in old places is a relic; target about 1 armour find in 8;
  `--ladder-pace` fails outside 8-20 % of metal armour drops in its assumed hour. A relic is exactly as strong as a floored
  alloy of its band.
- **Luck gate (b):** floors unchanged (Legendary 20, Mythic 30); luck lifts to Legendary only from level 17 and to Mythic
  only from 27; campaign finales and the dragon's hoard exempt.
- **Alloys:** everyday band 4, warriors' 5, masterwork 6-7; `matMult(alloy) >= bandStep(band)`; craft caps by rank 4/5/6.
- **Kit:** ilvl = min(place D, hero level + 4); level sync already uses a piece at <= companion level + 4 = hero + 3, so a
  companion's strength at the join is unchanged.
- **Pile:** 240 s, magnet 40 px, taken at 9 px, potions wait while the belt is full (the existing Pickup rules).
- **Spoils:** soldiers 10 %, captains 50 % (Uncommon or better), at most 3 a siege, item level capped by the hero's level.
- **Power:** same-band trash still dies in 3-5 hits (`--power-curve`), the best allowed rarity still 2.5+.

### 2.5 Owner pack for A

Shots (Witness): a companion declining a worse gift and taking a better one (party page, arrow at their level), TAKE BACK
greyed on a kit piece, the dismissal alert "2 GIFTS COME BACK", the death pile on the ground, the parted list, the rejoin
page; a relic card and an alloy card with the rank row; a smith's shelf before/after (B9).

---

## 3. B. Game fixes

**B1 sword live strike reads as a bow (ART).** Real. Root cause: the Steel Arc (art_fx.cpp, drawn by
`View::drawSwingFx`, render_anim.cpp:600) is a closed crescent ahead of the hand, and the sword's strike frames hold the
blade level across its chord, so arc + blade = a drawn bow and an arrow (strongest facing R; also in the frozen T4 strike).
Fix: the sword's strike reads as a cut: the blade diagonal through the strike frames (sword.clip, sword_shield.clip), the
trail a smear that ends at the blade's tip and never closes ahead of the hand (art_fx.cpp / render_anim.cpp sizing).
Other families' arcs unchanged unless the same read appears.

**B2 axe and mace frozen strikes look idle and alike (ART).** Real. Root cause: axe.clip `strike R ... armR=ax_1s
weapon=315` and mace.clip `armR=mc_1s weapon=292`: both put the head low in front at the hip after the blow, the same
silhouette as a relaxed hold. Fix: redraw the strike frames (every facing, variants 0-2) at the blow: the axe a diagonal
chop with the head at shoulder-to-chest height and the torso turned; the mace a downward smash, head above the waist, the
body leaning in. Check: weapon-head centres axe vs mace >= 4 px apart, each >= 4 px from the idle frame's.

**B3 no on-screen notice when web saves are off (HARNESS).** Real. Root cause: when IDBFS cannot mount or sync, the web
shell (`web/index.html` preRun) and `writeSave` (save_store.cpp) only log to the console. Fix: the shell records the state
(`Module.embSaves = 'off'|'failed'`); `App::probeSave` reads it (EM_ASM_INT) and sets the title note "SAVES ARE OFF IN THIS
BROWSER - PROGRESS WILL NOT BE KEPT"; a failed sync shows "THE GAME COULD NOT BE SAVED" once (the existing notice path).
Desktop builds unchanged. Check: `tools/witness_web/firstlaunch.mjs` gains `--no-idb` (IndexedDB blocked in the page).

**B4 the bow fires with the other weapon in hand (ART).** Real. Root cause: the look's hand rule (gear_look.cpp:299-314:
melee, else staff, else bow) never changes for a shot; `shootArrow` puts the hero in `AState::Cast`, and `pickV2` plays the
spell gesture with the sword in hand. Fix, draw side only: while the hero shoots (Cast with no spell held, `eqBow` set,
the bow not already in hand) `drawHumanBody` (render_anim.cpp:91) draws and picks from a copy of the hero's look with the
bow in hand and the melee weapon put away, so the Release and Draw clips play; the paperdoll and the sim state keep the worn
look. No sim change (the digest bot never shoots).

**B5 script `key D` does not turn the hero (HARNESS).** Real. Root cause: `App::runCmd` `key` pushes the down and up
events in the same frame (script_runner.cpp:522-525) and never sets the held state; `View::input` (input.cpp:15-18) moves and
turns from held keys only. Fix: in Play mode, `key` on a movement key (W A S D, arrows) holds it for exactly one sim step
(the existing `held` list, release after one step), as a real tap lasts at least a frame: the hero turns and moves one step.
Menus are unchanged (they read the key events). The party radial must stay as it is (m8_view_party.txt identical).

**B6 the spear U thrust goes WNW (ART).** Real. Root cause: spear.clip `strike U` frames carry `weapon=157.5,back`
(clip angles are 0 = east, 90 = up; mace.clip's note "a270" is straight down), i.e. west-north-west. Fix: the U thrust
points up (about 90, foreshortened, the head above the helm within 2 columns of the body's centre); check every facing's
thrust against its facing. Variants 0, 1 and the glaive variant 2.

**B7 the creator preview is half size (VIEW).** Real (by the layout arithmetic). Root cause: `View::menuDollScale`
(render.cpp:982) starts its box at the T6 envelope (41 px above the pivot, sides >= max(e6 shoulders / 2, 8 + cloak back))
and the creator asks for the swing's frames too (creator.cpp:408): on the 142 x 144 px stage (480 x 270, footY 190) that
gives 2x; v1 drew the 16 x 24 sprite at 5x. Fix: the creator sizes by the shirt look's own idle, walk and run frames (no T6
reserve: the creator never shows armour), giving 4x on that stage; the swing shows only when it fits at that scale
(`menuSwingFits`), and the figure never changes size. The paperdoll keeps its envelope sizing.

**B8 zoom "close" is not close (VIEW + HARNESS).** Real. Root cause: CLOSE is D0 + 2 kept inside 150-300 world rows
(screen.cpp:369-385, screen.h:50-51): at 1080 / 1179 px tall it is 7 against NORMAL 5 (1.4x); at 540 and 720 px CLOSE is not
allowed and falls back to NEAR; the play shot's world:fit caps K at 4 (witness_driver.cpp:219-232), so a tester's CLOSE
shot is only 4/3 of NORMAL's. Fix: CLOSE = min(2 x D0, floor(h / 100)) and always above NEAR (its own lower bound of 100
rows; FAR, NORMAL, NEAR unchanged): 2.0x NORMAL at 540, 720, 1080, 1179 and 1440 px. HARNESS lets world:fit grow K to 8
under the same 1568 px / 1.15 MP limits.

| height | D0 | NORMAL | NEAR | CLOSE now | CLOSE after |
|---|---|---|---|---|---|
| 540 (Witness) | 2 | 2 | 3 | 3 (= NEAR) | 4 (135 rows) |
| 720 | 3 | 3 | 4 | 4 (= NEAR) | 6 (120 rows) |
| 1080 | 5 | 5 | 6 | 7 | 10 (108 rows) |
| 1179 (phone) | 5 | 5 | 6 | 7 | 10 (117 rows) |
| 1440 | 6 | 6 | 7 | 8 | 12 (120 rows) |

**B9 smiths stock no mail or plate (LOOT).** Partly real. Root cause: a smith's shelf holds one body armour
(game_shop.cpp:98), 60 % leather below ilvl 5 (gear.cpp:561), so a band-1 town often sells no metal coat at all. By the
ladder, plate (steel) starts at band 3, so "plate at the start" stays out. Fix: a smith stocks two body armours, the band's
light piece and the band's metal piece (band 1 leather + bronze, band 2 leather + iron, band 3 iron + steel, band 4+ steel
or the everyday alloy + the next metal up to ruling 7's warriors' cap), each in its people's form; the helm as today.
HARNESS rewrites the T5 playtest goal to the ladder.

**B10 the bronze brigandine reads as cloth (ART).** Real. Root cause: a brigandine paints Upper and Fore in the maker's
cloth (doll_resolve.cpp:790, gear_look.cpp:337) with a rivets finish that is noise at 1x. Fix: it reads as a coat of plates:
the cloth darkened behind rows of plates and rivet heads in the metal's ramp, metal at the shoulders' edges; still simple
and slim at T1-T2 (owner rule: starting armour not bulky; no pauldron growth). Its helm unchanged. Changes every brigandine
wearer (hero, guards, bandits): art_hash, freeze and village art move (declared).

**B11 loot goes into the pack with no message (VIEW).** Partly real. `Game::addItem` already emits `Ev::Notice` and the
HUD shows a toast with the bare item name (shot: "LEATHER ARMOR" under the gold counter). Root causes: the toast reads as a
label, not an event, and the Witness state has no toasts (only `notice`, witness_state.cpp:172), so testers see nothing.
Fix: an item toast reads "+ LEATHER ARMOR" in its rarity colour (hud.cpp:1409 draw, loot_ui.cpp `lootNotice`); the Witness
state gains `toasts` (the last 5: text, age). No sim change.

**B12 "goto Exit" picks a fight with a boss (VIEW; Witness part is decision 2).** Real; the game is right (the Ruin
Warden aggroes on a passing hero). Root cause: Witness's `autofight` (../witness src/play/autopilot.js:1576-1601) starts
a fight with any aggro hostile within reach x 2.2 during a goto, and Embervale never sends `boss`, so Witness's observe.js
`BOSS` label (:286, :314) never shows. Fix in Embervale: each entity in the snapshot gains `boss` (rank >= 4: dungeon and
world bosses) and `rank` (0 ordinary, 1 elite, 2 champion, 3 named, 4 boss, 5 world boss). Fix in Witness (separate repo,
owner's OK): autofight never starts a fight with a `boss` thing during goto / explore; it says "a BOSS bars the way" and the
order stops for the tester to choose.

---

## 4. C. Gate tools

**C1 W7 without `--allow-calls` (GATES; goals by HARNESS).** wave_gates.sh:222 starts `witness play batch` without
`--allow-calls`; in the three W7 batches (11010107, 11010706, 11010849) every T4 and T6 tester was blocked (9 of 12) and
T5/T7 named a `female` reset and a `ladderkit` reset no tester can run. Fix: the W7 line passes `--allow-calls`; the goals
file (tools/scripts/p2c_playtests.txt) gives T5's woman as goal `params` `{"female": true}`, T7's tiers as
`call ladderkit lt1` .. `lt6`, and T5 a ladder goal (leather, then the land's metal from the smith, then `call gotoculture`
or travel for the next metal).

**C2 S3.save moves with load (PARTY, which owns save_test.cpp).** `play()` runs the stream worker and the realm's 2 ms
wall-clock slice, so the save's size (and so where the 48 random flips land) differs under load: "damaged saves: 35 loaded,
13 refused" and "damaged life blocks: 87 loaded" move; two other lines are already masked. Fix: save_test's games set the
digest's determinism switches (`streamThreads = false`, `syncStreams = true` before the new game, `realm.focusBudgetMs =
1e18` after it: test_sim_digest.cpp:139-146). Every line is then stable, and the two S3.save masks in unstable.txt go
(that file may only shrink).

**C3 the alone reruns are not alone (GATES).** `slot_alone` (lib.sh:189-230) takes slots 1..cap and stops there: while
the owner plays, the cap is 2, so it runs with jobs still holding slots 3-5; it never looks at heavy work outside slot.sh
(perf_pack.sh does, with its idle-CPU wait). Fix: hold every slot 1..EMB_SLOTS (wait for 3-5 to drain whatever the cap),
then the perf_pack quiet wait (CPU at the machine's idle + 15 %, capped at 240 s) moved into lib.sh and shared; every
rerun line says `alone` or `NOT ALONE (held k/5, cpu x %)`. Timing lines seen flaky under load: `--doll --gates` B10,
`--towns` (wave 5), `--wards` overlay cost (this audit).

**C4 size_report renames (GATES).** `--check` matches functions by name, so a function renamed by SOLID counts as new.
Fix: `--renames tools/gates/size_renames.txt` ("old new" per line, the SOLID renames listed) maps baseline names first.

**C5 `--look-key` in CI (GATES).** Add `./build-native/rpg_test --look-key` to web.yml's test job and to `kCiList`
(tools/tests/main.cpp:62): 68 -> 69 commands (`rpg_test --list-ci` equals `tools/gates/ci_cmds.sh`).

**C6 quest-mix (GATES).** `--gates-p1b` needs the port town in the visit set; SOLID_PLAN.md:402 and :750 omit `--ports`.
Fix: `--gates-p1b` implies `--ports` (CI already passes it, so its output is unchanged) and the plan lines say so. The
drift: two A/A pairs were identical here (seeds 1..4 twice, seeds 5..9 four times side by side), but the test runs with the
stream worker thread and the realm's wall-clock slice, the two A/A drift sources the sim digest documents (probes notes,
test_sim_digest.cpp:136-146). It is a timing effect of the probe's setup, not a sim bug: the test sets the same switches,
and a loaded A/A (20 seeds, two runs beside three other heavy jobs) must print identical lines apart from the "(N s)"
timings. If a drift remains after that, it is a real nondeterminism bug: bisect it by checkpoint as the digest does.

---

## 5. D. Creator under Witness (HARNESS)

Under `--witness`, New Game skips the creator (`App::wStep`, witness_driver.cpp:184: `wantNewGame` -> `startNew`), so
witness_pack SKIPs every script with `expect mode creator` (witness_pack.mjs:184): creator, inn, m2_ui_screens,
m3_people_creator_phone, p2c_flip_look, p2c_flip_look_phone, p2c_menus_creator, p2c_menus_creator_phone, sim_inn.
Hook: the App flag `wCreator` (runtime only), set by the reset param `"creatorUi": true` (pool runs, Witness tests) or the
launch flag `--witness-creator` (scripted runs: all nine start on the title, so they run on fresh `--witness --script`
instances). With it, `wStep` does what `frameRequests` does on New Game (`startNew`, `beginCreator`, `creatorPrepare`),
so BEGIN hands over to play as in the windowed game. Default behaviour unchanged (playtests and the existing tests keep
skipping the creator). witness_pack passes the flag to scripts that hold an `expect mode creator` line and drops that SKIP
reason (the `blur` SKIP stays). Done when all nine PASS; a script that still fails for a harness gap is fixed in the
harness, never by weakening the script's checks. A new `witness/p3_creator.witness.json` covers the reset-param path.

## 6. E. The audio mix (AUDIO)

`audio_preview` (tools/audio_preview.cpp:885-897) asks every gameplay sfx (steps, talk, menus, animals, cheer, hoof and
the spell beds excepted) to stand 6 dB over the loudest music bed (Combat, -25.9 dB) in its loudest 200 ms, 6 dB over the
loudest phone bed (-28.9 dB) through the phone filter, and to peak at or over the loudest music peak (0.295). Seven sfx, all
added in M10 after the check was written, fail (levels at master 0.6, music 0.7):

| sfx | trim now | peak | 200 ms | phone | needs | fix |
|---|---|---|---|---|---|---|
| ReelClick | 3.0 | 0.200 | -34.0 | -34.0 | +14.6 dB (would clip) | denser ratchet (more clicks per 200 ms at peak <= 0.6) + trim |
| SailFlap | 2.4 | 0.173 | -22.6 | -30.6 | +8.2 dB (phone) | trim about x2.6 (peak about 0.44) |
| OarStroke | 1.2 | 0.243 | -26.3 | -26.6 | +6.9 dB | trim about x2.2 (peak about 0.54) |
| MerrowChatter | 2.0 | 0.234 | -22.9 | -22.9 | +3.5 dB | trim about x1.5 |
| LineSnap | 1.2 | 0.559 | -21.0 | -21.0 | +1.6 dB | trim about x1.2 |
| FishPlop | 1.3 | 0.355 | -21.0 | -21.0 | +1.6 dB | trim about x1.2 |
| MistWhisper | 1.0 | 0.445 | -14.2 | -23.6 | +1.2 dB (phone) | trim about x1.15 |

Targets with 0.5 dB of margin: 200 ms >= -19.4 dB, phone >= -22.4 dB, peak >= 0.31, every peak <= 0.8 (the limiter's line,
sfx.cpp:43) and 0 clipped. In play these are further scaled at the call (render_sea.cpp:487-711: 0.45-0.9), so they stay
under a combat hit (Hit over Combat: peak 0.774). No new exemption is added (decision 4). Only these seven `sfx_*.wav` and
the full mode's exit code change in audio_pack; the report names the quietest gameplay sfx.

## 7. The SOLID judges' P3 notes (cheap, folded in)

- `rpg/view/render_sorted.cpp`: the open-coded `setMarketSnow` on/off bracket becomes a `MarketSnowScope` (art_props.h
  already has it). VIEW. View pack identical.
- `rpg/art/doll/doll.h`: the header comment's file list names today's doll files (compose, paint, passes, look fields,
  resolve, ramps, alloys). ART. Comment only.
- `rpg/sim/mark_tags.h`: one named `constexpr` per register row, used by the `kMarkTags` rows in place of bare numbers;
  `tools/tests/test_marks.cpp` static_asserts each lane's own constant equals it (lanes untouched; key hashes unchanged).
  LOOT (it also adds the spoils row).

---

## 8. Areas

Seven areas, one owner per file. A file not listed is touched by nobody. New files in existing folders need no CMake edit.
Every area also owns its moves file `tools/gates/baseline/moves/<AREA>.txt` (section 10) and its scratch dir
`...\scratchpad\p3\<area>`. Build dirs: `build-p3-<area>` (`set BDIR=...&& tools\build.bat` through tools/slot.sh).

### 8.1 LOOT (engineer-high): the loot rules, war spoils, commissions, shop shelves

Files: `rpg/sim/gear.h`, `rpg/sim/gear.cpp`, `rpg/sim/items.h`, `rpg/sim/items.cpp`, `rpg/sim/loot_ops.h`,
`rpg/sim/game_items.cpp`, `rpg/sim/game_shop.cpp`, `rpg/sim/craft_game.cpp`, `rpg/sim/craft_hooks.h`, `rpg/sim/war_game.cpp`,
`rpg/sim/war_ops.h`, `rpg/sim/mark_tags.h`, `rpg/story/story.cpp`, `rpg/story/story_internal.h`, `rpg/story/story_effects.cpp`,
`rpg/story/story_game.cpp`, `rpg/view/scripts/loot.cpp`, `tools/tests/test_loot*.cpp`, `tools/tests/test_folk_sim.cpp`,
`tools/tests/test_wards.cpp`, `tools/tests/test_marks.cpp`, `tools/tests/test_power.cpp`, `tools/scripts/p3_loot.txt` (new),
`docs/ARMOUR_SPAWN_RARITY.md`, `docs/VISION_PLAN.md`.

Brief: first the **contract commit** (wave 1): `IF_KIT = 16` (items.h); `DropSource::relicPct` (runtime, never saved);
`gear::verdictAt(const Item& it, const Item* worn, int level, int* pct)` and `gear::betterAt(...)` (the compare arrow's
rule moved from `lootp::verdict`, numbers identical; `lootp::verdict` delegates until VIEW switches); the relic and rank
helpers' declarations; `gear::makerLine(it, maker)` (the card's maker row, today's text until the rules land) and
`gear::metalWord(it, maker)` (the HUD's word for a worn piece), so VIEW never waits for wave 2. Then rulings 1, 2, 6, 7, 10 and the IN extras (2.2), the spoils counter as a mark row, sell and
break-down refusing IF_KIT, B9's shelf, and the mark_tags P3 note. `--ladder-pace` gains the relic, metal-rank and spoils
rows and the fail list (`--worlds a..b`, `--gates`), registered as a gate.
- core: the rules exactly as ruled, every source through the game's own makers, no new random draw except the commission
  seed and the spoils roll;
- depth: the relic slot table (the old makers' hero styles), the card's rank words come from one table, chests in door
  interiors take the town's maker and the interior's level;
- delight: a smith's shelf with a real choice (B9); a relic in a band-4 ruin chest reads "OLD MAKERS' WORK".

Done (commands from the area's worktree):
- `tools/slot.sh cmd //c "set BDIR=build-p3-loot&& tools\\build.bat" < /dev/null` rc 0, no new warning;
- `build-p3-loot/rpg_test.exe --ladder-pace --worlds 1..20 --report --gates`: every fail-list line PASS (no rarer piece
  drawn plainer; every alloy at its band and strength; no shelf or commission masterwork outside the two exemptions; relics
  only from band 4, only in old places, never with a culture id, 8-20 % of metal armour drops; no Legendary below 17 or
  Mythic below 27 outside finales and the hoard);
- rc 0: `--loot-sources --seeds 1..3` (new WAR SPOILS and RELIC rows), `--loot`, `--loot-audit`, `--power-curve`,
  `--metrics`, `--foes --seeds 1..3`, `--arms --seeds 1..3`, `--wards --seeds 1..10`, `--marks`, `--seeds 1..20`;
- `rpg_test --sim-digest --check tools/gates/baseline/119de74/probes/sim_digest_msvc.txt` identical, or every differing
  seed and field named in moves/LOOT.txt with its cause;
- `node tools/gates/witness_pack.mjs --bdir build-p3-loot --out <scratch>/wp --keep tools/scripts/p3_loot.txt` PASS (the
  start town's smith shows a leather and a bronze coat; a relic card; an alloy card's rank row);
- `tools/gates/check_layers.sh` PASS; moves/LOOT.txt complete.

### 8.2 PARTY (engineer-high): companions' kit, gifts, death pile, parted roll, the save

Files: `rpg/sim/party_game.cpp`, `rpg/sim/party.h`, `rpg/sim/party.cpp`, `rpg/sim/party_recruit.cpp`,
`rpg/sim/party_talk.cpp`, `rpg/sim/banter.cpp`, `rpg/sim/game_internal.h` (PartyOps' declarations; nobody else edits it),
`rpg/sim/game_save.cpp`, `tools/save_test.cpp`, `tests/fixtures/save_v24.bin` (new), `tools/tests/test_party*.cpp`,
`rpg/view/scripts/party.cpp`, `witness/p3_party_return.witness.json` (new), `tools/scripts/p3_party.txt` (new).

Brief: after LOOT's contract, the **party contract commit** (`Party::parted` and its record, MAX_PARTED 32, VERSION 5 reading
4, PartyOps declarations; the record's fields VIEW reads for the parted list: name, origin, home site id, inn building id
and slot, left day, angry), then companions rules 1-8 (2.2) and the live gift bug, SAVE_VER 24 / MIN 23 with the fixture
test (2.3), C2 (save_test determinism), and the script hooks `party give <item> [m]`, `party takeback <slot> [m]`,
`party snap <name>`, `expect party same <name>` (id, bond, approval, memories, kit seeds by slot).
- core: the eight rules, the one door, the parted roll, the save;
- depth: a companion declines a piece their vocation never uses; an angry sellsword's apology at his table;
- delight: "JOINS YOU AGAIN."; a parted sellsword waits at his table in the kit he left with.

Done:
- build rc 0; `rpg_test --companion --seeds 1..20` rc 0 with every case of ARMOUR_SPAWN_RARITY.md section 9's `--companion`
  list (lines 1020-1049) named in its output, the gift-at-dismissal case first, and the seeded fuzz (each kit seed lives
  once across pack, members, parted roll and ground; no IF_KIT ever in the pack or on the ground);
- `save_test` ALL OK, including "save_v23 loads (party block v4, empty parted roll)" and the save_v24 round trip; two
  `save_test` runs side by side with 3 other heavy jobs print identical output (C2);
- rc 0: `--banter --seeds 1..3`, `--save-soak 100`, `--seeds 1..20`, `--loot-sources --seeds 1..3` (kit and fallen-pile rows);
- `rpg_test --sim-digest --check <119de74 msvc record>`: only `ser`/`len` and S lines differ (SAVE_VER 24, party block v5),
  `act`, `rng` and `marks` identical in all 560 checkpoints (listed in moves/PARTY.txt);
- `tools/slot.sh node ../witness/bin/witness.js test witness/p3_party_return.witness.json` passes (dismiss, ask back at home,
  `expect party same`).

### 8.3 VIEW (engineer): the loot and party UI, creator, zoom, pickup toast, Witness snapshot

Files: `rpg/view/hud.cpp`, `rpg/view/loot_paint.h`, `rpg/view/loot_card.h`, `rpg/view/loot_ui.cpp`, `rpg/view/paperdoll.cpp`,
`rpg/view/creator.cpp`, `rpg/view/render.cpp`, `rpg/view/render_sorted.cpp`, `rpg/view/view.h`, `rpg/view/fx_state.h`,
`rpg/view/witness_state.cpp`, `rpg/view/screen.cpp`, `rpg/view/screen.h`, `rpg/view/scripts/view_p3.cpp` (new),
`witness/p3_view.witness.json` (new), `tools/scripts/p3_view.txt` (new).

Brief: B7, B8 (the CLOSE level), B11, B12's Embervale part, the render_sorted P3 note at once; after the two contract
commits, A's UI: the card's rank row and relic row, the HUD's metal word, and on the party page TAKE BACK greyed on an
IF_KIT piece (pressing it plays the companion's line), the decline shown as the companion's line, the compare arrow read at
the companion's level through `gear::verdictAt`, the lent-piece mark and the parted list (where each waits).
- core: the items above with no change to rarity colours, no sim change;
- depth: toasts and boss/rank in the Witness state so testers read them in text;
- delight: the parted list's "WAITS AT THE <INN>" once a sellsword's week is out.

Done:
- build rc 0; `tools/gates/check_layers.sh` PASS; `bash tools/gates/size_report.sh --check tools/gates/baseline/119de74/checks/size_report.txt` no new item (hud.cpp and render.cpp not grown by new functions);
- `node tools/gates/witness_pack.mjs --bdir build-p3-view --out <scratch>/wp --keep tools/scripts/p3_view.txt` PASS:
  `zoom close` gives world scale 4 at 960 x 540 and 10 at the phone window (`expect zoomscale`), an item pickup's toast reads
  "+ <NAME>", the party page and loot card lines (`expect lootcard`);
- `tools/slot.sh node ../witness/bin/witness.js test witness/p3_view.witness.json` passes (`state.toasts` holds the pickup;
  a staged boss carries `boss: true, rank: 4`);
- after HARNESS merges (integration): the creator scripts' figure measures 4x (eyes: the figure's height in
  `creator_1_looks.png` >= 1.9x b3463ce's), every gait at one size;
- view pack (`witness_pack --quick --check ...view_quick/hashes.txt`): only shots listed in moves/VIEW.txt differ.

### 8.4 ART (artist): the swings, the spear, the brigandine, the bow draw

Files: `rpg/art/doll/clips/actions/sword.clip`, `sword_shield.clip`, `axe.clip`, `mace.clip`, `spear.clip`,
`rpg/art/doll/maps/**` (ADD views; nothing drawn is deleted), `rpg/art/doll/doll_resolve.cpp`, `rpg/art/doll/doll.h`
(P3 comment), `rpg/art/art_fx.h`, `rpg/art/art_fx.cpp`, `rpg/view/render_anim.cpp`, `rpg/sim/gear_look.cpp`,
`rpg/art/doll/gen/**` (regenerated by `node tools/doll/pxc.mjs` only), `tools/doll/**` (the clip ledger
`p2c_clips.ledger.json`, the looks lists, `owner_pack.py`; `freeze/p2c_freeze.txt` re-signed for the changed frames only,
`freeze/p2c_freeze_rig1.txt` never), `docs/PLAN.md` (one decision per art change), `tools/scripts/p3_art.txt` (new).

Brief: B1, B2, B4, B6, B10 and the doll.h note. Follow docs/STYLE_P2.md and the envelope (docs/PLAN.md D42-D96); rig 1
untouched (its art_hash, village and freeze lines identical); a new look ADDs views. Every change goes into the owner pack
as before/after and stays "pending owner review" until the owner has seen it.
- core: a sword cut that reads as a cut; axe and mace strikes that read as blows and differ; the spear thrusts where it
  faces; the bronze coat reads as metal while staying slim; the bow drawn for a shot;
- depth: the same reads facing R, D, L and U, all three combo variants, man and woman;
- delight: the axe's chop and the mace's smash each land with their own body line.

Done:
- `node tools/doll/pxc.mjs` then `node tools/doll/pxc.mjs --check`: up to date, pool under its budget (2,621,440 B);
- rc 0 or "same as baseline": `rpg_test --doll --gates`, `--anim --gates`, `--doll-arms --matrix arms`, `--doll-ladder`,
  `--doll-envelope`, `--doll-coverage --scope all --seeds 1..20 --gates`;
- `rpg_test --doll-wear --freeze-check tools/doll/freeze/p2c_freeze.txt` 0 changed after a re-sign whose diff holds only the
  changed clips' frames and brigandine looks (listed in moves/ART.txt); `rpg_test --rig=1 --doll-wear --freeze-check
  tools/doll/freeze/p2c_freeze_rig1.txt`, `art_hash --rig=1`, `city_gallery --rig=1 --village-art-check
  tests/fixtures/golden_village_art_rig1.txt` identical;
- `tools/gates/doll_gates.sh --bdir build-p3-art --out <scratch>/doll`: every line as baseline except moves/ART.txt's;
- `node tools/gates/witness_pack.mjs --bdir build-p3-art --out <scratch>/wp --keep tools/scripts/p3_art.txt` PASS, and the
  measured checks: axe vs mace strike head centres >= 4 px apart and >= 4 px from idle (every facing); the spear's U thrust
  tip above the helm within 2 columns of the body centre; with a sword and a bow equipped, `key k` shows the bow in hand
  during Release; the brigandine shows metal-ramp plates on the torso at 1x;
- the owner pack sheets (section 12) written.

### 8.5 HARNESS (engineer): creator under Witness, script keys, web saves, play goals

Files: `rpg/app/witness_driver.cpp`, `rpg/app/app_setup.cpp`, `rpg/app/app.h` (the runtime `wCreator` flag),
`rpg/app/script_runner.cpp`, `rpg/app/save_store.cpp`, `web/index.html`, `tools/witness_web/**`,
`tools/gates/witness_pack.mjs`, `tools/gates/witness_pack/**`, the nine creator scripts in `tools/scripts/` (only if a
harness gap needs a script line), `tools/scripts/p2c_playtests.txt`, `tools/scripts/p3_keys.txt` (new),
`rpg/view/scripts/harness_p3.cpp` (new: `expect face <n>`), `witness/p3_creator.witness.json` (new), `docs/PLAYTEST.md`.

Brief: D (section 5), B5, B3, B8's world:fit K cap (8), and C1's goals file.
- core: the nine creator scripts PASS headless; `key d` turns the hero; a browser without IndexedDB says saves are off;
- depth: the creator hook on both paths (reset param and launch flag), documented in PLAYTEST.md;
- delight: a tester's `call zoom close` shot is twice NORMAL's.

Done:
- build rc 0; `node tools/gates/witness_pack.mjs --bdir build-p3-harness --out <scratch>/wp creator inn m2_ui_screens
  m3_people_creator_phone p2c_flip_look p2c_flip_look_phone p2c_menus_creator p2c_menus_creator_phone sim_inn`: 9 PASS,
  0 SKIP; `tools/slot.sh node ../witness/bin/witness.js test witness/p3_creator.witness.json` passes;
- `witness_pack ... tools/scripts/p3_keys.txt` PASS (`p2cline face 0`, `key d`, `expect face 2`); m8_view_party.txt's
  PNGs identical to the record;
- web: `tools\gates\build_web.bat <dir>` OK, the web.yml export grep passes, `tools/witness_web/firstlaunch.mjs --no-idb`
  screenshot shows the title note, the default firstlaunch unchanged;
- world:fit: at `zoom close` on the phone window the play shot's K is >= 1.8x NORMAL's;
- the goals file parses; every `call` it names is a registered command.

### 8.6 GATES (engineer): the harness fixes and the baseline move mechanism

Files: `tools/gates/*.sh` (lib, step_gates, wave_gates, doll_gates, witness_smoke, perf_pack, size_report, check_layers,
audio_pack, view_pack, clang_ci, ci_cmds), `tools/gates/rebase_baseline.sh` (new), `tools/gates/size_renames.txt` (new),
`tools/gates/size_allow.txt`, `tools/gates/baseline/CURRENT` (new pointer), `tools/gates/baseline/README.md` (new),
`tools/tests/test_quest_mix.cpp`, `tools/tests/main.cpp`, `.github/workflows/web.yml`, `docs/SOLID_PLAN.md`, `README.md`,
`docs/ARCHITECTURE.md`. Not `tools/gates/witness_pack.mjs` (HARNESS) and not `tools/gates/baseline/moves/*` (each area its own).

Brief: C1 (the W7 line), C3, C4, C5, C6, and section 10's mechanism: every gate script reads its baseline root from
`tools/gates/baseline/CURRENT` (today `119de74`; web.yml's art_hash report too), and `rebase_baseline.sh` checks a run's
differences against the moves files and re-records only declared items into a new root.
- core: a red line means a real change; an alone rerun is alone or says it was not;
- depth: `rebase_baseline.sh --check-moves` names every undeclared difference and every declared move that did not happen;
- delight: `tools/gates/baseline/README.md` explains how a batch moves a baseline, in five lines.

Done:
- `grep -n -- --allow-calls tools/gates/wave_gates.sh` shows the W7 line;
- `bash tools/gates/lib.sh` self-test (`GATES_SELFTEST=alone`): with fake holders on slots 3-5 and the cap forced to 2,
  `slot_alone` waits for 3-5 (or prints `NOT ALONE (held 2/5 ...)` after its wait);
- `bash tools/gates/size_report.sh --check tools/gates/baseline/119de74/checks/size_report.txt --renames tools/gates/size_renames.txt`
  reports no rename as new; a negative test (a new 301-line function in a throwaway copy) is reported;
- `build/rpg_test --list-ci | diff - <(tools/gates/ci_cmds.sh)` empty, 69 lines, one `rpg_test --look-key`;
- loaded A/A: two `rpg_test --quest-mix --seeds 1..20 --gates --p1b --gates-p1b --report` runs beside three other heavy jobs
  print identical lines but the timings; the plan's form without `--ports` passes too;
- `tools/gates/rebase_baseline.sh --selftest` (a throwaway copy: a declared move is accepted and recorded, an undeclared
  one refused, the old root untouched, CURRENT flipped only on success);
- `tools/gates/step_gates.sh --bdir build-p3-gates --out <scratch>/step` on GATES' own branch: every line as baseline
  (GATES moves nothing but the CI count: CLANG_CMDS 69 in moves/GATES.txt).

### 8.7 AUDIO (engineer): the sea's sounds over the music

Files: `engine/sound_ids.h`, `engine/audio/sfx.cpp`, `tools/audio_preview.cpp` (the report line names the quietest sfx;
the check itself unchanged).

Brief: section 6. Change only the seven sfx; no new exemption.
- core: `audio_preview` exits 0; depth: the ReelClick ratchet still speeds up with the tension (k); delight: a 20 s
  sailing-with-Wild and a fishing mix in the owner pack.

Done:
- `tools/slot.sh build-p3-audio/audio_preview.exe <scratch>/wav` exit 0, "all checks passed", margins >= 6 dB and phone
  >= 6 dB, the quietest sfx named, every changed sfx peak <= 0.8 and 0 clipped;
- `tools/gates/audio_pack.sh --bdir build-p3-audio --check tools/gates/baseline/119de74/probes/audio_hashes.txt`: exactly the
  seven `full sfx_<name>.wav` lines and `full __rc__` differ (moves/AUDIO.txt);
- the owner pack WAVs (before/after of the seven, the two mixes).

---

## 9. Waves and integration

- **Wave 1 (starts at once):** LOOT's contract commit (items.h, gear.h, `lootp::verdict` delegating; no behaviour
  change; about an hour), then PARTY's contract commit (party.h, game_internal.h). ART, HARNESS, GATES, AUDIO and VIEW's
  B-items (B7, B8, B11, B12, P3) start at the same time: they need nothing from anyone.
- **Wave 2:** LOOT's rules, PARTY's rules (on LOOT's contract), VIEW's A-UI (on both contracts). No other barrier.
- **Integration order** (merges in `emb-solid-int` on `fix-p3-int`; disjoint files, so a conflict is a plan error):
  1. LOOT, 2. PARTY, 3. AUDIO, 4. ART, 5. HARNESS, 6. VIEW (its creator check needs HARNESS), 7. GATES (last, so its
  CI list and baseline tools see the finished tree). After each merge: the merged area's own Done lines on the merged
  head (fix rounds verify only their fixes). Then the one full regression (section 11).

## 10. Baselines: declaring and re-recording moves

The SOLID rule "no baseline is re-recorded" ends with this batch, but only for explained moves.
- **Moves files.** Each area writes `tools/gates/baseline/moves/<AREA>.txt`, one line per expected difference:
  `<gate> <item or glob> <id> <reason>` (e.g. `W2.view_all p2c_swings/p2c_swings_axe_* B2 axe strike redrawn`,
  `S10.audio full/sfx_ReelClick.wav E ratchet denser`, `S10.digest ser,len PARTY SAVE_VER 24`). An area measures its
  moves against the 119de74 record before it hands over.
- **Rebase.** `tools/gates/rebase_baseline.sh --check-moves --run <wave out>` compares the run with the current root and
  the union of the moves files: an undeclared difference or a declared move that did not happen fails. On a clean check,
  `--from 119de74 --to p3 --run <wave out>` copies the old root, re-records only the declared items from that same run,
  re-saves listed Witness refs and fixtures, writes `tools/gates/baseline/p3/REASONS.md` (each item, its id, its reason,
  its area), and flips `tools/gates/baseline/CURRENT` to `p3`. The 119de74 root stays as it is.

**Expected moves** (each area confirms or narrows its own rows; anything else moving is a bug):

| gate | expected | why (area) |
|---|---|---|
| S1.build | 36 exes; warnings <= 366, no new kind | - |
| S2.seeds | identical, or the seed lines whose drops or shelves change | LOOT / PARTY declare the lines |
| S3.save | the SAVE_VER line; new stable byte counts; the two masks dropped | PARTY (SAVE_VER 24, C2) |
| S3.savever | 24 | PARTY |
| S3.fixtures | `save_v24.bin` added; `golden_village_art.txt` if brigandine villagers change | PARTY / ART |
| S4.clang, W1.msvc_ci | 69 commands, 0 fails | GATES (C5) |
| S5.rig2 | the categories holding brigandine looks and the changed clips' frames | ART |
| S5.rig1, S6.freeze1, S7.rig1 | identical | rig 1 untouched |
| S6.freeze | 0 changed against ART's re-signed manifest (the re-sign diff is reviewed) | ART |
| S6.pool | bytes and hash, if views were ADDed | ART |
| S7.rig2 | the groups with brigandine wearers | ART |
| S8 | builds, exports unchanged | HARNESS (index.html) |
| S9.test / S9.shot | pass; `village_start.png` only if a brigandine guard or HUD change shows at t = 2 s | ART / VIEW |
| S10.digest, W6.digest_clang | `ser`/`len`/S lines only; act, rng, marks identical | PARTY |
| S10.view | listed shots (zoom close, swings, shops, pickups) | VIEW / ART / LOOT / HARNESS |
| S10.audio | 7 sfx WAVs + `full __rc__` 2 -> 0 | AUDIO |
| S10.layers | PASS, no new violation (the allowlist may only shrink) | all |
| W2.scripts | 0 FAIL; the nine creator scripts PASS (were SKIP); new `p3_*.txt` scripts | HARNESS and the script owners |
| W2.view_all | listed scripts' PNGs; the nine creator scripts' PNGs (new); new scripts' PNGs | all |
| W3.witness | 15 -> 18 files (p3_party_return, p3_creator, p3_view), all pass | PARTY / HARNESS / VIEW |
| W4.doll | the lines touching the four families' strike frames and brigandine looks | ART |
| W5.firstlaunch | default screenshot unchanged | HARNESS |
| W6.perf | within the record's noise + 3 % | - |
| W7.play | runs with calls; read the report | GATES / HARNESS |

## 11. The full regression (once, at integration)

The owner's rule: one all-systems regression per update; fix rounds rerun only their own lines.

```sh
cd /c/Users/joshu/Documents/emb-solid-int        # on fix-p3-int with all seven areas merged
S=/c/Users/joshu/AppData/Local/Temp/claude/C--Users-joshu/1979ea42-238a-4969-bda8-d76ab6bf2de5/scratchpad/p3
export WITNESS_MAX_PROCS=5 WITNESS_MAX_BROWSERS=1   # keep-awake loop running (PC sleep trap)
# 0. a fresh clone and build dir (plan 4.3), SDL by junction (rule 3), no network fetch
git clone -q --no-hardlinks . $S/p3clone && git -C $S/p3clone -c advice.detachedHead=false checkout -q "$(git rev-parse HEAD)"
mkdir -p $S/p3clone/build/_deps
cmd //c mklink /J "$(cygpath -w $S/p3clone/build/_deps/sdl3-src)" "C:\Users\joshu\Documents\embervale\build\_deps\sdl3-src"
# 1. ONE wave run against the current root (119de74), W7 included
( cd $S/p3clone && tools/gates/wave_gates.sh --bdir build-p3wave --out $S/p3wave --w7 )
# 2. every difference must be a declared move, and every declared move must have happened
tools/gates/rebase_baseline.sh --check-moves --run $S/p3wave
# 3. re-record the declared items from that same run into tools/gates/baseline/p3, flip CURRENT; commit with REASONS.md
tools/gates/rebase_baseline.sh --from 119de74 --to p3 --run $S/p3wave
# 4. confirm the rebase on the moved lines only, with the clone's build (no second full run)
tools/gates/step_gates.sh --bdir $S/p3clone/build-p3wave --out $S/p3recheck --only S2,S3,S5,S6,S7,S10
node tools/gates/witness_pack.mjs --bdir $S/p3clone/build-p3wave --out $S/p3recheck/view --all \
     --check tools/gates/baseline/p3/probes/view_all/hashes.txt
# 5. clean up: never rm -rf through the junction
cmd //c rmdir "$(cygpath -w $S/p3clone/build/_deps/sdl3-src)"
test -f /c/Users/joshu/Documents/embervale/build/_deps/sdl3-src/CMakeLists.txt && rm -rf $S/p3clone
```

Plus the A lines the judge runs on the integrated build (several are already in the CI list):
`rpg_test --ladder-pace --worlds 1..20 --report --gates`, `--companion --seeds 1..20`, `--loot-sources --seeds 1..3`,
`--loot`, `--power-curve`, `--metrics`, `--foes --seeds 1..3`, `--arms --seeds 1..3`, `--wards --seeds 1..10`,
`--banter --seeds 1..3`, `--save-soak 100`, `save_test`, and `witness test witness/p3_party_return.witness.json`.
Time: the wave gates took 61-67 min of wall time at b3463ce with 5 slots (3,657 s and 4,026 s); W7 adds up to about
4 h (12 testers, 2 at a time, 44 wall minutes each); everything is about 2.5x slower while the owner plays (cap 2).

## 12. The owner pack

Before/after images (and WAVs) under `...\scratchpad\p3_owner_pack\<item>\`, each with a one-line caption, made from
hidden Witness runs at the same seed and spot before (b3463ce build) and after:
- **art, pending the owner's review:** B1 (live sword strike, 5 frames; frozen R/D/L/U), B2 (axe and mace strikes, every
  facing, beside idle), B6 (spear thrust facing U, and all facings), B10 (bronze brigandine on the man, the woman, a guard
  and a bandit; T1-T2 beside leather), B4 (a shot with a sword equipped: draw, release);
- **visible changes:** B7 (creator before/after), B8 (CLOSE at 1080 px, the phone and 960 x 540), B11 (pickup toast),
  B3 (the saves-off title note), B9 (the start town's smith shelf), A (2.5's companion and card shots);
- **audio:** E's seven sfx before/after and the two mixes.

## 13. Decisions for the owner (the plan proceeds on the first reading of each)

1. **SAVE_VER 24 with SAVE_VER_MIN 23** (the batch rule) instead of the design's "23 holds": v23 saves load either way.
2. **B12's Witness side:** the autofight rule lives in ../witness (a separate repo). This batch only adds `boss`/`rank` to
   Embervale's snapshot; the autopilot change (never auto-fight a boss during goto/explore) needs the owner's OK to edit
   Witness.
3. **B8 CLOSE = twice NORMAL** (down to 100 world rows), a visible zoom change on every screen.
4. **E: louder sea sounds, no new exemption.** If the owner prefers the sail, oar and reel as soft beds, the fallback is
   exempting them with reasons (a looser check), not chosen here.
5. **B9:** a smith sells a light and a metal body armour of his band; plate stays a band-3 metal.
6. **The Q3 marks** (the `epic` bit and the finish marks, D86) are not done and not in this batch.
7. `--ladder-pace --gates` is a judge line this batch; adding it to the CI list is a follow-up (it would couple LOOT and
   GATES at merge time).
8. VISION 15.37's item 3 (where the rules update sits beside P2d) is taken as answered by "go ahead": it ships now.

## 14. What the judge checks

From the state a player starts in, look and feel first, measured (eyes for shots, numbers for the rest):
- A: a new game and save_v23: a better gift taken, a worse one declined in voice, TAKE BACK greyed on the kit, dismissal
  handing every gift back (the live bug), a death pile of everything on the ground for 240 s, the same friend back at home
  with bond, skills and kit (`expect party same`), a sellsword rehired after his week; a relic in a band-4 ruin; no shop
  masterwork outside the exemptions; `--ladder-pace --gates` green.
- B: each item's own repro (section 3) now reads right, in the owner pack's before/after; nothing else in the view pack
  moved without a moves line.
- C/D/E: the gate fixes' Done lines; the nine creator scripts PASS; `audio_preview` exits 0.
- The regression: section 11 steps 1-4 clean; `REASONS.md` lists every moved item with its reason.
