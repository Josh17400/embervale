// EMBERVALE simulation: actors, combat, AI, inventory, quests, dialogue, time of day, saving. No SDL.
#pragma once
#include <array>
#ifndef __EMSCRIPTEN__
#include <future>
#endif
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "rpg/core/art_ids.h"
#include "rpg/art/art_life.h"   // art::Bubble (Actor::bubble)
#include "rpg/sim/appearance.h"
#include "rpg/sim/backgrounds.h"
#include "rpg/core/common.h"
#include "rpg/sim/craft.h"
#include "rpg/sim/explored.h"
#include "rpg/sim/factions.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/home.h"
#include "rpg/sim/items.h"
#include "rpg/sim/jobs.h"
#include "rpg/sim/life.h"
#include "rpg/sim/lordship.h"
#include "rpg/sim/magic.h"
#include "rpg/sim/party.h"
#include "rpg/sim/realm.h"
#include "rpg/sim/rise.h"
#include "rpg/sim/sea.h"
#include "rpg/sim/service.h"
#include "rpg/sim/skills.h"
#include "rpg/sim/talk.h"
#include "rpg/sim/war.h"
#include "rpg/sim/world.h"
#include "rpg/story/story.h"
#include "rpg/sim/game_types.h"

namespace art { struct GraveLook; struct HoundLook; }   // rpg/art/art_party.h (Game::partyGraveLook / partyHoundLook)

class Game {
 public:
  explicit Game(uint64_t seed = 1);
  void newEndlessGame(uint64_t seed);   // a new game on the endless mainland (VISION_PLAN 15.6; M2: the only kind)
  void update(float dt, const Input& in);

  // --- world & level
  World world;
  Map sub;                 // current cave/ruin/interior (when inside)
  bool inside = false;
  int subSite = -1, subBldg = -1;
  int subFloor = 0;        // M0b: which floor of building subBldg (0 ground; SAVE_VER 4)
  Map& map() { return inside ? sub : world.over; }
  const Map& map() const { return inside ? sub : world.over; }
  // identity of the current map (looted chests, killed spawns, terrain caches). Upper floors get their own keys.
  int mapKey() const {
    if (!inside) return 0;
    if (subBldg >= 0) return subFloor > 0 ? 10000000 + subBldg * 16 + subFloor : 100000 + subBldg;
    return 1 + subSite;
  }

  // --- state
  Mode mode = Mode::Title;
  uint64_t seed = 1;
  float time = 0;          // sim seconds since start
  float hour = 8.0f;       // 0..24 time of day
  int day = 1;
  std::vector<Actor> actors;   // actors[0] is always the player
  std::vector<Projectile> projs;
  std::vector<Pickup> pickups;
  std::vector<Event> events;
  Actor& pl() { return actors[0]; }
  const Actor& pl() const { return actors[0]; }

  // player progression
  int plLevel = 1, plXp = 0, gold = 25, perkPts = 0;
  float mp = 60, maxMp = 60, stamina = 80, maxSt = 80;
  float baseHp = 100;
  std::vector<Item> inv;
  int eqWeapon = -1, eqBow = -1, eqStaff = -1, eqArmor = -1, eqHelmet = -1, eqShield = -1, eqRing = -1, eqAmulet = -1;
  int eqGloves = -1, eqBoots = -1, eqCloak = -1;   // SAVE_VER 3
  int* equipSlot(ItemKind k);                       // the eq* index for an equippable kind (nullptr otherwise)
  // everything worn (not wielded): enchantment bonuses come from these
  std::array<int, 8> worn() const { return {eqArmor, eqHelmet, eqShield, eqRing, eqAmulet, eqGloves, eqBoots, eqCloak}; }
  // character (SAVE_VER 3): chosen in the creator; pre-M0 saves load with the defaults (created == false)
  Appearance app;
  Background background = Background::None;
  uint32_t storyFlags = 0;                          // SF_* bits
  bool hasFlag(uint32_t f) const { return (storyFlags & f) != 0; }
  uint8_t spellsKnown = 1;     // bit per Spell
  Spell spell = Spell::Flames;
  int kills = 0, dungeonsCleared = 0;
  float blessT = 0;            // shrine blessing time left
  std::string blessName;
  int lastTown = -1;           // respawn point
  Lodging lodging;             // M0b: the rented inn room (SAVE_VER 4)
  bool lodgingActive() const { return lodging.bldg >= 0 && (day < lodging.untilDay || (day == lodging.untilDay && hour < 12.0f)); }
  float sleepFade = 0;         // view: fade-out when resting/travelling
  float hitStop = 0;           // brief freeze on heavy hits (game feel)
  float slowMo = 0;            // perfect-roll / level-up slow motion left (real seconds; sim runs at 30 %)
  float stFlash = 0;           // view: flash the stamina bar (tried to act with too little stamina)
  float lastHurtT = -99;       // time the player last took damage (out-of-combat regen)

  // quests
  std::vector<Quest> quests;
  int nextQuestId = 1;
  int trackedQuest = -1;
  int errandPrevTracked_ = -1;   // (P1 fixer) the quest tracked before a street errand took the pin (restored when it ends; not saved)
  std::map<uint64_t, int> npcQuestsDone;     // npc key -> completed count (new offers)
  // P1 Fresh Roads (SAVE_VER 23, rpg/sim/jobs.h; JOBS lane): the settlements' weekly job pools and the journal's leads
  jobs::Book jobBook;
  std::set<uint64_t> looted;                 // mapKey<<32 | tile, or lootKey's global key (endless overworld)
  ExploredMask explored;                     // M1: fog of war for the world map (global tiles the player has seen)
  std::map<int, std::set<int>> killedSlots;  // mapKey -> spawn slots defeated (no respawn)
  // M2 (SAVE_VER 6): per-world facts keyed by stable ids, for the wayside places and the quests the sim lane adds (the
  // day a vignette's standing stones were last used, a toll paid, a grave read, a rumour bought...). The sim lane
  // defines its key spaces next to the code (e.g. a site Gid xor a small tag).
  std::map<uint64_t, int32_t> marks;

