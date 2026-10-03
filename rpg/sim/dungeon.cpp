// Sub-level generation: natural caves (cellular automata), ancient ruins (rooms + corridors), building interiors.
#include <algorithm>
#include <queue>
#include "rpg/sim/world.h"

using art::Prop;

namespace {
void bfs(const Map& m, int sx, int sy, std::vector<int>& dist) {
  dist.assign((size_t)m.w * m.h, -1);
  std::queue<int> q;
  dist[(size_t)sy * m.w + sx] = 0;
  q.push(sy * m.w + sx);
  static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
  while (!q.empty()) {
    int c = q.front(); q.pop();
    int x = c % m.w, y = c / m.w;
    for (int k = 0; k < 4; k++) {
      int nx = x + dx[k], ny = y + dy[k];
      if (!m.in(nx, ny) || groundSolid(m.at(nx, ny))) continue;
      size_t ni = (size_t)ny * m.w + nx;
      if (dist[ni] >= 0) continue;
      dist[ni] = dist[(size_t)c] + 1;
      q.push((int)ni);
    }
  }
}
int wallsAround(const Map& m, int x, int y, Ground wall) {
  int n = 0;
  for (int oy = -1; oy <= 1; oy++)
    for (int ox = -1; ox <= 1; ox++)
      if ((ox || oy) && (!m.in(x + ox, y + oy) || m.at(x + ox, y + oy) == wall)) n++;
  return n;
}
void spawnMonsters(Map& m, const std::vector<int>& dist, int exitX, int exitY, Rng& r, art::Monster theme, int count, int site) {
  std::vector<int> cells;
  for (int i = 0; i < m.w * m.h; i++)
    if (dist[(size_t)i] > 14) cells.push_back(i);
  int slot = 0;
  for (int k = 0; k < count && !cells.empty(); k++) {
    int c = cells[r.irange((int)cells.size())];
    int x = c % m.w, y = c / m.w;
    if (std::abs(x - exitX) + std::abs(y - exitY) < 10) continue;
    Spawn sp; sp.x = x; sp.y = y; sp.site = site; sp.slot = slot++;
    // mixed inhabitants: mostly theme, some extras
    static const art::Monster extra[] = {art::Monster::Bat, art::Monster::Spider, art::Monster::Skeleton, art::Monster::Slime};
    sp.mon = r.f() < 0.7f ? theme : extra[r.irange(4)];
    if (theme == art::Monster::Draugr || theme == art::Monster::Wraith) sp.mon = r.f() < 0.6f ? art::Monster::Draugr : (r.f() < 0.5f ? art::Monster::Skeleton : theme);
    if (theme == art::Monster::Goblin && r.f() < 0.5f) sp.mon = art::Monster::Goblin;
    m.spawns.push_back(sp);
  }
}
}  // namespace

