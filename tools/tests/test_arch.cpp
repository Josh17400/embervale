// rpg_test lane checks: city walls, gates, building footprints and architecture styles. M0 architecture lane.
// Called once per seed after runSeed; returns failures, reports each with out("FAIL: ...").
//   ARCH_DUMP=1 rpg_test 7     also prints every city's wall ring as ASCII (# wall, G gate, = gap, . walkable, ~ blocked)
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "rpg/culture/style.h"
#include "tools/tests/tests.h"

namespace {

int wallChecks(const World& w, int si, bool dump) {
  const Map& m = w.over;
  const Site& s = w.sites[si];
  int bad = 0;
  auto W = [&](int x, int y) { return m.in(x, y) && m.wall[(size_t)y * m.w + x] != 0; };
  // gate passages and wall gaps of this city
  std::vector<uint8_t> gap((size_t)m.w * m.h, 0);
  auto inSite = [&](int x, int y) { return x >= s.r.x - 2 && y >= s.r.y - 2 && x < s.r.x + s.r.w + 2 && y < s.r.y + s.r.h + 2; };
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
  for (int y = s.r.y - 2; y < s.r.y + s.r.h + 2; y++)
    for (int x = s.r.x - 2; x < s.r.x + s.r.w + 2; x++) {
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
      if (!inSite(x, y) || x == s.r.x - 2 || y == s.r.y - 2 || x == s.r.x + s.r.w + 1 || y == s.r.y + s.r.h + 1) { escaped = true; ex = x; ey = y; break; }
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
    for (int y = s.r.y - 2; y < s.r.y + s.r.h + 2; y++) {
      std::string line;
      for (int x = s.r.x - 2; x < s.r.x + s.r.w + 2; x++) {
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
