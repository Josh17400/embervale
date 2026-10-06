// rpg_test: the repetition audit.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include "rpg/world/poi.h"
#include "rpg/world/source.h"
#include "rpg/world/town_rules.h"
#include "tools/tests/tests.h"

namespace {
const char* bldgName(art::Building b) {
  static const char* n[] = {"house", "stonehouse", "inn", "smithy", "shop", "temple", "keep", "tower", "farmhouse", "hut"};
  return (int)b < (int)(sizeof(n) / sizeof(n[0])) ? n[(int)b] : "?";
}
bool isSettlement(SiteType t) { return t == SiteType::City || t == SiteType::Town || t == SiteType::Village; }

}  // namespace

void newTestGame(Game& g, uint64_t seed);   // seed_run.cpp: a new endless game

Audit repetitionAudit(uint64_t seed) {
  Audit au;
  Game g(seed);
  newTestGame(g, seed);
  g.mode = Mode::Play;
  const World& W = g.world;
  const Map& m = W.over;
  // distances from the start village's heart (M2: the start guarantee is about the village, wherever the player is)
  const int sx = W.sites[(size_t)W.startSite].ex, sy = W.sites[(size_t)W.startSite].ey;
  auto distStart = [&](const Site& s) { return std::hypot((float)(s.ex - sx), (float)(s.ey - sy)); };

  // 1. point-of-interest kinds near the start: site types, vignette and wonder kinds (rpg/world/poi.h), and dens
  {
    std::map<int, std::string> kinds, nearKinds;
    for (int si = 0; si < (int)W.sites.size(); si++) {
      const Site& s = W.sites[(size_t)si];
      if (si == W.startSite || distStart(s) > 120) continue;
      au.poiCount++;
      const int k = ew::poiKindKey(s.type, s.kind);
      kinds[k] = ew::poiKindName(s.type, s.kind);
      if (distStart(s) <= 60) nearKinds[k] = kinds[k];
    }
    for (const Den& d : W.dens) {
      const float dd = std::hypot((float)(d.x - sx), (float)(d.y - sy));
      if (dd <= 120) kinds[99] = "DEN";
      if (dd <= 60) nearKinds[99] = "DEN";
    }
    au.poiKinds = (int)kinds.size();
    au.poiNear = (int)nearKinds.size();
    for (auto& kv : kinds) { if (!au.poiList.empty()) au.poiList += ","; au.poiList += kv.second; }
  }

  // 2. settlement layout: the "shape" (type, centrepiece, size of the paved square) and the building count.
  //    Today a layout archetype is not stored, so these stand in for it until task 3 adds real archetypes.
  {
    std::map<std::string, int> shape, nearShape, count;
    for (const Site& s : W.sites) {
      if (!isSettlement(s.type)) continue;
      int plaza = 0;
      for (int y = s.r.y; y < s.r.y + s.r.h; y++)
        for (int x = s.r.x; x < s.r.x + s.r.w; x++) if (m.at(x, y) == Ground::Plaza) plaza++;
      std::string sh = std::to_string((int)s.type) + "/" + std::to_string(m.propAt(s.ex, s.ey)) + "/" + std::to_string(plaza / 10);
      au.settlements++;
      shape[sh]++;
      count[std::to_string((int)s.type) + "/" + std::to_string(s.bldgCount)]++;
      if (distStart(s) <= 120) { au.nearSettlements++; nearShape[sh]++; }
    }
    au.layoutSigs = (int)shape.size();
    for (auto& kv : shape) au.layoutMaxGroup = std::max(au.layoutMaxGroup, kv.second);
    for (auto& kv : nearShape) au.nearLayoutMaxGroup = std::max(au.nearLayoutMaxGroup, kv.second);
    for (auto& kv : count) au.countMaxGroup = std::max(au.countMaxGroup, kv.second);
  }

  // 3. distinct greeting lines per town: walk up to every outdoor NPC of each settlement and say hello
  //    (endless: the settlements within 600 tiles of the start, each a fast travel away)
  {
    g.godMode = true;
    std::vector<ew::Gid> towns;
    for (const Site& s : W.sites)
      if (isSettlement(s.type) && (!W.endless || distStart(s) <= 600)) towns.push_back(s.id);
    for (ew::Gid tid : towns) {
      int si = W.siteHandle(tid);
      if (si < 0) continue;
      g.world.sites[si].discovered = true;
      if (!g.fastTravel(si)) continue;
      g.hour = 12;
      for (int f = 0; f < 3; f++) g.update(SIM_DT, Input());
      std::vector<int> ids;
      for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].npc && g.actors[k].site == si) ids.push_back(g.actors[k].id);
      std::set<std::string> lines;
      int talks = 0;
      for (int id : ids) {
        int k = -1;
        for (size_t j = 1; j < g.actors.size(); j++) if (g.actors[j].id == id) k = (int)j;
        if (k < 0) continue;
        g.mode = Mode::Play;
        g.pl().p = g.actors[k].p + Vec2(0, 12);
        g.hour = 12;
        Input talk; talk.interact = true;
        g.update(SIM_DT, talk);
        if (g.mode == Mode::Dialogue && g.dlg.actor == id) { lines.insert(g.dlg.text); talks++; }
        g.closeDialogue();
        g.mode = Mode::Play;
      }
      g.events.clear();
      if (!talks) continue;
      au.greetTowns++;
      au.greetTalks += talks;
      au.greetDistinct += (int)lines.size();
      int pct = (int)(100 * lines.size() / talks);
      if (talks >= 3 && pct < au.greetWorstPct) { au.greetWorstPct = pct; au.greetWorst = W.sites[si].name + " " + std::to_string(lines.size()) + "/" + std::to_string(talks); }
    }
  }

  // 4. the largest group of buildings that look the same: identical (type, w, h, roof)
  {
    std::map<std::tuple<int, int, int, uint32_t>, int> grp;
    for (const Bldg& b : m.bldgs) grp[{(int)b.type, b.r.w, b.r.h, b.roof}]++;
    au.bldgs = (int)m.bldgs.size();
    au.bldgSigs = (int)grp.size();
    for (auto& kv : grp)
      if (kv.second > au.bldgMaxGroup) {
        au.bldgMaxGroup = kv.second;
        char d[96];
        snprintf(d, sizeof d, "%s %dx%d roof %06x", bldgName((art::Building)std::get<0>(kv.first)), std::get<1>(kv.first), std::get<2>(kv.first), std::get<3>(kv.first) & 0xFFFFFF);
        au.bldgMaxDesc = d;
      }
  }
  return au;
}

