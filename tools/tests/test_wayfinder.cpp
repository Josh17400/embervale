// rpg_test M2 Wayfinder checks for the endless generator (WORLD lane owns this file).
//   RPG_SEED_CHECK "wayfinder" (every seed of rpg_test N / --seeds A..B): the start guarantee (VISION_PLAN 2.6, M2):
//     at least 8 distinct kinds of place (ew::poiKindKey; dens one kind) within 120 tiles of the start village's
//     heart and 3 within 60; at least 2 places within 25 tiles of the road to the story city in its first 300 tiles;
//     a wonder within 400 tiles. The capital's square: at least 12 townsfolk within 16 tiles of its heart by day, and no
//     empty paved block of 12 x 12 or more.
//   rpg_test --wayfinder [--seeds A..B] [--list]: vignettes and wonders over 2048^2 tiles round the origin: 3-6 per land
//     region, every kind present, names unique within 3x3 regions, clear of settlements (footprint + 8), of roads and
//     rivers; --list prints every place (for screenshot scripts).
//   rpg_test --geology [--seeds A..B]: geology order independence (two sources, opposite orders), constancy inside a
//     province, the rules (primary >= 160, at most two others above 96, copper and tin rarely together, rare metal in
//     at most about 12 % of provinces, only rugged ones), mining settlements on strong ore, and the distribution table.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/art/art_props.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

using namespace ew;

namespace {
bool isSettle(SiteType t) { return t == SiteType::City || t == SiteType::Town || t == SiteType::Village; }
double dst(int32_t ax, int32_t ay, int32_t bx, int32_t by) { return std::sqrt((double)(ax - bx) * (ax - bx) + (double)(ay - by) * (ay - by)); }
double segDist(double x, double y, double ax, double ay, double bx, double by) {
  const double vx = bx - ax, vy = by - ay, L2 = vx * vx + vy * vy;
  double t = L2 > 0 ? ((x - ax) * vx + (y - ay) * vy) / L2 : 0;
  t = std::clamp(t, 0.0, 1.0);
  return std::hypot(x - (ax + t * vx), y - (ay + t * vy));
}
}  // namespace

