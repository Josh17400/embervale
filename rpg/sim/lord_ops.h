// M12 "Rise and Fall": the lordship's Game hooks (Game's internals are private; LordOps is its friend, game.h). LORD lane
// (it may ADD functions; the hooks below are called from where phase A put them and keep their meaning). Defined in
// rpg/sim/lord_game.cpp (phase A: a lordship granted / bought / resettled, the daily taxes into the hall's coffers, the
// tribute every 30 days, independence and the player's realm with the player's own heraldry) and the LORD lane's own
// files (lord_*.cpp: the steward, works raised in the village, the militia, the liege's anger and the war for
// independence, growing the realm by conquest, founding a village on the player's land, the crown).
//
// Where each hook is called (phase A wired them all):
//   update          step        after SeaOps::step, before ServiceOps::step: the day's taxes, tribute due, works done
//   talkTo          talk        the lordship hook (P_SERVICE): the steward / elder of a held village (COFFERS, TAX,
//                               BUILD, PAY TRIBUTE, DECLARE INDEPENDENCE), rulers (ASK FOR A LORDSHIP, BUY A CHARTER);
//                               options in [DLG_LORD, DLG_LORD_END)
//   dialogueChoose  choose      DLG_LORD .. DLG_LORD_END
//   completeQuest   questDone   a Fief quest done (loyalty, settlers)
//   resetPlayer     reset       a new game: no holdings
//   deserialize     loaded      the renown mirror (lordOf, liege, realmOf) put back in step with the block
#pragma once
#include <string>
#include "rpg/sim/game.h"
#include "rpg/sim/lordship.h"
#include "rpg/sim/rise.h"

struct LordOps {
  // ---- the hooks
  static void step(Game& g, float dt);
  static void talk(Game& g, Actor& a);
  static bool choose(Game& g, const DlgOpt& o);
  static void questDone(Game& g, const Quest& q);
  static void reset(Game& g);
  static void loaded(Game& g);

  // ---- phase A's working core (the LORD lane builds on it)
  // the player becomes lord of World::sites[site] (a settlement) under `liege` (0: its current owner), how = the path
  // (false + why: not a settlement, besieged, a town or city (villages only until the crown), already held)
  static bool grant(Game& g, int site, lord::How how, ew::Gid liege, std::string& why);
  // raise the player's banner: a realm of their own from every holding (false + why: no holding, already independent)
  static bool independence(Game& g, const std::string& name, std::string& why);
  static int collect(Game& g);                 // the day's taxes of every holding into the coffers (gold added)
  static bool payTribute(Game& g);             // pay what is due from the coffers (false: not enough: missed)
  static int take(Game& g, int gold);          // gold from the coffers into the player's purse (gold moved)
  static int heldSite(const Game& g);          // the World::sites index of the holding the player stands in (-1)
  static void sync(Game& g);                   // Realm::renown().lordOf / liege / realmOf from the block
  // ---- Game's private doors for the LORD lane's own files
  static void tell(Game& g, const std::string& s) { g.say(s); }
  static void rise(Game& g, RiseEv k, float f, const std::string& s, ew::Gid kingdom = 0);   // Ev::Rise for the view
  static void snd(Game& g, int s, Vec2 p, float pitch = 1, float vol = 1) { g.sfx(s, p, pitch, vol); }
  static int findActor(const Game& g, int id) { return g.findActor(id); }
  static Rng& rng(Game& g) { return g.rng_; }

  // ---- LORD lane (phase B)
  // the four paths (lord_game.cpp): RESETTLED: refugees led home to their burned / abandoned village (Realm::forceResettle
  // then grant How::Resettled; false + why); CONQUERED: the player's realm marches on a settlement (Realm::forceSiege with
  // the player's realm as attacker, the M4 assault; the step adopts it when the realm says it fell: How::Conquered)
  static bool resettle(Game& g, int site, std::string& why);
  static bool march(Game& g, ew::Gid site, std::string& why);   // a settlement the realm knows (loaded or not)
  // the hall (lord_game.cpp): works (gold from the coffers, then the purse), the tax, the levy, a feast, the tribute
  static bool startWork(Game& g, lord::Holding& h, uint16_t bit, std::string& why);
  static void setTax(Game& g, lord::Holding& h, uint8_t pct);
  static bool muster(Game& g, lord::Holding& h, std::string& why);
  static bool feast(Game& g, lord::Holding& h, std::string& why);
  static void withhold(Game& g, bool on);                 // the next tribute is withheld (refuseTribute on its day)
  static bool acceptPeace(Game& g);                       // the old liege's offer taken (Realm::forcePeace)
  static int nearestHolding(const Game& g);               // index into holdings nearest the player (-1 none)
  static int siteOf(const Game& g, const lord::Holding& h) { return g.world.siteHandle(h.site); }
  // the day (lord_game.cpp): loyalty, unrest, famine, raids, works finished, weekly tax lines, tasks (lord_tasks.cpp)
  static void daily(Game& g);
  static void adopt(Game& g);                             // settlements the player's realm holds that are not yet holdings
  static void crownCheck(Game& g);                        // KING / QUEEN at lord::CROWN_HOLDINGS (RiseEv::Crowned)
  // the liege's anger and the war for independence (lord_war.cpp)
  static void wrathStep(Game& g);
  static void refused(Game& g);                           // a tribute withheld or missed: the liege's anger climbs
  static bool forceWrathSiege(Game& g);                   // (tests, scripts) the liege's siege of the seat now
  static void releaseService(Game& g);                    // sworn to the liege's army: released (or a deserter) first
  static std::string freeCost(const Game& g);             // what raising the banner will cost (the confirm line)
  // fief tasks (lord_tasks.cpp): QType::Fief quests from the village's needs
  static uint32_t giveTask(Game& g, lord::Holding& h, lord::Task t);
  static void tasksStep(Game& g);
  static void finishTask(Game& g, uint32_t id);
  // the village's works and people stamped into the window (lord_works.cpp, the M7 home's pattern): the banner, the
  // palisade, the watchtower, the granary, the militia yard, the market cross, the shrine, the hall's banners, the
  // walls; scaffolds while a work goes up; the steward and the militiamen
  static void stampStep(Game& g);
  static void restamp(Game& g);                           // the next stampStep re-lays the works (a work done or lost)
  static void forgetStamps(Game& g);                      // a new game / a load: the window is new, nothing to take up
  struct WorkTile { int32_t gx, gy; uint8_t prop; uint16_t work; };   // a stamped prop (global tile, art::Prop + 1)
  static std::vector<WorkTile> stamped(const Game& g);    // what the window holds now (tests, scripts)
  static int stewardActor(const Game& g, const lord::Holding& h);   // actors index (-1 none)
  // the talk pages (lord_talk.cpp)
  static void stewardTalk(Game& g, Actor& a, lord::Holding& h);
  static void rulerTalk(Game& g, Actor& a, int si);
  static void refugeeTalk(Game& g, Actor& a);
  static bool talkChoose(Game& g, const DlgOpt& o);
  // ---- Game's private doors (phase B)
  static void talkTo(Game& g, Actor& a) { g.talkTo(a); }   // (tests, scripts) open a conversation
  static int nextId(Game& g) { return g.nextId_++; }
  static void makeLook(Game& g, Actor& a, Role r, Rng& rr) { g.makeLook(a, r, rr); }
  static Vec2 freeSpot(const Game& g, int tx, int ty) { return g.freeSpot(tx, ty); }
  static void acceptQuest(Game& g, const Quest& q) { g.acceptQuest(q); }
  static void completeQuest(Game& g, Quest& q) { g.completeQuest(q); }
};