// ---- M3 (VISION_PLAN 5.7): the settlement signature audit. rpg_test --town-audit [--seeds A..B] [--r REGIONS]
// Every settlement planned in a square of REGIONS x REGIONS regions round the start (default 16: 4096 tiles across) gets
// its signature (culture, archetype, layout, wealth): the culture it is built by (its kingdom's dialect, else its
// cell's family), its ew::Archetype (farming, fishing, port...), the layout the generator picks for it
// (ew::townLayoutFor; for a village its street form too, for a town its squares: ew::townForm) and its wealth
// (ew::townWealthFor), among settlements of one type (a village is no copy of a city). The rule: no more than 2 with one signature
// within 1500 tiles of each other. Reports the worst group, the settlements breaking the rule (fails on any), how many
// distinct signatures, and the spread of layouts.
namespace {
int cmdTownAudit(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  int R = 16;
  std::string list;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
    else if (!strcmp(argv[i], "--r") && i + 1 < argc) R = std::max(2, atoi(argv[++i]));
    else if (!strcmp(argv[i], "--list") && i + 1 < argc) list = argv[++i];   // (screenshots: where that layout stands)
  }
  static const char* layoutN[] = {"organic", "grid", "radial", "linear", "compound", "terraced", "stilt"};
  int bad = 0, totalS = 0, worstAll = 0;
  std::map<int, int> layouts;
  for (uint64_t seed = a; seed <= b; seed++) {
    g_curSeed = seed;
    ew::EndlessSource src(seed);
    const ew::StartPlan& sp = src.start();
    const int32_t r0x = ew::regionOf(sp.spawn.x) - R / 2, r0y = ew::regionOf(sp.spawn.y) - R / 2;
    struct S { int32_t x, y; uint64_t culture; int arch, layout, wealth; std::string name; SiteType type; };
    std::vector<S> ss;
    for (int32_t ry = r0y; ry < r0y + R; ry++)
      for (int32_t rx = r0x; rx < r0x + R; rx++) {
        const ew::RegionPlan RP = src.region(rx, ry);   // (a copy: the culture look-ups below may plan other regions)
        for (const ew::SitePlan& p : RP.sites) {
          if (!isSettlement(p.type)) continue;
          const uint64_t cid = p.culture ? p.culture : src.cultureAt(p.ex, p.ey);
          const cult::Culture& K = src.culture(cid);
          S s;
          s.x = p.ex; s.y = p.ey; s.culture = cid; s.arch = (int)p.archetype;
          // (a village's layout is its style and its street form: a crossroads hamlet, a road village, a green; a
          // town's its style and its squares)
          s.layout = (int)ew::townLayoutFor(&K, p.type, p.seed, p.ex, p.ey) * 3 +
                     (p.type != SiteType::City ? ew::townForm(ew::townColour(&K, p.type, p.ex, p.ey)) : 0);
          s.wealth = ew::townWealthFor(&K, p.type, (p.flags & ew::SPF_CAPITAL) != 0, p.seed, p.ex, p.ey);
          s.name = p.name;
          s.type = p.type;
          ss.push_back(s);
          layouts[s.layout / 3]++;
          if (!list.empty() && list == layoutN[s.layout / 3])
            printf("  %s %s (%s, form %d, wealth %d) heart %d,%d\n", siteTypeName(p.type), p.name.c_str(), cult::archetypeName(K.archetype), s.layout % 3,
                   s.wealth, p.ex, p.ey);
        }
      }
    int worst = 0, breaking = 0, worstCA = 0;
    std::string worstDesc;
    std::set<std::tuple<uint64_t, int, int, int, int>> sigs;
    for (size_t i = 0; i < ss.size(); i++) {
      sigs.insert({ss[i].culture, ss[i].arch, ss[i].layout, ss[i].wealth, (int)ss[i].type});
      int same = 0;
      for (size_t j = 0; j < ss.size(); j++) {
        if (ss[j].culture != ss[i].culture || ss[j].arch != ss[i].arch || ss[j].layout != ss[i].layout || ss[j].wealth != ss[i].wealth || ss[j].type != ss[i].type) continue;
        const int64_t dx = ss[j].x - ss[i].x, dy = ss[j].y - ss[i].y;
        if (dx * dx + dy * dy <= 1500ll * 1500ll) same++;
      }
      if (same > worst) {
        worst = same;
        worstDesc = ss[i].name + " (" + layoutN[ss[i].layout / 3] + " " + std::to_string(ss[i].layout % 3) + ", archetype " + std::to_string(ss[i].arch) + ", wealth " + std::to_string(ss[i].wealth) + ")";
      }
      if (same > 2) {
        breaking++;
        if (breaking <= 4)
          out("  over 2: %s (%s, %s %d, archetype %d, wealth %d) at %d,%d\n", ss[i].name.c_str(), siteTypeName(ss[i].type), layoutN[ss[i].layout / 3],
              ss[i].layout % 3, ss[i].arch, ss[i].wealth, ss[i].x, ss[i].y);
      }
      // (how many of one culture and archetype stand within 1500 tiles at all: what layout and wealth have to spread)
      int ca = 0;
      for (size_t j = 0; j < ss.size(); j++) {
        if (ss[j].culture != ss[i].culture || ss[j].arch != ss[i].arch) continue;
        const int64_t dx = ss[j].x - ss[i].x, dy = ss[j].y - ss[i].y;
        if (dx * dx + dy * dy <= 1500ll * 1500ll) ca++;
      }
      worstCA = std::max(worstCA, ca);
    }
    totalS += (int)ss.size();
    worstAll = std::max(worstAll, worst);
    out("town audit seed %llu: %zu settlements in %dx%d regions, %zu signatures, worst same-signature group within 1500 tiles %d (%s), %d settlements over 2; "
        "most of one culture and archetype within 1500 tiles %d\n",
        (unsigned long long)seed, ss.size(), R, R, sigs.size(), worst, worstDesc.c_str(), breaking, worstCA);
    if (breaking) { out("FAIL: town audit seed %llu: %d settlements share their signature with 2+ others within 1500 tiles\n", (unsigned long long)seed, breaking); bad++; }
  }
  std::string ls;
  for (auto& kv : layouts) ls += std::string(" ") + layoutN[kv.first] + " " + std::to_string(kv.second);
  printf("town audit: %d settlements, worst group %d (target 2), layouts:%s; %d failure(s)\n", totalS, worstAll, ls.c_str(), bad);
  return bad ? 1 : 0;
}
}  // namespace
RPG_TEST_CMD("--town-audit", "M3 repetition audit: (culture, archetype, layout, wealth) settlement signatures, no more than 2 alike within 1500 tiles [--seeds A..B] [--r REGIONS]", cmdTownAudit);

void printAudit(const Audit& a) {
  printf("  audit: poi kinds within 120 tiles of the start village %d, within 60: %d (%d sites: %s)\n", a.poiKinds, a.poiNear, a.poiCount, a.poiList.c_str());
  printf("  audit: settlement shapes %d distinct of %d, largest same-shape group %d (within 120 tiles: %d settlements, group %d), same building count %d\n",
         a.layoutSigs, a.settlements, a.layoutMaxGroup, a.nearSettlements, a.nearLayoutMaxGroup, a.countMaxGroup);
  printf("  audit: greetings %d distinct of %d talks in %d towns (worst %s)\n", a.greetDistinct, a.greetTalks, a.greetTowns,
         a.greetWorst.empty() ? "-" : a.greetWorst.c_str());
  printf("  audit: buildings %d, %d distinct looks, largest identical group %d (%s)\n", a.bldgs, a.bldgSigs, a.bldgMaxGroup, a.bldgMaxDesc.c_str());
}