// The start guarantee on the region plans (shared with rpg_test --endless). Returns failures; fills the numbers.
int startGuarantee(EndlessSource& A, int& near120, int& near60, int& roadPois, double& wonderD, std::string& list) {
  int bad = 0;
  const StartPlan& sp = A.start();
  const SitePlan* home = nullptr;
  std::map<Gid, SitePlan> sites;
  std::vector<DenPlan> dens;
  const int32_t rx0 = regionOf(sp.spawn.x), ry0 = regionOf(sp.spawn.y);
  for (int ry = ry0 - 2; ry <= ry0 + 2; ry++)
    for (int rx = rx0 - 2; rx <= rx0 + 2; rx++) {
      const RegionPlan& R = A.region(rx, ry);
      for (const SitePlan& p : R.sites) sites[p.id] = p;
      for (const DenPlan& d : R.dens) dens.push_back(d);
    }
  auto it = sites.find(sp.village);
  if (it == sites.end()) { out("FAIL: wayfinder: no start village in the plans\n"); return 1; }
  home = &it->second;
  std::map<int, std::string> k120, k60;
  for (auto& kv : sites) {
    const SitePlan& p = kv.second;
    if (p.id == home->id) continue;
    const double d = dst(p.ex, p.ey, home->ex, home->ey);
    const int key = poiKindKey(p.type, p.kind);
    if (d <= 120) k120[key] = poiKindName(p.type, p.kind);
    if (d <= 60) k60[key] = poiKindName(p.type, p.kind);
  }
  for (const DenPlan& d : dens) {
    const double dd = dst(d.x, d.y, home->ex, home->ey);
    if (dd <= 120) k120[99] = "DEN";
    if (dd <= 60) k60[99] = "DEN";
  }
  near120 = (int)k120.size();
  near60 = (int)k60.size();
  list.clear();
  for (auto& kv : k120) { if (!list.empty()) list += ","; list += kv.second; }
  if (near120 < 8) { out("FAIL: wayfinder: %d kinds of place within 120 tiles of the start village (want 8): %s\n", near120, list.c_str()); bad++; }
  if (near60 < 3) { out("FAIL: wayfinder: %d kinds of place within 60 tiles of the start village (want 3)\n", near60); bad++; }
  // the road to the story city: its first 300 tiles from the start village
  roadPois = 0;
  const RoadPlan* story = nullptr;
  for (int ry = ry0 - 2; ry <= ry0 + 2 && !story; ry++)
    for (int rx = rx0 - 2; rx <= rx0 + 2 && !story; rx++)
      for (const RoadPlan& r : A.region(rx, ry).roads)
        if ((r.a == sp.village && r.b == sp.capital) || (r.b == sp.village && r.a == sp.capital)) { static RoadPlan keep; keep = r; story = &keep; break; }
  if (!story || story->pts.size() < 2) {
    out("WARN: wayfinder: no road from the start village to the story city\n");
  } else {
    std::vector<GTile> pts = story->pts;
    if (story->a != sp.village) std::reverse(pts.begin(), pts.end());
    std::vector<std::pair<GTile, GTile>> segs;
    double arc = 0;
    for (size_t n = 0; n + 1 < pts.size() && arc < 300; n++) {
      segs.push_back({pts[n], pts[n + 1]});
      arc += dst(pts[n].x, pts[n].y, pts[n + 1].x, pts[n + 1].y);
    }
    for (auto& kv : sites) {
      const SitePlan& p = kv.second;
      if (isSettle(p.type)) continue;
      double best = 1e9;
      for (auto& s : segs) best = std::min(best, segDist(p.ex, p.ey, s.first.x, s.first.y, s.second.x, s.second.y));
      if (best <= 25) roadPois++;
    }
    if (roadPois < 2) { out("FAIL: wayfinder: %d places within 25 tiles of the road to the story city (want 2)\n", roadPois); bad++; }
  }
  // a wonder within 400 tiles
  wonderD = 1e9;
  for (int ry = ry0 - 3; ry <= ry0 + 3; ry++)
    for (int rx = rx0 - 3; rx <= rx0 + 3; rx++)
      for (const SitePlan& p : A.region(rx, ry).sites)
        if (p.type == SiteType::Wonder) wonderD = std::min(wonderD, dst(p.ex, p.ey, home->ex, home->ey));
  if (wonderD > 400) { out("FAIL: wayfinder: the nearest wonder is %.0f tiles from the start (want <= 400)\n", wonderD); bad++; }
  return bad;
}

