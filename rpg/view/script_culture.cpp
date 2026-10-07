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
//   society              (M3b) print the society of the culture here (government, seat, titles, gathering places, law,
//                        military, trade) and the seat of power of the settlement here
//   gotoseat [south N]   (M3b) stand N tiles (default 3) south of the door of the seat of power of the settlement here
//                        (or the nearest loaded one), facing it
//   expect seat <purpose>  (M3b) that seat of power is built as that purpose (palace, keep, guildhall, meadhall...)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/culture/society.h"
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

// ---------------------------------------------------------------- M3b societies and seats of power
// the settlement the player is in, else the nearest one with its buildings loaded (-1: none)
int hereSettlement(Game& g) {
  if (g.curSite >= 0 && g.curSite < (int)g.world.sites.size() && g.world.sites[(size_t)g.curSite].settlement() &&
      g.world.sites[(size_t)g.curSite].bldgCount > 0)
    return g.curSite;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  int best = -1;
  float bd = 1e30f;
  for (size_t i = 0; i < g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    if (!s.settlement() || s.bldgCount <= 0) continue;
    const float d = std::hypot(s.ex - px, s.ey - py);
    if (d < bd) { bd = d; best = (int)i; }
  }
  return best;
}
// its seat of power: the royal seat first, else a lord's seat (bldgIsSeat), else -1
int seatOf(Game& g, int si) {
  if (si < 0) return -1;
  const Site& s = g.world.sites[(size_t)si];
  int lord = -1;
  for (int b = s.bldgFirst; b < s.bldgFirst + s.bldgCount && b < (int)g.world.over.bldgs.size(); b++) {
    const Bldg& B = g.world.over.bldgs[(size_t)b];
    if (bldgIsRoyalSeat(B)) return b;
    if (lord < 0 && (B.civic & bld::CIVIC_SEAT)) lord = b;
  }
  if (lord >= 0) return lord;
  for (int b = s.bldgFirst; b < s.bldgFirst + s.bldgCount && b < (int)g.world.over.bldgs.size(); b++)
    if (bldgIsSeat(g.world.over.bldgs[(size_t)b])) return b;
  // a town or village has no seat of power: its elders' council hall, its gathering place, its chief temple, its inn
  static const uint8_t order[] = {bld::CIVIC_INSTITUTION, bld::CIVIC_GATHERING, bld::CIVIC_SACRED};
  for (uint8_t bit : order)
    for (int b = s.bldgFirst; b < s.bldgFirst + s.bldgCount && b < (int)g.world.over.bldgs.size(); b++) {
      const Bldg& B = g.world.over.bldgs[(size_t)b];
      if ((B.civic & bit) && (bit != bld::CIVIC_INSTITUTION || B.type == art::Building::CouncilHall)) return b;
    }
  for (int b = s.bldgFirst; b < s.bldgFirst + s.bldgCount && b < (int)g.world.over.bldgs.size(); b++)
    if (g.world.over.bldgs[(size_t)b].type == art::Building::Inn) return b;
  return -1;
}

bool cmdSociety(ScriptCtx& c) {
  const cult::Culture* k = hereCulture(c.game);
  if (!k) { std::printf("script: society: no culture here\n"); return true; }
  const cult::Society S = cult::societyOf(*k);
  std::printf("script: society %s (%s): %s, seat %s (%s), ruler %s, lord %s, gathering %s / %s, justice %d law %d crime %d, inheritance %d, "
              "military %d, trade %d%s, classes %03x institutions %03x\n",
              k->name.c_str(), cult::archetypeName(k->archetype), cult::governmentName(S.government), cult::seatName(S.seat), S.seatTitle,
              S.rulerTitle, S.lordTitle, cult::gatheringName(S.gathering), cult::gatheringName(S.gathering2), (int)S.justice, (int)S.lawStrict,
              (int)S.crimeTolerance, (int)S.inheritance, (int)S.military, (int)S.trade, S.nomadic ? ", nomadic" : "", (unsigned)S.classes,
              (unsigned)S.institutions);
  const int si = hereSettlement(c.game), sb = seatOf(c.game, si);
  if (sb >= 0) {
    const Bldg& B = c.game.world.over.bldgs[(size_t)sb];
    std::printf("script: society: %s's seat is %s (%s form %d, %d storeys, %dx%d) at %d,%d\n", c.game.world.sites[(size_t)si].name.c_str(),
                bldgTypeName(B.type), bldgIsRoyalSeat(B) ? "royal" : "lord's", (int)B.form, (int)B.storeys, B.r.w, B.r.h,
                c.game.world.ox + B.doorX(), c.game.world.oy + B.doorY());
  }
  return true;
}

