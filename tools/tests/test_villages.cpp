// rpg_test --villages [--seeds A..B] [--write]: the owner's M7b rule (VISION_PLAN 15.23, 15.24): "villages look great",
// so the M7b city plan (street hierarchy, kerbs, row houses, parks) must NOT change a single village. LEAD-owned and
// FROZEN: no lane edits this file or tests/fixtures/golden_villages.txt (recorded before any M7b generation change).
//
// Two kinds of village are hashed, with this file's own frozen hash (a copy of the M7 townHash, so a lane that adds
// fields to rpg_test --towns cannot move it):
//  - synthetic villages built straight from SettlementCtx (every settlement archetype on plains, river, hill and coast,
//    with and without roads; every culture archetype on its own land; every layout style forced), seeds 1..3 by default;
//  - the real villages of the endless world: every village whose heart lies in the 5 x 5 regions round the origin, as
//    the chunks deliver them (ground, props, walls, relief, biome, paving marks and every building record inside the
//    footprint), seeds 1..3.
// Beyond the hash, every village must keep the M7b layers empty: Map::urb all 0 (no street classes, no kerbs) and no
// Bldg::row party walls.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/world/dmath.h"
#include "rpg/world/settlement.h"
#include "rpg/world/source.h"
#include "rpg/world/town_gen.h"
#include "tools/tests/tests.h"

namespace {

uint64_t fnv(uint64_t h, const void* p, size_t n) {
  const uint8_t* b = (const uint8_t*)p;
  for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; }
  return h;
}
// the M7 townHash, frozen
uint64_t villageHash(const ew::SettlementOut& o) {
  uint64_t h = 1469598103934665603ull;
  const Map& m = o.buf;
  int hd[6] = {m.w, m.h, o.gx, o.gy, o.ex, o.ey};
  h = fnv(h, hd, sizeof hd);
  h = fnv(h, m.ground.data(), m.ground.size());
  h = fnv(h, m.prop.data(), m.prop.size());
  h = fnv(h, m.wall.data(), m.wall.size());
  h = fnv(h, m.biome.data(), m.biome.size());
  h = fnv(h, m.height.data(), m.height.size());
  h = fnv(h, o.used.data(), o.used.size());
  for (const Bldg& b : m.bldgs) {
    uint64_t v[10] = {b.id, (uint64_t)b.type, (uint64_t)b.r.x, (uint64_t)b.r.y, (uint64_t)b.r.w, (uint64_t)b.r.h, b.seed, b.storeys, b.banner, (uint64_t)b.owner};
    h = fnv(h, v, sizeof v);
  }
  for (const Spawn& s : m.spawns) { int v[4] = {s.x, s.y, (int)s.role, s.slot}; h = fnv(h, v, sizeof v); }
  for (auto& g : o.gates) { int v[2] = {g.first, g.second}; h = fnv(h, v, sizeof v); }
  for (auto& g : o.wallGaps) { int v[4] = {g.x, g.y, g.w, g.h}; h = fnv(h, v, sizeof v); }
  for (const ew::PlotPlan& l : o.plots) {
    int64_t v[8] = {(int64_t)l.id, l.gx, l.gy, l.w, l.h, l.flags, l.gateX, l.gateY};
    h = fnv(h, v, sizeof v);
  }
  // the builder's request fields a village building carries (its look): wealth, form, civic, seat, urban, variant
  for (const Bldg& b : m.bldgs) {
    uint8_t v[6] = {b.wealth, b.form, b.civic, b.seat, b.urban, b.variant};
    h = fnv(h, v, sizeof v);
  }
  return h;
}

enum class Land { Plains, River, Hill, Coast };

struct VCase {
  ew::Archetype arch = ew::Archetype::Plain;
  Land land = Land::Plains;
  std::vector<float> roads;
  uint32_t seed = 0;
  int culture = -1, forceLayout = -1;
  Biome bio = Biome::Plains;
};

