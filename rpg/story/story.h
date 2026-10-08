// M4 "Banners": the quest and campaign engine (owner 15.9) and the realm's voice (rumours, news, lore: VISION_PLAN 4.6,
// 15.3). STORY lane.
//
// The engine runs DATA: quest and campaign scripts written in a small DSL (stages, objectives, conditions, dialogue
// trees with choices, variables, world-state hooks) and a caster that binds each script's ROLES to real entities of the
// living world (this kingdom and its ruler, that ruin and its record, this innkeeper, the history sim's events), so the
// stories come out of the world instead of floating above it. Scripts are text embedded in the code (no data files:
// the web build ships one wasm), parsed and validated at start-up, and walked headlessly by `rpg_test --story`.
//
// Ownership (M4 phase B): the STORY lane owns this header and rpg/story/*. It may ADD to it freely; what phase A
// declared here is the contract the other lanes use (VIEW: newsLine, lore() for the journal; Game's hooks in
// rpg/story/story_game.cpp): never rename or remove it.
//
// Every running story mirrors itself into Game::quests as a Quest of type QType::Story (title, desc, the tracked
// objective's marker through hasPos / tgx / tgy, state), so the journal, markers and the map need nothing new.
//
// Files (STORY lane, phase B):
//   story.h / story.cpp   the engine: instances, the save block, effects, objectives, the mirror into Game::quests
//   dsl.h / dsl.cpp       the DSL's data, its parser and its validator (rpg/story/dsl.h documents the language)
//   caster.cpp            binds a script's roles to real entities (sites, kingdoms, rulers, ruins, wars, events, people)
//   lib_tales.cpp         the tier-2 story library (archetypes from myth, folklore, scripture and classic fantasy)
//   lib_campaign.cpp      the first campaign (a succession crisis cast from the player's real kingdom)
//   rumours.cpp           newsLine, the culture voices, rumour reach and expiry, foreshadowing (VISION_PLAN 4.6)
//   lore.cpp              ruin records (with a local stand-in until the realm fills Realm::ruin), lore props, Lost History
//   places.cpp            notice boards and heralds (placed and spawned at runtime)
//   story_game.cpp        Game's story hooks (talk, choose, kill, enter, use a prop, step)
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "rpg/sim/realm.h"
#include "rpg/world/ids.h"

class Game;
struct Actor;
struct Item;
struct Spawn;
struct Quest;

