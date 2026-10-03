// Headless EMBERVALE checks: world generation stats + overview map PNG, a wandering combat bot, save round-trip.
//   rpg_test [seed] [--map out.png] [--secs N] [--mortal] [--noaudit]
//   rpg_test --seeds 1..20      one summary line per seed, a repetition-audit summary, nonzero exit on any failure
//   rpg_test --metrics [--seeds 1..5] [--metric-secs 2400]   game-feel and balance metrics (PLAN.md targets table)
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <map>
#include <set>
#include <tuple>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "rpg/sim/game.h"

namespace {
uint32_t crcTable[256];
void crcInit() {
  for (uint32_t n = 0; n < 256; n++) {
    uint32_t c = n;
    for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    crcTable[n] = c;
  }
}
uint32_t crc(const uint8_t* b, size_t n, uint32_t c = 0xFFFFFFFFu) {
  for (size_t i = 0; i < n; i++) c = crcTable[(c ^ b[i]) & 255] ^ (c >> 8);
  return c;
}
void be32(std::vector<uint8_t>& v, uint32_t x) { v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x); }
void chunk(FILE* f, const char* type, const std::vector<uint8_t>& data) {
  std::vector<uint8_t> b;
  be32(b, (uint32_t)data.size());
  b.insert(b.end(), type, type + 4);
  b.insert(b.end(), data.begin(), data.end());
  uint32_t c = crc(b.data() + 4, b.size() - 4) ^ 0xFFFFFFFFu;
  be32(b, c);
  fwrite(b.data(), 1, b.size(), f);
}
bool writePng(const char* path, int w, int h, const std::vector<uint32_t>& rgba) {
  crcInit();
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  fwrite(sig, 1, 8, f);
  std::vector<uint8_t> ih;
  be32(ih, w); be32(ih, h);
  ih.push_back(8); ih.push_back(6); ih.push_back(0); ih.push_back(0); ih.push_back(0);
  chunk(f, "IHDR", ih);
  std::vector<uint8_t> raw;
  for (int y = 0; y < h; y++) {
    raw.push_back(0);
    for (int x = 0; x < w; x++) { uint32_t c = rgba[(size_t)y * w + x]; raw.push_back(c & 255); raw.push_back((c >> 8) & 255); raw.push_back((c >> 16) & 255); raw.push_back(255); }
  }
  std::vector<uint8_t> z{0x78, 0x01};
  size_t pos = 0;
  while (pos < raw.size()) {
    size_t n = std::min<size_t>(65535, raw.size() - pos);
    z.push_back(pos + n == raw.size() ? 1 : 0);
    z.push_back(n & 255); z.push_back(n >> 8); z.push_back(~n & 255); z.push_back((~n >> 8) & 255);
    z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
    pos += n;
  }
  uint32_t a = 1, b = 0;
  for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
  be32(z, (b << 16) | a);
  chunk(f, "IDAT", z);
  chunk(f, "IEND", {});
  fclose(f);
  return true;
}

uint32_t groundColor(Ground g) {
  switch (g) {
    case Ground::DeepWater: return rgba(30, 60, 120);
    case Ground::Water: return rgba(50, 100, 170);
    case Ground::Sand: return rgba(220, 200, 140);
    case Ground::Swamp: return rgba(80, 96, 60);
    case Ground::Grass: return rgba(96, 160, 70);
    case Ground::Meadow: return rgba(120, 175, 80);
    case Ground::ForestFloor: return rgba(60, 110, 50);
    case Ground::Autumn: return rgba(170, 110, 50);
    case Ground::Tundra: return rgba(120, 140, 110);
    case Ground::Snow: return rgba(235, 240, 245);
    case Ground::Dirt: return rgba(140, 110, 70);
    case Ground::Farmland: return rgba(110, 80, 50);
    case Ground::Road: return rgba(190, 170, 130);
    case Ground::Plaza: return rgba(170, 170, 170);
    case Ground::Rock: return rgba(110, 104, 100);
    case Ground::Bridge: return rgba(150, 100, 60);
    case Ground::StoneFloor: return rgba(150, 150, 160);
    default: return rgba(255, 0, 255);
  }
}

// ---- output: in range mode (--seeds A..B) only FAIL lines are printed (tagged with the seed), plus one summary per seed
bool g_quiet = false;
uint64_t g_curSeed = 0;
void out(const char* fmt, ...) {
  if (g_quiet && strncmp(fmt, "FAIL", 4) != 0) return;
  if (g_quiet) printf("  [seed %llu] ", (unsigned long long)g_curSeed);
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
}

