// M10 "Beyond the Mist": the sea's Game hooks (Game's internals are private; SeaOps is its friend, game.h). SEA lane
// (it may ADD functions; the hooks below are called from where phase A put them and keep their meaning). Defined in
// rpg/sim/sea_game.cpp (phase A: boarding, sailing with the wind, mooring, the Mist's turn-back, the saved boat
// position) and the SEA lane's own files (sea_*.cpp: storms, ferries, encounters, the chart chain, first contact,
// fishing, the swimmers' AI, harbourmasters and shipwrights).
//
// Where each hook is called (phase A wired them all):
//   updatePlayer  input        first thing after the regen / burn block: true = the step is the sea's (aboard, fishing,
//                              swimming), updatePlayer returns
//   moveActor     passable     the player only, on a tile the land rules call solid: true = the hero may enter it
//                              (swimming in the shallows; 15.1 the Sailor swims farther). Actor::swim actors (a boat
//                              aboard, the sea's creatures) use Game::swimSolid instead.
//   update        step         after MagicOps::step: the wind, storms, the Mist, the boats, voyages, encounters
//   updateAI      aiOverride   after MagicOps::aiOverride: the swimmers (serpent, kraken, merrow), pirates afloat
//   interact      interact     after homeInteract (people first): board / disembark / cast a line / a pier's use
//   talkTo        talk         the service hook: harbourmasters, shipwrights, sailors (DLG_SEA options)
//   dialogueChoose choose      DLG_SEA .. DLG_SEA_END
//   shopStock     shopStock    a fishmonger's catch, a harbour chandler's bait and charts
//   windowMoved   shift        storms and the line's bob move with the window
//   clearNonPlayer clearMap    a map change: the line reeled in, encounters dropped (the boat aboard stays)
//   resetPlayer   reset        a new game: no boats, nothing known
//   finishCreator background   15.1 the Sailor's hook (a skiff of their own at the start village's water, or an early
//                              sale: the SEA lane decides)
//   deserialize   loaded       the boat aboard put back under the hero (Actor::swim, radius), boats re-anchored
//   serialize     saving       (on a copy) the boat aboard takes the hero's global tile and heading
// The HUD asks interactLabel for the use button's verb ("BOARD", "DISEMBARK", "FISH", "REEL"...: the same decision
// interact makes) and reads Game::sea for the sail / fishing panels (rpg/view, ART lane).
#pragma once
#include <string>
#include "rpg/sim/game.h"
#include "rpg/sim/sea.h"
#include "rpg/world/source.h"

struct SeaOps {
  // ---- the hooks
  static bool input(Game& g, const Input& in, float dt);
  static bool passable(const Game& g, const Actor& a, int tx, int ty);
  static void step(Game& g, float dt);
  static bool aiOverride(Game& g, Actor& a, float dt);
  static bool interact(Game& g);
  static std::string interactLabel(const Game& g);
  static void talk(Game& g, Actor& a);
  static bool choose(Game& g, const DlgOpt& o);
  static void shopStock(Game& g, const Actor& a, std::vector<Item>& stock);
  static void shift(Game& g, Vec2 d);
  static void clearMap(Game& g);
  static void reset(Game& g);
  static void background(Game& g);
  static void loaded(Game& g);
  static void saving(const Game& g, sea::Sea& out);

  // ---- phase A's working core (the SEA lane builds on it)
  // the hero's global tile now (the window origin plus the body's tile)
  static int32_t heroGx(const Game& g);
  static int32_t heroGy(const Game& g);
  // the sea a hull may sail on a tile of the current map: its ground (Water: shallow, DeepWater: deep; Bridge, land and
  // props: no), the storm sea from the world (ew::EndlessSource::seaAt) where the ground is deep
  static bool sailable(const Game& g, sea::Hull h, int tx, int ty);
  // give the player a boat lying on the nearest water this hull may sail within `radius` tiles of the hero, or of the
  // GLOBAL tile (gx, gy) when given (scripts, tests, a purchase); its boats index, -1 when there is no water near
  static int giveBoat(Game& g, sea::Hull h, int radius = 8, const std::string& name = "", const ew::GTile* at = nullptr);
  static int boatNear(const Game& g, float px = 40.0f);   // a moored boat of ours within px of the hero (-1 none)
  static bool board(Game& g, int boat);                   // step aboard (true: aboard)
  static bool disembark(Game& g);                         // step ashore onto free land within 2 tiles (true: ashore)
  static bool landing(const Game& g, Vec2& out);          // the free land spot nearest the hero within 2 tiles
  // the boat aboard's body (window px) and its hull info; nullptr ashore
  static const sea::Boat* aboardBoat(const Game& g);
  // ---- Game's private doors for the SEA lane's own files
  static void ev(Game& g, sea::SeaEv k, Vec2 p, float f = 1, uint32_t id = 0, const std::string& s = "", Vec2 q = Vec2());
  static void tell(Game& g, const std::string& s) { g.say(s); }
  static void snd(Game& g, int s, Vec2 p, float pitch = 1, float vol = 1) { g.sfx(s, p, pitch, vol); }
  static void move(Game& g, Actor& a, Vec2 d) { g.moveActor(a, d); }
  static int findActor(const Game& g, int id) { return g.findActor(id); }
  static Rng& rng(Game& g) { return g.rng_; }
  static void hit(Game& g, Actor& v, float dmg, Vec2 from, int attacker) { g.damage(v, dmg, from, attacker, Ench::None, 0, false); }
  static int spawnMonster(Game& g, art::Monster m, Vec2 at, int level) { return g.spawnMonster(m, at, level, false); }

