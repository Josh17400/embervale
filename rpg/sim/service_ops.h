// M12 "Rise and Fall": the service's Game hooks (Game's internals are private; ServiceOps is its friend, game.h). SERVICE
// lane (it may ADD functions; the hooks below are called from where phase A put them and keep their meaning). Defined in
// rpg/sim/service_game.cpp (phase A: enlisting, ranks, merit, the weekly wage, discharge and desertion, forced orders
// and missions mirrored as quests) and the SERVICE lane's own files (service_*.cpp: orders generated from the sworn
// kingdom's sieges and wars, the squad that marches with a sergeant or captain, diplomacy missions to other courts,
// the marshal's council).
//
// Where each hook is called (phase A wired them all):
//   update          step        after SeaOps::step (and LordOps::step): the day's wage, orders due, missions arrived
//   talkTo          talk        the service hook (P_SERVICE): captains and guards of a kingdom (ENLIST, REPORT,
//                               ORDERS, LEAVE THE ARMY), envoys and rulers (missions); options in [DLG_SERVICE, _END)
//   dialogueChoose  choose      DLG_SERVICE .. DLG_SERVICE_END
//   killed          kill        after warKill: an enemy of the sworn kingdom slain (merit, battles)
//   completeQuest   questDone   a Campaign / Envoy quest done (merit, the realm's roll)
//   resetPlayer     reset       a new game: never enlisted
//   deserialize     loaded      the renown mirror (rank, sworn) put back in step with the block
#pragma once
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/rise.h"
#include "rpg/sim/service.h"

struct ServiceOps {
  // ---- the hooks
  static void step(Game& g, float dt);
  static void talk(Game& g, Actor& a);
  static bool choose(Game& g, const DlgOpt& o);
  static void kill(Game& g, const Actor& victim, int killer);
  static void questDone(Game& g, const Quest& q);
  static void reset(Game& g);
  static void loaded(Game& g);

  // ---- phase A's working core (the SERVICE lane builds on it)
  // swear to a kingdom's army as a Recruit (false + why: a deserter of it, at war with the player's realm, hostile
  // standing (rep < -10), already sworn elsewhere)
  static bool enlist(Game& g, ew::Gid kingdom, std::string& why);
  static void addMerit(Game& g, int merit);   // rises a rank when the merit reaches meritToRise (Promoted)
  static bool promote(Game& g);               // the next rank now (scripts, tests; false at Marshal or not enlisted)
  static void leave(Game& g, bool desert);    // discharged honourably (rank Captain+ or 60 days served) or deserted
  // an order given now (scripts, tests; the SERVICE lane's generator calls it too): its Quest is added (QType::Campaign)
  static uint32_t giveOrder(Game& g, service::OrderKind k, ew::Gid site, int32_t gx, int32_t gy, uint32_t war, uint32_t siege);
  static void finishOrder(Game& g, uint32_t id, bool done);
  // a diplomacy mission given now (QType::Envoy); arrive() rolls the realm's terms at the receiving court
  static uint32_t giveMission(Game& g, service::MissionKind k, ew::Gid from, ew::Gid to);
  static bool arrive(Game& g, uint32_t mission);   // true: the court accepted
  static void sync(Game& g);                       // Realm::renown().rank / sworn from the block
  // ---- Game's private doors for the SERVICE lane's own files
  static void tell(Game& g, const std::string& s) { g.say(s); }
  static void rise(Game& g, RiseEv k, float f, const std::string& s, ew::Gid kingdom = 0);   // Ev::Rise for the view
  static void snd(Game& g, int s, Vec2 p, float pitch = 1, float vol = 1) { g.sfx(s, p, pitch, vol); }
  static int findActor(const Game& g, int id) { return g.findActor(id); }
  static Rng& rng(Game& g) { return g.rng_; }

  // ---- SERVICE lane (phase B) additions
  // the opening of a conversation exactly as a tap on a person makes it (tests drive enlisting through the dialogue)
  static void talkTo(Game& g, Actor& a) { g.talkTo(a); }
  // the open order (nullptr none) and the one mission still to carry (nullptr none)
  static service::Order* openOrder(Game& g);
  static service::Mission* openMission(Game& g);
  // orders (rpg/sim/service_orders.cpp): the daily generator from the sworn kingdom's sieges and wars, each kind played
  // out with actors near the player, the squad, the town captains of the watch who take recruits
  static void ordersDaily(Game& g);                 // a new day: generate, fail what is due, the siege boost's end
  static void ordersStep(Game& g, float dt);        // progress, the squad, the order's own enemies and people
  static void captainsStep(Game& g, float dt);      // a kingdom's towns and cities keep a captain of the watch
  static uint32_t generateOrder(Game& g);           // the next order from the sworn kingdom's wars (0: nothing to do)
  // the sieges a Marshal may choose to press (sieges of the sworn kingdom, attacking or defending; nearest first)
  static std::vector<uint32_t> councilSieges(const Game& g);
  static uint32_t pressSiege(Game& g, uint32_t siege);   // a Marshal's choice: the order for that siege
  static void orderKill(Game& g, const Actor& victim, int killer);   // an order's kill (the player's or the squad's)
  static bool squadMember(const Game& g, int actorId);
  static int squadAlive(const Game& g);             // squad men in play and standing
  static void sendSquadHome(Game& g);
  static bool isServiceCaptain(const Actor& a);     // a town's captain of the watch (spawned by this lane)
  static std::string orderLine(const Game& g, const service::Order& o);   // what the order asks now (the journal)
  // missions (rpg/sim/service_missions.cpp): what a court would ask of the player now, and the realm's roll
  static float missionBonus(const Game& g);         // the envoy's weight at a court: rank and fame (0..1)
  static void missionOffers(Game& g, Actor& ruler, ew::Gid court);   // a ruler's options (DLG_SERVICE + K_MISSION...)
  static bool missionChoose(Game& g, int act, int arg);
  static void missionsDaily(Game& g);               // missions past their due day fail
  // a culture's voice for the army's words (cult::Archetype of the kingdom; 2 Heartland when unknown)
  static int archetypeOf(const Game& g, ew::Gid kingdom);
  static std::string kingdomName(const Game& g, ew::Gid kingdom);
  static ew::Gid kingdomOf(const Game& g, const Actor& a);
  // a settlement's name by id, loaded or not ("" unknown), and its heart (global tile; false unknown)
  static std::string siteName(Game& g, ew::Gid site);
  static bool siteHeart(const Game& g, ew::Gid site, int32_t& gx, int32_t& gy);
  // a runtime soldier (squad, an order's enemies) or a refugee, like the war's camp people (Faction::Army; refugees Town)
  static int spawnMan(Game& g, Role r, ew::Gid kingdom, Vec2 at, int slot, int level);
};