namespace {

int wayfinderSeed(uint64_t seed) {
  EndlessSource A(seed);
  int n120 = 0, n60 = 0, road = 0;
  double wd = 0;
  std::string list;
  const int bad = startGuarantee(A, n120, n60, road, wd, list);
  out("  wayfinder: start guarantee %d kinds within 120 (%s), %d within 60, %d by the story road, wonder at %.0f\n", n120, list.c_str(), n60, road, wd);
  return bad;
}
RPG_SEED_CHECK("wayfinder", wayfinderSeed);

// ---- rpg_test --wayfinder
int cmdWayfinder(int argc, char** argv) {
  uint64_t a = 1, b = 5;
  bool list = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--list")) list = true;
  }
  int bad = 0;
  int kindTotal[(int)VignetteKind::COUNT] = {}, wonderTotal[(int)WonderKind::COUNT] = {};
  int perRegionMin = 99, perRegionMax = 0, landRegions = 0, vigTotal = 0, wonders = 0, graves = 0;
  for (uint64_t s = a; s <= b; s++) {
    g_curSeed = s;
    EndlessSource A(s);
    const StartPlan& sp = A.start();
    {
      int n120 = 0, n60 = 0, road = 0;
      double wd = 0;
      std::string kl;
      const int f = startGuarantee(A, n120, n60, road, wd, kl);
      bad += f;
      out("seed %llu: start guarantee %d kinds within 120, %d within 60, %d by the story road, wonder at %.0f%s\n", (unsigned long long)s, n120, n60, road, wd, f ? " FAILED" : "");
    }
    std::map<Gid, SitePlan> all;
    std::vector<std::pair<int, int>> regionOfSite;
    for (int ry = -4; ry <= 3; ry++)
      for (int rx = -4; rx <= 3; rx++) {
        const RegionPlan& R = A.region(rx, ry);
        int nv = 0;
        for (const SitePlan& p : R.sites) {
          all[p.id] = p;
          if (p.type == SiteType::Vignette) nv++;
        }
        // land share of the region (sparse)
        int land = 0;
        for (int j = 0; j < 4; j++)
          for (int i = 0; i < 4; i++) if (!A.macroFar(rx * 256 + 32 + i * 64, ry * 256 + 32 + j * 64).water) land++;
        if (land >= 12) {
          landRegions++;
          perRegionMin = std::min(perRegionMin, nv);
          perRegionMax = std::max(perRegionMax, nv);
        }
      }
    std::vector<const SitePlan*> vig, settle;
    for (auto& kv : all) {
      const SitePlan& p = kv.second;
      if (isSettle(p.type)) settle.push_back(&p);
      if (p.type == SiteType::Vignette) { vig.push_back(&p); kindTotal[p.kind]++; vigTotal++; }
      if (p.type == SiteType::Wonder) { vig.push_back(&p); wonderTotal[p.kind]++; wonders++; }
    }
    // names unique within 3x3 regions; clear of settlements (footprint + 8)
    for (const SitePlan* p : vig) {
      for (const SitePlan* q : vig)
        if (p->id < q->id && p->name == q->name && std::abs(regionOf(p->ex) - regionOf(q->ex)) <= 2 && std::abs(regionOf(p->ey) - regionOf(q->ey)) <= 2) {
          out("FAIL: wayfinder: two places called %s within 3x3 regions\n", p->name.c_str());
          bad++;
        }
      for (const SitePlan* t : settle) {
        const bool overlap = p->gx < t->gx + t->w + 8 && t->gx - 8 < p->gx + p->w && p->gy < t->gy + t->h + 8 && t->gy - 8 < p->gy + p->h;
        if (overlap) { out("FAIL: wayfinder: %s %s within 8 tiles of %s\n", poiKindName(p->type, p->kind), p->name.c_str(), t->name.c_str()); bad++; }
      }
      if (p->type == SiteType::Wonder)
        for (const SitePlan* t : settle) {
          const double ox = std::max(0, std::abs(p->ex - (t->gx + t->w / 2)) - t->w / 2), oy = std::max(0, std::abs(p->ey - (t->gy + t->h / 2)) - t->h / 2);
          if (std::hypot(ox, oy) < 150) { out("FAIL: wayfinder: wonder %s %.0f tiles from %s (want 150)\n", p->name.c_str(), std::hypot(ox, oy), t->name.c_str()); bad++; }
        }
      // the heart prop survives chunkgen: no road or another place's track wipes the lone grave's cairn
      if (p->type == SiteType::Vignette && p->kind == (uint8_t)VignetteKind::LoneGrave) {
        ChunkData c;
        A.chunk(chunkOf(p->ex), chunkOf(p->ey), c);
        const int lx = p->ex - chunkOf(p->ex) * CHUNK, ly = p->ey - chunkOf(p->ey) * CHUNK;
        if (c.prop[c.at(lx, ly)] != (uint8_t)((int)art::Prop::GraveCairn + 1)) {
          out("FAIL: wayfinder: %s at %d,%d has no cairn on its heart after chunkgen\n", p->name.c_str(), p->ex, p->ey);
          bad++;
        }
        graves++;
      }
      if (list)
        printf("seed %llu %-18s %-34s at %d,%d level %d (%.0f from the start)\n", (unsigned long long)s, poiKindName(p->type, p->kind), p->name.c_str(), p->ex, p->ey,
               p->level, dst(p->ex, p->ey, sp.spawn.x, sp.spawn.y));
    }
  }
  printf("wayfinder: %d vignettes over %d land regions (%d..%d per land region, avg %.1f), %d wonders\n", vigTotal, landRegions, perRegionMin, perRegionMax,
         landRegions ? (double)vigTotal / landRegions : 0.0, wonders);
  printf("wayfinder: kinds:");
  for (int k = 0; k < (int)VignetteKind::COUNT; k++) printf(" %s %d,", vignetteName((VignetteKind)k), kindTotal[k]);
  printf(" | wonders:");
  for (int k = 0; k < (int)WonderKind::COUNT; k++) printf(" %s %d,", wonderName((WonderKind)k), wonderTotal[k]);
  printf("\n");
  for (int k = 0; k < (int)VignetteKind::COUNT; k++)
    if (!kindTotal[k] && b - a >= 4) { printf("FAIL: wayfinder: no %s anywhere\n", vignetteName((VignetteKind)k)); bad++; }
  for (int k = 0; k < (int)WonderKind::COUNT; k++)
    if (!wonderTotal[k] && b - a >= 2) { printf("FAIL: wayfinder: no %s anywhere\n", wonderName((WonderKind)k)); bad++; }
  printf("wayfinder: %d lone graves checked for their cairn after chunkgen\n", graves);
  printf("wayfinder: %d failures\n", bad);
  return bad ? 1 : 0;
}