// ---- repetition audit: informational numbers that later generator tasks should raise (PLAN.md task 1)
struct Audit {
  int poiKinds = 0, poiCount = 0;          // distinct site kinds / sites within 60 tiles of the start
  std::string poiList;
  int settlements = 0, layoutSigs = 0, layoutMaxGroup = 0;   // settlement layout signatures (world-wide)
  int nearSettlements = 0, nearLayoutMaxGroup = 0;           // ... within 120 tiles of the start
  int countMaxGroup = 0;                                     // settlements of one type with the same building count
  int greetTowns = 0, greetTalks = 0, greetDistinct = 0, greetWorstPct = 100;
  std::string greetWorst;
  int bldgs = 0, bldgSigs = 0, bldgMaxGroup = 0;             // buildings with identical (type, w, h, roof)
  std::string bldgMaxDesc;
};

const char* bldgName(art::Building b) {
  static const char* n[] = {"house", "stonehouse", "inn", "smithy", "shop", "temple", "keep", "tower", "farmhouse", "hut"};
  return (int)b < (int)(sizeof(n) / sizeof(n[0])) ? n[(int)b] : "?";
}
bool isSettlement(SiteType t) { return t == SiteType::City || t == SiteType::Town || t == SiteType::Village; }

Audit repetitionAudit(uint64_t seed) {
  Audit au;
  Game g(seed);
  g.newGame(seed);
  g.mode = Mode::Play;
  const World& W = g.world;
  const Map& m = W.over;
  int sx = (int)(g.pl().p.x / TILE), sy = (int)(g.pl().p.y / TILE);
  auto distStart = [&](const Site& s) { return std::hypot((float)(s.ex - sx), (float)(s.ey - sy)); };

  // 1. point-of-interest kinds near the start (today: site types; vignettes join this list in task 4)
  {
    bool kind[(int)SiteType::COUNT] = {};
    for (int si = 0; si < (int)W.sites.size(); si++) {
      if (si == W.startSite || distStart(W.sites[si]) > 60) continue;
      au.poiCount++;
      kind[(int)W.sites[si].type] = true;
    }
    for (int t = 0; t < (int)SiteType::COUNT; t++)
      if (kind[t]) { au.poiKinds++; if (!au.poiList.empty()) au.poiList += ","; au.poiList += siteTypeName((SiteType)t); }
  }

  // 2. settlement layout: the "shape" (type, centrepiece, size of the paved square) and the building count.
  //    Today a layout archetype is not stored, so these stand in for it until task 3 adds real archetypes.
  {
    std::map<std::string, int> shape, nearShape, count;
    for (const Site& s : W.sites) {
      if (!isSettlement(s.type)) continue;
      int plaza = 0;
      for (int y = s.r.y; y < s.r.y + s.r.h; y++)
        for (int x = s.r.x; x < s.r.x + s.r.w; x++) if (m.at(x, y) == Ground::Plaza) plaza++;
      std::string sh = std::to_string((int)s.type) + "/" + std::to_string(m.propAt(s.ex, s.ey)) + "/" + std::to_string(plaza / 10);
      au.settlements++;
      shape[sh]++;
      count[std::to_string((int)s.type) + "/" + std::to_string(s.bldgCount)]++;
      if (distStart(s) <= 120) { au.nearSettlements++; nearShape[sh]++; }
    }
    au.layoutSigs = (int)shape.size();
    for (auto& kv : shape) au.layoutMaxGroup = std::max(au.layoutMaxGroup, kv.second);
    for (auto& kv : nearShape) au.nearLayoutMaxGroup = std::max(au.nearLayoutMaxGroup, kv.second);
    for (auto& kv : count) au.countMaxGroup = std::max(au.countMaxGroup, kv.second);
  }

  // 3. distinct greeting lines per town: walk up to every outdoor NPC of each settlement and say hello
  {
    g.godMode = true;
    for (int si = 0; si < (int)W.sites.size(); si++) {
      if (!isSettlement(W.sites[si].type)) continue;
      g.world.sites[si].discovered = true;
      if (!g.fastTravel(si)) continue;
      g.hour = 12;
      for (int f = 0; f < 3; f++) g.update(SIM_DT, Input());
      std::vector<int> ids;
      for (size_t k = 1; k < g.actors.size(); k++) if (g.actors[k].npc && g.actors[k].site == si) ids.push_back(g.actors[k].id);
      std::set<std::string> lines;
      int talks = 0;
      for (int id : ids) {
        int k = -1;
        for (size_t j = 1; j < g.actors.size(); j++) if (g.actors[j].id == id) k = (int)j;
        if (k < 0) continue;
        g.mode = Mode::Play;
        g.pl().p = g.actors[k].p + Vec2(0, 12);
        g.hour = 12;
        Input talk; talk.interact = true;
        g.update(SIM_DT, talk);
        if (g.mode == Mode::Dialogue && g.dlg.actor == id) { lines.insert(g.dlg.text); talks++; }
        g.closeDialogue();
        g.mode = Mode::Play;
      }
      g.events.clear();
      if (!talks) continue;
      au.greetTowns++;
      au.greetTalks += talks;
      au.greetDistinct += (int)lines.size();
      int pct = (int)(100 * lines.size() / talks);
      if (talks >= 3 && pct < au.greetWorstPct) { au.greetWorstPct = pct; au.greetWorst = W.sites[si].name + " " + std::to_string(lines.size()) + "/" + std::to_string(talks); }
    }
  }

  // 4. the largest group of buildings that look the same: identical (type, w, h, roof)
  {
    std::map<std::tuple<int, int, int, uint32_t>, int> grp;
    for (const Bldg& b : m.bldgs) grp[{(int)b.type, b.r.w, b.r.h, b.roof}]++;
    au.bldgs = (int)m.bldgs.size();
    au.bldgSigs = (int)grp.size();
    for (auto& kv : grp)
      if (kv.second > au.bldgMaxGroup) {
        au.bldgMaxGroup = kv.second;
        char d[96];
        snprintf(d, sizeof d, "%s %dx%d roof %06x", bldgName((art::Building)std::get<0>(kv.first)), std::get<1>(kv.first), std::get<2>(kv.first), std::get<3>(kv.first) & 0xFFFFFF);
        au.bldgMaxDesc = d;
      }
  }
  return au;
}