void buildVillage(const VCase& c, ew::SettlementOut& so) {
  ew::SitePlan p;
  p.type = SiteType::Village;
  p.archetype = c.arch;
  p.seed = c.seed;
  ew::settlementFootprint(p.type, c.arch, p.seed, p.w, p.h);
  p.gx = 1000 - p.w / 2; p.gy = -2000 - p.h / 2; p.ex = 1000; p.ey = -2000;
  p.bldgCap = 400;
  p.name = "TESTHOLD";
  ew::KingdomPlan k;
  k.id = 77; k.name = "TESTMARK"; k.color = rgba(40, 70, 160); k.color2 = rgba(230, 200, 80); k.emblem = (uint8_t)(c.seed % 8);
  ew::SettlementCtx ctx;
  ctx.plan = &p;
  ctx.kingdom = &k;
  cult::Culture K;
  if (c.culture >= 0) {
    K = cult::Atlas::make((cult::Archetype)c.culture, (uint32_t)ew::mix64(c.seed ^ 0xC17u), (int)c.bio);
    if (c.forceLayout >= 0) K.town.layout = K.town.altLayout = (cult::Layout)c.forceLayout;
    ctx.culture = &K;
  }
  ctx.rx = 3; ctx.ry = -8;
  ctx.roadBearings = c.roads;
  const int cxg = 1000, cyg = -2000, hw = p.w / 2;
  const Land land = c.land;
  const Biome open = c.bio;
  ctx.base = [land, cxg, cyg, hw, open](int32_t gx, int32_t gy, Ground& g, Biome& bi, uint8_t& h) {
    g = open == Biome::Desert ? Ground::Sand : (open == Biome::Snow ? Ground::Snow : Ground::Grass); bi = open; h = 1;
    int dx = gx - cxg, dy = gy - cyg;
    switch (land) {
      case Land::River: {
        int rxv = -hw / 3 + (int)(ew::dsin(dy * 0.07f) * 4.0f) + dy / 6;
        if (std::abs(dx - rxv) <= 1) g = Ground::Water;
        else if (std::abs(dx - rxv) == 2) g = Ground::Dirt;
        break;
      }
      case Land::Hill:
        h = (uint8_t)std::clamp(3 - (dy + dx / 2) / 13, 0, 6);
        bi = Biome::Forest;
        if ((gx * 7 + gy * 13) % 17 == 0) g = Ground::ForestFloor;
        break;
      case Land::Coast:
        if (dx > hw * 2 / 3 + (int)(ew::dsin(dy * 0.11f) * 3.0f)) { g = Ground::Water; bi = Biome::Ocean; }
        else if (dx > hw * 2 / 3 - 3) { g = Ground::Sand; bi = Biome::Beach; }
        break;
      default: break;
    }
  };
  so = ew::SettlementOut();
  so.gx = p.gx - ew::town::MARGIN;
  so.gy = p.gy - ew::town::MARGIN;
  ew::buildSettlement(ctx, so);
}

std::vector<VCase> casesFor(uint64_t s) {
  std::vector<VCase> cs;
  Rng r(s * 6151 + 29);
  static const ew::Archetype archs[] = {ew::Archetype::Plain, ew::Archetype::Farming, ew::Archetype::Fishing, ew::Archetype::Port,
                                        ew::Archetype::Mining, ew::Archetype::RiverCrossing, ew::Archetype::HillFort, ew::Archetype::Market};
  auto roads = [&](VCase& c) {
    if (r.irange(3) == 0) return;   // a third without roads
    const int nr = 1 + r.irange(2);
    const float a0 = r.f() * ew::D_TAU;
    for (int k = 0; k < nr; k++) {
      const float jit = r.range(-0.4f, 0.4f);
      c.roads.push_back(ew::dwrap(a0 + k * ew::D_TAU / nr + jit));
    }
  };
  // the classic villages (no culture): every archetype on every land it can stand on
  for (int a = 0; a < 8; a++)
    for (int li = 0; li < 4; li++) {
      VCase c;
      c.arch = archs[a];
      c.land = (Land)li;
      if ((c.arch == ew::Archetype::Fishing || c.arch == ew::Archetype::Port) && c.land != Land::Coast) continue;
      if (c.arch == ew::Archetype::RiverCrossing && c.land != Land::River) continue;
      roads(c);
      c.seed = (uint32_t)ew::mix64(s * 4099 + (uint64_t)a * 31 + (uint64_t)li * 7);
      cs.push_back(c);
    }
  // every culture on its own land, then every layout style forced
  static const Land landOf[12] = {Land::Coast, Land::Plains, Land::Plains, Land::Plains, Land::Plains, Land::Plains,
                                  Land::Plains, Land::Plains, Land::River, Land::Plains, Land::Plains, Land::Plains};
  static const Biome bioOf[12] = {Biome::Taiga, Biome::Forest, Biome::Plains, Biome::Plains, Biome::Desert, Biome::Plains,
                                  Biome::Swamp, Biome::Forest, Biome::Plains, Biome::Forest, Biome::Forest, Biome::Snow};
  static const int byLayout[7] = {2, 3, 5, 8, 4, 7, 6};
  for (int pass = 0; pass < 2; pass++)
    for (int a = 0; a < (pass == 0 ? 12 : 7); a++)
      for (int v = 0; v < 2; v++) {
        VCase c;
        c.culture = pass == 0 ? a : byLayout[a];
        c.forceLayout = pass == 0 ? -1 : a;
        c.arch = archs[(s + (uint64_t)a * 3 + (uint64_t)v * 5 + (uint64_t)pass) % 8];
        c.land = landOf[c.culture];
        c.bio = bioOf[c.culture];
        if (c.arch == ew::Archetype::Fishing || c.arch == ew::Archetype::Port) c.land = Land::Coast;
        if (c.arch == ew::Archetype::RiverCrossing) c.land = Land::River;
        if (c.land == Land::Coast && c.bio == Biome::Desert) c.bio = Biome::Plains;
        roads(c);
        c.seed = (uint32_t)ew::mix64(s * 8191 + (uint64_t)a * 131 + (uint64_t)v * 17 + (uint64_t)pass * 3331 + 5);
        cs.push_back(c);
      }
  return cs;
}

