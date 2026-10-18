// P1 "Fresh Roads" (VISION_PLAN 15.34), JOBS lane: the settlements' weekly job pools.
//
// A settlement's pool is worked out the first time anyone there is asked for work in a week (Game::hasOffer, the
// notice board, a rumour naming a job there) and frozen in Game::jobBook for the week (saved). Its jobs come from the
// settlement's real needs, read from the region plans and the realm, never from a dice roll over a fixed menu:
//   dens within a morning's walk, beasts on the hunting grounds        Hunt (a den or a ground: the quest's target)
//   bandit camps within reach                                          Bounty, NamedBandit (and deserters in a war)
//   caves and ruins within reach                                       Clear, Heirloom, Missing
//   settlements 150-700 tiles away on the same land                    Deliver
//   the census trades' goods, sold by a loaded neighbour               Supply (life_quests.cpp jobsTradeSupply)
//   a den or a camp near the fields of a village or town               Protect
//   a famine (the realm's SS_FAMINE)                                   Supply of food
// Size by settlement: 3-5 jobs a week in a village, 6-10 in a town, 10-16 in a city (fewer when fewer people can ask),
// plus the notice board's two posted jobs in towns and cities. Each job goes to a resident whose trade has it on their
// menu (every trade has 4 or more kinds), the trade drawn first (a farmer weighs less than a specialist, and no more
// than a third of a pool goes to farmers), then the person who gave least lately (last week's givers wait a week).
// An open lead naming a resident of the settlement keeps its promise: their job is in the new week's pool too.
//
// hasOffer runs every frame (the talk prompt, the markers), so the pools are read through Book::live (giver key ->
// job), rebuilt only when a pool changes or the week turns; an actor's census identity is cached in Book::canon.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "rpg/sim/game_internal.h"
#include "rpg/sim/home.h"
#include "rpg/sim/jobs_ops.h"
#include "rpg/story/radiant.h"
#include "rpg/world/source.h"

// life_quests.cpp (JOBS lane): the nearest loaded settlement that sells `good` to site `si` (-1 none)
int jobsSupplyFrom(const Game& g, int si, ew::Good good);

namespace jobs {

const char* jobNeedName(uint8_t n) {
  static const char* t[] = {"NONE", "DEN", "GROUND", "HIDES", "CAMP", "RAIDERS", "CHIEF", "CAVE", "RUIN", "DESECRATED", "RELIC",
                            "HOLYRELIC", "STOLEN", "LOST", "STRAYED", "PARCEL", "TOOLS", "BREAD", "FIELDS", "SUPPLY", "FAMINE", "WARBAND"};
  static_assert(sizeof(t) / sizeof(t[0]) == JN_COUNT, "a name for every need");
  if (n >= JN_RADIANT0 && n < JN_RADIANT_END) return "RADIANT";   // (P1b: story::radiant::kindId names the kind)
  return n < JN_COUNT ? t[n] : "?";
}

}  // namespace jobs

