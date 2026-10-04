// rpg_test lane checks: city walls, gates, building footprints and architecture styles. M0 architecture lane.
// Called once per seed after runSeed; returns failures, reports each with out("FAIL: ...").
//   ARCH_DUMP=1 rpg_test 7     also prints every city's wall ring as ASCII (# wall, G gate, = gap, . walkable, ~ blocked)
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "rpg/culture/style.h"
#include "tools/tests/tests.h"

namespace {

// the town box (Town::x0..) is the site rect grown by 3, and the wall ring can reach its edge: check one tile beyond
constexpr int MG = 4;

int wallChecks(const World& w, int si, bool dump) {
  const Map& m = w.over;
  const Site& s = w.sites[si];
  int bad = 0;
  auto W = [&](int x, int y) { return m.in(x, y) && m.wall[(size_t)y * m.w + x] != 0; };
  // gate passages and wall gaps of this city
  std::vector<uint8_t> gap((size_t)m.w * m.h, 0);
  auto inSite = [&](int x, int y) { return x >= s.r.x - MG && y >= s.r.y - MG && x < s.r.x + s.r.w + MG && y < s.r.y + s.r.h + MG; };
  for (const IRect& r : w.wallGaps)
    for (int y = r.y; y < r.y + r.h; y++)
      for (int x = r.x; x < r.x + r.w; x++)
        if (m.in(x, y)) gap[(size_t)y * m.w + x] = 1;
  int nGates = 0;
  for (auto& gt : w.gates) {
    if (!inSite(gt.first, gt.second)) continue;
    nGates++;
    for (int k = 0; k < 3; k++) gap[(size_t)gt.second * m.w + gt.first + k] = 2;
    // the gate can be walked through: its three tiles and the middle tile's approaches are open
    for (int k = 0; k < 3; k++)
      if (m.blocked(gt.first + k, gt.second)) { out("FAIL: city %d gate at %d,%d: passage tile %d blocked\n", si, gt.first, gt.second, k); bad++; }
    if (m.blocked(gt.first + 1, gt.second - 1) || m.blocked(gt.first + 1, gt.second + 1)) {
      out("FAIL: city %d gate at %d,%d: approach blocked\n", si, gt.first, gt.second); bad++;
    }
    // flanking towers stand on wall
    if (w.genVersion >= WORLDGEN_V3 && (!W(gt.first - 1, gt.second) || !W(gt.first + 3, gt.second))) {
      out("FAIL: city %d gate at %d,%d: no wall under a flanking tower\n", si, gt.first, gt.second); bad++;
    }
  }
  auto G = [&](int x, int y) { return m.in(x, y) ? gap[(size_t)y * m.w + x] : 0; };
  int walls = 0, lonely = 0;
  for (int y = s.r.y - MG; y < s.r.y + s.r.h + MG; y++)
    for (int x = s.r.x - MG; x < s.r.x + s.r.w + MG; x++) {
      if (!W(x, y)) continue;
      walls++;
      int n = 0;
      for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if ((ox || oy) && W(x + ox, y + oy)) n++;
      bool jamb = G(x - 1, y) || G(x + 1, y) || G(x, y - 1) || G(x, y + 1);
      if (n < 2 && !jamb) {
        lonely++;
        if (w.genVersion >= WORLDGEN_V3) { out("FAIL: city %d wall tile %d,%d has %d wall neighbours\n", si, x, y, n); bad++; }
      }
    }
  if (nGates == 0) { out("FAIL: city %d has no gate\n", si); bad++; }
  // closed ring: a flood from the keep's door over walkable tiles, with gates and gaps blocked, stays inside
  int start = -1;
  for (int b = s.bldgFirst; b < s.bldgFirst + s.bldgCount; b++)
    if (m.bldgs[b].type == art::Building::Keep) start = b;
  if (start >= 0 && walls > 0) {
    const Bldg& kb = m.bldgs[start];
    std::vector<uint8_t> seen((size_t)m.w * m.h, 0);
    std::vector<int> q{kb.doorY() * m.w + kb.doorX() + m.w};
    seen[q[0]] = 1;
    bool escaped = false;
    int ex = -1, ey = -1;
    for (size_t qi = 0; qi < q.size() && !escaped; qi++) {
      int x = q[qi] % m.w, y = q[qi] / m.w;
      if (!inSite(x, y) || x == s.r.x - MG || y == s.r.y - MG || x == s.r.x + s.r.w + MG - 1 || y == s.r.y + s.r.h + MG - 1) { escaped = true; ex = x; ey = y; break; }
      static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
      for (int k = 0; k < 4; k++) {
        int nx = x + dx[k], ny = y + dy[k];
        if (!m.in(nx, ny) || seen[(size_t)ny * m.w + nx]) continue;
        if (W(nx, ny) || G(nx, ny)) continue;
        if (groundSolid(m.at(nx, ny))) continue;   // water and rock close a ring too
        seen[(size_t)ny * m.w + nx] = 1;
        q.push_back(ny * m.w + nx);
      }
    }
    if (escaped) {
      if (w.genVersion >= WORLDGEN_V3) { out("FAIL: city %d wall ring leaks (reached %d,%d)\n", si, ex, ey); bad++; }
      else out("  city %d (v%d) wall ring leaks at %d,%d\n", si, w.genVersion, ex, ey);
    }
  }
  out("  city %d %s: %d wall tiles, %d gates, %d lonely tiles\n", si, s.name.c_str(), walls, nGates, lonely);
  for (auto& gt : w.gates)
    if (inSite(gt.first, gt.second)) out("    gate at %d,%d (walk through: %d,%d -> %d,%d)\n", gt.first, gt.second, gt.first + 1, gt.second + 2, gt.first + 1, gt.second - 2);
  if (dump) {
    for (int y = s.r.y - MG; y < s.r.y + s.r.h + MG; y++) {
      std::string line;
      for (int x = s.r.x - MG; x < s.r.x + s.r.w + MG; x++) {
        char c = '.';
        if (W(x, y)) c = '#';
        else if (G(x, y) == 2) c = 'G';
        else if (G(x, y)) c = '=';
        else if (m.bldgAt[(size_t)y * m.w + x] >= 0) c = 'b';
        else if (m.blocked(x, y)) c = '~';
        line += c;
      }
      out("    %s\n", line.c_str());
    }
  }
  return bad;
}

}  // namespace