  static void emitEv(Game& g, Ev t, Vec2 p, int a = 0, float f = 1, const std::string& s = "") { g.emit(t, p, a, f, s); }
  // ---- SEA lane additions (phase B). Each lives in the file named.
  // hooks added in phase B (called from game_rpg.cpp)
  static void questDone(Game& g, const Quest& q);            // completeQuest: a hidden kingdom's people warm to the hero
  // sea_game.cpp: the steering and the core
  static bool moorStart(Game& g);                            // aboard near a berth: glide into it (true: begun)
  static void sailStep(Game& g, const Input& in, float dt);  // the boat aboard this step (stick, autopilot, mooring)
  static float autopilotHeading(Game& g, float want);        // the course to hold for a wished heading (tacks upwind)
  static void portScan(Game& g);                             // the nearest port (Live::near*) and ports known
  // sea_sail.cpp: the sea's moods, the hull's fate, swimming
  static bool stormSea(const Game& g, int tx, int ty);       // the storm sea under a window tile (cached per tile)
  static void stormStep(Game& g, float dt);                  // weather fronts over the sea (push, rough water, lightning)
  static void beginStorm(Game& g, Vec2 c, float power, float life);   // a front coming in at window px c
  static void hurtBoat(Game& g, float dmg, Vec2 at);         // the hull aboard takes a blow (it may sink)
  static void sinkStep(Game& g, float dt);                   // a boat going down: the fade, then ashore
  static bool swimInput(Game& g, const Input& in, float dt); // the hero in the water ashore (true: the step is the swim's)
  static bool coastNear(const Game& g, Vec2 from, int radius, Vec2& out);   // the nearest free land (window px)
  // sea_fish.cpp: fishing (15.4)
  static bool fishSpot(const Game& g, Vec2& bob, uint8_t& water);   // facing water a line can reach (bob: window px)
  static bool castLine(Game& g);                             // begin a cast (true: begun)
  static bool fishInput(Game& g, const Input& in, float dt); // the line's step (true: the step is the line's)
  static uint8_t watersAt(const Game& g, int tx, int ty);    // FW_* of a water tile
  static Item fishItem(int species, int cm);                 // a catch as food (Ingr::Fish when cooked)
  // sea_trade.cpp: harbours, boats for sale, the yard, ferries, the dock folk, the Sailor
  static void tradeStep(Game& g, float dt);                  // ports known, the dock folk near the hero
  static void voyageStep(Game& g, float dt);                 // a ferry passage under way
  static bool beginVoyage(Game& g, ew::Gid to, int32_t toX, int32_t toY, int gold, int tiles);   // aboard the ferry
  static int buyBoat(Game& g, sea::Hull h, ew::Gid port);    // a new boat at this port's berth (index, -1)
  // sea_chain.cpp: the sea-chart chain, first contact, isolation, the sea's quests
  static void chainStep(Game& g, float dt);
  static void isolationSync(Game& g);                        // kingdoms of lands landed on are no longer isolated
  static void firstContact(Game& g, uint32_t land);          // the hero set foot on a hidden land
  static bool chainPlaces(Game& g, uint32_t land, ew::SitePlan& wreck, ew::SitePlan& cave);   // the world's, else ours
  static void tellOfLand(Game& g, uint32_t land, const std::string& teller);   // chain step 1 (an old sailor's tale)
  static int seaQuest(Game& g, int kind, uint32_t land, const std::string& title, int32_t tgx, int32_t tgy, ew::Gid target);
  static void seaQuestStep(Game& g, int kind, uint32_t land, int32_t tgx, int32_t tgy, const std::string& line, bool done);
  // sea_foes.cpp: the sea's creatures and pirates
  static void encounterStep(Game& g, float dt);
  static int spawnSwimmer(Game& g, art::Monster m, Vec2 at, int level, uint8_t kind);   // actor id (-1)
  static void spawnPirates(Game& g, Vec2 near, int level);
  static void voyageEvent(Game& g, int kind);                // a ferry's trouble: 1 pirates, 2 a storm, 3 a serpent
  static int foesNear(const Game& g, float px);              // hostiles of the sea within px of the hero
  static void dropSwimmers(Game& g);                         // the sea's creatures and ships out of play
};
