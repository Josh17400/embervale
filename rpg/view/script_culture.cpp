// M3 culture script commands (rpg/view/script_api.h; `embervale --script-help` lists them). CULTURE lane after
// phase A; every lane uses them for screenshots of a given culture's settlements.
//   gotoculture <archetype|other> [city|capital|town|village|any] [n]
//                        stand just south of the heart of the n-th nearest settlement (default the nearest) whose
//                        culture has that archetype (fjordfolk, highland, heartland, imperial, dune, steppe, marsh, jade,
//                        river, suntemple, sylvan, starspire), or "other": the nearest whose FAMILY differs from the
//                        player's present one and from every family an earlier "gotoculture other" left (so repeated
//                        "gotoculture other" walks a tour of NEW borders). Searches the
//                        culture cells within 12 cells, then the region plans inside the matching cells. "capital":
//                        a kingdom's capital city (its dialect at its grandest).
//   culture              print the culture where the player stands (name, archetype, family / dialect, people mix,
//                        music scale, architecture, layout, wall, alloys, gods) to stdout
//   expect culture <archetype>   the culture where the player stands has that archetype
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#include "rpg/world/source.h"

namespace {

int archetypeOf(const std::string& w) {
  std::string s;
  for (char ch : w) if (ch != '-' && ch != '_') s += (char)(ch >= 'A' && ch <= 'Z' ? ch + 32 : ch);
  for (int a = 0; a < (int)cult::Archetype::COUNT; a++) {
    std::string n;
    for (const char* p = cult::archetypeName((cult::Archetype)a); *p; p++)
      if (*p != '-' && *p != ' ') n += (char)(*p >= 'A' && *p <= 'Z' ? *p + 32 : *p);
    if (n == s) return a;
  }
  return -1;
}

// the culture of the place the player stands in (its settlement's, else the land's)
const cult::Culture* hereCulture(Game& g) {
  if (!g.world.src) return nullptr;
  if (!g.inside && g.curSite >= 0) {
    if (const cult::Culture* c = g.world.cultureOf(g.curSite)) return c;
  }
  return g.world.cultureAtTile((int)std::floor(g.pl().p.x / TILE), (int)std::floor(g.pl().p.y / TILE));
}

bool cmdGotoCulture(ScriptCtx& c) {
  Game& g = c.game;
  if (!g.world.src) { c.fail("gotoculture: no endless world"); return true; }
  const std::string what = c.arg(1);
  const bool other = what == "other";
  const int want = other ? -1 : archetypeOf(what);
  if (!other && want < 0) { c.fail("gotoculture <fjordfolk|highland|heartland|imperial|dune|steppe|marsh|jade|river|suntemple|sylvan|starspire|other> [city|capital|town|village|any] [n]"); return true; }
  const std::string ts = c.arg(2);
  const int nth = c.arg(3).empty() ? 0 : std::max(0, std::atoi(c.arg(3).c_str()));
  auto typeOk = [&](SiteType t) {
    if (ts == "city") return t == SiteType::City;
    if (ts == "town") return t == SiteType::Town;
    if (ts == "village") return t == SiteType::Village;
    return t == SiteType::City || t == SiteType::Town || t == SiteType::Village;
  };
  ew::EndlessSource& S = *g.world.src;
  auto planOk = [&](const ew::SitePlan& s) {
    if (ts != "capital") return typeOk(s.type);
    if (s.type != SiteType::City || !s.kingdom) return false;
    const ew::KingdomPlan* K = S.kingdom(s.kingdom);
    return K && K->capital == s.id;
  };
  const int32_t px = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), py = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  const cult::Culture* here = hereCulture(g);
  const uint64_t hereFam = here ? cult::familyOf(here->id) : 0;
  // "other" never goes back: the families this script has stood in are remembered (a tour crosses NEW borders)
  static std::vector<uint64_t> seenFams;
  if (other && hereFam && std::find(seenFams.begin(), seenFams.end(), hereFam) == seenFams.end()) seenFams.push_back(hereFam);
  auto seen = [&](uint64_t fam) { return std::find(seenFams.begin(), seenFams.end(), fam) != seenFams.end(); };
  const int32_t ci0 = ew::floorDiv(px, ew::CCELL), cj0 = ew::floorDiv(py, ew::CCELL);
  // the candidate culture cells, nearest first
  struct Cell { int32_t ci, cj; double d; };
  std::vector<Cell> cells;
  for (int32_t cj = cj0 - 12; cj <= cj0 + 12; cj++)
    for (int32_t ci = ci0 - 12; ci <= ci0 + 12; ci++) {
      const double d = std::hypot((double)(ci * ew::CCELL + ew::CCELL / 2 - px), (double)(cj * ew::CCELL + ew::CCELL / 2 - py));
      cells.push_back({ci, cj, d});
    }
  std::sort(cells.begin(), cells.end(), [](const Cell& a, const Cell& b) { return a.d < b.d; });
  std::vector<std::pair<ew::SitePlan, double>> hits;
  for (const Cell& cl : cells) {
    if (!other && (int)S.atlas().family(cl.ci, cl.cj).archetype != want) continue;
    // the settlements whose culture qualifies, in the regions of this cell (dialects of kingdoms seated elsewhere
    // count by their own culture, so check each plan's culture)
    const int32_t rx0 = ew::regionOf(cl.ci * ew::CCELL), ry0 = ew::regionOf(cl.cj * ew::CCELL);
    const int nr = ew::CCELL / ew::REGION;
    for (int32_t ry = ry0; ry < ry0 + nr; ry++)
      for (int32_t rx = rx0; rx < rx0 + nr; rx++) {
        const ew::RegionPlan& P = S.region(rx, ry);
        for (const ew::SitePlan& s : P.sites) {
          if (!planOk(s) || !s.culture) continue;
          const cult::Culture& sc = S.culture(s.culture);
          if (other ? (cult::familyOf(sc.id) == hereFam || seen(cult::familyOf(sc.id))) : (int)sc.archetype != want) continue;
          hits.push_back({s, std::hypot((double)(s.ex - px), (double)(s.ey - py))});
        }
      }
    if ((int)hits.size() > nth) break;
  }
  if ((int)hits.size() <= nth) { c.fail("gotoculture " + what + ": no such settlement within 12 culture cells"); return true; }
  std::sort(hits.begin(), hits.end(), [](const std::pair<ew::SitePlan, double>& a, const std::pair<ew::SitePlan, double>& b) { return a.second < b.second; });
  const ew::SitePlan& s = hits[(size_t)nth].first;
  if (g.inside) g.debugLeave();
  g.teleportGlobal(s.ex, s.ey + 3);
  g.pl().aim = Vec2(0, -1);
  g.pl().face = 1;
  g.mode = Mode::Play;
  c.view.snap(g);
  const cult::Culture& sc = S.culture(s.culture);
  std::printf("script: gotoculture %s -> %s (%s, %s) at %d,%d (%.0f tiles away)\n", what.c_str(), s.name.c_str(), sc.name.c_str(),
              cult::archetypeName(sc.archetype), s.ex, s.ey, hits[(size_t)nth].second);
  return true;
}