void printAudit(const Audit& a) {
  printf("  audit: poi kinds within 60 tiles %d (%d sites: %s)\n", a.poiKinds, a.poiCount, a.poiList.c_str());
  printf("  audit: settlement shapes %d distinct of %d, largest same-shape group %d (within 120 tiles: %d settlements, group %d), same building count %d\n",
         a.layoutSigs, a.settlements, a.layoutMaxGroup, a.nearSettlements, a.nearLayoutMaxGroup, a.countMaxGroup);
  printf("  audit: greetings %d distinct of %d talks in %d towns (worst %s)\n", a.greetDistinct, a.greetTalks, a.greetTowns,
         a.greetWorst.empty() ? "-" : a.greetWorst.c_str());
  printf("  audit: buildings %d, %d distinct looks, largest identical group %d (%s)\n", a.bldgs, a.bldgSigs, a.bldgMaxGroup, a.bldgMaxDesc.c_str());
}

struct SeedResult { int bad = 0; double genMs = 0; int sites = 0, kills = 0, level = 1; float maxStep = 0; };

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
  if (counts[(int)SiteType::City] < 2 || counts[(int)SiteType::Town] < 3 || counts[(int)SiteType::Cave] < 8 || mq < 3 || g.world.lair < 0) { out("FAIL: too few sites\n"); bad++; }
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
        if (story) { out("FAIL: %s (%s) is not reachable on foot from the start\n", s.name.c_str(), siteTypeName(s.type)); bad++; }
        else if (town) { out("FAIL: settlement %s is not reachable on foot\n", s.name.c_str()); bad++; }
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
    in.bow = (f % 97) == 0;
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
          q.dialogueChoose(0);
          out("quests now %zu: %s\n", q.quests.size(), q.quests.back().title.c_str());
          if (q.quests.size() < 2) { out("FAIL: quest not accepted\n"); bad++; }
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

