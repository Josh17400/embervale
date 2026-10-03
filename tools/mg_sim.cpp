// Headless HOLDLINE checks: world gen, merge rules, save/load, raids, and a bot run.
//   holdline_sim --test
//   holdline_sim [seconds=600] [seed=1]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "game2/mg_bot.h"

static int g_fail = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf("FAIL: %s  (line %d)\n", msg, __LINE__); g_fail++; } else std::printf("ok:   %s\n", msg); } while (0)

// Walk the hero toward a point (straight line, sliding along obstacles).
static Vec2 toward(const MGame& g, Vec2 p) { Vec2 d = p - g.hero.p; return len(d) < 4 ? Vec2() : norm(d); }

static int runTests() {
  {
    MGame a(5), b(5), c(6);
    bool same = a.tiles == b.tiles && a.objs == b.objs;
    bool diff = a.tiles != c.tiles;
    CHECK(same, "same seed generates the same world");
    CHECK(diff, "different seed generates a different world");
    CHECK(a.camps.size() >= 4, "world has at least 4 camps");
    CHECK(!a.solidAt(a.hero.p.x, a.hero.p.y), "hero does not start inside a wall");
    int wild = 0; for (const Unit& u : a.units) if (u.st == UState::Wild) wild++;
    CHECK(wild >= 12, "plenty of wild units to find");
  }
  for (uint64_t sd : {1ull, 2ull, 3ull, 7ull, 42ull, 99ull}) {  // reachability on foot (8-way flood fill over walkable tiles)
    MGame g(sd);
    std::vector<uint8_t> seen((size_t)MAP_W * MAP_H, 0);
    std::vector<int> q;
    int sx = (int)(g.hero.p.x / TILE), sy = (int)(g.hero.p.y / TILE);
    q.push_back(sy * MAP_W + sx); seen[sy * MAP_W + sx] = 1;
    for (size_t h = 0; h < q.size(); h++) {
      int x = q[h] % MAP_W, y = q[h] / MAP_W;
      for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        int nx = x + dx, ny = y + dy;
        if (!MGame::inMap(nx, ny) || seen[ny * MAP_W + nx]) continue;
        if (g.solidAt((nx + 0.5f) * TILE, (ny + 0.5f) * TILE)) continue;
        seen[ny * MAP_W + nx] = 1; q.push_back(ny * MAP_W + nx);
      }
    }
    bool all = true;
    for (const Camp& c : g.camps) all &= seen[(int)(c.p.y / TILE) * MAP_W + (int)(c.p.x / TILE)] != 0;
    all &= seen[(int)(g.villages[1].door.y / TILE) * MAP_W + (int)(g.villages[1].door.x / TILE)] != 0;
    int wildOk = 0, wild = 0;
    for (const Unit& u : g.units) if (u.st == UState::Wild) { wild++; wildOk += seen[(int)(u.p.y / TILE) * MAP_W + (int)(u.p.x / TILE)] != 0; }
    char msg[160];
    std::snprintf(msg, sizeof msg, "seed %llu: all camps + village 2 reachable on foot; %d/%d wild units reachable, %zu tiles walkable", (unsigned long long)sd, wildOk, wild, q.size());
    CHECK(all, msg);
  }
  {  // merge rules
    MGame g(3); g.mode = MMode::Play;
    g.units.clear();
    Unit k1; k1.id = 100; k1.sp = Species::Knight; k1.lv = 1; k1.st = UState::Party; k1.p = g.hero.p; k1.maxhp = k1.hp = 70;
    Unit k2 = k1; k2.id = 101;
    Unit a1 = k1; a1.id = 102; a1.sp = Species::Archer;
    Unit k3 = k1; k3.id = 103; k3.lv = 2;
    g.units = {k1, k2, a1, k3};
    CHECK(!g.tryMerge(100, 102), "different species cannot merge");
    CHECK(!g.tryMerge(100, 103), "different levels cannot merge");
    CHECK(g.tryMerge(100, 101), "same species + level merges");
    CHECK(g.units.size() == 3, "a merge consumes one unit");
    const Unit* m = g.unitById(101);
    CHECK(m && m->lv >= 2, "merged unit is now level 2+");
    CHECK(g.tryMerge(101, 103) == (m->lv == 2), "merged lv2 can merge with another lv2 (unless it lucked into lv3)");
  }
  {  // auto merge cascades
    MGame g(3); g.mode = MMode::Play;
    g.units.clear();
    for (int i = 0; i < 8; i++) {
      Unit u; u.id = 200 + i; u.sp = Species::Mage; u.lv = 1; u.st = UState::Lodged; u.village = 0; u.p = g.hero.p; u.maxhp = u.hp = 28;
      g.units.push_back(u);
    }
    int n = g.autoMerge();
    CHECK(n >= 7 - 0 || g.units.size() <= 2, "auto-merge cascades 8 lv1 into one or two high-level units");
    CHECK(g.units.size() <= 2, "units collapsed by merging");
  }
  {  // recruit a wild unit by walking into it
    MGame g(3); g.mode = MMode::Play; g.godMode = true;
    for (Enemy& e : g.enemies) e.hp = 0;
    g.enemies.clear();
    int wid = -1; for (const Unit& u : g.units) if (u.st == UState::Wild) { wid = u.id; break; }
    Unit* w = g.unitById(wid);
    g.hero.p = w->p + Vec2(6, 0);
    g.update(SIM_DT2, Vec2());
    CHECK(g.unitById(wid)->st == UState::Party, "walking up to a wild unit recruits it");
  }
  {  // inn: lodge + hire
    MGame g(3); g.mode = MMode::Play;
    g.enemies.clear();
    g.gold = 100;
    int before = (int)g.units.size();
    CHECK(g.hire(), "can hire at the home inn with enough gold");
    CHECK((int)g.units.size() == before + 1 && g.gold == 100 - 20, "hire costs gold and adds a unit");
    g.hero.p = g.villages[0].p + Vec2(300, 300);
    CHECK(!g.hire(), "cannot hire away from the inn");
  }
  {  // save / load round trip
    MGame g(9); g.mode = MMode::Play;
    g.gold = 123; g.hired = 2; g.raidNum = 3; g.camps[0].cleared = true;
    g.hero.p = Vec2(700, 700);
    g.units.clear();
    Unit u; u.id = 500; u.sp = Species::Archer; u.lv = 4; u.st = UState::Lodged; u.village = 0; u.p = Vec2(900, 900); u.maxhp = u.hp = MGame::unitMaxHp(Species::Archer, 4);
    g.units.push_back(u);
    std::vector<uint8_t> buf;
    g.serialize(buf);
    MGame h(1);
    CHECK(h.deserialize(buf), "save file loads");
    CHECK(h.gold == 123 && h.raidNum == 3 && h.hired == 2, "gold / raid / hired restored");
    CHECK(h.camps[0].cleared, "cleared camps stay cleared");
    CHECK(h.units.size() == 1 && h.units[0].lv == 4 && h.units[0].sp == Species::Archer, "units restored with level and species");
    CHECK(h.tiles == g.tiles, "world regenerates identically from the saved seed");
    std::vector<uint8_t> trunc(buf.begin(), buf.begin() + 40);
    MGame t(1);
    CHECK(!t.deserialize(trunc), "truncated save is rejected");
  }
  std::printf("\n%s (%d failures)\n", g_fail ? "TESTS FAILED" : "ALL TESTS PASSED", g_fail);
  return g_fail ? 1 : 0;
}