// gotoseat [south N]: stand before the door of the seat of power of the settlement here (or the nearest), N tiles south
// of it (default 3), facing it. Waits (a few frames) for the settlement's buildings to load after a teleport.
bool cmdGotoSeat(ScriptCtx& c) {
  Game& g = c.game;
  if (g.inside) g.debugLeave();
  const int si = hereSettlement(g), sb = seatOf(g, si);
  if (sb < 0) {
    if (c.waited < 3.0f) return false;
    c.fail("gotoseat: no settlement with a seat of power near");
    return true;
  }
  const int south = c.arg(1) == "south" && !c.arg(2).empty() ? std::max(1, std::atoi(c.arg(2).c_str())) : 3;
  const Bldg& B = g.world.over.bldgs[(size_t)sb];
  const std::string name = g.world.sites[(size_t)si].name;
  // (fixer round 3) on the door's own column: where the tile N south of the door is taken (a lamp, a statue, a pool on
  // the forecourt's axis) the teleport nudged the player a column aside and walking north met the facade. The nearest
  // open tile on the door column instead (nearer the door first, then further out)
  int dy = south;
  {
    const Map& m = g.world.over;
    auto open = [&](int x, int y) { return m.in(x, y) && !m.solid[(size_t)y * m.w + x] && !groundSolid(m.at(x, y)) && m.bldgAt[(size_t)y * m.w + x] < 0; };
    if (!open(B.doorX(), B.doorY() + south)) {
      for (int k = 1; k <= 8; k++) {
        if (south - k >= 1 && open(B.doorX(), B.doorY() + south - k)) { dy = south - k; break; }
        if (open(B.doorX(), B.doorY() + south + k)) { dy = south + k; break; }
      }
    }
  }
  const int32_t gx = g.world.ox + B.doorX(), gy = g.world.oy + B.doorY() + dy;
  const std::string what = std::string(bldgTypeName(B.type)) + (bldgIsRoyalSeat(B) ? " (royal)" : "");
  g.teleportGlobal(gx, gy);
  g.pl().aim = Vec2(0, -1);
  g.pl().face = 1;
  g.mode = Mode::Play;
  c.view.snap(g);
  std::printf("script: gotoseat -> %s of %s at %d,%d\n", what.c_str(), name.c_str(), gx, gy);
  return true;
}

// seatdoor (debug): the seat's door tile and the tile before it: ground, solid, the building there, and every
// building record whose footprint covers either
bool cmdSeatDoor(ScriptCtx& c) {
  Game& g = c.game;
  const int sb = seatOf(g, hereSettlement(g));
  if (sb < 0) { c.fail("seatdoor: no seat here"); return true; }
  const Map& m = g.world.over;
  const Bldg& B = m.bldgs[(size_t)sb];
  for (int dy = 0; dy <= 1; dy++) {
    const int x = B.doorX(), y = B.doorY() + dy;
    if (!m.in(x, y)) continue;
    const size_t i = (size_t)y * m.w + x;
    std::printf("seatdoor: seat %d tile %d,%d ground %d solid %d bldgAt %d prop %d wall %d\n", sb, x, y, (int)m.ground[i], (int)m.solid[i],
                (int)m.bldgAt[i], (int)m.prop[i], (int)m.wall[i]);
    for (int b = 0; b < (int)m.bldgs.size(); b++) {
      const IRect& r = m.bldgs[(size_t)b].r;
      if (x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h)
        std::printf("seatdoor:   covered by bldg %d type %d rect %d,%d %dx%d door %d,%d\n", b, (int)m.bldgs[(size_t)b].type, r.x, r.y, r.w, r.h,
                    m.bldgs[(size_t)b].doorX(), m.bldgs[(size_t)b].doorY());
    }
  }
  return true;
}

