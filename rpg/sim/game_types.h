// EMBERVALE simulation: the plain types Game is made of (SOLID 4.1, split from game.h): Mode, Input, Actor, Projectile,
// Pickup, Event, Quest, the dialogue (DlgOpt, the DLG_* option ranges, Dialogue), Lodging, Travel and the shop's state.
// No lane headers: a file that needs only these types includes this header, not game.h (class Game).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "rpg/core/art_ids.h"
#include "rpg/art/art_life.h"   // art::Bubble (Actor::bubble)
#include "rpg/core/common.h"
#include "rpg/sim/factions.h"
#include "rpg/sim/items.h"
#include "rpg/sim/magic_status.h"   // magic::Status, STATUSES, statusBit (Actor::status)
#include "rpg/world/records.h"   // Role, ew::Gid

// Creator: the character creator after NEW GAME (the world exists; the player picks looks and a background).
// Forge (M6): the crafting screen at a workstation (Game::bench; rpg/view/craft_ui.cpp draws it, the NUMBERS lane).
// Build (M7): the build / decorate / storage / cooking screens of the player's property (Game::home.ui; rpg/view/
// buildmode.cpp draws it, the VIEW lane; the home:: actions apply it).
enum class Mode : uint8_t { Title, Play, Dialogue, Menu, Shop, LevelUp, Dead, Paused, Creator, Forge, Build };

// Game::storyFlags bits (saved, SAVE_VER 3). Bits 8..31 are free: define them next to the code that owns them,
// with a comment here when they become permanent.
constexpr uint32_t SF_CREATED = 1u << 0;        // the character creator was finished for this save
constexpr uint32_t SF_FIRST_WEAPON = 1u << 1;   // the opening's guaranteed first weapon has been handed out

struct Input {
  Vec2 move;
  bool attack = false, bow = false, spell = false, roll = false, interact = false, potion = false, swapSpell = false;
  // ---- M9 Arcana (MagicOps::input, SkillOps::step): the quick-cast slots (15.5: 3-4 on the phone). castSlot: the slot
  //      pressed this step (-1 none); castHold: a slot's button still held (hold to upcast; letting go casts); castDual:
  //      the cast is a dual-cast (a swipe on the phone, a modifier on the keyboard). sneak: toggles sneaking (Stealth)
  int8_t castSlot = -1;
  bool castHold = false, castDual = false;
  bool sneak = false;
  // ---- M10 Beyond the Mist (SeaOps::input): reel, the fishing reel held (the action button held while a line is out:
  //      the HUD sets it, rpg/view; the keyboard's J / Space held)
  bool reel = false;
};

// Down: a skeleton that has collapsed and may reassemble (a hit while it is down finishes it)
enum class AState : uint8_t { Idle, Walk, Windup, Strike, Recover, Roll, Hurt, Dead, Cast, Down };

enum class Spell : uint8_t { Flames, Heal, IceSpike, COUNT };
const char* spellName(Spell s);
int spellCost(Spell s);