void genCave(Map& m, const Site& s, uint32_t seed) {
  Rng r(seed);
  m.kind = MapKind::Cave;
  m.seed = seed;
  int W = 72 + r.irange(16), H = 56 + r.irange(12);
  m.alloc(W, H, Ground::CaveWall);
  std::vector<uint8_t> a((size_t)W * H), b;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      a[(size_t)y * W + x] = (x < 2 || y < 2 || x >= W - 2 || y >= H - 2) ? 1 : (r.f() < 0.46f ? 1 : 0);
  for (int it = 0; it < 5; it++) {
    b = a;
    for (int y = 1; y < H - 1; y++)
      for (int x = 1; x < W - 1; x++) {
        int n = 0;
        for (int oy = -1; oy <= 1; oy++) for (int ox = -1; ox <= 1; ox++) if (ox || oy) n += a[(size_t)(y + oy) * W + x + ox];
        b[(size_t)y * W + x] = n >= 5 || (it < 2 && n <= 1) ? 1 : 0;
      }
    a.swap(b);
  }
  for (int i = 0; i < W * H; i++) m.ground[(size_t)i] = (uint8_t)(a[(size_t)i] ? Ground::CaveWall : Ground::CaveFloor);
  // exit near the bottom middle: find a floor tile closest to (W/2, H-4)
  int ex = W / 2, ey = H - 4, bd = 1 << 30;
  for (int y = 2; y < H - 2; y++)
    for (int x = 2; x < W - 2; x++)
      if (m.at(x, y) == Ground::CaveFloor && m.at(x, y - 1) == Ground::CaveFloor) {
        int d = (x - W / 2) * (x - W / 2) + (y - (H - 4)) * (y - (H - 4)) * 3;
        if (d < bd) { bd = d; ex = x; ey = y; }
      }
  std::vector<int> dist;
  bfs(m, ex, ey, dist);
  // seal everything unreachable
  int far = 0, fx = ex, fy = ey;
  for (int i = 0; i < W * H; i++) {
    if (m.ground[(size_t)i] == (uint8_t)Ground::CaveFloor && dist[(size_t)i] < 0) m.ground[(size_t)i] = (uint8_t)Ground::CaveWall;
    if (dist[(size_t)i] > far) { far = dist[(size_t)i]; fx = i % W; fy = i / W; }
  }
  m.exitX = ex; m.exitY = ey;
  m.setProp(ex, ey, Prop::Ladder);
  bool icy = s.theme == art::Monster::FrostSpider || s.theme == art::Monster::IceWolf;
  // underground pools
  for (int k = 0; k < 3; k++) {
    int px = 4 + r.irange(W - 8), py = 4 + r.irange(H - 8);
    if (dist[(size_t)py * W + px] < 8) continue;
    for (int oy = -2; oy <= 2; oy++)
      for (int ox = -3; ox <= 3; ox++) {
        int x = px + ox, y = py + oy;
        if (m.at(x, y) == Ground::CaveFloor && ox * ox * 0.5f + oy * oy < 4.5f && dist[(size_t)y * W + x] > 6 && wallsAround(m, x, y, Ground::CaveWall) < 3)
          m.setG(x, y, icy ? Ground::Ice : Ground::Water);
      }
  }
  bfs(m, ex, ey, dist);
  // decor
  for (int y = 1; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) {
      if (m.at(x, y) != Ground::CaveFloor || m.propAt(x, y) || dist[(size_t)y * W + x] < 3) continue;
      int wn = wallsAround(m, x, y, Ground::CaveWall);
      float q = r.f();
      if (wn >= 3 && q < 0.10f) m.setProp(x, y, icy ? Prop::Crystal : (r.f() < 0.5f ? Prop::Stalagmite : Prop::Mushrooms));
      else if (wn >= 2 && q < 0.13f) m.setProp(x, y, s.theme == art::Monster::Spider || s.theme == art::Monster::FrostSpider ? Prop::Cobweb : Prop::Bones);
      else if (q < 0.008f) m.setProp(x, y, Prop::Crystal);
      else if (q < 0.016f) m.setProp(x, y, Prop::Torch);
    }
  // chests in dead ends far from the exit
  int chests = 0;
  for (int tries = 0; tries < 800 && chests < 4; tries++) {
    int x = 2 + r.irange(W - 4), y = 2 + r.irange(H - 4);
    if (m.at(x, y) != Ground::CaveFloor || m.propAt(x, y) || dist[(size_t)y * W + x] < 15) continue;
    if (wallsAround(m, x, y, Ground::CaveWall) < 5) continue;
    m.setProp(x, y, Prop::Chest);
    chests++;
  }
  // boss lair at the far end
  m.setProp(fx, fy, Prop::Chest);
  for (int oy = -2; oy <= 2; oy++)
    for (int ox = -2; ox <= 2; ox++)
      if ((ox || oy) && m.at(fx + ox, fy + oy) == Ground::CaveFloor && m.propAt(fx + ox, fy + oy) == (int)Prop::Torch + 1) m.setP(fx + ox, fy + oy, 0);
  spawnMonsters(m, dist, ex, ey, r, s.theme, 10 + r.irange(6), -1);
  // boss: nearest floor next to the far chest
  Spawn boss; boss.boss = true; boss.slot = 99; boss.x = fx; boss.y = fy;
  static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
  for (int k = 0; k < 4; k++) if (m.at(fx + dx[k], fy + dy[k]) == Ground::CaveFloor && !m.propAt(fx + dx[k], fy + dy[k])) { boss.x = fx + dx[k]; boss.y = fy + dy[k]; break; }
  boss.mon = s.theme == art::Monster::Spider ? art::Monster::Spider : s.theme == art::Monster::Bat ? art::Monster::Troll
           : s.theme == art::Monster::Skeleton ? art::Monster::Draugr : s.theme == art::Monster::Goblin ? art::Monster::Troll : s.theme;
  m.spawns.push_back(boss);
  // the arrival tile above the exit must always be free
  if (m.propAt(m.exitX, m.exitY - 1) == (int)Prop::Chest + 1) m.setProp(m.exitX - 1 >= 0 && !groundSolid(m.at(m.exitX - 1, m.exitY - 1)) ? m.exitX - 1 : m.exitX, m.exitY - 2, Prop::Chest);
  m.setP(m.exitX, m.exitY - 1, 0);
  m.rebuildSolid();
}