// gotobridge [n] (debug, M3b round 3): stand 3 tiles south of the n-th nearest bridge crossing in the loaded window
// (one per 12 tiles; a stilt town's boardwalks are skipped)
bool cmdGotoBridge(ScriptCtx& c) {
  Game& g = c.game;
  if (g.inside) g.debugLeave();
  const Map& m = g.world.over;
  const int want = c.arg(1).empty() ? 0 : std::max(0, std::atoi(c.arg(1).c_str()));
  const int px = (int)std::floor(g.pl().p.x / 16.0f), py = (int)std::floor(g.pl().p.y / 16.0f);
  struct B { int d, x, y; };
  std::vector<B> bs;
  std::vector<int> taken;   // one per crossing
  for (int y = 1; y < m.h - 1; y++)
    for (int x = 1; x < m.w - 1; x++) {
      if (m.at(x, y) != Ground::Bridge || (m.blendAt(x, y) >> 4) == (Map::BOARDWALK_MARK >> 4)) continue;
      bs.push_back({(x - px) * (x - px) + (y - py) * (y - py), x, y});
    }
  std::sort(bs.begin(), bs.end(), [](const B& a, const B& b) { return a.d < b.d || (a.d == b.d && (a.y < b.y || (a.y == b.y && a.x < b.x))); });
  int k = 0;
  for (const B& b : bs) {
    bool near = false;   // one per crossing (bridges 12 tiles apart)
    for (int t : taken) if (std::abs(t % 4096 - b.x) < 12 && std::abs(t / 4096 - b.y) < 12) near = true;
    if (near) continue;
    taken.push_back(b.y * 4096 + b.x);
    const int si = g.world.siteAt(b.x, b.y, 8);
    if (k++ < want) continue;
    const std::string name = si >= 0 ? g.world.sites[(size_t)si].name : std::string("(wild)");
    const int32_t gx = g.world.ox + b.x, gy = g.world.oy + b.y;
    g.teleportGlobal(gx, gy + 3);
    g.mode = Mode::Play;
    c.view.snap(g);
    std::printf("script: gotobridge -> %s bridge at %d,%d\n", name.c_str(), gx, gy);
    return true;
  }
  {
    int nb = 0;
    for (int y = 0; y < m.h; y++) for (int x = 0; x < m.w; x++) nb += m.at(x, y) == Ground::Bridge;
    std::printf("script: gotobridge: %d bridge tiles in a %dx%d map, %zu near settlements\n", nb, m.w, m.h, bs.size());
  }
  c.fail("gotobridge: no settlement bridge loaded");
  return true;
}