int archChecks(uint64_t seed) {
  int bad = 0;
  bool dump = getenv("ARCH_DUMP") != nullptr;
  World w;
  w.generate(seed, WORLDGEN_LATEST);
  const Map& m = w.over;
  for (int si = 0; si < (int)w.sites.size(); si++)
    if (w.sites[si].type == SiteType::City) bad += wallChecks(w, si, dump);
  // building footprints stay inside the map and on their own tiles
  for (size_t bi = 0; bi < m.bldgs.size(); bi++) {
    const Bldg& b = m.bldgs[bi];
    for (int y = b.r.y; y < b.r.y + b.r.h; y++)
      for (int x = b.r.x; x < b.r.x + b.r.w; x++)
        if (!m.in(x, y) || m.bldgAt[(size_t)y * m.w + x] != (int16_t)bi) { out("FAIL: building %zu footprint tile %d,%d not owned\n", bi, x, y); bad++; y = 1 << 20; break; }
  }
  // WORLDGEN_V5 placement: no building's sprite covers another's front (foundation row and doorstep) or its door
  // apron, no market stall stands against a facade or under a roof, no gatehouse stands by a river, and no road runs
  // up to the city wall away from an opening
  if (w.genVersion >= WORLDGEN_V5) {
    auto spriteOf = [](const Bldg& b) { int up = bldgRiseTiles(b.type); return IRect{b.r.x - 1, b.r.y - up, b.r.w + 2, b.r.h + up + 1}; };
    int clashes = 0;
    for (const Site& s : w.sites) {
      for (int i = s.bldgFirst; i < s.bldgFirst + s.bldgCount; i++)
        for (int j = s.bldgFirst; j < s.bldgFirst + s.bldgCount; j++) {
          if (i == j) continue;
          const Bldg& A = m.bldgs[(size_t)i];
          const Bldg& B = m.bldgs[(size_t)j];
          IRect front{B.r.x, B.r.y + B.r.h - 1, B.r.w, 1}, apron{B.doorX() - 1, B.r.y + B.r.h, 3, 2};
          if (spriteOf(A).overlaps(front) || spriteOf(A).overlaps(apron)) {
            if (clashes++ < 5) out("FAIL: %s: building %d (type %d at %d,%d) covers the front of building %d (type %d at %d,%d)\n", s.name.c_str(), i,
                                   (int)A.type, A.r.x, A.r.y, j, (int)B.type, B.r.x, B.r.y);
            bad++;
          }
        }
    }
    for (int y = 2; y < m.h - 4; y++)
      for (int x = 1; x < m.w - 1; x++) {
        if (m.prop[(size_t)y * m.w + x] != (int)art::Prop::MarketStall + 1) continue;
        for (int oy = -2; oy <= 3; oy++)
          for (int ox = -1; ox <= 1; ox++)
            if (m.bldgAt[(size_t)(y + oy) * m.w + x + ox] >= 0) { out("FAIL: market stall at %d,%d against a building\n", x, y); bad++; oy = 9; break; }
      }
    for (auto& gt : w.gates)
      for (int oy = -2; oy <= 2; oy++)
        for (int ox = -2; ox <= 4; ox++)
          if (m.in(gt.first + ox, gt.second + oy) && (groundWater(m.at(gt.first + ox, gt.second + oy)) || m.at(gt.first + ox, gt.second + oy) == Ground::Bridge)) {
            out("FAIL: gate at %d,%d stands by water (%d,%d)\n", gt.first, gt.second, gt.first + ox, gt.second + oy); bad++; oy = 9; break;
          }
    for (int si = 0; si < (int)w.sites.size(); si++) {
      const Site& s = w.sites[si];
      if (s.type != SiteType::City) continue;
      for (int y = s.r.y - MG; y < s.r.y + s.r.h + MG; y++)
        for (int x = s.r.x - MG; x < s.r.x + s.r.w + MG; x++) {
          if (!m.in(x, y) || m.wall[(size_t)y * m.w + x]) continue;
          Ground gg = m.at(x, y);
          if (gg != Ground::Road && gg != Ground::Bridge) continue;
          bool touches = false, nearOpening = false;
          for (int k = 0; k < 4; k++) {
            static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            if (m.in(x + dx[k], y + dy[k]) && m.wall[(size_t)(y + dy[k]) * m.w + x + dx[k]]) touches = true;
          }
          for (const IRect& g : w.wallGaps) if (x >= g.x - 3 && x < g.x + g.w + 3 && y >= g.y - 3 && y < g.y + g.h + 3) nearOpening = true;
          // (a road may run along the wall's foot; one that meets it head-on - wall ahead, road behind - dead-ends)
          if (touches && !nearOpening) {
            int headOn = 0;
            for (int k = 0; k < 4; k++) {
              static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
              bool wallAhead = m.in(x + dx[k], y + dy[k]) && m.wall[(size_t)(y + dy[k]) * m.w + x + dx[k]];
              Ground back = m.at(x - dx[k], y - dy[k]);
              bool sideRoad = m.at(x + dy[k], y + dx[k]) == Ground::Road || m.at(x - dy[k], y - dx[k]) == Ground::Road;
              if (wallAhead && (back == Ground::Road || back == Ground::Bridge) && !sideRoad) headOn++;
            }
            if (headOn) {
              out("FAIL: city %s: a road meets the wall head-on at %d,%d, away from any opening\n", s.name.c_str(), x, y);
              bad++;
              for (int oy = -4; oy <= 4; oy++) {   // # wall, R road, B bridge, ~ water, b building, . other
                std::string row;
                for (int ox = -6; ox <= 6; ox++) {
                  int qx = x + ox, qy = y + oy;
                  char ch = '.';
                  if (!m.in(qx, qy)) ch = ' ';
                  else if (m.wall[(size_t)qy * m.w + qx]) ch = '#';
                  else if (m.bldgAt[(size_t)qy * m.w + qx] >= 0) ch = 'b';
                  else if (m.at(qx, qy) == Ground::Road) ch = 'R';
                  else if (m.at(qx, qy) == Ground::Bridge) ch = 'B';
                  else if (groundWater(m.at(qx, qy))) ch = '~';
                  if (ox == 0 && oy == 0) ch = '@';
                  row += ch;
                }
                out("      %s\n", row.c_str());
              }
            }
          }
        }
    }
  }
  // settlements per biome (for screenshots: --play --seed N --goto village picks the village nearest the start)
  const Site& home = w.sites[w.startSite];
  Biome hb = m.biomeAt(home.r.cx(), home.r.cy());
  art::ArchStyle st = art::archForBiome((int)hb, 1);
  out("  start village %s at %d,%d: biome %s (roof %d/%d wall %d)\n", home.name.c_str(), home.r.cx(), home.r.cy(), biomeName(hb), (int)st.roof,
      (int)st.roofMat, (int)st.wall);
  int nc = w.nearestSite(home.ex, home.ey, SiteType::City);
  if (nc >= 0) out("  --goto city -> %s (city %d), biome %s\n", w.sites[nc].name.c_str(), nc, biomeName(m.biomeAt(w.sites[nc].r.cx(), w.sites[nc].r.cy())));
  int nt = w.nearestSite(home.ex, home.ey, SiteType::Town);
  if (nt >= 0) out("  --goto town -> %s, biome %s\n", w.sites[nt].name.c_str(), biomeName(m.biomeAt(w.sites[nt].r.cx(), w.sites[nt].r.cy())));
  int nv = w.nearestSite(home.ex, home.ey, SiteType::Village);
  if (nv >= 0) out("  --goto village -> %s, biome %s\n", w.sites[nv].name.c_str(), biomeName(m.biomeAt(w.sites[nv].r.cx(), w.sites[nv].r.cy())));
  for (int si = 0; si < (int)w.sites.size(); si++) {
    const Site& s = w.sites[si];
    if (s.type != SiteType::City && s.type != SiteType::Town && s.type != SiteType::Village) continue;
    out("    %-8s %-14s at %3d,%3d biome %s\n", siteTypeName(s.type), s.name.c_str(), s.r.cx(), s.r.cy(), biomeName(m.biomeAt(s.r.cx(), s.r.cy())));
  }
  return bad;
}
