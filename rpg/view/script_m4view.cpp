// M4 "Banners" VIEW lane script commands (rpg/view/script_api.h; `embervale --script-help` lists them):
//   hearall               test only: every realm event so far counts as heard (the journal's NEWS, the map's markers)
//   journal quests|news|history   open the QUESTS tab on that section
//   testlore              test only: a Lost History entry from the realm's record of the nearest ruin (or a fallen
//                         settlement), so the journal's HISTORY section has something to show before the STORY lane
//                         writes lore in ruins
//   warcamp [dx dy]       test only: lay out a siege camp's war props (tents, the command tent, a catapult, a palisade,
//                         a barricade) at the nearest siege's camp, or dx dy tiles from the hero; trees cleared under it
//   charbldgs [n] [all]   test only: the n-th nearest settlement's buildings burn (Bldg::charred 2 on 70 %, 1 on the
//                         rest; all: every one burned out), as the WARDS overlays will
//   crossborder [n]       stand 6 tiles inside the n-th nearest kingdom border from the hero's land, facing it (walk
//                         across with `walk <dir> 2`; the herald shows); prints the direction
//   expect herald [words] the border herald shows (its title contains the words)
//   expect m4 warprops|owned|burned|fires|markers|border [n]   the last frame drew at least n of those
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/art/art_props.h"
#include "rpg/sim/game.h"
#include "rpg/view/realm_ui.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#include "rpg/world/source.h"

