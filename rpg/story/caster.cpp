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
#include <cstdint>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/life.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/dmath.h"
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
  // (fixer M6b r2) integer distances only (owner rule: generation is integer/dmath): squared distances for the bands, a
  // 24.8 fixed-point distance (dmath isqrt) plus penalties for the score, so MSVC and Emscripten pick the same place
  int64_t best = INT64_MAX;
  const int64_t minD2 = (int64_t)minD * minD, maxD2 = (int64_t)maxD * maxD;
  auto dist2 = [](int64_t dx, int64_t dy) { return dx * dx + dy * dy; };
  auto fixd = [](int64_t d2) { return (int64_t)ew::isqrt((uint64_t)d2 << 16); };   // distance * 256, floor
  const bool settlement = t == SiteType::Village || t == SiteType::Town || t == SiteType::City;
  if (settlement) {
    for (const ew::SettlementNode& n : src.settlementsIn(gx - maxD, gy - maxD, gx + maxD + 1, gy + maxD + 1, true)) {
      // (a script's "town" is any market town: a city serves when no town lies near)
      if ((n.type != t && !(t == SiteType::Town && n.type == SiteType::City)) || avoid.count(n.id)) continue;
      const int64_t townPenalty = (t == SiteType::Town && n.type == SiteType::City) ? 300 : 0;
      const int64_t d2 = dist2(n.x - gx, n.y - gy);
      if (d2 < minD2 || d2 > maxD2) continue;
      const int64_t score = fixd(d2) + ((townPenalty + (prefer && n.kingdom != prefer ? 4000 : 0)) << 8);
      if (score < best) { best = score; out.id = n.id; out.x = n.x; out.y = n.y; out.name = n.name; out.culture = 0; }
    }
    if (!out.id) {
      // (the lattice found none in the band: the region plans' nearest of the type, as the scripts' goto does)
      const int h = g.world.findSiteNear(gx, gy, t, (maxD + ew::REGION - 1) / ew::REGION);
      if (h >= 0) {
        const Site& S = g.world.sites[(size_t)h];
        const int32_t sx = g.world.ox + S.ex, sy = g.world.oy + S.ey;
        const int64_t d2 = dist2(sx - gx, sy - gy);
        if (!avoid.count(S.id) && d2 >= minD2 && d2 <= maxD2) { out.id = S.id; out.x = sx; out.y = sy; }
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
        const int64_t d2 = dist2(p.ex - gx, p.ey - gy);
        if (d2 < minD2 || d2 > maxD2) continue;
        if (d2 < best) { best = d2; out.id = p.id; out.x = p.ex; out.y = p.ey; out.name = p.name; out.culture = p.culture; }
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
  // (M6b) the giver as a census resident (their settlement and index), when they are one
  ew::Gid giverResSite = 0;
  int giverRes = -1;
  if (A) residentOfActor(g, *A, giverResSite, giverRes);
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
          // (M6b) a giver who is a census resident: the census knows their sex
          if (giverRes >= 0)
            if (const life::Census* GC = g.life.find(giverResSite))
              if (giverRes < (int)GC->res.size()) b.female = GC->res[(size_t)giverRes].female;
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
            why += " " + std::to_string((int)n.type) + "@" + std::to_string((int)ew::isqrt((uint64_t)((int64_t)(n.x - hx) * (n.x - hx) + (int64_t)(n.y - hy) * (n.y - hy))));
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
          int64_t bd = INT64_MAX;   // (fixer M6b r2: integer squared distance, no libm)
          const ew::KingdomPlan* hp = land ? g.world.src->kingdom(land) : nullptr;
          const int64_t ox = hp ? hp->gx : hx, oy = hp ? hp->gy : hy;
          for (const realm::KingdomState& K : g.realm.kingdoms()) {
            if (K.id == land || K.fallen) continue;
            const ew::KingdomPlan* kp = g.world.src->kingdom(K.id);
            const int64_t d = kp ? ((int64_t)kp->gx - ox) * ((int64_t)kp->gx - ox) + ((int64_t)kp->gy - oy) * ((int64_t)kp->gy - oy) : INT64_MAX - 1;
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
      case dsl::RoleKind::Resident: {
        // (M6b, COMPOSER lane) a real resident of the M5 census of the `at` settlement (default the hook's): any adult;
        // kin (the `of` person's household: spouse, child, parent, sibling) or friend (their life Ties); a mourner (a
        // grieving household: .lost names their dead); someone needy (hungry or penniless); a child; an elder. The
        // relations need the `of` person in the census too (a giver who is a census resident, or a resident role):
        // otherwise the role fails honestly. Only `any` may invent someone where no census exists (a far hamlet).
        ew::Gid sid = home.id;
        int32_t sx = hx, sy = hy;
        if (const Binding* at = placeOf(R.at)) { sid = at->site ? at->site : at->id; sx = at->gx; sy = at->gy; }
        const int sh = siteByIdLoad(g, sid);
        const int rel0 = dsl::relationWord(R.a);
        // (fixer M6b r1) kin relations: 1 any kin; 7 child, 8 spouse, 9 parent, 10 sibling of the `of` person
        const int kinRel = rel0 >= 7 ? rel0 : (rel0 == 1 ? 1 : 0);
        const int rel = kinRel ? 1 : rel0;
        life::Census* C = sh >= 0 && g.world.sites[(size_t)sh].settlement() ? g.life.census(g.world, sh) : nullptr;
        // the `of` person as a census index of C (-1: not a resident of that census)
        int ofIx = -1;
        if (C && (rel == 1 || rel == 2)) {
          const std::string ofName = R.b.empty() ? std::string("giver") : R.b;
          const Binding* ob = findB(ofName);
          const int ori = s.role(ofName);
          if (ob && ori >= 0 && s.roles[(size_t)ori].kind == dsl::RoleKind::Giver) {
            if (giverRes >= 0 && giverResSite == sid && giverRes < (int)C->res.size()) ofIx = giverRes;
          } else if (ob && ob->trade == RESIDENT_CENSUS && ob->site == sid) {
            for (const life::Resident& r : C->res) if (life::npcId(sid, r.idx) == ob->id) { ofIx = r.idx; break; }
          }
          if (ofIx < 0) { why = "'" + ofName + "' has no household in the census for role '" + R.name + "'"; return false; }
        }
        // residents the cast may not use twice: bound already, or the giver themself
        std::set<int> used;
        if (C) {
          if (giverRes >= 0 && giverResSite == sid) used.insert(giverRes);
          for (const Binding& o : out)
            if (o.kind == (uint8_t)dsl::RoleKind::Resident && o.trade == RESIDENT_CENSUS && o.site == sid)
              for (const life::Resident& r : C->res) if (life::npcId(sid, r.idx) == o.id) { used.insert(r.idx); break; }
        }
        auto usable = [&](const life::Resident& r) {
          return !(r.flags & (life::RF_DEAD | life::RF_AWAY)) && !used.count(r.idx);
        };
        // what `k` is to `of` (kin words) / the dead a mourner grieves for
        auto kinWord = [&](const life::Resident& k, const life::Resident& of) -> std::string {
          if (k.idx == of.spouse || of.idx == k.spouse) return k.female ? "WIFE" : "HUSBAND";
          if (k.parent == of.idx) return k.female ? "DAUGHTER" : "SON";
          if (of.parent == k.idx) return k.female ? "MOTHER" : "FATHER";
          if (k.parent >= 0 && k.parent == of.parent) return k.female ? "SISTER" : "BROTHER";
          return k.female ? "KINSWOMAN" : "KINSMAN";
        };
        int pickIx = -1;
        std::string kin;
        if (C && !C->res.empty()) {
          std::vector<int> cand;
          if (rel == 1) {
            const life::Resident& of = C->res[(size_t)ofIx];
            for (const life::Resident& r : C->res)
              if (r.idx != of.idx && usable(r) && r.age >= 8 &&
                  (kinRel == 7   ? r.parent == of.idx
                   : kinRel == 8 ? (r.idx == of.spouse || r.spouse == of.idx)
                   : kinRel == 9 ? of.parent == r.idx
                   : kinRel == 10 ? (r.parent >= 0 && r.parent == of.parent)
                   : (r.household == of.household || r.idx == of.spouse || r.parent == of.idx || of.parent == r.idx)))
                cand.push_back(r.idx);
          } else if (rel == 2) {
            for (int f : g.life.friendsOf(*C, ofIx))
              if (f >= 0 && f < (int)C->res.size() && usable(C->res[(size_t)f]) && C->res[(size_t)f].age >= 12) cand.push_back(f);
          } else {
            for (const life::Resident& r : C->res) {
              if (!usable(r)) continue;
              bool ok = true;
              switch (rel) {
                case 3: ok = (r.flags & life::RF_GRIEVING) != 0 && r.age >= 14; break;                                   // mourner
                case 4: ok = r.age >= 14 && (r.need[(size_t)life::Need::Hunger] < 35 || r.need[(size_t)life::Need::Money] < 30); break;  // needy
                case 5: ok = r.age >= 7 && r.age < 18; break;                                                             // young
                case 6: ok = r.age >= 60; break;                                                                          // old
                default: ok = r.age >= 16; break;                                                                         // any
              }
              if (ok) cand.push_back(r.idx);
            }
          }
          if (!cand.empty()) {
            // kin and friends: the closest first (the list's order) with a little variety; others: anyone, by the key
            const size_t n = cand.size();
            const size_t pickAt = (rel == 1 || rel == 2) ? (size_t)(rk % std::min<size_t>(n, 2)) : (size_t)(rk % n);
            pickIx = cand[pickAt];
          }
        }
        if (pickIx >= 0) {
          const life::Resident& r = C->res[(size_t)pickIx];
          b.id = life::npcId(sid, r.idx);
          b.name = r.name;
          b.female = r.female;
          b.trade = RESIDENT_CENSUS;
          if (rel == 1) kin = kinWord(r, C->res[(size_t)ofIx]);
          else if (rel == 2) kin = "FRIEND";
          else if (rel == 5) kin = r.female ? "GIRL" : "BOY";
          else if (rel == 6) kin = "ELDER";
          else kin = "NEIGHBOUR";
          std::string lost;
          if (rel == 3) {
            // whom they mourn: a dead member of their household, a dead spouse / parent / child, a dead friend
            for (const life::Resident& d : C->res) {
              if (!(d.flags & life::RF_DEAD) || d.idx == r.idx) continue;
              if (d.household == r.household || d.idx == r.spouse || d.idx == r.parent || d.parent == r.idx) { lost = d.name; break; }
            }
            if (lost.empty())
              for (int f : g.life.friendsOf(*C, r.idx))
                if (f >= 0 && f < (int)C->res.size() && (C->res[(size_t)f].flags & life::RF_DEAD)) { lost = C->res[(size_t)f].name; break; }
            if (lost.empty()) {
              // (grief the census does not explain: someone of the household who died before the census was taken)
              const bool lf = ((rk >> 13) & 1) != 0;
              lost = personNameIn(g, culture, rk ^ 0x10571ull, lf);
            }
          }
          b.extra = std::string(life::jobName(r.job)) + "|" + kin + "|" + lost;
          // where they live: their home's door (the quest marker), else the square
          const Site& S = g.world.sites[(size_t)sh];
          const int bh = C->bldgHandle(S, r.home);
          if (bh >= 0 && bh < (int)g.world.over.bldgs.size()) {
            const Bldg& B = g.world.over.bldgs[(size_t)bh];
            sx = g.world.ox + B.doorX(); sy = g.world.oy + B.doorY() + 1;
          } else { sx = g.world.ox + S.ex; sy = g.world.oy + S.ey; }
        } else if (rel == 0 && !C) {
          // no census there (a settlement out of reach): someone invented, met like a person of the story
          b.female = ((rk >> 9) & 1) != 0;
          b.id = rk | 1;
          b.name = personNameIn(g, culture, rk, b.female);
          b.extra = "VILLAGER|NEIGHBOUR|";
        } else {
          static const char* const relName[] = {"", "kin", "friend", "mourning", "needy", "young", "old"};
          why = std::string("no ") + relName[rel < 0 ? 0 : rel] + " resident for role '" + R.name + "'" + (C ? "" : " (no census there)");
          return false;
        }
        b.site = sid; b.gx = sx; b.gy = sy; b.hasPos = true;
        break;
      }
      case dsl::RoleKind::Beast: {
        // (M6b) the nearest living named unique of the hook's region ring (near: the hook's region and its neighbours;
        // far: two or three regions out). The dead stay dead: one slain (foes' mark) is never cast again.
        const bool far = R.a == "far";
        const int32_t rx0 = ew::regionOf(hx), ry0 = ew::regionOf(hy);
        const int rIn = far ? 2 : 0, rOut = far ? 3 : 1;
        std::vector<foes::NamedUnique> pool;
        for (int32_t ry = ry0 - rOut; ry <= ry0 + rOut; ry++)
          for (int32_t rx = rx0 - rOut; rx <= rx0 + rOut; rx++) {
            if (std::max(std::abs(rx - rx0), std::abs(ry - ry0)) < rIn) continue;
            for (const foes::NamedUnique& u : foes::namedInRegion(*g.world.src, rx, ry)) pool.push_back(u);
          }
        const foes::NamedUnique* best = nullptr;
        int64_t bd = INT64_MAX;
        for (const foes::NamedUnique& u : pool) {
          bool bound = false;
          for (const Binding& o : out) if (o.id == u.id) bound = true;
          if (bound || g.marks.count(storyMarkKey(u.id, foes::MK_FOES_NAMED_SLAIN))) continue;
          const int64_t dx = u.gx - hx, dy = u.gy - hy, d = dx * dx + dy * dy;
          if (d < bd) { bd = d; best = &u; }
        }
        if (!best) { why = "no living named beast for role '" + R.name + "'"; return false; }
        b.id = best->id; b.name = best->name; b.gx = best->gx; b.gy = best->gy; b.hasPos = true;
        b.extra = upper(monsterPlural(best->mon));
        b.trade = (uint8_t)best->mon;
        break;
      }
      case dsl::RoleKind::Boss: {
        bool ok = false;
        const foes::WorldBoss wb = foes::worldBossOf(*g.world.src, ew::EndlessSource::kcellOf(hx), ew::EndlessSource::kcellOf(hy), ok);
        if (!ok) { why = "no world boss for role '" + R.name + "'"; return false; }
        if (g.marks.count(storyMarkKey(wb.id, foes::MK_FOES_BOSS_SLAIN))) { why = wb.name + " is dead (role '" + R.name + "')"; return false; }
        b.id = wb.id; b.name = wb.name; b.gx = wb.lairX; b.gy = wb.lairY; b.hasPos = true;
        b.extra = wb.kind.empty() ? std::string(monsterName(wb.mon)) : wb.kind;
        b.trade = (uint8_t)wb.mon;
        break;
      }
      default: break;
    }
    out.push_back(b);
  }
  return true;
}

}  // namespace story