// the M7b layers stay empty in a village
int emptyM7b(const Map& m, const char* what) {
  int bad = 0;
  for (uint8_t u : m.urb)
    if (u) { out("FAIL: villages: %s: a village tile has an urban-fabric class (Map::urb %d)\n", what, u); bad++; break; }
  for (const Bldg& b : m.bldgs)
    if (b.row) { out("FAIL: villages: %s: a village building has party walls (Bldg::row %d)\n", what, b.row); bad++; break; }
  return bad;
}

// a real village of the endless world as the chunks deliver it
uint64_t realVillageHash(ew::EndlessSource& src, const ew::SitePlan& p, int& bad, const std::string& what) {
  uint64_t h = 1469598103934665603ull;
  const int32_t x0 = p.gx - 8, y0 = p.gy - 8, x1 = p.gx + p.w + 8, y1 = p.gy + p.h + 8;
  std::set<ew::Gid> seen;
  ew::ChunkData cd;
  bool urb = false, row = false;
  for (int32_t cy = ew::floorDiv(y0, ew::CHUNK); cy <= ew::floorDiv(y1 - 1, ew::CHUNK); cy++)
    for (int32_t cx = ew::floorDiv(x0, ew::CHUNK); cx <= ew::floorDiv(x1 - 1, ew::CHUNK); cx++) {
      src.chunk(cx, cy, cd);
      int32_t v[2] = {cx, cy};
      h = fnv(h, v, sizeof v);
      for (int ly = 0; ly < ew::CHUNK; ly++)
        for (int lx = 0; lx < ew::CHUNK; lx++) {
          const int32_t gx = cx * ew::CHUNK + lx, gy = cy * ew::CHUNK + ly;
          if (gx < x0 || gy < y0 || gx >= x1 || gy >= y1) continue;
          const int i = cd.at(lx, ly);
          uint8_t t[7] = {cd.ground[i], cd.prop[i], cd.wall[i], cd.height[i], cd.biome[i], cd.blend[i], (uint8_t)(cd.bldg[i] != 0)};
          h = fnv(h, t, sizeof t);
          if (cd.urb[i]) urb = true;
        }
      for (size_t k = 0; k < cd.bldgs.size(); k++) {
        if (cd.bldgSite[k] != p.id || !seen.insert(cd.bldgs[k].id).second) continue;
        const Bldg& b = cd.bldgs[k];
        uint64_t bv[11] = {b.id, (uint64_t)b.type, (uint64_t)(int64_t)b.r.x, (uint64_t)(int64_t)b.r.y, (uint64_t)b.r.w, (uint64_t)b.r.h, b.seed, b.storeys,
                           (uint64_t)b.owner, (uint64_t)b.wealth | ((uint64_t)b.form << 8) | ((uint64_t)b.civic << 16) | ((uint64_t)b.urban << 24),
                           (uint64_t)b.variant};
        h = fnv(h, bv, sizeof bv);
        if (b.row) row = true;
      }
    }
  if (urb) { out("FAIL: villages: %s: a real village tile has an urban-fabric class\n", what.c_str()); bad++; }
  if (row) { out("FAIL: villages: %s: a real village building has party walls\n", what.c_str()); bad++; }
  return h;
}

