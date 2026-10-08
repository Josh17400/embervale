// M5 "Hearth and Hall": radiant quests from unmet needs (VISION_PLAN 15.12 "the baker has no flour"). CITIZENS lane.
//   - Supply: a keeper whose workplace is starved of what it works (a bakery without flour, a mill without grain, a
//     smithy without ingots, a smelter without ore, an inn's kitchen without bread or meat...) asks the player to fetch
//     N units from a loaded settlement that makes it (Quest::stage = ew::Good, Quest::target the producer). Walking
//     into the producer loads the goods (a quest item; its stores give them up), the giver pays at the turn-in (the
//     reward scales with the going price) and the goods go into the giver's stores (Life::supply; quests.cpp
//     questTick). Journal lines and markers: quests.cpp.
//   - Rescue: a villager carried off by a raid (life_raids.cpp; Census::takenIdx) is a QType::Missing quest offered by
//     the townsfolk: the M2 machinery places the person in the nearest uncleared cave or camp, the player leads them
//     out, and they come home (quests.cpp questTick clears RF_AWAY; a death is a death).
// lifeOffer is asked first by hasOffer (every frame for the markers) and makeOffer: it stays cheap (the loaded
// records only, no region plans).
#include <algorithm>
#include <cmath>
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"

namespace {

// a good's list price in gold (the reward is the going price, doubled for the trouble, plus a carrier's fee)
int listPrice(ew::Good g) {
  static const int p[] = {2, 3, 2, 2, 3, 4, 8, 12, 2, 3, 3, 6, 10, 4, 4, 7, 5};
  static_assert(sizeof(p) / sizeof(p[0]) == (size_t)ew::Good::COUNT, "a price for every good");
  return (int)g < (int)ew::Good::COUNT ? p[(int)g] : 3;
}

// what a workplace works (the input of its 15.11 recipe; the kitchen's food for an inn)
bool starvedGood(const Bldg& b, const life::Census& c, ew::Good& out) {
  using B = art::Building;
  using G = ew::Good;
  switch (b.type) {
    case B::Bakery: out = G::Flour; break;
    case B::Windmill: case B::Watermill: out = G::Grain; break;
    case B::Smelter: out = G::Ore; break;
    case B::Smithy: out = G::Ingot; break;
    case B::Sawmill: out = G::Logs; break;
    case B::Weaver: out = G::Wool; break;
    case B::Tanner: out = G::Hides; break;
    case B::Butcher: out = G::Livestock; break;
    case B::Fishmonger: out = G::Fish; break;
    case B::Inn: case B::MeadHall: case B::TeaHouse:
      out = c.stock[(size_t)G::Bread] <= c.stock[(size_t)G::Meat] ? G::Bread : G::Meat;
      break;
    default: return false;
  }
  const int have = c.stock[(size_t)out];
  return have <= std::max(2, (int)c.use[(size_t)out] / 2);
}

// a missing person's look and sex come from Quest::flags bits 16-31 (quests.cpp personRng): one that matches theirs
bool personFemaleOf(uint32_t flags, QType t, uint64_t worldSeed) {
  Rng r(((uint64_t)hash32(((flags >> 16) & 0xFFFFu) * 2654435761u ^ (uint32_t)worldSeed ^ 0x9E57u) << 8) ^ ((uint64_t)t << 40) ^ 0x51u);
  return r.f() < 0.45f;
}

}  // namespace

// the giver's own words for a lifeOffer quest (makeOffer shows them; quests.cpp)
std::string lifePitch(const Game& g, const Quest& q) {
  const std::string gold = std::to_string(q.gold);
  const std::string tname = q.target >= 0 && q.target < (int)g.world.sites.size() ? g.world.sites[(size_t)q.target].name : std::string("THE NEXT TOWN");
  if (q.type == QType::Supply) {
    const uint32_t h = hash32((uint32_t)q.giverId ^ (uint32_t)q.stage * 977u);
    static const char* open[] = {"I'M AT MY WITS' END.", "LOOK AT THESE SHELVES. EMPTY.", "NOTHING CAME IN THIS WEEK. NOTHING."};
    return std::string(open[h % 3u]) + " NOT A SCRAP OF " + q.subject + " LEFT, AND PEOPLE STILL COME ASKING. " + tname +
           " HAS PLENTY. BRING ME " + std::to_string(q.need) + " " + q.subject + " AND I'LL PAY YOU " + gold + " GOLD.";
  }
  const bool fem = personFemaleOf(q.flags, q.type, g.seed);
  return "THEY CAME IN THE NIGHT. " + q.subject + " WAS DRAGGED OFF TOWARD " + tname + ". BRING " + (fem ? "HER" : "HIM") +
         " HOME, PLEASE. " + gold + " GOLD IS ALL WE HAVE.";
}