// Bot run: collect wild units, merge, lodge/hire at home, then clear camps.
static int botRun(float seconds, uint64_t seed) {
  MGame g(seed); g.mode = MMode::Play;
  MBot bot;
  float t = 0;
  int lastPrint = 0;
  while (t < seconds) {
    g.update(SIM_DT2, bot.act(g, SIM_DT2));
    t += SIM_DT2;
    if (std::getenv("LOG") && g.bannerT > 2.98f) std::printf("    [%.0fs] %s\n", t, g.banner.c_str());
    if (std::getenv("LOG") && g.raidActive && (int)(t * 120) % 600 == 0) {
      int n = 0; float minD = 1e9f;
      for (const Enemy& e : g.enemies) if (e.camp < 0) { n++; minD = std::min(minD, len(e.p - g.villages[0].p)); }
      std::printf("    raiders=%d nearestToVillage=%.0f villageHp=%.0f kills=%d\n", n, minD, g.villages[0].hp, g.kills);
    }
    g.events.clear();
    if (t >= lastPrint + 60) {
      lastPrint += 60;
      int cleared = 0; for (const Camp& c : g.camps) cleared += c.cleared;
      int maxlv = 0; for (const Unit& u : g.units) maxlv = std::max(maxlv, u.lv);
      std::printf("  t=%4.0fs gold=%4d units=%2zu maxLv=%d kills=%3d merges=%2d camps=%d/%zu raids=%d heroHp=%.0f\n", t, g.gold, g.units.size(), maxlv, g.kills, g.merges, cleared, g.camps.size(), g.raidNum, g.hero.hp);
    }
  }
  return 0;
}

int main(int argc, char** argv) {
  if (argc > 1 && !std::strcmp(argv[1], "--test")) return runTests();
  float seconds = argc > 1 ? (float)std::atof(argv[1]) : 600.0f;
  uint64_t seed = argc > 2 ? (uint64_t)std::atoll(argv[2]) : 1;
  return botRun(seconds, seed);
}
