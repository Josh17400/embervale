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

void printAudit(const Audit& a) {
  printf("  audit: poi kinds within 120 tiles of the start village %d, within 60: %d (%d sites: %s)\n", a.poiKinds, a.poiNear, a.poiCount, a.poiList.c_str());
  printf("  audit: settlement shapes %d distinct of %d, largest same-shape group %d (within 120 tiles: %d settlements, group %d), same building count %d\n",
         a.layoutSigs, a.settlements, a.layoutMaxGroup, a.nearSettlements, a.nearLayoutMaxGroup, a.countMaxGroup);
  printf("  audit: greetings %d distinct of %d talks in %d towns (worst %s)\n", a.greetDistinct, a.greetTalks, a.greetTowns,
         a.greetWorst.empty() ? "-" : a.greetWorst.c_str());
  printf("  audit: buildings %d, %d distinct looks, largest identical group %d (%s)\n", a.bldgs, a.bldgSigs, a.bldgMaxGroup, a.bldgMaxDesc.c_str());
}
