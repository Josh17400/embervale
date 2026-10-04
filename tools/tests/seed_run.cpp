// rpg_test: one seed: world stats, reachability, quests, the wandering bot, a save round trip.
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
#include "tools/tests/tests.h"

// ---- the opening, played by a bot (M0: the real start is shirt only). Also used by metrics.cpp and test_defence.cpp.
// Walk to the start village inn, talk to the innkeeper (who hands over the old blade), walk back out.
namespace {
// 4-connected BFS over the current map to the goal tile, or to the reachable tile nearest it
void tilePath(const Map& m, int sx, int sy, int gx, int gy, std::vector<int>& path) {
  path.clear();
  if (!m.in(sx, sy)) return;
  const int R = 70;
  int x0 = std::max(0, std::min(sx, gx) - R), y0 = std::max(0, std::min(sy, gy) - R);
  int x1 = std::min(m.w - 1, std::max(sx, gx) + R), y1 = std::min(m.h - 1, std::max(sy, gy) + R);
  int bw = x1 - x0 + 1, bh = y1 - y0 + 1;
  std::vector<int> from((size_t)bw * bh, -1), q;
  auto id = [&](int x, int y) { return (y - y0) * bw + (x - x0); };
  q.push_back(id(sx, sy));
  from[(size_t)id(sx, sy)] = id(sx, sy);
  int best = id(sx, sy), bestD = (sx - gx) * (sx - gx) + (sy - gy) * (sy - gy);
  for (size_t h = 0; h < q.size(); h++) {
    int x = q[h] % bw + x0, y = q[h] / bw + y0;
    int d = (x - gx) * (x - gx) + (y - gy) * (y - gy);
    if (d < bestD) { bestD = d; best = q[h]; }
    if (d == 0) break;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) {
      int nx = x + dx[k], ny = y + dy[k];
      if (nx < x0 || ny < y0 || nx > x1 || ny > y1 || from[(size_t)id(nx, ny)] >= 0 || m.blocked(nx, ny)) continue;
      from[(size_t)id(nx, ny)] = q[h];
      q.push_back(id(nx, ny));
    }
  }
  for (int c = best;; c = from[(size_t)c]) {
    path.push_back((c / bw + y0) * m.w + (c % bw + x0));
    if (from[(size_t)c] == c) break;
  }
  std::reverse(path.begin(), path.end());
}
void followPath(Game& g, std::vector<int>& path, Input& in) {
  const Map& m = g.map();
  while (!path.empty()) {
    Vec2 t(path[0] % m.w * TILE + 8.0f, path[0] / m.w * TILE + 10.0f);
    Vec2 d = t - g.pl().p;
    if (std::fabs(d.x) <= 2.5f && std::fabs(d.y) <= 2.5f) { path.erase(path.begin()); continue; }
    in.move = norm(d);
    return;
  }
}
}  // namespace

// One frame of the opening bot: fills `in`; returns true once the player is armed and back outside (or there is no
// opening to play). `clock` counts frames (replanning cadence), `path` is its scratch.
bool openingStep(Game& g, std::vector<int>& path, int& clock, Input& in);
bool openingStep(Game& g, std::vector<int>& path, int& clock, Input& in) {
  in = Input();
  clock++;
  const bool armed = g.eqWeapon >= 0;
  const Actor& p = g.pl();
  int px = (int)std::floor(p.p.x / TILE), py = (int)std::floor((p.p.y - 2) / TILE);
  if (!g.inside) {
    if (armed) return true;
    const Quest* oq = g.questById(g.openingQuest());
    if (!oq || oq->giverBldg < 0) return true;
    const Bldg& B = g.world.over.bldgs[oq->giverBldg];
    int ax = B.doorX(), ay = B.doorY() + 1;
    if (px == ax && (py == ay || py == ay - 1)) {   // at the door: push in
      float cx = ax * TILE + 8.0f - p.p.x;
      in.move = Vec2(std::fabs(cx) > 2 ? (cx > 0 ? 0.5f : -0.5f) : 0.0f, -1.0f);
      return false;
    }
    if (path.empty() || clock % 30 == 0) tilePath(g.map(), px, py, ax, ay, path);
    followPath(g, path, in);
    return false;
  }
  if (!armed) {
    int keeper = -1;
    for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].role == Role::Innkeeper && g.actors[k].st != AState::Dead) keeper = (int)k;
    if (keeper >= 0) {
      if (g.interactTarget() == g.actors[keeper].id) { in.interact = clock % 6 == 0; return false; }
      if (path.empty() || clock % 20 == 0)
        tilePath(g.map(), px, py, (int)std::floor(g.actors[keeper].p.x / TILE), (int)std::floor((g.actors[keeper].p.y - 2) / TILE), path);
      followPath(g, path, in);
      return false;
    }
  }
  // walk back out: onto the exit, then down
  int ex = g.sub.exitX, ey = g.sub.exitY;
  if (px == ex && py >= ey - 1) { in.move = Vec2(0, 1); return false; }
  if (path.empty() || clock % 20 == 0) tilePath(g.map(), px, py, ex, ey - 1, path);
  followPath(g, path, in);
  return false;
}