bool Game::lifeOffer(const Actor& npc, Quest& q) const {
  if (!npc.npc || !npc.human || npc.quest > 0 || npc.site < 0 || npc.site >= (int)world.sites.size()) return false;
  switch (npc.role) {
    case Role::Guard: case Role::Child: case Role::King: case Role::Soldier: case Role::Captain: case Role::Herald: case Role::Traveller:
    case Role::Bandit: case Role::Refugee: return false;
    default: break;
  }
  const Site& S = world.sites[(size_t)npc.site];
  if (!S.settlement()) return false;
  const life::Census* c = life.find(S.id);
  if (!c || c->res.empty()) return false;
  const int32_t hx = world.ox + S.ex, hy = world.oy + S.ey;
  // ---- 1. a villager carried off in a raid: bring them home
  if (c->takenIdx >= 0 && c->takenIdx < (int)c->res.size() && c->takenQuest == 0 && npc.role != Role::Jarl) {
    const life::Resident& t = c->res[(size_t)c->takenIdx];
    bool open = false;
    for (const Quest& o : quests) if (o.type == QType::Missing && o.state != QState::Done && o.giverId == S.id && o.subject == t.name) open = true;
    int best = -1;
    float bd = 400.0f * 400.0f;
    for (int oi : world.nearSites) {
      if (oi < 0 || oi >= (int)world.sites.size()) continue;
      const Site& o = world.sites[(size_t)oi];
      // only a place the player can walk into: the Missing flow places the person inside the site (questMapLoaded)
      if ((o.type != SiteType::Cave && o.type != SiteType::Ruin) || o.cleared || o.mainQuest) continue;
      bool taken = false;
      for (const Quest& x : quests) if (x.state != QState::Done && x.type != QType::Main && (x.targetId == o.id || x.target == oi)) taken = true;
      if (taken) continue;
      const float dx = (float)(o.ex - S.ex), dy = (float)(o.ey - S.ey), d = dx * dx + dy * dy;
      if (d < bd) { bd = d; best = oi; }
    }
    if (!open && best >= 0) {
      const Site& T = world.sites[(size_t)best];
      q = Quest();
      q.type = QType::Missing;
      q.giverSite = npc.site; q.giverBldg = npc.bldg; q.giverSlot = npc.slot; q.giverName = npc.name; q.giverId = S.id;
      q.target = best; q.targetId = T.id; q.hasPos = true; q.tgx = world.ox + T.ex; q.tgy = world.oy + T.ey;
      q.subject = t.name;
      for (uint32_t k = 0; k < 64; k++) {   // a person seed whose look is of their sex
        const uint32_t f = (hash32((uint32_t)S.id ^ (uint32_t)c->takenIdx * 2654435761u ^ k) & 0xFFFFu) << 16;
        if (personFemaleOf(f, QType::Missing, seed) == t.female) { q.flags = f; break; }
      }
      const int lvl = std::max(S.level, T.level);
      q.gold = 60 + lvl * 22 + 100; q.xp = 40 + lvl * 14 + 70;
      const std::string obj = t.female ? "HER" : "HIM";
      q.title = "CARRIED OFF: " + t.name;
      q.desc = "RAIDERS CARRIED " + t.name + " OFF FROM " + S.name + " TO " + T.name + " (" + dirWord(q.tgx - hx, q.tgy - hy, false) +
               "). FIND " + obj + " ALIVE AND LEAD " + obj + " OUT.";
      return true;
    }
  }
  // ---- 2. a keeper whose work is starved: the supply run
  int ri = npc.resident;
  if (ri < 0 && npc.bldg >= 0 && npc.bldg >= S.bldgFirst && npc.bldg < S.bldgFirst + S.bldgCount) {
    Spawn sp;
    sp.npc = true; sp.role = npc.role; sp.slot = npc.slot; sp.site = npc.site;
    ri = life::spawnResident(*c, sp, npc.bldg - S.bldgFirst);
  }
  if (ri < 0 || ri >= (int)c->res.size()) return false;
  const life::Resident& r = c->res[(size_t)ri];
  if (r.work < 0 || r.keySlot != 0 || r.work != r.keyBldg || S.bldgFirst + r.work >= (int)world.over.bldgs.size()) return false;
  ew::Good g;
  if (!starvedGood(world.over.bldgs[(size_t)(S.bldgFirst + r.work)], *c, g)) return false;
  for (const Quest& o : quests) if (o.type == QType::Supply && o.state != QState::Done && o.giverId == S.id && o.stage == (int)g) return false;
  // where it can be had: the nearest loaded settlement that makes it (or keeps plenty)
  int best = -1;
  float bd = 600.0f * 600.0f;
  for (int oi : world.nearSites) {
    if (oi < 0 || oi >= (int)world.sites.size() || oi == npc.site) continue;
    const Site& o = world.sites[(size_t)oi];
    if (!o.settlement()) continue;
    const life::Census* oc = life.find(o.id);
    const bool makes = ((o.produces >> (int)g) & 1u) != 0 || (oc && oc->stock[(size_t)g] >= 20);
    if (!makes) continue;
    const float dx = (float)(o.ex - S.ex), dy = (float)(o.ey - S.ey), d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = oi; }
  }
  if (best < 0) return false;
  const Site& T = world.sites[(size_t)best];
  q = Quest();
  q.type = QType::Supply;
  q.stage = (int)g;
  q.need = 4 + 2 * c->tier + (int)(hash32((uint32_t)S.id ^ (uint32_t)g * 31u) % 3u);
  q.have = 0;
  q.giverSite = npc.site; q.giverBldg = npc.bldg; q.giverSlot = npc.slot; q.giverName = npc.name; q.giverId = S.id;
  q.target = best; q.targetId = T.id; q.hasPos = true; q.tgx = world.ox + T.ex; q.tgy = world.oy + T.ey;
  q.subject = ew::goodName(g);
  const int going = std::max(1, listPrice(g) * (int)c->price[(size_t)g] / 100);
  q.gold = 15 + q.need * going * 2;
  q.xp = 20 + q.need * 4;
  const std::string who = r.job == life::Job::Innkeeper ? std::string("THE INN") : "THE " + std::string(life::jobName(r.job));
  q.title = who + " HAS NO " + q.subject;
  q.desc = q.giverName + " IN " + S.name + " NEEDS " + std::to_string(q.need) + " " + q.subject + ". " + T.name + " (" +
           dirWord(q.tgx - hx, q.tgy - hy, false) + ") HAS SOME.";
  return true;
}