struct Actor {
  int id = 0;
  bool player = false, npc = false, hostile = false, human = false, boss = false, ranged = false, flying = false;
  Faction faction = Faction::Town;   // who it fights (factions.h); `hostile` stays == factionsHostile(faction, Player)
  art::Monster mon = art::Monster::Wolf;
  art::HumanLook look;
  std::string name;
  Role role = Role::Villager;
  int level = 1;
  Vec2 p, vel, knock, aim{0, 1};
  int face = 0;            // 0 down, 1 up, 2 right, 3 left
  float animT = 0;
  // (P2b FOLK-SIM, never saved; rig 2, rpg/sim/anim.cpp animStep) the drawn speed smoothed (px/s), the gait (0 walk,
  // 1 run) and the seconds a sheathable weapon stays drawn
  float moveV = 0, drawnT = 0;
  uint8_t gait = 0;
  // (P2c FOLK-SIM-C, never saved; visual only) the block flash's seconds left (anim.h kBlockShow on the sword's parry and a
  // shield bearer's parry: skills_game.cpp; animStep runs it down at rig 2; the pick shows Clip::Block while it is > 0)
  float blockT = 0;
  float hp = 10, maxHp = 10, dmg = 1, speed = 40, armor = 0, radius = 5;
  float range = 14, aggroR = 100, windup = 0.3f;
  AState st = AState::Idle;
  float stT = 0;
  float atkCd = 0, shootCd = 0, flash = 0, burnT = 0, slowT = 0, regen = 0, iframes = 0;
  int combo = 0;
  float comboT = 0;
  bool hitDone = false;
  int xp = 0;              // reward
  // AI
  int target = -1;
  Vec2 home, goal;
  float thinkT = 0;
  bool aggro = false;
  int site = -1, slot = -1, bldg = -1;
  bool fromMap = false;    // created from a map Spawn (persistent identity)
  bool wild = false;       // dynamic wilderness spawn
  bool fly = false;        // dragon in the air
  // M10 Beyond the Mist (never saved): a swimmer's ground. 0 walks the land (water is a wall); 1 the sea's own: water
  // is ground and land a wall (the sea serpent, the kraken; the PLAYER while aboard a boat: SeaOps sets it); 2
  // amphibious: water and land both (merrow). moveActor and Game::swimSolid honour it.
  uint8_t swim = 0;
  float special = 0;       // ability timer
  bool dropped = false;
  // behaviour sets (game feel)
  bool heavy = false;      // the current windup is a heavy, roll-through attack (bears, trolls)
  bool lunge = false;      // the current strike is a wolf lunge (hits on contact while it travels)
  bool fleeing = false;    // goblins run at low health
  bool reassembled = false;   // skeletons get back up at most once
  int den = -1;            // World::dens index this pack member belongs to
  int atkN = 0;            // attacks made (heavy-attack cadence)
  int8_t orbitDir = 1;     // wolves: circling direction
  float lastHitT = -99;    // Game::time this actor last took damage (troll regen pauses)
  float plHitT = -99;      // (M4 integration) Game::time the player last struck it: a hunt beast the guards finish counts
  // town defence (M0): townsfolk run home and hide, brave ones with a tool fight as militia, guards converge
  bool militia = false;    // a brave adult with a tool (smith, farmer...): fights weakly when monsters come
  bool stallKeeper = false; // (M1 economy) keeps a market stall: stands behind its counter facing the customers
  bool indoors = false;    // reached its home door this frame: Game moves it indoors (out of `actors`) until it is safe
  int homeBldg = -1;       // overworld building it shelters in (-1 not chosen yet, -2 none)
  float fleeT = 0;         // seconds spent running for home during this threat
  int navGoal = -1, navNext = -1;   // tile path-finding: goal tile index and the next tile on the way
  float navT = 0;          // time until the path is re-planned
  int unreach = -1;        // guards: a target no path reaches (across a wall, in water) is ignored for unreachT s
  float unreachT = 0;
  // M1 NPC level of detail (VISION_PLAN 4.4): townsfolk well off screen with nothing to react to sleep (their AI runs
  // a few times a second with the gathered time instead of every step)
  bool asleep = false;
  float lodAcc = 0;
  // M2 (SIM lane, never saved): the quest this actor belongs to (Quest::id; 0 none): a missing person to lead out, a
  // beast of a farm raid, a named bandit chief. nightHome: a square-goer gone home for the night (sheltered until dawn)
  int quest = 0;
  bool nightHome = false;
  // M4 Banners (never saved): the kingdom this guard, soldier, captain or herald serves (0: none, a frontier militia or
  // a civilian). Who fights whom between kingdoms is Game::warHostile (war_game.cpp), from this and the realm's wars.
  ew::Gid realm = 0;
  // M5 Hearth and Hall (never saved). resident: the census index (life::Census::res) this actor embodies within its
  // site `site` (-1: not a resident: a wayside person, a soldier, a monster). posture: what the body is doing (art::Posture;
  // None = the standing sheet) and postureT its animation clock; useX / useY the furniture tile it is using (a chair, a
  // bed, an anvil: no two actors take the same one; -1 none). bubble: the speech icon over its head and how long it
  // stays (art::Bubble; a chat pair, a greeting, a bark). critter: a village animal (art::Critter + 1; 0 a person or a
  // monster): npc, not human, Faction::Town, drawn from art::critterSheet(kind, critterVar). lifeBits / lifeT / lifeA /
  // lifeB: per-actor scratch whose meaning the TOWNSFOLK lane defines in life_game.cpp (route steps, chat partner,
  // the seat reserved...).
  int resident = -1;
  // a keyed resident's other quest identity (its street body's (bldg -1, LIFE_SLOT0 + idx) when this is the building's
  // slot-0 keeper, the keeper's (building, slot) when this is the street body; altSlot -1: none): isGiver matches either
  int altBldg = -2, altSlot = -1;
  art::Posture posture = art::Posture::None;
  float postureT = 0;
  int useX = -1, useY = -1;
  art::Bubble bubble = art::Bubble::None;
  float bubbleT = 0;
  uint8_t critter = 0;
  uint32_t critterVar = 0;
  uint32_t lifeBits = 0;
  float lifeT = 0;
  int lifeA = -1, lifeB = -1;
  // M6 Steel (never saved; rpg/sim/foes.h, FOES lane fills them in foeSpawned). rank: foes::Rank; affixes: foes::Affix
  // bits; overlays / bodyTint / scalePct: its art::MonsterLook (variant overlays, recolour, draw scale; the view draws
  // monsterSheetLook and the aura from these); bossPhase: dungeon-boss phases passed (0, 1 at 66 %, 2 at 33 %);
  // affixT / affixT2: affix timers (summoner, blinking, burning trail...); unique: a named unique's or world boss's id
  // (0 none); pack: champion pack id (members share affixes; 0 none); culture: the cult::CultureId whose arms it wears
  // and whose make its loot is (soldiers, guards, bandits; 0 the land's own); dropD: the danger its loot rolls at (0:
  // its level)
  uint8_t rank = 0;
  uint16_t affixes = 0;
  uint16_t overlays = 0;
  uint32_t bodyTint = 0;
  uint8_t scalePct = 0;
  uint8_t bossPhase = 0;
  float affixT = 0, affixT2 = 0;
  ew::Gid unique = 0;
  int pack = 0;
  uint64_t culture = 0;
  int dropD = 0;
  // M8 Fellowship (never saved; rpg/sim/party.h). comp: party::Party::members index + 1 of the companion this actor
  // embodies (0: not a companion). A companion is npc, human (the hound is not), Faction::Player, never hostile; its
  // behaviour is the FELLOWS lane's (partyActor). partyTag: the FELLOWS lane's per-actor scratch (a mercenary for hire
  // at an inn, a grave's mourner...), meaning defined in party_game.cpp
  uint8_t comp = 0;
  uint8_t partyTag = 0;
  // M8b Treasure (never saved; rpg/sim/powers.h, the POWERS lane's scratch, meaning defined in powers.cpp / game.cpp):
  // powTag a per-actor mark (a summoned spectral wolf, an actor already struck by this chain...), powT its timer,
  // powBy the actor id that caused it (whose power burns / summoned it)
  uint8_t powTag = 0;
  float powT = 0;
  uint32_t powBy = 0;
  // (M8b POWERS, never saved) Affix::Burn's own fire: burnDps damage a second while burnDpsT lasts, set by burnBy's
  // blows (the classic burnT stays the fire enchantment's / Ember's); powCd a worn power's cooldown on this body (a
  // companion's Tempest / Starfall), powRegenT / powAuraT a companion's HealthRegen and BrambleCrown clocks
  float burnDps = 0, burnDpsT = 0;
  int burnBy = -1;
  float powCd = 0, powRegenT = 0, powAuraT = 0;
  // M9 Arcana & Mastery (never saved). status: magic::Status bits and statusT the seconds left of each, statusBy the actor
  // id that put the latest one on (kills credited to them); magicTag / magicT / magicBy: the MAGIC lane's per-actor
  // scratch (a conjured form and its time left, its caster; meaning defined in magic_game.cpp). bleedDps / bleedT /
  // bleedBy: a bleed (an axe's perk; the SKILLS lane's, like Affix::Burn's fire); skillTag / skillT: the SKILLS lane's
  // per-actor scratch (staggered, marked...; meaning defined in skills_game.cpp)
  uint16_t status = 0;
  float statusT[magic::STATUSES] = {};
  int statusBy = -1;
  uint8_t magicTag = 0;
  float magicT = 0;
  int magicBy = -1;
  float bleedDps = 0, bleedT = 0;
  int bleedBy = -1;
  uint8_t skillTag = 0;
  float skillT = 0;
  // M11 High Ground (CLIMB lane, never saved): the relief walking factor this body moves at now (Map::reliefSpeed of the
  // ground under its feet, eased over about 0.15 s of walking so a tile's edge never snaps the pace; moveActor)
  float reliefK = 1.0f;
  bool hasStatus(magic::Status s) const { return (status & magic::statusBit(s)) != 0; }
};