bool cmdCulture(ScriptCtx& c) {
  const cult::Culture* k = hereCulture(c.game);
  if (!k) { std::printf("script: culture: none\n"); return true; }
  std::printf("script: culture %s (%s%s%s) %s %s, people %d/%d/%d, scale %d lead %d bpm %d meter %d | roof %d/%d alt %d wall %d "
              "window %d door %d layout %d citywall %d | dress cut %d/%d head %d | helm %d blade %d shield %d\n",
              k->name.c_str(), cult::archetypeName(k->archetype), k->isolated ? " x " : "",
              k->isolated ? cult::archetypeName(k->archetype2) : "", cult::cultureKind(k->id) == 2 ? "dialect" : "family",
              k->adjective.c_str(), k->peopleMix[0], k->peopleMix[1], k->peopleMix[2], (int)k->music.scale, (int)k->music.lead,
              (int)k->music.bpm, (int)k->music.meter, (int)k->arch.roof, (int)k->arch.roofMat, (int)k->altRoof, (int)k->arch.wall,
              (int)k->arch.window, (int)k->arch.door, (int)k->town.layout, (int)k->town.wall, (int)k->dress.cutM, (int)k->dress.cutF,
              (int)k->dress.head[0], (int)k->arms.helm[0], (int)k->arms.blade, (int)k->arms.shield);
  std::string alloys, gods;
  for (const cult::Alloy& a : k->arms.alloys) alloys += (alloys.empty() ? "" : ", ") + a.name;
  for (const std::string& n : k->faith.names) gods += (gods.empty() ? "" : ", ") + n;
  std::printf("script: culture %s: alloys %s | gods %s\n", k->name.c_str(), alloys.c_str(), gods.c_str());
  return true;
}

bool expCulture(ScriptCtx& c) {
  const cult::Culture* k = hereCulture(c.game);
  const int want = archetypeOf(c.arg(2));
  if (want < 0) { c.fail("expect culture <archetype>"); return true; }
  if (!k || (int)k->archetype != want) c.fail("expect culture " + c.arg(2) + ": here is " + (k ? std::string(cult::archetypeName(k->archetype)) : std::string("none")));
  return true;
}

}  // namespace

EMB_SCRIPT_CMD("gotoculture", "gotoculture <archetype|other> [city|capital|town|village|any] [n]: stand south of the nearest settlement of that culture (M3)", cmdGotoCulture);
EMB_SCRIPT_CMD("culture", "culture: print the culture where the player stands (M3)", cmdCulture);
EMB_SCRIPT_CMD("expect:culture", "expect culture <archetype>: the culture here has that archetype (M3)", expCulture);