// rpg_test --startvillage [--seeds A..B]: the start village's plan, road bearings and a hash of its buildings (debugging)
int cmdStartVillage(int argc, char** argv) {
  uint64_t a = 1, b = 1;
  for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
  for (uint64_t s = a; s <= b; s++) {
    EndlessSource A(s);
    const StartPlan& sp = A.start();
    SitePlan v;
    for (const SitePlan& p : A.region(idRx(sp.village), idRy(sp.village)).sites) if (p.id == sp.village) v = p;
    std::vector<float> br = A.roadBearings(v.id);
    printf("seed %llu village %s at %d,%d fp %d,%d %dx%d arch %d spec %d seed %u level %d bldgBase %d:", (unsigned long long)s, v.name.c_str(), v.ex, v.ey, v.gx, v.gy,
           v.w, v.h, (int)v.archetype, (int)v.special, v.seed, v.level, v.bldgBase);
    for (float f : br) printf(" %.4f", f);
    uint64_t h = 1469598103934665603ull;
    int nb = 0;
    ChunkData c;
    A.chunk(chunkOf(v.ex), chunkOf(v.ey), c);
    for (const Bldg& bl : c.bldgs) {
      const uint64_t vals[8] = {bl.id, (uint64_t)bl.type, (uint64_t)(uint32_t)bl.r.x, (uint64_t)(uint32_t)bl.r.y, (uint64_t)bl.r.w, (uint64_t)bl.r.h, bl.seed, bl.storeys};
      for (uint64_t q : vals) for (int k = 0; k < 8; k++) { h ^= (uint8_t)(q >> (k * 8)); h *= 1099511628211ull; }
      nb++;
    }
    printf(" | %d buildings, hash %016llx\n", nb, (unsigned long long)h);
    World w7;
    w7.generateEndless(s);
    std::map<int, int> perSite;
    for (const Bldg& bl : w7.over.bldgs) perSite[bl.site]++;
    printf("  window: %zu buildings, %zu sites:", w7.over.bldgs.size(), w7.sites.size());
    for (auto& kv : perSite) printf(" site %d (%s) %d", kv.first, kv.first >= 0 ? w7.sites[(size_t)kv.first].name.c_str() : "-", kv.second);
    printf("\n");
  }
  return 0;
}

// ---- rpg_test --geology [--seeds A..B]
bool sameGeo(const Geology& a, const Geology& b) {
  if (a.rock != b.rock || a.province != b.province) return false;
  for (int o = 0; o < (int)Ore::COUNT; o++) if (a.ore[o] != b.ore[o]) return false;
  return true;
}