namespace {

using jobs::JobNeed;

struct Kind { QType t; uint8_t need; };

// what a trade asks for (VISION_PLAN 15.34 "wider per-trade menus": 4 kinds or more each)
constexpr int MENU_MAX = 24;   // (P1b: the trade's own kinds and the radiant kinds its trade may offer)
int menuOf(Role role, life::Job job, Kind out[MENU_MAX]) {
  using J = life::Job;
  int n = 0;
  auto add = [&](QType t, uint8_t need) { if (n < MENU_MAX) out[n++] = Kind{t, need}; };
  switch (role) {
    case Role::Jarl:
      add(QType::Clear, jobs::JN_CAVE); add(QType::Clear, jobs::JN_RUIN); add(QType::NamedBandit, jobs::JN_CHIEF);
      add(QType::Bounty, jobs::JN_CAMP); add(QType::Missing, jobs::JN_LOST); add(QType::Bounty, jobs::JN_WARBAND);
      break;
    case Role::Guard:
      add(QType::Bounty, jobs::JN_CAMP); add(QType::NamedBandit, jobs::JN_CHIEF); add(QType::Clear, jobs::JN_CAVE);
      add(QType::Hunt, jobs::JN_DEN); add(QType::Bounty, jobs::JN_WARBAND);
      break;
    case Role::Innkeeper:
      add(QType::Bounty, jobs::JN_CAMP); add(QType::Hunt, jobs::JN_DEN); add(QType::Deliver, jobs::JN_PARCEL);
      add(QType::NamedBandit, jobs::JN_CHIEF); add(QType::Heirloom, jobs::JN_RELIC); add(QType::Supply, jobs::JN_SUPPLY);
      add(QType::Supply, jobs::JN_FAMINE);
      break;
    case Role::Priest:
      add(QType::Missing, jobs::JN_LOST); add(QType::Heirloom, jobs::JN_HOLYRELIC); add(QType::Clear, jobs::JN_DESECRATED);
      add(QType::Deliver, jobs::JN_PARCEL); add(QType::Supply, jobs::JN_FAMINE);
      break;
    case Role::Smith:
      add(QType::Supply, jobs::JN_SUPPLY); add(QType::Deliver, jobs::JN_TOOLS); add(QType::Hunt, jobs::JN_HIDES); add(QType::Clear, jobs::JN_CAVE);
      break;
    case Role::Mage:
      add(QType::Clear, jobs::JN_RUIN); add(QType::Heirloom, jobs::JN_RELIC); add(QType::Deliver, jobs::JN_PARCEL); add(QType::Clear, jobs::JN_CAVE);
      break;
    case Role::Merchant:
      if (job == J::Baker) {
        add(QType::Supply, jobs::JN_SUPPLY); add(QType::Deliver, jobs::JN_BREAD); add(QType::Missing, jobs::JN_LOST); add(QType::Hunt, jobs::JN_GROUND);
      } else if (job == J::Tailor) {
        add(QType::Supply, jobs::JN_SUPPLY); add(QType::Deliver, jobs::JN_PARCEL); add(QType::Hunt, jobs::JN_HIDES); add(QType::Heirloom, jobs::JN_STOLEN);
      } else {
        add(QType::Deliver, jobs::JN_PARCEL); add(QType::Bounty, jobs::JN_RAIDERS); add(QType::Heirloom, jobs::JN_STOLEN); add(QType::Supply, jobs::JN_SUPPLY);
      }
      break;
    case Role::Farmer:
      if (job == J::Miner) {
        add(QType::Clear, jobs::JN_CAVE); add(QType::Supply, jobs::JN_SUPPLY); add(QType::Missing, jobs::JN_LOST); add(QType::Hunt, jobs::JN_DEN);
      } else {
        add(QType::Protect, jobs::JN_FIELDS); add(QType::Hunt, jobs::JN_DEN); add(QType::Supply, jobs::JN_SUPPLY);
        add(QType::Missing, jobs::JN_STRAYED); add(QType::Supply, jobs::JN_FAMINE);
      }
      break;
    case Role::Child: case Role::King: case Role::Bandit:   // (never asked: kept off the radiant menus below too)
      return 0;
    case Role::Hunter:
      add(QType::Hunt, jobs::JN_DEN); add(QType::Hunt, jobs::JN_GROUND); add(QType::Deliver, jobs::JN_PARCEL); add(QType::Missing, jobs::JN_LOST);
      add(QType::Clear, jobs::JN_CAVE);
      break;
    case Role::Fisher:
      add(QType::Hunt, jobs::JN_DEN); add(QType::Deliver, jobs::JN_PARCEL); add(QType::Missing, jobs::JN_LOST); add(QType::Supply, jobs::JN_SUPPLY);
      break;
    case Role::Herbalist:
      add(QType::Heirloom, jobs::JN_RELIC); add(QType::Missing, jobs::JN_LOST); add(QType::Hunt, jobs::JN_DEN); add(QType::Clear, jobs::JN_CAVE);
      break;
    default:   // the villagers: labourers, servers, stablehands, elders, bards...
      if (job == J::Scholar) {   // (P1 fixer) a scholar asks for books and relics, never for wolves culled
        add(QType::Heirloom, jobs::JN_RELIC); add(QType::Clear, jobs::JN_RUIN); add(QType::Missing, jobs::JN_LOST); add(QType::Deliver, jobs::JN_PARCEL);
        break;
      }
      add(QType::Hunt, jobs::JN_DEN); add(QType::Heirloom, jobs::JN_RELIC); add(QType::Missing, jobs::JN_LOST); add(QType::Deliver, jobs::JN_PARCEL);
      add(QType::Protect, jobs::JN_FIELDS);
      break;
  }
  // (P1b) the radiant job kinds this trade may offer (story::radiant::KindDef::trades; the world decides in possible())
  for (int k = 0; k < story::radiant::kindCount(); k++)
    for (int t : story::radiant::kinds()[(size_t)k]->trades)
      if (t == (int)role) { add(QType::Story, (uint8_t)(jobs::JN_RADIANT0 + k)); break; }
  return n;
}

// the good a trade's Supply job asks for (ew::Good::COUNT: none)
ew::Good tradeGood(Role role, life::Job job, uint8_t need) {
  using J = life::Job;
  using G = ew::Good;
  if (need == jobs::JN_FAMINE) return (role == Role::Farmer) ? G::Grain : G::Bread;
  switch (job) {
    case J::Smith: return G::Ore;
    case J::Baker: return G::Flour;
    case J::Tailor: return G::Wool;
    case J::Merchant: return G::Tools;
    case J::Innkeeper: return G::Meat;
    case J::Farmer: case J::Woodcutter: case J::Miner: return G::Tools;
    case J::Herder: return G::Grain;
    case J::Fisher: return G::Planks;
    default: break;
  }
  if (role == Role::Smith) return G::Ore;
  if (role == Role::Innkeeper) return G::Meat;
  if (role == Role::Merchant) return G::Tools;
  return G::COUNT;
}

// the kind's weight in a pool (x10, integers: the pool is saved and must come out the same on every compiler)
int baseWeight(QType t) {
  switch (t) {
    case QType::Hunt: return 21;
    case QType::Deliver: return 18;
    case QType::Supply: return 17;
    case QType::Bounty: case QType::Clear: return 16;
    case QType::Missing: return 14;
    case QType::Heirloom: return 12;
    case QType::Protect: case QType::NamedBandit: return 11;
    default: return 10;
  }
}

// (P1b) may this settlement's giver of trade `role` offer radiant kind `kind` this week? The places the kind's roles need
// lie near (the scan's counts: the caster still decides when it is offered), and the kind takes this giver in this world
// (KindDef::choose). Integer-only and deterministic: the pool is saved.
bool radiantPossible(Game& g, int si, const JobOps::Needs& N, int kind, Role role, int32_t week) {
  using namespace story::radiant;
  if (kind < 0 || kind >= kindCount()) return false;
  const KindDef& K = *kinds()[(size_t)kind];
  const std::string id = K.id;
  bool near = true;
  if (id == "roadwarden" || id == "homeward" || id == "champion") near = N.dests > 0;
  else if (id == "fence" || id == "ransom") near = N.camps > 0;
  else if (id == "rundown" || id == "remedy") near = N.caves + N.ruins > 0;
  else if (id == "survey") near = N.caves > 0 && N.ruins > 0;
  else if (id == "haunt" || id == "beacon") near = N.ruins > 0;
  else if (id == "smuggle") near = N.camps > 0 || N.caves > 0;
  if (!near || !K.choose) return false;
  const Site& S = g.world.sites[(size_t)si];
  JobCtx w;
  w.trade = (int)role;
  w.season = (int)home::seasonOf(g.day);
  w.tier = S.type == SiteType::City ? 2 : S.type == SiteType::Town ? 1 : 0;
  w.port = S.archetype == (uint8_t)ew::Archetype::Port;
  w.coast = w.port;
  w.war = N.war;
  JobCtx out;
  return K.choose(w, ew::mix64(S.id ^ (uint64_t)(uint32_t)week * 0x51ED27ull ^ ((uint64_t)kind << 32) ^ (uint64_t)role), out);
}

// (P1b) a radiant kind's weight in a pool (x10, like baseWeight): fifteen kinds share the ground the old kinds held, so
// each weighs a little under the least of them; the pool's radiant share stays under about half of its jobs
int radiantWeight(int kind, int radiantSoFar, int want) {
  if (radiantSoFar * 2 >= std::max(2, want)) return 0;
  // (measured with rpg_test --quest-mix: the kinds few trades offer, or that need rarer places, weigh a little more so
  // each is met; the kinds most trades offer a little less)
  const std::string id = kind >= 0 && kind < story::radiant::kindCount() ? story::radiant::kindId(kind) : "";
  if (id == "beacon") return 18;
  if (id == "bounds" || id == "survey") return 15;
  if (id == "feast" || id == "homeward" || id == "fence") return 11;
  if (id == "remedy" || id == "ransom" || id == "haunt") return 7;
  return 9;
}

}  // namespace

