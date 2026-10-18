// P1b "New Roads Content" (lead, phase A; VISION_PLAN 15.35): RADIANT JOBS RUN ON THE STORY ENGINE.
//
// The 15 new job kinds (THE ROAD WARDEN, THE FENCE'S CELLAR, ... BEACON CHAIN) are not new QTypes with hand-written
// state machines: each kind is a DSL TEMPLATE (saga.h markup: [[A|B|C]] alternatives, ?flag / !flag lines) that the JOBS
// lane's composer turns into an ordinary dsl::Script for one offer (the giver's trade, the season, war, the port, the
// job's dice), named by a spec id the script is regenerated from, exactly as sagas are (never saved: a running job's save
// record is the M4 Instance). So every job gets talk stages with choices (SITE-TALK, TURN-OPTS), goals, clocks, escorts,
// bouts, census effects and rewards from the one engine, the validator checks every composed line, and rpg_test
// --content-p1b walks every branch of every kind headlessly.
//
// Ownership: the lead wrote this header (the contract); the JOBS lane owns rpg/story/radiant.cpp and
// rpg/story/jobs/*.cpp (the 15 templates) and may ADD to this header, never rename or change what is listed here.
//
// The spec id: "job<RADIANT_GEN_VER>~<kind id>~<flags hex>~<seed hex>" (e.g. "job1~roadwarden~0005~1a2b3c4d"); a saved job
// of another version is "forgotten" (its journal entry closes; story.cpp deserialize already does this for any script
// it cannot resolve). Offering: the weekly pool (rpg/sim/jobs_pool.cpp) holds a radiant job as a PoolJob with qtype
// QType::Story and need = JN_RADIANT0 + kind (jobs.h, JOBS lane); "ANY WORK GOING?" with such a job starts the composed
// script on the giver (Engine::start(g, id, giverActorId, site)): its opening talk stage is the pitch with ACCEPT /
// DECLINE options, a decline closes it quietly (the M6b "_quiet" opening). JobOps::step marks the pool job done when the
// instance ends.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

class Game;
struct Actor;