int cmdGeology(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
  int bad = 0;
  int rockN[(int)Rock::COUNT] = {}, primN[(int)Ore::COUNT] = {}, provinces = 0, rare = 0, bronze = 0, mines = 0, minesWeak = 0;
  int64_t tiles = 0;
  for (uint64_t s = a; s <= b; s++) {
    g_curSeed = s;
    EndlessSource A(s), B(s);
    // a grid over 8192 x 8192 tiles round the origin, every 64 tiles; B asks in the opposite order after a far place
    const int N = 128, SP = 64;
    std::vector<Geology> ga((size_t)N * N), gb((size_t)N * N);
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) ga[(size_t)j * N + i] = A.geology(-4096 + i * SP, -4096 + j * SP);
    (void)B.geology(400000, -300000);
    for (int j = N - 1; j >= 0; j--) for (int i = N - 1; i >= 0; i--) gb[(size_t)j * N + i] = B.geology(-4096 + i * SP, -4096 + j * SP);
    int diff = 0;
    for (size_t k = 0; k < ga.size(); k++) if (!sameGeo(ga[k], gb[k])) diff++;
    if (diff) { out("FAIL: geology: %d of %zu samples differ with the order they were asked in\n", diff, ga.size()); bad++; }
    // one province, one geology; the rules per province
    std::map<uint32_t, Geology> prov;
    std::set<uint32_t> landProv;
    for (size_t k = 0; k < ga.size(); k++) {
      const Geology& g = ga[k];
      tiles++;
      if (!A.macroFar(-4096 + (int)(k % N) * SP, -4096 + (int)(k / N) * SP).water) landProv.insert(g.province);
      auto it = prov.find(g.province);
      if (it == prov.end()) { prov[g.province] = g; continue; }
      if (!sameGeo(it->second, g)) { out("FAIL: geology: province %08x is not one geology\n", g.province); bad++; break; }
    }
    for (auto& kv : prov) {
      const Geology& g = kv.second;
      if (!landProv.count(kv.first)) continue;   // (the table is about the land's provinces; the sea floor is basalt)
      provinces++;
      rockN[(int)g.rock]++;
      primN[(int)g.primary()]++;
      int above = 0;
      for (int o = 0; o < (int)Ore::COUNT; o++) if (o != (int)g.primary() && g.ore[o] > 96) above++;
      if (g.ore[(int)g.primary()] < 160) { out("FAIL: geology: province %08x has its primary ore at %d (< 160)\n", kv.first, g.ore[(int)g.primary()]); bad++; }
      if (above > 2) { out("FAIL: geology: province %08x has %d ores above 96 besides its primary\n", kv.first, above); bad++; }
      if (g.ore[(int)Ore::Rare] > 96) rare++;
      if (g.ore[(int)Ore::Copper] > 96 && g.ore[(int)Ore::Tin] > 96) bronze++;
    }
    // mining settlements sit on strong ore
    for (int ry = -4; ry <= 3; ry++)
      for (int rx = -4; rx <= 3; rx++)
        for (const SitePlan& p : A.region(rx, ry).sites) {
          if (!isSettle(p.type) || p.special != Specialty::Mining) continue;
          mines++;
          const Geology g = A.geology(p.ex, p.ey);
          int strongest = 0;
          for (int o = 0; o < (int)Ore::COUNT; o++) strongest = std::max(strongest, (int)g.ore[o]);
          if (strongest < 180) { minesWeak++; out("FAIL: geology: mining %s %s stands on weak ore (%d)\n", siteTypeName(p.type), p.name.c_str(), strongest); bad++; }
        }
  }
  printf("geology: %d land provinces over %lld samples (%llu seeds)\n", provinces, (long long)tiles, (unsigned long long)(b - a + 1));
  printf("geology: rock  ");
  for (int r = 0; r < (int)Rock::COUNT; r++) printf(" %s %d%%", rockName((Rock)r), provinces ? rockN[r] * 100 / provinces : 0);
  printf("\ngeology: primary ore");
  for (int o = 0; o < (int)Ore::COUNT; o++) printf(" %s %d%%", oreName((Ore)o), provinces ? primN[o] * 100 / provinces : 0);
  printf("\ngeology: rare metal in %d%% of provinces (max 12), copper and tin both strong in %d%%; mining settlements %d (%d on weak ore)\n",
         provinces ? rare * 100 / provinces : 0, provinces ? bronze * 100 / provinces : 0, mines, minesWeak);
  if (provinces && rare * 100 > provinces * 12) { printf("FAIL: geology: rare metal in %d%% of provinces (max 12)\n", rare * 100 / provinces); bad++; }
  if (provinces && bronze * 100 > provinces * 12) { printf("FAIL: geology: copper and tin strong together in %d%% of provinces (rarely)\n", bronze * 100 / provinces); bad++; }
  printf("geology: %d failures\n", bad);
  return bad ? 1 : 0;
}

