// Headless EMBERVALE checks: world generation stats + overview map PNG, a wandering combat bot, save round-trip.
//   rpg_test [seed] [--map out.png] [--secs N]
#include <chrono>
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
}  // namespace

int main(int argc, char** argv) {
  uint64_t seed = 12345;
  const char* mapOut = nullptr;
  float secs = 120;
  bool mortal = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--map") && i + 1 < argc) mapOut = argv[++i];
    else if (!strcmp(argv[i], "--secs") && i + 1 < argc) secs = (float)atof(argv[++i]);
    else if (!strcmp(argv[i], "--mortal")) mortal = true;
    else seed = (uint64_t)atoll(argv[i]);
  }
  auto t0 = std::chrono::steady_clock::now();
  Game g(seed);
  g.newGame(seed);
  auto t1 = std::chrono::steady_clock::now();
  printf("seed %llu generated in %.0f ms\n", (unsigned long long)seed, std::chrono::duration<double, std::milli>(t1 - t0).count());
  int counts[(int)SiteType::COUNT] = {};
  for (auto& s : g.world.sites) counts[(int)s.type]++;
  for (int t = 0; t < (int)SiteType::COUNT; t++) printf("  %-13s %d\n", siteTypeName((SiteType)t), counts[t]);
  printf("  buildings %zu, spawns %zu, start %s, capital %s, lair %d\n", g.world.over.bldgs.size(), g.world.over.spawns.size(),
         g.world.sites[g.world.startSite].name.c_str(), g.world.sites[g.world.capital].name.c_str(), g.world.lair);
  int mq = 0;
  for (auto& s : g.world.sites) if (s.mainQuest) mq++;
  printf("  main-quest ruins %d\n", mq);
  int bad = 0;
  if (counts[(int)SiteType::City] < 2 || counts[(int)SiteType::Town] < 3 || counts[(int)SiteType::Cave] < 8 || mq < 3 || g.world.lair < 0) { printf("FAIL: too few sites\n"); bad++; }
  if (g.map().blocked((int)(g.pl().p.x / 16), (int)(g.pl().p.y / 16))) { printf("FAIL: player starts inside a wall\n"); bad++; }
  {
    // the main quest needs a jarl in the capital's keep
    const Site& cap = g.world.sites[g.world.capital];
    bool keep = false;
    for (int b = cap.bldgFirst; b < cap.bldgFirst + cap.bldgCount; b++) if (g.world.over.bldgs[b].type == art::Building::Keep) keep = true;
    if (cap.type != SiteType::City || !keep) { printf("FAIL: capital %s has no keep\n", cap.name.c_str()); bad++; }
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
    printf("map %s: %s\n", mapOut, writePng(mapOut, m.w, m.h, px) ? "ok" : "FAILED");
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
        if (story) { printf("FAIL: %s (%s) is not reachable on foot from the start\n", s.name.c_str(), siteTypeName(s.type)); bad++; }
        else if (town) { printf("FAIL: settlement %s is not reachable on foot\n", s.name.c_str()); bad++; }
        else unreachable++;
      }
    }
    if (unreachable) printf("WARN: %d optional sites unreachable on foot\n", unreachable);
  }

  // interiors and dungeons generate and are walkable from their entrance
  for (int si = 0; si < (int)g.world.sites.size(); si++) {
    const Site& s = g.world.sites[si];
    if (s.type != SiteType::Cave && s.type != SiteType::Ruin) continue;
    Map m;
    if (s.type == SiteType::Ruin) genRuin(m, s, s.seed); else genCave(m, s, s.seed);
    bool boss = false;
    for (auto& sp : m.spawns) if (sp.boss) boss = true;
    if (m.blocked(m.exitX, m.exitY - 1)) printf("   exit %d,%d ground %d prop %d solid %d\n", m.exitX, m.exitY, (int)m.at(m.exitX, m.exitY - 1), m.propAt(m.exitX, m.exitY - 1), (int)m.solid[(size_t)(m.exitY - 1) * m.w + m.exitX]);
    if (!boss || m.blocked(m.exitX, m.exitY - 1)) { printf("FAIL: dungeon %s (boss %d, exit blocked %d)\n", s.name.c_str(), boss, m.blocked(m.exitX, m.exitY - 1)); bad++; }
  }
  for (size_t bi = 0; bi < g.world.over.bldgs.size(); bi++) {
    Map m;
    genInterior(m, g.world.over.bldgs[bi], g.world.over.bldgs[bi].seed);
    if (m.blocked(m.exitX, m.exitY - 1)) { printf("FAIL: interior %zu exit blocked\n", bi); bad++; }
    const Bldg& b = g.world.over.bldgs[bi];
    if (g.world.over.blocked(b.doorX(), b.doorY() + 1)) { const Map& o = g.world.over; int ax = b.doorX(), ay = b.doorY() + 1; printf("WARN: building %zu door approach blocked: ground %d prop %d wall %d bldg %d\n", bi, (int)o.at(ax, ay), o.propAt(ax, ay), (int)o.wall[(size_t)ay * o.w + ax], (int)o.bldgAt[(size_t)ay * o.w + ax]); }
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
  printf("deaths %d, potion attempts %d\n", deaths, potions);
  printf("bot: %.0fs, kills %d, lvl %d, actors %zu, events %d, gold %d, max step %.2f ms, inside %d\n", secs, g.kills, g.plLevel, g.actors.size(), evCount, g.gold, maxStep, g.inside);

  // dialogue + quest flow: walk into the start village inn, talk to the innkeeper, take a job
  {
    Game q(seed);
    q.newGame(seed);
    q.mode = Mode::Play;
    const Site& home = q.world.sites[q.world.startSite];
    int inn = -1;
    for (int b = home.bldgFirst; b < home.bldgFirst + home.bldgCount; b++) if (q.world.over.bldgs[b].type == art::Building::Inn) inn = b;
    if (inn < 0) { printf("FAIL: start village has no inn\n"); bad++; }
    else {
      const Bldg& B = q.world.over.bldgs[inn];
      q.pl().p = Vec2(B.doorX() * 16 + 8.0f, B.doorY() * 16 + 10.0f);
      q.update(SIM_DT, Input());
      if (!q.inside) { printf("FAIL: could not enter the inn\n"); bad++; }
      int keeper = -1;
      for (size_t k = 1; k < q.actors.size(); k++) if (q.actors[k].role == Role::Innkeeper) keeper = (int)k;
      if (keeper < 0) { printf("FAIL: no innkeeper\n"); bad++; }
      else {
        q.pl().p = q.actors[keeper].p + Vec2(0, 20);
        Input in2; in2.interact = true;
        q.update(SIM_DT, in2);
        if (q.mode != Mode::Dialogue) { printf("FAIL: talking did not open dialogue\n"); bad++; }
        else {
          printf("dialogue: %s: %s\n", q.dlg.speaker.c_str(), q.dlg.text.c_str());
          for (size_t o = 0; o < q.dlg.opts.size(); o++) printf("   [%zu] %s\n", o, q.dlg.opts[o].label.c_str());
          for (size_t o = 0; o < q.dlg.opts.size(); o++) if (q.dlg.opts[o].label.find("WORK") != std::string::npos) { q.dialogueChoose((int)o); break; }
          printf("offer: %s\n", q.dlg.text.c_str());
          q.dialogueChoose(0);
          printf("quests now %zu: %s\n", q.quests.size(), q.quests.back().title.c_str());
          if (q.quests.size() < 2) { printf("FAIL: quest not accepted\n"); bad++; }
          q.mode = Mode::Play;
          q.update(SIM_DT, in2);
          for (size_t o = 0; o < q.dlg.opts.size(); o++) if (q.dlg.opts[o].label.find("WARES") != std::string::npos) { q.dialogueChoose((int)o); break; }
          printf("shop stock %zu, mode %d\n", q.shop.stock.size(), (int)q.mode);
          int g0 = q.gold;
          int left0 = q.shop.stock.empty() ? 0 : q.shop.stock[0].count;
          size_t n0 = q.shop.stock.size();
          q.buy(0);
          printf("bought for %d gold\n", g0 - q.gold);
          // close and reopen: what was bought must stay sold
          q.mode = Mode::Play;
          q.update(SIM_DT, in2);
          for (size_t o = 0; o < q.dlg.opts.size(); o++) if (q.dlg.opts[o].label.find("WARES") != std::string::npos) { q.dialogueChoose((int)o); break; }
          bool restocked = q.shop.stock.size() > n0 || (q.shop.stock.size() == n0 && !q.shop.stock.empty() && q.shop.stock[0].count >= left0 && left0 > 1);
          if (q.mode != Mode::Shop || restocked) { printf("FAIL: shop refilled after a purchase (%zu items, first x%d)\n", q.shop.stock.size(), q.shop.stock.empty() ? 0 : q.shop.stock[0].count); bad++; }
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
    if (keep < 0 || !mq) { printf("FAIL: main quest setup\n"); bad++; }
    else {
      const Bldg& B = q.world.over.bldgs[keep];
      q.pl().p = Vec2(B.doorX() * 16 + 8.0f, B.doorY() * 16 + 10.0f);
      q.update(SIM_DT, Input());
      int jarl = -1;
      for (size_t k = 1; k < q.actors.size(); k++) if (q.actors[k].role == Role::Jarl) jarl = (int)k;
      if (!q.inside || jarl < 0) { printf("FAIL: no jarl in the keep\n"); bad++; }
      else {
        q.pl().p = q.actors[jarl].p + Vec2(0, 18);
        Input talk; talk.interact = true;
        q.update(SIM_DT, talk);
        int opt = -1;
        for (size_t o = 0; o < q.dlg.opts.size(); o++) if (q.dlg.opts[o].label.find("STOPPED") != std::string::npos) opt = (int)o;
        if (q.mode != Mode::Dialogue || opt < 0) { printf("FAIL: jarl has no main quest option\n"); bad++; }
        else {
          q.dialogueChoose(opt);
          for (auto& qq : q.quests) if (qq.type == QType::Main) mq = &qq;
          printf("main quest after jarl: stage %d (%s)\n", mq->stage, mq->title.c_str());
          if (mq->stage != 2) { printf("FAIL: shards from pre-cleared ruins did not count\n"); bad++; }
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
        if (!q.inside) { printf("FAIL: knockback onto the exit threw the player out\n"); bad++; }
        q.pl().knock = Vec2();
        Input down; down.move = Vec2(0, 1);
        for (int f = 0; f < 3 && q.inside; f++) q.update(SIM_DT, down);
        if (q.inside) { printf("FAIL: walking onto the exit did not leave\n"); bad++; }
      }
      q.pl().p = Vec2(L.ex * 16 + 8.0f, (L.ey + 2) * 16 + 8.0f);
      int dragon = -1;
      for (int f = 0; f < 10 && dragon < 0; f++) {
        q.update(SIM_DT, Input());
        for (size_t k = 1; k < q.actors.size(); k++) if (q.actors[k].mon == art::Monster::Dragon) dragon = (int)k;
      }
      if (dragon < 0) { printf("FAIL: the dragon never appeared at its lair\n"); bad++; }
      else {
        printf("dragon: %.0f hp, level %d\n", q.actors[dragon].maxHp, q.actors[dragon].level);
        q.actors[dragon].hp = 0.5f; q.actors[dragon].burnT = 1.0f;
        for (int f = 0; f < 30; f++) { q.update(SIM_DT, Input()); if (q.mode != Mode::Play) q.mode = Mode::Play; }
        for (auto& qq : q.quests) if (qq.type == QType::Main) mq = &qq;
        printf("after the dragon: stage %d, %s\n", mq->stage, mq->state == QState::Done ? "DONE" : "not done");
        if (mq->state != QState::Done) { printf("FAIL: killing the dragon did not finish the main quest\n"); bad++; }
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
    if (d.mode != Mode::Dead) { printf("FAIL: burning at 1 HP did not kill the player\n"); bad++; }
    else {
      std::vector<uint8_t> ds;
      d.serialize(ds);
      Game e(1);
      if (!e.deserialize(ds) || e.pl().hp < e.pl().maxHp || e.gold != 180) { printf("FAIL: loading a death-screen save did not respawn (hp %.0f, gold %d)\n", e.pl().hp, e.gold); bad++; }
      d.respawn();
      printf("death: respawned in %s with %.0f/%.0f HP, gold 200 -> %d\n", d.locName.c_str(), d.pl().hp, d.pl().maxHp, d.gold);
      if (d.mode != Mode::Play || d.pl().hp < d.pl().maxHp || d.gold != 180) { printf("FAIL: respawn state\n"); bad++; }
    }
  }

  // save round trip
  std::vector<uint8_t> buf;
  g.serialize(buf);
  Game h(1);
  if (!h.deserialize(buf)) { printf("FAIL: save did not load\n"); bad++; }
  else {
    std::vector<uint8_t> buf2;
    h.serialize(buf2);
    if (buf2.size() != buf.size()) { printf("FAIL: save round-trip size %zu vs %zu\n", buf.size(), buf2.size()); bad++; }
    printf("save %zu bytes ok\n", buf.size());
  }
  printf(bad ? "FAILED (%d)\n" : "ALL OK\n", bad);
  return bad ? 1 : 0;
}