namespace {

bool cmdHearAll(ScriptCtx& c) {
  std::vector<uint32_t> ids;
  for (const realm::WorldEvent& e : c.game.realm.events()) if (!e.heard) ids.push_back(e.id);
  for (uint32_t id : ids) c.game.realm.markHeard(id);
  std::printf("script: %d events heard\n", (int)ids.size());
  return true;
}
EMB_SCRIPT_CMD("hearall", "hearall: test only: every realm event so far counts as heard (M4 VIEW)", cmdHearAll);

bool cmdJournal(ScriptCtx& c) {
  const std::string w = c.arg(1);
  const int s = w == "news" ? 1 : (w == "history" || w == "lore") ? 2 : 0;
  c.view.openMenu(c.game, 1);
  c.view.setJournalSection(s);
  return true;
}
EMB_SCRIPT_CMD("journal", "journal quests|news|history: open the journal on that section (M4 VIEW)", cmdJournal);

bool cmdTestLore(ScriptCtx& c) {
  Game& g = c.game;
  if (!g.world.src) { c.fail("testlore: no endless world"); return true; }
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  int best = -1;
  float bd = 1e30f;
  for (size_t i = 0; i < g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    if (s.type != SiteType::Ruin) continue;
    const float d = (s.ex - px) * (s.ex - px) + (s.ey - py) * (s.ey - py);
    if (d < bd) { bd = d; best = (int)i; }
  }
  story::LoreEntry e;
  if (best >= 0) {
    const Site& s = g.world.sites[(size_t)best];
    const realm::RuinRecord r = g.realm.ruin(*g.world.src, s.id);
    e.key = s.id;
    e.title = "THE FALL OF " + (r.valid && !r.oldName.empty() ? r.oldName : s.name);
    if (r.valid) {
      if (!r.builtBy.empty()) e.lines.push_back("BUILT BY " + r.builtBy + ", " + std::to_string(r.foundedYearsAgo) + " WINTERS AGO.");
      if (!r.lastLord.empty()) e.lines.push_back("ITS LAST LORD WAS " + r.lastLord + ".");
      for (const std::string& cl : r.clues) e.lines.push_back(cl);
    }
  } else e.title = "THE FALL OF AN OLD TOWN";
  if (e.lines.empty()) {
    e.lines.push_back("A CARVED SLAB NAMES A QUEEN WHO HELD THIS HILL FOR FORTY WINTERS.");
    e.lines.push_back("A SOLDIER'S JOURNAL TELLS OF A SIEGE THAT LASTED A WHOLE SUMMER, AND OF THE FIRE AT THE END.");
  }
  e.found = (int)e.lines.size();
  e.total = e.found + 2;
  g.story.lore_.push_back(e);
  return true;
}
EMB_SCRIPT_CMD("testlore", "testlore: test only: a Lost History entry from the nearest ruin's record (M4 VIEW)", cmdTestLore);

// stamp one war prop on its frozen footprint (Filler round the anchor), clearing what stood there
void stampWar(Game& g, art::Prop p, int tx, int ty) {
  int w = 1, h = 1;
  art::m4Footprint(p, w, h);
  Map& m = g.world.over;
  for (int y = ty - h + 1; y <= ty; y++)
    for (int x = tx - w / 2; x <= tx + w / 2; x++) {
      if (!m.in(x, y)) return;
      m.setP(x, y, (x == tx && y == ty) ? (int)p + 1 : (int)art::Prop::Filler + 1);
    }
}

bool cmdWarCamp(ScriptCtx& c) {
  Game& g = c.game;
  Map& m = g.world.over;
  int cx = (int)std::floor(g.pl().p.x / TILE), cy = (int)std::floor(g.pl().p.y / TILE);
  bool atSiege = false;
  if (c.arg(1).empty()) {
    for (const realm::Siege& s : g.realm.sieges())
      if (!s.over && m.in(s.campX - g.world.ox, s.campY - g.world.oy)) { cx = s.campX - g.world.ox; cy = s.campY - g.world.oy; atSiege = true; break; }
  } else {
    cx += std::atoi(c.arg(1).c_str());
    cy += std::atoi(c.arg(2).c_str());
  }
  // a cleared field (no buildings, walls or water under the camp)
  for (int y = cy - 7; y <= cy + 6; y++)
    for (int x = cx - 11; x <= cx + 11; x++) {
      if (!m.in(x, y)) continue;
      if (m.bldgAt[(size_t)y * m.w + x] >= 0 || (!m.wall.empty() && m.wall[(size_t)y * m.w + x])) continue;
      m.setP(x, y, 0);
    }
  stampWar(g, art::Prop::CommandTent, cx, cy - 2);
  stampWar(g, art::Prop::WarTent, cx - 6, cy - 3);
  stampWar(g, art::Prop::WarTent, cx + 6, cy - 3);
  stampWar(g, art::Prop::WarTent, cx - 7, cy + 2);
  stampWar(g, art::Prop::Catapult, cx + 6, cy + 3);
  m.setP(cx, cy + 1, (int)art::Prop::Campfire + 1);
  for (int x = cx - 9; x <= cx + 9; x++) if (m.in(x, cy - 6) && (x < cx - 1 || x > cx + 1)) m.setP(x, cy - 6, (int)art::Prop::Palisade + 1);
  stampWar(g, art::Prop::Barricade, cx, cy + 5);
  m.rebuildSolid();
  // the hero stands south of the camp, looking at it
  g.pl().p = Vec2((cx - 2) * (float)TILE + 8, (cy + 4) * (float)TILE + 8);
  c.view.snap(g);
  std::printf("script: war camp at %d,%d%s\n", g.world.ox + cx, g.world.oy + cy, atSiege ? " (the siege's camp)" : "");
  return true;
}
EMB_SCRIPT_CMD("warcamp", "warcamp [dx dy]: test only: lay out a siege camp's war props at the siege's camp or near the hero (M4 VIEW)", cmdWarCamp);

std::vector<int> nearestSettlementsV(Game& g) {
  std::vector<std::pair<float, int>> d;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  for (size_t i = 0; i < g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    if (!s.settlement()) continue;
    d.push_back({(s.ex - px) * (s.ex - px) + (s.ey - py) * (s.ey - py), (int)i});
  }
  std::sort(d.begin(), d.end());
  std::vector<int> out;
  for (auto& e : d) out.push_back(e.second);
  return out;
}

bool cmdCharBldgs(ScriptCtx& c) {
  Game& g = c.game;
  const std::vector<int> near = nearestSettlementsV(g);
  const size_t n = (size_t)std::max(0, std::atoi(c.arg(1).c_str()));
  if (n >= near.size()) { c.fail("charbldgs: no such settlement"); return true; }
  const bool all = c.arg(2) == "all";
  int k = 0;
  for (Bldg& b : g.world.over.bldgs) {
    if (b.site != near[n]) continue;
    const uint32_t h = (uint32_t)(b.r.x * 73856093) ^ (uint32_t)(b.r.y * 19349663);
    b.charred = all || (h % 10) < 7 ? 2 : 1;
    k++;
  }
  std::printf("script: %d buildings of %s burned\n", k, g.world.sites[(size_t)near[n]].name.c_str());
  if (c.arg(2) == "go" || c.arg(3) == "go") {   // the hero stands in the middle of it
    const Site& S = g.world.sites[(size_t)near[n]];
    g.pl().p = Vec2(S.ex * (float)TILE + 8, (S.ey + 2) * (float)TILE + 8);
    c.view.snap(g);
  }
  return true;
}
EMB_SCRIPT_CMD("charbldgs", "charbldgs [n] [all]: test only: the n-th nearest settlement's buildings burn (M4 VIEW)", cmdCharBldgs);

int gBorderDx = 0, gBorderDy = 0, gBorderLeft = -1;
ew::Gid gBorderLand = 0;
bool cmdCrossBorder(ScriptCtx& c) {
  Game& g = c.game;
  if (!g.world.src) { c.fail("crossborder: no endless world"); return true; }
  const int32_t gx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), gy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  const ew::Gid here = rui::landOwnerAt(g, gx, gy);
  const int want = std::max(0, std::atoi(c.arg(1).c_str()));
  // march out along 16 directions until the land's owner changes; keep the nth nearest crossing on dry land
  struct X { int32_t r, x, y, dx, dy; };
  std::vector<X> found;
  static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  for (int k = 0; k < 4; k++)
    for (int32_t r = 8; r < 1600; r += 4) {
      const int32_t x = gx + dirs[k][0] * r, y = gy + dirs[k][1] * r;
      if (rui::landOwnerAt(g, x, y) == here) continue;
      if (g.world.src->macroFar(x, y).water) break;
      found.push_back({r, x, y, dirs[k][0], dirs[k][1]});
      break;
    }
  std::sort(found.begin(), found.end(), [](const X& a, const X& b) { return a.r < b.r; });
  if ((size_t)want >= found.size()) { c.fail("crossborder: no border found"); return true; }
  const X& f = found[(size_t)want];
  // the last tile still on our side, then 6 back
  int32_t sx = f.x, sy = f.y;
  while (rui::landOwnerAt(g, sx, sy) != here) { sx -= f.dx; sy -= f.dy; }
  sx -= f.dx * 5; sy -= f.dy * 5;
  g.teleportGlobal(sx, sy);
  c.view.snap(g);
  const char* dn = f.dx > 0 ? "east" : f.dx < 0 ? "west" : f.dy > 0 ? "south" : "north";
  std::printf("script: border %s of %d,%d (walk %s)\n", dn, sx, sy, dn);
  gBorderDx = f.dx; gBorderDy = f.dy; gBorderLand = here; gBorderLeft = -1;
  return true;
}
EMB_SCRIPT_CMD("crossborder", "crossborder [n]: stand just inside the n-th nearest border of the hero's land (M4 VIEW)", cmdCrossBorder);