// ---- game-feel metrics (PLAN.md "Game-feel and balance targets"): a bot that fights like an average player
// It walks in, swings when in reach, backs off when out of stamina, and rolls away from a telegraphed attack it
// notices (skill = chance of noticing each windup). Heavy attacks (bear/troll) are noticed more often (they read
// from further away) but the bot still misses some.
struct Bot {
  Rng r{1};
  float skill = 0.5f;
  std::map<long long, bool> seen;   // (enemy id, windup start) -> will roll for it
  int swingCd = 0, bowCd = 0;
  int rolls = 0;
  bool retreat = false;
  std::map<int, float> ignore;      // unreachable targets (stuck chasing them) -> until when
  int chase = -1; float chaseT = 0, chaseBest = 1e9f;
  Vec2 chaseP;
  Input think(Game& g, bool ranged, int* targetOut = nullptr) {
    Input in;
    const Actor& p = g.pl();
    int tgt = -1; float bd = 200 * 200;
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& e = g.actors[k];
      if (!e.hostile || e.st == AState::Dead || e.fly) continue;
      auto ig = ignore.find(e.id);
      if (ig != ignore.end() && ig->second > g.time) continue;
      float d2 = len2(e.p - p.p);
      if (d2 < bd) { bd = d2; tgt = (int)k; }
    }
    if (targetOut) *targetOut = tgt;
    if (swingCd > 0) swingCd--;
    if (bowCd > 0) bowCd--;
    // dodge: a telegraphed swing we noticed, about to land
    for (size_t k = 1; k < g.actors.size(); k++) {
      const Actor& e = g.actors[k];
      if (!e.hostile || e.st != AState::Windup) continue;
      float d = len(e.p - p.p);
      if (d > e.range + (e.lunge ? 48 : e.heavy ? 34 : 26)) continue;
      float wu = e.heavy ? (e.mon == art::Monster::Troll ? 0.9f : 0.8f) : e.windup;
      long long key = (long long)e.id * 100000 + (long long)((g.time - e.stT) * 20);
      auto it = seen.find(key);
      if (it == seen.end()) {
        bool heavy = e.heavy;
        it = seen.emplace(key, r.f() < (heavy ? std::min(0.9f, skill + 0.3f) : skill)).first;
      }
      if (it->second && e.stT >= wu - 0.14f && g.stamina >= 20) {
        Vec2 away = norm(p.p - e.p);
        Vec2 side(-away.y, away.x);
        if (r.f() < 0.5f) side = side * -1.0f;
        in.move = norm(away + side * 0.8f);
        in.roll = true;
        it->second = false;
        rolls++;
        return in;
      }
    }
    if (tgt < 0) return in;
    if (retreat && p.hp < p.maxHp * 0.25f) {   // out of potions and nearly dead: run (regen comes back once clear)
      bool pots = false;
      for (auto& it : g.inv) if (it.kind == ItemKind::Potion && it.sub == 0) pots = true;
      if (!pots) { in.move = norm(p.p - g.actors[tgt].p); if (g.stamina > 40 && r.f() < 0.02f) in.roll = true; return in; }
    }
    const Actor& e = g.actors[tgt];
    float d = std::sqrt(bd);
    // give up on a target we cannot get closer to for 4 s (across water, behind rock)
    // (progress is measured across targets: a stuck pack swaps "nearest" every few frames)
    if (chase < 0 || g.time - chaseT > 8.0f) { chase = e.id; chaseT = g.time; chaseBest = d; chaseP = p.p; }
    float reach0 = 17 + e.radius;
    if (d < chaseBest - 8 || d <= reach0) { chaseBest = d; chaseT = g.time; chaseP = p.p; }
    if (g.time - chaseT > 4.0f && d > reach0) {
      ignore[e.id] = g.time + 20.0f;
      for (const Actor& o : g.actors) if (o.hostile && (len2(o.p - p.p) < 90 * 90 || len2(o.p - e.p) < 60 * 60)) ignore[o.id] = g.time + 20.0f;
      chase = -1; if (targetOut) *targetOut = -1; return in;
    }
    if (g.time - chaseT > 1.5f && d > reach0) {   // blocked: walk around the obstacle
      Vec2 to0 = norm(e.p - p.p);
      in.move = Vec2(-to0.y, to0.x) * ((e.id & 1) ? 1.0f : -1.0f) + to0 * 0.3f;
      return in;
    }
    Vec2 to = norm(e.p - p.p);
    float reach = 17 + e.radius;
    if (g.stamina < 9 && d < 40) { in.move = to * -1.0f; return in; }   // winded: back off and recover
    if (ranged && d > 60 && bowCd == 0) { in.move = to * 0.2f; in.bow = true; bowCd = 30; return in; }
    if (d > reach) in.move = to;
    else if (swingCd == 0) { in.attack = true; swingCd = 9; in.move = to * 0.01f; }
    return in;
  }
};

// a quiet open patch of grassland far from every site, where a test fight can run undisturbed
bool arenaSpot(Game& g, int& ax, int& ay, int rx = 6, int ry = 4);
bool arenaSpot(Game& g, int& ax, int& ay, int rx, int ry) {
  const Map& m = g.world.over;
  const Site& home = g.world.sites[g.world.startSite];
  int best = -1; float bd = 1e30f;
  for (int y = 12; y < m.h - 12; y += 3)
    for (int x = 12; x < m.w - 12; x += 3) {
      Biome b = m.biomeAt(x, y);
      if (b != Biome::Plains && b != Biome::Forest && b != Biome::Autumn) continue;
      if (g.world.siteAt(x, y, 12) >= 0) continue;
      bool open = true;
      for (int oy = -ry; oy <= ry && open; oy++) for (int ox = -rx; ox <= rx && open; ox++) if (m.blocked(x + ox, y + oy)) open = false;
      if (!open) continue;
      float d = std::hypot((float)(x - home.r.cx()), (float)(y - home.r.cy()));
      if (d < bd) { bd = d; best = y * m.w + x; }
    }
  if (best < 0) return rx > 3 ? arenaSpot(g, ax, ay, rx - 1, ry - 1) : false;
  ax = best % m.w; ay = best / m.w;
  return true;
}