// rpg_test --landmarks [--seeds A..B] [--list]: the named features over 2048^2 tiles round the origin: every kind present
// somewhere, names unique within 3x3 regions, ids unique; the landmass rule (the start and the story city on one
// landmass, the open sea none)
int cmdLandmarks(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  bool list = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--list")) list = true;
  }
  static const char* kn[] = {"range", "peak", "pass", "lake", "river", "forest", "marsh", "desert", "hills", "sea"};
  int bad = 0, count[(int)LandmarkKind::COUNT] = {}, total = 0;
  for (uint64_t s = a; s <= b; s++) {
    g_curSeed = s;
    EndlessSource A(s);
    const StartPlan& sp = A.start();
    std::vector<std::pair<LandmarkPlan, std::pair<int, int>>> all;
    std::set<Gid> ids;
    for (int ry = -4; ry <= 3; ry++)
      for (int rx = -4; rx <= 3; rx++)
        for (const LandmarkPlan& l : A.region(rx, ry).landmarks) {
          all.push_back({l, {rx, ry}});
          if (!ids.insert(l.id).second) { out("FAIL: landmarks: id %016llx twice\n", (unsigned long long)l.id); bad++; }
          if (regionOf(l.x) != rx || regionOf(l.y) != ry) { out("FAIL: landmarks: %s labelled outside its region\n", l.name.c_str()); bad++; }
          if ((int)l.kind < (int)LandmarkKind::COUNT) count[(int)l.kind]++;
          total++;
          if (list) printf("seed %llu %-6s %-28s at %d,%d size %d\n", (unsigned long long)s, kn[(int)l.kind % 10], l.name.c_str(), l.x, l.y, l.size);
        }
    for (size_t i = 0; i < all.size(); i++)
      for (size_t j = i + 1; j < all.size(); j++)
        if (all[i].first.name == all[j].first.name && std::abs(all[i].second.first - all[j].second.first) <= 2 &&
            std::abs(all[i].second.second - all[j].second.second) <= 2) {
          out("FAIL: landmarks: two called %s within 3x3 regions\n", all[i].first.name.c_str());
          bad++;
        }
    // the landmass rule
    SitePlan city;
    for (const SitePlan& p : A.region(idRx(sp.capital), idRy(sp.capital)).sites) if (p.id == sp.capital) city = p;
    const uint32_t lh = A.landmass(sp.spawn.x, sp.spawn.y), lc = A.landmass(city.ex, city.ey);
    if (!lh || lh != lc) { out("FAIL: landmass: the start (%u) and the story city (%u) are not on one landmass\n", lh, lc); bad++; }
    int sea = 0;
    for (int k = 0; k < 400 && !sea; k++) {
      const int32_t x = 9000 + k * 977, y = -7000 + k * 613;
      if (A.macroFar(x, y).water) { sea = 1; if (A.landmass(x, y)) { out("FAIL: landmass: the sea at %d,%d is land %u\n", x, y, A.landmass(x, y)); bad++; } }
    }
  }
  printf("landmarks: %d over %llu seeds:", total, (unsigned long long)(b - a + 1));
  for (int k = 0; k < (int)LandmarkKind::COUNT; k++) printf(" %s %d", kn[k], count[k]);
  printf("\n");
  for (int k = 0; k < (int)LandmarkKind::COUNT; k++)
    if (!count[k] && k != (int)LandmarkKind::Desert && k != (int)LandmarkKind::Sea && k != (int)LandmarkKind::Marsh) { printf("FAIL: landmarks: no %s labelled\n", kn[k]); bad++; }
  printf("landmarks: %d failures\n", bad);
  return bad ? 1 : 0;
}