// ---------------------------------------------------------------- what the land around a settlement needs
JobOps::Needs JobOps::scanNeeds(Game& g, const Site& S) {
  Needs N;
  if (!g.world.endless || !g.world.src) return N;
  const int32_t hx = g.world.ox + S.ex, hy = g.world.oy + S.ey;
  const int rings = 3;   // 768 tiles: the radiant range (quests.cpp kRadiantRange)
  const int32_t rx0 = ew::regionOf(hx), ry0 = ew::regionOf(hy);
  for (int32_t ry = ry0 - rings; ry <= ry0 + rings; ry++)
    for (int32_t rx = rx0 - rings; rx <= rx0 + rings; rx++) {
      const ew::RegionPlan R = g.world.regionPlan(rx, ry);
      for (const ew::SitePlan& p : R.sites) {
        const int64_t dx = p.ex - hx, dy = p.ey - hy, d2 = dx * dx + dy * dy;
        if (p.type == SiteType::Village || p.type == SiteType::Town || p.type == SiteType::City) {
          if (d2 >= 150 * 150 && d2 <= 700 * 700) N.dests++;
          continue;
        }
        if (p.type != SiteType::Cave && p.type != SiteType::Ruin && p.type != SiteType::BanditCamp) continue;
        if ((p.flags & ew::SPF_MAINQUEST) || d2 > 768 * 768) continue;
        const int h = g.world.siteHandle(p.id);
        if (h >= 0 && g.world.sites[(size_t)h].cleared) continue;
        if (p.type == SiteType::Cave) N.caves++;
        else if (p.type == SiteType::Ruin) N.ruins++;
        else { N.camps++; if (d2 <= 300 * 300) N.nearCamps++; }
      }
      for (const ew::DenPlan& d : R.dens) {
        const int64_t dx = d.x - hx, dy = d.y - hy, d2 = dx * dx + dy * dy;
        if (d2 <= 260 * 260) N.dens++;
        if (d2 <= 200 * 200) N.nearDens++;
      }
    }
  if (const realm::SettlementState* ss = g.realm.settlement(S.id)) {
    N.famine = (ss->flags & realm::SS_FAMINE) != 0;
    for (const realm::War& w : g.realm.wars())
      if (w.endDay == 0 && ss->owner && (w.attacker == ss->owner || w.defender == ss->owner)) N.war = true;
  }
  return N;
}

ew::Good JobOps::tradeGood(Role role, life::Job job, uint8_t need) { return ::tradeGood(role, job, need); }

// ---------------------------------------------------------------- identities
uint64_t JobOps::giverKey(const Game& g, const Actor& a) {
  if (a.site < 0 || a.site >= (int)g.world.sites.size()) return g.npcKey(a);
  const uint64_t raw = g.npcKey(a);
  // a body that knows its census identity: still remember raw -> key, so a story cast that holds this body's own
  // key (an innkeeper at the post: npcKeyOf(site, bldg, 0)) maps back to the pool giver (JobOps::radiantStep)
  auto remember = [&](uint64_t key) {
    if (raw != key) {
      if (g.jobBook.canon.size() > 4096) g.jobBook.canon.clear();
      g.jobBook.canon[raw] = key;
    }
    return key;
  };
  if (a.resident >= 0) return remember(g.npcKeyOf(a.site, -1, jobs::RES_SLOT0 + a.resident));
  if (a.altSlot >= jobs::RES_SLOT0 && a.altBldg == -1) return remember(g.npcKeyOf(a.site, -1, a.altSlot));
  if (a.bldg == -1 && (a.slot == jobs::BOARD_SLOT0 || a.slot == jobs::BOARD_SLOT0 + 1)) return raw;   // the notice board
  auto it = g.jobBook.canon.find(raw);
  if (it != g.jobBook.canon.end()) return it->second;
  uint64_t key = raw;
  const Site& S = g.world.sites[(size_t)a.site];
  if (S.settlement()) {
    if (const life::Census* c = g.life.find(S.id)) {
      Spawn sp;
      sp.npc = true; sp.role = a.role; sp.slot = a.slot; sp.site = a.site;
      const int off = a.bldg >= S.bldgFirst && a.bldg < S.bldgFirst + S.bldgCount ? a.bldg - S.bldgFirst : -1;
      const int ri = (a.bldg >= 0 && off < 0) ? -1 : life::spawnResident(*c, sp, off);
      if (ri >= 0) key = g.npcKeyOf(a.site, -1, jobs::RES_SLOT0 + ri);
    }
  }
  if (g.jobBook.canon.size() > 4096) g.jobBook.canon.clear();
  g.jobBook.canon[raw] = key;
  return key;
}