// M9 appends Spell: a spell's missile (Projectile::spell names it; the view draws it from the spell's FxStyle)
enum class ProjKind : uint8_t { Arrow, Fireball, IceSpike, Spit, Magic, DragonFire, Spell };
struct Projectile {
  Vec2 p, v;
  float life = 1.5f, dmg = 5, radius = 3;
  int owner = -1;
  bool fromPlayer = false;
  ProjKind kind = ProjKind::Arrow;
  Ench ench = Ench::None;
  float enchPow = 0;
  Faction fac = Faction::Player;   // who fired it: it hits whatever that faction is hostile to (factionsHostile)
  // (M8b POWERS, never saved) where it was loosed (SplitArrows splits past 6 tiles from here) and PJ_* bits
  Vec2 from;
  uint8_t powFlags = 0;
  // (M9, never saved) the spell this missile carries (magic::SpellDef id; 0 none: also set on the classic Fireball /
  // IceSpike kinds the classic spells still fly as) and its magnitude (upcast / dual-cast: magic::magnitudeOf)
  uint32_t spell = 0;
  float mag = 1;
  uint8_t upcast = 0;
  bool dual = false;
};
constexpr uint8_t PJ_SPLIT = 1;      // (M8b) already split (or a split's child): never splits again
constexpr uint8_t PJ_REFLECTED = 2;  // (M8b) turned back by ReflectBlock

