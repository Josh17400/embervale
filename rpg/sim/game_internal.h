// Helpers shared by the simulation's .cpp files (game.cpp, ai.cpp, game_rpg.cpp...). Not used by the view.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>
#include "rpg/culture/society.h"
#include "rpg/sim/game.h"
#include "rpg/sim/mark_tags.h"       // Mk, markKey and the register of every saved mark tag
#include "rpg/sim/monster_stats.h"   // MStat, mstat, heavyWindup, monsterName
#include "rpg/sim/sim_text.h"        // goodUnits, monsterPlural, dirWord, giverTown, the titles, rewardReadyMsg, fitLine
#include "rpg/world/ids.h"
#include "rpg/world/wildlife.h"

// the hero's footstep sound, s of walking (Game::update at rig 2, sim_schedule.cpp; updatePlayer at rig 1, game.cpp)
constexpr float kHeroStepPeriod = 0.29f;

namespace gsim {
using art::Monster;

inline Vec2 faceVec(int f) {
  static const Vec2 v[4] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
  return v[f & 3];
}
inline int faceOf(Vec2 d) {
  if (std::fabs(d.x) > std::fabs(d.y) * 1.05f) return d.x > 0 ? 2 : 3;
  return d.y > 0 ? 0 : 1;
}

// the faction a monster species belongs to (factions.h)
inline Faction monsterFaction(Monster m) {
  switch (m) {
    case Monster::Wolf: case Monster::Boar: case Monster::Bear: case Monster::Spider: case Monster::Bat: case Monster::Mudcrab:
    case Monster::IceWolf: case Monster::FrostSpider: case Monster::Sandworm:
    case Monster::Scorpion: case Monster::Hyena: case Monster::Lurker: case Monster::Yeti:   // (M3c wildlife)
      return Faction::Wild;
    case Monster::Skeleton: case Monster::Draugr: case Monster::Wraith: return Faction::Undead;
    case Monster::Dragon: return Faction::Dragon;
    default: return Faction::Monster;
  }
}

// (SOLID 4.1) an option in one of the lanes' ranges (game_types.h DLG_*) handed to its lane's choose
// (dlg_routes.cpp: one row per range, in the order dialogueChoose always asked them). true: handled
bool dlgRoute(Game& g, const DlgOpt& o);

// dialogue option actions (DlgOpt::action). M2 appends the wayside and quest ones.
enum DlgAct { A_BYE, A_TRADE, A_REST, A_RUMOR, A_ACCEPT, A_TURNIN, A_MAIN, A_HEAL, A_LEARN, A_CHAT, A_DECLINE, A_ASK, A_BED,
              A_DELIVER, A_ESCORT, A_TOLLPAY, A_TOLLREFUSE, A_NEWS, A_GRAVE,
              A_GROUP, A_BACK };   // (M7b) an option group's header (arg: the group) and the way back to the first page

// (fixer M7b r1, review: "script choose can pick options hidden in a closed option group") the option a player can
// reach by tapping: one on show now, else one behind a header on show (the header is tapped first), else one back on
// the first page or in another group (SOMETHING ELSE. first). Test scripts choose through this, so they walk the
// player's own way through the option groups. open: tap the way there and return the option's index once it is on
// show; !open: only say whether it is reachable (its index, the dialogue untouched). -1: the player cannot reach it.
template <class Match>
inline int dlgReach(Game& g, Match match, bool open) {
  auto shownAt = [&]() {
    for (size_t o = 0; o < g.dlg.opts.size(); o++) if (g.dlg.shows(g.dlg.opts[o]) && match(g.dlg.opts[o])) return (int)o;
    return -1;
  };
  for (int step = 0; step < 3; step++) {
    const int k = shownAt();
    if (k >= 0) return k;
    int target = -1;
    for (size_t o = 0; o < g.dlg.opts.size() && target < 0; o++) if (match(g.dlg.opts[o])) target = (int)o;
    if (target < 0) return -1;
    const int grp = g.dlg.opts[(size_t)target].grp;
    int tap = -1;
    for (size_t h = 0; h < g.dlg.opts.size() && tap < 0; h++) {
      const DlgOpt& hd = g.dlg.opts[h];
      if (!g.dlg.shows(hd)) continue;
      if (hd.action == A_GROUP && hd.arg == grp) tap = (int)h;   // its header is on this page
    }
    for (size_t h = 0; h < g.dlg.opts.size() && tap < 0; h++) {
      const DlgOpt& hd = g.dlg.opts[h];
      if (g.dlg.shows(hd) && hd.action == A_BACK) tap = (int)h;   // back to the first page, then on from there
    }
    if (tap < 0) return -1;
    if (!open) {   // (a dry run: its header is on the first page, or on this one)
      if (g.dlg.opts[(size_t)tap].action == A_GROUP || grp == 0) return target;
      for (const DlgOpt& hd : g.dlg.opts) if (hd.action == A_GROUP && hd.arg == grp && hd.grp == 0) return target;
      return -1;
    }
    g.dialogueChoose(tap);
    if (g.mode != Mode::Dialogue) return -1;
  }
  return -1;
}

// (M7b) the kind of a townsfolk option (life_game.cpp): 1 a service (the inn's meal, food for the stores), 2 a kindness
// (share food, a coin, a day's hire), 3 the chat ("HOW ARE YOU KEEPING?"), 0 anything else
int lifeOptKind(int action);


// where the player is on the overworld (the door or entrance they went in by, when inside)
inline void overworldTile(const Game& g, int& x, int& y) {
  if (g.inside && g.subBldg >= 0) { x = g.world.over.bldgs[g.subBldg].doorX(); y = g.world.over.bldgs[g.subBldg].doorY() + 1; return; }
  if (g.inside && g.subSite >= 0) { x = g.world.sites[g.subSite].ex; y = g.world.sites[g.subSite].ey; return; }
  x = (int)std::floor(g.pl().p.x / TILE); y = (int)std::floor(g.pl().p.y / TILE);
}
// is this NPC the one who gave the quest? (the opening's giver is whoever keeps the start village inn)
inline bool isGiver(const Quest& q, const Actor& a) {
  if (!a.npc) return false;
  if (q.giverSlot < 0) return a.role == Role::Innkeeper && a.bldg >= 0 && a.bldg == q.giverBldg;
  if (q.giverSite != a.site) return false;
  return (q.giverBldg == a.bldg && q.giverSlot == a.slot) || (a.altSlot >= 0 && q.giverBldg == a.altBldg && q.giverSlot == a.altSlot);
}


// M2 travel behind the fade (travel.cpp): the fade to black, and frameWork's generation budget once it is black
constexpr float kTravelFadeOut = 0.25f;
constexpr double kTravelBlackBudgetMs = 9.0;   // (11 let one chunk job overrun the 16 ms step on the web after M7b cities)
}  // namespace gsim
using namespace gsim;