uint64_t JobOps::questGiverKey(const Game& g, const Quest& q) {
  const uint64_t raw = g.npcKeyOf(q.giverSite, q.giverBldg, q.giverSlot);
  auto it = g.jobBook.canon.find(raw);
  return it != g.jobBook.canon.end() ? it->second : raw;
}

life::Job JobOps::censusJob(const Game& g, const Actor& a) {
  if (a.site < 0 || a.site >= (int)g.world.sites.size()) return life::Job::None;
  const Site& S = g.world.sites[(size_t)a.site];
  const life::Census* c = S.settlement() ? g.life.find(S.id) : nullptr;
  if (!c) return life::Job::None;
  const uint64_t key = giverKey(g, a);
  for (const life::Resident& r : c->res)
    if (g.npcKeyOf(a.site, -1, jobs::RES_SLOT0 + r.idx) == key) return r.job;
  return life::Job::None;
}

// ---------------------------------------------------------------- the live index
void JobOps::refreshLive(const Game& g) {
  const int32_t week = g.day / 7;
  if (!g.jobBook.liveDirty && g.jobBook.liveWeek == week) return;
  g.jobBook.live.clear();
  for (size_t pi = 0; pi < g.jobBook.pools.size(); pi++) {
    const jobs::Pool& p = g.jobBook.pools[pi];
    if (p.week != week) continue;
    for (size_t ji = 0; ji < p.jobs.size(); ji++)
      if (p.jobs[ji].state == jobs::PJ_OPEN && !g.jobBook.live.count(p.jobs[ji].giver))
        g.jobBook.live[p.jobs[ji].giver] = (uint32_t)(pi << 16 | ji);
  }
  g.jobBook.liveDirty = false;
  g.jobBook.liveWeek = week;
}

const jobs::PoolJob* JobOps::openJobByKey(const Game& g, uint64_t key) {
  refreshLive(g);
  auto it = g.jobBook.live.find(key);
  if (it == g.jobBook.live.end()) return nullptr;
  const size_t pi = it->second >> 16, ji = it->second & 0xFFFFu;
  if (pi >= g.jobBook.pools.size() || ji >= g.jobBook.pools[pi].jobs.size()) return nullptr;
  return &g.jobBook.pools[pi].jobs[ji];
}

const jobs::PoolJob* JobOps::openJob(const Game& g, const Actor& a) {
  if (a.site < 0 || a.site >= (int)g.world.sites.size()) return nullptr;
  const Site& S = g.world.sites[(size_t)a.site];
  if (!S.settlement()) return nullptr;
  const uint64_t key = giverKey(g, a);
  if (const jobs::PoolJob* j = openJobByKey(g, key)) return j;
  // not a giver this week; the pool may not be worked out yet (the first ask of the week works it out and freezes it;
  // hasOffer is const for its callers, but the Game it is asked of is always a live, mutable one)
  if (const_cast<Game&>(g).jobBook.pool(S.id, g.day / 7)) return nullptr;
  if (!ensurePool(const_cast<Game&>(g), a.site)) return nullptr;
  return openJobByKey(g, key);
}