// a level-L character: health perks every level, a weapon and (from level 3) armour of their level
void makeHero(Game& g, int L) {
  g.plLevel = L;
  g.perkPts = L - 1;
  for (int k = 1; k < L; k++) g.chooseLevelUp(0);
  g.mode = Mode::Play;
  g.inv[g.eqWeapon].power = (int16_t)(8 + (L - 1) / 2);
  if (L >= 3) {
    Item a; a.kind = ItemKind::Armor; a.power = (int16_t)(8 + L / 2); a.name = "LEATHER ARMOR"; a.icon = art::Icon::Armor; a.value = 40;
    g.inv.push_back(a);
    g.useItem((int)g.inv.size() - 1);
  }
  g.pl().hp = g.pl().maxHp; g.stamina = g.maxSt;
}

struct FightResult { bool died = false, won = false; float secs = 0, hpLeft = 0; int hits = 0, rolls = 0, hitsTaken = 0; };

FightResult arenaFight(uint64_t seed, int trial, art::Monster mon, int n, int L, float timeout, bool fleeWins) {
  FightResult fr;
  Game g(seed);
  g.newGame(seed);
  g.mode = Mode::Play;
  g.noWildSpawns = true;
  g.hour = 12;
  int ax, ay;
  if (!arenaSpot(g, ax, ay)) { static bool warned = false; if (!warned) printf("  (seed %llu: no open arena spot)\n", (unsigned long long)seed); warned = true; return fr; }
  g.pl().p = Vec2(ax * TILE + 8.0f, ay * TILE + 8.0f);
  for (int f = 0; f < 4; f++) g.update(SIM_DT, Input());
  g.actors.resize(1);
  makeHero(g, L);
  Rng rr((uint32_t)(seed * 7919 + trial * 104729 + (int)mon * 31 + L));
  std::vector<int> ids;
  float a0 = rr.f() * TAU;
  for (int k = 0; k < n; k++) {
    float a = a0 + k * TAU / n + rr.range(-0.3f, 0.3f);
    ids.push_back(g.debugSpawnAt(mon, g.pl().p + Vec2(std::cos(a), std::sin(a) * 0.7f) * rr.range(70, 95), L));
  }
  Bot bot; bot.r = Rng((uint32_t)(seed * 31 + trial * 977 + L));
  std::map<int, float> lastHp;
  float plHp = g.pl().hp;
  int frames = (int)(timeout * 60);
  for (int f = 0; f < frames; f++) {
    Input in = bot.think(g, false);
    g.update(SIM_DT, in);
    g.events.clear();
    if (g.mode == Mode::LevelUp || g.mode == Mode::Dialogue) g.mode = Mode::Play;
    if (getenv("RPG_TRACE_ARENA") && trial == 0 && f % 6 == 0) {
      printf("  t%5.2f pl hp %3.0f st %d stam %2.0f |", f / 60.0f, g.pl().hp, (int)g.pl().st, g.stamina);
      for (size_t j = 1; j < g.actors.size(); j++) {
        const Actor& e = g.actors[j];
        printf(" [st%d d%3.0f cd%4.1f sp%3.1f %c%c hp%2.0f]", (int)e.st, len(e.p - g.pl().p), e.atkCd, e.special, e.lunge ? 'L' : '-', e.aggro ? 'A' : '-', e.hp);
      }
      printf("\n");
    }
    if (g.pl().hp < plHp - 0.5f) fr.hitsTaken++;
    plHp = g.pl().hp;
    int alive = 0;
    for (int id : ids) {
      int k = -1;
      for (size_t j = 1; j < g.actors.size(); j++) if (g.actors[j].id == id) k = (int)j;
      if (k < 0) continue;
      const Actor& e = g.actors[k];
      auto it = lastHp.find(id);
      if (it != lastHp.end() && e.hp < it->second - 0.01f && g.pl().st == AState::Strike) fr.hits++;
      lastHp[id] = e.hp;
      if (e.st != AState::Dead && !(fleeWins && e.fleeing)) alive++;   // a fleeing enemy has lost the fight
    }
    if (g.mode == Mode::Dead) { fr.died = true; fr.secs = f / 60.0f; return fr; }
    if (!alive) { fr.won = true; fr.secs = f / 60.0f; fr.hpLeft = g.pl().hp / g.pl().maxHp; fr.rolls = bot.rolls; return fr; }
  }
  fr.secs = timeout;
  fr.rolls = bot.rolls;
  return fr;
}

struct ProgResult { float lvl2 = -1, lvl5 = -1; int deaths = 0, potions = 0, kills = 0, level = 1, gold = 0, dens = 0; std::string killers; };

