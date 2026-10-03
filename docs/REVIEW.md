# EMBERVALE review: playtest plus code review (2026-10-03)

Scope: one playtester agent playing as a player (screenshots, test flags, the headless bot), plus a lead
review of all of `rpg/sim`, `rpg/view`, `rpg/main.cpp` and the test tool. The bugs and the low-risk,
high-value polish were fixed in this pass. The rest is ranked in section 4. The next-step plan is
`docs/PLAN.md`.

**State after this pass:** `rpg_test` prints **ALL OK on 36 seeds** (1 to 30, 42, 99, 123, 2024, 4242 and
123456). A 600 s mortal bot run on seed 7 is also OK. The test now covers five new things: walking
reachability of every site, the main quest end to end, the dungeon-exit rule, death and respawn, and shop
restock. `C:\Users\joshu\Desktop\Embervale.exe` was rebuilt from this tree.

**Save compatibility:** except where it was broken, world generation is unchanged for every seed. Building
counts, spawns and site indices match the pre-review binary on all the seeds compared. The owner's existing
save therefore still loads into the same world. There is one deliberate exception. If a capital had no Keep
(seed 5's Frosthelm, for example), a city that has one becomes the capital, because the old world couldn't
start the main quest. Mountain-pass trails (R19) only change ground and props, not buildings or sites.

---

## 1. Playtester findings (summary)

The playtester covered the title screen, villages, towns and cities, NPC dialogue, the innkeeper's quest and
shop, fights against several enemy types (with and without god mode), caves and ruins, night, every menu,
death, `rpg_test` on 7 seeds and `--perf`. To reach the dialogue screens it added a test-only
`--talk 0|1|2` flag to `main.cpp`, and that flag was kept.

**Bugs reported** (status in section 3):
1. **High:** getting hit near a dungeon's entry ladder throws you back outside. You arrive on the ladder, so
   knockback pushes you onto it (`--seed 2 --goto ruin --enter --fight skeleton`). Fixed (R18).
2. The off-screen quest marker draws over HUD text: the quest title, the stamina bar and the minimap. Fixed
   (R22).
3. Fast travel to a bandit camp drops you on top of the Bandit Chief. Fixed (R21).
4. **IP risk:** Elder Scrolls content. Divine names on shrines and blessings, "arrow in the knee",
   "sweetroll", "lollygagging". The lead review also found Daedric, Ebony, Elven and Glass tiers, the
   Frostbite Spider, soul gems, the Flames / Healing / Ice Spike spell names, and town names that can
   generate as Whiterun, Riverwood or Windhelm. Fixed (R20).
5. A statue overlapping a market stall or a house, and a roof swallowing a stall. Fixed (R24).
6. The shop footer is drawn over the panel border. Fixed.
7. "Frozen …" dungeons in the desert and in temperate forest. Fixed (R25).
8. Quest-givers refer to themselves by name ("…FRIRIKWYN WILL PAY YOU"). Fixed (R26).
9. `--perf` prints "parts" with no number. Fixed.
10. Test gap: the bot never dies, so death and respawn are untested. A test has been added.

**Confusing moments:** the marker was a bare number plus a blob (redesigned as a chevron). The HUD text had
no backing (a backing panel was added). Caves are very dark and wall tops look like void (open). The dungeon
minimap used only half its box (fixed). The player disappears under crowds (open). Villagers ignore monsters
in the village (they now flee). The seed 7 start village looked walled in by mountains. **That was true:
see R19, the biggest find of the session.**

**Good:** the art (autumn trees, lit windows, lamp light, rain, inn interiors), clean dialogue and shop
panels, a clear boss bar, and very fast performance: 860 to 1160 fps uncapped, worst sim step 1.3 ms, world
generation about 200 ms.

**Bad:** every innkeeper said the same line (now varied). Only bounty and hunt quests turned up. Innkeepers
sell only food. The lair is empty before the dragon quest. Mountains read as flat grey. Town centres all
looked alike (centrepieces now vary). Roads run in right-angle lines.

**Balance:** the bot never died and never dropped below 35 % HP in 600 s: 153 kills and level 8 on seed 2.
Quest rewards (93 to 192 gold) dwarf shop prices. Levelling slows to about 20 kills per level by level 8.
Standing still against three trolls or three draugr felt fair.

**Variety:** villages feel organic. Towns and cities share the same plaza, fountain and stalls core. Every
world is a round island with snow in the north and desert in the south, and on some seeds about 40 % of it
is one grey mountain mass. Names repeat across seeds (Ashwood twice).

**Their top 5 for fun:** fix the ladder ejection; more quest variety; varied town centres and curving
roads; tougher, scaled enemies, villagers and guards that react, and shops worth spending gold in; and
readable dungeon walls and real-looking mountains.

---

## 2. Lead review findings

How serious each one is: **S1** breaks progress or the core loop, **S2** a clear bug or exploit a player
will hit, **S3** polish, readability or performance.

| # | Sev | Finding |
|---|---|---|
| R1 | S1 | **The main quest could never be finished.** Ashfang was spawned with `site = -1`, and `kill()` only clears overworld boss sites with `site >= 0`, so killing the dragon never reached stage 4 |
| R2 | S1 | **Main quest soft-lock:** clearing an Ember Shard ruin *before* talking to the Jarl gave no shard, so 3/3 became impossible. Exploring first is exactly what an open-world player does |
| R3 | S1 | **The capital might have no Keep** (the "required" placement can fail twice), which leaves no Jarl and no way to start the main quest. **It happens on seed 5** |
| R19 | S1 | **Unreachable world:** mountains or forests can wall the start village off. On seed 7 the capital, every shard ruin, the lair and 15 settlements couldn't be reached on foot, and 1 to 37 optional dungeons couldn't be reached on 10 of 20 seeds. A full soft-lock, because fast travel only goes to places you've discovered |
| R4 | S2 | **Projectiles were invisible:** never queued for drawing. That covered arrows, fireballs, ice, spit, wraith bolts and dragon fire |
| R5 | S2 | **Presses lost on 120/144 Hz displays** (and on iPhone ProMotion): one-shot inputs were cleared on frames where the sim didn't step |
| R6 | S2 | **Shops refilled when reopened** (an infinite best-item exploit), and sold items vanished |
| R7 | S2 | **Burning couldn't kill the player** (HP below zero, still walking). It also ignored god mode |
| R8 | S2 | **Quitting on the death screen saved a dead player**, who loaded with 1 HP where they fell and skipped the penalty |
| R9 | S2 | **The death screen was skipped by mashing attack** (Space or a tap respawned instantly) |
| R10 | S2 | **No keyboard fast travel** on the map tab (mouse only) |
| R18 | S2 | **Dungeon exit triggered by knockback** (the playtester's #1) |
| R11 | S3 | **Ashfang's HP scaled to about 4,100** (with armour): a slog of about 300 hits |
| R12 | S3 | **No prompt for chests, shrines, beds, bushes or signposts**. On touch, tapping them attacked |
| R13 to R17 | S3 | The HUD column was unreadable. The Hero tab ignored enchantments. The terrain worker leaked finished bakes and pending keys (a hitch on return). Small talk was one pool of 7 lines. Respawn kept knockback |

Open observations (ranked in section 4): early combat is nearly risk-free; NPCs have no schedules; wild
monsters spawn from nothing; every biome uses the same house kit; there are 4 radiant quest types with fixed
sentences; roads are 4-connected A* L-shapes. Settlement generation itself (winding streets, setbacks,
gardens, greens, fields, curved walls) reads as organic in screenshots.

---

## 3. What was fixed (file:line)

Line numbers are from this pass. `main.cpp`, `view.h`, `render.cpp` and `terrain.cpp` are being edited at
the same time for the web port, so they may drift.

| Fix | Where |
|---|---|
| R1: the dragon belongs to its lair, so its death clears the lair and the main quest reaches DONE | `rpg/sim/game.cpp:1065` |
| R11: Ashfang's HP is `700 + 60 × level` (1420 at level 12) | `rpg/sim/game.cpp:932` |
| R7: burning can kill, and respects god mode | `rpg/sim/game.cpp:306-310` |
| R18: leaving a dungeon or interior takes intent: moving down, not hurt, no knockback | `rpg/sim/game.cpp:423` |
| Villagers flee monsters within 90 px (outdoors, not guards) | `rpg/sim/game.cpp:737` |
| R2: stage 1 credits shard ruins that were already cleared, and chains to stage 2 at 3/3 | `rpg/sim/game_rpg.cpp:479-500` |
| R6: per-merchant stock cache per 2-day restock. Sold items stack back onto the shelf | `rpg/sim/game_rpg.cpp:758, 792`, `rpg/sim/game.h:213` |
| R8: a save made on the death screen respawns on load | `rpg/sim/game_rpg.cpp:945` |
| R17: respawn clears knockback and velocity, with 1.5 s of grace i-frames | `rpg/sim/game_rpg.cpp:832` |
| R21: fast travel arrives 4 tiles south of a bandit camp, and outside the lair | `rpg/sim/game_rpg.cpp:813` |
| R26: quest offers are spoken in the first person ("RETURN TO ME"). The log keeps the third person | `rpg/sim/game_rpg.cpp:645` |
| R16: world-aware small talk (nearest dungeon and its monsters, biome, night, the Jarl's call, fame). Innkeeper, merchant and smith lines vary per NPC and day | `rpg/sim/game_rpg.cpp:249-310` |
| R12: `Game::interactProp()` shared by the logic and the UI. Prompts "E: OPEN / PRAY / PICK / READ / SLEEP". Touch uses the prop when no enemy is within 70 px | `rpg/sim/game_rpg.cpp:167`, `rpg/view/hud.cpp:70` |
| R20: original pantheon (Solmir, Veyna, Haldrun, Orissa, Kevran, Ilmath, Brannock, Eskara); a shrine blesses in its own god's name. New guard lines. Tiers are Iron, Steel, Gilded, Jade, Obsidian and Emberforged. Rime Spider, Spirit Stone, and the spells Firebolt, Mend and Frost Lance. Whiterun, Riverwood and Windhelm are banned as town names | `game.cpp`, `game_rpg.cpp:201`, `items.cpp`, `world.cpp:97, 242` |
| R3: if the capital has no Keep, the nearest city with one takes over | `rpg/sim/world.cpp:1233` |
| R19: `connectAll()` floods from the start village and cuts mountain-pass trails (A* with rock allowed) from each stranded site to the nearest walkable road. Inside a settlement's margin, `road()` now clears rock and solid props on pass trails | `rpg/sim/world.cpp:1135, 968` |
| R25: "FROZEN" dungeon names only in Snow or Taiga | `rpg/sim/world.cpp:94` |
| R24: stalls under roofs and statues jammed against props or buildings are removed *after* all RNG draws (so layouts and saves don't change). Town and city centrepieces vary (fountain, statue, well, old tree) through a hash, not the RNG | `rpg/sim/world.cpp:590, 760` |
| R4: projectiles in the y-sorted draw list | `rpg/view/render.cpp:360` |
| R15: drop stale finished bakes, and clear `pending_` together with dropped jobs | `rpg/view/terrain.cpp:379-395` |
| R9: the death screen waits 1.2 s before the prompt fades in and input is accepted | `rpg/view/hud.cpp:782, 918, 1043`, `view.h` (`modeT_`), `render.cpp:185` |
| R10: map tab: W/S cycles the discovered places nearest first, Enter travels | `rpg/view/hud.cpp:841` |
| R13: translucent backing behind the location, clock and quest column | `rpg/view/hud.cpp:211` |
| R22: the quest marker is a bold outlined chevron, slid out of the HUD corners, with the distance behind it | `rpg/view/hud.cpp:253-270` |
| The dungeon minimap is clamped to the map, so it fills the box. The player dot moves with you | `rpg/view/hud.cpp:418` |
| R14: Hero tab maxima include enchantments | `rpg/view/hud.cpp:642` |
| The shop detail line and footer were moved off the panel border | `rpg/view/hud.cpp` drawShop |
| R5: one-shot inputs are latched until a sim step consumes them. The `--perf` label was fixed | `rpg/main.cpp:226` |
| Tests (all new): the capital has a Keep. **Every settlement and story site reachable on foot** (FAIL), with optional sites reported. **The main quest end to end** (pre-cleared shards, then the Jarl reaching stage 2, then knockback onto the exit *not* leaving and walking onto it leaving, then the dragon spawning and its death completing the quest). **Death by burning**, then a save on the death screen, then respawn on load. Respawn state. **Shop restock** | `tools/rpg_sim.cpp:123, 149, 289, 295-360, 364` |

Verified by screenshots: a village fight; night lighting; the city square with the HUD backing; the
playtester's ruin repro (still inside after 6 s with skeletons on the ladder); a seed 2 village fight
(chevron marker, villagers clear of wolves); the town square at night from the final binary; and the seed 7
map with the new passes. All screenshots and the temporary build dirs were deleted.

---

## 4. Further changes, prioritized

Effort: **S** is under half a day, **M** is 1 to 2 days, **L** is 3 or more days of agent work.

### Must
| Change | Why | Effort |
|---|---|---|
| **Save versioning and migration** (`SAVE_VER` gates, a checked-in v1 blob test), plus a **world-gen version** stored in the save | Any generator change that touches the RNG reshuffles buildings and breaks the owner's saves. This pass avoided that by hand, which won't scale | S |
| **A scripted-input mode** (`--script file`, with `--talk` folded in) | Real play (walk, fight, doors, menus) can't be regression-tested today | S |
| **Early-game difficulty pass** (pack size by zone, damage per level, out-of-combat-only regen, XP curve, potion prices against quest gold) | Two independent runs: zero deaths and zero potions in 10 minutes. No tension means loot and levels mean little | M |
| **CI** running rpg_test over 20 or more seeds on every push | R1, R2, R3 and R19 were all seed- or order-dependent soft-locks. Range runs catch the next ones | S |
| **iOS/TestFlight pipeline** copying The Fort (`docs/PLAN.md` § iOS path) | The platform target. Device-only problems need finding early | M |

### Should
| Change | Why | Effort |
|---|---|---|
| **Settlement archetypes and regional building palettes** (fishing, mining, farming, river crossing, hill fort, market town; snow, desert, swamp and autumn kits) and **varied town centres** (market street, off-centre square, green) | The biggest remaining visual repetition. The playtester found three seeds' city centres looked alike | L |
| **Curving roads**: 8-connected A* plus smoothing (with 4-connected filling so they stay walkable) | Roads are ruler L-shapes, the opposite of the owner's "organic" direction. Adding noise to costs was tried in this pass and doesn't help on a 4-connected grid | M |
| **Readable dungeons and mountains**: lit wall faces and a top-rim highlight, torches along corridors, smooth light falloff, peaks with snow caps and ridgelines instead of flat grey | Both the playtester and the screenshots show caves hiding their shape and mountains reading unfinished | M |
| **NPC day/night schedules**, and guards fighting monsters in town | Towns feel the same at noon and midnight | M |
| **Wilderness vignettes and dens** | The wilderness between sites has few places, and monsters appear from nothing | M |
| **Enemy behaviour variety** (flanking, fleeing, a telegraphed heavy attack you roll through) | Every melee enemy walks in and swings | M |
| **Quest variety and text grammar**, plus town quest chains | The playtester saw only bounty and hunt. Fixed sentences repeat | M |
| **Player silhouette when occluded** (outline drawn over crowds and roofs) | The player vanishes under wolves and draugr | S |
| **Continent shape variety** (archipelagos, peninsulas, an inland sea, climate not always north-cold and south-hot) and a cap on mountain mass | Every world is a round island and some are 40 % grey rock | M |
| **Dynamic logical width on phones** (about 585×270) | Fewer black bars on iPhones | M |

### Could
| Change | Why | Effort |
|---|---|---|
| Weather fronts and seasons | Weather flips per 3-hour hash | M |
| Ambient wildlife (birds, deer, fish) | Life in the wilderness | M |
| Bigger, biome-flavoured name pools | Ashwood came up twice. Names share one world-wide pool | S |
| Dungeon room templates, keys and levers, cave variants | One generator each | L |
| A sleeping Ashfang silhouette at the lair before the quest, and a foreshadowing fly-over | The playtester found the lair empty and flat | S |
| Innkeepers selling a potion or two. Crafting uses for pelts, ore and silk | Gives gold and loot more purpose | M |
| Companions from inn chains | Engagement | L |
