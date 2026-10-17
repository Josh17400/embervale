// rpg_test --save-soak N [--seeds A..B] [--days D] [--budget KB]: the M12 save soak (VISION_PLAN 13 M12 verification:
// "--save-soak 100 at <= 1 MB"). Per seed (default 1), N cycles of: D in-game days pass (default 7) while the game
// plays on (the realm ticks, the fief and the army keep their days), every 5th cycle the hero travels about 1500 tiles
// (new regions, realms and settlements come into focus), every 10th cycle something of M12 happens (enlist, a
// promotion, an order, a lordship, independence, a warlord); then the game is saved, the save is loaded into a fresh
// Game, the loaded game is saved again and must be byte-identical, and play continues ON THE LOADED COPY. FAILS when a
// save exceeds the budget (default 1024 KB), a round trip differs, or a load fails. Prints the size curve (every 10th
// cycle), the largest block sizes and the save / load times. Phase A (lead); the REALM lane owns it from phase B
// (bounded logs, pruning) and every lane keeps its block inside the budget.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include "rpg/sim/lord_ops.h"
#include "rpg/sim/service_ops.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

double msSince(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

void playDays(Game& g, int days) {
  for (int d = 0; d < days; d++) {
    g.day++;
    for (int i = 0; i < 4; i++) { g.update(SIM_DT, Input()); g.events.clear(); }
  }
}

int soakSeed(uint64_t seed, int N, int days, size_t budget) {
  int bad = 0;
  auto fail = [&](int cyc, const char* what) { out("FAIL: save-soak seed %llu cycle %d: %s\n", (unsigned long long)seed, cyc, what); bad++; };
  auto g = std::make_unique<Game>(seed);
  g->newEndlessGame(seed);
  g->mode = Mode::Play;
  g->godMode = true;
  g->noWildSpawns = true;
  for (int i = 0; i < 60; i++) { g->update(SIM_DT, Input()); g->events.clear(); }
  size_t maxSize = 0;
  double saveMs = 0, loadMs = 0;
  uint64_t walk = ew::mix64(seed ^ 0x534F414Bull);
  std::string why;
  for (int c = 1; c <= N; c++) {
    playDays(*g, days);
    if (c % 5 == 0) {   // travel: new land comes into focus
      walk = ew::mix64(walk);
      const double a = (double)(walk % 6283) / 1000.0;
      const int32_t hx = g->world.ox + (int32_t)std::floor(g->pl().p.x / TILE), hy = g->world.oy + (int32_t)std::floor(g->pl().p.y / TILE);
      g->teleportGlobal(hx + (int32_t)(std::cos(a) * 1500), hy + (int32_t)(std::sin(a) * 1500));
      for (int i = 0; i < 30; i++) { g->update(SIM_DT, Input()); g->events.clear(); }
    }
    if (c % 10 == 0) {   // something of M12
      const int32_t hx = g->world.ox + (int32_t)std::floor(g->pl().p.x / TILE), hy = g->world.oy + (int32_t)std::floor(g->pl().p.y / TILE);
      const ew::Gid land = g->world.src ? g->realm.landOwner(g->world.src->kingdomAt(hx, hy), hx, hy) : 0;
      switch ((c / 10) % 5) {
        case 1: if (!g->service.enlisted() && land) ServiceOps::enlist(*g, land, why); break;
        case 2: ServiceOps::promote(*g); ServiceOps::giveOrder(*g, service::OrderKind::Scout, 0, hx + 20, hy, 0, 0); break;
        case 3:
          for (size_t si = 0; si < g->world.sites.size() && g->lordship.holdings.empty(); si++)
            if (g->world.sites[si].type == SiteType::Village) LordOps::grant(*g, (int)si, lord::How::Charter, 0, why);
          break;
        case 4: if (g->lordship.lord() && !g->lordship.realm) LordOps::independence(*g, "", why); break;
        default:
          for (const Site& s : g->world.sites)
            if (s.type == SiteType::BanditCamp) { g->realm.noteCamp(s.id, g->world.ox + s.ex, g->world.oy + s.ey, 0, g->day); g->realm.forceWarlord(s.id, g->day); break; }
          break;
      }
      g->realmSync();
    }
    if (g->inside) g->debugLeave();
    std::vector<uint8_t> a, b;
    auto t0 = std::chrono::steady_clock::now();
    g->serialize(a);
    saveMs += msSince(t0);
    maxSize = std::max(maxSize, a.size());
    if (a.size() > budget) fail(c, "the save is over the budget");
    auto h = std::make_unique<Game>(seed);
    t0 = std::chrono::steady_clock::now();
    const bool ok = h->deserialize(a);
    loadMs += msSince(t0);
    if (!ok) { fail(c, "the save did not load"); break; }
    h->mode = Mode::Play;
    h->godMode = true;
    h->noWildSpawns = true;
    h->serialize(b);
    if (a != b) fail(c, "the loaded game saves differently");
    if (c % 10 == 0 || c == 1)
      out("  seed %llu cycle %3d day %4d: save %7zu B, realm %zu kingdoms, sim %zu B, %zu quests\n", (unsigned long long)seed, c, h->day, a.size(),
          h->realm.kingdoms().size(), h->realm.memoryBytes(), h->quests.size());
    g = std::move(h);   // play on the loaded copy
  }
  out("save-soak seed %llu: %d cycles x %d days, largest save %zu B (budget %zu), save %.1f ms, load %.1f ms on average: %s\n",
      (unsigned long long)seed, N, days, maxSize, budget, saveMs / std::max(1, N), loadMs / std::max(1, N), bad ? "FAIL" : "ok");
  return bad;
}

int soakCmd(int argc, char** argv) {
  uint64_t A = 1, B = 1;
  int N = 100, days = 7;
  size_t budget = 1024 * 1024;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], A, B);
    else if (!strcmp(argv[i], "--save-soak") && i + 1 < argc) N = std::max(1, atoi(argv[++i]));
    else if (!strcmp(argv[i], "--days") && i + 1 < argc) days = std::max(1, atoi(argv[++i]));
    else if (!strcmp(argv[i], "--budget") && i + 1 < argc) budget = (size_t)std::max(1, atoi(argv[++i])) * 1024;
  }
  int bad = 0;
  for (uint64_t s = A; s <= B; s++) bad += soakSeed(s, N, days, budget);
  out("save-soak: %d failures\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--save-soak", "M12: N save / load cycles while the world plays on, every save <= 1 MB and byte-identical [N] [--seeds A..B] [--days D] [--budget KB]", soakCmd);