// ---------------------------------------------------------------- working a pool out
jobs::Pool* JobOps::ensurePool(Game& g, int si) {
  if (si < 0 || si >= (int)g.world.sites.size()) return nullptr;
  const Site S = g.world.sites[(size_t)si];   // (a copy: the scan may load more sites)
  if (!S.settlement() || !S.id) return nullptr;
  const int32_t week = g.day / 7;
  if (jobs::Pool* p = g.jobBook.pool(S.id, week)) return p;
  const life::Census* c = g.life.find(S.id);
  if (!c || c->res.empty()) return nullptr;
  const Needs N = scanNeeds(g, S);
  const bool noble = g.background == Background::Noble;
  Rng r(ew::mix64(g.seed * 0x9E3779B97F4A7C15ull ^ ew::mix64(S.id + 0x7A0Bull) ^ (uint64_t)(uint32_t)week * 0xC2B2AE3D27D4EB4Full));

  // is a kind possible here at all?
  auto supplyOk = [&](Role role, life::Job job, uint8_t need) {
    const ew::Good gd = tradeGood(role, job, need);
    return gd != ew::Good::COUNT && jobsSupplyFrom(g, si, gd) >= 0;
  };
  auto possible = [&](const Kind& k, Role role, life::Job job) {
    switch (k.need) {
      case jobs::JN_DEN: case jobs::JN_GROUND: case jobs::JN_HIDES: return true;   // a ground always lies near
      case jobs::JN_CAMP: case jobs::JN_RAIDERS: case jobs::JN_CHIEF: return N.camps > 0;
      case jobs::JN_WARBAND: return N.war && N.camps > 0;
      case jobs::JN_CAVE: return N.caves > 0;
      case jobs::JN_RUIN: case jobs::JN_DESECRATED: return N.ruins > 0;
      case jobs::JN_RELIC: case jobs::JN_HOLYRELIC: return N.caves + N.ruins > 0;
      case jobs::JN_STOLEN: case jobs::JN_LOST: case jobs::JN_STRAYED: return N.caves > 0;
      case jobs::JN_PARCEL: case jobs::JN_TOOLS: case jobs::JN_BREAD: return N.dests > 0;
      case jobs::JN_FIELDS: return (S.type == SiteType::Village || S.type == SiteType::Town) && (N.nearDens > 0 || N.nearCamps > 0);
      case jobs::JN_SUPPLY: return supplyOk(role, job, k.need);
      case jobs::JN_FAMINE: return N.famine && supplyOk(role, job, k.need);
      default: break;
    }
    // (P1b) a radiant job kind: the places it needs lie near, and the kind will take this giver in this world
    if (k.t == QType::Story && k.need >= jobs::JN_RADIANT0 && k.need < jobs::JN_RADIANT0 + story::radiant::kindCount())
      return radiantPossible(g, si, N, k.need - jobs::JN_RADIANT0, role, week);
    return false;
  };

  // who may ask: the living residents at home, never a child, a guard (unless the exile is a noble) or a beggar
  struct Cand { uint16_t idx; Role role; life::Job job; uint64_t key; int score; Kind menu[MENU_MAX]; int nm; bool used; };
  std::vector<Cand> cands;
  const jobs::Pool* last = g.jobBook.pool(S.id, week - 1);
  for (const life::Resident& rs : c->res) {
    if (rs.flags & (life::RF_DEAD | life::RF_AWAY | life::RF_COMPANION)) continue;
    if (rs.job == life::Job::Child || rs.job == life::Job::Beggar || (rs.job == life::Job::Guard && !noble)) continue;
    Cand cd;
    cd.idx = rs.idx; cd.job = rs.job; cd.role = life::jobRole(rs.job);
    cd.key = g.npcKeyOf(si, -1, jobs::RES_SLOT0 + rs.idx);
    cd.used = false;
    Kind all[MENU_MAX];
    const int na = menuOf(cd.role, cd.job, all);
    cd.nm = 0;
    for (int k = 0; k < na; k++) if (possible(all[k], cd.role, cd.job)) cd.menu[cd.nm++] = all[k];
    if (cd.nm == 0) continue;
    // rotation: the dice of the week, then who gave lately waits (last week's givers, the jobs done for them)
    int sc = (int)(ew::mix64(cd.key ^ (uint64_t)(uint32_t)week * 0x9E37ull) % 1000u);
    if (last) for (const jobs::PoolJob& j : last->jobs) if (j.giver == cd.key) sc += 700;
    auto dn = g.npcQuestsDone.find(cd.key);
    if (dn != g.npcQuestsDone.end()) sc += 120 * std::min(dn->second, 4);
    cd.score = sc;
    cands.push_back(cd);
  }

  jobs::Pool P;
  P.site = S.id;
  P.week = week;
  const int tier = std::min<int>(c->tier, 2);
  int want = tier == 0 ? 3 + r.irange(3) : tier == 1 ? 6 + r.irange(5) : 10 + r.irange(7);
  want = std::min<int>(want, (int)cands.size());
  int farmers = 0, perType[(int)QType::COUNT] = {}, perNeed[64] = {};
  auto give = [&](Cand& cd, const Kind& k) {
    jobs::PoolJob j;
    j.qtype = (uint8_t)k.t; j.need = k.need; j.state = jobs::PJ_OPEN; j.giver = cd.key;
    P.jobs.push_back(j);
    cd.used = true;
    if (cd.role == Role::Farmer) farmers++;
    perType[(int)k.t]++;
    perNeed[k.need & 63]++;
  };
  // 1. an open lead about someone here keeps its promise; a rumour heard far off ("THE INNKEEPER OF VOND PAYS FOR A
  //    HUNT", no person known yet) finds its person now: one of that trade if there is one, else anyone who asks for it
  for (jobs::Lead& l : g.jobBook.leads) {
    if (l.state != jobs::LS_OPEN || l.site != S.id || (int)P.jobs.size() >= want) continue;
    if (l.who) {
      for (Cand& cd : cands) {
        if (cd.used || cd.key != l.who) continue;
        int pickK = -1;
        for (int k = 0; k < cd.nm; k++) if ((uint8_t)cd.menu[k].t == l.qtype) { pickK = k; break; }
        if (pickK < 0 && l.qtype == (uint8_t)QType::COUNT) pickK = 0;
        if (pickK >= 0) give(cd, cd.menu[pickK]);
        break;
      }
      continue;
    }
    if (l.qtype == (uint8_t)QType::COUNT) continue;
    Cand* best = nullptr;
    int bestK = -1;
    bool bestTrade = false;
    for (Cand& cd : cands) {
      if (cd.used) continue;
      int k = -1;
      for (int m = 0; m < cd.nm; m++) if ((uint8_t)cd.menu[m].t == l.qtype) { k = m; break; }
      if (k < 0) continue;
      const std::string jw = cd.job == life::Job::Noble ? (l.text.find(" LADY ") != std::string::npos ? "LADY" : "LORD") : std::string(life::jobName(cd.job));
      const bool trade = l.text.find(" " + jw + " ") != std::string::npos;
      if (!best || (trade && !bestTrade) || (trade == bestTrade && cd.score < best->score)) { best = &cd; bestK = k; bestTrade = trade; }
    }
    if (best) { give(*best, best->menu[bestK]); l.who = best->key; }
  }
  // 2. the week's needs: a kind drawn by weight (each kind already in the pool weighs less), then a trade that asks
  //    for it, then the person of that trade whose turn it is
  const int farmerCap = std::max(1, want / 3);
  // (who gave last week rests this week: the kinds come from the rested first, and only when they run out from all)
  bool anyone = false;
  std::vector<uint8_t> rested(cands.size(), 1);
  if (last)
    for (size_t i = 0; i < cands.size(); i++)
      for (const jobs::PoolJob& j : last->jobs) if (j.giver == cands[i].key) rested[i] = 0;
  auto open = [&](const Cand& cd) { return !cd.used && !(cd.role == Role::Farmer && farmers >= farmerCap) && (anyone || rested[(size_t)(&cd - cands.data())]); };
  for (int guard = 0; (int)P.jobs.size() < want && guard < 64; guard++) {
    struct KW { Kind k; int w; };
    std::vector<KW> kinds;
    for (const Cand& cd : cands) {
      if (!open(cd)) continue;
      for (int k = 0; k < cd.nm; k++) {
        bool have = false;
        for (const KW& x : kinds) if (x.k.t == cd.menu[k].t && x.k.need == cd.menu[k].need) have = true;
        if (have) continue;
        int w = baseWeight(cd.menu[k].t);
        if (cd.menu[k].need == jobs::JN_FAMINE) w *= 4;
        if (cd.menu[k].need == jobs::JN_WARBAND) w *= 3;
        // (P1b) the radiant kinds are fifteen kinds of their own: each weighs less once it is in the pool (not the
        // whole QType::Story), and together they never take more than about half a pool
        const bool radiant = cd.menu[k].t == QType::Story;
        if (radiant) w = radiantWeight(cd.menu[k].need - jobs::JN_RADIANT0, perType[(int)QType::Story], want);
        for (int n = 0; n < (radiant ? perNeed[cd.menu[k].need & 63] * 2 : perType[(int)cd.menu[k].t]); n++) w = w * 4 / 10;
        if (cd.menu[k].t == QType::Protect && perType[(int)QType::Protect] > 0) w = 0;   // (one night on the fields a week)
        if (w > 0) kinds.push_back({cd.menu[k], w});
      }
    }
    if (kinds.empty()) { if (anyone) break; anyone = true; continue; }
    // (a kind with several needs - a den hunt and a hide hunt - shares its type's weight)
    int tot = 0;
    for (const KW& x : kinds) tot += x.w;
    int roll = r.irange(std::max(1, tot));
    const KW* pk = &kinds.front();
    for (const KW& x : kinds) { if (roll < x.w) { pk = &x; break; } roll -= x.w; }
    const Kind k = pk->k;
    // the trades that ask for it
    struct RW { Role role; int w; };
    std::vector<RW> roles;
    for (const Cand& cd : cands) {
      if (!open(cd)) continue;
      bool fits = false;
      for (int m = 0; m < cd.nm; m++) if (cd.menu[m].t == k.t && cd.menu[m].need == k.need) fits = true;
      if (!fits) continue;
      bool have = false;
      for (RW& x : roles)
        if (x.role == cd.role) {
          have = true;
          // (P1b) a radiant kind leans to the trades with more people free to ask, so the weeks rotate through people
          // rather than back and forth between a town's one priest and one innkeeper
          if (k.t == QType::Story && x.w < 3 * (cd.role == Role::Farmer ? 14 : cd.role == Role::Villager ? 9 : 20)) x.w += cd.role == Role::Farmer ? 14 : cd.role == Role::Villager ? 9 : 20;
        }
      if (!have) roles.push_back({cd.role, cd.role == Role::Farmer ? 14 : cd.role == Role::Villager ? 9 : 20});
    }
    if (roles.empty()) continue;
    int rt = 0;
    for (const RW& x : roles) rt += x.w;
    int rr = r.irange(std::max(1, rt));
    Role role = roles.front().role;
    for (const RW& x : roles) { if (rr < x.w) { role = x.role; break; } rr -= x.w; }
    Cand* best = nullptr;
    for (Cand& cd : cands) {
      if (!open(cd) || cd.role != role) continue;
      bool fits = false;
      for (int m = 0; m < cd.nm; m++) if (cd.menu[m].t == k.t && cd.menu[m].need == k.need) fits = true;
      if (fits && (!best || cd.score < best->score)) best = &cd;
    }
    if (best) give(*best, k);
  }
  // 3. the notice board's two posted jobs (towns and cities): a bounty or a dark place, and a hunt or a ruin
  if (S.type == SiteType::Town || S.type == SiteType::City) {
    for (int b = 0; b < 2; b++) {
      std::vector<Kind> opts;
      if (b == 0) {
        if (N.camps > 0) { opts.push_back({QType::Bounty, jobs::JN_CAMP}); opts.push_back({QType::NamedBandit, jobs::JN_CHIEF}); }
        if (N.war && N.camps > 0) opts.push_back({QType::Bounty, jobs::JN_WARBAND});
        if (N.caves > 0) opts.push_back({QType::Clear, jobs::JN_CAVE});
      } else {
        opts.push_back({QType::Hunt, N.dens > 0 ? (uint8_t)jobs::JN_DEN : (uint8_t)jobs::JN_GROUND});
        if (N.ruins > 0) opts.push_back({QType::Clear, jobs::JN_RUIN});
      }
      if (opts.empty()) opts.push_back({QType::Hunt, jobs::JN_GROUND});
      const Kind k = opts[(size_t)r.irange((int)opts.size())];
      jobs::PoolJob j;
      j.qtype = (uint8_t)k.t; j.need = k.need; j.state = jobs::PJ_OPEN; j.giver = g.npcKeyOf(si, -1, jobs::BOARD_SLOT0 + b);
      P.jobs.push_back(j);
    }
  }
  g.jobBook.pools.push_back(std::move(P));
  g.jobBook.liveDirty = true;
  return &g.jobBook.pools.back();
}

