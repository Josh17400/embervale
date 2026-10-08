// M4 "Banners": the caster (owner 15.9: "a generator that casts real world entities (this kingdom, that ruin, this
// NPC, the history sim's events) into roles, so stories come out of the living world"). STORY lane.
//
// Every role of a script is bound, in declaration order, to a real entity near the hook: the speaker who told the
// story, the settlement it was told in, the nearest cave or camp, the kingdom whose land it is and its ruler as the
// realm records them, a rival kingdom, a ruin and its true record, an ongoing war, a realm event the rumours carried
// there. Bindings store stable ids (site ids, npcKeys, kingdom ids, event and war ids) and the global tile of the
// place, so a save keeps them and the quest marker needs no loaded handle.
#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/source.h"

namespace story {

namespace {

SiteType siteTypeWord(const std::string& w) {
  if (w == "cave") return SiteType::Cave;
  if (w == "ruin") return SiteType::Ruin;
  if (w == "camp") return SiteType::BanditCamp;
  if (w == "village") return SiteType::Village;
  if (w == "town") return SiteType::Town;
  if (w == "city") return SiteType::City;
  return SiteType::COUNT;
}

struct Found { ew::Gid id = 0; int32_t x = 0, y = 0; std::string name; uint64_t culture = 0; };

// the nearest site of a type between minD and maxD tiles of (gx, gy), not in `avoid`; settlements of kingdom `prefer`
// win over others within the band
bool findSite(Game& g, int32_t gx, int32_t gy, SiteType t, int minD, int maxD, const std::set<uint64_t>& avoid, ew::Gid prefer,
              Found& out) {
  if (!g.world.src) return false;
  ew::EndlessSource& src = *g.world.src;
  double best = 1e30;
  const bool settlement = t == SiteType::Village || t == SiteType::Town || t == SiteType::City;
  if (settlement) {
    for (const ew::SettlementNode& n : src.settlementsIn(gx - maxD, gy - maxD, gx + maxD + 1, gy + maxD + 1, true)) {
      // (a script's "town" is any market town: a city serves when no town lies near)
      if ((n.type != t && !(t == SiteType::Town && n.type == SiteType::City)) || avoid.count(n.id)) continue;
      const double townPenalty = (t == SiteType::Town && n.type == SiteType::City) ? 300.0 : 0.0;
      const double d = std::hypot((double)(n.x - gx), (double)(n.y - gy));
      if (d < minD || d > maxD) continue;
      const double score = d + townPenalty + (prefer && n.kingdom != prefer ? 4000.0 : 0.0);
      if (score < best) { best = score; out.id = n.id; out.x = n.x; out.y = n.y; out.name = n.name; out.culture = 0; }
    }
    if (!out.id) {
      // (the lattice found none in the band: the region plans' nearest of the type, as the scripts' goto does)
      const int h = g.world.findSiteNear(gx, gy, t, (maxD + ew::REGION - 1) / ew::REGION);
      if (h >= 0) {
        const Site& S = g.world.sites[(size_t)h];
        const int32_t sx = g.world.ox + S.ex, sy = g.world.oy + S.ey;
        const double d = std::hypot((double)(sx - gx), (double)(sy - gy));
        if (!avoid.count(S.id) && d >= minD && d <= maxD) { out.id = S.id; out.x = sx; out.y = sy; }
      }
    }
    if (out.id) {
      const int h = siteByIdLoad(g, out.id);
      if (h >= 0) { out.name = g.world.sites[(size_t)h].name; out.culture = g.world.sites[(size_t)h].culture; }
    }
    return out.id != 0;
  }
  const int rings = (maxD + ew::REGION - 1) / ew::REGION;
  const int32_t rx0 = ew::regionOf(gx), ry0 = ew::regionOf(gy);
  for (int32_t ry = ry0 - rings; ry <= ry0 + rings; ry++)
    for (int32_t rx = rx0 - rings; rx <= rx0 + rings; rx++) {
      const ew::RegionPlan R = g.world.regionPlan(rx, ry);
      for (const ew::SitePlan& p : R.sites) {
        if (p.type != t || avoid.count(p.id)) continue;
        const double d = std::hypot((double)(p.ex - gx), (double)(p.ey - gy));
        if (d < minD || d > maxD) continue;
        if (d < best) { best = d; out.id = p.id; out.x = p.ex; out.y = p.ey; out.name = p.name; out.culture = p.culture; }
      }
    }
  return out.id != 0;
}

const char* const kEpithets[] = {"THE RED", "ONE-EAR", "THE GRIM", "BLACKTOOTH", "THE CROW", "IRONHAND", "THE WOLF",
                                 "HALF-HANG", "THE QUIET", "SALTBEARD", "THE LAME", "GALLOWS-BORN"};

bool looksFemale(const Actor& a) {
  return a.look.outfit == art::Outfit::Dress || a.look.hair == art::Hair::Long || a.look.hair == art::Hair::Braids ||
         a.look.hair == art::Hair::Ponytail;
}

}  // namespace

std::set<uint64_t> boundSites(const Game& g) {
  std::set<uint64_t> s;
  for (const Instance& in : g.story.running())
    for (const Binding& b : in.cast)
      if (b.site && (b.kind == (uint8_t)dsl::RoleKind::Site || b.kind == (uint8_t)dsl::RoleKind::Ruin)) s.insert(b.site);
  return s;
}

bool castScript(Game& g, const dsl::Script& s, int hookActor, int hookSite, uint64_t key, std::vector<Binding>& out, std::string& why) {
  out.clear();
  if (!g.world.endless || !g.world.src) { why = "no endless world"; return false; }
  const Actor* A = nullptr;
  if (hookActor >= 0) {
    const int ai = host().findActor ? host().findActor(g, hookActor) : -1;
    if (ai >= 0) A = &g.actors[(size_t)ai];
  }
  if (hookSite < 0 && A) hookSite = homeSiteOf(g, *A);
  if (hookSite < 0 || hookSite >= (int)g.world.sites.size()) { why = "no hook place"; return false; }
  const Site home = g.world.sites[(size_t)hookSite];   // (a copy: casting loads more sites)
  const int32_t hx = g.world.ox + home.ex, hy = g.world.oy + home.ey;
  uint64_t culture = home.culture;
  if (!culture) culture = g.world.src->cultureAt(hx, hy);
  const ew::Gid land = landAt(g, hx, hy);
  std::set<uint64_t> avoid = boundSites(g);
  auto findB = [&](const std::string& n) -> const Binding* { for (const Binding& b : out) if (b.role == n) return &b; return nullptr; };
  for (size_t ri = 0; ri < s.roles.size(); ri++) {
    const dsl::RoleDef& R = s.roles[ri];
    Binding b;
    b.role = R.name;
    b.kind = (uint8_t)R.kind;
    const uint64_t rk = ew::mix64(key ^ (0x9E37ull * (ri + 1)));
    auto placeOf = [&](const std::string& at) -> const Binding* {
      if (at.empty()) return nullptr;
      return findB(at);
    };
    auto atHome = [&]() { b.site = home.id; b.gx = hx; b.gy = hy; b.hasPos = true; };
    switch (R.kind) {
      case dsl::RoleKind::Giver:
        if (A) {
          b.id = g.npcKey(*A);
          b.name = A->name;
          b.trade = (uint8_t)A->role;
          b.female = looksFemale(*A);
          atHome();
          // a giver indoors: the marker points at the building's door
          if (A->bldg >= 0 && A->bldg < (int)g.world.over.bldgs.size()) {
            const Bldg& B = g.world.over.bldgs[(size_t)A->bldg];
            b.gx = g.world.ox + B.doorX(); b.gy = g.world.oy + B.doorY() + 1;
          } else if (!g.inside) {
            b.gx = g.world.ox + (int32_t)std::floor(A->p.x / TILE); b.gy = g.world.oy + (int32_t)std::floor(A->p.y / TILE);
          }
        } else {
          b.id = home.id;
          b.name = home.name;
          atHome();
        }
        break;
      case dsl::RoleKind::Person: case dsl::RoleKind::Foe: {
        b.female = R.female;
        b.id = rk | 1;
        std::string nm = personNameIn(g, culture, rk, R.female);
        if (R.kind == dsl::RoleKind::Foe) nm += " " + std::string(kEpithets[(rk >> 20) % (sizeof(kEpithets) / sizeof(kEpithets[0]))]);
        b.name = nm;
        if (const Binding* at = placeOf(R.at)) { b.site = at->site; b.gx = at->gx; b.gy = at->gy; b.hasPos = at->hasPos; }
        else atHome();
        break;
      }
      case dsl::RoleKind::Npc: {
        b.trade = (uint8_t)R.trade;
        b.name = std::string("THE ") + dsl::tradeName(R.trade);
        if (const Binding* at = placeOf(R.at)) { b.site = at->site; b.gx = at->gx; b.gy = at->gy; b.hasPos = at->hasPos; b.extra = at->name; }
        else { atHome(); b.extra = home.name; }
        b.id = ew::mix64(b.site ^ (0x7EADull + (uint64_t)R.trade));
        break;
      }
      case dsl::RoleKind::Site: {
        if (R.a == "home") {
          if (!home.settlement() && s.hook != dsl::HookKind::Ruin) { why = "the hook is not a settlement"; return false; }
          b.id = home.id; b.name = home.name; atHome();
          break;
        }
        const SiteType t = siteTypeWord(R.a);
        Found f;
        const bool far = R.b == "far";
        if (!findSite(g, hx, hy, t, far ? 450 : 40, far ? 1300 : 560, avoid, land, f) &&
            !findSite(g, hx, hy, t, far ? 300 : 24, far ? 1800 : 1400, avoid, land, f)) {
          why = "no " + R.a + " for role '" + R.name + "' (near:";
          for (const ew::SettlementNode& n : g.world.src->settlementsIn(hx - 900, hy - 900, hx + 900, hy + 900))
            why += " " + std::to_string((int)n.type) + "@" + std::to_string((int)std::hypot((double)(n.x - hx), (double)(n.y - hy)));
          why += ")";
          return false;
        }
        b.id = f.id; b.site = f.id; b.name = f.name; b.gx = f.x; b.gy = f.y; b.hasPos = true;
        avoid.insert(f.id);
        break;
      }
      case dsl::RoleKind::Ruin: {
        Found f;
        if (!findSite(g, hx, hy, SiteType::Ruin, 30, 640, avoid, 0, f) && !findSite(g, hx, hy, SiteType::Ruin, 16, 1200, avoid, 0, f)) {
          why = "no ruin for role '" + R.name + "'";
          return false;
        }
        b.id = f.id; b.site = f.id; b.name = f.name; b.gx = f.x; b.gy = f.y; b.hasPos = true;
        avoid.insert(f.id);
        break;
      }
      case dsl::RoleKind::Kingdom: {
        ew::Gid k = land;
        if (R.a == "rival") {
          k = 0;
          double bd = 1e30;
          const ew::KingdomPlan* hp = land ? g.world.src->kingdom(land) : nullptr;
          const double ox = hp ? hp->gx : hx, oy = hp ? hp->gy : hy;
          for (const realm::KingdomState& K : g.realm.kingdoms()) {
            if (K.id == land || K.fallen) continue;
            const ew::KingdomPlan* kp = g.world.src->kingdom(K.id);
            const double d = kp ? std::hypot(kp->gx - ox, kp->gy - oy) : 1e20;
            if (d < bd) { bd = d; k = K.id; }
          }
        }
        if (!k) { why = R.a == "rival" ? "no rival kingdom" : "the hook lies in the wildlands"; return false; }
        b.id = k;
        b.name = kingdomName(g, k);
        if (const ew::KingdomPlan* kp = g.world.src->kingdom(k)) { b.gx = kp->gx; b.gy = kp->gy; b.hasPos = true; b.site = kp->capital; }
        break;
      }
      case dsl::RoleKind::Capital: case dsl::RoleKind::Ruler: {
        const Binding* K = findB(R.a);
        if (!K || !K->id) { why = "no kingdom for role '" + R.name + "'"; return false; }
        ew::Gid cap = 0;
        if (const realm::KingdomState* ks = g.realm.kingdom(K->id)) cap = ks->capital;
        if (!cap) if (const ew::KingdomPlan* kp = g.world.src->kingdom(K->id)) cap = kp->capital;
        if (!cap) { why = "the kingdom has no capital"; return false; }
        const int ch = siteByIdLoad(g, cap);
        if (ch < 0) { why = "the capital could not be found"; return false; }
        const Site& C = g.world.sites[(size_t)ch];
        b.site = cap; b.gx = g.world.ox + C.ex; b.gy = g.world.oy + C.ey; b.hasPos = true;
        if (R.kind == dsl::RoleKind::Capital) { b.id = cap; b.name = C.name; break; }
        std::string rn, rt;
        if (!g.story.rulerOf(g, K->id, rn, rt)) { why = "the realm has no ruler for " + K->name; return false; }
        b.id = K->id;
        b.name = rt + " " + rn;
        b.extra = rt;
        const realm::KingdomState* ks = g.realm.kingdom(K->id);
        b.female = ks ? ks->ruler.female : false;
        for (const RulerOverride& o : g.story.rulers()) if (o.kingdom == K->id) b.female = (ew::mix64(K->id ^ (uint64_t)o.day) & 1) != 0;
        // the ruler holds court in the seat of power: the marker points at its door
        for (int bi = C.bldgFirst; bi < C.bldgFirst + C.bldgCount && bi < (int)g.world.over.bldgs.size(); bi++)
          if (bldgIsRoyalSeat(g.world.over.bldgs[(size_t)bi])) {
            b.gx = g.world.ox + g.world.over.bldgs[(size_t)bi].doorX(); b.gy = g.world.oy + g.world.over.bldgs[(size_t)bi].doorY() + 1;
            break;
          }
        break;
      }
      case dsl::RoleKind::War: {
        const Binding* K = findB(R.a);
        if (!K) { why = "no kingdom for the war"; return false; }
        const realm::War* best = nullptr;
        for (const realm::War& w : g.realm.wars())
          if (!w.endDay && (w.attacker == K->id || w.defender == K->id) && (!best || w.startDay > best->startDay)) best = &w;
        if (!best) { why = K->name + " is at peace"; return false; }
        b.id = best->id;
        const ew::Gid foe = best->attacker == K->id ? best->defender : best->attacker;
        b.extra = kingdomName(g, foe);
        b.name = best->name.empty() ? "THE WAR WITH " + b.extra : best->name;
        break;
      }
      case dsl::RoleKind::Event: {
        const realm::WorldEvent* best = nullptr;
        for (const realm::WorldEvent& e : g.realm.events())
          if ((int)e.type == R.ev && rumourReaches(e, g.day, hx, hy) && (!best || e.id > best->id)) best = &e;
        if (!best) { why = std::string("no ") + realm::evTypeName((realm::EvType)R.ev) + " has reached " + home.name; return false; }
        b.id = best->id;
        b.name = newsLine(g, *best, 0);
        b.site = best->site; b.gx = best->gx; b.gy = best->gy; b.hasPos = best->gx || best->gy;
        if (best->site) { const int h = siteByIdLoad(g, best->site); if (h >= 0) b.extra = g.world.sites[(size_t)h].name; }
        break;
      }
      default: break;
    }
    out.push_back(b);
  }
  return true;
}

}  // namespace story