// ---------------------------------------------------------------- (owner 2026-10-06) open fronts
// gotoopen [n] [west|east] [south N]: in the settlement here (or the nearest), the n-th nearest building with an open
// front of 3 or more walk-in bays (a colonnade, an arcade, a veranda); stand N tiles (3) south of its westmost (or
// eastmost) bay, a side bay, never the door column, facing it. Waits for the buildings to load after a teleport.
int g_openCol = -1;   // the global column of the bay gotoopen chose (expect outbay)
bool cmdGotoOpen(ScriptCtx& c) {
  Game& g = c.game;
  if (g.inside) g.debugLeave();
  const int si = hereSettlement(g);
  int want = 0, south = 3;
  bool east = false;
  for (size_t i = 1; i < c.a.size(); i++) {
    if (c.a[i] == "east") east = true;
    else if (c.a[i] == "west") east = false;
    else if (c.a[i] == "south" && i + 1 < c.a.size()) south = std::max(1, std::atoi(c.a[++i].c_str()));
    else want = std::max(0, std::atoi(c.a[i].c_str()));
  }
  struct Cand { float d; int b; };
  std::vector<Cand> cs;
  const Map& m = g.world.over;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  if (si >= 0) {
    const Site& S = g.world.sites[(size_t)si];
    for (int b = S.bldgFirst; b < S.bldgFirst + S.bldgCount && b < (int)m.bldgs.size(); b++) {
      const Bldg& B = m.bldgs[(size_t)b];
      const Bldg::Open& o = bldgOpenFront(B);
      if (!o.open || o.raised) continue;
      int n = 0;
      for (int k = 0; k < 32; k++) n += (int)((o.mask >> k) & 1u);
      if (n < 3) continue;
      cs.push_back({std::hypot(B.doorX() - px, B.doorY() - py), b});
    }
  }
  std::sort(cs.begin(), cs.end(), [](const Cand& a, const Cand& b) { return a.d < b.d || (a.d == b.d && a.b < b.b); });
  int k = 0;
  for (const Cand& cd : cs) {
    const Bldg& B = m.bldgs[(size_t)cd.b];
    const std::vector<int> cols = bldgEntryColumns(B);
    int x = -1;
    for (size_t i = 0; i < cols.size(); i++) {
      const int cx = cols[east ? cols.size() - 1 - i : i];
      if (cx == B.doorX()) break;
      bool clear = true;
      for (int dy = 1; dy <= south; dy++) if (m.blocked(cx, B.doorY() + dy)) clear = false;
      if (clear) { x = cx; break; }
    }
    if (x < 0) continue;
    if (k++ < want) continue;
    const int32_t gx = g.world.ox + x, gy = g.world.oy + B.doorY() + south;
    g_openCol = gx;
    g.teleportGlobal(gx, gy);
    g.pl().aim = Vec2(0, -1);
    g.pl().face = 1;
    g.mode = Mode::Play;
    c.view.snap(g);
    {
      const Bldg::Open& o = bldgOpenFront(B);
      std::string sm;
      for (int k = 0; k < B.r.w && k < 32; k++) { char t[16]; std::snprintf(t, sizeof t, " %04x", (unsigned)o.solid[(size_t)k]); sm += t; }
      std::printf("script: gotoopen: bldg %dx%d at %d,%d mask %08x solid%s\n", B.r.w, B.r.h, B.r.x, B.r.y, (unsigned)o.mask, sm.c_str());
    }
    std::printf("script: gotoopen -> %s (%d entry bays, door column %d) %s bay at column %d, %d tiles south\n", bldgTypeName(B.type), (int)cols.size(),
                g.world.ox + B.doorX(), east ? "east" : "west", gx, south);
    return true;
  }
  if (c.waited < 3.0f) return false;
  c.fail("gotoopen: no open-fronted building with a free side bay near");
  return true;
}
// expect bay side|door: inside, the player stands just inside one of the ground floor's open bays (floor 0, the row inside
// the threshold: where entering puts you): a side bay (not the door column) or the door column
bool expBay(ScriptCtx& c) {
  Game& g = c.game;
  if (!g.inside || g.subBldg < 0) { c.fail("expect bay: not inside a building"); return true; }
  const Map& in = g.sub;
  const int tx = (int)std::floor(g.pl().p.x / TILE), ty = (int)std::floor(g.pl().p.y / TILE);
  // (fixer r2) just inside: on the ground floor, on the row inside the threshold (where entering puts you), in a bay
  const bool row = g.subFloor == 0 && ty == in.exitY - 1;
  const bool bay = row && in.isExit(tx, in.exitY) && !in.exits.empty();
  const bool side = bay && tx != in.exitX;
  if (c.arg(2) == "side" ? !side : !(bay && tx == in.exitX))
    c.fail("expect bay " + c.arg(2) + ": the player is at column " + std::to_string(tx) + ", row " + std::to_string(ty) + ", floor " +
           std::to_string(g.subFloor) + " (door column " + std::to_string(in.exitX) + ", inside row " + std::to_string(in.exitY - 1) + ", " +
           std::to_string(in.exits.size()) + " bays)");
  return true;
}
// expect outbay: outdoors again, standing before the bay gotoopen chose (out the way the player came in)
bool expOutBay(ScriptCtx& c) {
  Game& g = c.game;
  const int gx = g.world.ox + (int)std::floor(g.pl().p.x / TILE);
  if (g.inside || gx != g_openCol) c.fail("expect outbay: " + std::string(g.inside ? "still inside" : "outside at column ") + (g.inside ? "" : std::to_string(gx)) + ", came in at " + std::to_string(g_openCol));
  return true;
}

