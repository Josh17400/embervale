// P1 "Fresh Roads" (VISION_PLAN 15.34): the job pools' and leads' Game hooks (Game's internals are private; JobOps is
// its friend, game.h). JOBS lane (it may ADD functions; the hooks below are called from where phase A put them and keep
// their meaning). Defined in rpg/sim/jobs.cpp.
//
// Where each hook is called (phase A wired them all):
//   update          step        after ServiceOps::step: leads going stale, last week's pools dropped
//   acceptQuest     accepted    a quest taken: the pool's job given out, the leads that named it taken
//   completeQuest   questDone   a quest done: the pool's job done
//   resetPlayer     reset       a new game: no pools, no leads
//   deserialize     loaded      after the jobs block is read
#pragma once
#include "rpg/sim/game.h"
#include "rpg/sim/jobs.h"

struct JobOps {
  static void step(Game& g, float dt);
  static void accepted(Game& g, const Quest& q);
  static void questDone(Game& g, const Quest& q);
  static void taleStarted(Game& g, uint64_t who);   // (P1 fixer) a teller's story begun: their TALE lead taken
  static void reset(Game& g);
  static void loaded(Game& g);
  // ---- Game's private doors for the JOBS lane's own files
  static void tell(Game& g, const std::string& s) { g.say(s); }
  static bool hasOffer(const Game& g, const Actor& a) { return g.hasOffer(a); }
  static Quest makeOffer(Game& g, const Actor& a) { return g.makeOffer(a); }   // (tests: rpg_test --jobs)

  // ---- (JOBS lane) the weekly pools (rpg/sim/jobs_pool.cpp)
  struct Needs {   // what the land round a settlement needs (the region plans within 768 tiles, the realm)
    int caves = 0, ruins = 0, camps = 0, dens = 0, nearDens = 0, nearCamps = 0, dests = 0;
    bool famine = false, war = false;
  };
  static Needs scanNeeds(Game& g, const Site& S);
  static jobs::Pool* ensurePool(Game& g, int site);                      // this week's pool of a settlement (worked out once)
  static const jobs::PoolJob* openJob(const Game& g, const Actor& a);    // a's open job this week (works the pool out)
  static const jobs::PoolJob* openJobByKey(const Game& g, uint64_t key); // by giver key (no pool is worked out)
  static void refreshLive(const Game& g);                                // Book::live, when a pool changed or the week turned
  static uint64_t giverKey(const Game& g, const Actor& a);               // a's census identity (any of its bodies)
  static uint64_t questGiverKey(const Game& g, const Quest& q);          // the same for a quest's giver
  static life::Job censusJob(const Game& g, const Actor& a);             // a's trade in the census (None: not a resident)
  static ew::Good tradeGood(Role role, life::Job job, uint8_t need);     // the good a trade's Supply job asks for (COUNT none)
  // ---- leads
  static uint32_t rumourJob(Game& g, int fromSite);   // a rumour naming an open job in another loaded settlement (0 none)
  static uint32_t rumourFar(Game& g, int fromSite);   // ... in a settlement not loaded (its person found when its pool is worked out)
  static bool leadNames(const Game& g, const Actor& a);   // an open lead names this person (the subtle marker)
  static uint32_t placeLead(Game& g, int site, const std::string& text);   // a rumoured place (wayside.cpp)
  static bool waysideFree(const Game& g, uint64_t rawKey);   // a giver outside the settlements has not given a job this week
};