struct Pickup {
  Vec2 p;
  Item item;
  int gold = 0;
  float t = 0;
  bool magnet = false;     // already flying to the player (sound played)
  bool full = false;       // (M6 fixer r2) a potion left lying because the belt is full: no magnet until there is room
  bool told = false;       // the "no more than 10 potions" notice was given for this approach (reset when walked away)
  bool sighted = false;    // (M8b fixer r3) view only: an Epic+ piece already chimed for (stable across pulls and window shifts)
};

// WindowShift (M1): the endless world's Active Window moved; p is the pixel offset every overworld position just got
// (the view moves its camera, particles and floating texts by it)
// Discover: a = the site, s = its name, f = 1 a place found, 2 a WONDER found (M2: the view may make more of it; the
// sim also gives 100 XP and a journal toast "WONDER FOUND: <NAME>")
// M4 Banners: News, the player heard of a world event (a = the realm::WorldEvent id, s = its line: the view shows a
// toast and the News journal gains it); Border, the player crossed into another kingdom's land (s = the kingdom's
// name, a = its banner colour (rgba), f = 1 entering a kingdom, 0 leaving into the wildlands: the view's herald banner
// "ENTERING THE KHAGANATE OF ...")
enum class Ev : uint8_t { Sfx, Hit, Blood, Explode, Sparkle, Dust, Heal, Frost, Text, Discover, LevelUp, QuestUpdate, Shake, MapChange, Notice,
                          WindowShift, News, Border,
                          Banter, PartyAlert, Power,
                          Spell, Skill,
                          Sea,
                          Rise };