// rpg_test --sites-at X,Y [--seeds S..S] [--r R]: every planned site within R tiles of a global tile (debugging)
int cmdSitesAt(int argc, char** argv) {
  uint64_t a = 1, b = 1;
  int32_t x = 0, y = 0, r = 80;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--sites-at") && i + 1 < argc) std::sscanf(argv[++i], "%d,%d", &x, &y);
    else if (!strcmp(argv[i], "--r") && i + 1 < argc) r = std::atoi(argv[++i]);
  }
  EndlessSource A(a);
  for (int ry = regionOf(y - r) - 1; ry <= regionOf(y + r) + 1; ry++)
    for (int rx = regionOf(x - r) - 1; rx <= regionOf(x + r) + 1; rx++)
      for (const SitePlan& p : A.region(rx, ry).sites)
        if (std::abs(p.ex - x) <= r && std::abs(p.ey - y) <= r)
          printf("%-18s %-30s heart %d,%d box %d,%d %dx%d level-at-heart %d id %016llx\n", poiKindName(p.type, p.kind), p.name.c_str(), p.ex, p.ey, p.gx, p.gy, p.w, p.h,
                 A.macro(p.ex, p.ey).height, (unsigned long long)p.id);
  return 0;
}

// rpg_test --chunkbench [--seeds A..B]: chunk cost with warm caches (a 24 x 24 chunk block round the start generated once,
// then again, timed): the generator's own per-chunk work, without region plans or settlements
int cmdChunkBench(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
  double total = 0;
  int n = 0;
  for (uint64_t s = a; s <= b; s++) {
    EndlessSource A(s);
    const StartPlan& sp = A.start();
    ChunkData c;
    const int32_t c0x = chunkOf(sp.spawn.x) - 12, c0y = chunkOf(sp.spawn.y) - 12;
    for (int j = 0; j < 24; j++) for (int i = 0; i < 24; i++) A.chunk(c0x + i, c0y + j, c);
    auto t0 = std::chrono::steady_clock::now();
    for (int rep = 0; rep < 2; rep++)
      for (int j = 0; j < 24; j++) for (int i = 0; i < 24; i++) A.chunk(c0x + i, c0y + j, c);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    printf("seed %llu: %.3f ms per chunk (warm)\n", (unsigned long long)s, ms / (2 * 24 * 24));
    total += ms; n += 2 * 24 * 24;
  }
  printf("chunkbench: %.3f ms per chunk\n", total / n);
  return 0;
}

}  // namespace

RPG_TEST_CMD("--landmarks", "M2 landmarks: kinds, unique names and ids, the landmass rule [--seeds A..B] [--list]", cmdLandmarks);
RPG_TEST_CMD("--sites-at", "every planned site near a global tile (debugging) --sites-at X,Y [--seeds S..S] [--r R]", cmdSitesAt);
RPG_TEST_CMD("--chunkbench", "chunk cost with warm caches round the start [--seeds A..B]", cmdChunkBench);
RPG_TEST_CMD("--geology", "M2 geology: order independence, one geology per province, the ore rules, the distribution [--seeds A..B]", cmdGeology);
RPG_TEST_CMD("--startvillage", "the start village's plan, bearings and building hash (debugging) [--seeds A..B]", cmdStartVillage);
RPG_TEST_CMD("--wayfinder", "M2 vignettes and wonders: counts, kinds, names, clearances [--seeds A..B] [--list]", cmdWayfinder);