  // UI-facing state
  Dialogue dlg;
  ShopState shop;
  std::string locName;     // current region/site label
  int curSite = -1;
  float noticeT = 0;
  std::string notice;

  // --- actions called by the UI
  void startPlay() { mode = Mode::Play; }
  void beginCreator() { mode = Mode::Creator; }   // after newEndlessGame(): the UI edits app/background, then...
  void finishCreator();                            // ...applies the background's trait, rebuilds the look, starts play
  bool rewardWaiting(const Actor& npc) const;      // a completed quest's reward waits with this NPC (world "!" marker)
  bool offersWork(const Actor& npc) const { return hasOffer(npc); }   // M2: this NPC has a job to offer (scripts, tests)
  // ---- P1 Fresh Roads (VISION_PLAN 15.34). Leads (jobs.h; JOBS lane, rpg/sim/jobs.cpp): a journal line pointing at a
  //      quest not offered yet. Any lane may add one: the LIFE lane's overheard gossip and street incidents, the JOBS
  //      lane's rumours, the STORY lane's tellers. id / day / state are filled in; a lead naming the same person and
  //      quest type (or with the same text) as an open one is not added twice (its id is returned). 0: not added.
  uint32_t addLead(const jobs::Lead& lead, bool announce = true);   // (P1 fixer r2) announce: the NEW LEAD notice
  //      Errands (QType::Errand; LIFE lane): a street incident's quest, taken on the spot. openErrand accepts `q` (type
  //      Errand, stage the jobs::ErrandKind, giver the victim, gold / xp, hasPos / tgx / tgy, subject the journal line)
  //      and returns its id; finishErrand ends it at once: success pays (completeQuest: gold, xp, the giver's thanks
  //      counted), failure marks it Done with QF_FAILED. game_rpg.cpp.
  int openErrand(const Quest& q);
  void finishErrand(int qid, bool success);
  std::string questStatus(const Quest& q) const;   // one journal line: what to do next ("" = nothing extra)
  void dialogueChoose(int optIndex);
  void closeDialogue();
  bool buy(int stockIndex);
  bool sell(int invIndex);
  void useItem(int invIndex);          // equip / consume
  void dropItem(int invIndex);
  void chooseLevelUp(int stat);        // 0 health 1 magicka 2 stamina
  bool fastTravel(int site);           // immediate (scripts, tests); the UI uses beginTravel
  // M2 travel behind the fade (Travel above). beginTravel: false (with a notice) when the journey cannot be made.
  bool beginTravel(int site, bool carriage = false);
  // (M3c carry) a teleport behind the fade: the same journey to a GLOBAL tile (no hours, no fare, no site rules)
  bool beginTravelTo(int32_t gx, int32_t gy);
  void finishTravel();                 // the view: the arrival's terrain is baked, fade back in
  bool travelling() const { return travel.phase != TravelPhase::None; }
  TravelQuote travelQuote(int site, bool carriage = false) const;
  Travel travel;
  Travel lastTravel;                   // M2: the last journey that arrived (its costs: scripts, tests, --perf reports)
  // ---- M4 Banners: the living world (rpg/sim/realm.h, REALM lane), the war's actors and overlays (rpg/sim/war.h, WARDS
  //      lane), the story engine (rpg/story/story.h, STORY lane). realm and story are saved (their own blocks); war is
  //      regenerated from the realm (never saved).
  realm::Realm realm;
  story::Engine story;
  WarState war;
  // ---- M5 Hearth and Hall (rpg/sim/life.h): the census, needs, plans, moods and the player's buffs (CITIZENS lane;
  //      saved: the life block), and the townsfolk's actors (rpg/sim/life_game.cpp, TOWNSFOLK lane)
  life::Life life;
  // ---- M6 Steel: the player's smithing skill, learned culture secrets and apprenticeship trust (rpg/sim/craft.h; NUMBERS
  //      lane; saved: the craft block, SAVE_VER 12)
  craft::Knowledge craft;
  craft::Bench bench;          // the forge screen's state (Mode::Forge; UI-facing, never saved)
  // ---- M7 Home: the player's houses, lots, yards, crops, animals, stores, discovered styles and the horse ridden
  //      (rpg/sim/home.h; HOMESTEAD lane; saved: the home block, SAVE_VER 14). home.ui is the Mode::Build screen's state.
  home::Homes home;
  // ---- M7b Voice (rpg/sim/talk.h; TALK lane): who the player has met, who has introduced themselves, the lines said
  //      lately (saved: the talk block, SAVE_VER 15). talkTo touches the record of every person spoken to.
  speech::Memory talkMem;
  // ---- M8 Fellowship (rpg/sim/party.h; FELLOWS lane): the companions, the fallen and their graves, the deeds bus's
  //      queue (saved: the party block, SAVE_VER 16)
  party::Party party;
  // ---- M9 Arcana & Mastery (VISION_PLAN 15.29, 15.30). skills: every skill's level, XP and perks, familiarity with
  //      materials and cultures, books read, perk points (rpg/sim/skills.h; SKILLS lane; saved: the skills block,
  //      SAVE_VER 20). arcana: the spellbook, the quick slots, mastery and attunement (saved: the magic block) and the
  //      casting / surfaces / concentration runtime (never saved) (rpg/sim/magic.h; MAGIC lane).
  skills::Skills skills;
  magic::Arcana arcana;
  // ---- M10 Beyond the Mist (VISION_PLAN 2.12, 15.4, 15.31; rpg/sim/sea.h, SEA lane): the boats owned and the one
  //      aboard, the ports known, the lands and their sea-chart chains, the catches (saved: the sea block, SAVE_VER 21)
  //      and the wind / storm / Mist / fishing / voyage runtime (sea.live, never saved)
  sea::Sea sea;
  // ---- M12 Rise and Fall (VISION_PLAN 13 M12, 4.7, 15.6.4, 15.33). service: the army the player serves, the rank,
  //      merit, orders and diplomacy missions (rpg/sim/service.h, SERVICE lane; saved: the service block, SAVE_VER 22).
  //      lordship: the villages the player holds, the hall's coffers, tribute and the player's own realm
  //      (rpg/sim/lordship.h, LORD lane; saved: the lordship block). Realm::renown() mirrors rank / sworn / lordOf.
  service::Service service;
  lord::Lordship lordship;
  //   the one doorway for skill gains: `base` use points of an act of that skill (rpg/sim/skills_game.cpp SkillOps::use)
  void skillUse(skills::Skill s, float base);
  // (M7b Voice) how the dialogue panel titles a speaker: "SUSUT, INNKEEPER" once they have introduced themselves,
  // "THE INNKEEPER" before; a guard stays "GUARD", a grave its own words
  std::string speakerTitle(const Actor& a) const;
  std::string speakerTitle() const;    // the open dialogue's speaker
  // the conversation recorder (tests, the dialogue review; never saved): when set, talkTo and dialogueChoose record
  // every conversation turn by turn (speech::Recorder::begin / reply)
  speech::Recorder* talkRec = nullptr;
  // apply the realm to the loaded world now (owners and banners of every loaded settlement, the news): scripts and tests
  // call it after forcing a realm change; realmStep calls it whenever the realm changed (realm_game.cpp)
  void realmSync();
  // every loaded settlement not yet known to the realm noted (true: one the realm holds for another owner). realmStep
  // calls it each step (fixer M4 r1: factored out; a load calls realmSync alone before re-entering a building)
  bool realmNoteSites();
  // the kingdom whose camp a war prop at overworld tile (tx, ty) belongs to (0: none / not a war prop): the view draws
  // it in that kingdom's colours (art::warPropSprite). war_game.cpp.
  ew::Gid warPropKingdom(int tx, int ty) const;
  // M2 quests (rpg/sim/quests.cpp). offerFor: the offer this NPC would make when asked for work, of a given type
  // (QType::COUNT: whatever they would offer); `ok` false when they have none of that kind. Scripts and tests use it
  // with debugAccept to drive every quest type from code.
  Quest offerFor(const Actor& npc, QType want, bool& ok);
  void debugAccept(const Quest& q) { acceptQuest(q); }
  bool questChestAt(int qid, int& tx, int& ty) const;   // inside: the tile of an Heirloom quest's chest (scripts)
  bool debugOfferTalk(int actorId, QType t);   // scripts: this NPC offers a job of that type as a dialogue (accept / decline)
  // M2 rumours (rpg/sim/wayside.cpp): mark the nearest unknown dungeon, wayside place or wonder as rumoured and return
  // the line that tells of it ("" when there is nothing left to hear of)
  std::string hearRumour();
  // M2 wayside: whether the player may cross the troll bridge of `site` today (WaysideOps::tollOf: the toll)
  bool tollPaid(int site) const;
  void respawn();
  int interactTarget() const;          // actor id the player would talk to (-1 none)
  bool foeInReach() const;             // (fixer M4 r3) an awake foe within striking reach: the use button attacks
  int interactProp(int& tx, int& ty) const;   // usable prop in front of the player (art::Prop + 1, 0 none)
  bool bedIsYours(int tx, int ty) const;      // M0b: a bed you may sleep in (an inn's beds are let room by room)
  bool nearDoorOrExit() const;
  float armorRating() const;
  float weaponDamage() const;
  // first level in ~4 min, level 5 in ~30 (PLAN.md targets; measured by rpg_test --metrics)
  int xpForNext() const { return gear::xpForNext(plLevel); }   // (M6) the tuned curve (rpg/sim/gear.cpp)
  const Quest* questById(int id) const;
  bool questTarget(int qid, int& tx, int& ty) const;   // overworld tile of the tracked quest's objective
  uint64_t npcKey(const Actor& a) const;
  bool isNight() const { return hour < 5.5f || hour > 20.0f; }
  float daylight() const;              // 0 night .. 1 noon
  // town defence (M0): the settlement whose bell is ringing (-1 none). The view plays combat music while it rings.
  int alarmSite = -1;
  int settlementAt(Vec2 p) const;      // the city/town/village whose footprint holds this overworld point (-1 none)
  int shelteredCount(int site = -1) const;   // townsfolk hiding indoors from a threat (site -1: all)
  // background traits (VISION_PLAN 15.1), applied where they act; small and visible in dialogue, shops and stats
  float blessingSecs() const;          // how long a shrine or temple blessing lasts
  float priceFactor(Role seller) const;   // what this background pays at a seller (1 = list price)