// M12 Rise and Fall: Rise, a moment of the player's place in history for the view (a = RiseEv, rpg/sim/rise.h; f, s, id
// as noted there; View::riseEvent, rpg/view/render_rise.cpp, VIEW lane): enlisted, promoted, paid, orders, missions,
// a lordship granted, taxes, tribute, independence, a warlord or a world boss near
// M10 Beyond the Mist: Sea, a moment at sea for the view (a = sea::SeaEv, p, q, f, id, s: rpg/sim/sea.h; View::seaEvent,
// rpg/view/render_sea.cpp, ART lane): boarding, wakes, hull hits, the Mist, storms, fishing, voyages, first contact
// M9 Arcana & Mastery: Spell, a phase of a spell's life (a = magic::Phase, Event::spell = the spell id, p / q / f / id:
// see magic.h Phase; the view: View::magicEvent, rpg/view/render_magic.cpp, SPELLFX lane). Skill, a skill moment for the
// HUD (a = skills::Skill, f = the kind: 1 a level gained (Event::spell = the new level), 2 a perk point earned, 3 a perk
// taken (Event::spell = its index), 4 a book read, 5 levels trained, 6 mastered (100), 7 respec; s = the line to show;
// View::skillEvent, rpg/view/skills_ui.cpp, UI lane)
// M8b Treasure: Power, a worn power or affix fired (a = powers::Fx, p = from, q = to, f = magnitude, id = the actor it
// follows or 0): the view draws it (View::powerEvent, rpg/view/render_powers.cpp; POWERS lane)
// M8 Fellowship: Banter, a companion speaks (a = party::Party::members index, s = the line, f = seconds to show; the
// view shows it by their portrait and over their head; rpg/sim/banter.cpp through Game::partySay). PartyAlert: what the
// HUD must make plain (a = members index, f = the kind: 1 low health, 2 downed (bleeding out), 3 revived, 4 died, 5
// joined, 6 left, 7 complains, 8 a skill unlocked; s = the line to show)
struct Event {
  Ev type = Ev::Sfx;
  Vec2 p;
  int a = 0;          // Sfx id / colour / amount
  float f = 1;        // pitch / magnitude
  std::string s;
  float vol = 1;      // Sfx volume
  uint32_t id = 0;    // (M8) Ev::Banter / Ev::PartyAlert: the speaker's party::Member::id (a, their members index, can
                      // be stale by the time the view reads it: a member who dies that step is gone from the list)
  Vec2 q;             // (M8b) Ev::Power: the second point (a lightning arc's end, a falling star's mark)
  uint32_t spell = 0; // (M9) Ev::Spell: the spell id; Ev::Skill: the level reached / the perk index
};