namespace story {
namespace dsl { struct Script; }
namespace radiant {

constexpr int RADIANT_GEN_VER = 1;

// "job<digit>..." (cheap: scriptById asks it for every id that is not the library's or a saga's)
inline bool isJobId(const std::string& id) { return id.size() > 4 && id.compare(0, 3, "job") == 0 && id[3] >= '0' && id[3] <= '9'; }
// the composed script of a job spec id (cached; nullptr: malformed, another version, an unknown kind)
const dsl::Script* script(const std::string& id);
// the kinds: count, their ids ("roadwarden") and titles ("THE ROAD WARDEN"), the kind of a spec id (-1: not a job id)
int kindCount();
const char* kindId(int kind);
const char* kindTitle(int kind);
int kindOf(const std::string& id);
// the radiant job person `a` offers now ("" none): the weekly pool's radiant job for them, composed and cast-checked
// (rpg_test --quest-mix counts these as job offers by kind)
std::string offerFor(Game& g, const Actor& a);
// (tests) a job of kind `kind` composed for a giver (`giver` may be null: the hook settlement's first fitting trade)
// at settlement `hookSite` with dice `seed`; "" when the kind cannot be composed there
std::string composeFor(Game& g, int kind, const Actor* giver, int hookSite, uint64_t seed);
// (tests) every variant flag combination worth walking for a kind (each a spec id composed with `seed`): the
// --content-p1b walker plays each through every branch
int variantCount(int kind);
std::string composeVariant(Game& g, int kind, int variant, int hookSite, uint64_t seed);

// ---------------------------------------------------------------------------------------------------------------------
// (JOBS lane, added) THE COMPOSER'S KINDS. Each kind (rpg/story/jobs/<kind>.cpp) is a DSL template in the saga markup
// (saga.h: [[A|B]], [[name=A|B]] / [[=name]], [[~name|A|B]], ?flag / !flag lines) from `title` on, whose first roles
// are `role giver giver` and `role home site home`. The composer prepends `script <spec id>`, `archetype <kind title>`,
// `tier 2`, `family job`, `hook npc <the giver's trade>` and a pitch, expands it with the job's seed and these FLAGS:
//   t_<trade>    the giver's trade (t_merchant, t_innkeeper, t_jarl, t_guard, t_priest, t_smith, t_farmer, t_mage,
//                t_hunter, t_fisher, t_villager)
//   s_<season>   s_spring s_summer s_autumn s_winter
//   war / peace  the hook settlement's kingdom is at war or not
//   port, coast  the hook settlement is a harbour town / the sea lies within reach
//   danger       the hook's danger is beyond the player (site level >= player level + 3)
//   fb           a mechanic the kind prefers is not cast in this world (the kind's fallback: e.g. no watchtowers)
//   v_<name>     the variant (KindDef::variants), tw_<name> the twist (KindDef::twists; tw_none), x_<name> the extra
//   village / town / city   the hook settlement's size (a capital is a city)
// and the named picks its prep() made (cargo words, marks, names of things), then parses, links and validates it.
// Before the saga markup the composer keeps or drops BLOCKS: a line "{?flag" ("{!flag", "{?a|b": any of them) opens a
// block kept only when the condition holds, a line "}" closes it; blocks nest (a twist's stages, a variant's roles).
// A job's flags are chosen by the world when it is offered (choose()); the walker enumerates KindDef::walks.
struct JobCtx {
  int trade = 0;                   // the giver's Role
  int season = 0;                  // home::Season
  bool war = false, port = false, coast = false, danger = false, fallback = false;
  int tier = 0;                    // 0 village, 1 town, 2 city
  int variant = 0, twist = 0, extra = 0;
};
// the flags word inside a spec id (8 hex digits) and back
uint32_t packCtx(const JobCtx& c);
JobCtx unpackCtx(uint32_t f);
struct KindDef {
  const char* id = "";             // "roadwarden"
  const char* title = "";          // "THE ROAD WARDEN" (reports; the archetype line)
  const char* lead = "";           // what a rumour lead says the giver wants ("NEEDS A WARDEN FOR A WAGON")
  const char* body = "";           // the template
  std::vector<const char*> variants, twists, extras;   // flag names (index = JobCtx field)
  std::vector<int> trades;         // the census trades (Role) that may offer it: the weekly pools' menus
  bool fallback = false;           // the kind has a fallback (fb): composeFor tries it when the preferred cast fails
  // the offer: may a giver of `w.trade` in the world `w` offer it, and as what (variant, twist, extra); dice decide
  // between the fits. False: not offered by this giver here.
  bool (*choose)(const JobCtx& w, uint64_t dice, JobCtx& out) = nullptr;
  // the picks of one composition (pure in the flags and the seed): name -> the words [[=name]] repeats
  void (*prep)(const JobCtx& c, uint64_t seed, std::map<std::string, std::string>& picks) = nullptr;
  std::vector<JobCtx> walks;       // (tests) every combination worth walking
};
const std::vector<const KindDef*>& kinds();
// the kinds' dice (deterministic integer draws: prep and choose)
struct Dice {
  uint64_t s = 0;
  explicit Dice(uint64_t seed) : s(seed) {}
  uint64_t next() { s += 0x9E3779B97F4A7C15ull; uint64_t z = s; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return z ^ (z >> 31); }
  int below(int n) { return n <= 1 ? 0 : (int)(next() % (uint64_t)n); }
  bool chance(int pct) { return below(100) < pct; }
  std::string pick(const std::vector<std::string>& v) { return v.empty() ? std::string() : v[(size_t)below((int)v.size())]; }
};
// (tests, reports) the expansion / parse / validation problems of a spec id (empty: it composes)
std::vector<std::string> composeErrors(const std::string& id);
// (tests) the composed DSL text of a spec id
std::string composedText(const std::string& id);
// the spec id of a kind with these flags and seed
std::string specIdOf(int kind, const JobCtx& c, uint32_t seed);
// the kinds' files (rpg/story/jobs/*.cpp)
const KindDef& kindRoadWarden();
const KindDef& kindFence();
const KindDef& kindRunDown();
const KindDef& kindSurvey();
const KindDef& kindHomeward();
const KindDef& kindHaunt();
const KindDef& kindRemedy();
const KindDef& kindSmuggle();
const KindDef& kindRansom();
const KindDef& kindRounds();
const KindDef& kindChampion();
const KindDef& kindBounds();
const KindDef& kindMend();
const KindDef& kindFeast();
const KindDef& kindBeacon();

}  // namespace radiant
}  // namespace story