  // persistence. Only the current SAVE_VER loads (owner, 2026-10-04: old saves are not a concern); an older save is
  // refused and the title says so (saveVersion tells the UI which case it is). M2: SAVE_VER 6.
  void serialize(std::vector<uint8_t>& out) const;
  bool deserialize(const std::vector<uint8_t>& in);
  static int saveVersion(const std::vector<uint8_t>& in);   // the file's SAVE_VER (0: not a save at all)
  static int currentSaveVersion();
  // a current-format save whose world came from an older generator (ew::ENDLESS_GEN_VER):
  // refused like an older SAVE_VER, and the title words it the same way
  static bool saveFromOlderGenerator(const std::vector<uint8_t>& in);
  // M1 stable keys: an overworld chest (endless worlds key it by global tile, so it stays looted wherever the window is)
  uint64_t lootKey(int tx, int ty) const;   // (endless: by global tile)
  uint64_t npcKeyOf(int site, int bldg, int slot) const;   // npcKey's formula from handles (quest givers)

  // M1 endless streaming and NPC level of detail (SIM lane)
  void frameWork(double budgetMs);     // once per rendered frame: the streamer's work (web: generation within the budget)
  // (M3c carry) the interior made ahead for the building the player is walking up to (null: none ready); `bldg` is
  // its index in world.over.bldgs, so a view can bake its terrain and paint its pieces before the door is reached
  // (its map key is 100000 + bldg, as mapKey() will say once inside)
  const Map* preparedInterior(int& bldg) const;
  int prepBldg_ = -1;   // the building prep_'s ready map belongs to (index in world.over.bldgs)
  int prepHits = 0, prepMisses = 0;   // entries that found their interior made ahead / had to make it (tests, perf)
  // (M3c carry) the frame probe: the longest wall-clock gap between two frameWork calls (a whole rendered frame: the
  // update, the draw and the present) since the last reset (scripts' `perfmark`), and the frame it happened in
  double frameGapWorstMs = 0;
  double frameGapLastMs_ = 0;
  struct PerfCounters {
    int npcAwake = 0, npcAsleep = 0;   // townsfolk in `actors` this step, by LOD
    int hostiles = 0;                  // monsters and bandits in `actors`
    int activeSites = 0;               // settlements and camps whose people are streamed in
    int spawnedNpcs = 0, despawnedNpcs = 0;   // people streamed in / out so far
    // (M6 fixer) the worst ms of each world subsystem's step since --perf last reset them (once a second): the
    // systems a hitch can hide in (names: perfSysName)
    enum Sys { S_QUEST, S_REALM, S_WAR, S_STORY, S_LIFESTEP, S_RAID, S_FOE, S_LIFETICK, S_SPAWN, S_HOME, S_PARTY, S_COUNT };   // S_HOME: M7, S_PARTY: M8
    double sysWorstMs[S_COUNT] = {};
  };
  PerfCounters perf;
  static constexpr int FOLK_CAP = 80;          // townsfolk streamed in at once (nearest first)
  static constexpr int FOLK_IN = 34, FOLK_OUT = 44;   // tiles: a person streams in within FOLK_IN, out beyond FOLK_OUT
  static constexpr int SITE_IN = 26, SITE_OUT = 40;   // tiles from a site's area: it activates / deactivates
  // half the view in tiles plus a margin (the platform layer sets it from the logical canvas: the VIEW lane's screen
  // fit makes it wider on phones); townsfolk beyond it may sleep
  float sleepHalfW = 22.0f, sleepHalfH = 13.0f;
  // M5 TOWNSFOLK lane (rpg/sim/life_game.cpp): what the townsfolk's actors are doing (tests, scripts, --perf). Counts are
  // of the actors in play this step; the ms figures time lifeStep plus every resident's lifeFolk (the 15.12 budget:
  // 120 residents <= 1.5 ms per step on average)
  struct LifeFolkStats {
    int residents = 0;        // resident actors in play (Actor::resident >= 0)
    int walking = 0, seated = 0, working = 0, sleeping = 0, critters = 0;
    int spawned = 0, despawned = 0;          // resident actors brought in / put away so far
    int pathRequests = 0, pathCacheHits = 0, pathFails = 0;
    int stuck = 0;            // walkers that moved less than a tile in the last 30 s while their plan moved them
    int steps = 0;
    double lastMs = 0, worstMs = 0, sumMs = 0;
  };
  const LifeFolkStats& lifeFolkStats() const;
  int lifeFolkCap = 0;        // tests: resident actors a settlement may put on the street at once (0: FOLK_CAP)