ProgResult progression(uint64_t seed, float secs) {
  ProgResult pr;
  Game g(seed);
  g.newGame(seed);
  g.mode = Mode::Play;
  Bot bot; bot.r = Rng((uint32_t)seed * 2654435761u); bot.retreat = true;
  Rng r(seed);
  Input wander;
  Vec2 lastPos;
  int insideT = 0;
  int frames = (int)(secs * 60);
  for (int f = 0; f < frames; f++) {
    int tgt = -1;
    Input in = bot.think(g, true, &tgt);
    if (tgt < 0) {
      // wander; turn when blocked; walk back out of buildings and dungeons after a while
      if (f % 240 == 0 || (f % 30 == 0 && len2(g.pl().p - lastPos) < 4 * 4)) { float a = r.f() * TAU; wander.move = Vec2(std::cos(a), std::sin(a)); }
      if (f % 30 == 0) lastPos = g.pl().p;
      in.move = wander.move;
      if (g.inside) {
        insideT++;
        if (insideT > (g.subBldg >= 0 ? 300 : 3600)) {
          Vec2 ex(g.sub.exitX * TILE + 8.0f, g.sub.exitY * TILE + 8.0f);
          Vec2 d = ex - g.pl().p;
          in.move = len2(d) > 30 * 30 || f % 120 < 90 ? norm(d) : wander.move;
          if (len2(d) < 24 * 24) in.move = std::fabs(d.x) > 2 ? Vec2(d.x > 0 ? 1.0f : -1.0f, 0) : Vec2(0, 1);
        }
      } else insideT = 0;
      in.interact = (f % 60) == 0;
    }
    if (g.pl().hp < g.pl().maxHp * 0.3f && f % 30 == 0) {
      int before = 0; for (auto& it : g.inv) if (it.kind == ItemKind::Potion) before += it.count;
      in.potion = true;
      g.update(SIM_DT, in);
      int after = 0; for (auto& it : g.inv) if (it.kind == ItemKind::Potion) after += it.count;
      if (after < before) pr.potions++;
    } else g.update(SIM_DT, in);
    g.events.clear();
    if (g.mode == Mode::Dialogue) g.dialogueChoose(0), g.mode = Mode::Play;
    if (g.mode == Mode::Shop || g.mode == Mode::LevelUp) g.mode = Mode::Play;
    while (g.perkPts > 0) { g.chooseLevelUp(0); g.mode = Mode::Play; }
    if (g.mode == Mode::Dead) {
      pr.deaths++;
      const Actor* k = nullptr; float kd = 1e9f;
      for (size_t j = 1; j < g.actors.size(); j++) { const Actor& e = g.actors[j]; if (e.hostile && e.st != AState::Dead && len2(e.p - g.pl().p) < kd) { kd = len2(e.p - g.pl().p); k = &e; } }
      if (k) { if (!pr.killers.empty()) pr.killers += ","; pr.killers += k->name + "@" + std::to_string(k->level); }
      g.respawn();
    }
    if (g.inside && f % 600 == 0 && r.f() < 0.5f) { /* wander out of buildings eventually */ }
    float t = f / 60.0f;

    if (getenv("RPG_TRACE") && f > 25200 && f < 25500 && f % 15 == 0) {
      printf("      st %d move %.2f,%.2f tgt %d stam %.0f hp %.0f p %.1f,%.1f time %.2f chaseT %.2f best %.1f", (int)g.pl().st, in.move.x, in.move.y, tgt, g.stamina, g.pl().hp, g.pl().p.x, g.pl().p.y,
             g.time, bot.chaseT, bot.chaseBest);
      if (tgt >= 0 && tgt < (int)g.actors.size()) printf(" -> %s at %.0f", g.actors[tgt].name.c_str(), len(g.actors[tgt].p - g.pl().p));
      printf("\n");
    }
    if (getenv("RPG_TRACE") && f % 3600 == 0)
      printf("    t %4.0f pos %d,%d inside %d bldg %d site %d mode %d lvl %d kills %d loc %s\n", t, (int)(g.pl().p.x / TILE), (int)(g.pl().p.y / TILE), g.inside, g.subBldg, g.subSite,
             (int)g.mode, g.plLevel, g.kills, g.locName.c_str());
    if (pr.lvl2 < 0 && g.plLevel >= 2) pr.lvl2 = t;
    if (pr.lvl5 < 0 && g.plLevel >= 5) pr.lvl5 = t;
  }
  pr.kills = g.kills; pr.level = g.plLevel; pr.gold = g.gold;
  for (auto& kv : g.killedSlots) if (kv.first == -1) pr.dens = (int)kv.second.size();
  return pr;
}