namespace story {

// One cast binding: a role name in the script ("giver", "rival_king", "ruin") and the stable id it was bound to (a
// site, building, kingdom, npcKey or realm event id; what kind is the script's declaration of the role).
struct Binding {
  std::string role;
  uint64_t id = 0;
  std::string name;        // what the text calls it ("ASTRID", "THE KINGDOM OF OSKVAR", "THE RUINS OF HRAFNSTAD")
  // (STORY lane, block v2) what the caster learnt: the role's kind (dsl.h RoleKind), where it is (a site id and its
  // global tile: the quest marker), a person's sex, an npc's trade (Role) and small extra words (a ruler's title)
  uint8_t kind = 0;
  uint64_t site = 0;
  int32_t gx = 0, gy = 0;
  bool hasPos = false;
  bool female = false;
  uint8_t trade = 0;
  std::string extra;
};

// A running quest or campaign (saved in Game's story block).
struct Instance {
  uint32_t id = 0;               // serial
  std::string script;            // the script's name in the library
  int stage = 0;                 // index of its current stage
  int questId = 0;               // the Game::quests entry that mirrors it (QType::Story; 0 none)
  bool done = false, failed = false;
  std::vector<Binding> cast;
  std::map<std::string, int32_t> vars;
  // (block v2) the day the current stage began and its counter (kills so far)
  int32_t stageDay = 0;
  int32_t count = 0;
};

// 15.3: an entry of the "Lost History" journal: what the player has pieced together about a ruin or a fallen realm.
struct LoreEntry {
  uint64_t key = 0;              // the ruin's site id (or a kingdom id)
  std::string title;             // "THE FALL OF HRAFNSTAD"
  std::vector<std::string> lines;   // the clues found so far, in the order found
  int found = 0, total = 0;      // clues found of those that exist
  uint32_t foundMask = 0;        // (block v2) which clues (bit per clue index; bit 31: the statue's line)
  bool complete = false;         // (block v2) every clue found: the reward and the lead were given
};

// (block v2) consequences that persist (15.9): an NPC who remembers, a family who moved, a shop that changed hands, a
// succession the story made. Facts are the chronicle of the player's deeds the world talks about.
struct Memory { uint64_t who = 0; std::string line; int32_t day = 0; };
struct Fact { uint64_t site = 0; int32_t day = 0; std::string text; };
struct RulerOverride { ew::Gid kingdom = 0; std::string name, title; int32_t day = 0; };

// Library statistics (tests, reports)
struct LibraryStats { int scripts = 0, campaigns = 0, stages = 0, words = 0, options = 0; };

class Engine {
 public:
  void reset(uint64_t worldSeed);
  // the library (parsed and validated at first use): how many scripts, and the problems found (empty: all valid)
  int scriptCount() const;
  std::vector<std::string> validate() const;
  const std::vector<Instance>& running() const { return running_; }
  const std::vector<LoreEntry>& lore() const { return lore_; }
  // persistence (Game's story block: its own version byte first). An empty engine writes a block every later format
  // must still read as "nothing running" (the save fixture carries one).
  void serialize(std::vector<uint8_t>& out) const;
  bool deserialize(const std::vector<uint8_t>& in);

  // ---- (STORY lane) the library
  static LibraryStats libraryStats();
  static std::vector<std::string> scriptNames();        // every script's id, library order
  static std::string scriptTitle(const std::string& id);
  // ---- starting and driving stories. All take the Game they run in (its public state; Game's hooks install what the
  //      engine may do beyond that: rpg/story/story_game.cpp hostInstall).
  // cast `script` from a hook context (the hook speaker's actors id, or -1; the hook's site handle) and start it.
  // Returns the instance id (0: it could not be cast here; why says what failed).
  uint32_t start(Game& g, const std::string& script, int hookActor, int hookSite, std::string* why = nullptr);
  Instance* find(uint32_t id);
  const Instance* find(uint32_t id) const;
  Instance* byScript(const std::string& script);
  // the current stage's name of an instance ("" none)
  std::string stageName(const Instance& in) const;
  // the instance's current stage is a dialogue with this actor (storyTalk shows it)
  bool talksTo(const Game& g, const Instance& in, const Actor& a) const;
  // fill g.dlg with the instance's current dialogue stage (its say and options: action DLG_STORY + option index, arg the
  // instance id); false when the stage is not a dialogue
  bool showDialogue(Game& g, Instance& in);
  // choose option `opt` of the instance's current dialogue stage (checks evaluated; effects run; the dialogue goes on
  // or closes). forceIf: ignore `if` conditions (the headless path walker); forceCheck: 0 evaluate, 1 pass, 2 fail.
  bool choose(Game& g, uint32_t inst, int opt, bool forceIf = false, int forceCheck = 0);
  // test helpers: complete the current objective as if the player had done it (kill counts, items, places, days)
  bool debugComplete(Game& g, uint32_t inst);
  // the options of the current dialogue stage: labels, and whether each is a check (two outcomes) (tests)
  int optionCount(const Instance& in) const;
  bool optionIsCheck(const Instance& in, int opt) const;
  // the visible text of the current stage with the cast filled in (say, journal) (tests: no "{" may remain)
  std::string sayText(const Game& g, const Instance& in) const;
  std::string journalText(const Game& g, const Instance& in) const;
  // every text of the current stage filled in (say, journal, options, end notice): the walker checks them
  std::vector<std::string> stageTexts(const Game& g, const Instance& in) const;
  bool isEnd(const Instance& in) const;
  bool isGoal(const Instance& in) const;
  // per-step work (objectives that the world completes: places reached, days waited, realm conditions, items held)
  void step(Game& g);
  // events the hooks forward
  void onKill(Game& g, const Actor& victim, bool byPlayer);
  void onEntered(Game& g, int site, int bldg);
  bool onUseProp(Game& g, int prop, int tx, int ty);
  // the scripts an NPC may start (hook "npc <role>"), first the one they would offer now (-1 none): storyTalk
  int offerFor(Game& g, const Actor& a);
  // story hooks of a notice board / a herald / a ruin / a realm event near the player
  int offerForPlace(Game& g, int hookKind, int site);
  // forget a running instance (tests walking every path restore the engine between paths)
  void drop(uint32_t inst);