// ---------------------------------------------------------------- leads: a rumour that names a real job elsewhere
namespace {
std::string jobWord(const life::Resident& r) {
  switch (r.job) {
    case life::Job::Noble: return r.female ? "LADY" : "LORD";
    default: break;
  }
  std::string w = life::jobName(r.job);
  return w.empty() ? "GOOD FOLK" : w;
}
std::string leadLine(uint8_t qtype, uint8_t need, const std::string& who, const std::string& town) {
  // (a trade with no name of its own is "someone": "SOMEONE IN VOND NEEDS A CARRIER")
  const bool anyone = who == "VILLAGER" || who == "GOOD FOLK" || who == "LABOURER" || who == "SERVANT";
  const std::string of = anyone ? "SOMEONE IN " + town : "THE " + who + " OF " + town;
  switch ((QType)qtype) {
    case QType::Hunt: return need == jobs::JN_HIDES ? of + " WANTS HIDES" : of + " PAYS FOR A HUNT";
    case QType::Bounty: return need == jobs::JN_RAIDERS ? of + " WANTS THE CARAVAN RAIDERS DEAD" : need == jobs::JN_WARBAND ? of + " WANTS DESERTERS DEALT WITH" : of + " OFFERS A BOUNTY";
    case QType::NamedBandit: return of + " WANTS A BANDIT CHIEF DEAD";
    case QType::Clear: return need == jobs::JN_DESECRATED ? of + " WANTS A RUIN MADE CLEAN" : of + " WANTS A DARK PLACE CLEARED";
    case QType::Heirloom: return need == jobs::JN_STOLEN ? of + " WAS ROBBED" : need == jobs::JN_HOLYRELIC ? of + " SEEKS A LOST RELIC" : of + " LOST A KEEPSAKE";
    case QType::Missing: return anyone ? "SOMEONE IS MISSING IN " + town : "SOMEONE IS MISSING IN " + town + ": ASK THE " + who;
    case QType::Deliver: return of + " NEEDS A CARRIER";
    case QType::Protect: return of + " FEARS A RAID ON THE FIELDS";
    case QType::Supply: return need == jobs::JN_FAMINE ? of + " BEGS FOR FOOD" : of + " IS SHORT OF GOODS";
    case QType::Story:   // (P1b) a radiant job: the kind's own words ("THE MERCHANT OF VOND NEEDS A WARDEN FOR A WAGON")
      if (need >= jobs::JN_RADIANT0 && need < jobs::JN_RADIANT0 + story::radiant::kindCount())
        return of + " " + story::radiant::kinds()[(size_t)(need - jobs::JN_RADIANT0)]->lead;
      return of + " HAS WORK";
    default: return of + " HAS WORK";
  }
}
}  // namespace