// stepborder: the hero walks (2 px a frame) the way crossborder found until he has crossed and gone on a little
bool cmdStepBorder(ScriptCtx& c) {
  Game& g = c.game;
  if (!gBorderDx && !gBorderDy) { c.fail("stepborder: no crossborder first"); return true; }
  const int32_t gx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), gy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  if (gBorderLeft < 0 && rui::landOwnerAt(g, gx, gy) != gBorderLand) gBorderLeft = 0;
  if (gBorderLeft >= 0 && ++gBorderLeft > 24) return true;
  g.pl().p.x += 2.0f * (float)gBorderDx;
  g.pl().p.y += 2.0f * (float)gBorderDy;
  g.pl().aim = Vec2((float)gBorderDx, (float)gBorderDy);
  return false;
}
EMB_SCRIPT_CMD("stepborder", "stepborder: walk across the border crossborder found (M4 VIEW)", cmdStepBorder);

bool expHerald(ScriptCtx& c) {
  if (!c.view.heraldShowing()) { c.fail("expect herald: no herald showing"); return true; }
  const std::string w = c.rest(2);
  if (!w.empty() && c.view.heraldTitle().find(w) == std::string::npos) c.fail("expect herald: '" + c.view.heraldTitle() + "' lacks '" + w + "'");
  else std::printf("script: herald %s\n", c.view.heraldTitle().c_str());
  return true;
}
EMB_SCRIPT_CMD("expect:herald", "expect herald [words]: the border herald shows (its title contains the words) (M4 VIEW)", expHerald);