  // test helpers
  bool godMode = false;
  bool noWildSpawns = false;           // metrics arenas: no roaming spawns or dens
  bool streamThreads = true;           // false: stream as the web build does (no worker; frameWork generates); tests
  // (Witness) a harness that compares runs: every cold generation the foes step would leave to a worker or spread over
  // ticks (named uniques, world bosses) is made at once on this thread, so a run never depends on thread timing or on
  // what an earlier run in the same process left in the memo
  bool syncStreams = false;
  // (Witness) the actor id counter: a recorded session's header carries the value its world began with, so a replay's
  // fresh Game hands out the same ids (ids feed foe ranks and affixes, orbit directions, loot rolls)
  int idCounter() const { return nextId_; }
  void setIdCounter(int n) { nextId_ = n; }
  void debugSpawn(art::Monster m, int n, float dist);
  int debugSpawnAt(art::Monster m, Vec2 at, int level);   // returns the actor id (already aggro)
  // (M6 fixer r2) a free spot with every tile within `rad` of it free too (a big body: a world boss is not stood on a wall)
  Vec2 freeSpotClear(int tx, int ty, int rad) const;
  void debugFell(int actorId, bool byPlayer = false);   // tests: an actor falls as if a monster struck it down (no killer), or as the player's kill
  void debugKit();                     // the pre-M0 starting kit (iron sword, hunting bow, 20 arrows, 3 potions, bread),
                                       // equipped: for fight scripts and bots once the real start is shirt-only
  // M0b: go into building bi (from anywhere, leaving the current sub-level) and up to floor f; false if f is not one
  // of its floors. Scripts, tests and save loading use it; play goes through doors and stairs.
  bool debugEnterBuilding(int bi, int floor = 0);
  void debugLeave() { if (inside) leaveSub(); }   // step out of the building or site (tests)
  bool debugEnterSite(int si);         // M2: into a cave or ruin from anywhere (the window comes along; tests, scripts)
  // the dragon's lair by name (its peak is named per world: makeDungeonName)
  std::string lairName() const { return world.lair >= 0 && world.lair < (int)world.sites.size() ? world.sites[(size_t)world.lair].name : std::string("THE DRAGON'S PEAK"); }
  // M1: put the player on a GLOBAL tile (the window recentres there), outdoors, on the
  // nearest free tile. Scripts (`at X Y`), --at and tests use it.
  void teleportGlobal(int32_t gx, int32_t gy);
  void changeFloor(int floor);         // inside a building: move to another of its floors, arriving by its stairs
  bool stairsAsleep() const { return stairsArrive_ >= 0; }   // just climbed: the stairwell down sleeps against a push north