// M2 (VISION_PLAN M2 sim lane, PLAN.md task 6) appends: Deliver (a parcel to an NPC in another settlement), Heirloom
// (retrieve a named item from a named chest in a named dungeon), Missing (find a person alive in a cave and walk them
// out), NamedBandit (a unique chief with a title), Protect (a night wave at a settlement's farm). Saved as u8: append.
// M4 Banners appends: Story (a running story quest or campaign stage mirrored from the story engine, rpg/story/story.h:
// Quest::stage is the engine's Instance id), War (a siege quest, "BREAK THE SIEGE" / "JOIN THE ASSAULT", WARDS lane:
// Quest::targetId the besieged site, Quest::stage the realm's Siege id, Quest::flags bit QF_WAR_ATTACK the side).
// M5 Hearth and Hall appends: Supply (a radiant quest from an unmet need, 15.12 "the baker has no flour": bring `need`
// units of a good (Quest::stage = ew::Good) to the giver or the settlement's stores; CITIZENS lane, life_quests.cpp).
// M10 Beyond the Mist appends: Sea (SEA lane, rpg/sim/sea_chain.cpp): Quest::stage the kind (1 the sea-chart chain of a
// hidden land: Quest::giverId the land's id, Quest::have the chain step, the marker the step's place; 2 "TELL KING X OF
// THE LAND BEYOND THE MIST": giverId the land, targetId the mainland capital; 3 a fishmonger's order: subject the fish,
// need / have the count).
// M12 Rise and Fall appends: Campaign (SERVICE lane, rpg/sim/service*.cpp: an enlisted player's order, Quest::stage the
// service::Order id, targetId / tgx / tgy its place), Envoy (SERVICE lane: a diplomacy mission, Quest::stage the
// service::Mission id, targetId the receiving court's seat), Fief (LORD lane, rpg/sim/lord*.cpp: a lord's task in a held
// village (settlers, works, a raid to stand against), Quest::stage the task id, targetId the holding).
// P1 Fresh Roads appends: Errand (LIFE lane, rpg/sim/life_game.cpp: a street incident's quest, jobs.h ErrandKind in
// Quest::stage, hasPos / tgx / tgy where it happens, subject the journal line; opened with Game::openErrand and ended
// with Game::finishErrand, which pays at once: nobody has to be found again).
enum class QType : uint8_t { Main, Clear, Hunt, Retrieve, Bounty, Deliver, Heirloom, Missing, NamedBandit, Protect, Story, War, Supply, Sea,
                             Campaign, Envoy, Fief, Errand, COUNT };
enum class QState : uint8_t { Active, Complete, Done };
struct Quest {
  int id = 0;
  QType type = QType::Clear;
  QState state = QState::Active;
  std::string title, desc, giverName;
  int giverSite = -1, giverBldg = -1, giverSlot = -1;   // NPC identity
  int target = -1;              // site
  art::Monster mon = art::Monster::Wolf;
  int need = 1, have = 0;
  int gold = 0, xp = 0;
  int stage = 0;                // main quest; M2 multi-step quests: their step
  // ---- M2 (SAVE_VER 6): targets anchored in the endless world, valid without any loaded handle (VISION_PLAN 2.11)
  ew::Gid targetId = 0;         // what the objective is by stable id: a site, a building (Deliver's recipient), a den
  bool hasPos = false;          // tgx, tgy hold the objective's GLOBAL tile (markers, walking time, the map)
  int32_t tgx = 0, tgy = 0;
  ew::Gid giverId = 0;          // the giver's site id (with giverBldg / giverSlot: the NPC)
  std::string subject;          // who or what it is about: "ASTRID", "GRANDMOTHER'S SILVER RING", "TORVALD THE RED"
  int destBldg = -1;            // Deliver: the recipient's building (handle; saved by id)
  int deadlineDay = -1;         // Protect: the day of the night attack (-1 none)
  uint32_t flags = 0;           // per-type progress bits (QF_*: defined by the sim lane next to the code that sets them)
};
// Quest::flags (M2 SIM lane, rpg/sim/quests.cpp). Bits 8-15 hold a Deliver's recipient Role; bits 16-31 are free.
constexpr uint32_t QF_DANGER = 1u << 0;   // offered as "DANGEROUS COUNTRY" (the target is beyond the player: +50 % reward)
constexpr uint32_t QF_PARCEL = 1u << 1;   // Deliver: the parcel was handed over at the accept (it is in the pack)
constexpr uint32_t QF_FOUND = 1u << 2;    // Missing: found and following / Heirloom: the chest was opened
constexpr uint32_t QF_WAVE1 = 1u << 3;    // Protect: the first wave came
constexpr uint32_t QF_WAVE2 = 1u << 4;    // Protect: the second wave came
constexpr uint32_t QF_FAILED = 1u << 5;   // the quest ended without success (state Done, no reward)
constexpr uint32_t QF_GRAVE = 1u << 6;    // Heirloom read on a lone grave: the item is laid on the grave (giverSlot -2)
constexpr uint32_t QF_ESCORTED = 1u << 7; // Missing: brought out alive (complete; the reward waits with the giver)
constexpr uint32_t QF_WAR_ATTACK = 1u << 16;   // (M4) War: with the attackers ("JOIN THE ASSAULT"); clear: the defenders
constexpr uint32_t QF_WAR_CAPDEAD = 1u << 17;  // (M4) War, BREAK THE SIEGE: the besiegers' captain fell (saved with the quest)
inline Role questRecipientRole(const Quest& q) { return (Role)((q.flags >> 8) & 0xFF); }