// enterseat [floor]: enter the seat of power of the settlement here (or the nearest): its palace, great hall, tent
// court, guildhall, temple complex... whatever the society built (fixer r2: "enter palace" takes the nearest Palace)
bool cmdEnterSeat(ScriptCtx& c) {
  Game& g = c.game;
  if (g.inside) g.debugLeave();
  const int si = hereSettlement(g);
  const int b = seatOf(g, si);
  if (b < 0) { c.fail("enterseat: no seat of power here"); return true; }
  const int fl = std::atoi(c.arg(1).c_str());
  const Bldg& B = g.world.over.bldgs[(size_t)b];
  if (fl < 0 || fl >= B.floors() || !g.debugEnterBuilding(b, fl)) { c.fail("enterseat: could not enter floor " + c.arg(1)); return true; }
  g.mode = Mode::Play;
  c.view.snap(g);
  std::printf("script: enterseat -> %s of %s, floor %d\n", bldgTypeName(B.type), g.world.sites[(size_t)si].name.c_str(), fl);
  return true;
}
// expect seat <purpose>: the seat of power of the settlement here is built as that purpose (palace, keep, guildhall,
// meadhall, temple, councilhall, ...: the `enter` names)
bool expSeat(ScriptCtx& c) {
  static const struct { const char* n; art::Building b; } et[] = {
      {"palace", art::Building::Palace}, {"keep", art::Building::Keep}, {"temple", art::Building::Temple}, {"guildhall", art::Building::Guildhall},
      {"meadhall", art::Building::MeadHall}, {"councilhall", art::Building::CouncilHall}, {"lodge", art::Building::Lodge},
      {"exchange", art::Building::Exchange}, {"inn", art::Building::Inn}};
  int want = -1;
  for (auto& e : et) if (c.arg(2) == e.n) want = (int)e.b;
  if (want < 0) { c.fail("expect seat <palace|keep|temple|guildhall|meadhall|councilhall|...>"); return true; }
  const int sb = seatOf(c.game, hereSettlement(c.game));
  if (sb < 0) { c.fail("expect seat: no seat of power here"); return true; }
  const Bldg& B = c.game.world.over.bldgs[(size_t)sb];
  if ((int)B.type != want) c.fail("expect seat " + c.arg(2) + ": the seat here is " + std::string(bldgTypeName(B.type)));
  return true;
}

}  // namespace

EMB_SCRIPT_CMD("society", "society: print the society of the culture here and its settlement's seat of power (M3b)", cmdSociety);
EMB_SCRIPT_CMD("seatdoor", "seatdoor: print the seat of power's door tile, the tile before it and every building covering them (debug, M3b)", cmdSeatDoor);
EMB_SCRIPT_CMD("gotobridge", "gotobridge [n]: stand south of the n-th nearest bridge crossing in the loaded window (debug, M3b)", cmdGotoBridge);
EMB_SCRIPT_CMD("gotoopen", "gotoopen [n] [west|east] [south N]: stand N tiles (3) south of a side bay of the n-th nearest open-fronted building (3+ bays) here (owner 2026-10-06)", cmdGotoOpen);
EMB_SCRIPT_CMD("expect:bay", "expect bay side|door: inside, standing in an open front's side bay (not the door column) or its door column", expBay);
EMB_SCRIPT_CMD("expect:outbay", "expect outbay: outdoors, before the bay gotoopen chose", expOutBay);
EMB_SCRIPT_CMD("enterseat", "enterseat [floor]: enter the seat of power of the settlement here or nearest (fixer r2)", cmdEnterSeat);
EMB_SCRIPT_CMD("gotoseat", "gotoseat [south N]: stand N tiles (3) before the seat of power of the settlement here or nearest (M3b)", cmdGotoSeat);
EMB_SCRIPT_CMD("expect:seat", "expect seat <purpose>: the seat of power here is built as that purpose (M3b)", expSeat);
EMB_SCRIPT_CMD("gotoculture", "gotoculture <archetype|other> [city|capital|town|village|any] [n]: stand south of the nearest settlement of that culture (M3)", cmdGotoCulture);
EMB_SCRIPT_CMD("culture", "culture: print the culture where the player stands (M3)", cmdCulture);
EMB_SCRIPT_CMD("expect:culture", "expect culture <archetype>: the culture here has that archetype (M3)", expCulture);