int runMetrics(uint64_t A, uint64_t B, float progSecs) {
  printf("game-feel metrics, seeds %llu..%llu (bot skill 0.5: notices half the normal windups)\n", (unsigned long long)A, (unsigned long long)B);
  // 1. progression
  if (progSecs > 0) {
    double s2 = 0, s5 = 0; int n2 = 0, n5 = 0, deaths = 0, pots = 0, n = 0, kills = 0, dens = 0;
    for (uint64_t s = A; s <= B; s++) {
      ProgResult p = progression(s, progSecs);
      printf("  prog seed %-4llu lvl2 %5.0fs  lvl5 %5.0fs  final lvl %d  kills %3d  deaths %d  potions %d  gold %d  dens cleared %d  killed by: %s\n", (unsigned long long)s,
             p.lvl2, p.lvl5, p.level, p.kills, p.deaths, p.potions, p.gold, p.dens, p.killers.c_str());
      fflush(stdout);
      if (p.lvl2 >= 0) { s2 += p.lvl2; n2++; }
      if (p.lvl5 >= 0) { s5 += p.lvl5; n5++; }
      deaths += p.deaths; pots += p.potions; kills += p.kills; dens += p.dens; n++;
    }
    printf("METRIC first level-up: avg %.1f min (%d/%d reached)   [target 3-5 min]\n", n2 ? s2 / n2 / 60 : -1.0, n2, n);
    printf("METRIC level 5: avg %.1f min (%d/%d reached in %.0f min)   [target 25-35 min]\n", n5 ? s5 / n5 / 60 : -1.0, n5, n, progSecs / 60);
    printf("METRIC bot run: deaths %.2f, potions %.2f, kills %.0f, dens cleared %.1f per run\n", (double)deaths / n, (double)pots / n, (double)kills / n, (double)dens / n);
  }
  // 2. three same-level wolves, no potions
  for (int L : {1, 3, 5}) {
    int died = 0, trials = 0; double secs = 0, hp = 0; int won = 0, taken = 0, rolls = 0;
    for (uint64_t s = A; s <= B; s++)
      for (int t = 0; t < 8; t++) {
        FightResult fr = arenaFight(s, t, art::Monster::Wolf, 3, L, 90, true);
        trials++; if (fr.died) died++; taken += fr.hitsTaken; rolls += fr.rolls;
        if (fr.won) { won++; secs += fr.secs; hp += fr.hpLeft; }
      }
    printf("METRIC 3 wolves lvl %d, no potions: died %d%% (%d/%d), won fights %.1fs avg, %.0f%% hp left, %.1f hits taken, %.1f rolls   [target ~25%%]\n", L, 100 * died / std::max(1, trials), died, trials,
           won ? secs / won : 0.0, won ? 100 * hp / won : 0.0, (double)taken / trials, (double)rolls / trials);
    fflush(stdout);
  }
  // 3. hits to kill one same-level enemy (melee only), fight length, rolls the bot made
  {
    struct M { art::Monster m; const char* n; };
    const M ms[] = {{art::Monster::Wolf, "wolf"}, {art::Monster::Boar, "boar"}, {art::Monster::Goblin, "goblin"}, {art::Monster::Skeleton, "skeleton"},
                    {art::Monster::Bear, "bear"}, {art::Monster::Troll, "troll"}};
    for (int L : {1, 3, 5}) {
      printf("METRIC hits to kill at lvl %d:", L);
      for (const M& m : ms) {
        double hits = 0, secs = 0, rolls = 0; int n = 0, died = 0;
        for (uint64_t s = A; s <= B; s++)
          for (int t = 0; t < 3; t++) {
            FightResult fr = arenaFight(s, 100 + t, m.m, 1, L, 120, false);
            if (fr.died) died++;
            if (!fr.won) continue;
            hits += fr.hits; secs += fr.secs; rolls += fr.rolls; n++;
          }
        printf("  %s %.1f (%.0fs, %.1f rolls%s)", m.n, n ? hits / n : -1.0, n ? secs / n : 0.0, n ? rolls / n : 0.0, died ? (std::string(", ") + std::to_string(died) + " died").c_str() : "");
      }
      printf("   [target 3-4 small, 8-12 bear/troll]\n");
      fflush(stdout);
    }
  }
  return 0;
}

// "A..B", "A-B" or a single number
bool parseRange(const char* s, uint64_t& a, uint64_t& b) {
  char* e = nullptr;
  a = strtoull(s, &e, 10);
  if (e == s) return false;
  if (*e == 0) { b = a; return true; }
  if (e[0] == '.' && e[1] == '.') e += 2; else if (e[0] == '-') e += 1; else return false;
  const char* s2 = e;
  b = strtoull(s2, &e, 10);
  return e != s2 && *e == 0 && b >= a;
}
}  // namespace