// ---- M7 HOMESTEAD lane: the home's internals (Game's friend). Defined in rpg/sim/home_game.cpp (the actions' helpers,
//      riding), home_talk.cpp (deeds, taxes, builders, stables, recipes, the miller, the farmhand's hire),
//      home_actors.cpp (farm animals, the horse, builders, the farmhand, predators) and home_interior.cpp (the player's
//      own house: furniture, stores, beds, cooking places).
struct HomeOps {
  static void say(Game& g, const std::string& s) { g.say(s); }
  static void give(Game& g, const Item& it) { g.addItem(it); }
  static void notice(Game& g, const std::string& s, uint32_t colour) { g.emit(Ev::Notice, g.pl().p, (int)colour, 0, s); }
  static void news(Game& g, const std::string& s) { g.emit(Ev::News, g.pl().p, 0, 1, s); }
  static void sfx(Game& g, int s, Vec2 p, float pitch = 1, float vol = 1) { g.sfx(s, p, pitch, vol); }
  // take n of the pack items matching pred (false, nothing taken: not enough)
  template <class F>
  static bool take(Game& g, int n, F&& pred) {
    int have = 0;
    for (const Item& it : g.inv) if (pred(it)) have += it.stackable() ? it.count : 1;
    if (have < n) return false;
    for (size_t i = 0; i < g.inv.size() && n > 0;) {
      Item& it = g.inv[i];
      if (!pred(it)) { i++; continue; }
      const int k = std::min(n, it.stackable() ? it.count : 1);
      n -= k;
      it.count -= k;
      if (it.count <= 0 || !it.stackable()) dropAt(g, (int)i);
      else i++;
    }
    return true;
  }
  template <class F>
  static int count(const Game& g, F&& pred) {
    int have = 0;
    for (const Item& it : g.inv) if (pred(it)) have += it.stackable() ? it.count : 1;
    return have;
  }
  static void dropAt(Game& g, int i);
  static bool hasTool(const Game& g, home::Tool t);
  static int settlementOf(const Game& g, int32_t gx, int32_t gy);
  static void restamp(Game& g) { g.homeSerial_ = g.world.placeSerial + 0x80000000u; }
  static int nextId(Game& g) { return g.nextId_++; }
  static int findActor(const Game& g, int id) { return g.findActor(id); }
  static void makeLook(Game& g, Actor& a, Role r, Rng& rr) { g.makeLook(a, r, rr); }
  static int spawnMonster(Game& g, art::Monster m, Vec2 p, int level) { return g.spawnMonster(m, p, level, false); }
  static void moveActor(Game& g, Actor& a, Vec2 d) { ClimbOps::moveActor(g, a, d); }
  static bool bodyFree(const Game& g, Vec2 p, float r) { return ClimbOps::bodyFree(g, p, r, false); }
  static const std::vector<int>& hostiles(const Game& g) { return g.hostiles_; }
  static void travelHomeOnRespawn(Game& g, int32_t gx, int32_t gy);
  // the census and resident an actor embodies (false: not a resident)
  static bool residentOf(Game& g, const Actor& a, life::Census*& c, int& idx, int& site);
  // the settlement an actor belongs to (its site, or its building's site; -1 none)
  static int siteOfActor(const Game& g, const Actor& a);
  // ---- home_talk.cpp
  //   talkTo: reeves, stewards, innkeepers (deeds, recipes, cooking), builders, stablemasters, seed sellers, farmhands:
  //   options in [DLG_HOME, DLG_HOME_END); called after LifeHooks::talk
  static void talk(Game& g, Actor& a);
  static bool choose(Game& g, const DlgOpt& o);
  // ---- home_actors.cpp
  static void actorsStep(Game& g, float dt);
  static bool folk(Game& g, Actor& a, float dt);       // a home actor's behaviour (lifeFolk routes LB_HOMESTEAD here)
  static bool critterUse(Game& g, Actor& a);           // talkTo on a farm animal or the waiting horse: true handled
  static std::string critterLabel(const Game& g, const Actor& a);   // what using it does ("RIDE", "PET CLOVER")
  static void dismounted(Game& g, home::Dismount why); // the horse stays where the rider got off (or goes home)
  static void dayTurned(Game& g, int plot, int day);   // a new day on a plot near the player: the live raid roll
  static void clearHomeActors(Game& g);
  static int raidStart(Game& g, int plot);             // home::forceRaid
  static void wakeActors(Game& g) { g.homeActT_ = 0; } // the home actors are reconciled on the next step
  static Vec2 freeSpot(const Game& g, int tx, int ty) { return ClimbOps::freeSpot(g, tx, ty); }
  static void talkTo(Game& g, Actor& a) { g.talkTo(a); }   // (tests, scripts) open a conversation
  static void interact(Game& g) { g.interact(); }           // (tests, scripts) the use button
  // ---- home_interior.cpp
  //   a building interior or a site map was just made (enterBuilding / enterSite, after questMapLoaded): the player's
  //   own house gets its furniture, stores and wall decor stamped
  static void mapLoaded(Game& g);
  //   the player used a prop (a cooking hearth / pot / campfire: cook; a bed of their own: sleep): true = handled.
  //   Called first among the prop handlers (before LifeHooks::useProp).
  static bool useProp(Game& g, art::Prop p, int tx, int ty);
  //   interactProp: a prop the player may use for the home's sake (a hearth, a cooking pot, a campfire to cook at;
  //   the player's own stores and furniture): true makes it usable (useProp then handles it)
  static bool propUsable(const Game& g, art::Prop p, int tx, int ty);
  //   bedIsYours: a bed in the player's own house (true: sleep, respawn here)
  static bool bed(const Game& g, int tx, int ty);
  static void stampObj(Map& m, const home::PlacedObj& o, bool clear);   // one piece of furniture into / out of a map
  static int insideObjAt(const Game& g, int plot, int tx, int ty);     // Plot::inside index at a tile of this floor
  // ---- the lane's hooks into the core loop (were Game::home*, SOLID 4.1)
  //   once per update step (after foeStep): stamp the owned plots when World::placeSerial changed, offline catch-up of a
  //   plot coming into the window, construction days, the farmhand, animals in their pens (critter actors), predators at
  //   night, the troughs, taxes, discovered styles (a settlement found: its culture), riding rules (water, cliffs)
  static void step(Game& g, float dt);
  //   interact(): a tile of an owned plot in front of the player (till, plant, water, harvest with the tool in hand;
  //   collect from a coop; open a store; mount a horse; the FOR SALE sign): true = handled. Called before the props.
  static bool plotInteract(Game& g);
  //   useItem: food eaten (a cooked meal's Well Fed, after lifeAte)
  static void ate(Game& g, const Item& food);
  //   shopStock: seeds (by the merchant culture's staples), tools, hay, saddles... appended to a merchant's stock
  static void shopStock(Game& g, const Actor& merchant, std::vector<Item>& stock);
};
// Actor::lifeBits bit of the actors the HOMESTEAD lane spawns (farm animals, the waiting horse, builders, the
// farmhand): lifeFolk hands them to HomeOps::folk. Actor::slot tells which (HOME_SLOT_*).
constexpr uint32_t LB_HOMESTEAD = 1u << 20;
constexpr int HOME_SLOT_ANIMAL = 8000;   // + plot * 64 + animal
constexpr int HOME_SLOT_HORSE = 8999;    // the horse out of its stable, waiting
constexpr int HOME_SLOT_BUILDER = 9000;  // + plot * 4 + k
constexpr int HOME_SLOT_HAND = 9100;     // + plot