// a rumour of work in a settlement whose people nobody here knows yet (not loaded: no census): the kind from the land's
// needs round it, the trade by the kind and the place; its person is found when the pool there is worked out
uint32_t JobOps::rumourFar(Game& g, int fromSite) {
  if (!g.world.endless || !g.world.src) return 0;
  int32_t fx = g.world.ox + (int)std::floor(g.pl().p.x / TILE), fy = g.world.oy + (int)std::floor(g.pl().p.y / TILE);
  if (fromSite >= 0 && fromSite < (int)g.world.sites.size()) { fx = g.world.ox + g.world.sites[(size_t)fromSite].ex; fy = g.world.oy + g.world.sites[(size_t)fromSite].ey; }
  const ew::Gid fromId = fromSite >= 0 && fromSite < (int)g.world.sites.size() ? g.world.sites[(size_t)fromSite].id : 0;
  struct C { uint32_t h; ew::Gid id; };
  std::vector<C> cs;
  for (int32_t ry = ew::regionOf(fy) - 2; ry <= ew::regionOf(fy) + 2; ry++)
    for (int32_t rx = ew::regionOf(fx) - 2; rx <= ew::regionOf(fx) + 2; rx++) {
      const ew::RegionPlan R = g.world.regionPlan(rx, ry);
      for (const ew::SitePlan& p : R.sites) {
        if ((p.type != SiteType::Village && p.type != SiteType::Town && p.type != SiteType::City) || p.id == fromId) continue;
        const int64_t dx = p.ex - fx, dy = p.ey - fy, d2 = dx * dx + dy * dy;
        if (d2 < 60 * 60 || d2 > 520 * 520) continue;
        bool named = false;
        for (const jobs::Lead& l : g.jobBook.leads) if (l.state == jobs::LS_OPEN && l.site == p.id && l.qtype != (uint8_t)QType::COUNT) named = true;
        if (!named) cs.push_back({(uint32_t)(ew::mix64(p.id ^ (uint64_t)(uint32_t)g.day * 0x9E37ull) & 0xFFFFFFFFu), p.id});
      }
    }
  if (cs.empty()) return 0;
  const C* best = &cs.front();
  for (const C& c : cs) if (c.h < best->h) best = &c;
  const int si = g.world.ensureSite(best->id);
  if (si < 0) return 0;
  const Site S = g.world.sites[(size_t)si];
  const Needs N = scanNeeds(g, S);
  const bool village = S.type == SiteType::Village;
  struct K { QType t; const char* who; uint8_t need; };
  std::vector<K> ks;
  ks.push_back({QType::Hunt, village ? "FARMER" : "INNKEEPER", jobs::JN_DEN});
  if (N.camps > 0 && !village) ks.push_back({QType::Bounty, "INNKEEPER", jobs::JN_CAMP});
  if (N.dests > 0) ks.push_back({QType::Deliver, village ? "VILLAGER" : "MERCHANT", jobs::JN_PARCEL});
  if (N.caves > 0) ks.push_back({QType::Missing, village ? "FARMER" : "PRIEST", village ? (uint8_t)jobs::JN_STRAYED : (uint8_t)jobs::JN_LOST});
  if (N.caves > 0 && !village) ks.push_back({QType::Clear, "NOBLE", jobs::JN_CAVE});
  const K& k = ks[(size_t)(best->h >> 8) % ks.size()];
  jobs::Lead l;
  l.from = jobs::LeadFrom::Rumour;
  l.qtype = (uint8_t)k.t;
  l.site = S.id;
  l.hasPos = true;
  l.gx = g.world.ox + S.ex; l.gy = g.world.oy + S.ey;
  l.text = leadLine((uint8_t)k.t, k.need, std::string(k.who) == "NOBLE" ? "LORD" : k.who, S.name);
  return g.addLead(l);
}