 private:
  int nextId_ = 1;
  float exploreT_ = 0;
  float spawnT_ = 0;
  bool exitArmed_ = false;
  int leaveCol_ = -1;          // (owner) the interior exit column walked out by (an open front's bays: the matching bay)
  bool stairsArmed_ = false;   // M0b: the player has stepped off the stairs they arrived by
  bool stairsLatch_ = false;   // M0b: just arrived by the stairs: they stay asleep until the stick is let go or the
                               // player has walked more than a tile and a half away (no floor ping-pong while held)
  Vec2 stairsFrom_;            // where the player arrived
  Vec2 stairsOff_;             // M0b: the player's last position off the stairs (the stairwell's railing turns them back)
  int stairsArrive_ = -1;      // M0b: the tile (y * w + x) the player arrived on by the stairs: the stairs stay asleep
                               // until the player has stood on some other tile off the steps (-1: none)
  bool stairsNorth_ = false;   // M0b: the stick was let go while standing on the stairs: pushing north takes them
  Quest pendingOffer_;
  int dragonId_ = -1;
  // what each merchant has left this restock period (every 2 days): buying must not refill the shelf
  std::map<uint64_t, std::pair<int, std::vector<Item>>> shopCache_;
  int findActor(int id) const;
  void placePlayerAt(int tx, int ty);   // endless: recentres the window first when the tile is off its middle
  std::set<int> activeSites_;
  std::map<int, std::set<int>> felled_;   // site -> townsfolk slots felled while it is active (no respawn until it reloads)
  std::set<int> activeDens_;
  float denT_ = 0;
  bool perfectRoll_ = false;   // this roll already earned its slow-mo blip
  int denClearedDay(int den) const;    // -1 = not cleared
  // town defence (M0, ai.cpp)
  struct SiteAlarm { float lastThreatT = -99, bellT = 0, quietT = 0; bool ringing = false; int hostiles = 0; };
  std::map<int, SiteAlarm> alarms_;    // per active settlement (derived each frame, never saved)
  std::vector<Actor> sheltered_;       // townsfolk hiding indoors; they come back out ~30 s after the threat ends
  std::vector<int> navPrev_;           // path-finding scratch
  Rng rng_;
  void emit(Ev t, Vec2 p, int a = 0, float f = 1, const std::string& s = "") { events.push_back({t, p, a, f, s}); }
  void sfx(int s, Vec2 p, float pitch = 1, float vol = 1);
  void say(const std::string& s);