  // ---- consequences
  const std::vector<Memory>& memories() const { return memories_; }
  const std::vector<Fact>& facts() const { return facts_; }
  const std::vector<RulerOverride>& rulers() const { return rulers_; }
  const std::string* memoryOf(uint64_t who) const;
  // the ruler of a kingdom as the story knows it (a succession the player made wins over the realm's record)
  bool rulerOf(const Game& g, ew::Gid kingdom, std::string& name, std::string& title) const;
  // the times a script has ended (offers prefer the unplayed)
  int timesPlayed(const std::string& script) const;

  // (STORY lane: the engine's state; the lane adds what it needs)
  std::vector<Instance> running_;
  std::vector<LoreEntry> lore_;
  uint32_t nextId_ = 1;
  uint64_t seed_ = 0;
  std::vector<Memory> memories_;
  std::vector<Fact> facts_;
  std::vector<RulerOverride> rulers_;
  std::map<std::string, int32_t> played_;   // script -> times ended
  uint32_t proclaimed_ = 0;                 // the realm event serial heralds have proclaimed up to
  // runtime (never saved): the lore props of the ruin map the player is in, and the actors the engine spawned
  struct PropRef { int tx = 0, ty = 0; int clue = 0; int prop = 0; };
  std::vector<PropRef> ruinProps_;
  uint64_t ruinPropsSite_ = 0;
  float stepT_ = 0;
  float placeT_ = 0;