std::string fixturePath() {
#ifdef EMB_SOURCE_DIR
  return std::string(EMB_SOURCE_DIR) + "/tests/fixtures/golden_villages.txt";
#else
  return "tests/fixtures/golden_villages.txt";
#endif
}

int cmdVillages(int argc, char** argv) {
  uint64_t a = 1, b = 3;
  bool write = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--write")) write = true;
    else if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], a, b);
  }
  int bad = 0, synth = 0, real = 0;
  std::map<std::string, uint64_t> hashes;
  for (uint64_t s = a; s <= b; s++) {
    g_curSeed = s;
    const std::vector<VCase> cs = casesFor(s);
    for (size_t ci = 0; ci < cs.size(); ci++) {
      ew::SettlementOut so;
      buildVillage(cs[ci], so);
      char key[64];
      std::snprintf(key, sizeof key, "village_s%llu_c%zu", (unsigned long long)s, ci);
      hashes[key] = villageHash(so);
      bad += emptyM7b(so.buf, key);
      synth++;
    }
    ew::EndlessSource src(s);
    int n = 0;
    for (int ry = -2; ry <= 2; ry++)
      for (int rx = -2; rx <= 2; rx++) {
        const std::vector<ew::SitePlan> sites = src.region(rx, ry).sites;   // (a copy: chunk() may evict the region cache)
        for (const ew::SitePlan& p : sites) {
          if (p.type != SiteType::Village || p.ex < rx * ew::REGION || p.ey < ry * ew::REGION || p.ex >= (rx + 1) * ew::REGION ||
              p.ey >= (ry + 1) * ew::REGION)
            continue;
          char key[64];
          std::snprintf(key, sizeof key, "real_s%llu_%016llx", (unsigned long long)s, (unsigned long long)p.id);
          hashes[key] = realVillageHash(src, p, bad, key);
          real++;
          n++;
        }
      }
    if (!n) { out("FAIL: villages: seed %llu has no village in the 5x5 regions round the origin\n", (unsigned long long)s); bad++; }
  }
  if (write) {
    printf("# rpg_test --villages golden values (seeds %llu..%llu; rpg_test --villages --write). FROZEN by the owner's M7b rule\n",
           (unsigned long long)a, (unsigned long long)b);
    printf("# (VISION_PLAN 15.23): villages must not change. Never re-record this file to make a change pass.\n");
    for (auto& kv : hashes) printf("%s %016llx\n", kv.first.c_str(), (unsigned long long)kv.second);
    return bad ? 1 : 0;
  }
  FILE* f = fopen(fixturePath().c_str(), "r");
  if (!f) { printf("FAIL: cannot read %s\n", fixturePath().c_str()); return 1; }
  int seen = 0, changed = 0;
  char line[256], key[96];
  unsigned long long val;
  while (fgets(line, sizeof line, f)) {
    if (line[0] == '#' || sscanf(line, "%95s %llx", key, &val) != 2) continue;
    auto it = hashes.find(key);
    if (it == hashes.end()) continue;   // (a narrower --seeds run checks its own keys)
    seen++;
    if (it->second != val) {
      if (changed < 12) printf("FAIL: villages: %s = %016llx, expected %016llx (a village changed)\n", key, (unsigned long long)it->second, val);
      changed++;
    }
  }
  fclose(f);
  if (seen != (int)hashes.size()) { printf("FAIL: villages: the fixture has %d of %zu keys (run with seeds inside 1..3)\n", seen, hashes.size()); bad++; }
  bad += changed;
  printf("villages: %d synthetic and %d real villages, %d changed: %s\n", synth, real, changed, bad ? "FAILED" : "ok");
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--villages", "M7b: every village unchanged (frozen hashes, synthetic and real) and free of city fabric [--seeds A..B] [--write]", cmdVillages);