int main(int argc, char** argv) {
  uint64_t seed = 12345, seedA = 0, seedB = 0;
  bool range = false, audit = true;
  const char* mapOut = nullptr;
  float secs = 120;
  bool mortal = false, metrics = false;
  float metricSecs = 2400;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--map") && i + 1 < argc) mapOut = argv[++i];
    else if (!strcmp(argv[i], "--metrics")) metrics = true;
    else if (!strcmp(argv[i], "--metric-secs") && i + 1 < argc) metricSecs = (float)atof(argv[++i]);
    else if (!strcmp(argv[i], "--secs") && i + 1 < argc) secs = (float)atof(argv[++i]);
    else if (!strcmp(argv[i], "--mortal")) mortal = true;
    else if (!strcmp(argv[i], "--noaudit")) audit = false;
    else if (!strcmp(argv[i], "--seeds") && i + 1 < argc) {
      if (!parseRange(argv[++i], seedA, seedB)) { printf("bad --seeds range '%s' (use A..B)\n", argv[i]); return 2; }
      range = true;
    } else seed = (uint64_t)atoll(argv[i]);
  }
  if (metrics) {
    if (!range) { seedA = 1; seedB = 5; }
    return runMetrics(seedA, seedB, metricSecs);
  }
  if (!range) {
    SeedResult r;
    int bad = runSeed(seed, mapOut, secs, mortal, r);
    if (audit) printAudit(repetitionAudit(seed));
    printf(bad ? "FAILED (%d)\n" : "ALL OK\n", bad);
    return bad ? 1 : 0;
  }

  // range mode: one summary line per seed, then the totals
  g_quiet = true;
  int pass = 0, fail = 0;
  std::vector<uint64_t> failed;
  Audit sum;
  int n = 0, minPoi = 1 << 30, maxLayout = 0, maxNearLayout = 0, maxCount = 0, maxBldg = 0, minGreetPct = 100;
  for (uint64_t s = seedA; s <= seedB; s++) {
    SeedResult r;
    int bad = runSeed(s, nullptr, secs, mortal, r);
    if (bad) { fail++; failed.push_back(s); } else pass++;
    printf("seed %-6llu %s  gen %4.0f ms  sites %3d  bot kills %3d lvl %2d  step %.2f ms", (unsigned long long)s, bad ? "FAIL" : "ok  ",
           r.genMs, r.sites, r.kills, r.level, r.maxStep);
    if (audit) {
      Audit a = repetitionAudit(s);
      int gp = a.greetTalks ? 100 * a.greetDistinct / a.greetTalks : 100;
      printf("  | poi %d  shapes %d/%d (grp %d, near %d, count %d)  greet %d%%  bldg grp %d", a.poiKinds, a.layoutSigs, a.settlements, a.layoutMaxGroup,
             a.nearLayoutMaxGroup, a.countMaxGroup, gp, a.bldgMaxGroup);
      n++;
      sum.poiKinds += a.poiKinds; sum.settlements += a.settlements; sum.layoutSigs += a.layoutSigs;
      sum.greetTalks += a.greetTalks; sum.greetDistinct += a.greetDistinct; sum.bldgMaxGroup += a.bldgMaxGroup;
      minPoi = std::min(minPoi, a.poiKinds); maxLayout = std::max(maxLayout, a.layoutMaxGroup);
      maxNearLayout = std::max(maxNearLayout, a.nearLayoutMaxGroup); maxCount = std::max(maxCount, a.countMaxGroup); maxBldg = std::max(maxBldg, a.bldgMaxGroup);
      minGreetPct = std::min(minGreetPct, gp);
    }
    printf("\n");
    fflush(stdout);
  }
  if (audit && n) {
    printf("repetition audit over %d seeds (informational):\n", n);
    printf("  poi kinds within 60 tiles: avg %.1f, min %d (target >= 8)\n", (double)sum.poiKinds / n, minPoi);
    printf("  settlement shapes: %.0f%% distinct, worst same-shape group %d world-wide, %d within 120 tiles (target 1), same building count %d\n",
           sum.settlements ? 100.0 * sum.layoutSigs / sum.settlements : 0.0, maxLayout, maxNearLayout, maxCount);
    printf("  greetings: %.0f%% distinct per town on average, worst seed %d%%\n", sum.greetTalks ? 100.0 * sum.greetDistinct / sum.greetTalks : 0.0, minGreetPct);
    printf("  identical buildings: avg largest group %.1f, worst %d\n", (double)sum.bldgMaxGroup / n, maxBldg);
  }
  printf("%d passed, %d failed", pass, fail);
  if (!failed.empty()) { printf(" (seeds"); for (uint64_t s : failed) printf(" %llu", (unsigned long long)s); printf(")"); }
  printf("\n");
  return fail ? 1 : 0;
}