  LoreEntry* loreFor(uint64_t key);
  void addMemory(uint64_t who, const std::string& line, int day);
  void addFact(uint64_t site, const std::string& text, int day);
  // advance an instance into stage `st` (runs the stage's effects; an end stage finishes it)
  void enterStage(Game& g, Instance& in, int st);
  void finish(Game& g, Instance& in, bool success);
  void mirror(Game& g, Instance& in);
};

// The realm's voice (VISION_PLAN 4.6): one line telling of a world event, in the speaker's culture voice
// ("THEY SAY THE SIEGE OF HRAFNSTAD IS BROKEN, NINE DAYS AGO."). voiceCulture 0: the plain voice (the journal).
std::string newsLine(const Game& g, const realm::WorldEvent& e, uint64_t voiceCulture = 0);

// ---- (STORY lane) rumours (rpg/story/rumours.cpp)
// The rumour's life: 30 days (60 for a fall or a first contact), and its reach in tiles `age` days after the event
// (24 + 30 per day: VISION_PLAN 4.6 "reach grows by 30 tiles per day").
int rumourLifeDays(realm::EvType t);
int rumourReach(int ageDays);
// can someone standing at global tile (gx, gy) on `day` have heard of the event?
bool rumourReaches(const realm::WorldEvent& e, int day, int32_t gx, int32_t gy);
// war news (what a guard talks of)
bool warNews(realm::EvType t);
// the speaker chance (percent) for a role (innkeepers 70, travellers 50, guards 60, everyone else 25)
int speakerChance(int role);
// the event a speaker at (gx, gy) would tell of now (nullptr none): unheard first, then the freshest. guardOnly:
// war news only
const realm::WorldEvent* pickRumour(const Game& g, int32_t gx, int32_t gy, bool warOnly, uint64_t salt);
// tell of it: realm.markHeard, Ev::News, a place it names marked rumoured on the map. Returns the spoken line.
std::string hearEvent(Game& g, const realm::WorldEvent& e, uint64_t voiceCulture);
// a foreshadowing line for a kingdom on the road to war (prices, troops, refugees: 15.6.3); "" when all is calm
std::string foreshadowLine(const Game& g, ew::Gid kingdom, uint64_t voiceCulture, uint64_t salt);
// a speaker's voice family (0 plain .. 7) from a culture id
int voiceOf(const Game& g, uint64_t cultureId);
// "THREE DAYS AGO", "YESTERDAY"...
std::string agoWords(int days);

// ---- (STORY lane) lore (rpg/story/lore.cpp)
// the record behind a ruin: the realm's (Realm::ruin) when it has one, else a deterministic stand-in built from the
// site's culture and id (a pure function: placement and text stay stable across visits and saves)
realm::RuinRecord ruinRecordFor(Game& g, ew::Gid site);
// the lore props a ruin map holds (3-6, deterministic per site): placed into `m` (dungeon.cpp placeRuinLore) and
// returned with the clue each tells
std::vector<Engine::PropRef> placeRuinLore(Game& g, int siteHandle);
// read a lore prop at (tx, ty) in the current ruin (true: it was one). Adds to the Lost History entry; all clues found
// completes it (a reward and a lead)
bool readLore(Game& g, int tx, int ty);
// overworld ruins: a toppled statue of the last lord near the entrance (some ruins), placed at runtime
void placeRuinStatues(Game& g);
bool readStatue(Game& g, int tx, int ty);

// ---- (STORY lane) places (rpg/story/places.cpp)
// notice boards in town and city squares, heralds in capitals (runtime, restored identically each visit)
void placeBoards(Game& g);
bool boardTile(Game& g, int siteHandle, int& tx, int& ty);   // where the site's board stands (false: none)
void spawnHeralds(Game& g);
void heraldsProclaim(Game& g);
bool useBoard(Game& g, int tx, int ty);
void heraldTalk(Game& g, Actor& a);
bool boardChoose(Game& g, int action, int arg);   // DLG_STORY sub-range for board and herald options
bool isBoardSite(const Game& g, int siteHandle);

// ---- (STORY lane) what the engine may do to the game beyond its public state. Game's hooks fill this table with
// captureless lambdas written inside Game's member functions (so they may call its private helpers); the engine calls
// through it. Installed on the first hook call of any Game (update, talk...).
struct Host {
  void (*gold)(Game&, int) = nullptr;
  void (*xp)(Game&, int) = nullptr;
  void (*item)(Game&, const Item&) = nullptr;
  void (*sound)(Game&, int sfx, float pitch, float vol) = nullptr;
  void (*say)(Game&, const std::string&) = nullptr;
  int (*spawnHuman)(Game&, const Spawn&, float x, float y) = nullptr;   // the new actor's id
  int (*findActor)(const Game&, int id) = nullptr;                       // actors index (-1)
  void (*accept)(Game&, const Quest&) = nullptr;
  void (*complete)(Game&, int questId) = nullptr;
  int (*spawnMonster)(Game&, int monster, float x, float y, int level, bool boss) = nullptr;
  void (*loot)(Game&, int level) = nullptr;                              // a random piece of loot of a level
  void (*talkTo)(Game&, int actorId) = nullptr;                          // open the talk dialogue (tests, scripts)
};
Host& host();

// DLG_STORY sub-ranges (game.h DLG_STORY .. DLG_WAR): story options, hook offers, boards and heralds
constexpr int SA_OPT = 0;        // + option index (arg: instance id)
constexpr int SA_START = 300;    // + library script index (arg: hook site)
constexpr int SA_BOARD = 600;    // + board entry kind (arg: entry argument)
constexpr int SA_HERALD = 800;   // herald talk

}  // namespace story