// grp (M7b Voice): 0 shown on the first page; n > 0 belongs to the option group n, shown when its header (A_GROUP, arg n)
// is chosen (Dialogue::group == n). Every option stays in Dialogue::opts, so code that picks one by its label finds it.
struct DlgOpt { std::string label; int action = 0; int arg = 0; uint8_t grp = 0; };
// M4: dialogue action ranges the lanes' own files handle (Game::dialogueChoose routes them): the story engine's
// options (rpg/story/story_game.cpp storyChoose) and the war's (rpg/sim/war_game.cpp warChoose). The built-in actions
// (game_internal.h DlgAct) stay below DLG_STORY.
constexpr int DLG_STORY = 1000, DLG_WAR = 2000, DLG_END = 3000;
// M5: the townsfolk's options (feed, employ, supply, befriend, the inn's meal; rpg/sim/life_game.cpp lifeChoose)
constexpr int DLG_LIFE = 3000, DLG_LIFE_END = 4000;
// M6 Steel: the foes' talk (innkeepers' warnings of named uniques and world bosses; rpg/sim/foes_game.cpp foeChoose) and
// the forge's (smelting, smithing, tanning, apprenticeship, learning secrets; rpg/sim/craft_game.cpp craftChoose)
constexpr int DLG_FOES = 4000, DLG_CRAFT = 4500, DLG_M6_END = 5000;
// M7 Home: the property's talk (the reeve / steward / innkeeper selling deeds, builders, stablemasters selling horses,
// seed sellers, hiring a farmhand, an innkeeper teaching recipes and cooking; rpg/sim/home_game.cpp homeChoose)
constexpr int DLG_HOME = 5000, DLG_HOME_END = 5500;
// M8 Fellowship: the companions' talk (recruiting, a companion's own conversation, orders, gear, their story and personal
// quest, the kin and friends who speak of them; rpg/sim/party_talk.cpp partyChoose, VOICE lane)
constexpr int DLG_PARTY = 5500, DLG_PARTY_END = 6000;
// M9 Arcana & Mastery: the trainers', companions' teaching and speech options (rpg/sim/skills_game.cpp SkillOps::choose,
// SKILLS lane) and the mage teachers' (rpg/sim/magic_game.cpp MagicOps::choose, MAGIC lane)
constexpr int DLG_SKILLS = 6000, DLG_SKILLS_END = 6500, DLG_MAGIC = 6500, DLG_MAGIC_END = 7000;
constexpr int DLG_SEA = 7000, DLG_SEA_END = 7500;   // (M10) harbourmasters, shipwrights, sailors, captains (SeaOps::choose)
// (M12) enlisting, orders and missions (ServiceOps::choose, service_game.cpp); the lordship (LordOps::choose, lord_game.cpp)
constexpr int DLG_SERVICE = 7500, DLG_SERVICE_END = 8000, DLG_LORD = 8000, DLG_LORD_END = 8500;
struct Dialogue {
  int actor = -1;
  std::string speaker, text;
  std::vector<DlgOpt> opts;
  Role role = Role::Villager;
  // M7b Voice (rpg/sim/talk.h): the standing of what `text` says now. speech::propose replaces the line only with one of
  // equal or higher priority (speech::Pri) and records its topic (speech::Topic), so the hooks of talkTo agree on ONE line
  // instead of the last hook overwriting or stitching onto the others. A direct write to text leaves them as they are.
  int pri = 0;
  uint8_t topic = 0;
  // (M7b) the option group on show (0: the first page; see DlgOpt::grp) and the line the first page said (A_BACK)
  uint8_t group = 0;
  std::string front;
  bool shows(const DlgOpt& o) const { return group == 0 ? o.grp == 0 : (o.grp == group || o.action == 0 /* A_BYE */); }
};

