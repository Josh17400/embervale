// rpg_test lane checks: city walls, gates, building footprints and architecture styles. M0 architecture lane.
// Called once per seed after runSeed; returns failures, reports each with out("FAIL: ...").
// M2: on the endless world (the classic island is retired): the start window, then the nearest city brought into the
// window (its wall ring, gates and streets); only places lying wholly inside the window are checked.
//   ARCH_DUMP=1 rpg_test 7     also prints every city's wall ring as ASCII (# wall, G gate, = gap, . walkable, ~ blocked)
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <map>
#include <tuple>
#include <vector>
#include "rpg/culture/style.h"
#include "tools/tests/tests.h"

namespace {

// the town's buffer is the site rect grown by ew::town::MARGIN (8), and the wall ring may reach into it (where the land
// or the ring's wobble bulges it out): check one tile beyond
constexpr int MG = 9;

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
      // (M2: the flood stops at the town's buffer, which the ring may reach into; rpg_test --towns checks the ring
      // exactly on the generator's own buffer)
      if (w.genVersion >= WORLDGEN_V3) { out("FAIL: city %d %s: the wall ring flood left the town's buffer at %d,%d\n", si, s.name.c_str(), ex, ey); bad++; }
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
        else if (m.at(x, y) == Ground::Road) c = 'R';
        else if (m.at(x, y) == Ground::Plaza) c = 'P';
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
  Game g(seed);
  g.newEndlessGame(seed);
  {
    const Site& home = g.world.sites[(size_t)g.world.startSite];
    const int city = g.world.findSiteNear(g.world.ox + home.ex, g.world.oy + home.ey, SiteType::City, 8);
    if (city >= 0) g.teleportGlobal(g.world.ox + g.world.sites[(size_t)city].ex, g.world.oy + g.world.sites[(size_t)city].ey + 2);
    else out("WARN: arch: no city within 8 regions of the start\n");
  }
  const World& w = g.world;
  const Map& m = w.over;
  auto inWindow = [&](const IRect& r, int pad) { return m.in(r.x - pad, r.y - pad) && m.in(r.x + r.w - 1 + pad, r.y + r.h - 1 + pad); };
  int cities = 0;
  for (int si = 0; si < (int)w.sites.size(); si++)
    if (w.sites[si].type == SiteType::City && inWindow(w.sites[si].r, MG)) { bad += wallChecks(w, si, dump); cities++; }
  if (!cities) out("WARN: arch: no city wholly inside the window\n");
  // M3 (owner carry-over 4): a city's same-size houses must not share one facade. rpg_test does not link the painters,
  // so this holds the STYLES apart (ArchStyle::key, which covers the window / door shapes, the facade variant bits,
  // ornaments and tints); arch_gallery --check paints them and holds >= 70 % of the painted sprites distinct.
  for (int si = 0; si < (int)w.sites.size(); si++) {
    const Site& s = w.sites[si];
    if (s.type != SiteType::City || !inWindow(s.r, 0)) continue;
    std::map<std::tuple<int, int, int>, std::vector<uint64_t>> groups;
    for (int b = s.bldgFirst; b < s.bldgFirst + s.bldgCount; b++) {
      const Bldg& B = m.bldgs[(size_t)b];
      if (B.type != art::Building::House) continue;
      groups[{B.r.w, B.r.h, (int)B.storeys}].push_back(bldgArch(B).key());
    }
    int n = 0, distinct = 0;
    for (auto& kv : groups) {
      if (kv.second.size() < 2) continue;
      std::sort(kv.second.begin(), kv.second.end());
      n += (int)kv.second.size();
      distinct += (int)(std::unique(kv.second.begin(), kv.second.end()) - kv.second.begin());
    }
    if (!n) continue;
    out("  city %d %s: %d same-size houses, %d distinct facade styles (%.0f%%)\n", si, s.name.c_str(), n, distinct, 100.0 * distinct / n);
    if (distinct * 2 < n) { out("FAIL: city %d %s: only %d of %d same-size houses have their own facade style\n", si, s.name.c_str(), distinct, n); bad++; }
  }
  // building footprints stay on their own tiles
  for (size_t bi = 0; bi < m.bldgs.size(); bi++) {
    const Bldg& b = m.bldgs[bi];
    if (!inWindow(b.r, 0)) continue;
    for (int y = b.r.y; y < b.r.y + b.r.h; y++)
      for (int x = b.r.x; x < b.r.x + b.r.w; x++)
        if (m.bldgAt[(size_t)y * m.w + x] != (int)bi) { out("FAIL: building %zu footprint tile %d,%d not owned\n", bi, x, y); bad++; y = 1 << 20; break; }
  }
  // M0b (WORLDGEN_V7): the storeys and hearths the generator decides are ones the exterior can paint (VISION_PLAN 15.7:
  // the outside and the inside agree): inns and keeps 2, mage towers 3, houses, stone houses and shops 1 or 2 (narrow
  // houses 1), everything else 1; temples and towers burn no hearth. arch_gallery --check paints them all.
  if (w.genVersion >= WORLDGEN_V7) {
    int n2 = 0;
    for (size_t bi = 0; bi < m.bldgs.size(); bi++) {
      const Bldg& b = m.bldgs[bi];
      int s = b.storeys, lo = 1, hi = 1;
      switch (b.type) {
        case art::Building::Inn: case art::Building::Keep: lo = hi = 2; break;
        case art::Building::Tower: lo = hi = 3; break;
        case art::Building::Palace: case art::Building::Barracks: case art::Building::Windmill: lo = hi = 2; break;   // (M1 types)
        case art::Building::Bakery: case art::Building::Butcher: case art::Building::Fishmonger: case art::Building::Weaver: hi = b.r.w >= 4 ? 2 : 1; break;
        case art::Building::House: hi = b.r.w >= 4 ? 2 : 1; break;
        case art::Building::StoneHouse: hi = 2; break;
        case art::Building::Shop: hi = b.r.w >= 4 ? 2 : 1; break;
        default: break;
      }
      if (s < lo || s > hi) { out("FAIL: building %zu (type %d, %d wide) has %d storeys, the exterior shows %d..%d\n", bi, (int)b.type, b.r.w, s, lo, hi); bad++; }
      if (b.hearth && (b.type == art::Building::Temple || b.type == art::Building::Tower)) { out("FAIL: building %zu (type %d) has a hearth\n", bi, (int)b.type); bad++; }
      if (b.floors() != s) { out("FAIL: building %zu: %d floors inside, %d storeys outside\n", bi, b.floors(), s); bad++; }
      if (s >= 2 && b.type != art::Building::Inn && b.type != art::Building::Keep) n2++;
    }
    out("  M0b: %d houses, stone houses and shops of 2 storeys\n", n2);
  }
  // WORLDGEN_V5 placement: no building's sprite covers another's front (foundation row and doorstep) or its door
  // apron, no market stall stands against a facade or under a roof, no gatehouse stands by a river, and no road runs
  // up to the city wall away from an opening
  if (w.genVersion >= WORLDGEN_V5) {
    auto spriteOf = [&](const Bldg& b) { int up = w.genVersion >= WORLDGEN_V7 ? bldgRiseTiles(b.type, b.storeys) : bldgRiseTiles(b.type); return IRect{b.r.x - 1, b.r.y - up, b.r.w + 2, b.r.h + up + 1}; };
    int clashes = 0;
    for (const Site& s : w.sites) {
      if (!inWindow(s.r, 0)) continue;
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
      if (s.type != SiteType::City || !inWindow(s.r, MG)) continue;
      // (M2) the ring only: the palace compound's own wall (a rectangle round the palace) is not the city's wall, and
      // streets may run past it
      IRect compound{0, 0, 0, 0};
      for (int b = s.bldgFirst; b < s.bldgFirst + s.bldgCount; b++)
        if (m.bldgs[(size_t)b].type == art::Building::Palace) {
          const IRect& p = m.bldgs[(size_t)b].r;
          compound = IRect{p.x - 6, p.y - 4, p.w + 12, 23};
        }
      auto ringWall = [&](int x, int y) {
        if (!m.in(x, y) || !m.wall[(size_t)y * m.w + x]) return false;
        return !(compound.w && x >= compound.x && y >= compound.y && x < compound.x + compound.w && y < compound.y + compound.h);
      };
      auto roadG = [&](int x, int y) { const Ground q = m.at(x, y); return m.in(x, y) && (q == Ground::Road || q == Ground::Bridge); };
      auto paved = [&](int x, int y) { const Ground q = m.at(x, y); return m.in(x, y) && (q == Ground::Road || q == Ground::Bridge || q == Ground::Plaza); };
      int heads = 0;
      for (int y = s.r.y - MG; y < s.r.y + s.r.h + MG; y++)
        for (int x = s.r.x - MG; x < s.r.x + s.r.w + MG; x++) {
          if (!m.in(x, y) || m.wall[(size_t)y * m.w + x] || !roadG(x, y)) continue;
          bool nearOpening = false;
          for (const IRect& g : w.wallGaps) if (x >= g.x - 3 && x < g.x + g.w + 3 && y >= g.y - 3 && y < g.y + g.h + 3) nearOpening = true;
          if (nearOpening) continue;
          // a road that meets the ring head-on: the wall ahead, and behind it a run of road at least three tiles long
          // that is a road (nothing paved either side of the run), not a square or a lane running along the wall's
          // foot. Outside the ring it is an overland road that misses the gates; inside, a street that dead-ends.
          for (int k = 0; k < 4; k++) {
            static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            if (!ringWall(x + dx[k], y + dy[k])) continue;
            bool run = true;
            for (int t = 0; t < 3 && run; t++) {
              const int bx = x - dx[k] * t, by = y - dy[k] * t;
              if (!roadG(bx, by)) run = false;
              else if (paved(bx + dy[k], by + dx[k]) && paved(bx - dy[k], by - dx[k])) run = false;   // as wide as a square
            }
            // a road two tiles wide (a highway) counts as one road: its twin beside it is road too
            if (!run) continue;
            // a road that turns along the wall here (road on either side of its end) is no dead end
            if (roadG(x + dy[k], y + dx[k]) || roadG(x - dy[k], y - dx[k])) {
              bool twin = false;   // (unless that side tile is only the highway's twin lane, running into the wall too)
              for (int sd : {1, -1}) {
                const int sx = x + dy[k] * sd, sy = y + dx[k] * sd;
                if (roadG(sx, sy) && ringWall(sx + dx[k], sy + dy[k]) && roadG(sx - dx[k], sy - dy[k]) && roadG(sx - 2 * dx[k], sy - 2 * dy[k])) twin = true;
              }
              if (!twin) continue;
            }
            bool sideRun = true;
            for (int t = 0; t < 3 && sideRun; t++)
              if (!paved(x - dx[k] * t + dy[k], y - dy[k] * t + dx[k]) && !paved(x - dx[k] * t - dy[k], y - dy[k] * t - dx[k])) sideRun = false;
            (void)sideRun;
            // a road running along the wall's foot (a ring road beside a diagonal stretch of wall touches it tile after
            // tile, stepping like the wall does): many road tiles touch the wall round here, so this is no dead end
            int along = 0;
            for (int oy = -3; oy <= 3; oy++)
              for (int ox = -3; ox <= 3; ox++) {
                const int qx = x + ox, qy = y + oy;
                if (!roadG(qx, qy) || m.wall[(size_t)qy * m.w + qx]) continue;
                bool t = false;
                for (int k2 = 0; k2 < 4 && !t; k2++) t = ringWall(qx + dx[k2], qy + dy[k2]);
                if (t) along++;
              }
            if (along >= 5) continue;
            // the edge of a wide paved expanse (a district's yards and lanes run together, a square by the wall) that
            // brushes the wall is no road running into it either: a road is at most two tiles wide
            int paveN = 0;
            for (int oy = -3; oy <= 3; oy++)
              for (int ox = -3; ox <= 3; ox++) if (paved(x + ox, y + oy)) paveN++;
            if (paveN > 16) continue;
            heads++;
            out("FAIL: city %s: a road meets the wall head-on at %d,%d (global %d,%d), away from any opening\n", s.name.c_str(), x, y, w.ox + x, w.oy + y);
            bad++;
            for (int oy = -4; oy <= 4; oy++) {   // # wall, R road, P paving, B bridge, ~ water, b building, . other
              std::string row;
              for (int ox = -6; ox <= 6; ox++) {
                int qx = x + ox, qy = y + oy;
                char ch = '.';
                if (!m.in(qx, qy)) ch = ' ';
                else if (m.wall[(size_t)qy * m.w + qx]) ch = '#';
                else if (m.bldgAt[(size_t)qy * m.w + qx] >= 0) ch = 'b';
                else if (m.at(qx, qy) == Ground::Road) ch = 'R';
                else if (m.at(qx, qy) == Ground::Plaza) ch = 'P';
                else if (m.at(qx, qy) == Ground::Bridge) ch = 'B';
                else if (groundWater(m.at(qx, qy))) ch = '~';
                if (ox == 0 && oy == 0) ch = '@';
                row += ch;
              }
              out("      %s\n", row.c_str());
            }
            break;
          }
        }
      (void)heads;
    }
  }
  // settlements per biome (for screenshots: --play --seed N --goto village picks the village nearest the start)
  if (!dump) return bad;
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