uint32_t JobOps::rumourJob(Game& g, int fromSite) {
  struct C { int si; size_t ji; uint64_t key; uint32_t h; };
  std::vector<C> cs;
  const int32_t week = g.day / 7;
  for (int si : g.world.nearSites) {
    if (si < 0 || si >= (int)g.world.sites.size() || si == fromSite || si == g.curSite) continue;
    const Site& S = g.world.sites[(size_t)si];
    if (!S.settlement() || !g.life.find(S.id)) continue;
    jobs::Pool* p = ensurePool(g, si);
    if (!p) continue;
    (void)week;
    for (size_t ji = 0; ji < p->jobs.size(); ji++) {
      const jobs::PoolJob& j = p->jobs[ji];
      if (j.state != jobs::PJ_OPEN) continue;
      bool named = false;
      for (const jobs::Lead& l : g.jobBook.leads) if (l.state == jobs::LS_OPEN && l.who == j.giver) named = true;
      if (named) continue;
      cs.push_back({si, ji, j.giver, (uint32_t)(ew::mix64(j.giver ^ (uint64_t)(uint32_t)g.day * 0x51ull) & 0xFFFFFFFFu)});
    }
  }
  if (cs.empty()) return rumourFar(g, fromSite);
  const C* best = &cs.front();
  for (const C& c : cs) if (c.h < best->h) best = &c;
  const Site& S = g.world.sites[(size_t)best->si];
  const jobs::Pool* p = g.jobBook.pool(S.id, week);
  if (!p) return 0;
  const jobs::PoolJob& j = p->jobs[best->ji];
  std::string who = "NOTICE BOARD";
  const life::Census* c = g.life.find(S.id);
  bool board = false;
  for (int b = 0; b < 2; b++) if (j.giver == g.npcKeyOf(best->si, -1, jobs::BOARD_SLOT0 + b)) board = true;
  if (!board && c)
    for (const life::Resident& r : c->res)
      if (g.npcKeyOf(best->si, -1, jobs::RES_SLOT0 + r.idx) == j.giver) { who = jobWord(r); break; }
  jobs::Lead l;
  l.from = jobs::LeadFrom::Rumour;
  l.qtype = j.qtype;
  l.site = S.id;
  l.who = j.giver;
  l.hasPos = true;
  l.gx = g.world.ox + S.ex; l.gy = g.world.oy + S.ey;
  l.text = board ? "A NOTICE IS UP IN " + S.name + ": " + (j.qtype == (uint8_t)QType::Hunt ? std::string("A HUNT") : std::string("A BOUNTY"))
                 : leadLine(j.qtype, j.need, who, S.name);
  return g.addLead(l);
}

bool JobOps::leadNames(const Game& g, const Actor& a) {
  if (g.jobBook.leads.empty() || !a.npc || a.site < 0) return false;
  bool any = false;
  for (const jobs::Lead& l : g.jobBook.leads) if (l.state == jobs::LS_OPEN && l.who) { any = true; break; }
  if (!any) return false;
  const uint64_t key = giverKey(g, a), raw = g.npcKey(a);
  for (const jobs::Lead& l : g.jobBook.leads)
    if (l.state == jobs::LS_OPEN && l.who && (l.who == key || l.who == raw)) return true;
  return false;
}

uint32_t JobOps::placeLead(Game& g, int si, const std::string& text) {
  if (si < 0 || si >= (int)g.world.sites.size() || text.empty()) return 0;
  const Site& S = g.world.sites[(size_t)si];
  jobs::Lead l;
  l.from = jobs::LeadFrom::Rumour;
  l.qtype = (uint8_t)QType::COUNT;
  l.site = S.id;
  l.hasPos = true;
  l.gx = g.world.ox + S.ex; l.gy = g.world.oy + S.ey;
  l.text = text;
  return g.addLead(l);
}