  void resetPlayer();
  void beginWorld();   // newGame / newEndlessGame: the player, the opening quests, the first actors
  void resetSession(); // beginWorld's first half: every per-game state back to a new game's, the player alone
  void recalcPlayer();
  void updatePlayer(float dt, const Input& in);
  void updateActor(Actor& a, float dt);
  void updateProjectiles(float dt);
  void updatePickups(float dt);
  // M1 (SIM lane): streaming and LOD
  float prefetchT_ = 0;
  Vec2 prefetchFrom_;                  // the player's position at the last wish-list update (heading)
  float siteScanT_ = 0;
  std::vector<int> hostiles_;          // actors indices that may threaten townsfolk (rebuilt each step)
  void updateLocation();
  void meleeHit(Actor& a);
  void damage(Actor& victim, float dmg, Vec2 from, int attacker, Ench ench = Ench::None, float enchPow = 0, bool crit = false);
  void kill(Actor& a, int killer);
  void gainXp(int xp);
  void addItem(const Item& it, bool announce = true);
  void giveGold(int g);
  void interact();
  void talkTo(Actor& a);               // (M7b) talkToInner, then the talk memory and the recorder
  void talkToInner(Actor& a);          // the conversation's opening (greeting, the hooks, the options)
  void dialogueChooseInner(int optIndex);   // (M7b) dialogueChoose's body (the wrapper records the turn)
  void openShop(Actor& a);
  void rest(int hours);
  void shootArrow();
  void castSpell();
  void enterSite(int site);
  void enterBuilding(int bldg, int col = -1);   // col: the front-row column walked in by (an open front's bay)
  void leaveSub();
  void loadMapActors();
  void clearNonPlayer();
  int spawnMonster(art::Monster m, Vec2 p, int level, bool boss);
  int spawnHuman(const Spawn& sp, Vec2 p);
  void makeLook(Actor& a, Role r, Rng& rr);
  void applyLevel(Actor& a, int level);
  bool hasOffer(const Actor& npc) const;
  void acceptQuest(const Quest& q);
  std::string greeting(const Actor& a, int salt = 0);   // (M7b salt: another pick when the first was said lately)
  // ---- M2 SIM lane
  // travel behind the fade (travel.cpp)
  int frameWorkCalls_ = 0;             // frameWork ran this session (else update pumps a thread-less streamer itself)
  // (M3c carry, the seat-of-power entry hitch) the ground floor of the building the player is walking up to, made
  // ahead (prepInteriorTick: natively on a worker thread, on the web in a frame the streamer leaves idle) and used by
  // enterBuilding. genInterior depends on the building alone (never its window position), so the map is the one
  // entering would make, bit for bit; it is keyed by the building's id and floor and dropped with a new world.
  struct PrepInterior {
    uint64_t key = 0;         // interiorKey of the map held (0: none)
    bool ready = false;
    Map map;
  };
  PrepInterior prep_;
  float prepT_ = 0;
#ifndef __EMSCRIPTEN__
  std::shared_ptr<std::future<Map>> prepJob_;   // (shared: Game stays copyable)
  uint64_t prepJobKey_ = 0;
  int prepJobBldg_ = -1;
#endif
  // ---- M4: the realm's runtime state (the hooks: RealmOps in realm_ops.h, WarOps in war_ops.h, StoryOps in story_ops.h)
  uint32_t realmSeen_ = 0;             // the realm's eventSerial() last applied (realm_game.cpp)
  uint32_t riseSeen_ = 0;              // (M12) the last realm event id told to the view as a rise banner (realm_game.cpp)
  size_t realmSites_ = 0;              // world.sites already registered with the realm (sites is append-only)
  int realmDay_ = -1;                  // the day the realm last advanced to
  ew::Gid realmLand_ = 0;              // whose land the player stood on (Ev::Border when it changes)
  bool realmLandKnown_ = false;
  bool foeWarmedTick_ = false;   // (M6 fixer r3) foeStep generated a cold region or kingdom cell this tick (one per tick)
  // ---- M7 Home hooks (VISION_PLAN 8, 15.2, 15.22). Called from the core loop (game.cpp / game_rpg.cpp) and DEFINED in
  //      rpg/sim/home_game.cpp (HOMESTEAD lane). Keep the calls where phase A put them.
 public:
  //   the horse: on horseback (Homes::riding), the speed factor on the player's walking speed this step (1 on foot;
  //   breed speed / walking speed, +RIDE_ROAD_BONUS on roads; updatePlayer multiplies by it), and getting off
  //   (home::Dismount: why; updatePlayer calls it on an attack, a bow shot, a spell or a roll, damage() when the
  //   player is hit, enterBuilding / enterSite, water under the hooves, sleep and travel). Public: the view and scripts
  //   ask homeRiding(); the UI's DISMOUNT button calls homeDismount(Player).
  bool homeRiding() const { return home.riding >= 0; }
  float homeSpeedMul() const;
  void homeDismount(home::Dismount why);
  // ---- M8 Fellowship (rpg/sim/party.h). Public: the UI (the radial, the paper doll, the portraits), the companions'
  //      talk (VOICE, party_talk.cpp), scripts and tests. DEFINED in rpg/sim/party_game.cpp (FELLOWS lane) unless named.
  //   the deeds bus: a deed of the player's (party::Deed; about: its subject's stable id: a site, a kingdom, a person;
  //   weight 1 normal, 2 a grave one). Queued; partyStep turns it into each member's approval, memory and reactions.
  void deed(party::Deed d, ew::Gid about = 0, int weight = 1);
  //   whether this person would join the player now, and on what terms (a tavern regular's willingness, a mercenary's
  //   wage, the party full...). why: the reason when not, said in the person's voice by the talk ("" when ok)
  struct PartyOffer {
    bool ok = false;
    party::Origin origin = party::Origin::Tavern;
    party::Vocation voc = party::Vocation::Warrior;
    int wage = 0;
    std::string why;
  };
  PartyOffer partyOffer(const Actor& a) const;
  //   they join: their census resident is marked RF_AWAY | RF_COMPANION and their street actor becomes the companion.
  //   Returns the new Party::members index, -1 when they would not
  int partyRecruit(Actor& a, party::Origin origin);
  //   someone who is no census resident joins (a mercenary, a saga's rescued or sworn ally: the VOICE lane's DSL
  //   `recruit` effect, the M2 missing person led home): a generated member of that culture and look seed, standing at
  //   `at` (map pixels). Returns the members index, -1 when the party is full
  int partyRecruitStranger(const std::string& name, bool female, uint64_t culture, uint32_t lookSeed, party::Origin origin, Vec2 at);
  //   a fallen companion's grave (Party::fallen index): 1 lay flowers (GF_FLOWERS), 2 take one keepsake from the
  //   belongings into the pack (GF_KEEPSAKE), 3 visited (GF_VISITED). false: nothing to do
  bool partyGraveAct(int fallen, int act);
  //   a member's order (the radial): false when it makes no sense now (a downed member cannot wait...)
  bool partyCommand(int member, party::Cmd c);
  // (fixer M8 r1) a member waiting or at home can be called to follow only where they are: their body in play within
  // CATCH_UP_TILES of the player (or their census street body is); else the player goes and fetches them
  bool partyCanCall(int member) const;
  //   the companion's paper doll: an item of the player's pack (inv index) handed over and put on; a worn slot
  //   (party::Slot) taken back into the pack; a potion or food given to keep (they drink potions below 25 % health)
  bool partyEquip(int member, int invIndex);
  bool partyUnequip(int member, int slot);
  bool partyGive(int member, int invIndex);
  //   parting for good: a census person goes home (their census life resumes), a mercenary simply leaves
  void partyDismiss(int member);
  //   a member says a line now (banter, a cry): Ev::Banter, the speech memory, Party::lastBanter
  void partySay(int member, const std::string& line, float secs = 4.0f);
  //   the companion's look: their census look (or their generated one) dressed in their gear in its makers' styles
  art::HumanLook partyLook(const party::Member& m) const;
  art::HumanLook heroLook(const Appearance& app) const;   // recalcPlayer's look: app dressed in the equipped gear (player.cpp)
  //   (M8 lead; rpg/sim/life_game.cpp) a census resident's street look on any actor, as the TOWNSFOLK rules dress them:
  //   the look, the name and the role the census gives them. false: no such resident
  bool dressAsResident(Actor& a, ew::Gid site, int idx);
  // ---- M8 phase B (FELLOWS lane; rpg/sim/party_*.cpp). What VIEW draws from (party.h "how VIEW draws the companions")
  //   the look of a fallen companion's grave (Party::fallen index): their people's rite, the weapon laid on it, fresh
  //   earth for 3 days, flowers once laid (GF_FLOWERS)
  art::GraveLook partyGraveLook(int fallen) const;
  //   the bonded animal's look (Party::members index of a Vocation::Hound member): species, age (pup / half grown /
  //   grown by the days since it was taken in), coat from its seed, the player's collar
  art::HoundLook partyHoundLook(int member) const;
  //   the tavern regulars of a settlement (census indices; VISION_PLAN 9: one in 40 % of towns, two in a city, none in a
  //   village): adults, no key people, whose evening plan is Act::Tavern at the inn. Deterministic; cached per session
  std::vector<int> partyRegulars(int site);
  //   a horse of a breed (home::Breed) bought for a member at a stablemaster (the breed's price; MF_MOUNTED, Member::horse)
  bool partyBuyHorse(int member, int breed, std::string& why);
  //   the bonded animal taken in as a pup (species 0 a hound from a farmer, a herder or a stablemaster; 1 a wolf pup at a
  //   cleared wolf den), standing at `at` (map pixels). Returns the members index, -1 with why ("ANIMAL", "FULL")
  int partyAdoptAnimal(uint8_t species, uint32_t seed, Vec2 at, std::string& why);
 private:
  uint32_t homeSerial_ = 0;            // the World::placeSerial last stamped (home_game.cpp)
  int32_t homeDryX_ = 0, homeDryY_ = 0; // (M7 phase B) the rider's last dry tile (global): a horse balking at water waits there
  // (M7 phase B, never saved) the home actors' clock and the night raid going on at a plot near the player (wolves
  // at the pen: home_actors.cpp)
  float homeActT_ = 0;
  struct HomeRaid { int plot = -1; std::vector<int> wolves, dead; float t = 0, atPen = 0; int outcome = 0; };   // dead: the wolves seen killed
  HomeRaid homeRaid_;
 public:
  const HomeRaid& homeRaid() const { return homeRaid_; }   // (tests, scripts) the live raid: outcome 1 driven off, 2 an animal taken
 private:
  friend struct HomeOps;               // M7 HOMESTEAD: the home's internals (home_game.cpp)
  friend struct PartyOps;              // M8 FELLOWS: the companions' internals (party_game.cpp)
  friend struct PartyTalkOps;          // M8 VOICE: the companions' talk (party_talk.cpp, banter.cpp)
  // the TOWNSFOLK lane's own runtime state (route caches, seat reservations, chat pairs...): defined in life_game.cpp
  // (shared: Game stays copyable; never saved)
  struct LifeRuntime;
  std::shared_ptr<LifeRuntime> lifeRt_;
  friend struct LifeOps;               // M5 TOWNSFOLK: the actors' internals (life_game.cpp)
  friend struct LifeHooks;             // M5 TOWNSFOLK / CITIZENS: the hooks into the core loop (rpg/sim/life_hooks.h)
  friend struct FoeOps;                // M6 FOES: the foes' internals (foes_game.cpp)
  friend struct FoeHooks;              // M6 FOES: the hooks into the core loop (rpg/sim/foe_hooks.h)
  friend struct CraftOps;              // M6 NUMBERS / FORGE: crafting's internals (craft_game.cpp)
  friend struct CraftHooks;            // M6 NUMBERS / FORGE: the hooks into the core loop (rpg/sim/craft_hooks.h)
  friend struct LootOps;               // M8b SOURCES: the loot internals for the tests (rpg/sim/loot_ops.h)
  friend struct PowerOps;              // M8b POWERS: the worn powers' internals (defined by the POWERS lane in powers.cpp)
  friend struct SkillOps;              // M9 SKILLS: the skills' hooks and internals (rpg/sim/skill_ops.h, skills_game.cpp)
  friend struct MagicOps;              // M9 MAGIC: casting, surfaces, statuses (rpg/sim/magic_ops.h, magic_game.cpp)
  friend struct MagicLaunch;           // (SOLID 3.5) MagicOps::launch's form groups (magic_launch.cpp), as MagicOps
  friend struct SeaOps;                // M10 SEA: boats, sailing, the Mist, fishing (rpg/sim/sea_ops.h, sea_game.cpp)
  friend struct AnimOps;               // P2a ANIM: the animation pick's hooks into the sim (rpg/sim/anim.h, anim.cpp)
  friend struct ServiceOps;            // M12 SERVICE: enlisting, ranks, orders, missions (rpg/sim/service_ops.h)
  friend struct LordOps;               // M12 LORD: the lordship and the player's realm (rpg/sim/lord_ops.h)
  friend struct JobOps;                // P1 JOBS: the job pools and the leads (rpg/sim/jobs_ops.h, jobs.cpp)
  friend struct RealmOps;              // M4 REALM: the living world's hook (rpg/sim/realm_ops.h, realm_game.cpp)
  friend struct WarOps;                // M4 WARDS: the war's hooks (rpg/sim/war_ops.h, war_game.cpp)
  friend struct TravelOps;             // M2 SIM: travel behind the fade, the interior made ahead (rpg/sim/travel_ops.h)
  friend struct WaysideOps;            // M2 SIM: wayside life, tolls, rumours (rpg/sim/wayside_ops.h, wayside.cpp)
  friend struct QuestOps;              // M2 SIM: the quests' machinery (rpg/sim/quest_ops.h, quests.cpp, quest_offers.cpp)
  friend struct AiOps;                 // the actors' behaviour: beasts, folk, town defence (rpg/sim/ai_ops.h, ai.cpp)
  friend struct StreamOps;             // M1 SIM: the endless window's upkeep, NPC level of detail (rpg/sim/stream_ops.h)
  friend struct StoryOps;              // M4 STORY: the story engine's hooks (rpg/sim/story_ops.h, story_game.cpp)
  friend struct ClimbOps;              // M11 CLIMB: moving over the relief, the long path search (game.cpp, ai.cpp)
  // M11 CLIMB (never saved): the long path search's cache (ai.cpp). A body whose short search (the bounded BFS round
  // buildings) cannot reach its goal (a cliff between, its way up 16-24 tiles along the face, stacked faces,
  // switchbacks) gets an A* path over a wider window, followed until it is walked or the goal moves off; a failed
  // search is remembered a moment so an unreachable goal costs one search, not one a frame.
  struct NavPath {
    int id = 0;                        // Actor::id
    int32_t gx = 0, gy = 0;            // the goal tile (global outdoors, local inside)
    int key = 0;                       // mapKey() the path was found on
    std::vector<int32_t> tiles;        // the way: tile pairs (x, y), global outdoors; the goal's last
    int at = 0;                        // the pair walked toward now
    float t = 0;                       // its age (s)
    float stuck = 0;                   // seconds without headway (a body in the way, a corner): re-planned past 0.6
    bool fail = false;                 // no way found (remembered NAV_FAIL_HOLD s)
  };
  std::vector<NavPath> navPaths_;
  std::vector<int32_t> navCost_;       // the long search's scratch (cost so far, parent, generation stamp)
  std::vector<int32_t> navFrom_;
  std::vector<uint32_t> navStamp_;
  uint32_t navGen_ = 0;
 public:
  // M11 CLIMB: path search counters (--perf, scripts' `climbnav`, rpg_test --climb); reset by whoever reads them
  struct NavStats {
    int requests = 0;                  // navStep calls that needed a path (the straight way shut)
    int shortFound = 0, shortFails = 0;   // the bounded BFS
    int longSearches = 0, longFound = 0, longFails = 0, failHeld = 0, cacheFollows = 0;
    int nodes = 0, worstNodes = 0;     // A* expansions, all and the worst single search
    double ms = 0, worstMs = 0;        // time in the path searches, all and the worst single navStep
  };
  NavStats navStats;
 private:
  // (M8b POWERS, never saved) > 0 while a power's own damage is being dealt: procs never fire from proc damage (a
  // Tempest arc never chains again, a star never calls a star); compStats_: each companion in play's worn gear summed
  // (actor id, stats), refreshed every step by PowerOps::step
  int powDepth_ = 0;
  std::vector<std::pair<int, gear::GearStats>> compStats_;
  // quests (quests.cpp)
  std::string pendingPitch_;           // the spoken offer (first person) for pendingOffer_
  // wayside life (wayside.cpp)
  float tollAskT_ = 0;                 // the troll asked a moment ago (no repeat while the player stays on the bridge)
  Vec2 offBridge_;                     // the player's last position off a toll bridge (the troll's demand turns them back)
  // the key killedSlots[0] holds for an overworld spawn of a site (M1: 4096 slots per site; saves store site id + slot)
  static int owKillKey(int site, int slot) { return site * 4096 + (slot & 4095); }   // (game.cpp, game_combat.cpp)
  // updateAI's behaviour sets (ai_beasts.cpp): p the target, toP / dist from the hostile to it
  struct BeastKind;                    // the kind flags (pack, brute, kite ...) and the windup
  // offerFor's pieces (quest_offers.cpp, QuestOps::offerBuild ... offerPitch)
  struct OfferCtx;                     // one offer being built (the giver, the dice, the grammar's variables)
};