void genRuin(Map& m, const Site& s, uint32_t seed) {
  Rng r(seed);
  m.kind = MapKind::Ruin;
  m.seed = seed;
  int W = 70, H = 58;
  m.alloc(W, H, Ground::CaveWall);
  struct Room { IRect rc; };
  std::vector<Room> rooms;
  // entrance hall at the bottom middle
  rooms.push_back({IRect{W / 2 - 4, H - 10, 9, 7}});
  for (int tries = 0; tries < 400 && rooms.size() < 13; tries++) {
    int rw = 6 + r.irange(8), rh = 5 + r.irange(6);
    IRect rc{2 + r.irange(W - rw - 4), 2 + r.irange(H - rh - 14), rw, rh};
    bool ok = true;
    for (auto& o : rooms) if (rc.overlaps(o.rc, 2)) ok = false;
    if (ok) rooms.push_back({rc});
  }
  auto carve = [&](IRect rc) {
    for (int y = rc.y; y < rc.y + rc.h; y++) for (int x = rc.x; x < rc.x + rc.w; x++) m.setG(x, y, Ground::StoneFloor);
  };
  for (auto& rm : rooms) carve(rm.rc);
  // connect each room to its nearest already-connected room with an L corridor (2 wide)
  std::vector<int> conn{0};
  std::vector<uint8_t> isConn(rooms.size(), 0);
  isConn[0] = 1;
  while (conn.size() < rooms.size()) {
    int ba = -1, bb = -1, bd = 1 << 30;
    for (int a : conn)
      for (int b = 0; b < (int)rooms.size(); b++) {
        if (isConn[b]) continue;
        int dx = rooms[a].rc.cx() - rooms[b].rc.cx(), dy = rooms[a].rc.cy() - rooms[b].rc.cy();
        int d = dx * dx + dy * dy;
        if (d < bd) { bd = d; ba = a; bb = b; }
      }
    isConn[bb] = 1; conn.push_back(bb);
    int x0 = rooms[ba].rc.cx(), y0 = rooms[ba].rc.cy(), x1 = rooms[bb].rc.cx(), y1 = rooms[bb].rc.cy();
    bool hFirst = r.f() < 0.5f;
    int cx = hFirst ? x1 : x0, cy = hFirst ? y0 : y1;
    for (int x = std::min(x0, x1); x <= std::max(x0, x1); x++) { m.setG(x, cy, Ground::StoneFloor); m.setG(x, cy + 1, Ground::StoneFloor); }
    for (int y = std::min(y0, y1); y <= std::max(y0, y1); y++) { m.setG(cx, y, Ground::StoneFloor); m.setG(cx + 1, y, Ground::StoneFloor); }
  }
  m.exitX = rooms[0].rc.cx(); m.exitY = rooms[0].rc.y + rooms[0].rc.h - 1;
  m.setProp(m.exitX, m.exitY, Prop::Ladder);
  std::vector<int> dist;
  bfs(m, m.exitX, m.exitY, dist);
  // boss room = furthest room
  int bossRoom = 0, far = -1;
  for (int i = 1; i < (int)rooms.size(); i++) {
    int d = dist[(size_t)rooms[i].rc.cy() * W + rooms[i].rc.cx()];
    if (d > far) { far = d; bossRoom = i; }
  }
  for (int i = 0; i < (int)rooms.size(); i++) {
    IRect rc = rooms[i].rc;
    // braziers in corners, urns and coffins along walls
    m.setProp(rc.x, rc.y, Prop::Brazier);
    m.setProp(rc.x + rc.w - 1, rc.y, Prop::Brazier);
    for (int k = 0; k < rc.w * rc.h / 14; k++) {
      int x = rc.x + r.irange(rc.w), y = rc.y + r.irange(rc.h);
      if (m.propAt(x, y) || (x == m.exitX && std::abs(y - m.exitY) < 3)) continue;
      bool edge = x == rc.x || y == rc.y || x == rc.x + rc.w - 1 || y == rc.y + rc.h - 1;
      if (!edge) { if (r.f() < 0.3f) m.setProp(x, y, r.f() < 0.5f ? Prop::Bones : Prop::Cobweb); continue; }
      static const Prop deco[] = {Prop::Urn, Prop::Coffin, Prop::Urn, Prop::SkullPile, Prop::Cobweb, Prop::Statue};
      m.setProp(x, y, deco[r.irange(6)]);
    }
    if (i != 0 && i != bossRoom && r.f() < 0.4f) {
      int x = rc.x + 1 + r.irange(std::max(1, rc.w - 2)), y = rc.y;
      if (!m.propAt(x, y)) m.setProp(x, y, Prop::Chest);
    }
  }
  IRect br = rooms[bossRoom].rc;
  m.setProp(br.cx(), br.y, Prop::Altar);
  m.setProp(br.cx() + 1, br.y, Prop::Chest);
  spawnMonsters(m, dist, m.exitX, m.exitY, r, s.theme, 12 + r.irange(6), -1);
  Spawn boss; boss.boss = true; boss.slot = 99; boss.x = br.cx(); boss.y = br.cy() + 1;
  boss.mon = s.theme == art::Monster::Wraith ? art::Monster::Wraith : art::Monster::Draugr;
  m.spawns.push_back(boss);
  // the arrival tile above the exit must always be free
  if (m.propAt(m.exitX, m.exitY - 1) == (int)Prop::Chest + 1) m.setProp(m.exitX - 1 >= 0 && !groundSolid(m.at(m.exitX - 1, m.exitY - 1)) ? m.exitX - 1 : m.exitX, m.exitY - 2, Prop::Chest);
  m.setP(m.exitX, m.exitY - 1, 0);
  m.rebuildSolid();
}