bool expM4(ScriptCtx& c) {
  const View::M4Stats s = c.view.m4Stats();
  const std::string w = c.arg(2);
  const int n = c.arg(3).empty() ? 1 : std::atoi(c.arg(3).c_str());
  int v = -1;
  if (w == "warprops") v = s.warProps;
  else if (w == "owned") v = s.warPropsOwned;
  else if (w == "burned") v = s.burned;
  else if (w == "fires") v = s.campFires;
  else if (w == "markers") v = s.mapMarkers;
  else if (w == "border") v = s.borderPx;
  else { c.fail("expect m4 warprops|owned|burned|fires|markers|border [n]"); return true; }
  std::printf("script: m4 %s = %d\n", w.c_str(), v);
  if (v < n) c.fail("expect m4 " + w + ": " + std::to_string(v) + " < " + std::to_string(n));
  return true;
}
EMB_SCRIPT_CMD("expect:m4", "expect m4 warprops|owned|burned|fires|markers|border [n]: the last frame drew at least n (M4 VIEW)", expM4);

}  // namespace

namespace {
// gotoboardwalk [diag|wild|any] [n]: stand on the n-th nearest boardwalk tile (Map::BOARDWALK_MARK) of the loaded
// window: diag, one in a diagonal staircase run; wild, one outside every settlement (a marsh road's)
bool cmdGotoBoardwalk(ScriptCtx& c) {
  Game& g = c.game;
  const Map& m = g.world.over;
  const std::string want = c.arg(1).empty() ? "any" : c.arg(1);
  const int nth = std::max(0, std::atoi(c.arg(2).c_str()));
  auto bw = [&](int x, int y) { return m.in(x, y) && m.at(x, y) == Ground::Bridge && (m.blendAt(x, y) >> 4) == (Map::BOARDWALK_MARK >> 4); };
  const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
  std::vector<std::pair<int, int>> hits;
  for (int y = 1; y < m.h - 1; y++)
    for (int x = 1; x < m.w - 1; x++) {
      if (!bw(x, y)) continue;
      if (want == "wild" && g.world.siteAt(x, y, 2) >= 0) continue;
      if (want == "diag") {
        // a step of a staircase: the run goes on through a side neighbour and turns through the other axis, with the
        // tiles across the step's corner open
        const bool stepA = bw(x + 1, y) && bw(x + 1, y + 1) && !bw(x, y + 1) && bw(x + 2, y + 1);
        const bool stepB = bw(x + 1, y) && bw(x + 1, y - 1) && !bw(x, y - 1) && bw(x + 2, y - 1);
        if (!stepA && !stepB) continue;
      }
      hits.push_back({(x - px) * (x - px) + (y - py) * (y - py), y * 65536 + x});
    }
  std::sort(hits.begin(), hits.end());
  if ((size_t)nth >= hits.size()) { c.fail("gotoboardwalk: none (" + want + ")"); return true; }
  const int x = hits[(size_t)nth].second & 65535, y = hits[(size_t)nth].second >> 16;
  g.pl().p = Vec2(x * (float)TILE + 8, y * (float)TILE + 8);
  c.view.snap(g);
  std::printf("script: boardwalk (%s) at %d,%d, %d found\n", want.c_str(), g.world.ox + x, g.world.oy + y, (int)hits.size());
  return true;
}
EMB_SCRIPT_CMD("gotoboardwalk", "gotoboardwalk [diag|wild|any] [n]: stand on the n-th nearest boardwalk tile of the window (M4 VIEW)", cmdGotoBoardwalk);
}  // namespace