// ---- M8 FELLOWS lane: the companions' internals (Game's friend). Defined across rpg/sim/party_game.cpp (records,
//      the hooks' bodies, deeds, downed and death), party_ai.cpp (fighting by vocation and stance, the bond skills,
//      potions), party_move.cpp (following, the trail, the catch-up, waiting, living at the player's house, riding,
//      the carry), party_recruit.cpp (offers, recruits, tavern regulars, sellswords, the rescued, the animal, the
//      stake from the real world) and party_graves.cpp (graves sited, stamped and visited; the looks VIEW reads).
struct PartyOps {
  // Game::marks tags (Mk 112..127)
  static constexpr uint64_t MK_HIRED = 112;   // a sellsword (inn Bldg id * 4 + slot): the day they were hired (gone a while)
  static constexpr int32_t MERC_FALLEN = 2147483647;   // MK_HIRED's value once that sellsword died: the slot stays empty
  static constexpr uint64_t MK_PUP = 113;     // a wolf den (Den id) whose pup was taken / a seller's pup (npcKey): 1
  static constexpr uint8_t PT_WALKOFF = 16;   // Actor::partyTag: a body walking home out of sight (left, dismissed, sent home)
  // ---- records (party_game.cpp)
  static party::Member* of(Game& g, const Actor& a);
  static int indexOf(const Game& g, const party::Member& m) { return (int)(&m - g.party.members.data()); }
  static ew::Gid homeland(const Game& g, const party::Member& m);
  static int actorOf(Game& g, int mi);                 // actors index of member mi (-1 not in play)
  static void removeMember(Game& g, int mi);
  static void dropActor(Game& g, int mi);              // the member's body leaves play (the record stays)
  static life::Resident* resident(Game& g, const party::Member& m);
  static void alert(Game& g, int mi, int kind, const std::string& s);
  static void kit(Game& g, party::Member& m, int level);
  static void body(Game& g, const party::Member& m, Actor& a);
  static bool baseLook(Game& g, const party::Member& m, art::HumanLook& out);   // (fixer M8 r1) street look, before gear
  static void fixLook(Game& g, party::Member& m);      // Member::look fixed once their census has dressed them
  static int spawn(Game& g, int mi, Vec2 at);          // the member's actor at `at` (map pixels): its Actor::id
  static void dieForGood(Game& g, int mi, const std::string& cause);
  static void censusDeaths(Game& g);                   // (fixer M8 r1) members who died in their census life: Fallen
  static void applyDeed(Game& g, const party::Party::Pending& d);
  static void daily(Game& g);
  static void downedStep(Game& g, float dt);           // bleeding out, the binding, the player's kneel
  static void revive(Game& g, int mi, Actor& a, bool fromAfar);
  //   a companion actor's whole behaviour this step (updateActor, after knock-back, before the skeleton's Down rule):
  //   true = handled. Following, fighting by vocation and stance, skills, downed, mounted
  static bool actorStep(Game& g, Actor& a, float dt);
  static void leave(Game& g, int mi, bool angry);      // Left (angry) or dismissed: the body walks home out of sight
  // ---- P3 PARTY: gifts, the one door, partings and returns, the death pile (party_game.cpp; returns party_recruit.cpp)
  //   the one door from a companion to the hero's pack: m.inv[e] into Game::inv, refused for a kit piece (IF_KIT)
  static bool toHero(Game& g, party::Member& m, int e);
  //   their stowed kit piece for this slot put back on (an empty slot stays empty when they have none): true = worn
  static bool wearKit(party::Member& m, int slot);
  //   every gift handed back through the door, their stowed kit re-worn in each slot a gift held: the count
  static int giftsBack(Game& g, int mi);
  //   every living parting (dismissed, unpaid, a personal quest's farewell, a sellsword's angry leave): the gifts back
  //   (the alert names how many), a sellsword's hire mark set to the day, the body walks off, and the record into
  //   Party::parted when they can be met again (else it goes, as before); the member is removed
  static void part(Game& g, int mi, bool angry);
  //   a parted friend comes back as themselves (rule 8): Party::parted[pi] moved into the party (its kit made only when
  //   it holds none: an old save's companion); their actor at `at`; the members index (-1: no room)
  static int rejoin(Game& g, int pi, Vec2 at);
  //   a death: everything they carried laid on the ground round `at` as Pickups (IF_KIT cleared, one per item, spread
  //   by each item's seed: no new random draw); nothing when `onMap` is false (no body on the hero's map)
  static void dropAll(Game& g, party::Member& m, Vec2 at, bool onMap);
  //   parted census companions the census shows dead: their records leave the roll
  static void partedDeaths(Game& g);
  // ---- fighting (party_ai.cpp)
  static int pickFoe(Game& g, const Actor& a, const party::Member& m);
  static bool fight(Game& g, Actor& a, party::Member& m, int mi, float dt);   // true: the fight drove the body this step
  static bool useSkill(Game& g, Actor& a, party::Member& m, int k, bool ordered);   // skill k of skillsOf(voc) now
  static void autoSkills(Game& g, Actor& a, party::Member& m, int mi);
  static void potion(Game& g, Actor& a, party::Member& m);
  static void heal(Game& g, Actor& a, party::Member& m, int mi);   // the healer's mend (base and Mend)
  static float tauntT(const party::Member& m);         // > 0 while the warrior's taunt holds
  static bool ranged(const party::Member& m) { return m.voc == party::Vocation::Archer || m.voc == party::Vocation::Mage; }
  static void strike(Game& g, Actor& a, party::Member& m);         // the blow / the shot at the end of a windup
  // ---- moving (party_move.cpp)
  static Vec2 slotBehind(const Game& g, int mi);
  static Vec2 spotBehind(Game& g, int mi);
  static void trailStep(Game& g);                      // the player's footsteps (Party::trail)
  static bool moveTo(Game& g, Actor& a, party::Member& m, Vec2 goal, float speed, float dt);   // navStep, else the trail
  static void follow(Game& g, Actor& a, party::Member& m, int mi, float dt);
  static Vec2 outOfSight(Game& g, int mi, int& crumb); // where a catch-up lands: behind the player, off the screen
  static void keepUp(Game& g, float dt);               // members with the player brought into play, the catch-up
  static void placesStep(Game& g);                     // waiting members and those at the player's house in and out of play
  static void homeLife(Game& g, Actor& a, party::Member& m, float dt);   // a member at the player's house: their day
  static int homePlotOf(const Game& g, const party::Member& m);         // Homes::plots index of Member::homePlot (-1)
  static float riderSpeed(const Game& g);              // the player's pace on horseback (px/s)
  // ---- recruiting (party_recruit.cpp)
  static void mapChanged(Game& g);                     // a new map: sellswords at an inn
  static int mercsAt(const Game& g, int bldg);         // sellswords an inn keeps (0..MERC_MAX)
  static int mercWage(const Game& g, int site);        // gold a day by the place's danger
  static party::Vocation mercVoc(uint32_t seed);
  //   (ai.cpp updateFolk) an actor the FELLOWS lane put in the world that is not a companion (a sellsword at an inn:
  //   Actor::partyTag): true = handled
  static bool folk(Game& g, Actor& a, float dt);
  static bool denInteract(Game& g);                    // a cleared wolf den's pup (interact)
  // ---- graves (party_graves.cpp)
  static void siteGrave(Game& g, party::Fallen& f);   // into their home's temple yard / graveyard, or where they fell
  static void gravesStep(Game& g);                     // unsited graves sited when their home loads; stamping
  static int graveNear(const Game& g, Vec2 p, float r);   // a sited grave within r px of p (Party::fallen index, -1)
  // ---- the lane's hooks into the core loop (were Game::party*, SOLID 4.1)
  //   the fallen companion whose grave stands on this LOCAL overworld tile (-1 none; only GF_SITED graves)
  static int graveAt(const Game& g, int tx, int ty);
  //   Called from the core loop (game.cpp, game_rpg.cpp, ai.cpp, quests.cpp) and DEFINED in rpg/sim/party_game.cpp
  //   unless named otherwise. Keep the calls where phase A put them. Each must cost next to nothing while Game::party
  //   is empty.
  //   once per update step (after homeStep, timed as PerfCounters::S_PARTY): the pending deeds, members brought into
  //   play beside the player after any map change (a door, stairs, a journey, a load, a respawn), the catch-up rule,
  //   bleeding out, bond by the day, wages, the waiting and the at-home, graves stamped into the window, banter
  //   (party::banterStep, VOICE) last
  static void step(Game& g, float dt);
  //   kill() of a companion actor, first thing: true = handled (downed instead of dead); false lets the death happen
  //   (the lane then makes it permanent: the grave, the grief)
  static bool fell(Game& g, Actor& a, int killer);
  //   damage(), after foeHit: the blow about to land on anyone: the damage to apply (shield wall, defend) and the
  //   share statistics (Party::stats)
  static float hit(Game& g, Actor& victim, float dmg, int attacker);
  //   kill(), after foeKilled: deeds from what died (an innocent, a guard, bandits, a named beast) by the player's or
  //   a companion's hand; a companion's kill count
  static void killed(Game& g, const Actor& victim, int killer);
  //   pickTarget: how much closer a hostile counts a companion (1 = as is; taunting < 1; downed or passive > 1;
  //   0 = never: not a target at all)
  static float aggro(const Game& g, const Actor& hostile, const Actor& member);
  //   completeQuest / questFail: the deeds of a quest's end
  static void quest(Game& g, const Quest& q, bool success);
  //   rest(): the members with the player rest too (health, the wounded healed)
  static void rested(Game& g, int hours);
  //   interact(), before homeInteract: a downed companion in reach (bind their wounds), a grave of the fallen (read
  //   it, lay flowers, take a keepsake): true = handled
  static bool interact(Game& g);
};
// (M8 FELLOWS, life_game.cpp) a companion's body laid in the bed at interior tile (x, y) / sat on the seat there (the
// TOWNSFOLK rules' fit: the head on the pillow, the seat line on the seat), or false when that prop is no bed / seat
bool lifeLayInBed(Game& g, Actor& a, int x, int y);
bool lifeSitOn(Game& g, Actor& a, int x, int y, art::Posture post);