void genInterior(Map& m, const Bldg& b, uint32_t seed) {
  Rng r(seed);
  m.kind = MapKind::Interior;
  m.seed = seed;
  int W = std::max(11, b.r.w * 2 + 3), H = std::max(9, b.r.h * 2 + 5);
  if (b.type == art::Building::Keep) { W = 23; H = 15; }
  if (b.type == art::Building::Temple) { W = 15; H = 13; }
  if (b.type == art::Building::Inn) { W = 17; H = 12; }
  if (W % 2 == 0) W++;
  bool stone = b.type == art::Building::Keep || b.type == art::Building::Temple || b.type == art::Building::StoneHouse || b.type == art::Building::Tower;
  m.alloc(W, H, Ground::InteriorWall);
  for (int y = 2; y < H - 1; y++)
    for (int x = 1; x < W - 1; x++) m.setG(x, y, stone ? Ground::StoneFloor : Ground::WoodFloor);
  int ex = W / 2, ey = H - 1;
  m.setG(ex, ey, stone ? Ground::StoneFloor : Ground::WoodFloor);
  m.exitX = ex; m.exitY = ey;
  auto put = [&](int x, int y, Prop p) { if (m.at(x, y) != Ground::InteriorWall && !m.propAt(x, y) && !(x == ex && y >= H - 3)) m.setProp(x, y, p); };
  Spawn owner; owner.npc = true; owner.role = b.owner; owner.slot = 0; owner.site = b.site;
  switch (b.type) {
    case art::Building::Inn:
      for (int x = 2; x < 7; x++) put(x, 4, Prop::Counter);
      owner.x = 4; owner.y = 3;
      put(2, 2, Prop::Barrel); put(3, 2, Prop::Barrel2); put(6, 2, Prop::Shelf);
      put(W / 2, 2, Prop::Fireplace);
      for (int k = 0; k < 3; k++) { put(9 + (k % 2) * 4, 5 + (k / 2) * 3, Prop::Table); put(10 + (k % 2) * 4, 5 + (k / 2) * 3, Prop::Chair); put(8 + (k % 2) * 4, 5 + (k / 2) * 3, Prop::Chair); }
      put(W - 2, 2, Prop::Bed); put(W - 4, 2, Prop::Bed);
      put(W / 2, H - 4, Prop::Rug);
      for (int k = 1; k <= 2; k++) { Spawn p; p.npc = true; p.role = Role::Villager; p.slot = k; p.site = b.site; p.x = 9 + k * 2; p.y = 7; m.spawns.push_back(p); }
      break;
    case art::Building::Shop:
      for (int x = 2; x < W - 2; x++) if (x != ex) put(x, 4, Prop::Counter);
      owner.x = W / 2 - 2; owner.y = 3;
      for (int x = 1; x < W - 1; x += 2) put(x, 2, Prop::Shelf);
      put(1, H - 3, Prop::Barrel); put(W - 2, H - 3, Prop::Crate); put(W - 2, H - 4, Prop::Barrel2);
      put(2, H - 3, Prop::PlantPot);
      break;
    case art::Building::Smithy:
      put(2, 2, Prop::Fireplace); put(4, 3, Prop::Anvil); put(W - 2, 2, Prop::Shelf); put(W - 3, 2, Prop::Shelf);
      put(1, H - 3, Prop::Woodpile); put(W - 2, H - 3, Prop::Barrel); put(W - 2, H - 4, Prop::Crate);
      owner.x = 5; owner.y = 4;
      break;
    case art::Building::Temple:
      put(ex, 2, Prop::Altar); put(ex - 2, 2, Prop::Brazier); put(ex + 2, 2, Prop::Brazier);
      put(2, 2, Prop::Statue); put(W - 3, 2, Prop::Statue);
      for (int y = 5; y < H - 3; y += 2) { for (int x = 2; x < ex - 1; x++) put(x, y, Prop::Chair); for (int x = ex + 2; x < W - 2; x++) put(x, y, Prop::Chair); }
      for (int y = 3; y < H - 1; y++) if (!m.propAt(ex, y)) m.setProp(ex, y, Prop::Rug);
      owner.x = ex; owner.y = 3;
      break;
    case art::Building::Keep:
      put(ex, 2, Prop::Throne); put(ex - 3, 2, Prop::Banner); put(ex + 3, 2, Prop::Banner);
      put(2, 2, Prop::Bookshelf); put(3, 2, Prop::Bookshelf); put(W - 3, 2, Prop::Bookshelf); put(W - 4, 2, Prop::Shelf);
      for (int y = 3; y < H - 1; y++) if (!m.propAt(ex, y)) m.setProp(ex, y, Prop::Rug);
      for (int x = 3; x < 8; x++) put(x, 8, Prop::Table);
      for (int x = 3; x < 8; x++) { put(x, 7, Prop::Chair); put(x, 9, Prop::Chair); }
      put(W - 3, 6, Prop::Brazier); put(2, 6, Prop::Brazier); put(W - 3, 10, Prop::Brazier); put(2, 10, Prop::Brazier);
      owner.x = ex; owner.y = 3;
      for (int k = 1; k <= 2; k++) { Spawn g; g.npc = true; g.role = Role::Guard; g.slot = k; g.site = b.site; g.x = ex + (k == 1 ? -2 : 2); g.y = 4; m.spawns.push_back(g); }
      break;
    case art::Building::Tower:
      put(2, 2, Prop::Bookshelf); put(3, 2, Prop::Bookshelf); put(W - 3, 2, Prop::Bookshelf);
      put(ex, 4, Prop::Cauldron); put(W - 2, H - 3, Prop::PlantPot); put(2, 5, Prop::Table);
      owner.x = ex + 1; owner.y = 5;
      break;
    default: {   // homes and farms
      put(1 + r.irange(2), 2, Prop::Bed);
      put(W - 3, 2, Prop::Fireplace);
      int tx = 3 + r.irange(std::max(1, W - 7)), ty = 4 + r.irange(std::max(1, H - 8));
      put(tx, ty, Prop::Table); put(tx - 1, ty, Prop::Chair); put(tx + 1, ty, Prop::Chair);
      put(W - 2, H - 3, Prop::Barrel); put(1, H - 3, r.f() < 0.5f ? Prop::Shelf : Prop::Crate);
      if (r.f() < 0.5f) put(ex, H - 4, Prop::Rug);
      if (r.f() < 0.4f) put(W - 2, 3, Prop::Chest);
      if (b.type == art::Building::Farmhouse) { put(2, H - 3, Prop::Haystack); put(3, H - 3, Prop::Woodpile); }
      owner.x = tx; owner.y = ty + 1;
      break;
    }
  }
  if (b.owner != Role::Villager || r.f() < 0.75f) m.spawns.push_back(owner);
  // wall torches along the top wall
  for (int x = 3; x < W - 3; x += 5) if (!m.propAt(x, 1)) m.setProp(x, 1, Prop::Torch);
  m.rebuildSolid();
}