int runSeed(uint64_t seed, const char* mapOut, float secs, bool mortal, SeedResult& res) {
  g_curSeed = seed;
  auto t0 = std::chrono::steady_clock::now();
  Game g(seed);
  g.newGame(seed);
  auto t1 = std::chrono::steady_clock::now();
  res.genMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
  res.sites = (int)g.world.sites.size();
  out("seed %llu generated in %.0f ms\n", (unsigned long long)seed, std::chrono::duration<double, std::milli>(t1 - t0).count());
  int counts[(int)SiteType::COUNT] = {};
  for (auto& s : g.world.sites) counts[(int)s.type]++;
  for (int t = 0; t < (int)SiteType::COUNT; t++) out("  %-13s %d\n", siteTypeName((SiteType)t), counts[t]);
  out("  buildings %zu, spawns %zu, start %s, capital %s, lair %d\n", g.world.over.bldgs.size(), g.world.over.spawns.size(),
         g.world.sites[g.world.startSite].name.c_str(), g.world.sites[g.world.capital].name.c_str(), g.world.lair);
  int mq = 0;
  for (auto& s : g.world.sites) if (s.mainQuest) mq++;
  out("  main-quest ruins %d\n", mq);
  {
    // wilderness dens (WORLDGEN_V2): count by kind and the nearest few to the start (for test scripts: walkto X Y)
    std::map<std::string, int> byKind;
    std::vector<std::pair<float, int>> near;
    int sx = (int)(g.pl().p.x / TILE), sy = (int)(g.pl().p.y / TILE);
    for (int i = 0; i < (int)g.world.dens.size(); i++) {
      const Den& d = g.world.dens[i];
      byKind[g.world.dens[i].mon == art::Monster::Wolf ? "wolf" : d.mon == art::Monster::IceWolf ? "icewolf" : d.mon == art::Monster::Goblin ? "goblin"
             : d.mon == art::Monster::Skeleton ? "skeleton" : d.mon == art::Monster::Bear ? "bear" : d.mon == art::Monster::Troll ? "troll" : "spider"]++;
      near.push_back({std::hypot((float)(d.x - sx), (float)(d.y - sy)), i});
    }
    std::string kinds;
    for (auto& kv : byKind) kinds += " " + kv.first + " " + std::to_string(kv.second);
    out("  dens %zu:%s\n", g.world.dens.size(), kinds.c_str());
    std::sort(near.begin(), near.end());
    for (size_t k = 0; k < near.size() && k < 4; k++) {
      const Den& d = g.world.dens[near[k].second];
      out("    den %d at %d,%d (monster %d, pack %d, zone level %d) %.0f tiles from the start\n", near[k].second, d.x, d.y, (int)d.mon, d.pack, g.world.zoneLevel(d.x, d.y), near[k].first);
    }
    if (g.world.genVersion >= WORLDGEN_V2 && g.world.dens.size() < 20) out("WARN: only %zu dens\n", g.world.dens.size());
  }
  int bad = 0;
  if (counts[(int)SiteType::City] < 2 || counts[(int)SiteType::Town] < 3 || counts[(int)SiteType::Cave] < 8 || mq < 3 || g.world.lair < 0) {
    out("FAIL: too few sites (cities %d towns %d caves %d main-quest ruins %d lair %d)\n", counts[(int)SiteType::City], counts[(int)SiteType::Town],
        counts[(int)SiteType::Cave], mq, g.world.lair);
    bad++;
  }
  if (g.map().blocked((int)(g.pl().p.x / 16), (int)(g.pl().p.y / 16))) { out("FAIL: player starts inside a wall\n"); bad++; }
  {
    // the main quest needs a jarl in the capital's keep
    const Site& cap = g.world.sites[g.world.capital];
    bool keep = false;
    for (int b = cap.bldgFirst; b < cap.bldgFirst + cap.bldgCount; b++) if (g.world.over.bldgs[b].type == art::Building::Keep) keep = true;
    if (cap.type != SiteType::City || !keep) { out("FAIL: capital %s has no keep\n", cap.name.c_str()); bad++; }
  }

  if (mapOut) {
    const Map& m = g.world.over;
    std::vector<uint32_t> px((size_t)m.w * m.h);
    for (int y = 0; y < m.h; y++)
      for (int x = 0; x < m.w; x++) {
        uint32_t c = groundColor(m.at(x, y));
        int p = m.propAt(x, y);
        if (p && propSolid((art::Prop)(p - 1))) c = rgba(((c & 255) * 6) / 10, (((c >> 8) & 255) * 6) / 10, (((c >> 16) & 255) * 6) / 10);
        if (m.bldgAt[(size_t)y * m.w + x] >= 0) c = rgba(170, 60, 50);
        if (m.wall[(size_t)y * m.w + x]) c = rgba(60, 60, 60);
        px[(size_t)y * m.w + x] = c;
      }
    for (auto& s : g.world.sites) {
      uint32_t c = s.type == SiteType::Cave ? rgba(0, 0, 0) : s.type == SiteType::Ruin ? rgba(160, 0, 200) : s.type == SiteType::BanditCamp ? rgba(255, 0, 0)
                 : s.type == SiteType::DragonLair ? rgba(255, 120, 0) : s.type == SiteType::Shrine ? rgba(255, 255, 0) : 0;
      if (!c) continue;
      for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) { int x = s.ex + ox, y = s.ey + oy; if (m.in(x, y)) px[(size_t)y * m.w + x] = c; }
    }
    int sx = (int)(g.pl().p.x / 16), sy = (int)(g.pl().p.y / 16);
    for (int k = -3; k <= 3; k++) { if (m.in(sx + k, sy)) px[(size_t)sy * m.w + sx + k] = rgba(255, 255, 255); if (m.in(sx, sy + k)) px[(size_t)(sy + k) * m.w + sx] = rgba(255, 255, 255); }
    out("map %s: %s\n", mapOut, writePng(mapOut, m.w, m.h, px) ? "ok" : "FAILED");
  }

  // everything the story needs must be reachable on foot from the start village
  {
    const Map& m = g.world.over;
    std::vector<uint8_t> seen((size_t)m.w * m.h, 0);
    std::vector<int> q;
    int sx = (int)(g.pl().p.x / 16), sy = (int)(g.pl().p.y / 16);
    q.push_back(sy * m.w + sx); seen[(size_t)sy * m.w + sx] = 1;
    for (size_t h = 0; h < q.size(); h++) {
      int x = q[h] % m.w, y = q[h] / m.w;
      static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
      for (int k = 0; k < 4; k++) {
        int nx = x + dx[k], ny = y + dy[k];
        if (!m.in(nx, ny) || seen[(size_t)ny * m.w + nx]) continue;
        bool door = m.bldgAt[(size_t)ny * m.w + nx] >= 0;   // doors count as reached, but don't walk through buildings
        if (m.blocked(nx, ny) && !door) continue;
        seen[(size_t)ny * m.w + nx] = 1;
        if (!door) q.push_back(ny * m.w + nx);
      }
    }
    auto reach = [&](int x, int y) {   // the tile or any neighbour (entrances sit in rock / on props)
      for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if (m.in(x + ox, y + oy) && seen[(size_t)(y + oy) * m.w + x + ox]) return true;
      return false;
    };
    int unreachable = 0;
    for (int si = 0; si < (int)g.world.sites.size(); si++) {
      const Site& s = g.world.sites[si];
      bool story = si == g.world.capital || s.mainQuest || si == g.world.lair;
      bool town = s.type == SiteType::City || s.type == SiteType::Town || s.type == SiteType::Village;
      int ty = s.ey + (s.type == SiteType::Cave ? 1 : (s.type == SiteType::Ruin ? 3 : 0));
      if (!reach(s.ex, ty)) {
        if (story) { out("FAIL: %s (%s) at %d,%d is not reachable on foot from the start\n", s.name.c_str(), siteTypeName(s.type), s.ex, ty); bad++; }
        else if (town) { out("FAIL: settlement %s at %d,%d is not reachable on foot\n", s.name.c_str(), s.ex, ty); bad++; }
        else unreachable++;
      }
    }
    if (unreachable) out("WARN: %d optional sites unreachable on foot\n", unreachable);
  }

  // interiors and dungeons generate and are walkable from their entrance
  for (int si = 0; si < (int)g.world.sites.size(); si++) {
    const Site& s = g.world.sites[si];
    if (s.type != SiteType::Cave && s.type != SiteType::Ruin) continue;
    Map m;
    if (s.type == SiteType::Ruin) genRuin(m, s, s.seed); else genCave(m, s, s.seed);
    bool boss = false;
    for (auto& sp : m.spawns) if (sp.boss) boss = true;
    if (m.blocked(m.exitX, m.exitY - 1)) out("   exit %d,%d ground %d prop %d solid %d\n", m.exitX, m.exitY, (int)m.at(m.exitX, m.exitY - 1), m.propAt(m.exitX, m.exitY - 1), (int)m.solid[(size_t)(m.exitY - 1) * m.w + m.exitX]);
    if (!boss || m.blocked(m.exitX, m.exitY - 1)) { out("FAIL: dungeon %s (boss %d, exit blocked %d)\n", s.name.c_str(), boss, m.blocked(m.exitX, m.exitY - 1)); bad++; }
  }
  for (size_t bi = 0; bi < g.world.over.bldgs.size(); bi++) {
    Map m;
    genInterior(m, g.world.over.bldgs[bi], g.world.over.bldgs[bi].seed);
    if (m.blocked(m.exitX, m.exitY - 1)) { out("FAIL: interior %zu exit blocked\n", bi); bad++; }
    const Bldg& b = g.world.over.bldgs[bi];
    if (g.world.over.blocked(b.doorX(), b.doorY() + 1)) { const Map& o = g.world.over; int ax = b.doorX(), ay = b.doorY() + 1; out("WARN: building %zu door approach blocked: ground %d prop %d wall %d bldg %d\n", bi, (int)o.at(ax, ay), o.propAt(ax, ay), (int)o.wall[(size_t)ay * o.w + ax], (int)o.bldgAt[(size_t)ay * o.w + ax]); }
  }

  // bot: wander, fight, use doors
  g.mode = Mode::Play;
  g.godMode = !mortal;
  int deaths = 0, potions = 0;
  Rng r(seed);
  Input in;
  int frames = (int)(secs * 60);
  int evCount = 0;
  float maxStep = 0;
  // the opening first (shirt only): the start inn's old blade must be in hand within 5 minutes
  {
    int geared = 0;
    for (const Item& it : g.inv) if (it.kind == ItemKind::Weapon || it.kind == ItemKind::Bow || it.kind == ItemKind::Armor || it.kind == ItemKind::Arrows) geared++;
    if (geared || g.eqWeapon >= 0) { out("FAIL: the start is not shirt-only (%d weapon/armour items)\n", geared); bad++; }
    if (g.openingQuest() < 0 || g.trackedQuest != g.openingQuest()) { out("FAIL: the opening quest is not tracked at the start\n"); bad++; }
    int tx = 0, ty = 0;
    const Quest* oq = g.questById(g.openingQuest());
    if (oq && (!g.questTarget(oq->id, tx, ty) || tx != g.world.over.bldgs[oq->giverBldg].doorX())) { out("FAIL: the opening quest does not point at the inn door\n"); bad++; }
  }
  {
    std::vector<int> path;
    int clock = 0, of = 0;
    float armedAt = -1;
    for (; of < 300 * 60; of++) {
      Input oin;
      bool done = openingStep(g, path, clock, oin);
      if (g.eqWeapon >= 0 && armedAt < 0) armedAt = of / 60.0f;
      if (done) break;
      if (g.pl().st != AState::Dead) {   // fists for anything that bothers us on the way
        for (size_t k = 1; k < g.actors.size(); k++)
          if (g.actors[k].hostile && g.actors[k].st != AState::Dead && len2(g.actors[k].p - g.pl().p) < 22 * 22) { oin.attack = of % 12 == 0; break; }
      }
      g.update(SIM_DT, oin);
      g.events.clear();
      if (g.mode == Mode::Dialogue || g.mode == Mode::Shop || g.mode == Mode::LevelUp) g.mode = Mode::Play;
      if (g.mode == Mode::Dead) g.respawn();
    }
    out("opening: first weapon at %.1f s, back outside at %.1f s (%s)\n", armedAt, of / 60.0f, g.eqWeapon >= 0 ? g.inv[g.eqWeapon].name.c_str() : "unarmed");
    if (armedAt < 0 || armedAt > 300) { out("FAIL: no first weapon within 5 minutes of the start (%.1f s)\n", armedAt); bad++; }
    if (!g.hasFlag(SF_FIRST_WEAPON)) { out("FAIL: SF_FIRST_WEAPON not set by the opening\n"); bad++; }
    if (g.inside) { out("FAIL: the opening bot is stuck inside the inn\n"); bad++; }
  }
  for (int f = 0; f < frames; f++) {
    // hunt the nearest hostile if one is close, otherwise wander
    int tgt = -1; float bd = 200 * 200;
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& e = g.actors[k];
      if (!e.hostile || e.st == AState::Dead) continue;
      float d2 = len2(e.p - g.pl().p);
      if (d2 < bd) { bd = d2; tgt = (int)k; }
    }
    if (tgt >= 0) in.move = norm(g.actors[tgt].p - g.pl().p) * (bd > 18 * 18 ? 1.0f : 0.0f);
    else if (f % 90 == 0) { float a = r.f() * TAU; in.move = Vec2(std::cos(a), std::sin(a)); }
    in.attack = (f % 20) == 0 && tgt >= 0 && bd < 30 * 30;
    in.bow = (f % 97) == 0 && g.eqBow >= 0;
    in.spell = (f % 151) == 0;
    in.roll = (f % 211) == 0;
    in.interact = (f % 60) == 0;
    auto a = std::chrono::steady_clock::now();
    g.update(SIM_DT, in);
    auto b = std::chrono::steady_clock::now();
    maxStep = std::max(maxStep, (float)std::chrono::duration<double, std::milli>(b - a).count());
    evCount += (int)g.events.size();
    g.events.clear();
    if (g.mode == Mode::Dialogue) g.dialogueChoose(0), g.mode = Mode::Play;
    if (g.mode == Mode::Shop) g.mode = Mode::Play;
    if (g.mode == Mode::Dead) { deaths++; g.respawn(); }
    if (g.pl().hp < g.pl().maxHp * 0.35f && f % 30 == 0) { Input q; q.potion = true; g.update(SIM_DT, q); potions++; }
  }
  out("deaths %d, potion attempts %d\n", deaths, potions);
  out("bot: %.0fs, kills %d, lvl %d, actors %zu, events %d, gold %d, max step %.2f ms, inside %d\n", secs, g.kills, g.plLevel, g.actors.size(), evCount, g.gold, maxStep, g.inside);
  res.kills = g.kills; res.level = g.plLevel; res.maxStep = maxStep;

  // dialogue + quest flow: walk into the start village inn, talk to the innkeeper, take a job
  {
    Game q(seed);
    q.newGame(seed);
    q.mode = Mode::Play;
    const Site& home = q.world.sites[q.world.startSite];
    int inn = -1;
    for (int b = home.bldgFirst; b < home.bldgFirst + home.bldgCount; b++) if (q.world.over.bldgs[b].type == art::Building::Inn) inn = b;
    if (inn < 0) { out("FAIL: start village has no inn\n"); bad++; }
    else {
      const Bldg& B = q.world.over.bldgs[inn];
      q.pl().p = Vec2(B.doorX() * 16 + 8.0f, B.doorY() * 16 + 10.0f);
      q.update(SIM_DT, Input());
      if (!q.inside) { out("FAIL: could not enter the inn\n"); bad++; }
      int keeper = -1;
      for (size_t k = 1; k < q.actors.size(); k++) if (q.actors[k].role == Role::Innkeeper) keeper = (int)k;
      if (keeper < 0) { out("FAIL: no innkeeper\n"); bad++; }
      else {
        q.pl().p = q.actors[keeper].p + Vec2(0, 20);
        Input in2; in2.interact = true;
        q.update(SIM_DT, in2);
        if (q.mode != Mode::Dialogue) { out("FAIL: talking did not open dialogue\n"); bad++; }
        else {
          out("dialogue: %s: %s\n", q.dlg.speaker.c_str(), q.dlg.text.c_str());
          for (size_t o = 0; o < q.dlg.opts.size(); o++) out("   [%zu] %s\n", o, q.dlg.opts[o].label.c_str());
          for (size_t o = 0; o < q.dlg.opts.size(); o++) if (q.dlg.opts[o].label.find("WORK") != std::string::npos) { q.dialogueChoose((int)o); break; }
          out("offer: %s\n", q.dlg.text.c_str());
          size_t nq = q.quests.size();
          q.dialogueChoose(0);
          out("quests now %zu: %s\n", q.quests.size(), q.quests.back().title.c_str());
          if (q.quests.size() != nq + 1) { out("FAIL: quest not accepted\n"); bad++; }
          q.mode = Mode::Play;
          q.update(SIM_DT, in2);
          for (size_t o = 0; o < q.dlg.opts.size(); o++) if (q.dlg.opts[o].label.find("WARES") != std::string::npos) { q.dialogueChoose((int)o); break; }
          out("shop stock %zu, mode %d\n", q.shop.stock.size(), (int)q.mode);
          int g0 = q.gold;
          int left0 = q.shop.stock.empty() ? 0 : q.shop.stock[0].count;
          size_t n0 = q.shop.stock.size();
          q.buy(0);
          out("bought for %d gold\n", g0 - q.gold);
          // close and reopen: what was bought must stay sold
          q.mode = Mode::Play;
          q.update(SIM_DT, in2);
          for (size_t o = 0; o < q.dlg.opts.size(); o++) if (q.dlg.opts[o].label.find("WARES") != std::string::npos) { q.dialogueChoose((int)o); break; }
          bool restocked = q.shop.stock.size() > n0 || (q.shop.stock.size() == n0 && !q.shop.stock.empty() && q.shop.stock[0].count >= left0 && left0 > 1);
          if (q.mode != Mode::Shop || restocked) { out("FAIL: shop refilled after a purchase (%zu items, first x%d)\n", q.shop.stock.size(), q.shop.stock.empty() ? 0 : q.shop.stock[0].count); bad++; }
        }
      }
    }
  }

  // main quest end to end: the jarl, shards from warlords slain early, the dragon's death
  {
    Game q(seed);
    q.newGame(seed);
    q.mode = Mode::Play;
    q.godMode = true;
    for (auto& s : q.world.sites) if (s.mainQuest) s.cleared = true;   // warlords slain before meeting the jarl
    const Site& cap = q.world.sites[q.world.capital];
    int keep = -1;
    for (int b = cap.bldgFirst; b < cap.bldgFirst + cap.bldgCount; b++) if (q.world.over.bldgs[b].type == art::Building::Keep) keep = b;
    Quest* mq = nullptr;
    for (auto& qq : q.quests) if (qq.type == QType::Main) mq = &qq;
    if (keep < 0 || !mq) { out("FAIL: main quest setup\n"); bad++; }
    else {
      const Bldg& B = q.world.over.bldgs[keep];
      q.pl().p = Vec2(B.doorX() * 16 + 8.0f, B.doorY() * 16 + 10.0f);
      q.update(SIM_DT, Input());
      int jarl = -1;
      for (size_t k = 1; k < q.actors.size(); k++) if (q.actors[k].role == Role::Jarl) jarl = (int)k;
      if (!q.inside || jarl < 0) { out("FAIL: no jarl in the keep\n"); bad++; }
      else {
        q.pl().p = q.actors[jarl].p + Vec2(0, 18);
        Input talk; talk.interact = true;
        q.update(SIM_DT, talk);
        int opt = -1;
        for (size_t o = 0; o < q.dlg.opts.size(); o++) if (q.dlg.opts[o].label.find("STOPPED") != std::string::npos) opt = (int)o;
        if (q.mode != Mode::Dialogue || opt < 0) { out("FAIL: jarl has no main quest option\n"); bad++; }
        else {
          q.dialogueChoose(opt);
          for (auto& qq : q.quests) if (qq.type == QType::Main) mq = &qq;
          out("main quest after jarl: stage %d (%s)\n", mq->stage, mq->title.c_str());
          if (mq->stage != 2) { out("FAIL: shards from pre-cleared ruins did not count\n"); bad++; }
        }
      }
      // the dragon hunt
      q.mode = Mode::Play;
      for (auto& qq : q.quests) if (qq.type == QType::Main) { qq.stage = 3; mq = &qq; }
      const Site& L = q.world.sites[q.world.lair];
      if (q.inside) {
        // stepping onto the exit while knocked back must NOT leave; walking down onto it must
        q.pl().p = Vec2(q.sub.exitX * 16 + 8.0f, (q.sub.exitY - 2) * 16 + 8.0f);
        q.update(SIM_DT, Input());
        q.pl().p = Vec2(q.sub.exitX * 16 + 8.0f, q.sub.exitY * 16 + 4.0f);
        q.pl().knock = Vec2(0, 110);
        q.update(SIM_DT, Input());
        if (!q.inside) { out("FAIL: knockback onto the exit threw the player out\n"); bad++; }
        q.pl().knock = Vec2();
        Input down; down.move = Vec2(0, 1);
        for (int f = 0; f < 3 && q.inside; f++) q.update(SIM_DT, down);
        if (q.inside) { out("FAIL: walking onto the exit did not leave\n"); bad++; }
      }
      q.pl().p = Vec2(L.ex * 16 + 8.0f, (L.ey + 2) * 16 + 8.0f);
      int dragon = -1;
      for (int f = 0; f < 10 && dragon < 0; f++) {
        q.update(SIM_DT, Input());
        for (size_t k = 1; k < q.actors.size(); k++) if (q.actors[k].mon == art::Monster::Dragon) dragon = (int)k;
      }
      if (dragon < 0) { out("FAIL: the dragon never appeared at its lair\n"); bad++; }
      else {
        out("dragon: %.0f hp, level %d\n", q.actors[dragon].maxHp, q.actors[dragon].level);
        q.actors[dragon].hp = 0.5f; q.actors[dragon].burnT = 1.0f;
        for (int f = 0; f < 30; f++) { q.update(SIM_DT, Input()); if (q.mode != Mode::Play) q.mode = Mode::Play; }
        for (auto& qq : q.quests) if (qq.type == QType::Main) mq = &qq;
        out("after the dragon: stage %d, %s\n", mq->stage, mq->state == QState::Done ? "DONE" : "not done");
        if (mq->state != QState::Done) { out("FAIL: killing the dragon did not finish the main quest\n"); bad++; }
      }
    }
  }

  // dying (to fire, which once couldn't kill) and waking in town; saving on the death screen respawns on load
  {
    Game d(seed);
    d.newGame(seed);
    d.mode = Mode::Play;
    d.gold = 200;
    d.pl().hp = 1; d.pl().burnT = 2;
    for (int f = 0; f < 120 && d.mode == Mode::Play; f++) d.update(SIM_DT, Input());
    if (d.mode != Mode::Dead) { out("FAIL: burning at 1 HP did not kill the player\n"); bad++; }
    else {
      std::vector<uint8_t> ds;
      d.serialize(ds);
      Game e(1);
      if (!e.deserialize(ds) || e.pl().hp < e.pl().maxHp || e.gold != 180) { out("FAIL: loading a death-screen save did not respawn (hp %.0f, gold %d)\n", e.pl().hp, e.gold); bad++; }
      d.respawn();
      out("death: respawned in %s with %.0f/%.0f HP, gold 200 -> %d\n", d.locName.c_str(), d.pl().hp, d.pl().maxHp, d.gold);
      if (d.mode != Mode::Play || d.pl().hp < d.pl().maxHp || d.gold != 180) { out("FAIL: respawn state\n"); bad++; }
    }
  }

  // save round trip
  std::vector<uint8_t> buf;
  g.serialize(buf);
  Game h(1);
  if (!h.deserialize(buf)) { out("FAIL: save did not load\n"); bad++; }
  else {
    std::vector<uint8_t> buf2;
    h.serialize(buf2);
    if (buf2.size() != buf.size()) { out("FAIL: save round-trip size %zu vs %zu\n", buf.size(), buf2.size()); bad++; }
    out("save %zu bytes ok\n", buf.size());
  }
  res.bad = bad;
  return bad;
}