// M11 CLIMB: moving over the relief (rpg/sim/game.cpp moveActor, ai.cpp navStep): the internals the tests and scripts
// drive (rpg_test --climb, rpg/view/script_climb.cpp)
struct ClimbOps {
  static void moveActor(Game& g, Actor& a, Vec2 d);
  static bool navStep(Game& g, Actor& a, Vec2 goal, float speed, float dt);   // walk toward goal around buildings; false = no path
  static bool bodyFree(const Game& g, Vec2 p, float r, bool flying = false, uint8_t swim = 0);   // an actor of radius r fits here (moveActor's box)
  static Vec2 freeSpot(const Game& g, int tx, int ty);
  static void clearPaths(Game& g) { g.navPaths_.clear(); }
  // the factor the ground under a body asks of it now (Map::reliefSpeed; 1 flying, swimming, aboard; the player's
  // sure-footed horse; softened a little by a high Athletics): what Actor::reliefK eases toward
  static float reliefTarget(const Game& g, const Actor& a);
  // the long path search (ai.cpp): an A* over the start and goal's box plus 40 tiles; the way as local tile pairs
  // (x, y; the goal's last) and the expansions it took
  static bool longSearch(Game& g, const Actor& a, int sx, int sy, int gx, int gy, std::vector<int32_t>& out, int& nodes);
  // (M7 fixer r2) townsfolk keep out of the player's yards (a villager stood on the new crops and shouldered the hero off
  // the row): true when tile (tx, ty) of the overworld lies in an owned plot that `a` (a friendly NPC that is not one of
  // the farm's own actors) is not already in. The way out stays open to one who is in. (home_game.cpp)
  static bool yardBarred(const Game& g, const Actor& a, int tx, int ty);
  // scripts: a town guard at `at` (map pixels; hostile: one hostile to the hero, who chases them as the watch chases a
  // wanted man); its Actor::id
  static int spawnGuard(Game& g, Vec2 at, bool hostile);
  static bool pillarHit(const Game& g, float x, float y, float rx, float ry);   // (owner) a body against an open front's pillars
  static bool solidAt(const Game& g, float x, float y, bool flying);
  // (M10) a point is a wall for a swimmer (Actor::swim 1: anything but water, or a prop on the water; 2: the land's own
  // rules ashore, water free)
  static bool swimSolid(const Game& g, float x, float y, uint8_t swim);
};