// M0b: the inn room the player has rented (SAVE_VER 4). It is theirs until noon of untilDay: its bed can be slept in,
// and the innkeeper sends them to it. room indexes the Map::rooms of that building's floor `floor` (a GuestRoom).
struct Lodging {
  int bldg = -1, floor = 0, room = -1;
  int untilDay = -1;
};

// M2 travel behind the fade (owner carry-over: the ~180 ms fast-travel / respawn hitch, worse on iPhone web). A journey
// started by beginTravel goes dark first (sleepFade held at 1), gathers the destination window's chunks across frames
// while the screen is black (Gather: the streamer's worker, or the web pump with a larger budget), then moves the
// window and the player in one cheap step (Arrive). The view bakes the terrain the arrival shows (budgeted, behind the
// same black) and then calls finishTravel(), which fades back in. Headless runs never call finishTravel: Arrive ends by
// itself after a short while, so tests can drive beginTravel through update() alone.
enum class TravelPhase : uint8_t { None, Gather, Arrive };
struct Travel {
  TravelPhase phase = TravelPhase::None;
  int site = -1;                // destination site handle (-1: a tile, gx / gy)
  int32_t gx = 0, gy = 0;       // destination GLOBAL tile (where the player will stand)
  uint8_t kind = 0;             // 0 fast travel on foot, 1 carriage (paid), 2 respawn after death
  float t = 0;                  // seconds in this phase
  float hours = 0;              // journey time (the clock moves by it on arrival)
  int gold = 0;                 // what it cost
  // M2 SIM lane (rpg/sim/travel.cpp): the destination window and what the journey cost the frame loop
  int32_t nox = 0, noy = 0;     // the window origin at the destination (global tile)
  bool move = false;            // the window moves (false: the destination already lies in the window's middle)
  double wall0 = 0;             // wall-clock ms when the journey began
  double gatherReal = 0;        // (M2 fixer round 2) real ms spent gathering while the game ran (the 2 s cap): a
                                // pause or an app switch stops it (each step adds at most 250 ms)
  double lastStepMs = 0;        // wall-clock ms of the last travel step
  int steps = 0;                // update steps spent travelling
  int syncChunks = 0;           // chunks the arrival generated on the main thread (0: all came from the streamer)
  double worstStepMs = 0;       // the slowest travel step (update + its streaming work)
  double arriveMs = 0;          // the arrival step (moving the window, placing the player)
  double gatherMs = 0;          // real time from the start to the arrival
  bool viewed = false;          // a view drives the arrival (View::travelArrive calls finishTravel once its picture is ready)
};
// what a journey would take (the map shows it before the player commits; VISION_PLAN 2.11)
struct TravelQuote {
  bool ok = false;
  std::string why;              // why not ("YOU CANNOT TRAVEL WITH ENEMIES NEARBY", "ACROSS THE SEA"...)
  float hours = 0;              // in-game hours on the road
  int gold = 0;                 // carriage fare (0 on foot)
};

struct ShopState {
  int actor = -1;
  uint64_t key = 0;        // merchant identity (npcKey)
  std::vector<Item> stock;
  std::string title;
};
